#include "quickshare.hpp"

#include <vw/process.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <format>
#include <fstream>
#include <map>
#include <mutex>
#include <random>
#include <thread>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace blx {
void wake_services();  // services.cpp
}

namespace blx::quickshare {
namespace {

using nlohmann::json;
using Clock = std::chrono::steady_clock;

constexpr const char* group = "224.0.0.167";
constexpr std::uint16_t port = 53317;
constexpr const char* api = "/api/localsend/v2/";

std::string random_id() {
  static std::mt19937_64 random{std::random_device{}()};
  return std::format("{:x}", random());
}

void notify(const std::string& summary, const std::string& body) {
  vw::spawn({"notify-send", "--app-name=BlxShell", "--icon=folder-download", "--urgency=normal",
             "--expire-time=8000", summary, body});
}

std::string shell_quote(const std::string& text) {
  std::string result = "'";
  for (char c : text) result += c == '\'' ? std::string("'\\''") : std::string(1, c);
  return result + "'";
}

// Runs a shell command, returning stdout.
std::pair<std::string, int> run(const std::string& command) {
  FILE* pipe = popen(command.c_str(), "re");
  if (pipe == nullptr) return {"", -1};
  std::string output;
  std::array<char, 4096> chunk{};
  for (std::size_t count; (count = std::fread(chunk.data(), 1, chunk.size(), pipe)) > 0;) output.append(chunk.data(), count);
  return {output, pclose(pipe)};
}

struct PeerDevice {
  Device device;
  std::string protocol = "http";
  int port = 0;
  Clock::time_point seen{};
};

struct HeldIncoming {
  Incoming info;
  int socket = -1;
  json files;
  std::map<std::string, std::string> tokens;
  bool approved = false;
};

struct Connection {
  std::string data;
  std::string body;
  std::string method;
  std::string path;
  std::map<std::string, std::string> headers;
  long long length = -1;
  // Uploads stream to a partial file in Downloads instead of memory.
  std::ofstream sink;
  std::filesystem::path part;
  std::string session;
  std::string file_id;
  long long received = 0;
};

struct State {
  std::mutex mutex;
  bool started = false;
  bool running = false;
  std::string error;
  std::string alias;
  std::string fingerprint;
  std::map<std::string, PeerDevice> devices;
  std::vector<Transfer> transfers;
  std::map<std::string, HeldIncoming> incoming;
  int udp = -1;
  int server = -1;
};

State& state() {
  static State instance;
  return instance;
}

json device_info(const State& s) {
  return {{"alias", s.alias}, {"version", "2.1"}, {"deviceModel", "BlxShell"}, {"deviceType", "desktop"},
          {"fingerprint", s.fingerprint}, {"port", port}, {"protocol", "http"}, {"download", false}};
}

void announce(State& s) {
  if (s.udp < 0) return;
  json message = device_info(s);
  message["announce"] = true;
  message["announcement"] = true;  // LocalSend used both spellings
  const std::string data = message.dump();
  sockaddr_in to{};
  to.sin_family = AF_INET;
  to.sin_port = htons(port);
  inet_pton(AF_INET, group, &to.sin_addr);
  sendto(s.udp, data.data(), data.size(), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
}

std::string url(const PeerDevice& peer, const std::string& endpoint) {
  return std::format("{}://{}:{}{}{}", peer.protocol, peer.device.address, peer.port, api, endpoint);
}

void register_with(const PeerDevice& peer, const json& info) {
  std::thread([peer, body = info.dump()] {
    run(std::format("curl -ksS --max-time 5 -X POST -H 'Content-Type: application/json' --data-binary {} {} >/dev/null 2>&1",
                    shell_quote(body), shell_quote(url(peer, "register"))));
  }).detach();
}

void send_raw(int socket, int status, const std::string& body) {
  const std::string response = std::format(
      "HTTP/1.1 {} OK\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}", status,
      body.size(), body);
  const auto written = ::send(socket, response.data(), response.size(), MSG_NOSIGNAL);
  (void)written;
  close(socket);
}

void send_json(int socket, int status, const json& body) { send_raw(socket, status, body.dump()); }
void send_error(int socket, int status, const std::string& message) { send_json(socket, status, {{"message", message}}); }

std::string downloads() {
  const char* home = std::getenv("HOME");
  std::string path = std::string(home ? home : ".") + "/Downloads";
  if (const char* configured = std::getenv("XDG_DOWNLOAD_DIR"); configured && *configured) path = configured;
  std::error_code error;
  std::filesystem::create_directories(path, error);
  return path;
}

std::string query_value(const std::string& query, const std::string& key) {
  std::size_t at = 0;
  while (at < query.size()) {
    auto end = query.find('&', at);
    if (end == std::string::npos) end = query.size();
    const std::string pair = query.substr(at, end - at);
    if (pair.starts_with(key + "=")) return pair.substr(key.size() + 1);
    at = end + 1;
  }
  return {};
}

// Returns true when the socket was handed over (held for approval).
bool handle(State& s, int socket, Connection& c) {
  const std::string base = api;
  if (c.method == "GET" && c.path == base + "info") {
    send_json(socket, 200, device_info(s));
    return false;
  }
  if (c.method != "POST") {
    send_error(socket, 405, "POST required");
    return false;
  }
  if (c.path == base + "register") {
    send_json(socket, 200, device_info(s));
    return false;
  }
  if (c.path == base + "prepare-upload") {
    const json root = json::parse(c.body, nullptr, false);
    if (root.is_discarded() || !root.contains("info") || !root.contains("files") || root["files"].empty()) {
      send_error(socket, 400, "Invalid upload request");
      return false;
    }
    HeldIncoming held;
    held.info.id = random_id();
    held.info.sender = root["info"].value("alias", "Unknown device");
    sockaddr_in peer{};
    socklen_t length = sizeof(peer);
    getpeername(socket, reinterpret_cast<sockaddr*>(&peer), &length);
    std::array<char, INET_ADDRSTRLEN> address{};
    inet_ntop(AF_INET, &peer.sin_addr, address.data(), address.size());
    held.info.address = address.data();
    held.files = root["files"];
    held.info.files = held.files.size();
    for (const auto& [id, file] : held.files.items()) held.info.bytes += file.value("size", 0ULL);
    held.socket = socket;
    notify("Quick Share request", std::format("{} wants to send {} file{}", held.info.sender, held.info.files,
                                              held.info.files == 1 ? "" : "s"));
    s.incoming[held.info.id] = std::move(held);
    return true;
  }
  send_error(socket, 404, "Unknown LocalSend endpoint");
  return false;
}

bool is_upload(const Connection& c) { return c.method == "POST" && c.path.starts_with(std::string(api) + "upload?"); }

void discard(Connection& c) {
  if (c.part.empty()) return;
  c.sink.close();
  std::error_code error;
  std::filesystem::remove(c.part, error);
  c.part.clear();
}

// Checks the token and size, and opens the partial file.
bool begin_upload(State& s, int socket, Connection& c) {
  const std::string query = c.path.substr(c.path.find('?') + 1);
  c.session = query_value(query, "sessionId");
  c.file_id = query_value(query, "fileId");
  const auto found = s.incoming.find(c.session);
  if (found == s.incoming.end() || !found->second.approved) {
    send_error(socket, 403, "Invalid upload token");
    return false;
  }
  const auto token = found->second.tokens.find(c.file_id);
  if (token == found->second.tokens.end() || token->second != query_value(query, "token")) {
    send_error(socket, 403, "Invalid upload token");
    return false;
  }
  const json file = found->second.files.value(c.file_id, json::object());
  const std::string name = std::filesystem::path(file.value("fileName", "")).filename().string();
  if (name.empty() || name == "." || name == ".." || file.value("size", 0ULL) != static_cast<std::uint64_t>(c.length)) {
    send_error(socket, 400, "Invalid file data");
    return false;
  }
  c.part = std::filesystem::path(downloads()) / std::format(".{}.{}.part", c.session, random_id());
  c.sink.open(c.part, std::ios::binary);
  if (!c.sink) {
    c.part.clear();
    send_error(socket, 500, "Unable to save file");
    return false;
  }
  return true;
}

void finish_upload(State& s, int socket, Connection& c) {
  c.sink.close();
  const auto found = s.incoming.find(c.session);
  if (found == s.incoming.end() || c.sink.fail()) {
    discard(c);
    send_error(socket, found == s.incoming.end() ? 403 : 500, "Unable to save file");
    return;
  }
  const json file = found->second.files.value(c.file_id, json::object());
  std::string name = std::filesystem::path(file.value("fileName", "")).filename().string();
  if (name.size() > 255) name.resize(255);
  const std::filesystem::path directory = downloads();
  std::filesystem::path destination = directory / name;
  const std::string stem = std::filesystem::path(name).stem().string();
  const std::string extension = std::filesystem::path(name).extension().string();
  for (int number = 1; std::filesystem::exists(destination); ++number)
    destination = directory / std::format("{} ({}){}", stem, number, extension);
  std::error_code error;
  std::filesystem::rename(c.part, destination, error);
  if (error) {
    discard(c);
    send_error(socket, 500, "Unable to save file");
    return;
  }
  c.part.clear();
  const std::string sender = found->second.info.sender;
  found->second.tokens.erase(c.file_id);
  if (found->second.tokens.empty()) s.incoming.erase(found);
  notify("Quick Share received", std::format("{} from {} saved to Downloads", destination.filename().string(), sender));
  send_json(socket, 200, json::object());
}

// Reads what is available; returns true while the request is incomplete.
// Completes (and closes or hands over) the socket otherwise.
bool read_connection(State& s, int socket, Connection& c) {
  std::array<char, 65536> buffer{};
  const auto count = ::recv(socket, buffer.data(), buffer.size(), MSG_DONTWAIT);
  if (count <= 0) {
    discard(c);
    close(socket);
    return false;
  }
  if (c.length < 0) c.data.append(buffer.data(), static_cast<std::size_t>(count));
  else c.body.append(buffer.data(), static_cast<std::size_t>(count));
  if (c.length < 0) {
    const auto end = c.data.find("\r\n\r\n");
    if (end == std::string::npos) {
      if (c.data.size() > 32768) send_error(socket, 431, "Headers too large");
      return c.data.size() <= 32768;
    }
    const std::string head = c.data.substr(0, end);
    const auto first = head.find("\r\n");
    const std::string request = head.substr(0, first);
    const auto space1 = request.find(' ');
    const auto space2 = request.find(' ', space1 + 1);
    if (space1 == std::string::npos || space2 == std::string::npos) {
      send_error(socket, 400, "Malformed request");
      return false;
    }
    c.method = request.substr(0, space1);
    c.path = request.substr(space1 + 1, space2 - space1 - 1);
    std::size_t line = first == std::string::npos ? head.size() : first + 2;
    while (line < head.size()) {
      auto next = head.find("\r\n", line);
      if (next == std::string::npos) next = head.size();
      const std::string header = head.substr(line, next - line);
      if (const auto colon = header.find(':'); colon != std::string::npos) {
        std::string key = header.substr(0, colon);
        std::ranges::transform(key, key.begin(), [](unsigned char ch) { return std::tolower(ch); });
        std::string value = header.substr(colon + 1);
        while (!value.empty() && value.front() == ' ') value.erase(0, 1);
        c.headers[key] = value;
      }
      line = next + 2;
    }
    if (c.method == "GET") c.length = 0;
    else {
      try {
        c.length = std::stoll(c.headers.at("content-length"));
      } catch (const std::exception&) {
        send_error(socket, 411, "Content-Length required");
        return false;
      }
      if (c.length < 0 || (!is_upload(c) && c.length > 16 * 1024 * 1024)) {
        send_error(socket, 413, "Request too large");
        return false;
      }
      if (is_upload(c) && !begin_upload(s, socket, c)) return false;
    }
    if (c.headers["expect"] == "100-continue") {
      const std::string go = "HTTP/1.1 100 Continue\r\n\r\n";
      const auto written = ::send(socket, go.data(), go.size(), MSG_NOSIGNAL);
      (void)written;
    }
    c.body = c.data.substr(end + 4);
    c.data.clear();
  }
  if (c.sink.is_open()) {
    c.sink.write(c.body.data(), static_cast<std::streamsize>(c.body.size()));
    c.received += static_cast<long long>(c.body.size());
    c.body.clear();
    if (c.received < c.length && c.sink) return true;
    if (c.received > c.length || !c.sink) {
      discard(c);
      send_error(socket, 400, "Invalid file data");
      return false;
    }
    finish_upload(s, socket, c);
    wake_services();
    return false;
  }
  if (static_cast<long long>(c.body.size()) < c.length) return true;
  if (static_cast<long long>(c.body.size()) > c.length) {
    send_error(socket, 400, "Pipelined requests unsupported");
    return false;
  }
  handle(s, socket, c);  // closes the socket unless it holds it
  wake_services();
  return false;
}

void loop() {
  State& s = state();
  std::map<int, Connection> connections;
  auto last_announce = Clock::now();
  for (;;) {
    std::vector<pollfd> fds{{s.udp, POLLIN, 0}, {s.server, POLLIN, 0}};
    for (const auto& [socket, connection] : connections) fds.push_back({socket, POLLIN, 0});
    poll(fds.data(), fds.size(), 1000);
    std::lock_guard lock(s.mutex);
    if ((fds[0].revents & POLLIN) != 0) {
      std::array<char, 8192> buffer{};
      sockaddr_in from{};
      socklen_t length = sizeof(from);
      const auto count = recvfrom(s.udp, buffer.data(), buffer.size(), MSG_DONTWAIT, reinterpret_cast<sockaddr*>(&from), &length);
      const json message = count > 0 ? json::parse(std::string(buffer.data(), static_cast<std::size_t>(count)), nullptr, false)
                                     : json();
      if (message.is_object()) {
        const std::string fingerprint = message.value("fingerprint", "");
        const int peer_port = message.value("port", 0);
        if (!fingerprint.empty() && fingerprint != s.fingerprint && peer_port > 0 && peer_port < 65536) {
          std::array<char, INET_ADDRSTRLEN> address{};
          inet_ntop(AF_INET, &from.sin_addr, address.data(), address.size());
          PeerDevice& peer = s.devices[fingerprint];
          peer.device = {fingerprint, message.value("alias", ""), address.data(), message.value("deviceModel", ""),
                         message.value("deviceType", "")};
          peer.protocol = message.value("protocol", "http");
          peer.port = peer_port;
          peer.seen = Clock::now();
          if (message.value("announce", false) || message.value("announcement", false)) register_with(peer, device_info(s));
          wake_services();
        }
      }
    }
    if ((fds[1].revents & POLLIN) != 0) {
      const int client = accept4(s.server, nullptr, nullptr, SOCK_CLOEXEC);
      if (client >= 0) connections[client] = {};
    }
    for (std::size_t index = 2; index < fds.size(); ++index) {
      if ((fds[index].revents & (POLLIN | POLLHUP | POLLERR)) == 0) continue;
      const int socket = fds[index].fd;
      if (!read_connection(s, socket, connections[socket])) connections.erase(socket);
    }
    // Every 5 s: announce, and forget devices unseen for 15 s.
    if (Clock::now() - last_announce >= std::chrono::seconds(5)) {
      last_announce = Clock::now();
      announce(s);
      const auto before = s.devices.size();
      std::erase_if(s.devices, [](const auto& entry) { return Clock::now() - entry.second.seen > std::chrono::seconds(15); });
      if (s.devices.size() != before) wake_services();
      // A sender that gave up while waiting for approval closed its socket.
      const auto waiting = s.incoming.size();
      std::erase_if(s.incoming, [](const auto& entry) {
        const int held = entry.second.socket;
        if (held < 0) return false;
        char ignored = 0;
        if (::recv(held, &ignored, 1, MSG_PEEK | MSG_DONTWAIT) != 0) return false;
        close(held);
        return true;
      });
      if (s.incoming.size() != waiting) wake_services();
    }
  }
}

void start() {
  State& s = state();
  if (s.started) return;
  s.started = true;
  std::array<char, 256> host{};
  gethostname(host.data(), host.size());
  s.alias = host[0] != '\0' ? host.data() : "BlxShell";
  s.fingerprint = std::format("{:x}{:x}", std::hash<std::string>{}(s.alias + random_id()), std::hash<std::string>{}(random_id()));
  s.server = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  const int yes = 1;
  setsockopt(s.server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(s.server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(s.server, 16) != 0) {
    s.error = std::format("Cannot listen on port {}: {}", port, std::strerror(errno));
    close(s.server);
    s.server = -1;
    return;
  }
  s.udp = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  setsockopt(s.udp, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  setsockopt(s.udp, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
  if (bind(s.udp, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    s.error = std::format("Cannot bind discovery socket: {}", std::strerror(errno));
    close(s.udp);
    close(s.server);
    s.udp = s.server = -1;
    return;
  }
  ip_mreq membership{};
  inet_pton(AF_INET, group, &membership.imr_multiaddr);
  membership.imr_interface.s_addr = htonl(INADDR_ANY);
  setsockopt(s.udp, IPPROTO_IP, IP_ADD_MEMBERSHIP, &membership, sizeof(membership));
  s.running = true;
  announce(s);
  std::thread(loop).detach();
}

State& started() {
  State& s = state();
  std::lock_guard lock(s.mutex);
  start();
  return s;
}

void set_transfer(const std::string& id, const std::function<void(Transfer&)>& change) {
  State& s = state();
  {
    std::lock_guard lock(s.mutex);
    for (auto& transfer : s.transfers)
      if (transfer.id == id) change(transfer);
  }
  wake_services();
}

}  // namespace

bool running() {
  State& s = started();
  std::lock_guard lock(s.mutex);
  return s.running;
}

std::string error() {
  State& s = started();
  std::lock_guard lock(s.mutex);
  return s.error;
}

std::vector<Device> devices() {
  State& s = started();
  std::lock_guard lock(s.mutex);
  std::vector<Device> result;
  for (const auto& [id, peer] : s.devices) result.push_back(peer.device);
  return result;
}

std::vector<Transfer> transfers() {
  State& s = started();
  std::lock_guard lock(s.mutex);
  return s.transfers;
}

std::vector<Incoming> incoming() {
  State& s = started();
  std::lock_guard lock(s.mutex);
  std::vector<Incoming> result;
  for (const auto& [id, held] : s.incoming)
    if (!held.approved) result.push_back(held.info);
  return result;
}

void refresh() {
  State& s = started();
  std::lock_guard lock(s.mutex);
  announce(s);
}

void send_file(const std::string& device_id, const std::string& path) {
  State& s = started();
  PeerDevice peer;
  json info;
  Transfer transfer;
  {
    std::lock_guard lock(s.mutex);
    const auto found = s.devices.find(device_id);
    std::error_code error;
    if (found == s.devices.end() || !std::filesystem::is_regular_file(path, error)) {
      s.error = "Device or readable file not found";
      return;
    }
    peer = found->second;
    info = device_info(s);
    transfer = {random_id(), std::filesystem::path(path).filename().string(), "preparing", "", 0};
    s.transfers.push_back(transfer);
  }
  wake_services();
  std::thread([peer, info, transfer, path] {
    const std::string file_id = random_id();
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    const json body = {{"info", info},
                       {"files", {{file_id, {{"id", file_id}, {"fileName", transfer.name}, {"size", size},
                                             {"fileType", "application/octet-stream"}}}}}};
    const auto [response, status] = run(std::format(
        "curl -ksS --fail --max-time 60 -X POST -H 'Content-Type: application/json' --data-binary {} {} 2>/dev/null",
        shell_quote(body.dump()), shell_quote(url(peer, "prepare-upload"))));
    const json answer = json::parse(response, nullptr, false);
    const std::string session = answer.is_object() ? answer.value("sessionId", "") : "";
    const std::string token = answer.is_object() && answer.contains("files") ? answer["files"].value(file_id, "") : "";
    if (status != 0 || session.empty() || token.empty()) {
      set_transfer(transfer.id, [](Transfer& t) {
        t.state = "error";
        t.error = "Peer rejected the upload";
      });
      return;
    }
    set_transfer(transfer.id, [](Transfer& t) { t.state = "uploading"; });
    // curl's progress bar ("###  45.0%") on stderr drives the progress.
    const std::string command = std::format(
        "curl -k --fail -X POST -H 'Content-Type: application/octet-stream' --data-binary @{} -o /dev/null -# {} 2>&1",
        shell_quote(path), shell_quote(url(peer, std::format("upload?sessionId={}&fileId={}&token={}", session, file_id, token))));
    FILE* pipe = popen(command.c_str(), "re");
    std::string chunk;
    int character = 0;
    while (pipe != nullptr && (character = std::fgetc(pipe)) != EOF) {
      if (character != '\r' && character != '\n') {
        chunk += static_cast<char>(character);
        continue;
      }
      if (const auto percent = chunk.rfind('%'); percent != std::string::npos) {
        auto start = chunk.find_last_of(' ', percent);
        try {
          const double value = std::stod(chunk.substr(start == std::string::npos ? 0 : start + 1, percent - start - 1));
          set_transfer(transfer.id, [value](Transfer& t) { t.progress = value / 100; });
        } catch (const std::exception&) {
        }
      }
      chunk.clear();
    }
    const int result = pipe != nullptr ? pclose(pipe) : -1;
    set_transfer(transfer.id, [result](Transfer& t) {
      if (result == 0) {
        t.state = "complete";
        t.progress = 1;
      } else {
        t.state = "error";
        t.error = "Upload failed";
      }
    });
  }).detach();
}

void pick_and_send(const std::string& device) {
  std::thread([device] {
    const auto [output, status] = run(
        "zenity --file-selection --multiple --separator='\n' --title='Send files with LocalSend' 2>/dev/null || "
        "kdialog --getopenfilename --multiple --separate-output 2>/dev/null");
    std::size_t start = 0;
    while (start < output.size()) {
      auto end = output.find('\n', start);
      if (end == std::string::npos) end = output.size();
      const std::string path = output.substr(start, end - start);
      if (!path.empty()) send_file(device, path);
      start = end + 1;
    }
    (void)status;
  }).detach();
}

void approve(const std::string& id) {
  State& s = started();
  {
    std::lock_guard lock(s.mutex);
    const auto found = s.incoming.find(id);
    if (found == s.incoming.end() || found->second.socket < 0) return;
    json tokens = json::object();
    for (const auto& [file_id, file] : found->second.files.items()) {
      const std::string token = random_id();
      tokens[file_id] = token;
      found->second.tokens[file_id] = token;
    }
    found->second.approved = true;
    send_json(found->second.socket, 200, {{"sessionId", id}, {"files", tokens}});
    found->second.socket = -1;
  }
  wake_services();
}

void reject(const std::string& id) {
  State& s = started();
  {
    std::lock_guard lock(s.mutex);
    const auto found = s.incoming.find(id);
    if (found == s.incoming.end()) return;
    if (found->second.socket >= 0) send_error(found->second.socket, 403, "Rejected by user");
    s.incoming.erase(found);
  }
  wake_services();
}

}  // namespace blx::quickshare
