// The lock screen: one more surface of the shell, drawn only while locked. A
// glanceable Android-style page; any key or click raises the password sheet.
// vw handles ext_session_lock_v1, one surface per output and the PAM check.
#include <vw/vw.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <format>
#include <optional>
#include <string>
#include <vector>

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

draw::TextStyle pill_text() {
  return {.size = 15, .color = colours().text, .weight = vw::weight::medium, .family = font::utility};
}

// The status icons, top right, in one pill.
void draw_status(vw::Ui& ui, float right, float fade) {
  if (fade <= 0.01F) return;
  const Colours& c = colours();
  struct Item {
    std::string glyph;
    std::string label;
  };
  std::vector<Item> items;
  const auto network = net::status();
  const bool wired = network.primary == vw::network::Kind::wired;
  items.push_back({wired ? "lan" : network.wifi_enabled ? "wifi" : "wifi_off", ""});
  if (const auto adapter = vw::bluetooth::adapter(); adapter && adapter->powered) items.push_back({"bluetooth", ""});
  if (const auto battery = vw::upower::display_device()) {
    const char* glyph = battery->percentage < 20 ? "battery_alert" : battery->percentage < 60 ? "battery_4_bar" : "battery_full";
    items.push_back({glyph, std::format("{}%", std::lround(battery->percentage))});
  }
  constexpr float icon = 20;
  constexpr float gap = 14;
  constexpr float pill = 40;
  float width = 32 + gap * static_cast<float>(items.size() - 1);
  for (const Item& item : items)
    width += icon + (item.label.empty() ? 0 : 6 + draw::measure(item.label, 14, vw::weight::medium, font::utility));
  constexpr float y = 22;
  float x = right - width;
  draw::rect(ui, x, y, width, pill, pill / 2, with_alpha(c.surface_container_high, 0.72F * fade));
  x += 16;
  for (const Item& item : items) {
    draw::glyph(ui, item.glyph, x + icon / 2, y + pill / 2, icon, with_alpha(c.text, fade));
    x += icon;
    if (!item.label.empty()) {
      x += 6;
      draw::text_middle(ui, item.label, x, y + pill / 2,
                        {.size = 14, .color = with_alpha(c.text, fade), .weight = vw::weight::medium, .family = font::utility});
      x += draw::measure(item.label, 14, vw::weight::medium, font::utility);
    }
    x += gap;
  }
}

