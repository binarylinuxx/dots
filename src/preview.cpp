// blxshell-preview: the lock screen on an overlay, without locking the
// session and without the shell, to look at it (or screenshot it) while
// working on it. Close it with Ctrl+C or timeout(1).
//
//   blxshell-preview lock           the page, as it looks when locked
//   blxshell-preview lock --sheet   with the password sheet raised
//
// The greeter previews itself: blxshell-greeter --layer [--sheet].
#include <vw/vw.hpp>

#include <cstdlib>
#include <iostream>
#include <string_view>

#include "lockpage.hpp"
#include "net.hpp"
#include "style.hpp"

int main(int argc, char** argv) {
  bool sheet = false;
  bool known = argc >= 2 && std::string_view(argv[1]) == "lock";
  for (int index = 2; index < argc; ++index) {
    if (std::string_view(argv[index]) == "--sheet") sheet = true;
    else known = false;
  }
  if (!known) {
    std::cerr << "usage: blxshell-preview lock [--sheet]\n";
    return EXIT_FAILURE;
  }

  blx::sync_style();
  vw::App app;
  app.per_output([sheet](vw::Ui& ui, const vw::Output& output) {
    ui.keyboard([](const vw::KeyEvent& key) {
      if (key.name == "Escape") std::exit(EXIT_SUCCESS);
      return true;
    });
    blx::draw_lock_preview(ui, static_cast<float>(output.width), static_cast<float>(output.height), sheet);
    ui.surface({.width = 0, .height = 0, .anchor = vw::anchor::fill, .layer = vw::Layer::overlay, .exclusive_zone = -1,
                .surface_namespace = "blxshell-preview", .refresh_interval = 1});
  });
  app.on_event(blx::colours_event_fd(), blx::reload_colours)
      .on_event(blx::net::event_fd(), blx::net::drain_events)
      .on_event(vw::upower::event_fd(), vw::upower::drain_events)
      .on_event(vw::bluetooth::event_fd(), vw::bluetooth::drain_events)
      .on_event(vw::mpris::event_fd(), vw::mpris::drain_events);
  return app.run();
}
