// The lock screen: one more surface of the shell, drawn only while locked. A
// glanceable Android-style page; any key or click raises the password sheet.
// vw handles ext_session_lock_v1, one surface per output and the PAM check.
#include <vw/vw.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <optional>

#include <unistd.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "net.hpp"
#include "blob.hpp"
#include "config.hpp"
#include "draw.hpp"
#include "lockpage.hpp"
#include "notify.hpp"
#include "plugins.hpp"
#include "parts.hpp"
#include "services.hpp"
#include "shapes.hpp"
#include "state.hpp"
#include "style.hpp"
#include "surfaces.hpp"
#include "system.hpp"

namespace blx {
namespace {

using Clock = std::chrono::steady_clock;

constexpr const char* password_id = "lock-password";

struct LockState {
  // Bumped per lock so the entrance tween starts over each time.
  int entrances = 0;
  bool sheet = false;
  Clock::time_point opened_at{};
  Clock::time_point last_input{};
  bool was_failed = false;
  Clock::time_point failed_at{};
  bool typed = false;  // whether anything is, for the arrow button
};

LockState& lock_state() {
  static LockState state;
  return state;
}

void close_sheet() {
  lock_state().sheet = false;
  lock_state().typed = false;
  vw::reset_text_field(password_id);
}

void open_sheet() {
  if (!lock_state().sheet) lock_state().opened_at = Clock::now();
  lock_state().sheet = true;
  lock_state().last_input = Clock::now();
}

float blend(float from, float to, float t) { return from + (to - from) * t; }

constexpr float pill_height = 44;

struct Size {
  float w = 0;
  float h = 0;
};

// The backdrop opacity the item being drawn asks for; below 0 each part's
// own (the classic layout never sets it).
float& background_override() {
  static float value = -1;
  return value;
}

// The colour the item being drawn asks for; empty: the screen's setting.
std::string& backdrop_name_override() {
  static std::string value;
  return value;
}

// A backdrop's colour: the item's, else the lock screen's setting, else
// `fallback` (the theme's).
vw::Color backdrop_of(vw::Color fallback) {
  const std::string& name = backdrop_name_override().empty() ? config().lock_backdrop_colour : backdrop_name_override();
  return backdrop_colour(name, fallback);
}

// A backdrop's alpha: the item's background setting, else `base`; times the
// item's opacity.
float back(float base, float alpha) {
  return (background_override() >= 0 ? std::clamp(background_override(), 0.0F, 1.0F) : base) * alpha;
}

// A rectangle in the lock material (a plugin's shader, which sees the
// wallpaper behind it) when one is installed: its colour's alpha is the
// backdrop setting, `opacity` fades the whole of it. False without one.
bool material_rect(vw::Ui& ui, float x, float y, float width, float height, float radius, vw::Color colour, float opacity) {
  const auto material = plugins::lock_material();
  if (!material) return false;
  ui.rectangle({.width = width, .height = height, .radius = radius, .color = colour, .shader = material->shader,
                .shader_params = {material->params[0], material->params[1], material->params[2], opacity},
                .place = {.x = x, .y = y}});
  return true;
}

// An item's backdrop: in the lock material, else the theme's colour at the
// item's Background setting (`base` while automatic).
void pane(vw::Ui& ui, float x, float y, float width, float height, float radius, float base, float alpha) {
  const vw::Color colour = backdrop_of(colours().surface_container_high);
  if (!material_rect(ui, x, y, width, height, radius, with_alpha(colour, back(base, 1)), alpha))
    draw::rect(ui, x, y, width, height, radius, with_alpha(colour, back(base, alpha)));
}

draw::TextStyle pill_text(float k = 1, float alpha = 1) {
  return {.size = 15 * k, .color = with_alpha(colours().text, alpha), .weight = vw::weight::medium, .family = font::utility};
}

// --- Status icons --------------------------------------------------------------

struct StatusItem {
  std::string glyph;
  std::string label;
};

std::vector<StatusItem> status_items() {
  std::vector<StatusItem> items;
  const auto network = net::status();
  const bool wired = network.primary == vw::network::Kind::wired;
  items.push_back({wired ? "lan" : network.wifi_enabled ? "wifi" : "wifi_off", ""});
  if (const auto adapter = vw::bluetooth::adapter(); adapter && adapter->powered) items.push_back({"bluetooth", ""});
  if (const auto battery = vw::upower::display_device()) {
    const char* glyph = battery->percentage < 20 ? "battery_alert" : battery->percentage < 60 ? "battery_4_bar" : "battery_full";
    items.push_back({glyph, std::format("{}%", std::lround(battery->percentage))});
  }
  return items;
}

Size status_size(const std::vector<StatusItem>& items, float k) {
  float width = (32 + 14 * static_cast<float>(items.size() - 1)) * k;
  for (const StatusItem& item : items)
    width += 20 * k + (item.label.empty() ? 0 : 6 * k + draw::measure(item.label, 14 * k, vw::weight::medium, font::utility));
  return {width, 40 * k};
}

void draw_status_at(vw::Ui& ui, float x, float y, float k, float fade) {
  if (fade <= 0.01F) return;
  const Colours& c = colours();
  const auto items = status_items();
  const Size size = status_size(items, k);
  pane(ui, x, y, size.w, size.h, size.h / 2, 0.72F, fade);
  x += 16 * k;
  for (const StatusItem& item : items) {
    draw::glyph(ui, item.glyph, x + 10 * k, y + size.h / 2, 20 * k, with_alpha(c.text, fade));
    x += 20 * k;
    if (!item.label.empty()) {
      x += 6 * k;
      draw::text_middle(ui, item.label, x, y + size.h / 2,
                        {.size = 14 * k, .color = with_alpha(c.text, fade), .weight = vw::weight::medium, .family = font::utility});
      x += draw::measure(item.label, 14 * k, vw::weight::medium, font::utility);
    }
    x += 14 * k;
  }
}

// The status icons, top right, in one pill.
void draw_status(vw::Ui& ui, float right, float fade) {
  draw_status_at(ui, right - status_size(status_items(), 1).w, 22, 1, fade);
}

// --- Date and weather ------------------------------------------------------------

Size date_size(const std::string& date, const std::optional<weather::Report>& now, float k) {
  float width = 40 * k + draw::measure(date, 15 * k, vw::weight::medium, font::utility);
  if (now) width += 44 * k + draw::measure(now->temperature, 15 * k, vw::weight::medium, font::utility);
  return {width, pill_height * k};
}

void draw_date_at(vw::Ui& ui, float x, float y, const std::string& date, const std::optional<weather::Report>& now,
                  float k, float alpha) {
  const Colours& c = colours();
  const Size size = date_size(date, now, k);
  pane(ui, x, y, size.w, size.h, size.h / 2, 0.78F, alpha);
  x += 20 * k;
  draw::text_middle(ui, date, x, y + size.h / 2, pill_text(k, alpha));
  x += draw::measure(date, 15 * k, vw::weight::medium, font::utility);
  if (now) {
    x += 20 * k;
    draw::glyph(ui, now->glyph, x + 10 * k, y + size.h / 2, 20 * k, with_alpha(c.primary, alpha));
    x += 24 * k;
    draw::text_middle(ui, now->temperature, x, y + size.h / 2, pill_text(k, alpha));
  }
}

// Date and weather in one pill.
void draw_date_pill(vw::Ui& ui, float centre, float y, const std::string& date, const std::optional<weather::Report>& now) {
  draw_date_at(ui, centre - date_size(date, now, 1).w / 2, y, date, now, 1, 1);
}

// Weather on its own: the glyph and the temperature.
Size weather_size(const weather::Report& now, float k) {
  return {60 * k + draw::measure(now.temperature, 15 * k, vw::weight::medium, font::utility), pill_height * k};
}

void draw_weather_at(vw::Ui& ui, float x, float y, const weather::Report& now, float k, float alpha) {
  const Colours& c = colours();
  const Size size = weather_size(now, k);
  pane(ui, x, y, size.w, size.h, size.h / 2, 0.78F, alpha);
  draw::glyph(ui, now.glyph, x + 30 * k, y + size.h / 2, 22 * k, with_alpha(c.primary, alpha));
  draw::text_middle(ui, now.temperature, x + 46 * k, y + size.h / 2, pill_text(k, alpha));
}

// --- Notifications ------------------------------------------------------------------

constexpr std::size_t notifications_shown = 7;

// How many discs: the icons shown, and a "+n" one past them. `sample`
// stands in for an empty history in the editor.
std::size_t notification_discs(bool sample) {
  const std::size_t count = notify::history().size();
  if (count == 0) return sample ? 3 : 0;
  return std::min(count, notifications_shown) + (count > notifications_shown ? 1 : 0);
}

Size notifications_size(float k, bool sample) {
  const auto count = static_cast<float>(notification_discs(sample));
  if (count == 0) return {};
  return {(count * 40 + (count - 1) * 8) * k, 40 * k};
}

// Notifications as icons only: what arrived, never what it says.
void draw_notifications_at(vw::Ui& ui, float x, float y, float k, float alpha, bool sample) {
  const Colours& c = colours();
  const auto groups = notify::history();
  const float disc = 40 * k;
  if (groups.empty()) {
    if (!sample) return;
    for (int index = 0; index < 3; ++index) {
      pane(ui, x, y, disc, disc, disc / 2, 0.85F, alpha);
      draw::glyph(ui, "notifications", x + disc / 2, y + disc / 2, 20 * k, with_alpha(c.primary, alpha));
      x += disc + 8 * k;
    }
    return;
  }
  const std::size_t shown = std::min(groups.size(), notifications_shown);
  for (std::size_t index = 0; index < shown; ++index) {
    pane(ui, x, y, disc, disc, disc / 2, 0.85F, alpha);
    if (!draw::icon(ui, groups[index].entries.front().icon, "", x + 9 * k, y + 9 * k, 22 * k))
      draw::glyph(ui, "notifications", x + disc / 2, y + disc / 2, 20 * k, with_alpha(c.primary, alpha));
    x += disc + 8 * k;
  }
  if (groups.size() > shown) {
    draw::rect(ui, x, y, disc, disc, disc / 2, with_alpha(c.primary, alpha));
    draw::text_centered(ui, std::format("+{}", groups.size() - shown), x + disc / 2, y + disc / 2,
                        {.size = 13 * k, .color = with_alpha(c.primary_text, alpha), .weight = vw::weight::bold, .family = font::utility});
  }
}

float draw_notifications(vw::Ui& ui, float centre, float y) {
  const Size size = notifications_size(1, false);
  if (size.w <= 0) return y;
  draw_notifications_at(ui, centre - size.w / 2, y, 1, 1, false);
  return y + size.h;
}

// --- Media ----------------------------------------------------------------------------

constexpr float media_width = 480;
constexpr float media_height = 104;

bool has_media() {
  const auto player = vw::mpris::active();
  return player && !player->title.empty();
}

// Now playing: art, title and artist, skip, and a play button that morphs.
// `sample` draws an empty card when nothing plays (the editor).
void draw_media_at(vw::Ui& ui, float x, float y, float k, float alpha, bool sample) {
  const auto player = vw::mpris::active();
  const bool live = player && !player->title.empty();
  if (!live && !sample) return;
  const Colours& c = colours();
  const float width = media_width * k;
  const float height = media_height * k;
  const float padding = 14 * k;
  pane(ui, x, y, width, height, 36 * k, 0.9F, alpha);
  const float art_size = height - padding * 2;
  // The placeholder only without art: under it, its soft corners would show
  // through the art's anti-aliased ones as a light rim.
  if (!live || !draw::art(ui, art::path(player->art_url), x + padding, y + padding, art_size, 24 * k)) {
    draw::rect(ui, x + padding, y + padding, art_size, art_size, 24 * k, with_alpha(c.primary_container, alpha));
    draw::glyph(ui, "music_note", x + padding + art_size / 2, y + height / 2, 32 * k, with_alpha(c.primary, alpha));
  }
  const bool playing = live && player->playback_status == "Playing";
  const float play_size = 56 * k;
  const float play_x = x + width - padding - 4 * k - play_size;
  const float play_y = y + (height - play_size) / 2;
  const float skip = 40 * k;
  const float skip_x = play_x - 10 * k - skip;
  const float skip_y = y + (height - skip) / 2;
  const float text_x = x + padding + art_size + 16 * k;
  const float text_width = skip_x - 12 * k - text_x;
  draw::text(ui, live ? player->title : "Nothing playing", text_x, y + 30 * k,
             {.size = 16 * k, .color = with_alpha(c.text, alpha), .weight = vw::weight::semibold, .family = font::display,
              .max_width = text_width});
  draw::text(ui, !live ? "Media player" : player->artist.empty() ? player->name : player->artist, text_x, y + 56 * k,
             {.size = 13 * k, .color = with_alpha(c.text_muted, alpha), .family = font::display, .max_width = text_width});
  const bool can_skip = live && player->can_go_next;
  draw::glyph(ui, "skip_next", skip_x + skip / 2, y + height / 2, 22 * k, with_alpha(can_skip ? c.text : c.outline_variant, alpha));
  if (can_skip && !sample)
    draw::state_layer(ui, skip_x, skip_y, skip, skip, skip / 2, c.text,
                      {.on_click = [](const vw::ClickEvent&) { vw::mpris::next(); }});
  // Playing: a squircle; paused: a circle, as Android's media controls do.
  const float radius = std::max(0.0F, static_cast<float>(vw::animate("lock.play", playing ? 18 : 28,
                                                                     {.duration = 0.35, .easing = vw::Easing::out_back}))) * k;
  draw::rect(ui, play_x, play_y, play_size, play_size, radius, with_alpha(c.primary, alpha));
  draw::glyph(ui, playing ? "pause" : "play_arrow", play_x + play_size / 2, play_y + play_size / 2, 28 * k,
              with_alpha(c.primary_text, alpha));
  if (live && !sample)
    draw::state_layer(ui, play_x, play_y, play_size, play_size, radius, c.primary_text,
                      {.on_click = [](const vw::ClickEvent&) { vw::mpris::play_pause(); }});
}

void draw_media(vw::Ui& ui, float centre, float y) { draw_media_at(ui, centre - media_width / 2, y, 1, 1, false); }

// --- The clock, the hint, and the rest of the layout's items ---------------------

struct ClockText {
  std::string time;
  std::size_t colon = std::string::npos;
  bool stacked = false;
  float size = 0;
  float line = 0;
};

ClockText clock_text(const LockLook& look, float k) {
  ClockText clock;
  clock.time = system::format_time(look.clock_format);
  clock.colon = clock.time.find(':');
  clock.stacked = look.clock_style == "stacked" && clock.colon != std::string::npos;
  clock.size = static_cast<float>(std::clamp(look.clock_size, 48, 260)) * (clock.stacked ? 1.6F : 1.0F) * k;
  clock.line = clock.size * 0.86F;
  return clock;
}

Size clock_size(const ClockText& clock) {
  const auto width = [&](const std::string& text) { return draw::measure(text, clock.size, vw::weight::black, font::display); };
  if (clock.stacked)
    return {std::max(width(clock.time.substr(0, clock.colon)), width(clock.time.substr(clock.colon + 1))), clock.line * 2};
  return {width(clock.time), clock.line};
}

void draw_clock_at(vw::Ui& ui, const ClockText& clock, float centre, float y, float alpha) {
  const Colours& c = colours();
  const draw::TextStyle top{.size = clock.size, .color = with_alpha(c.primary, alpha), .weight = vw::weight::black,
                            .family = font::display};
  if (clock.stacked) {
    draw::TextStyle bottom = top;
    bottom.color = with_alpha(mix(c.primary, c.tertiary, 0.7F), alpha);
    draw::text_centered(ui, clock.time.substr(0, clock.colon), centre, y + clock.line / 2, top);
    draw::text_centered(ui, clock.time.substr(clock.colon + 1), centre, y + clock.line * 1.5F, bottom);
  } else {
    draw::text_centered(ui, clock.time, centre, y + clock.line / 2, top);
  }
}

Size hint_size(const std::string& hint, float k) {
  return {std::max(52 * k, draw::measure(hint, 14 * k, vw::weight::medium, font::utility)), 52 * k + 34 * k};
}

// The way in: a cookie with a glyph, and the words under it.
void draw_hint_at(vw::Ui& ui, float centre, float y, const std::string& glyph, const std::string& hint, float k, float alpha) {
  const Colours& c = colours();
  const float disc = 52 * k;
  shapes::draw(ui, "lock.hint", "cookie9sided", centre - disc / 2, y, disc, with_alpha(c.primary_container, back(0.95F, alpha)));
  draw::glyph(ui, glyph, centre, y + disc / 2, 24 * k, with_alpha(c.primary, alpha));
  draw::text_centered(ui, hint, centre, y + disc + 24 * k,
                      {.size = 14 * k, .color = with_alpha(c.text_muted, alpha), .weight = vw::weight::medium, .family = font::utility});
}

vw::Color role_colour(const std::string& role) {
  const Colours& c = colours();
  if (role == "primary") return c.primary;
  if (role == "secondary") return c.secondary;
  if (role == "tertiary") return c.tertiary;
  if (role == "muted") return c.text_muted;
  return c.text;
}

draw::TextStyle text_item_style(const LockItem& item, float k, float alpha) {
  return {.size = 28 * k, .color = with_alpha(role_colour(item.colour), alpha), .weight = vw::weight::semibold,
          .family = font::display};
}

// Power off and restart, for the login screen.
struct PowerChip {
  const char* glyph;
  const char* label;
  const char* command;
};
constexpr PowerChip power_chips[] = {{"power_settings_new", "Power off", "poweroff"}, {"restart_alt", "Restart", "reboot"}};

float power_chip_width(const PowerChip& chip, float k) {
  return draw::measure(chip.label, 15 * k, vw::weight::medium, font::utility) + 58 * k;
}

Size power_size(float k) {
  return {power_chip_width(power_chips[0], k) + 8 * k + power_chip_width(power_chips[1], k), 40 * k};
}

void draw_power_at(vw::Ui& ui, float x, float y, float k, float alpha, bool live) {
  const Colours& c = colours();
  const float height = 40 * k;
  for (const PowerChip& chip : power_chips) {
    const float width = power_chip_width(chip, k);
    pane(ui, x, y, width, height, height / 2, 0.9F, alpha);
    draw::glyph(ui, chip.glyph, x + 26 * k, y + height / 2, 20 * k, with_alpha(c.primary, alpha));
    draw::text_middle(ui, chip.label, x + 42 * k, y + height / 2, pill_text(k, alpha));
    if (live)
      draw::state_layer(ui, x, y, width, height, height / 2, c.text,
                        {.on_click = [command = chip.command](const vw::ClickEvent&) { vw::spawn({"systemctl", command}); }});
    x += width + 8 * k;
  }
}

std::string plugin_of(const std::string& id) { return id.starts_with("plugin:") ? id.substr(7) : std::string(); }

nlohmann::json plugin_state(float k, const LockPage& page) {
  nlohmann::json state{{"lock", {{"scale", k}, {"editing", page.editing}, {"greeter", page.greeter}, {"sheet", page.reveal},
                                 {"background", background_override()}}}};
  // The backdrop colour, when not the theme's: plugin cards follow it.
  if (const std::string& name = backdrop_name_override().empty() ? config().lock_backdrop_colour : backdrop_name_override();
      !name.empty() && name != "theme") {
    const vw::Color colour = backdrop_colour(name, colours().surface_container_high);
    state["lock"]["backdropColour"] = {colour[0], colour[1], colour[2]};
  }
  if (const auto material = plugins::lock_material())
    state["lock"]["material"] = {{"shader", material->shader},
                                 {"params", {material->params[0], material->params[1], material->params[2]}}};
  return state;
}

Size plugin_size(const std::string& view, const nlohmann::json& extra) {
  vw::Scene scene;
  vw::Ui scratch(scene);
  const auto measured = plugins::draw_view(scratch, view, extra);
  if (!measured) return {};
  return {measured->width, measured->height};
}

void draw_plugin_at(vw::Ui& ui, const std::string& view, const nlohmann::json& extra, float x, float y) {
  const vw::Widget first = ui.hit_area({.width = 1, .height = 1});
  const auto drawn = plugins::draw_view(ui, view, extra);
  const vw::Widget last = ui.hit_area({.width = 1, .height = 1});
  if (!drawn) return;
  ui.row({vw::Widget{first.begin, last.end, drawn->width, drawn->height}}, {.place = {.x = x, .y = y}});
}

// A custom layout: every visible item where the user put it. With the
// sheet up they shrink into the room above it, and what only matters at a
// glance (status, hint, media, notifications, power) fades away.
void draw_layout(vw::Ui& ui, const LockPage& page, const LockLook& look, const std::string& date,
                 const std::optional<weather::Report>& weather) {
  const Colours& c = colours();
  const float width = page.width;
  const float height = page.height;
  const float reveal = page.reveal;
  const float room = page.sheet_room > 0 ? page.sheet_room : lock_sheet_height;
  const std::vector<LockItem>& items = *page.layout;
  for (std::size_t index = 0; index < items.size(); ++index) {
    const LockItem& item = items[index];
    if (!item.visible || !lock_item_allowed(item.id, page.greeter)) continue;
    const std::string& id = item.id;
    const bool session = id == "notifications" || id == "media";
    if (session && !page.session_widgets && !page.editing) continue;
    const bool glance = session || id == "status" || id == "hint" || id == "power";
    if ((session && reveal >= 0.2F) || (id == "hint" && reveal >= 0.6F) || (id == "power" && reveal > 0.99F)) continue;
    float alpha = std::clamp(item.opacity, 0.05F, 1.0F) * (glance ? 1 - reveal : 1.0F);
    background_override() = item.background;
    backdrop_name_override() = item.backdrop;
    const float k = std::clamp(item.scale, 0.2F, 5.0F) * blend(1, glance ? 1.0F : id == "clock" ? 0.55F : 0.75F, reveal);

    // What it is, and how big at this scale.
    std::optional<ClockText> clock;
    std::string text;
    nlohmann::json extra;
    const std::string view = plugin_of(id);
    Size size;
    bool placeholder = false;
    if (id == "clock") size = clock_size(*(clock = clock_text(look, k)));
    else if (id == "date") size = date_size(date, weather, k);
    else if (id == "weather") size = weather ? weather_size(*weather, k) : Size{};
    else if (id == "status") size = status_size(status_items(), k);
    else if (id == "hint") size = page.hint.empty() ? Size{} : hint_size(page.hint, k);
    else if (id == "notifications") size = notifications_size(k, page.editing);
    else if (id == "media") size = (has_media() || page.editing) ? Size{media_width * k, media_height * k} : Size{};
    else if (id == "user") size = {std::max(96 * k, draw::measure(page.user_name, 22 * k, vw::weight::semibold, font::display)), 140 * k};
    else if (id == "power") size = power_size(k);
    else if (id.starts_with("text:")) {
      text = system::format_time(item.text.empty() ? std::string(" ") : item.text);
      const auto style = text_item_style(item, k, alpha);
      size = {draw::measure(text, style.size, style.weight, style.family), style.size * 1.3F};
    } else if (!view.empty()) {
      extra = plugin_state(k, page);
      size = plugin_size(view, extra);
    }
    if (size.w < 1 || size.h < 1) {
      if (!page.editing) continue;
      placeholder = true;
      size = {200 * k, 56 * k};
    }

    // The anchor point moves into the room above the sheet as it rises.
    const float anchor_x = item.x * width;
    const float anchor_y = blend(item.y * height, item.y * (height - room), reveal) - (1 - page.intro) * 96;
    const float x = std::clamp(anchor_x - item.ax * size.w, 0.0F, std::max(0.0F, width - size.w));
    const float y = std::clamp(anchor_y - item.ay * size.h, 0.0F, std::max(0.0F, height - size.h));

    if (placeholder) {
      draw::rect(ui, x, y, size.w, size.h, 18 * k, with_alpha(c.surface_container_high, 0.7F));
      const char* what = id == "weather" ? "Weather (none yet)" : id == "hint" ? "Hint (no text)" :
                         !view.empty() ? "Plugin not running" : "Nothing to show";
      draw::glyph(ui, view.empty() ? "visibility" : "extension", x + 28 * k, y + size.h / 2, 20 * k, c.text_muted);
      draw::text_middle(ui, what, x + 48 * k, y + size.h / 2,
                        {.size = 13 * k, .color = c.text_muted, .weight = vw::weight::medium, .family = font::utility});
    } else if (clock) {
      draw_clock_at(ui, *clock, x + size.w / 2, y, alpha);
    } else if (id == "date") {
      draw_date_at(ui, x, y, date, weather, k, alpha);
    } else if (id == "weather") {
      draw_weather_at(ui, x, y, *weather, k, alpha);
    } else if (id == "status") {
      draw_status_at(ui, x, y, k, alpha);
    } else if (id == "hint") {
      draw_hint_at(ui, x + size.w / 2, y, page.hint_glyph, page.hint, k, alpha);
    } else if (id == "notifications") {
      draw_notifications_at(ui, x, y, k, alpha, page.editing);
    } else if (id == "media") {
      draw_media_at(ui, x, y, k, alpha, page.editing);
    } else if (id == "user") {
      draw_avatar(ui, std::format("lock.user.{}", index), page.user_picture, x + size.w / 2, y, 96 * k, page.avatar_shape);
      draw::text_centered(ui, page.user_name, x + size.w / 2, y + 96 * k + 26 * k,
                          {.size = 22 * k, .color = with_alpha(c.text, alpha), .weight = vw::weight::semibold, .family = font::display});
    } else if (id == "power") {
      draw_power_at(ui, x, y, k, alpha, !page.editing);
    } else if (!text.empty()) {
      // A backdrop only when asked for one.
      if (item.background > 0)
        pane(ui, x - 16 * k, y - 4 * k, size.w + 32 * k, size.h + 8 * k, (size.h + 8 * k) / 2, 0, alpha);
      draw::text_middle(ui, text, x, y + size.h / 2, text_item_style(item, k, alpha));
    } else if (!view.empty()) {
      draw_plugin_at(ui, view, extra, x, y);
      // Shown, never clicked: a click on it raises the sheet like the page.
      if (!page.editing)
        ui.rectangle({.width = size.w, .height = size.h, .color = with_alpha(c.surface, 0),
                      .place = {.x = x, .y = y}, .input = {.on_click = [click = page.on_click](const vw::ClickEvent&) {
                        if (click) click();
                      }}});
    }
    if (page.placed != nullptr) page.placed->push_back({index, x, y, size.w, size.h});
  }
  background_override() = -1;
  backdrop_name_override().clear();
}

// The password sheet in its floating card; `reveal` 0 is below the edge.
// `live` binds the field to the session lock's PAM check; a preview has none.
void draw_sheet(vw::Ui& ui, float width, float height, float reveal, bool failed, float shake, bool live = true) {
  const Colours& c = colours();
  const Config& settings = config();
  const float card_height = settings.lock_show_user ? 318 : 168;
  const SheetCard card = draw_sheet_card(ui, width, height, card_height, reveal);
  const float centre = card.centre;

  float cursor = card.y + 40;
  if (settings.lock_show_user) {
    constexpr float badge = 96;
    draw_avatar(ui, "lock.avatar", system::avatar(), centre, cursor, badge);
    cursor += badge + 22;
    const char* user = std::getenv("USER");
    draw::text_centered(ui, user != nullptr ? user : "", centre, cursor + 12,
                        {.size = 22, .color = c.text, .weight = vw::weight::semibold, .family = font::display});
    cursor += 36;
  }

  constexpr float field_height = 60;
  const float field_width = std::min(440.0F, card.width - 56);
  const float field_x = centre - field_width / 2 + shake;
  // In the lock material a wrong password tints the field instead of
  // ringing it.
  if (!material_rect(ui, field_x, cursor, field_width, field_height, field_height / 2,
                     failed ? with_alpha(c.error, 0.8F) : with_alpha(c.surface_container_highest, 0.7F), 1))
    ui.rectangle({.width = field_width, .height = field_height, .radius = field_height / 2, .color = c.surface_container_highest,
                  .border_width = failed ? 2.0F : 0.0F, .border_color = c.error, .place = {.x = field_x, .y = cursor}});
  draw::glyph(ui, "lock", field_x + 30, cursor + field_height / 2, 22, failed ? c.error : c.primary);
  ui.text_field({.id = password_id, .placeholder = "Password", .width = field_width - 124, .height = field_height, .radius = 0,
                 .border_width = 0, .padding = 4, .color = with_alpha(c.surface_container_highest, 0),
                 .focus_color = with_alpha(c.primary, 0), .text_color = c.text, .placeholder_color = c.text_muted,
                 .caret_color = c.primary, .font = font::utility, .size = 17, .password = true,
                 .authenticate = live, .autofocus = live,
                 .on_change = [](const std::string& text) { lock_state().typed = !text.empty(); },
                 .place = {.x = field_x + 56, .y = cursor}});
  // Enter submits; the arrow lights up, and squares off, once there is
  // something to submit.
  constexpr float go = 44;
  const bool ready = lock_state().typed;
  const float go_x = field_x + field_width - go - 8;
  const float go_radius = std::max(0.0F, static_cast<float>(vw::animate("lock.go", ready ? 14 : go / 2,
                                                                        {.duration = 0.3, .easing = vw::Easing::out_back})));
  draw::rect(ui, go_x, cursor + (field_height - go) / 2, go, go, go_radius, ready ? c.primary : c.surface_container_high);
  draw::glyph(ui, "arrow_forward", go_x + go / 2, cursor + field_height / 2, 22, ready ? c.primary_text : c.text_muted);
  cursor += field_height + 28;
  draw::text_centered(ui, failed ? "Wrong password, try again" : "Press Enter to unlock", centre, cursor,
                      {.size = 13, .color = failed ? c.error : c.text_muted, .weight = vw::weight::medium, .family = font::utility});
}

}  // namespace

SheetCard draw_sheet_card(vw::Ui& ui, float width, float height, float card_height, float reveal) {
  const Colours& c = colours();
  constexpr float margin = 28;
  constexpr float radius = 44;
  SheetCard card;
  card.width = std::min(520.0F, width - 32);
  card.height = card_height;
  card.x = (width - card.width) / 2;
  card.centre = width / 2;
  card.y = blend(height + 24, height - card_height - margin, reveal);
  const vw::Color sheet = backdrop_colour(config().lock_backdrop_colour, c.surface_container);
  if (!material_rect(ui, card.x, card.y, card.width, card.height, radius, with_alpha(sheet, 0.85F), 1))
    draw::rect(ui, card.x, card.y, card.width, card.height, radius, sheet);
  draw::rect(ui, card.centre - 18, card.y + 12, 36, 4, 2, c.outline_variant);
  return card;
}

vw::Color backdrop_colour(const std::string& name, vw::Color fallback) {
  const Colours& c = colours();
  if (name == "primary") return c.primary_container;
  if (name == "secondary") return c.secondary_container;
  if (name == "tertiary") return c.tertiary_container;
  if (name == "error") return c.error_container;
  if (name == "neutral") return c.surface;
  if (name == "black") return vw::rgb(0x000000);
  if (name == "white") return vw::rgb(0xffffff);
  if (name.size() == 7 && name[0] == '#') {
    try {
      return vw::rgb(static_cast<std::uint32_t>(std::stoul(name.substr(1), nullptr, 16)));
    } catch (const std::exception&) {}
  }
  return fallback;
}

LockLook lock_look() {
  const Config& c = config();
  return {c.lock_clock_format, c.lock_date_format, c.lock_clock_style, c.lock_clock_size, c.lock_tint,
          c.lock_blur_passes, c.lock_blur_offset, c.lock_show_status, c.lock_show_weather, {}, c.lock_blur};
}

std::string greeter_wallpaper_path() {
  std::string path = config().greeter_wallpaper;
  if (const char* home = std::getenv("HOME"); path.starts_with("~/") && home != nullptr) path = home + path.substr(1);
  return path;
}

// Its own wallpaper only when readable here: after an export it already is
// the theme's wallpaper.
LockLook greeter_look() {
  const Config& c = config();
  LockLook look = c.greeter_match_lock
                      ? lock_look()
                      : LockLook{c.greeter_clock_format, c.greeter_date_format, c.greeter_clock_style,
                                 c.greeter_clock_size, c.greeter_tint, c.greeter_blur_passes, c.greeter_blur_offset,
                                 c.greeter_show_status, c.greeter_show_weather, {}, c.greeter_blur};
  const std::string wallpaper = greeter_wallpaper_path();
  std::error_code error;
  if (!wallpaper.empty() && std::filesystem::is_regular_file(wallpaper, error) && access(wallpaper.c_str(), R_OK) == 0)
    look.wallpaper = wallpaper;
  return look;
}

std::vector<LockItem> default_lock_layout(bool greeter) {
  // Where the classic arrangement puts things on a 1080p screen.
  std::vector<LockItem> items{
      {.id = "clock", .x = 0.5F, .y = 0.207F, .ax = 0.5F, .ay = 0},
      {.id = "date", .x = 0.5F, .y = 0.566F, .ax = 0.5F, .ay = 0},
      {.id = "status", .x = 0.9875F, .y = 0.02F, .ax = 1, .ay = 0},
      {.id = "hint", .x = 0.5F, .y = 0.878F, .ax = 0.5F, .ay = 0},
      {.id = "weather", .x = 0.5F, .y = 0.64F, .ax = 0.5F, .ay = 0, .visible = false},
      {.id = "user", .x = 0.5F, .y = 0.08F, .ax = 0.5F, .ay = 0, .visible = false},
  };
  if (greeter) {
    items.push_back({.id = "power", .x = 0.0125F, .y = 0.0185F, .ax = 0, .ay = 0, .visible = config().greeter_show_power});
  } else {
    items.push_back({.id = "notifications", .x = 0.5F, .y = 0.628F, .ax = 0.5F, .ay = 0, .visible = config().lock_show_notifications});
    items.push_back({.id = "media", .x = 0.5F, .y = 0.69F, .ax = 0.5F, .ay = 0, .visible = config().lock_show_media});
  }
  return items;
}

std::string lock_item_label(const LockItem& item) {
  const std::string& id = item.id;
  if (id == "clock") return "Clock";
  if (id == "date") return "Date";
  if (id == "weather") return "Weather";
  if (id == "status") return "Status icons";
  if (id == "hint") return "Hint";
  if (id == "notifications") return "Notifications";
  if (id == "media") return "Media player";
  if (id == "user") return "Picture and name";
  if (id == "power") return "Power buttons";
  if (id.starts_with("text:")) return item.text.empty() ? "Text" : "Text: " + item.text;
  if (const std::string view = plugin_of(id); !view.empty()) {
    const auto colon = view.rfind(':');
    if (colon == std::string::npos) return view;
    std::string name = view.substr(colon + 1);
    if (name.starts_with("Lock") && name.size() > 4) name = name.substr(4);
    return plugins::plugin_name(view.substr(0, colon)) + " · " + name;
  }
  return id;
}

bool lock_item_allowed(const std::string& id, bool greeter) {
  if (greeter) return id != "notifications" && id != "media" && !id.starts_with("plugin:");
  return id != "power";
}

const std::vector<LockItem>* lock_layout_for(bool greeter) {
  const Config& c = config();
  if (!greeter) return c.lock_layout_custom ? &c.lock_layout : nullptr;
  if (c.greeter_layout_custom) return &c.greeter_layout;
  return c.greeter_match_lock && c.lock_layout_custom ? &c.lock_layout : nullptr;
}

void draw_avatar(vw::Ui& ui, const std::string& id, const std::string& picture, float centre_x, float y, float size,
                 const std::string& shape) {
  const Colours& c = colours();
  shapes::draw(ui, id, shape, centre_x - size / 2, y, size, c.primary_container);
  if (picture.empty() || !draw::icon(ui, picture, "", centre_x - size / 2, y, size, std::nullopt, 0, vw::ImageFit::cover, {},
                                     shapes::mask(shape)))
    draw::glyph(ui, "person", centre_x, y + size / 2, size * 0.45F, c.primary);
}

void draw_lock_page(vw::Ui& ui, const LockPage& page) {
  const Colours& c = colours();
  const Config& settings = config();
  const LockLook look = page.look ? *page.look : lock_look();
  const float width = page.width;
  const float height = page.height;
  const float reveal = page.reveal;
  const float intro = page.intro;
  const float sheet_room = page.sheet_room > 0 ? page.sheet_room : lock_sheet_height;

  // Fully opaque first, whatever the wallpaper does: nothing behind a lock
  // screen may ever show.
  ui.rectangle({.width = width, .height = height, .color = c.surface,
                .input = {.on_click = [click = page.on_click](const vw::ClickEvent&) {
                  if (click) click();
                }}});
  if (look.blur) {
    blob::Group group(with_alpha(c.surface, std::clamp(look.tint, 0.0F, 1.0F)), 1);
    group.shadows(false);
    if (apply_lock_frost(group, look.blur_passes, look.blur_offset, look.wallpaper)) {
      group.frame({-8, -8, 2, 2, {}});
      group.draw(ui, width, height);
    }
  } else {
    // Sharp: the wallpaper itself, with the tint over it.
    ui.image({.path = look.wallpaper.empty() ? current_wallpaper_path() : look.wallpaper, .width = width, .height = height,
              .fit = vw::ImageFit::cover});
    ui.rectangle({.width = width, .height = height, .color = with_alpha(c.surface, std::clamp(look.tint, 0.0F, 1.0F)),
                  .place = {.x = 0, .y = 0}});
  }

  const std::string date = system::format_time(look.date_format);
  const auto weather = look.show_weather ? weather::current() : std::nullopt;
  if (page.layout != nullptr) {
    draw_layout(ui, page, look, date, weather);
    if (intro < 0.999F)
      ui.rectangle({.width = width, .height = height, .color = with_alpha(c.surface, 1 - intro), .place = {.x = 0, .y = 0}});
    return;
  }

  const float centre = width / 2;
  if (look.show_status) draw_status(ui, width - 24, 1 - reveal);

  // What sits under the clock decides where the whole block goes.
  const bool widgets = page.session_widgets && reveal < 0.2F;
  const bool notifications = widgets && settings.lock_show_notifications && !notify::history().empty();
  const bool media = widgets && settings.lock_show_media && has_media();

  // The clock: huge and stacked, shrinking out of the sheet's way.
  const std::string time = system::format_time(look.clock_format);
  const auto colon = time.find(':');
  const bool stacked = look.clock_style == "stacked" && colon != std::string::npos;
  const float base = static_cast<float>(std::clamp(look.clock_size, 48, 260)) * (stacked ? 1.6F : 1.0F);
  const float size = blend(base, base * 0.55F, reveal);
  const float line = size * 0.86F;
  const float clock_height = line * (stacked ? 2.0F : 1.0F);
  const float block = clock_height + 24 + pill_height + (notifications ? 24 + 40 : 0) +
                      (media ? 24 + media_height : 0);

  // Idle: a little above the middle, clear of the hint. Raised: centred in
  // the room the sheet leaves.
  const float idle_top = std::max(72.0F, (height - 150 - block) * 0.45F);
  const float raised_top = std::max(20.0F, (height - sheet_room - block) / 2);
  float y = blend(idle_top, raised_top, reveal) - (1 - intro) * 96;


  if (stacked) {
    const draw::TextStyle top{.size = size, .color = c.primary, .weight = vw::weight::black, .family = font::display};
    const draw::TextStyle bottom{.size = size, .color = mix(c.primary, c.tertiary, 0.7F), .weight = vw::weight::black, .family = font::display};
    draw::text_centered(ui, time.substr(0, colon), centre, y + line / 2, top);
    draw::text_centered(ui, time.substr(colon + 1), centre, y + line * 1.5F, bottom);
  } else {
    draw::text_centered(ui, time, centre, y + line / 2, {.size = size, .color = c.primary, .weight = vw::weight::black, .family = font::display});
  }
  y += clock_height + 24;

  draw_date_pill(ui, centre, y, date, weather);
  y += pill_height;

  if (notifications) y = draw_notifications(ui, centre, y + 24);
  if (media) draw_media(ui, centre, y + 24);

  // The way in, at the bottom: sinks as the sheet comes up.
  if (reveal < 0.6F && !page.hint.empty()) {
    const float sink = reveal * 140;
    constexpr float disc = 52;
    const float disc_y = height - 132 + sink;
    shapes::draw(ui, "lock.hint", "cookie9sided", centre - disc / 2, disc_y, disc, with_alpha(c.primary_container, 0.95F));
    draw::glyph(ui, page.hint_glyph, centre, disc_y + disc / 2, 24, c.primary);
    draw::text_centered(ui, page.hint, centre, disc_y + disc + 24,
                        {.size = 14, .color = c.text_muted, .weight = vw::weight::medium, .family = font::utility});
  }
  if (intro < 0.999F)
    ui.rectangle({.width = width, .height = height, .color = with_alpha(c.surface, 1 - intro), .place = {.x = 0, .y = 0}});
}

void draw_lock_preview(vw::Ui& ui, float width, float height, bool sheet) {
  const float reveal = sheet ? 1.0F : 0.0F;
  const char* user = std::getenv("USER");
  draw_lock_page(ui, {.width = width, .height = height, .reveal = reveal, .hint = "Press any key to unlock", .hint_glyph = "lock",
                      .session_widgets = true, .layout = lock_layout_for(false), .user_name = user != nullptr ? user : "",
                      .user_picture = system::avatar()});
  draw_sheet(ui, width, height, reveal, false, 0, false);
}

void lock_session() {
  Shell& state = shell();
  state.locked = true;
  state.launcher_open = false;
  state.power_pinned = false;
  state.quick_share_open = false;
  state.sidebar_reveal = 0;
  state.stack_reveal = 0;
  const int entrances = lock_state().entrances + 1;
  lock_state() = {};
  lock_state().entrances = entrances;
}

void lock_surface(vw::Ui& ui) {
  if (!shell().locked) return;
  ui.session_lock({.on_unlock = [] {
    shell().locked = false;
    close_sheet();
  }});
  ui.lock_surface();

  const Config& settings = config();
  LockState& state = lock_state();
  const vw::LockSurfaceContext context = vw::lock_surface_context();
  const auto width = static_cast<float>(context.width);
  const auto height = static_cast<float>(context.height);

  const bool failed = vw::session_authentication_failed();
  if (failed && !state.was_failed) {
    state.failed_at = Clock::now();
    state.last_input = Clock::now();
  }
  state.was_failed = failed;
  // Idle: the sheet goes back down and takes what was typed with it.
  if (state.sheet && Clock::now() - state.last_input > std::chrono::seconds(30)) close_sheet();

  ui.keyboard([](const vw::KeyEvent& key) {
    if (key.name == "Escape") {
      close_sheet();
      return true;
    }
    // The first key only raises the sheet (a habitual space or Enter must
    // not land in the password); keys reach the field once it is up.
    if (!lock_state().sheet) {
      open_sheet();
      return true;
    }
    // Holding the activation key: its repeats are not typing either.
    if (key.repeat && Clock::now() - lock_state().opened_at < std::chrono::milliseconds(800)) return true;
    lock_state().last_input = Clock::now();
    return false;
  });

  const float reveal = std::clamp(static_cast<float>(vw::animate("lock.sheet", state.sheet ? 1 : 0, motion::bounce())), 0.0F, 1.0F);
  // Entrance: the page comes up out of solid surface while the clock drops
  // into place. The opaque base is never part of it.
  const float intro = settings.wallpaper_transition == "none"
                          ? 1.0F
                          : std::clamp(static_cast<float>(vw::animate(
                                           std::format("lock.intro.{}", state.entrances), 1,
                                           {.duration = 0.9, .initial = 0, .easing = vw::Easing::out_expo})),
                                       0.0F, 1.0F);
  draw_lock_page(ui, {.width = width, .height = height, .reveal = reveal, .intro = intro,
                      .hint = "Press any key to unlock", .hint_glyph = "lock", .session_widgets = true,
                      .on_click = [] { open_sheet(); }, .layout = lock_layout_for(false),
                      .user_name = std::getenv("USER") != nullptr ? std::getenv("USER") : "",
                      .user_picture = system::avatar()});

  // A short decaying shake on a wrong password.
  const float since = std::chrono::duration<float>(Clock::now() - state.failed_at).count();
  float shake = 0;
  if (failed && since < 0.45F) {
    shake = std::sin(since * 42) * 12 * (1 - since / 0.45F);
    vw::request_animation_continuation();
  }
  // Drawn (and focused) even while hidden below the edge, so it is ready the
  // moment the sheet is raised; keys only reach it once raised (see above).
  draw_sheet(ui, width, height, reveal, failed, shake);
}

}  // namespace blx
