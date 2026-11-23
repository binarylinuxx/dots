// blxshell: ~/shell's Quickshell desktop, rebuilt in C++ on libvw.
//
//   blxshell            run the shell
//   blxshell settings   the settings app
//   blxshell <command>  send a command to the running shell (blxshell help)
#include <vw/vw.hpp>

#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <sys/inotify.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "net.hpp"
#include "config.hpp"
#include "ipc.hpp"
#include "packages.hpp"
#include "plugins.hpp"
#include "quickshare.hpp"
#include "services.hpp"
#include "parts.hpp"
#include "settings.hpp"
#include "system.hpp"
#include "state.hpp"
#include "style.hpp"
#include "surfaces.hpp"

namespace {

// Runix: a rebuild switch points /run/current-system at the new system. When
// its blxshell is another build than this one, the shell becomes it, so it
// never keeps running old code after an update. Never while the screen is
// locked: that waits for the unlock.
constexpr const char* system_shell = "/run/current-system/sw/bin/blxshell";
char** shell_argv = nullptr;
int switch_retry_fd = -1;

// /nix/store/<hash>-<name>: one build of the package, whichever file in it.
std::filesystem::path store_path(const std::filesystem::path& file) {
  std::filesystem::path result;
  int parts = 0;
  for (const auto& part : file) {
    result /= part;
    if (++parts == 4) break;
  }
  return result;
}

void follow_system_switch() {
  namespace fs = std::filesystem;
  std::error_code error, self_error;
  const fs::path next = fs::canonical(system_shell, error);
  const fs::path self = fs::canonical("/proc/self/exe", self_error);
  if (error || self_error || store_path(next) == store_path(self)) return;
  if (blx::shell().locked) {
    const itimerspec retry{.it_interval = {5, 0}, .it_value = {5, 0}};
    if (switch_retry_fd >= 0) timerfd_settime(switch_retry_fd, 0, &retry, nullptr);
    return;
  }
  std::cerr << "blxshell: the system switched to " << store_path(next).string() << ", restarting into it\n";
  ::execv(next.c_str(), shell_argv);
  std::cerr << "blxshell: could not restart: " << std::strerror(errno) << '\n';
}

int watch_system_switch() {
  std::error_code error;
  if (!std::filesystem::exists(system_shell, error)) return -1;
  const int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (fd < 0) return -1;
  if (inotify_add_watch(fd, "/run", IN_CREATE | IN_MOVED_TO) < 0) {
    ::close(fd);
    return -1;
  }
  switch_retry_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  return fd;
}

bool drain_system_switch(int fd) {
  alignas(inotify_event) char buffer[4096];
  bool switched = false;
  for (ssize_t got; (got = ::read(fd, buffer, sizeof buffer)) > 0;) {
    for (ssize_t offset = 0; offset < got;) {
      const auto* event = reinterpret_cast<const inotify_event*>(buffer + offset);
      if (event->len > 0 && std::string_view(event->name) == "current-system") switched = true;
      offset += static_cast<ssize_t>(sizeof(inotify_event) + event->len);
    }
  }
  if (switched) follow_system_switch();
  return false;
}

bool drain_switch_retry(int fd) {
  std::uint64_t expirations = 0;
  if (::read(fd, &expirations, sizeof expirations) > 0) follow_system_switch();
  return false;
}

// BLXSHELL_DEBUG_EVENTS=1 prints, every second, which event sources woke
// the shell for a rebuild.
std::function<bool()> counted(const char* name, std::function<bool()> drain) {
  if (std::getenv("BLXSHELL_DEBUG_EVENTS") == nullptr) return drain;
  return [name, drain = std::move(drain)] {
    static std::map<std::string, int> counts;
    static auto last = std::chrono::steady_clock::now();
    const bool result = drain();
    if (result) ++counts[name];
    if (std::chrono::steady_clock::now() - last > std::chrono::seconds(1)) {
      last = std::chrono::steady_clock::now();
      for (const auto& [source, count] : counts) std::cerr << source << '=' << count << ' ';
      std::cerr << '\n';
      counts.clear();
    }
    return result;
  };
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1) {
    const std::string_view command = argv[1];
    if (command == "settings") return blx::run_settings(argc > 2 ? std::string_view(argv[2]) : std::string_view());
    if (command == "help" || command == "--help" || command == "-h") {
      std::cout << blx::ipc::usage() << '\n' << blx::packages::usage();
      return EXIT_SUCCESS;
    }
    // Package commands work on files; the rest go to the running shell.
    if (const auto result = blx::packages::command(argc, argv)) return *result;
    return blx::ipc::send(argc, argv);
  }

  shell_argv = argv;
  const int ipc_fd = blx::ipc::listen();

