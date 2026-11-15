// The lock screen: one more surface of the shell, drawn only while locked. A
// glanceable Android-style page; any key or click raises the password sheet.
// vw handles ext_session_lock_v1, one surface per output and the PAM check.
#include <vw/vw.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <format>
#include <string>

#include "net.hpp"
#include "blob.hpp"
#include "config.hpp"
#include "draw.hpp"
#include "lockpage.hpp"
#include "notify.hpp"
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
constexpr float sheet_height = lock_sheet_height;
constexpr float sheet_width = 560;

struct LockState {
  // Bumped per lock so the entrance tween starts over each time.
  int entrances = 0;
  bool sheet = false;
  Clock::time_point opened_at{};
  Clock::time_point last_input{};
  bool was_failed = false;
  Clock::time_point failed_at{};
};

LockState& lock_state() {
  static LockState state;
  return state;
}

void close_sheet() {
  lock_state().sheet = false;
  vw::reset_text_field(password_id);
}

void open_sheet() {
  if (!lock_state().sheet) lock_state().opened_at = Clock::now();
  lock_state().sheet = true;
  lock_state().last_input = Clock::now();
}

float blend(float from, float to, float t) { return from + (to - from) * t; }

// A tonal chip, centred on x; returns its width.
float chip_width(const std::string& glyph, const std::string& label) {
  return draw::measure(label, 15, vw::weight::medium, font::utility) + 32 + (glyph.empty() ? 0 : 26);
}

void chip(vw::Ui& ui, float x, float y, const std::string& glyph, const std::string& label) {
  const Colours& c = colours();
  constexpr float height = 40;
  const float width = chip_width(glyph, label);
  draw::rect(ui, x, y, width, height, height / 2, with_alpha(c.surface_container_high, 0.9F));
  float text_x = x + 16;
  if (!glyph.empty()) {
    draw::glyph(ui, glyph, text_x + 10, y + height / 2, 20, c.primary);
    text_x += 26;
  }
  draw::text_middle(ui, label, text_x, y + height / 2, {.size = 15, .color = c.text, .weight = vw::weight::medium, .family = font::utility});
}

void draw_status(vw::Ui& ui, float right) {
  const Colours& c = colours();
  float x = right;
  const float y = 40;
  if (const auto battery = vw::upower::display_device()) {
    const std::string label = std::format("{}%", std::lround(battery->percentage));
    x -= draw::measure(label, 15, vw::weight::medium, font::utility);
    draw::text_middle(ui, label, x, y, {.size = 15, .color = c.text, .weight = vw::weight::medium, .family = font::utility});
    x -= 16;
    draw::glyph(ui, battery->percentage < 20 ? "battery_alert" : "battery_full", x, y, 22, c.text);
    x -= 30;
  }
  if (const auto adapter = vw::bluetooth::adapter(); adapter && adapter->powered) {
    draw::glyph(ui, "bluetooth", x, y, 20, c.text);
    x -= 30;
  }
  const auto network = net::status();
  const bool wired = network.primary == vw::network::Kind::wired;
  draw::glyph(ui, wired ? "cable" : network.wifi_enabled ? "wifi" : "wifi_off", x, y, 20, c.text);
}

// Notifications as icons only: what arrived, never what it says.
float draw_notifications(vw::Ui& ui, float centre, float y) {
  const Colours& c = colours();
  const auto groups = notify::history();
  if (groups.empty()) return y;
  constexpr float disc = 40;
  constexpr std::size_t shown_most = 7;
  const std::size_t shown = std::min(groups.size(), shown_most);
  const bool more = groups.size() > shown;
  const float count = static_cast<float>(shown) + (more ? 1 : 0);
  float x = centre - (count * disc + (count - 1) * 8) / 2;
  for (std::size_t index = 0; index < shown; ++index) {
    draw::rect(ui, x, y, disc, disc, disc / 2, with_alpha(c.surface_container_high, 0.9F));
    if (!draw::icon(ui, groups[index].entries.front().icon, "", x + 9, y + 9, 22))
      draw::glyph(ui, "notifications", x + disc / 2, y + disc / 2, 20, c.primary);
    x += disc + 8;
  }
  if (more) {
    draw::rect(ui, x, y, disc, disc, disc / 2, c.primary);
    draw::text_centered(ui, std::format("+{}", groups.size() - shown), x + disc / 2, y + disc / 2,
                        {.size = 13, .color = c.primary_text, .weight = vw::weight::bold, .family = font::utility});
  }
  return y + disc;
}

