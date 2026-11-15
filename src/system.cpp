#include "system.hpp"

#include <vw/vw.hpp>

#include "halley.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <sys/statvfs.h>
#include <unistd.h>

namespace blx::system {
namespace {

using Clock = std::chrono::steady_clock;

}  // namespace

double cpu() {
  static Clock::time_point sampled{};
  static unsigned long long last_idle = 0;
  static unsigned long long last_total = 0;
  static double usage = 0;
  if (Clock::now() - sampled < std::chrono::seconds(1)) return usage;
  sampled = Clock::now();
  std::ifstream stat("/proc/stat");
  std::string label;
  unsigned long long values[8]{};
  stat >> label;
  for (auto& value : values) stat >> value;
  const unsigned long long idle = values[3] + values[4];
  unsigned long long total = 0;
  for (auto value : values) total += value;
  if (last_total != 0 && total > last_total)
    usage = 1.0 - static_cast<double>(idle - last_idle) / static_cast<double>(total - last_total);
  last_idle = idle;
  last_total = total;
  return usage;
}

double memory() {
  // Asked on every dashboard frame; /proc/meminfo is ~60 lines.
  static Clock::time_point sampled{};
  static double usage = 0;
  if (Clock::now() - sampled < std::chrono::seconds(1)) return usage;
  sampled = Clock::now();
  std::ifstream meminfo("/proc/meminfo");
  std::string key;
  double value = 0;
  double total = 0;
  double available = 0;
  while ((total == 0 || available == 0) && meminfo >> key >> value) {
    if (key == "MemTotal:") total = value;
    if (key == "MemAvailable:") available = value;
    meminfo.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  }
  usage = total > 0 ? 1.0 - available / total : 0;
  return usage;
}

double disk() {
  static Clock::time_point sampled{};
  static double usage = 0;
  if (Clock::now() - sampled < std::chrono::seconds(5)) return usage;
  sampled = Clock::now();
  struct statvfs info {};
  usage = statvfs("/", &info) != 0 || info.f_blocks == 0
              ? 0
              : 1.0 - static_cast<double>(info.f_bavail) / static_cast<double>(info.f_blocks);
  return usage;
}

std::string uptime() {
  // Shown to the minute: refresh every few seconds, not every frame.
  static Clock::time_point sampled{};
  static std::string text;
  if (!text.empty() && Clock::now() - sampled < std::chrono::seconds(5)) return text;
  sampled = Clock::now();
  std::ifstream input("/proc/uptime");
  double seconds = 0;
  input >> seconds;
  const auto total = static_cast<long>(seconds);
  text = total >= 86400 ? std::format("{}d {}h", total / 86400, total / 3600 % 24)
                        : std::format("{}h {}m", total / 3600, total / 60 % 60);
  return text;
}

std::string hostname() {
  static const std::string name = [] {
    std::array<char, 256> buffer{};
    if (gethostname(buffer.data(), buffer.size()) != 0) return std::string("localhost");
    return std::string(buffer.data());
  }();
  return name;
}

std::string os_name() {
  static const std::string name = [] {
    std::ifstream input("/etc/os-release");
    std::string line;
    while (std::getline(input, line)) {
      if (!line.starts_with("PRETTY_NAME=")) continue;
      std::string value = line.substr(12);
      if (value.size() >= 2 && value.front() == '"') value = value.substr(1, value.size() - 2);
      return value;
    }
    return std::string("Linux");
  }();
  return name;
}

std::string avatar() {
  // Drawn on every lock-screen frame: look for ~/.face every few seconds.
  static Clock::time_point checked{};
  static std::string found;
  if (checked != Clock::time_point{} && Clock::now() - checked < std::chrono::seconds(5)) return found;
  checked = Clock::now();
  const char* home = std::getenv("HOME");
  if (home == nullptr) return found = {};
  const auto path = std::filesystem::path(home) / ".face";
  std::error_code error;
  found = std::filesystem::exists(path, error) ? path.string() : std::string();
  return found;
}

namespace {

struct History {
  std::vector<double> cpu;
  std::vector<double> memory;
  Clock::time_point sampled{};
};

History& history() {
  static History instance;
  const auto now = Clock::now();
  if (now - instance.sampled >= std::chrono::seconds(2)) {
    instance.sampled = now;
    const auto push = [](std::vector<double>& samples, double value) {
      samples.push_back(value);
      if (samples.size() > 48) samples.erase(samples.begin());
    };
    push(instance.cpu, cpu());
    push(instance.memory, memory());
  }
  return instance;
}

}  // namespace

std::vector<double> cpu_history() { return history().cpu; }
std::vector<double> memory_history() { return history().memory; }

std::string kernel() {
  static const std::string release = [] {
    std::ifstream input("/proc/sys/kernel/osrelease");
    std::string value;
    std::getline(input, value);
    return value.empty() ? std::string("--") : value;
  }();
  return release;
}

namespace {

std::optional<Window> hyprland_window;
bool hyprland_window_fresh = false;
// Two blocking Hyprland requests: once per compositor event, not per frame.
std::optional<std::vector<Workspace>> hyprland_workspaces;
std::optional<nlohmann::json> zwwm_toplevels_cache;

// A snapshot is a blocking round trip to the compositor: at most one per
// change, not several per frame.
const nlohmann::json& zwwm_toplevels() {
  if (!zwwm_toplevels_cache) zwwm_toplevels_cache = vw::zwwm::snapshot(vw::zwwm::Snapshot::toplevels);
  return *zwwm_toplevels_cache;
}

}  // namespace

void invalidate_window() {
  hyprland_window_fresh = false;
  hyprland_workspaces.reset();
  zwwm_toplevels_cache.reset();
}

std::vector<Workspace> workspaces() {
  std::vector<Workspace> result;
  try {
    // Compositor.qml prefers Halley: its clusters are dynamic workspaces.
    if (halley::available()) {
      for (const auto& cluster : halley::clusters()) {
        const auto key = cluster.slot.value_or(static_cast<int>(cluster.id));
        Workspace workspace{cluster.name.empty() ? std::to_string(key) : cluster.name, cluster.active || cluster.focused,
                            static_cast<int>(cluster.members.size()), {},
                            [slot = cluster.slot] { if (slot) halley::focus_cluster_slot(*slot); }};
        for (const auto& member : cluster.members)
          if (workspace.apps.size() < 3) workspace.apps.push_back(member.app);
        result.push_back(std::move(workspace));
      }
      return result;
    }
    if (vw::zwwm::available()) {
      // zwwm's toplevel snapshot carries no tags: windows show on the active tag.
      std::vector<std::string> visible;
      for (const auto& toplevel : zwwm_toplevels())
        if (visible.size() < 3) visible.push_back(toplevel.value("app_id", ""));
      for (const auto& tag : vw::zwwm::tags()) {
        Workspace workspace{tag.name.empty() ? std::to_string(tag.id) : tag.name, tag.active, 0, {},
                            [id = tag.id] { vw::zwwm::set_active_tags({id}); }};
        if (tag.active) {
          workspace.apps = visible;
          workspace.windows = static_cast<int>(visible.size());
        }
        result.push_back(std::move(workspace));
      }
      return result;
    }
    if (vw::hyprland::available()) {
      if (hyprland_workspaces) return *hyprland_workspaces;
      const int active = vw::hyprland::query("activeworkspace").value("id", 1);
      const auto clients = vw::hyprland::query("clients");
      const int group = (active - 1) / 6 * 6;  // Workspaces.qml: shown 6
      for (int id = group + 1; id <= group + 6; ++id) {
        Workspace workspace{std::to_string(id), id == active, 0, {},
                            [id] { vw::hyprland::dispatch("workspace " + std::to_string(id)); }};
        for (const auto& client : clients)
          if (client.contains("workspace") && client["workspace"].value("id", 0) == id) {
            ++workspace.windows;
            if (workspace.apps.size() < 3) workspace.apps.push_back(client.value("class", ""));
          }
        result.push_back(std::move(workspace));
      }
      hyprland_workspaces = result;
    }
  } catch (const std::exception&) {
  }
  return result;
}

std::optional<Window> active_window() {
  try {
    if (halley::available()) {
      if (const auto node = halley::active_node()) return Window{node->app, node->title};
      return std::nullopt;
    }
    if (vw::zwwm::available()) {
      // zwwm's state bits: 1 mapped, 2 focused, 4 floating, 8 fullscreen.
      for (const auto& toplevel : zwwm_toplevels()) {
        const unsigned state = toplevel.value("state", 0U);
        if ((state & 2U) != 0)
          return Window{toplevel.value("app_id", ""), toplevel.value("title", ""), (state & 8U) != 0};
      }
      return std::nullopt;
    }
    if (vw::hyprland::available()) {
      if (!hyprland_window_fresh) {
        const auto window = vw::hyprland::query("activewindow");
        hyprland_window = std::nullopt;
        if (window.is_object() && window.contains("class"))
          hyprland_window = Window{window.value("class", ""), window.value("title", ""), window.value("fullscreen", 0) > 1};
        hyprland_window_fresh = true;
      }
      return hyprland_window;
    }
  } catch (const std::exception&) {
  }
  return std::nullopt;
}

bool fullscreen() {
  const auto window = active_window();
  return window && window->fullscreen;
}

std::string format_time(const std::string& format) {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_r(&now, &local);
  std::array<char, 128> buffer{};
  const std::size_t length = std::strftime(buffer.data(), buffer.size(), format.c_str(), &local);
  return std::string(buffer.data(), length);
}

std::string keyboard_layout() {
  // KeyboardLayout.qml: the main keyboard's keymap, polled every 2 s.
  static std::string cached;
  static Clock::time_point polled{};
  if (!vw::hyprland::available()) return {};
  if (Clock::now() - polled < std::chrono::seconds(2)) return cached;
  polled = Clock::now();
  try {
    const auto keyboards = vw::hyprland::query("devices").value("keyboards", nlohmann::json::array());
    std::string keymap;
    for (const auto& keyboard : keyboards)
      if (keyboard.value("main", false)) keymap = keyboard.value("active_keymap", "");
    if (keymap.empty() && !keyboards.empty()) keymap = keyboards[0].value("active_keymap", "");
    const auto open = keymap.find('(');
    const auto close = keymap.find(')', open == std::string::npos ? 0 : open);
    std::string source = open != std::string::npos && close != std::string::npos ? keymap.substr(open + 1, close - open - 1) : keymap;
    std::string letters;
    for (char c : source)
      if (std::isalpha(static_cast<unsigned char>(c)) && letters.size() < 3) letters += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    cached = letters.empty() ? "XKB" : letters;
  } catch (const std::exception&) {
    cached.clear();
  }
  return cached;
}

}  // namespace blx::system
