#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

// Data the shell shows that no vw service provides.
namespace blx::system {

// Fractions 0..1. CPU is sampled at most once a second.
double cpu();
double memory();
double disk();
// "1h 52m"
std::string uptime();
std::string hostname();
// /etc/os-release PRETTY_NAME, e.g. "NixOS 26.11 (Zokor)".
std::string os_name();
// ~/.face when it exists.
std::string avatar();

// The last 48 samples, one every two seconds (SystemState.qml).
std::vector<double> cpu_history();
std::vector<double> memory_history();
std::string kernel();

struct Workspace {
  std::string name;
  bool active = false;
  int windows = 0;
  // App ids of up to three of its windows.
  std::vector<std::string> apps;
  std::function<void()> focus;
};
// zwwm tags, else Hyprland workspaces 1..N.
std::vector<Workspace> workspaces();
// The focused window's class and title.
struct Window {
  std::string app;
  std::string title;
  bool fullscreen = false;
};
std::optional<Window> active_window();
// The focused window is fullscreen: the shell hides itself.
bool fullscreen();
// The local time through a strftime format.
std::string format_time(const std::string& format);
// Call on compositor events: active_window() caches its answer until then.
void invalidate_window();
// The main keyboard's layout, e.g. "US" (Hyprland only; empty elsewhere).
std::string keyboard_layout();

}  // namespace blx::system
