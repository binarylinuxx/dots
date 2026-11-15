#include "halley.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <format>
#include <mutex>
#include <thread>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace blx {
void wake_services();  // services.cpp
}

namespace blx::halley {
namespace {

using Bytes = std::vector<std::uint8_t>;
constexpr std::uint32_t max_frame = 1024 * 1024;

std::string socket_path() { return std::format("/run/user/{}/halley/halley.sock", getuid()); }

class Reader {
 public:
  explicit Reader(const Bytes& data) : data_(data) {}
  bool done() const { return at_ == data_.size(); }
  bool byte(std::uint8_t& value) {
    if (at_ >= data_.size()) return false;
    value = data_[at_++];
    return true;
  }
  bool boolean(bool& value) {
    std::uint8_t raw = 0;
    if (!byte(raw) || raw > 1) return false;
    value = raw != 0;
    return true;
  }
  bool tag(std::uint8_t& value) { return byte(value) && value <= 1; }
  bool varint(std::uint64_t& value) {
    value = 0;
    for (int shift = 0; shift < 64; shift += 7) {
      std::uint8_t raw = 0;
      if (!byte(raw) || (shift == 63 && raw > 1)) return false;
      value |= static_cast<std::uint64_t>(raw & 0x7f) << shift;
      if ((raw & 0x80) == 0) return true;
    }
    return false;
  }
  bool string(std::string& value) {
    std::uint64_t length = 0;
    if (!varint(length) || length > data_.size() - at_) return false;
    value.assign(reinterpret_cast<const char*>(data_.data() + at_), length);
    at_ += length;
    return true;
  }
  bool optional_string(std::string& value) {
    std::uint8_t present = 0;
    return tag(present) && (present == 0 || string(value));
  }
  bool optional_varint() {
    std::uint8_t present = 0;
    std::uint64_t ignored = 0;
    return tag(present) && (present == 0 || varint(ignored));
  }
  // Little-endian fixed-width fields (the host is little-endian).
  bool skip(std::size_t count) {
    if (data_.size() - at_ < count) return false;
    at_ += count;
    return true;
  }

 private:
  const Bytes& data_;
  std::size_t at_ = 0;
};

// One node record, as in decodeNodes/decodeClusterDetail.
bool decode_node(Reader& r, Node& node) {
  std::uint64_t kind = 0, state = 0, role = 0, family = 0, children = 0;
  bool latest = false, pinned = false, modal = false;
  return r.varint(node.id) && r.string(node.title) && r.optional_string(node.app) && r.optional_string(node.output) &&
         r.varint(kind) && r.varint(state) && r.boolean(node.visible) && r.boolean(node.focused) && r.boolean(latest) &&
         r.boolean(pinned) && r.varint(role) && r.varint(family) && r.boolean(modal) && r.optional_varint() &&
         r.optional_varint() && r.varint(children) && r.skip(16);  // x, y, width, height as f32
}

bool decode_nodes(const Bytes& payload, std::vector<Node>& nodes) {
  Reader r(payload);
  std::uint8_t response = 0;
  std::uint64_t groups = 0;
  if (!r.byte(response) || response != 5 || !r.varint(groups) || groups > 128) return false;
  std::vector<Node> parsed;
  for (std::uint64_t group = 0; group < groups; ++group) {
    std::string output;
    std::uint64_t count = 0;
    if (!r.string(output) || !r.varint(count) || count > 4096) return false;
    for (std::uint64_t index = 0; index < count; ++index) {
      Node node;
      if (!decode_node(r, node)) return false;
      if (node.output.empty()) node.output = output;
      parsed.push_back(std::move(node));
    }
  }
  if (!r.done()) return false;
  nodes = std::move(parsed);
  return true;
}

bool decode_cluster_head(Reader& r, Cluster& cluster) {
  std::uint8_t slot_tag = 0, layout = 0;
  std::uint64_t members = 0;
  if (!r.varint(cluster.id) || !r.tag(slot_tag)) return false;
  if (slot_tag != 0) {
    std::uint8_t slot = 0;
    if (!r.byte(slot)) return false;
    cluster.slot = slot;
  }
  return r.optional_string(cluster.name) && r.optional_string(cluster.output) && r.byte(layout) && layout <= 1 &&
         r.varint(members) && r.boolean(cluster.active) && r.boolean(cluster.focused);
}

bool decode_clusters(const Bytes& payload, std::vector<Cluster>& clusters) {
  Reader r(payload);
  std::uint8_t response = 0;
  std::uint64_t groups = 0;
  if (!r.byte(response) || response != 7 || !r.varint(groups) || groups > 128) return false;
  std::vector<Cluster> parsed;
  for (std::uint64_t group = 0; group < groups; ++group) {
    std::string output;
    std::uint64_t count = 0;
    if (!r.string(output) || !r.varint(count) || count > 128) return false;
    for (std::uint64_t index = 0; index < count; ++index) {
      Cluster cluster;
      if (!decode_cluster_head(r, cluster)) return false;
      if (cluster.output.empty()) cluster.output = output;
      parsed.push_back(std::move(cluster));
    }
  }
  if (!r.done()) return false;
  clusters = std::move(parsed);
  return true;
}

bool decode_cluster_detail(const Bytes& payload, Cluster& cluster) {
  Reader r(payload);
  std::uint8_t response = 0;
  std::uint64_t members = 0;
  if (!r.byte(response) || response != 8 || !decode_cluster_head(r, cluster)) return false;
  // The focused member index and id are optional in inactive clusters.
  if (!r.optional_varint() || !r.optional_varint() || !r.varint(members) || members > 128) return false;
  for (std::uint64_t index = 0; index < members; ++index) {
    Node node;
    if (!decode_node(r, node)) return false;
    if (node.output.empty()) node.output = cluster.output;
    cluster.members.push_back(std::move(node));
  }
  return r.done();
}

void varint(Bytes& out, std::uint64_t value) {
  do {
    const auto low = static_cast<std::uint8_t>(value & 0x7f);
    value >>= 7;
    out.push_back(low | (value != 0 ? 0x80 : 0));
  } while (value != 0);
}

// One request per connection; Halley closes after its reply.
std::optional<Bytes> request(const Bytes& payload) {
  const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) return std::nullopt;
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  const std::string path = socket_path();
  std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);
  timeval timeout{1, 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  std::optional<Bytes> result;
  if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
    Bytes frame(4);
    const auto length = static_cast<std::uint32_t>(payload.size());
    std::memcpy(frame.data(), &length, 4);
    frame.insert(frame.end(), payload.begin(), payload.end());
    if (::send(fd, frame.data(), frame.size(), MSG_NOSIGNAL) == static_cast<ssize_t>(frame.size())) {
      Bytes buffer;
      std::array<std::uint8_t, 16384> chunk{};
      for (ssize_t count; (count = ::recv(fd, chunk.data(), chunk.size(), 0)) > 0;) {
        buffer.insert(buffer.end(), chunk.begin(), chunk.begin() + count);
        if (buffer.size() >= 4) {
          std::uint32_t reply = 0;
          std::memcpy(&reply, buffer.data(), 4);
          if (reply > max_frame) break;
          if (buffer.size() >= 4 + reply) {
            result = Bytes(buffer.begin() + 4, buffer.begin() + 4 + reply);
            break;
          }
        }
      }
    }
  }
  close(fd);
  return result;
}