  blx::sync_style();
  vw::tray::start();
  vw::notifications::start();  // unless another daemon already runs
  blx::quickshare::running();  // LocalSend listens from the start, like QuickShareManager

  vw::App app;
  app.surface(blx::wallpaper_surface)
      .surface(blx::shell_surface)
      .surface(blx::reserve_left)
      .surface(blx::reserve_top)
      .surface(blx::reserve_right)
      .surface(blx::reserve_bottom)
      .surface(blx::lock_surface);
  blx::plugins::sync();
  app.on_event(blx::plugins::event_fd(), counted("plugins", blx::plugins::drain));
  if (ipc_fd >= 0) app.on_event(ipc_fd, counted("ipc", [ipc_fd] { return blx::ipc::drain(ipc_fd); }));
  if (const int switch_fd = watch_system_switch(); switch_fd >= 0) {
    app.on_event(switch_fd, [switch_fd] { return drain_system_switch(switch_fd); });
    if (switch_retry_fd >= 0) app.on_event(switch_retry_fd, [] { return drain_switch_retry(switch_retry_fd); });
  }
  app.on_event(blx::colours_event_fd(), counted("colours", [] {
        if (!blx::reload_colours()) return false;
        blx::plugins::colours_changed();
        return true;
      }))
      .on_event(blx::config_event_fd(), counted("config", [] {
        const blx::Config before = blx::config();
        if (!blx::reload_config()) return false;
        blx::sync_style();
        // The settings app only edits the file: act on what it changed here.
        const blx::Config& now = blx::config();
        if (now.wallpaper_path != before.wallpaper_path) blx::set_wallpaper(now.wallpaper_path);
        if (now.matugen_mode != before.matugen_mode || now.matugen_scheme != before.matugen_scheme ||
            now.matugen_contrast != before.matugen_contrast || now.matugen_prefer != before.matugen_prefer ||
            now.theme_source != before.theme_source || now.theme_seed != before.theme_seed)
          blx::regenerate_colours();
        blx::plugins::sync();
        return true;
      }))
      .on_event(blx::services_event_fd(), counted("services", blx::drain_services))
      .on_event(vw::tray::event_fd(), counted("tray", vw::tray::drain_events))
      .on_event(blx::net::event_fd(), counted("network", blx::net::drain_events))
      .on_event(vw::upower::event_fd(), counted("upower", vw::upower::drain_events))
      .on_event(vw::bluetooth::event_fd(), counted("bluetooth", vw::bluetooth::drain_events))
      .on_event(vw::brightness::event_fd(), counted("brightness", vw::brightness::drain_events))
      .on_event(vw::pipewire::event_fd(), counted("pipewire", vw::pipewire::drain_events))
      .on_event(vw::mpris::event_fd(), counted("mpris", vw::mpris::drain_events))
      .on_event(vw::notifications::event_fd(), counted("notifications", vw::notifications::drain_events));
  try {
    if (vw::zwwm::available()) {
      try {
        vw::zwwm::watch_edge_swipes(vw::zwwm::right | vw::zwwm::bottom);
      } catch (const std::exception&) {}  // an older zwwm without edge swipes
      app.on_event(vw::zwwm::event_fd(), counted("zwwm", [] {
        // zwwm streams events many times a second; only tag changes and
        // edge swipes matter.
        const bool dirty = vw::zwwm::drain_events();
        const bool swiped = blx::apply_edge_swipes(vw::zwwm::take_edge_swipes());
        if (!dirty) return swiped;
        blx::system::invalidate_window();
        // A window entering or leaving fullscreen hides or shows the shell.
        static bool was_fullscreen = false;
        if (const bool fullscreen = blx::system::fullscreen(); fullscreen != was_fullscreen) {
          was_fullscreen = fullscreen;
          return true;
        }
        // Windows opening, closing or changing tag redraw the workspaces pill.
        if (vw::zwwm::last_event() == "window") return true;
        static std::string last;
        std::string now;
        for (const auto& tag : vw::zwwm::tags())
          now += std::to_string(tag.id) + (tag.active ? "*" : "") + tag.name + ';';
        if (now == last) return swiped;
        last = std::move(now);
        return true;
      }));
    }
    else if (vw::hyprland::available())
      app.on_event(vw::hyprland::event_fd(), counted("hyprland", [] {
        blx::system::invalidate_window();
        return vw::hyprland::drain_events();
      }));
  } catch (const std::exception& error) {
    std::cerr << "blxshell: compositor events unavailable: " << error.what() << '\n';
  }
  const int result = app.run();
  blx::keep_awake::set(false);
  blx::plugins::stop_all();
  blx::ipc::close(ipc_fd);
  return result;
}
