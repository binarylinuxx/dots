// blxshell-preview: the lock screen on an overlay, without locking the
// session and without the shell, to look at it (or screenshot it) while
// working on it. Close it with Ctrl+C or timeout(1).
//
//   blxshell-preview lock           the page, as it looks when locked
//   blxshell-preview lock --sheet   with the password sheet raised
//   blxshell-preview waves          the wavy slider (drag it) and progress rings
//
// The greeter previews itself: blxshell-greeter --layer [--sheet].
#include <vw/vw.hpp>

#include <cstdlib>
#include <iostream>
#include <string_view>

#include <chrono>
#include <cmath>
#include <string>

#include "draw.hpp"
#include "lockpage.hpp"
#include "net.hpp"
#include "style.hpp"
#include "widgets.hpp"

namespace {

// The wavy widgets on a panel: a slider that plays along like a track and
// can be dragged, and the circular progress kinds.
void draw_waves(vw::Ui& ui, float width, float height) {
  const blx::Colours& c = blx::colours();
  ui.rectangle({.width = width, .height = height, .color = c.surface});
  static const auto start = std::chrono::steady_clock::now();
  static double seek = 0;
  const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  const double played = std::fmod(seek + elapsed / 30, 1.0);
  const float x = width / 2 - 300;
  const float y = height / 2 - 160;
  blx::draw::rect(ui, x - 32, y - 32, 664, 320, 32, c.surface_container);
  blx::draw::text(ui, "Wavy slider: plays along, drag it", x, y, {.size = 15, .color = c.text});
  blx::widgets::material_slider(
      ui, "preview.slider", x, y + 28, 600, 36,
      {.value = played, .track_thickness = 5, .handle_diameter = 18, .wave = true, .wave_amplitude = 2.5F,
       .wave_length = 22, .track = c.surface_variant, .fill = c.primary,
       .released = [elapsed](double to) { seek = to - elapsed / 30; }});
  blx::draw::text(ui, "Wavy, flat, critical, spinning", x, y + 96, {.size = 15, .color = c.text});
  const float ring_y = y + 128;
  blx::widgets::circular_progress(ui, "preview.wavy", x, ring_y, 110,
                                  {.value = played, .accent = c.primary, .track = c.surface_variant, .stroke = 6,
                                   .active = true, .wavy = true, .show_label = false});
  blx::widgets::circular_progress(ui, "preview.flat", x + 160, ring_y, 110,
                                  {.value = 0.62, .accent = c.primary, .track = c.surface_variant, .stroke = 6});
  blx::widgets::circular_progress(ui, "preview.critical", x + 320, ring_y, 110,
                                  {.value = 0.93, .accent = c.primary, .track = c.surface_variant, .stroke = 6,
                                   .critical_threshold = 0.8, .critical = c.error});
  blx::widgets::circular_progress(ui, "preview.spin", x + 480, ring_y, 110,
                                  {.value = 0.3, .accent = c.tertiary, .track = c.surface_variant, .stroke = 6,
                                   .sweep_angle = 280, .wavy = true, .indeterminate = true, .show_label = false});
  vw::request_animation_continuation();
}

}  // namespace

int main(int argc, char** argv) {
  bool sheet = false;
  const bool waves = argc == 2 && std::string_view(argv[1]) == "waves";
  bool known = waves || (argc >= 2 && std::string_view(argv[1]) == "lock");
  for (int index = 2; index < argc && !waves; ++index) {
    if (std::string_view(argv[index]) == "--sheet") sheet = true;
    else known = false;
  }
  if (!known) {
    std::cerr << "usage: blxshell-preview lock [--sheet] | waves\n";
    return EXIT_FAILURE;
  }

  blx::sync_style();
  vw::App app;
  app.per_output([sheet, waves](vw::Ui& ui, const vw::Output& output) {
    ui.keyboard([](const vw::KeyEvent& key) {
      if (key.name == "Escape") std::exit(EXIT_SUCCESS);
      return true;
    });
    if (waves) draw_waves(ui, static_cast<float>(output.width), static_cast<float>(output.height));
    else blx::draw_lock_preview(ui, static_cast<float>(output.width), static_cast<float>(output.height), sheet);
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