bool same(const Node& a, const Node& b) {
  return a.id == b.id && a.title == b.title && a.app == b.app && a.visible == b.visible && a.focused == b.focused;
}

bool same(const std::vector<Node>& a, const std::vector<Node>& b) {
  return std::ranges::equal(a, b, [](const Node& x, const Node& y) { return same(x, y); });
}

bool same(const std::vector<Cluster>& a, const std::vector<Cluster>& b) {
  return std::ranges::equal(a, b, [](const Cluster& x, const Cluster& y) {
    return x.id == y.id && x.slot == y.slot && x.name == y.name && x.active == y.active && x.focused == y.focused &&
           same(x.members, y.members);
  });
}

struct State {
  std::mutex mutex;
  bool started = false;
  bool available = false;
  std::vector<Node> nodes;
  std::vector<Cluster> clusters;
};

State& state() {
  static State instance;
  return instance;
}

std::optional<std::vector<Cluster>> fetch_clusters() {
  std::vector<Cluster> clusters;
  const auto reply = request({0x08, 0x00, 0x00});
  if (!reply || !decode_clusters(*reply, clusters)) return std::nullopt;
  for (Cluster& cluster : clusters) {
    Bytes inspect{0x08, 0x01, 0x01, 0x01};
    varint(inspect, cluster.id);
    inspect.push_back(0);
    Cluster detail;
    if (const auto answer = request(inspect); answer && decode_cluster_detail(*answer, detail) && detail.id == cluster.id)
      cluster.members = std::move(detail.members);
  }
  return clusters;
}

void poll_loop() {
  State& s = state();
  for (int tick = 0;; ++tick) {
    const bool available = std::filesystem::exists(socket_path());
    // A failed request keeps the last good state, like decode failures in
    // HalleyManager, which publish nothing.
    std::optional<std::vector<Node>> nodes;
    std::optional<std::vector<Cluster>> clusters;
    if (available) {
      std::vector<Node> parsed;
      if (const auto reply = request({0x02, 0x00, 0x00}); reply && decode_nodes(*reply, parsed)) nodes = std::move(parsed);
      if (tick % 4 == 0) clusters = fetch_clusters();
    }
    bool changed = false;
    {
      std::lock_guard lock(s.mutex);
      changed = available != s.available || (nodes && !same(*nodes, s.nodes)) || (clusters && !same(*clusters, s.clusters));
      s.available = available;
      if (nodes) s.nodes = std::move(*nodes);
      if (clusters) s.clusters = std::move(*clusters);
      if (!available) {
        s.nodes.clear();
        s.clusters.clear();
      }
    }
    // A focus change usually means a cluster change: fetch them next tick.
    if (changed) {
      wake_services();
      if (tick % 4 != 3) tick = 3;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(available ? 500 : 2000));
  }
}

State& started() {
  State& s = state();
  std::lock_guard lock(s.mutex);
  if (!s.started) {
    s.started = true;
    s.available = std::filesystem::exists(socket_path());
    std::thread(poll_loop).detach();
  }
  return s;
}

void action(Bytes payload) {
  std::thread([payload = std::move(payload)] {
    request(payload);
    wake_services();
  }).detach();
}

}  // namespace

bool available() {
  State& s = started();
  std::lock_guard lock(s.mutex);
  return s.available;
}

std::vector<Cluster> clusters() {
  State& s = started();
  std::lock_guard lock(s.mutex);
  return s.clusters;
}

std::optional<Node> active_node() {
  State& s = started();
  std::lock_guard lock(s.mutex);
  for (const Node& node : s.nodes)
    if (node.focused) return node;
  return std::nullopt;
}

void focus_cluster_slot(int slot) {
  if (slot < 1 || slot > 10) return;
  action({0x08, 0x05, static_cast<std::uint8_t>(slot), 0x00});
}

void focus_node(std::uint64_t id) {
  Bytes payload{0x02, 0x02, 0x01, 0x02};
  varint(payload, id);
  payload.push_back(0);
  action(std::move(payload));
}

}  // namespace blx::halley
