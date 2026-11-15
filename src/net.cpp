#include "net.hpp"

#include <spawn.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>

extern char** environ;

namespace blx::net {
namespace {

namespace fs = std::filesystem;
using vw::network::AccessPoint;
using vw::network::Kind;
using vw::network::Status;

constexpr const char* control_directory = "/run/wpa_supplicant";

// Runs a program without a shell and returns its stdout; empty on failure.
std::string run(std::vector<std::string> arguments) {
  int pipe_fds[2];
  if (pipe(pipe_fds) != 0) return {};
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
  posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
  posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", 0, 0);
  std::vector<char*> argv;
  for (auto& argument : arguments) argv.push_back(argument.data());
  argv.push_back(nullptr);
  pid_t pid = 0;
  const int failed = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  close(pipe_fds[1]);
  std::string output;
  if (failed == 0) {
    char buffer[4096];
    ssize_t got;
    while ((got = read(pipe_fds[0], buffer, sizeof buffer)) > 0) output.append(buffer, static_cast<std::size_t>(got));
    int status = 0;
    waitpid(pid, &status, 0);
  }
  close(pipe_fds[0]);
  return output;
}

std::string word(const fs::path& path) {
  std::ifstream file(path);
  std::string value;
  file >> value;
  return value;
}

// The interface wpa_supplicant listens for, from its control socket.
std::string interface() {
  std::error_code error;
  for (const auto& entry : fs::directory_iterator(control_directory, error))
    if (entry.is_socket(error)) return entry.path().filename().string();
  return {};
}

std::vector<std::string> wpa(const std::string& iface, std::vector<std::string> command) {
  std::vector<std::string> arguments{"wpa_cli", "-p", control_directory, "-i", iface};
  arguments.insert(arguments.end(), command.begin(), command.end());
  return arguments;
}

std::uint8_t strength(int dbm) {
  // -90 dBm is barely usable, -30 dBm is as good as it gets.
  return static_cast<std::uint8_t>(std::clamp((dbm + 90) * 100 / 60, 0, 100));
}

std::string hex(std::string_view text) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string out;
  for (unsigned char c : text) {
    out += digits[c >> 4];
    out += digits[c & 15];
  }
  return out;
}

struct Snapshot {
  Status status;
  std::vector<AccessPoint> access_points;
};

class Poller {
 public:
  Poller() : event_(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {
    std::thread([this] {
      while (true) {
        refresh();
        std::this_thread::sleep_for(std::chrono::seconds(2));
      }
    }).detach();
  }

  Snapshot snapshot() {
    std::lock_guard lock(mutex_);
    return snapshot_;
  }

  int fd() const { return event_; }

  bool drain() {
    std::uint64_t value = 0;
    return read(event_, &value, sizeof value) == sizeof value;
  }

  // Runs commands off the UI thread, then refreshes the state.
  void act(std::function<void(const std::string&)> work) {
    std::thread([this, work = std::move(work)] {
      const std::string iface = interface();
      if (!iface.empty()) work(iface);
      refresh();
    }).detach();
  }

  void refresh() {
    Snapshot next;
    Status& status = next.status;
    status.available = true;
    status.networking_enabled = true;
    const std::string iface = interface();
    status.has_wifi = !iface.empty();

    // rfkill: soft blocks turn Wi-Fi off, hard blocks take it away.
    status.wifi_enabled = status.has_wifi;
    status.wifi_hardware_enabled = status.has_wifi;
    std::error_code error;
    for (const auto& entry : fs::directory_iterator("/sys/class/rfkill", error)) {
      if (word(entry.path() / "type") != "wlan") continue;
      if (word(entry.path() / "soft") == "1") status.wifi_enabled = false;
      if (word(entry.path() / "hard") == "1") status.wifi_hardware_enabled = false;
    }

    // The default route says which interface is online.
    std::string route_device;
    std::istringstream routes(run({"ip", "-4", "route", "show", "default"}));
    for (std::string line; std::getline(routes, line);) {
      const auto at = line.find(" dev ");
      if (at == std::string::npos) continue;
      route_device = line.substr(at + 5, line.find(' ', at + 5) - (at + 5));
      break;
    }
    if (!route_device.empty()) {
      status.connectivity = vw::network::Connectivity::full;
      std::istringstream addresses(run({"ip", "-4", "-o", "address", "show", "dev", route_device}));
      std::string token;
      for (std::string previous; addresses >> token; previous = token)
        if (previous == "inet") status.address = token;
      const bool wireless = fs::exists(fs::path("/sys/class/net") / route_device / "wireless", error);
      status.primary = wireless ? Kind::wifi : Kind::wired;
      status.wired = !wireless;
      status.connection = route_device;
    } else {
      status.connectivity = vw::network::Connectivity::none;
    }

    if (!iface.empty()) {
      std::string active_bssid;
      std::istringstream lines(run(wpa(iface, {"status"})));
      for (std::string line; std::getline(lines, line);) {
        if (line.rfind("ssid=", 0) == 0) status.ssid = line.substr(5);
        if (line.rfind("bssid=", 0) == 0) active_bssid = line.substr(6);
      }
      std::istringstream poll(run(wpa(iface, {"signal_poll"})));
      for (std::string line; std::getline(poll, line);)
        if (line.rfind("RSSI=", 0) == 0) status.strength = strength(std::atoi(line.c_str() + 5));
      if (status.primary == Kind::wifi && status.connection == iface && !status.ssid.empty())
        status.connection = status.ssid;

      std::set<std::string> saved;
      std::istringstream networks(run(wpa(iface, {"list_networks"})));
      std::string line;
      std::getline(networks, line);  // header
      while (std::getline(networks, line)) {
        std::istringstream fields(line);
        std::string id, ssid;
        std::getline(fields, id, '\t');
        std::getline(fields, ssid, '\t');
        if (!ssid.empty()) saved.insert(ssid);
      }

      std::istringstream results(run(wpa(iface, {"scan_results"})));
      std::getline(results, line);  // header
      while (std::getline(results, line)) {
        std::istringstream fields(line);
        std::string bssid, frequency, signal, flags, ssid;
        std::getline(fields, bssid, '\t');
        std::getline(fields, frequency, '\t');
        std::getline(fields, signal, '\t');
        std::getline(fields, flags, '\t');
        std::getline(fields, ssid);
        if (ssid.empty()) continue;  // hidden
        AccessPoint point;
        point.ssid = ssid;
        point.bssid = bssid;
        point.frequency = static_cast<std::uint32_t>(std::atoi(frequency.c_str()));
        point.strength = strength(std::atoi(signal.c_str()));
        point.secured = flags.find("WPA") != std::string::npos || flags.find("SAE") != std::string::npos ||
                        flags.find("WEP") != std::string::npos;
        point.active = bssid == active_bssid;
        point.saved = saved.contains(ssid);
        auto same = std::ranges::find(next.access_points, ssid, &AccessPoint::ssid);
        if (same == next.access_points.end()) {
          next.access_points.push_back(point);
        } else if (point.active || (!same->active && point.strength > same->strength)) {
          *same = point;
        }
      }
      std::ranges::sort(next.access_points, [](const AccessPoint& a, const AccessPoint& b) {
        if (a.active != b.active) return a.active;
        return a.strength > b.strength;
      });
    }

    bool changed;
    {
      std::lock_guard lock(mutex_);
      changed = signature(next) != signature(snapshot_);
      snapshot_ = std::move(next);
    }
    if (changed) {
      const std::uint64_t one = 1;
      (void)!write(event_, &one, sizeof one);
    }
  }

 private:
  int event_;
  std::mutex mutex_;
  Snapshot snapshot_;

  static std::string signature(const Snapshot& value) {
    const Status& s = value.status;
    std::string text = std::to_string(static_cast<int>(s.primary)) + s.ssid + s.address + s.connection +
                       std::to_string(s.strength / 10) + (s.wifi_enabled ? "1" : "0") + (s.has_wifi ? "1" : "0");
    for (const auto& point : value.access_points)
      text += '|' + point.ssid + (point.active ? "*" : "") + std::to_string(point.strength / 10);
    return text;
  }
};

Poller& poller() {
  static Poller instance;
  return instance;
}

}  // namespace

bool runix() {
  static const bool detected = [] {
    std::error_code error;
    const bool runit = fs::exists("/run/runit", error) || fs::exists("/etc/runit", error);
    return runit && fs::is_directory(control_directory, error) && !vw::network::status().available;
  }();
  return detected;
}

vw::network::Status status() { return runix() ? poller().snapshot().status : vw::network::status(); }

std::vector<vw::network::AccessPoint> access_points() {
  return runix() ? poller().snapshot().access_points : vw::network::access_points();
}

bool set_wifi_enabled(bool enabled) {
  if (!runix()) return vw::network::set_wifi_enabled(enabled);
  poller().act([enabled](const std::string& iface) {
    run({"rfkill", enabled ? "unblock" : "block", "wlan"});
    run(wpa(iface, {enabled ? "reconnect" : "disconnect"}));
  });
  return true;
}

bool scan() {
  if (!runix()) return vw::network::scan();
  poller().act([](const std::string& iface) {
    run(wpa(iface, {"scan"}));
    std::this_thread::sleep_for(std::chrono::seconds(4));
  });
  return true;
}

bool connect(std::string_view ssid_view, std::string_view password_view) {
  if (!runix()) return vw::network::connect(ssid_view, password_view);
  poller().act([ssid = std::string(ssid_view), password = std::string(password_view)](const std::string& iface) {
    // A saved network is selected as it is, unless a new password came.
    std::istringstream networks(run(wpa(iface, {"list_networks"})));
    std::string line, id;
    std::getline(networks, line);
    while (std::getline(networks, line)) {
      std::istringstream fields(line);
      std::string network_id, network_ssid;
      std::getline(fields, network_id, '\t');
      std::getline(fields, network_ssid, '\t');
      if (network_ssid == ssid) id = network_id;
    }
    if (id.empty() || !password.empty()) {
      if (id.empty()) {
        id = run(wpa(iface, {"add_network"}));
        id.erase(std::remove_if(id.begin(), id.end(), [](char c) { return c < '0' || c > '9'; }), id.end());
        if (id.empty()) return;
        run(wpa(iface, {"set_network", id, "ssid", hex(ssid)}));
      }
      if (password.empty()) {
        run(wpa(iface, {"set_network", id, "key_mgmt", "NONE"}));
      } else {
        run(wpa(iface, {"set_network", id, "psk", "\"" + password + "\""}));
      }
    }
    run(wpa(iface, {"select_network", id}));
    run(wpa(iface, {"enable_network", id}));
    run(wpa(iface, {"save_config"}));
    // dhcpcd, already running, gets the address once wpa_supplicant joins.
    std::this_thread::sleep_for(std::chrono::seconds(4));
  });
  return true;
}

bool disconnect_wifi() {
  if (!runix()) return vw::network::disconnect_wifi();
  poller().act([](const std::string& iface) { run(wpa(iface, {"disconnect"})); });
  return true;
}

int event_fd() { return runix() ? poller().fd() : vw::network::event_fd(); }

bool drain_events() { return runix() ? poller().drain() : vw::network::drain_events(); }

}  // namespace blx::net
