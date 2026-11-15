// The dynamic media island, ported from
// ~/shell/blxshell/components/DynamicMediaIsland.qml: a pill under the top
// edge that announces track changes for five seconds, expands on hover and
// stays while pinned by a click.
#include <vw/vw.hpp>

#include <algorithm>
#include <chrono>
#include <string>

#include "config.hpp"
#include "draw.hpp"
#include "parts.hpp"
#include "services.hpp"
#include "state.hpp"
#include "widgets.hpp"

namespace blx {
namespace {

struct Island {
  bool hovered = false;
  bool pinned = false;
  Clock::time_point announced_until{};
  std::string seen;  // what was last announced
};

Island& island() {
  static Island instance;
  return instance;
}

}  // namespace

Panel island_panel(const Screen& screen, bool suppressed) {
  Island& state = island();
  const auto player = vw::mpris::active();
  // announce() on availability, title, artist, art and playback changes.
  const std::string signature = player ? player->title + '\x1f' + player->artist + '\x1f' + player->art_url + '\x1f' +
                                             player->playback_status
                                       : std::string();
  if (signature != state.seen) {
    state.seen = signature;
    if (player) state.announced_until = Clock::now() + std::chrono::seconds(config().island_announce_seconds);
    else state.pinned = false;
  }
  const bool announced = player && (state.hovered || Clock::now() < state.announced_until);
  if (!state.hovered && player && Clock::now() < state.announced_until) vw::request_animation_continuation();
  const bool expanded = player && (state.pinned || state.hovered);
  const bool shown = !suppressed && (announced || expanded);
  const float reveal = static_cast<float>(vw::animate("island.reveal", shown ? 1 : 0, motion::quick()));
  if (reveal < 0.01F) {
    state.hovered = false;
    return {};
  }
  const float width = static_cast<float>(vw::animate("island.width", expanded ? config().island_expanded_width : config().island_width, motion::bounce()));
  const float height = static_cast<float>(vw::animate("island.height", expanded ? 96 : 42, motion::bounce()));
  // scale: 0.92 + reveal * 0.08, around the centre.
  const float scale = 0.92F + reveal * 0.08F;
  Panel panel;
  panel.reveal = reveal;
  panel.box = {(screen.width - width * scale) / 2, props::frame_thickness + 1 + height * (1 - scale) / 2,
               width * scale, height * scale, blob::Radii::all(expanded ? 24 : height * scale / 2)};
  return panel;
}

void draw_island(vw::Ui& ui, const Panel& panel) {
  if (!panel.visible()) return;
  Island& state = island();
  const Colours& c = colours();
  const blob::Box& box = panel.box;
  // The whole island pins on click and expands while hovered.
  ui.rectangle({.width = box.width, .height = box.height, .radius = box.radii.top_left,
                .color = with_alpha(c.surface, 0), .place = {.x = box.x, .y = box.y},
                .input = {.on_click = [](const vw::ClickEvent&) { island().pinned = !island().pinned; },
                          .on_hover = [](bool inside) {
                            island().hovered = inside;
                            if (!inside) island().announced_until = Clock::now() + std::chrono::seconds(config().island_announce_seconds);
                          }}});
  if (panel.reveal < 0.9F) return;
  const auto player = vw::mpris::active();
  if (!player) return;
  const bool expanded = state.pinned || state.hovered;
  const bool playing = player->playback_status == "Playing";
  const float art = expanded ? 76 : 32;
  const float art_x = box.x + 5;
  const float art_y = box.y + (box.height - art) / 2;
  draw::rect(ui, art_x, art_y, art, art, expanded ? props::popout_radius : art / 2, c.primary_container);
  const std::string path = art::path(player->art_url);
  if (!draw::art(ui, path, art_x, art_y, art, expanded ? props::popout_radius : art / 2))
    draw::glyph(ui, playing ? "graphic_eq" : "music_note", art_x + art / 2, art_y + art / 2, expanded ? 30 : 18, c.primary);

  const float text_x = art_x + art + (expanded ? 13 : 8);
  const float text_right = box.x + box.width - (expanded ? 112 : 12);
  const float text_width = std::max(10.0F, text_right - text_x);
  const std::string title = player->title.empty() ? "Unknown track" : player->title;
  if (expanded) {
    const std::string artist = player->artist.empty() ? player->name : player->artist;
    const float block = 18 + 1 + 13;
    const float ty = box.y + (box.height - block) / 2;
    draw::text(ui, title, text_x, ty, {.size = 14, .color = c.text, .weight = vw::weight::semibold,
                                       .family = font::display, .max_width = text_width});
    draw::text(ui, artist, text_x, ty + 19, {.size = 10, .color = c.text_muted, .family = font::display,
                                             .max_width = text_width});
  } else {
    draw::text_middle(ui, title, text_x, box.y + box.height / 2,
                      {.size = 11, .color = c.text, .weight = vw::weight::semibold, .family = font::display,
                       .max_width = text_width});
  }

  if (!expanded) {
    const float bx = box.x + box.width - 6 - 30;
    const float by = box.y + (box.height - 30) / 2;
    ui.rectangle({.width = 30, .height = 30, .radius = 15, .color = c.surface_variant, .place = {.x = bx, .y = by},
                  .input = {.on_click = [](const vw::ClickEvent&) { vw::mpris::play_pause(); },
                            .hover = vw::Hover{.color = c.surface_container_high, .duration = 0.12F}}});
    draw::glyph(ui, playing ? "pause" : "play_arrow", bx + 15, by + 15, 21, c.primary);
    return;
  }
  // Transport row and the seek slider.
  struct Control {
    const char* glyph;
    bool enabled;
    void (*run)();
  };
  const Control controls[] = {
      {"skip_previous", player->can_go_previous, [] { vw::mpris::previous(); }},
      {playing ? "pause" : "play_arrow", player->can_play || player->can_pause, [] { vw::mpris::play_pause(); }},
      {"skip_next", player->can_go_next, [] { vw::mpris::next(); }},
  };
  float cx = box.x + box.width - 10 - (30 * 3 + 4 * 2);
  for (const auto& control : controls) {
    const float cy = box.y + (box.height - 30) / 2;
    const float alpha = control.enabled ? 1 : 0.35F;
    draw::glyph(ui, control.glyph, cx + 15, cy + 15, 21, with_alpha(c.primary, alpha));
    if (control.enabled)
      ui.rectangle({.width = 30, .height = 30, .radius = 15, .color = with_alpha(c.surface_container_high, 0),
                    .place = {.x = cx, .y = cy},
                    .input = {.on_click = [run = control.run](const vw::ClickEvent&) { run(); },
                              .hover = vw::Hover{.color = c.surface_container_high, .duration = 0.12F}}});
    cx += 34;
  }
  if (player->length > 0)
    widgets::material_slider(ui, "island", text_x, box.y + box.height - 6 - 16, text_width, 16,
                             {.value = player->position / player->length, .track_thickness = 4,
                              .handle_diameter = 14, .handle_width = 8, .handle_progress_gap = 7,
                              .endpoint_padding = 4, .wave = playing, .wave_amplitude = 1.5F, .wave_length = 18,
                              .track = c.surface_variant, .fill = c.primary,
                              .released = [length = player->length](double fraction) {
                                vw::mpris::set_position(fraction * length);
                              }});
}

}  // namespace blx
