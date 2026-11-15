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
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>

#include "config.hpp"
#include "ipc.hpp"
#include "quickshare.hpp"
#include "services.hpp"
#include "parts.hpp"
#include "settings.hpp"
#include "system.hpp"
#include "state.hpp"
#include "style.hpp"
#include "surfaces.hpp"

namespace {

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
    if (command == "settings") return blx::run_settings();
    if (command == "help" || command == "--help" || command == "-h") {
      std::cout << blx::ipc::usage();
      return EXIT_SUCCESS;
    }
    return blx::ipc::send(argc, argv);
  }

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
  if (ipc_fd >= 0) app.on_event(ipc_fd, counted("ipc", [ipc_fd] { return blx::ipc::drain(ipc_fd); }));
  app.on_event(blx::colours_event_fd(), counted("colours", blx::reload_colours))
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
        return true;
      }))
      .on_event(blx::services_event_fd(), counted("services", blx::drain_services))
      .on_event(vw::tray::event_fd(), counted("tray", vw::tray::drain_events))
      .on_event(vw::network::event_fd(), counted("network", vw::network::drain_events))
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
  blx::ipc::close(ipc_fd);
  return result;
}
