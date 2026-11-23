// The sidebar, after ~/shell's blxshell/sidebar and the sidebar parts of
// shell.qml: a notification centre and a utility stack that slide out of the
// right frame edge as you drag the top-right corner inwards.
#include <vw/vw.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <map>
#include <string>
#include <vector>

#include "net.hpp"
#include "config.hpp"
#include "draw.hpp"
#include "notify.hpp"
#include "parts.hpp"
#include "services.hpp"
#include "state.hpp"
#include "system.hpp"
#include "widgets.hpp"

namespace blx {
namespace {

using namespace props;

// Set from the config every layout.
float width = 380;
float tug = 64;
// Quick toggles, after Android 17's Quick Settings: a brightness bar, two wide
// labelled tiles, then a row of four.
constexpr float toggles_padding = utility_medium;
constexpr float brightness_height = 44;
float tile_height = 58;
constexpr float media_height = 96;
constexpr float menu_width = 220;
constexpr float menu_height = 200;

struct RecorderMode {
  const char* glyph;
  const char* title;
  const char* short_title;
  const char* mode;
  bool audio;
};

constexpr RecorderMode modes[] = {
    {"fullscreen", "Record fullscreen", "Fullscreen", "fullscreen", false},
    {"screenshot_region", "Record region", "Region", "region", false},
    {"select_to_speak", "Fullscreen with sound", "Fullscreen", "fullscreen", true},
    {"volume_up", "Region with sound", "Region", "region", true},
};

struct UiState {
  int mode = 0;
  bool menu_open = false;
  bool recordings_expanded = false;
  std::string delete_path;
  float drag_start_reveal = 0;
  float drag_start_stack = 0;
  // What is on screen now, where a drag picks the motion up.
  float shown_reveal = 0;
  float shown_stack = 0;
  float list_scroll = 0;
  std::map<std::string, bool> group_expanded;
  std::map<std::uint32_t, bool> entry_expanded;
  // Horizontal swipes on notification cards (Sidebar.qml's swipeOffset).
  struct Swipe {
    float offset = 0;
    bool dragging = false;
    int axis = 0;     // 0 undecided, 1 horizontal, 2 vertical
    int leaving = 0;  // -1 / +1 once flung past the threshold
  };
  std::map<std::string, Swipe> group_swipe;
  std::map<std::uint32_t, Swipe> entry_swipe;
};

UiState& ui_state() {
  static UiState instance;
  return instance;
}

// The displayed reveal: tracks the pointer exactly while dragging (a zero
// duration keeps the tween on it, so release continues from there), else
// springs.
float follow(const char* id, float target, bool dragging) {
  vw::AnimationOptions options = motion::bounce();
  if (dragging) options.duration = 0;
  return static_cast<float>(vw::animate(id, target, options));
}

float reveal() {
  Shell& state = shell();
  ui_state().shown_reveal = follow("sidebar.reveal", state.sidebar_reveal, state.sidebar_dragging);
  return ui_state().shown_reveal;
}

float stack_reveal() {
  Shell& state = shell();
  ui_state().shown_stack = follow("sidebar.stack", state.stack_reveal, state.stack_dragging);
  return ui_state().shown_stack;
}

float recorder_target() {
  if (!config().sidebar_show_recorder) return 0;
  if (recorder::running()) return 132;
  return ui_state().recordings_expanded ? 396 : 200;
}

float keep_awake_target() {
  if (!config().sidebar_show_keep_awake) return 0;
  return keep_awake::enabled() ? 110 : 72;
}

float clamp01(float value) { return std::clamp(value, 0.0F, 1.0F); }

// Desktops without a backlight have no brightness row.
bool has_backlight() {
  static const bool present = vw::brightness::screen().has_value();
  return present && config().sidebar_show_brightness;
}

bool wide_tiles() { return config().sidebar_tile_wifi || config().sidebar_tile_bluetooth; }

bool small_tiles() {
  const Config& c = config();
  return c.sidebar_tile_mic || c.sidebar_tile_dnd || c.sidebar_tile_share || c.sidebar_tile_settings ||
         c.sidebar_tile_lock;
}

float toggles_height() {
  const auto rows = static_cast<float>(wide_tiles()) + static_cast<float>(small_tiles());
  const float tiles = rows > 0 ? rows * tile_height + (rows - 1) * utility_small : 0;
  const float brightness = has_backlight() ? brightness_height + (rows > 0 ? utility_small : 0) : 0;
  return toggles_padding * 2 + tiles + brightness;
}

struct Layout {
  float x;           // the sidebar's resting left edge
  float stack_y;
  float stack_height;
  float recorder_height;
  float keep_height;
  float media_block;  // the media card and the gap under it; 0 without a player
};

Layout layout(const Screen& screen) {
  const Config& settings = config();
  width = static_cast<float>(std::clamp(settings.sidebar_width, 300, 560));
  tile_height = static_cast<float>(std::clamp(settings.sidebar_tile_height, 44, 84));
  tug = static_cast<float>(std::clamp(settings.sidebar_pull_area, 24, 160));
  Layout l;
  l.x = screen.width - frame_thickness - width + popout_frame_overlap;
  l.recorder_height = static_cast<float>(vw::animate(
      "sidebar.recorder", recorder_target(), {.duration = 0.5, .easing = vw::Easing::out_back}));
  l.keep_height = static_cast<float>(vw::animate(
      "sidebar.keep", keep_awake_target(), {.duration = 0.5, .easing = vw::Easing::out_back}));
  l.media_block = static_cast<float>(vw::animate(
      "sidebar.media", config().sidebar_show_media && vw::mpris::active() ? media_height + utility_medium : 0,
      {.duration = 0.5, .easing = vw::Easing::out_back}));
  l.stack_height = l.media_block + toggles_height() + l.recorder_height + l.keep_height + utility_medium * 2 +
                   utility_large + utility_extra_small;
  l.stack_y = screen.height - bar_height - l.stack_height - 2;
  return l;
}


std::string relative_time(std::int64_t timestamp) { return notify::age(timestamp); }

// --- Notification centre -------------------------------------------------------

constexpr float badge = 58;

float entry_height(const vw::notifications::Notification& entry, bool group_open, float text_width) {
  if (!group_open) return 20;
  if (!ui_state().entry_expanded[entry.id]) return 58;
  const std::string body = notify::plain(entry.body);
  const float lines = std::ceil(draw::measure(body, 11, vw::weight::regular, font::display) /
                                std::max(1.0F, text_width - 20));
  return std::max(58.0F, 20 + 17 + 3 + lines * 14);
}

bool group_open(const notify::Group& group) {
  auto& expanded = ui_state().group_expanded;
  if (!expanded.contains(group.key)) return group.entries.size() == 1;
  return expanded[group.key];
}

// Where a swiped card sits now: on the pointer while dragged, springing back
// when let go short of the threshold, flying out past it.
float swipe_offset(const std::string& id, const UiState::Swipe& swipe, float width) {
  if (swipe.dragging) return static_cast<float>(vw::animate(id, swipe.offset, {.duration = 0}));
  if (swipe.leaving != 0)
    return static_cast<float>(vw::animate(id, static_cast<float>(swipe.leaving) * (width + 24),
                                          {.duration = 0.22, .easing = vw::Easing::out_cubic}));
  return static_cast<float>(vw::animate(id, 0, {.duration = 0.35, .easing = vw::Easing::out_back}));
}

// Sidebar.qml fades a card with its swipe, never below a quarter while held.
float swipe_fade(float offset, float width, bool leaving) {
  const float fade = 1 - std::abs(offset) / std::max(1.0F, width);
  return leaving ? std::max(0.0F, fade) : std::max(0.25F, fade);
}

// The drag handlers shared by groups and entries. `vertical` runs once a
// drag is clearly up or down (groups expand and collapse with it).
template <typename Key>
vw::Input swipe_input(std::map<Key, UiState::Swipe>& swipes, Key key, float width,
                      std::function<void(double dy)> vertical = {}) {
  vw::Input input;
  input.on_drag = [&swipes, key, vertical](const vw::DragEvent& drag) {
    UiState::Swipe& swipe = swipes[key];
    const double dx = drag.x - drag.start_x;
    const double dy = drag.y - drag.start_y;
    if (swipe.axis == 0 && std::max(std::abs(dx), std::abs(dy)) > 6)
      swipe.axis = std::abs(dx) >= std::abs(dy) || !vertical ? 1 : 2;
    if (swipe.axis == 1) {
      swipe.dragging = true;
      swipe.offset = static_cast<float>(dx);
    } else if (swipe.axis == 2 && std::abs(dy) > 24) {
      vertical(dy);
    }
  };
  input.on_drag_end = [&swipes, key, width](const vw::DragEvent& drag) {
    UiState::Swipe& swipe = swipes[key];
    if (!drag.cancelled && swipe.axis == 1 && std::abs(swipe.offset) > width * 0.3F)
      swipe.leaving = swipe.offset < 0 ? -1 : 1;
    swipe.dragging = false;
    swipe.axis = 0;
    swipe.offset = 0;
  };
  return input;
}

float group_height(const notify::Group& group, float card_width) {
  const bool open = group_open(group);
  const float column_width = card_width - 24 - badge - 12;
  float entries = 0;
  int shown = 0;
  for (const auto& entry : group.entries) {
    if (!open && shown >= 3) break;
    entries += entry_height(entry, open, column_width) + (shown > 0 ? (open ? 5 : 1) : 0);
    ++shown;
  }
  const float column = 28 + (open ? 5 : 2) + entries;
  return std::max(badge, column) + 24;
}

// The card's height on screen: grows in when it first appears, follows
// expanding and collapsing (Behavior on height), and closes up to nothing
// once a swiped-away card has left, which is when it is dismissed.
float shown_group_height(const notify::Group& group, float card_width) {
  UiState::Swipe& swipe = ui_state().group_swipe[group.key];
  const bool gone = swipe.leaving != 0 &&
                    std::abs(swipe_offset("sidebar.swipe." + group.key, swipe, card_width)) >= card_width;
  const float target = gone ? 0 : group_height(group, card_width);
  return static_cast<float>(vw::animate("sidebar.group.h." + group.key, target,
                                        {.duration = 0.4, .initial = 0, .easing = vw::Easing::out_cubic}));
}

void draw_group(vw::Ui& ui, const notify::Group& group, float x, float y, float card_width,
                const vw::Clip& clip) {
  const Colours& c = colours();
  UiState& state = ui_state();
  const bool open = group_open(group);
  const bool critical = std::ranges::any_of(group.entries, [](const auto& e) { return e.urgency > 1; });
  const float height = shown_group_height(group, card_width);
  UiState::Swipe& swipe_state = state.group_swipe[group.key];
  const float swipe = swipe_offset("sidebar.swipe." + group.key, swipe_state, card_width);
  // Flown out and closed up: now it really goes.
  if (swipe_state.leaving != 0 && height < 0.5F) {
    state.group_swipe.erase(group.key);
    notify::dismiss_group(group);
    return;
  }
  const float fade = swipe_fade(swipe, card_width, swipe_state.leaving != 0);
  const float left = x + swipe;  // may be negative inside a listview row
  const auto ids = group.ids();
  const std::size_t count = group.entries.size();
  vw::Input card_input = swipe_input<std::string>(state.group_swipe, group.key, card_width, [key = group.key, count](double dy) {
    if (count > 1) ui_state().group_expanded[key] = dy > 0;  // pull down to expand, up to collapse
  });
  card_input.on_click = [key = group.key, ids, count](const vw::ClickEvent& click) {
    if (click.button == 273 && count > 1)
      ui_state().group_expanded[key] = !ui_state().group_expanded[key];
    else if (click.button == 274)
      for (auto id : ids) vw::notifications::dismiss(id);
  };
  const vw::Widget card = draw::capture(ui, card_width, height, [&] {
  ui.rectangle({.width = card_width, .height = height, .radius = utility_radius_large,
                .color = c.surface_container_high, .place = {.x = left, .y = y, .clip = clip},
                .input = std::move(card_input)});

  const auto& latest = group.entries.front();
  // App badge.
  const float badge_x = left + 12;
  const float badge_y = y + 12;
  draw::rect(ui, badge_x, badge_y, badge, badge, badge / 2, critical ? c.error_container : c.secondary_container);
  if (!latest.image.empty())
    draw::icon(ui, latest.image, "", badge_x, badge_y, badge, std::nullopt, badge / 2, vw::ImageFit::cover);
  else if (!latest.icon.empty())
    draw::icon(ui, latest.icon, "", badge_x + 11.5F, badge_y + 11.5F, 35);
  else
    draw::glyph(ui, critical ? "notification_important" : "notifications", badge_x + badge / 2, badge_y + badge / 2, 30,
                critical ? c.error_container_text : c.text);

  // Header: app, age, count chip.
  const float column_x = badge_x + badge + 12;
  const float column_width = card_width - 24 - badge - 12;
  const float chip_width = group.entries.size() > 1 ? 50 : 30;
  const std::string age = relative_time(latest.timestamp);
  const float age_width = draw::measure(age, 11, vw::weight::regular, font::display);
  draw::text_middle(ui, latest.app_name.empty() ? "System" : latest.app_name, column_x, y + 12 + 14,
                    {.size = 13, .color = c.text, .weight = vw::weight::semibold, .family = font::display,
                     .max_width = std::max(10.0F, column_width - age_width - chip_width - 14)});
  draw::text_middle(ui, age, column_x + column_width - chip_width - 7 - age_width, y + 12 + 14,
                    {.size = 11, .color = c.outline, .family = font::display});
  const float chip_x = column_x + column_width - chip_width;
  const vw::Color chip_text = critical ? c.error_container_text : c.text;
  draw::rect(ui, chip_x, y + 13, chip_width, 26, 13, critical ? c.error_container : c.surface_container_highest);
  draw::text_middle(ui, std::to_string(group.entries.size()),
                    chip_x + (group.entries.size() > 1 ? 10 : 11), y + 26,
                    {.size = 12, .color = chip_text, .weight = vw::weight::semibold, .family = font::display});
  if (group.entries.size() > 1) {
    draw::glyph(ui, open ? "expand_less" : "expand_more", chip_x + 33, y + 26, 18, chip_text);
    draw::state_layer(ui, chip_x, y + 13, chip_width, 26, 13, c.text,
                      {.on_click = [key = group.key, open](const vw::ClickEvent&) {
                        ui_state().group_expanded[key] = !open;
                      }});
  }

  // Entries: compact lines, or cards when the group is open.
  float entry_y = y + 12 + 28 + (open ? 5 : 2);
  int shown = 0;
  for (const auto& entry : group.entries) {
    if (!open && shown >= 3) break;
    const float h = static_cast<float>(vw::animate(std::format("sidebar.entry.h.{}", entry.id),
                                                   entry_height(entry, open, column_width),
                                                   {.duration = 0.3, .easing = vw::Easing::out_cubic}));
    const std::string body = notify::plain(entry.body);
    if (!open) {
      const float summary_width = body.empty() ? column_width
                                               : std::min(draw::measure(entry.summary, 11, vw::weight::semibold, font::display),
                                                          column_width * 0.52F);
      draw::text_middle(ui, entry.summary, column_x, entry_y + h / 2,
                        {.size = 11, .color = c.text, .weight = vw::weight::semibold, .family = font::display,
                         .max_width = summary_width});
      if (!body.empty())
        draw::text_middle(ui, body, column_x + summary_width + 6, entry_y + h / 2,
                          {.size = 11, .color = c.text_muted, .family = font::display,
                           .max_width = std::max(10.0F, column_width - summary_width - 6)});
    } else {
      UiState::Swipe& entry_state = state.entry_swipe[entry.id];
      const std::string swipe_id = std::format("sidebar.entry.{}", entry.id);
      const float entry_swipe = swipe_offset(swipe_id, entry_state, column_width);
      if (entry_state.leaving != 0 && std::abs(entry_swipe) >= column_width) {
        // Out of the card: dismiss it; the entries below close the gap as
        // the card's height follows.
        state.entry_swipe.erase(entry.id);
        vw::notifications::dismiss(entry.id);
        continue;
      }
      const float ex = column_x + entry_swipe;
      vw::Input entry_input = swipe_input<std::uint32_t>(state.entry_swipe, entry.id, column_width);
      entry_input.on_click = [id = entry.id](const vw::ClickEvent&) {
        ui_state().entry_expanded[id] = !ui_state().entry_expanded[id];
      };
      const vw::Widget entry_widget = draw::capture(ui, column_width, h, [&] {
      ui.rectangle({.width = column_width, .height = h, .radius = 14,
                    .color = entry.urgency > 1 ? c.secondary_container : c.surface_container_highest,
                    .place = {.x = ex, .y = entry_y, .clip = clip},
                    .input = std::move(entry_input)});
      const std::string time = relative_time(entry.timestamp);
      const float time_width = draw::measure(time, 10, vw::weight::regular, font::display);
      draw::text(ui, entry.summary, ex + 10, entry_y + 10,
                 {.size = 12, .color = c.text, .weight = vw::weight::semibold, .family = font::display,
                  .max_width = column_width - 28 - time_width});
      draw::text(ui, time, ex + column_width - 10 - time_width, entry_y + 11,
                 {.size = 10, .color = c.outline, .family = font::display});
      if (!body.empty())
        draw::text(ui, body, ex + 10, entry_y + 30,
                   {.size = 11, .color = c.text_muted, .family = font::display,
                    .max_width = column_width - 20,
                    .max_lines = ui_state().entry_expanded[entry.id] ? 0 : 1});
      });
      // Text follows the card as it grows and shrinks, and fades with a swipe.
      ui.clip(entry_widget, {ex, entry_y, column_width, h, 14});
      ui.fade(entry_widget, swipe_fade(entry_swipe, column_width, entry_state.leaving != 0));
    }
    entry_y += h + (open ? 5 : 1);
    ++shown;
  }
  });
  // Contents stay inside the (animating) card and fade with its swipe.
  ui.clip(card, {left, y, card_width, height, utility_radius_large});
  ui.fade(card, fade);
}

void draw_centre(vw::Ui& ui, const Panel& panel, float slide) {
  const Colours& c = colours();
  (void)slide;
  const float container_x = panel.box.x + utility_large;
  const float container_y = panel.box.y + utility_extra_small;
  const float container_width = width - utility_large - utility_extra_small;
  const float container_height = panel.box.height - 4 - utility_extra_small - utility_large - 1;
  draw::rect(ui, container_x, container_y, container_width, container_height, utility_radius_large,
             c.surface_container);

  const auto groups = notify::history();
  std::size_t count = 0;
  for (const auto& group : groups) count += group.entries.size();

  // Header.
  const float header_y = container_y + 16;
  float label_x = container_x + 18;
  const draw::TextStyle header_style{.size = 12, .color = c.outline, .weight = vw::weight::semibold,
                                     .family = font::display};
  if (count > 0) {
    const vw::Widget number = draw::text_middle(ui, std::to_string(count), label_x, header_y + 21, header_style);
    label_x += number.width + 6;
  }
  draw::text_middle(ui, count == 0 ? "Notifications" : count == 1 ? "notification" : "notifications",
                    label_x, header_y + 21, header_style);
  if (count > 0) {
    const float bx = container_x + container_width - 18 - 36;
    draw::glyph(ui, "clear_all", bx + 18, header_y + 21, 21, c.primary);
    draw::state_layer(ui, bx, header_y + 3, 36, 36, 18, c.primary,
                      {.on_click = [](const vw::ClickEvent&) { notify::clear_history(); }});
  }

  // List.
  const float list_x = container_x + 12;
  const float list_y = header_y + 42 + 8;
  const float list_width = container_width - 24;
  const float list_height = container_y + container_height - 12 - list_y;
  if (list_height < 20) return;
  if (count == 0) {
    const float cy = list_y + list_height / 2;
    draw::glyph(ui, "notifications_off", list_x + list_width / 2, cy - 22, 72, c.outline_variant);
    draw::text_centered(ui, "All up to date!", list_x + list_width / 2, cy + 34,
                        {.size = 20, .color = c.outline_variant, .weight = vw::weight::semibold,
                         .family = font::display});
    return;
  }
  // Virtualized: only the groups in view are built; groups vary in height.
  const vw::Clip none{.x = -1e5F, .y = -1e5F, .width = 2e5F, .height = 2e5F};
  ui.listview({.id = "sidebar-notifications", .width = list_width, .height = list_height, .gap = 10,
               .scroll_step = 120,
               .count = groups.size(),
               .render = [&](std::size_t index) -> std::optional<vw::Widget> {
                 const float h = shown_group_height(groups[index], list_width);
                 return draw::capture(ui, list_width, h, [&] { draw_group(ui, groups[index], 0, 0, list_width, none); });
               },
               .row_height = [&](std::size_t index) { return shown_group_height(groups[index], list_width); },
               .place = {.x = list_x, .y = list_y}});
  // Scroll indicator.
  float content = 0;
  for (const auto& group : groups) content += shown_group_height(group, list_width) + 10;
  content -= 10;
  const float scroll = vw::scroll_offset("sidebar-notifications");
  // Edge fades over whatever is scrolled out of view (Sidebar.qml).
  if (scroll > 2)
    ui.rectangle({.width = list_width, .height = 22, .color = c.surface_container,
                  .color2 = with_alpha(c.surface_container, 0), .gradient = vw::Gradient::vertical,
                  .place = {.x = list_x, .y = list_y}});
  if (content > list_height && scroll < content - list_height - 2)
    ui.rectangle({.width = list_width, .height = 22, .color = with_alpha(c.surface_container, 0),
                  .color2 = c.surface_container, .gradient = vw::Gradient::vertical,
                  .place = {.x = list_x, .y = list_y + list_height - 22}});
  if (content > list_height) {
    const float thumb = std::max(28.0F, list_height * list_height / content);
    const float thumb_y = list_y + (list_height - thumb) * scroll / std::max(1.0F, content - list_height);
    draw::rect(ui, list_x + list_width - 3, thumb_y, 3, thumb, 1.5F, with_alpha(c.outline, 0.5F));
  }
}

// --- Utility stack -------------------------------------------------------------

// Off: a dim pill. On: a filled rounded rectangle, reached with a bounce.
void draw_tile(vw::Ui& ui, const std::string& key, float x, float y, float width, const std::string& glyph, bool active,
               const std::string& label, const std::string& detail, std::function<void()> run) {
  const Colours& c = colours();
  const float on = static_cast<float>(vw::animate("toggle." + key, active ? 1 : 0, {.duration = 0.35, .easing = vw::Easing::out_back}));
  const float tint = clamp01(on);
  const float radius = std::max(0.0F, tile_height / 2 + (tile_height * config().sidebar_tile_radius - tile_height / 2) * on);
  const vw::Color content = mix(c.text, c.primary_text, tint);
  draw::rect(ui, x, y, width, tile_height, radius, mix(c.surface_container_highest, c.primary, tint));
  if (label.empty()) {
    draw::glyph(ui, glyph, x + width / 2, y + tile_height / 2, 24, content);
  } else {
    const float text_x = x + 16 + 24 + 10;
    const float text_width = x + width - 12 - text_x;
    draw::glyph(ui, glyph, x + 16 + 12, y + tile_height / 2, 24, content);
    draw::text_middle(ui, label, text_x, y + tile_height / 2 - 8,
                      {.size = 13, .color = content, .weight = vw::weight::semibold, .family = font::utility,
                       .max_width = text_width});
    draw::text_middle(ui, detail, text_x, y + tile_height / 2 + 9,
                      {.size = 11, .color = mix(c.text_muted, c.primary_text, tint), .family = font::utility,
                       .max_width = text_width});
  }
  draw::state_layer(ui, x, y, width, tile_height, radius, active ? c.primary_text : c.text,
                    {.on_click = [run = std::move(run)](const vw::ClickEvent&) { run(); }});
}

// A blocky bar, a thin handle standing clear of it, the rest of the track with
// the icon at its far end.
void draw_brightness(vw::Ui& ui, float x, float y, float w) {
  const auto backlight = vw::brightness::screen();
  if (!backlight) return;
  const Colours& c = colours();
  constexpr float gap = 4;
  constexpr float handle = 4;
  constexpr float outer = 16;
  constexpr float inner = 4;
  const float travel = w - gap * 2 - handle;
  const float filled = travel * static_cast<float>(backlight->fraction());
  const float handle_x = x + filled + gap;
  const float rest_x = handle_x + handle + gap;
  const float rest = x + w - rest_x;
  if (filled >= 1)
    ui.rectangle({.width = filled, .height = brightness_height, .color = c.primary,
                  .corner_radii = {outer, inner, inner, outer}, .place = {.x = x, .y = y}});
  if (rest >= 1)
    ui.rectangle({.width = rest, .height = brightness_height, .color = c.surface_container_highest,
                  .corner_radii = {inner, outer, outer, inner}, .place = {.x = rest_x, .y = y}});
  draw::rect(ui, handle_x, y - 3, handle, brightness_height + 6, handle / 2, c.primary);
  const float icon_x = x + w - 26;
  draw::glyph(ui, "brightness_medium", icon_x, y + brightness_height / 2, 22, handle_x > icon_x ? c.primary_text : c.text);
  const auto at = [x, travel](double px) {
    const double value = std::clamp((px - x - gap) / travel, 0.0, 1.0);
    // Writes go through logind: skip what the eye cannot see.
    static double written = -1;
    if (std::abs(value - written) < 0.005) return;
    written = value;
    vw::brightness::set_screen(value);
  };
  ui.hit_area({.width = w, .height = brightness_height, .place = {.x = x, .y = y},
               .input = {.on_click = [at](const vw::ClickEvent& e) { at(e.x); },
                         .on_drag_start = [at](const vw::DragEvent& e) { at(e.x); },
                         .on_drag = [at](const vw::DragEvent& e) { at(e.x); }}});
}

void draw_toggles(vw::Ui& ui, float x, float y, float w) {
  draw::rect(ui, x, y, w, toggles_height(), utility_radius_large, colours().surface_container);
  const auto network = net::status();
  const auto adapter = vw::bluetooth::adapter();
  const auto source = vw::pipewire::default_source();
  const bool wifi = network.wifi_enabled;
  const bool bluetooth = adapter && adapter->powered;
  const bool microphone = source && !source->muted;
  const bool quiet = vw::notifications::do_not_disturb();
  std::string bluetooth_detail = bluetooth ? "On" : "Off";
  for (const auto& device : vw::bluetooth::devices())
    if (device.connected) {
      bluetooth_detail = device.name;
      break;
    }

  const float row_x = x + toggles_padding;
  const float row_width = w - toggles_padding * 2;
  float row_y = y + toggles_padding;
  if (has_backlight()) {
    draw_brightness(ui, row_x, row_y, row_width);
    row_y += brightness_height + utility_small;
  }
  const Config& settings = config();
  if (wide_tiles()) {
    const bool both = settings.sidebar_tile_wifi && settings.sidebar_tile_bluetooth;
    const float wide = both ? (row_width - utility_small) / 2 : row_width;
    float tile_x = row_x;
    if (settings.sidebar_tile_wifi) {
      draw_tile(ui, "wifi", tile_x, row_y, wide, wifi ? "wifi" : "wifi_off", wifi, "Wi-Fi",
                !wifi ? "Off" : network.ssid.empty() ? "On" : network.ssid,
                [wifi] { net::set_wifi_enabled(!wifi); });
      tile_x += wide + utility_small;
    }
    if (settings.sidebar_tile_bluetooth)
      draw_tile(ui, "bluetooth", tile_x, row_y, wide, bluetooth ? "bluetooth" : "bluetooth_disabled", bluetooth, "Bluetooth",
                bluetooth_detail, [bluetooth] { vw::bluetooth::set_powered(!bluetooth); });
    row_y += tile_height + utility_small;
  }

  struct Tile {
    const char* key;
    std::string glyph;
    bool active;
    std::function<void()> run;
  };
  std::vector<Tile> tiles;
  if (settings.sidebar_tile_mic)
    tiles.push_back({"mic", microphone ? "mic" : "mic_off", microphone,
                     [id = source ? source->id : 0U] { if (id != 0) vw::pipewire::toggle_mute(id); }});
  if (settings.sidebar_tile_dnd)
    tiles.push_back({"dnd", quiet ? "do_not_disturb_on" : "do_not_disturb_off", quiet,
                     [quiet] { vw::notifications::set_do_not_disturb(!quiet); }});
  if (settings.sidebar_tile_share)
    tiles.push_back({"share", "phone_android", false, [] {
                       shell().quick_share_open = true;
                       shell().sidebar_reveal = 0;
                       shell().stack_reveal = 0;
                     }});
  if (settings.sidebar_tile_settings)
    tiles.push_back({"settings", "settings", false, [] {
                       shell().sidebar_reveal = 0;
                       shell().stack_reveal = 0;
                       vw::spawn({system::self_path(), "settings"});
                     }});
  if (settings.sidebar_tile_lock) tiles.push_back({"lock", "lock", false, [] { lock_session(); }});
  if (tiles.empty()) return;
  const auto count = static_cast<float>(tiles.size());
  const float cell = (row_width - utility_small * (count - 1)) / count;
  for (std::size_t index = 0; index < tiles.size(); ++index) {
    const Tile& tile = tiles[index];
    draw_tile(ui, tile.key, row_x + static_cast<float>(index) * (cell + utility_small), row_y, cell, tile.glyph, tile.active, "", "", tile.run);
  }
}

// The active player: thumbnail, track, a pill play button, and previous, seek
// and next along the bottom. Grows in and out with the player.
void draw_media(vw::Ui& ui, float x, float y, float w, float h) {
  if (h < 1) return;
  const Colours& c = colours();
  draw::rect(ui, x, y, w, h, utility_radius_large, c.surface_container);
  const auto player = vw::mpris::active();
  if (!player || h < media_height - 1) return;
  const bool playing = player->playback_status == "Playing";

  constexpr float padding = 12;
  constexpr float thumbnail = media_height - padding * 2;
  const float thumbnail_x = x + padding;
  const float thumbnail_y = y + padding;
  draw::rect(ui, thumbnail_x, thumbnail_y, thumbnail, thumbnail, 14, c.primary_container);
  if (!draw::art(ui, art::path(player->art_url), thumbnail_x, thumbnail_y, thumbnail, 14))
    draw::glyph(ui, playing ? "graphic_eq" : "music_note", thumbnail_x + thumbnail / 2, thumbnail_y + thumbnail / 2, 30,
                c.primary);

  // Playing squares the pill off, as the M3 Expressive play button does.
  constexpr float play_width = 52;
  constexpr float play_height = 40;
  const float play_x = x + w - padding - play_width;
  const float play_y = y + padding - 2;
  const float play_radius = std::max(0.0F, static_cast<float>(vw::animate(
      "sidebar.media.play", playing ? 12 : play_height / 2, {.duration = 0.35, .easing = vw::Easing::out_back})));
  draw::rect(ui, play_x, play_y, play_width, play_height, play_radius, c.primary);
  draw::glyph(ui, playing ? "pause" : "play_arrow", play_x + play_width / 2, play_y + play_height / 2, 24, c.primary_text);
  draw::state_layer(ui, play_x, play_y, play_width, play_height, play_radius, c.primary_text,
                    {.on_click = [](const vw::ClickEvent&) { vw::mpris::play_pause(); }});

  const float text_x = thumbnail_x + thumbnail + padding;
  const float text_width = std::max(10.0F, play_x - 8 - text_x);
  draw::text(ui, player->title.empty() ? "Unknown track" : player->title, text_x, y + padding,
             {.size = 14, .color = c.text, .weight = vw::weight::semibold, .family = font::display, .max_width = text_width});
  draw::text(ui, player->artist.empty() ? player->name : player->artist, text_x, y + padding + 19,
             {.size = 11, .color = c.text_muted, .family = font::display, .max_width = text_width});

  constexpr float control = 28;
  const float row_y = y + media_height - padding - control;
  const auto transport = [&](float cx, const char* glyph, bool enabled, void (*run)()) {
    draw::glyph(ui, glyph, cx + control / 2, row_y + control / 2, 20, with_alpha(c.text, enabled ? 1 : 0.35F));
    if (enabled)
      draw::state_layer(ui, cx, row_y, control, control, control / 2, c.text,
                        {.on_click = [run](const vw::ClickEvent&) { run(); }});
  };
  transport(text_x - 4, "skip_previous", player->can_go_previous, [] { vw::mpris::previous(); });
  const float right = x + w - padding;
  transport(right - control, "skip_next", player->can_go_next, [] { vw::mpris::next(); });
  const float slider_x = text_x - 4 + control + 4;
  const float slider_width = right - control - 4 - slider_x;
  if (player->length > 0)
    widgets::material_slider(ui, "sidebar-media", slider_x, row_y + (control - 16) / 2, slider_width, 16,
                             {.value = player->position / player->length, .track_thickness = 4,
                              .handle_diameter = 14, .handle_width = 4, .handle_progress_gap = 4,
                              .endpoint_padding = 4,
                              // The wave redraws on the CPU every frame: not during a pull.
                              .wave = playing && !shell().sidebar_dragging && !shell().stack_dragging,
                              .wave_amplitude = 1.5F, .wave_length = 18,
                              .track = c.surface_variant, .fill = c.primary,
                              .released = [length = player->length](double fraction) {
                                vw::mpris::set_position(fraction * length);
                              }});
}

void draw_keep_awake(vw::Ui& ui, float x, float y, float w, float h) {
  if (h < 8) return;
  const Colours& c = colours();
  const bool on = keep_awake::enabled();
  draw::rect(ui, x, y, w, h, utility_radius_large, c.surface_container);
  const float row_y = y + utility_large;
  draw::rect(ui, x + utility_large, row_y, 40, 40, 20, on ? c.secondary : c.secondary_container);
  draw::glyph(ui, "coffee", x + utility_large + 20, row_y + 21, 24, on ? c.primary_text : c.text);
  const float text_x = x + utility_large + 40 + utility_medium;
  const float text_width = w - utility_large * 2 - 40 - 51 - utility_medium * 2;
  draw::text(ui, "Keep Awake", text_x, row_y + 2,
             {.size = 14, .color = c.text, .family = font::utility, .max_width = text_width});
  draw::text(ui, on ? "Preventing sleep mode" : "Normal power management", text_x, row_y + 21,
             {.size = 12, .color = c.text_muted, .family = font::utility, .max_width = text_width});
  // Switch.
  const float sx = x + w - utility_large - 51;
  const float sy = row_y + 5;
  draw::rect(ui, sx, sy, 51, 30, 15, on ? c.primary : c.surface_container_highest);
  // Tween the travel within the switch, not the screen x, so the thumb stays
  // put while the panel itself is dragged.
  const float travel = static_cast<float>(
      vw::animate("keep.thumb", on ? 1 : 0, {.duration = 0.35, .easing = vw::Easing::out_back}));
  const float thumb_x = sx + 2 + travel * (51 - 26 - 4);
  draw::rect(ui, thumb_x, sy + 2, 26, 26, 13, on ? c.primary_text : c.outline);
  draw::glyph(ui, on ? "check" : "close", thumb_x + 13, sy + 15, 15,
              on ? c.primary : c.surface_container_highest);
  draw::state_layer(ui, sx, sy, 51, 30, 15, on ? c.primary_text : c.text,
                    {.on_click = [on](const vw::ClickEvent&) { keep_awake::set(!on); }});
  if (on && h > 100) {
    const auto since = std::chrono::zoned_time{std::chrono::current_zone(), keep_awake::since()};
    const std::string label = std::format("Active since {:%I:%M %p}", since);
    const float lw = draw::measure(label, 11, vw::weight::regular, font::utility) + 24;
    draw::rect(ui, x + utility_large, y + h - utility_large - 21, lw, 21, 10.5F, c.primary);
    draw::text_middle(ui, label, x + utility_large + 12, y + h - utility_large - 10.5F,
                      {.size = 11, .color = c.primary_text, .family = font::utility});
  }
}

void draw_recorder(vw::Ui& ui, float x, float y, float w, float h) {
  if (h < 8) return;
  const Colours& c = colours();
  UiState& state = ui_state();
  const bool running = recorder::running();
  const bool paused = recorder::paused();
  const RecorderMode& mode = modes[state.mode];
  draw::rect(ui, x, y, w, h, utility_radius_large, c.surface_container);

  // Header.
  const float hx = x + 16;
  const float hy = y + 16;
  draw::rect(ui, hx, hy, 40, 40, 20, running ? c.secondary : c.secondary_container);
  draw::glyph(ui, "screen_record", hx + 20, hy + 21, 24, running ? c.primary_text : c.text);
  const std::string error = recorder::error();
  draw::text(ui, "Screen Recorder", hx + 52, hy + 2, {.size = 14, .color = c.text, .family = font::utility});
  draw::text(ui, paused ? "Paused" : running ? "Running..." : error.empty() ? "Ready" : error, hx + 52,
             hy + 21, {.size = 12, .color = error.empty() || running ? c.text_muted : c.error,
                       .family = font::utility, .max_width = w - 52 - 32 - 150});
  if (!running) {
    // An M3 split button: one capsule cut in two, full-round outside and
    // small inside; the menu half becomes a circle while its menu is open.
    constexpr float button_height = 38;
    constexpr float outer = button_height / 2;
    constexpr float inner = 4;
    const float menu_inner = static_cast<float>(
        vw::animate("recorder.split", state.menu_open ? outer : inner, {.duration = 0.25, .easing = vw::Easing::out_cubic}));
    const draw::Corners leading{outer, inner, inner, outer};
    const draw::Corners trailing{menu_inner, outer, outer, menu_inner};
    const float bx = x + w - 16 - 38 - 2 - 108;
    draw::rect(ui, bx, hy + 1, 108, button_height, leading, c.primary);
    const float label_width = draw::measure(mode.short_title, 12, vw::weight::medium, font::utility);
    const float content_width = 20 + utility_small + label_width;
    draw::glyph(ui, mode.glyph, bx + (108 - content_width) / 2 + 10, hy + 20, 20, c.primary_text);
    draw::text_middle(ui, mode.short_title, bx + (108 - content_width) / 2 + 20 + utility_small, hy + 20,
                      {.size = 12, .color = c.primary_text, .weight = vw::weight::medium, .family = font::utility});
    draw::state_layer(ui, bx, hy + 1, 108, button_height, leading, c.primary_text,
                      {.on_click = [](const vw::ClickEvent&) {
                        const RecorderMode& m = modes[ui_state().mode];
                        recorder::start(m.mode, m.audio, "screen");
                      }});
    const float mx = x + w - 16 - 38;
    draw::rect(ui, mx, hy + 1, 38, button_height, trailing, c.primary);
    draw::glyph(ui, state.menu_open ? "expand_less" : "expand_more", mx + 19, hy + 20, 22, c.primary_text);
    draw::state_layer(ui, mx, hy + 1, 38, button_height, trailing, c.primary_text,
                      {.on_click = [](const vw::ClickEvent&) {
                        ui_state().delete_path.clear();
                        ui_state().menu_open = !ui_state().menu_open;
                      }});
  }

  const float body_x = x + utility_large;
  const float body_y = y + 68;
  const float body_width = w - 32;
  if (running) {
    const float row_y = y + h - utility_large - 48;
    const float pulse = paused ? 1 : 0.55F + 0.45F * static_cast<float>(
        0.5 + 0.5 * std::sin(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count() * 2.6));
    if (!paused) vw::request_animation_continuation();
    const std::string rec = paused ? "PAUSED" : "REC";
    const float rec_width = draw::measure(rec, 12, vw::weight::bold, font::utility) + 24;
    draw::rect(ui, body_x, row_y + 9, rec_width, 30, 15,
               with_alpha(paused ? c.tertiary : c.error, pulse));
    draw::text_middle(ui, rec, body_x + 12, row_y + 24,
                      {.size = 12, .color = paused ? c.primary_text : c.error_text, .weight = vw::weight::bold,
                       .family = font::utility});
    const int elapsed = recorder::elapsed();
    const std::string time = elapsed >= 3600 ? std::format("{}:{:02}:{:02}", elapsed / 3600, elapsed / 60 % 60, elapsed % 60)
                                             : std::format("{}:{:02}", elapsed / 60, elapsed % 60);
    draw::text_middle(ui, "Recording for " + time, body_x + rec_width + utility_medium, row_y + 24,
                      {.size = 14, .color = c.text, .family = font::utility,
                       .max_width = body_width - rec_width - 84 - utility_medium * 2});
    const auto control = [&](float cx, const char* glyph, bool stop, std::function<void()> run) {
      draw::rect(ui, cx, row_y + 4, 40, 40, 20, stop ? c.error : c.secondary_container);
      draw::glyph(ui, glyph, cx + 20, row_y + 25, 22, stop ? c.error_text : c.text);
      draw::state_layer(ui, cx, row_y + 4, 40, 40, 20, stop ? c.error_text : c.text,
                        {.on_click = [run](const vw::ClickEvent&) { run(); }});
    };
    control(x + w - utility_large - 40, "stop", true, [] { recorder::stop(); });
    control(x + w - utility_large - 84, paused ? "play_arrow" : "pause", false, [] { recorder::toggle_pause(); });
    return;
  }

  if (!state.delete_path.empty()) {
    draw::rect(ui, body_x, body_y, body_width, 116, utility_radius_large, c.surface_container_high);
    draw::text(ui, "Delete recording?", body_x + 16, body_y + 16,
               {.size = 16, .color = c.text, .weight = vw::weight::medium, .family = font::utility});
    draw::text(ui, "This permanently removes " + recorder::display_name(state.delete_path) + ".", body_x + 16,
               body_y + 44, {.size = 12, .color = c.text_muted, .family = font::utility,
                             .max_width = body_width - 32, .max_lines = 2});
    float bx = body_x + body_width - 12;
    for (const auto& [label, destructive] : {std::pair{"Delete", true}, {"Cancel", false}}) {
      const float bw = draw::measure(label, 13, vw::weight::medium, font::utility) + 16;
      bx -= bw;
      draw::text_middle(ui, label, bx + 8, body_y + 116 - 12 - 14,
                        {.size = 13, .color = destructive ? c.error : c.primary, .weight = vw::weight::medium,
                         .family = font::utility});
      draw::state_layer(ui, bx, body_y + 116 - 12 - 28, bw, 28, 14, destructive ? c.error : c.primary,
                        {.on_click = [destructive](const vw::ClickEvent&) {
                          if (destructive) recorder::remove(ui_state().delete_path);
                          ui_state().delete_path.clear();
                        }});
      bx -= utility_medium;
    }
    return;
  }

  // Recordings list.
  draw::glyph(ui, "list", body_x + 12, body_y + 16, 24, c.text);
  draw::text_middle(ui, "Recordings", body_x + 36, body_y + 16, {.size = 14, .color = c.text, .family = font::utility});
  draw::glyph(ui, state.recordings_expanded ? "unfold_less" : "unfold_more", body_x + body_width - 16,
              body_y + 16, 20, c.text_muted);
  draw::state_layer(ui, body_x, body_y, body_width, 32, utility_radius_small, c.text,
                    {.on_click = [](const vw::ClickEvent&) {
                      ui_state().recordings_expanded = !ui_state().recordings_expanded;
                    }});
  const auto recordings = recorder::recordings();
  const float list_y = body_y + 32;
  const float list_height = state.recordings_expanded ? 280 : 84;
  if (recordings.empty()) {
    const std::string text = "No recordings found";
    const float tw = draw::measure(text, 12, vw::weight::regular, font::utility) + 24 + utility_medium;
    const float sx = body_x + (body_width - tw) / 2;
    draw::glyph(ui, "scan_delete", sx + 12, list_y + list_height / 2, 24, c.outline);
    draw::text_middle(ui, text, sx + 24 + utility_medium, list_y + list_height / 2,
                      {.size = 12, .color = c.outline, .family = font::utility});
    return;
  }
  const std::size_t limit = state.recordings_expanded ? 10 : 3;
  float row_y = list_y;
  for (std::size_t index = 0; index < recordings.size() && index < limit; ++index) {
    if (row_y + 28 > list_y + list_height + 1 || row_y + 28 > y + h) break;
    const std::string& path = recordings[index];
    draw::text_middle(ui, recorder::display_name(path), body_x, row_y + 14,
                      {.size = 12, .color = c.text_muted, .family = font::utility, .max_width = body_width - 84 - 4});
    const auto action = [&](float ax, const char* glyph, bool destructive, std::function<void()> run) {
      draw::glyph(ui, glyph, ax + 14, row_y + 15, 18, destructive ? c.error : c.text_muted);
      draw::state_layer(ui, ax, row_y, 28, 28, 14, destructive ? c.error : c.text,
                        {.on_click = [run](const vw::ClickEvent&) { run(); }});
    };
    action(body_x + body_width - 84, "play_arrow", false, [path] { recorder::open(path); });
    action(body_x + body_width - 56, "folder", false, [] { recorder::open_directory(); });
    action(body_x + body_width - 28, "delete_forever", true, [path] { ui_state().delete_path = path; });
    row_y += 28;
  }
}

}  // namespace

Sidebar sidebar_panels(const Screen& screen) {
  Shell& state = shell();
  const float r = reveal();
  // An open sidebar takes over from notification popups.
  if (r > 0.35F) notify::close_popups();
  const float s = stack_reveal();
  if (s < 0.35F && ui_state().menu_open && state.stack_reveal == 0) ui_state().menu_open = false;
  Sidebar result;
  if (std::max(r, s) < 0.002F) {
    ui_state().menu_open = false;
    return result;
  }
  const Layout l = layout(screen);
  // Each section squares off the corner it shares only as the other arrives.
  // Each part reaches island_radius past the frame's inner edge on the sides
  // it is anchored to (right; top or bottom). That overhang is constant, so
  // the rounded corners there always stay hidden behind the frame however
  // far the part is revealed.
  const float centre_x = l.x + width * (1 - r);
  const blob::Box centre_box{centre_x, frame_thickness - island_radius, width * r + island_radius,
                             std::max(0.0F, l.stack_y - frame_thickness + island_radius + 4)};
  // Utility stack: straight along x, or out of the bottom-right corner when
  // pulled from there.
  const float stack_rise = state.stack_from_corner ? s : 1;
  const float stack_x = l.x + width * (1 - s);
  const float stack_y = l.stack_y + l.stack_height * (1 - stack_rise);
  const blob::Box stack_box{stack_x, stack_y, width * s + island_radius, l.stack_height * stack_rise + island_radius};

  // Where the two parts meet, a corner is square only while the other part
  // sits flush against it and covers it; it rounds as they come apart, by a
  // vertical gap or by one sliding out from under the other.
  const bool both = r >= 0.002F && s >= 0.002F;
  const float gap = stack_box.y - (centre_box.y + centre_box.height);
  const float apart = clamp01(gap / island_radius);
  // The centre's bottom-left is covered while the stack reaches at least as far left.
  const float centre_corner = both ? island_radius * std::max(apart, clamp01((stack_x - centre_x) / island_radius))
                                   : island_radius;
  const float stack_corner = both ? island_radius * std::max(apart, clamp01((centre_x - stack_x) / island_radius))
                                  : island_radius;
  if (r >= 0.002F) {
    result.centre.reveal = r;
    result.centre.box = centre_box;
    result.centre.box.radii = blob::Radii::all(island_radius);
    result.centre.box.radii.bottom_left = centre_corner;
  }
  if (s >= 0.002F) {
    result.stack.reveal = s;
    result.stack.box = stack_box;
    result.stack.box.radii = blob::Radii::all(island_radius);
    result.stack.box.radii.top_left = stack_corner;
  }

  const float menu_reveal = static_cast<float>(vw::animate(
      "recorder.menu", ui_state().menu_open && !recorder::running() ? 1 : 0,
      {.duration = 0.2, .easing = vw::Easing::out_cubic}));
  if (menu_reveal > 0.01F) {
    // Where draw_recorder puts the card (counted up from the toggles).
    const float recorder_y =
        stack_y + l.stack_height - utility_extra_small - toggles_height() - utility_medium - l.recorder_height;
    const float right = stack_x + width - utility_extra_small - utility_large;
    const float w = menu_width * (0.1F + 0.9F * menu_reveal);
    const float h = menu_height * (0.1F + 0.9F * menu_reveal);
    result.menu.reveal = menu_reveal;
    // Under the split button (top 17, 38 tall) with a clear gap.
    result.menu.box = {right - w, recorder_y + 17 + 38 + utility_medium, w, h,
                       blob::Radii::all(utility_radius_large)};
  }
  return result;
}

void sidebar_tug(vw::Ui& ui, const Screen& screen) {
  // Drag inwards from the top-right corner to pull both sections out, or from
  // just above the bar on the right for the utility stack alone.
  ui.hit_area({.width = tug, .height = tug, .place = {.x = screen.width - tug, .y = 0},
               .input = {.on_drag_start = [](const vw::DragEvent&) {
                           Shell& state = shell();
                           UiState& ui = ui_state();
                           ui.drag_start_reveal = clamp01(ui.shown_reveal);
                           ui.drag_start_stack = clamp01(ui.shown_stack);
                           if (ui.drag_start_stack < 0.01F) state.stack_from_corner = false;
                           state.sidebar_reveal = ui.drag_start_reveal;
                           state.stack_reveal = ui.drag_start_stack;
                           state.sidebar_dragging = state.stack_dragging = true;
                         },
                         .on_drag = [](const vw::DragEvent& drag) {
                           Shell& state = shell();
                           const auto inward = static_cast<float>(std::max(drag.start_x - drag.x, drag.y - drag.start_y) / 120);
                           state.sidebar_reveal = clamp01(ui_state().drag_start_reveal + inward);
                           state.stack_reveal = clamp01(ui_state().drag_start_stack + inward);
                         },
                         .on_drag_end = [](const vw::DragEvent&) {
                           Shell& state = shell();
                           state.sidebar_dragging = state.stack_dragging = false;
                           state.sidebar_reveal = state.sidebar_reveal >= 0.35F ? 1 : 0;
                           state.stack_reveal = state.stack_reveal >= 0.35F ? 1 : 0;
                         }}});
  ui.hit_area({.width = tug, .height = tug, .place = {.x = screen.width - tug, .y = screen.height - bar_height - tug},
               .input = {.on_drag_start = [](const vw::DragEvent&) {
                           Shell& state = shell();
                           if (state.stack_reveal > 0) return;
                           ui_state().drag_start_stack = clamp01(ui_state().shown_stack);
                           state.stack_from_corner = true;
                           state.stack_dragging = true;
                         },
                         .on_drag = [](const vw::DragEvent& drag) {
                           if (!shell().stack_dragging) return;
                           const double inward = std::max(drag.start_x - drag.x, drag.start_y - drag.y);
                           shell().stack_reveal = clamp01(ui_state().drag_start_stack + static_cast<float>(inward / 120));
                         },
                         .on_drag_end = [](const vw::DragEvent&) {
                           Shell& state = shell();
                           if (!state.stack_dragging) return;
                           state.stack_dragging = false;
                           state.stack_reveal = state.stack_reveal >= 0.35F ? 1 : 0;
                         }}});
}

bool apply_edge_swipes(const std::vector<vw::zwwm::EdgeSwipe>& swipes) {
  using Phase = vw::zwwm::EdgeSwipe::Phase;
  Shell& state = shell();
  UiState& ui = ui_state();
  bool changed = false;
  for (const auto& swipe : swipes) {
    if (state.locked) break;
    if (swipe.edge == vw::zwwm::right) {
      if (swipe.phase == Phase::began) {
        ui.drag_start_reveal = clamp01(ui.shown_reveal);
        ui.drag_start_stack = clamp01(ui.shown_stack);
        if (ui.drag_start_stack < 0.01F) state.stack_from_corner = false;
        state.sidebar_dragging = state.stack_dragging = true;
      }
      if (!state.sidebar_dragging) continue;
      // The panel tracks the finger: a sidebar's width of travel opens it.
      const float travel = static_cast<float>(std::clamp(config().sidebar_width, 300, 560));
      const float inward = std::max(0.0F, swipe.distance) / travel;
      state.sidebar_reveal = clamp01(ui.drag_start_reveal + inward);
      state.stack_reveal = clamp01(ui.drag_start_stack + inward);
      if (swipe.phase == Phase::ended) {
        state.sidebar_dragging = state.stack_dragging = false;
        // A flick opens it however short; a cancelled touch puts it back.
        const bool open = swipe.cancelled ? ui.drag_start_reveal >= 0.5F
                                          : state.sidebar_reveal >= 0.35F || swipe.velocity > 600;
        state.sidebar_reveal = state.stack_reveal = open ? 1 : 0;
      }
      changed = true;
    } else if (swipe.edge == vw::zwwm::bottom && swipe.phase == Phase::ended && !swipe.cancelled &&
               !state.launcher_open && (swipe.distance > 80 || swipe.velocity > 500)) {
      state.launcher_open = true;
      changed = true;
    }
  }
  return changed;
}

void draw_sidebar(vw::Ui& ui, const Screen& screen, const Sidebar& sidebar) {
  if (!sidebar.centre.visible() && !sidebar.stack.visible()) return;
  const Colours& c = colours();
  const Layout l = layout(screen);
  const float r = sidebar.centre.reveal;
  const float slide = width * (1 - r);
  const float x = l.x + slide;

  // Dragging a section back out of the way dismisses that section alone.
  if (sidebar.centre.visible())
    ui.hit_area({.width = std::max(1.0F, screen.width - x), .height = std::max(1.0F, l.stack_y - frame_thickness),
                 .place = {.x = x, .y = frame_thickness},
                 .input = {.on_drag_start = [](const vw::DragEvent&) {
                             ui_state().drag_start_reveal = clamp01(ui_state().shown_reveal);
                             shell().sidebar_reveal = ui_state().drag_start_reveal;
                             shell().sidebar_dragging = true;
                           },
                           .on_drag = [](const vw::DragEvent& drag) {
                             // 1:1, so the panel stays under the pointer.
                             shell().sidebar_reveal = clamp01(ui_state().drag_start_reveal -
                                                              static_cast<float>(drag.x - drag.start_x) / width);
                           },
                           .on_drag_end = [](const vw::DragEvent&) {
                             Shell& state = shell();
                             state.sidebar_dragging = false;
                             state.sidebar_reveal = state.sidebar_reveal <= 0.65F ? 0 : 1;
                           }}});
  const bool stack_out = sidebar.stack.visible();
  const float stack_x = sidebar.stack.box.x;
  const float stack_y = sidebar.stack.box.y;
  if (stack_out)
    ui.hit_area({.width = std::max(1.0F, screen.width - stack_x), .height = std::max(1.0F, screen.height - stack_y),
                 .place = {.x = stack_x, .y = stack_y},
                 .input = {.on_drag_start = [](const vw::DragEvent&) {
                             ui_state().drag_start_stack = clamp01(ui_state().shown_stack);
                             shell().stack_reveal = ui_state().drag_start_stack;
                             shell().stack_dragging = true;
                           },
                           .on_drag = [](const vw::DragEvent& drag) {
                             Shell& state = shell();
                             const double along = drag.x - drag.start_x;
                             const double outward = state.stack_from_corner ? std::max(along, drag.y - drag.start_y) : along;
                             state.stack_reveal = clamp01(ui_state().drag_start_stack - static_cast<float>(outward) / width);
                           },
                           .on_drag_end = [](const vw::DragEvent&) {
                             Shell& state = shell();
                             state.stack_dragging = false;
                             state.stack_reveal = state.stack_reveal <= 0.65F ? 0 : 1;
                           }}});
  if (stack_out && ui_state().menu_open)
    ui.hit_area({.width = width, .height = l.stack_height, .place = {.x = stack_x, .y = stack_y},
                 .input = {.on_click = [](const vw::ClickEvent&) { ui_state().menu_open = false; }}});

  if (sidebar.centre.visible() && x < screen.width - 8) {
    Panel centre = sidebar.centre;
    centre.box.x = x;
    centre.box.y = frame_thickness;  // contents start below the frame
    centre.box.height -= island_radius;
    centre.box.width = width;
    draw_centre(ui, centre, slide);
    // The divider under the centre, only along the stretch where the stack
    // sits flush beneath it (not once the stack slides away or drops).
    const float divider_x = std::max(x, stack_x) + utility_large;
    const float divider_end = x + width - utility_extra_small;
    if (stack_out && std::abs(stack_y - l.stack_y) < 0.5F && divider_end - divider_x > 1)
      draw::rect(ui, divider_x, l.stack_y - 1, divider_end - divider_x, 1, 0, c.outline_variant);
  }
  if (!stack_out || stack_x >= screen.width - 8 || stack_y >= screen.height - 8) return;

  const float content_x = stack_x + utility_large;
  const float content_width = width - utility_large - utility_extra_small;
  const float toggles_y = stack_y + l.stack_height - utility_extra_small - toggles_height();
  const float recorder_y = toggles_y - utility_medium - l.recorder_height;
  const float keep_y = recorder_y - utility_medium - l.keep_height;
  draw_media(ui, content_x, keep_y - l.media_block, content_width, l.media_block - utility_medium);
  draw_keep_awake(ui, content_x, keep_y, content_width, l.keep_height);
  draw_recorder(ui, content_x, recorder_y, content_width, l.recorder_height);
  draw_toggles(ui, content_x, toggles_y, content_width);

  // The recorder's mode menu.
  if (sidebar.menu.visible() && sidebar.menu.reveal > 0.9F) {
    const blob::Box& box = sidebar.menu.box;
    draw::rect(ui, box.x, box.y, box.width, box.height, utility_radius_large, c.surface_container_low);
    for (int index = 0; index < 4; ++index) {
      const RecorderMode& mode = modes[index];
      const bool active = index == ui_state().mode;
      const float oy = box.y + utility_extra_small + static_cast<float>(index) * 48;
      const float ow = box.width - utility_extra_small * 2;
      const float radius = active || index == 0 || index == 3 ? utility_radius_medium : utility_radius_extra_small;
      if (active) draw::rect(ui, box.x + utility_extra_small, oy, ow, 48, radius, c.tertiary_container);
      draw::glyph(ui, mode.glyph, box.x + utility_extra_small + utility_medium + 9, oy + 24, 18,
                  active ? c.text : c.text_muted);
      draw::text_middle(ui, mode.title, box.x + utility_extra_small + utility_medium + 26, oy + 24,
                        {.size = 12, .color = c.text, .family = font::utility, .max_width = ow - 50});
      draw::state_layer(ui, box.x + utility_extra_small, oy, ow, 48, radius, active ? c.text : c.text_muted,
                        {.on_click = [index](const vw::ClickEvent&) {
                          ui_state().mode = index;
                          ui_state().menu_open = false;
                          recorder::start(modes[index].mode, modes[index].audio, "screen");
                        }});
    }
  }
}

}  // namespace blx