float draw_media(vw::Ui& ui, float centre, float y) {
  const auto player = vw::mpris::active();
  if (!player || player->title.empty()) return y;
  const Colours& c = colours();
  constexpr float width = 460;
  constexpr float height = 96;
  constexpr float padding = 12;
  const float x = centre - width / 2;
  const bool playing = player->playback_status == "Playing";
  draw::rect(ui, x, y, width, height, 32, with_alpha(c.surface_container_high, 0.92F));
  constexpr float art_size = height - padding * 2;
  draw::rect(ui, x + padding, y + padding, art_size, art_size, 22, c.primary_container);
  if (!draw::art(ui, art::path(player->art_url), x + padding, y + padding, art_size, 22))
    draw::glyph(ui, "music_note", x + padding + art_size / 2, y + height / 2, 30, c.primary);
  constexpr float play_width = 64;
  constexpr float play_height = 48;
  const float play_x = x + width - padding - 8 - play_width;
  const float play_y = y + (height - play_height) / 2;
  const float text_x = x + padding + art_size + 14;
  const float text_width = play_x - 40 - text_x;
  draw::text(ui, player->title, text_x, y + 25, {.size = 16, .color = c.text, .weight = vw::weight::semibold, .family = font::display, .max_width = text_width});
  draw::text(ui, player->artist.empty() ? player->name : player->artist, text_x, y + 49,
             {.size = 13, .color = c.text_muted, .family = font::display, .max_width = text_width});
  draw::glyph(ui, "skip_next", play_x - 22, y + height / 2, 24, player->can_go_next ? c.text : c.outline_variant);
  if (player->can_go_next)
    draw::state_layer(ui, play_x - 40, y + height / 2 - 18, 36, 36, 18, c.text,
                      {.on_click = [](const vw::ClickEvent&) { vw::mpris::next(); }});
  const float radius = std::max(0.0F, static_cast<float>(vw::animate("lock.play", playing ? 15 : play_height / 2, {.duration = 0.35, .easing = vw::Easing::out_back})));
  draw::rect(ui, play_x, play_y, play_width, play_height, radius, c.primary);
  draw::glyph(ui, playing ? "pause" : "play_arrow", play_x + play_width / 2, play_y + play_height / 2, 26, c.primary_text);
  draw::state_layer(ui, play_x, play_y, play_width, play_height, radius, c.primary_text,
                    {.on_click = [](const vw::ClickEvent&) { vw::mpris::play_pause(); }});
  return y + height;
}

// The password sheet, risen from the bottom edge; `reveal` 0 is below it.
void draw_sheet(vw::Ui& ui, float width, float height, float reveal, bool failed, float shake) {
  const Colours& c = colours();
  Config& settings = config();
  const float sheet = std::min(sheet_width, width - 32);
  const float x = (width - sheet) / 2;
  const float y = height - sheet_height * reveal;
  ui.rectangle({.width = sheet, .height = sheet_height + 40, .color = c.surface_container,
                .corner_radii = {40, 40, 0, 0}, .place = {.x = x, .y = y}});
  const float centre = width / 2;
  draw::rect(ui, centre - 16, y + 12, 32, 4, 2, c.outline_variant);

  float cursor = y + 32;
  if (settings.lock_show_user) {
    constexpr float badge = 88;
    shapes::draw(ui, "lock.avatar", "cookie9sided", centre - badge / 2, cursor, badge, c.primary_container, 0, c.primary_container);
    if (!draw::icon(ui, system::avatar(), "", centre - badge / 2, cursor, badge, std::nullopt, 0, vw::ImageFit::cover, {},
                    shapes::mask("cookie9sided")))
      draw::glyph(ui, "person", centre, cursor + badge / 2, 40, c.primary);
    cursor += badge + 10;
    const char* user = std::getenv("USER");
    draw::text_centered(ui, user != nullptr ? user : "", centre, cursor + 12,
                        {.size = 18, .color = c.text, .weight = vw::weight::semibold, .family = font::display});
    cursor += 34;
  }

  constexpr float field_height = 60;
  const float field_width = std::min(460.0F, sheet - 48);
  const float field_x = centre - field_width / 2 + shake;
  ui.rectangle({.width = field_width, .height = field_height, .radius = field_height / 2, .color = c.surface_container_highest,
                .border_width = failed ? 2.0F : 0.0F, .border_color = c.error, .place = {.x = field_x, .y = cursor}});
  draw::glyph(ui, "lock", field_x + 32, cursor + field_height / 2, 22, failed ? c.error : c.primary);
  ui.text_field({.id = password_id, .placeholder = "Enter password", .width = field_width - 128, .height = field_height, .radius = 0,
                 .border_width = 0, .padding = 4, .color = with_alpha(c.surface_container_highest, 0),
                 .focus_color = with_alpha(c.primary, 0), .text_color = c.text, .placeholder_color = c.text_muted,
                 .caret_color = c.primary, .font = font::utility, .size = 17, .password = true,
                 .authenticate = true, .autofocus = true, .place = {.x = field_x + 58, .y = cursor}});
  // Enter submits: this is the affordance, as on Android's password bouncer.
  constexpr float go = 44;
  draw::rect(ui, field_x + field_width - go - 8, cursor + (field_height - go) / 2, go, go, go / 2, c.primary);
  draw::glyph(ui, "arrow_forward", field_x + field_width - 8 - go / 2, cursor + field_height / 2, 22, c.primary_text);
  draw::text_centered(ui, failed ? "Wrong password" : "Press Enter to unlock", centre, cursor + field_height + 22,
                      {.size = 13, .color = failed ? c.error : c.text_muted, .weight = vw::weight::medium, .family = font::utility});
}

}  // namespace