// Date and weather in one pill.
void draw_date_pill(vw::Ui& ui, float centre, float y, const std::string& date, const std::optional<weather::Report>& now) {
  const Colours& c = colours();
  float width = 40 + draw::measure(date, 15, vw::weight::medium, font::utility);
  if (now) width += 20 + 24 + draw::measure(now->temperature, 15, vw::weight::medium, font::utility);
  float x = centre - width / 2;
  draw::rect(ui, x, y, width, pill_height, pill_height / 2, with_alpha(c.surface_container_high, 0.78F));
  x += 20;
  draw::text_middle(ui, date, x, y + pill_height / 2, pill_text());
  x += draw::measure(date, 15, vw::weight::medium, font::utility);
  if (now) {
    x += 20;
    draw::glyph(ui, now->glyph, x + 10, y + pill_height / 2, 20, c.primary);
    x += 24;
    draw::text_middle(ui, now->temperature, x, y + pill_height / 2, pill_text());
  }
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
    draw::rect(ui, x, y, disc, disc, disc / 2, with_alpha(c.surface_container_high, 0.85F));
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

constexpr float media_height = 104;

bool has_media() {
  const auto player = vw::mpris::active();
  return player && !player->title.empty();
}

// Now playing: art, title and artist, skip, and a play button that morphs.
void draw_media(vw::Ui& ui, float centre, float y) {
  const auto player = vw::mpris::active();
  if (!player || player->title.empty()) return;
  const Colours& c = colours();
  constexpr float width = 480;
  constexpr float height = media_height;
  constexpr float padding = 14;
  const float x = centre - width / 2;
  const bool playing = player->playback_status == "Playing";
  draw::rect(ui, x, y, width, height, 36, with_alpha(c.surface_container_high, 0.9F));
  constexpr float art_size = height - padding * 2;
  draw::rect(ui, x + padding, y + padding, art_size, art_size, 24, c.primary_container);
  if (!draw::art(ui, art::path(player->art_url), x + padding, y + padding, art_size, 24))
    draw::glyph(ui, "music_note", x + padding + art_size / 2, y + height / 2, 32, c.primary);
  constexpr float play_size = 56;
  const float play_x = x + width - padding - 4 - play_size;
  const float play_y = y + (height - play_size) / 2;
  constexpr float skip = 40;
  const float skip_x = play_x - 10 - skip;
  const float skip_y = y + (height - skip) / 2;
  const float text_x = x + padding + art_size + 16;
  const float text_width = skip_x - 12 - text_x;
  draw::text(ui, player->title, text_x, y + 30,
             {.size = 16, .color = c.text, .weight = vw::weight::semibold, .family = font::display, .max_width = text_width});
  draw::text(ui, player->artist.empty() ? player->name : player->artist, text_x, y + 56,
             {.size = 13, .color = c.text_muted, .family = font::display, .max_width = text_width});
  draw::glyph(ui, "skip_next", skip_x + skip / 2, y + height / 2, 22, player->can_go_next ? c.text : c.outline_variant);
  if (player->can_go_next)
    draw::state_layer(ui, skip_x, skip_y, skip, skip, skip / 2, c.text,
                      {.on_click = [](const vw::ClickEvent&) { vw::mpris::next(); }});
  // Playing: a squircle; paused: a circle, as Android's media controls do.
  const float radius = std::max(0.0F, static_cast<float>(vw::animate("lock.play", playing ? 18 : play_size / 2,
                                                                     {.duration = 0.35, .easing = vw::Easing::out_back})));
  draw::rect(ui, play_x, play_y, play_size, play_size, radius, c.primary);
  draw::glyph(ui, playing ? "pause" : "play_arrow", play_x + play_size / 2, play_y + play_size / 2, 28, c.primary_text);
  draw::state_layer(ui, play_x, play_y, play_size, play_size, radius, c.primary_text,
                    {.on_click = [](const vw::ClickEvent&) { vw::mpris::play_pause(); }});
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
  draw::rect(ui, card.x, card.y, card.width, card.height, radius, c.surface_container);
  draw::rect(ui, card.centre - 18, card.y + 12, 36, 4, 2, c.outline_variant);
  return card;
}

void draw_avatar(vw::Ui& ui, const std::string& id, const std::string& picture, float centre_x, float y, float size) {
  const Colours& c = colours();
  shapes::draw(ui, id, "cookie9sided", centre_x - size / 2, y, size, c.primary_container);
  if (picture.empty() || !draw::icon(ui, picture, "", centre_x - size / 2, y, size, std::nullopt, 0, vw::ImageFit::cover, {},
                                     shapes::mask("cookie9sided")))
    draw::glyph(ui, "person", centre_x, y + size / 2, size * 0.45F, c.primary);
}

void draw_lock_page(vw::Ui& ui, const LockPage& page) {
  const Colours& c = colours();
  const Config& settings = config();
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
  blob::Group group(with_alpha(c.surface, std::clamp(settings.lock_tint, 0.0F, 1.0F)), 1);
  group.shadows(false);
  if (apply_lock_frost(group)) {
    group.frame({-8, -8, 2, 2, {}});
    group.draw(ui, width, height);
  }

  const float centre = width / 2;
  if (settings.lock_show_status) draw_status(ui, width - 24, 1 - reveal);

  // What sits under the clock decides where the whole block goes.
  const std::string date = system::format_time(settings.lock_date_format);
  const auto weather = settings.lock_show_weather ? weather::current() : std::nullopt;
  const bool widgets = page.session_widgets && reveal < 0.2F;
  const bool notifications = widgets && settings.lock_show_notifications && !notify::history().empty();
  const bool media = widgets && settings.lock_show_media && has_media();

  // The clock: huge and stacked, shrinking out of the sheet's way.
  const std::string time = system::format_time(settings.lock_clock_format);
  const auto colon = time.find(':');
  const bool stacked = settings.lock_clock_style == "stacked" && colon != std::string::npos;
  const float base = static_cast<float>(std::clamp(settings.lock_clock_size, 48, 260)) * (stacked ? 1.6F : 1.0F);
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
  draw_lock_page(ui, {.width = width, .height = height, .reveal = reveal, .hint = "Press any key to unlock", .hint_glyph = "lock",
                      .session_widgets = true});
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