void draw_lock_page(vw::Ui& ui, const LockPage& page) {
  const Colours& c = colours();
  const Config& settings = config();
  const float width = page.width;
  const float height = page.height;
  const float reveal = page.reveal;
  const float intro = page.intro;

  // Fully opaque first, whatever the wallpaper does: nothing behind a lock
  // screen may ever show.
  ui.rectangle({.width = width, .height = height, .color = c.surface,
                .input = {.on_click = [click = page.on_click](const vw::ClickEvent&) {
                  if (click) click();
                }}});
  blob::Group group(with_alpha(c.surface, std::clamp(settings.lock_tint, 0.0F, 1.0F)), 1);
  group.shadows(false);
  if (apply_lock_frost(group)) {
    group.frame({-8, -8, 2, 2, {}});
    group.draw(ui, width, height);
  }

  const float centre = width / 2;

  if (settings.lock_show_status && reveal < 0.99F) draw_status(ui, width - 36);

  // The clock: huge and stacked, shrinking out of the sheet's way.
  const std::string time = system::format_time(settings.lock_clock_format);
  const auto colon = time.find(':');
  const bool stacked = settings.lock_clock_style == "stacked" && colon != std::string::npos;
  const float base = static_cast<float>(std::clamp(settings.lock_clock_size, 48, 260)) * (stacked ? 1.6F : 1.0F);
  const float size = blend(base, base * 0.42F, reveal);
  const float line = size * 0.86F;
  const float room = height - sheet_height;
  const float clock_y = blend(height * 0.05F, std::max(24.0F, room * 0.1F), reveal) - (1 - intro) * 96;
  float y = clock_y;
  if (stacked) {
    const draw::TextStyle top{.size = size, .color = c.primary, .weight = vw::weight::black, .family = font::display};
    const draw::TextStyle bottom{.size = size, .color = mix(c.primary, c.tertiary, 0.7F), .weight = vw::weight::black, .family = font::display};
    draw::text_centered(ui, time.substr(0, colon), centre, y + line / 2, top);
    draw::text_centered(ui, time.substr(colon + 1), centre, y + line * 1.5F, bottom);
    y += line * 2;
  } else {
    draw::text_centered(ui, time, centre, y + line / 2, {.size = size, .color = c.primary, .weight = vw::weight::black, .family = font::display});
    y += line;
  }
  y += 16;

  // Date and weather under it.
  const std::string date = system::format_time(settings.lock_date_format);
  const auto weather = settings.lock_show_weather ? weather::current() : std::nullopt;
  const float date_width = chip_width("", date);
  const float weather_width = weather ? chip_width(weather->glyph, weather->temperature) : 0;
  float chip_x = centre - (date_width + (weather ? weather_width + 8 : 0)) / 2;
  chip(ui, chip_x, y, "", date);
  if (weather) chip(ui, chip_x + date_width + 8, y, weather->glyph, weather->temperature);
  y += 40 + 16;

  // The rest leaves room for the sheet.
  if (page.session_widgets && reveal < 0.2F) {
    if (settings.lock_show_notifications) {
      const float next = draw_notifications(ui, centre, y);
      if (next > y) y = next + 16;
    }
    if (settings.lock_show_media) draw_media(ui, centre, y);
  }

  // The hint sinks as the sheet comes up.
  if (reveal < 0.6F && !page.hint.empty()) {
    const float hint_width = chip_width(page.hint_glyph, page.hint);
    chip(ui, centre - hint_width / 2, height - 72 + reveal * 80, page.hint_glyph, page.hint);
  }
  if (intro < 0.999F)
    ui.rectangle({.width = width, .height = height, .color = with_alpha(c.surface, 1 - intro), .place = {.x = 0, .y = 0}});

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
                      .on_click = [] { open_sheet(); }});

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
