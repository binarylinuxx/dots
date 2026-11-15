// The top dashboard, ported from ~/shell/blxshell/dashboard and
// popouts/Popout.qml: it slides out of the top frame edge while the pointer
// is on that edge or inside it, and morphs between its four pages.
#include <vw/vw.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <functional>
#include <string>
#include <vector>

#include "config.hpp"
#include "draw.hpp"
#include "parts.hpp"
#include "services.hpp"
#include "shapes.hpp"
#include "state.hpp"
#include "system.hpp"
#include "widgets.hpp"

namespace blx {
namespace {

using namespace props;
using Clock = std::chrono::steady_clock;

constexpr float content_top = 72;

struct Size {
  float width;
  float height;
};

struct Destination {
  const char* label;
  const char* icon;
};
constexpr Destination destinations[] = {{"DASHBOARD", "space_dashboard"},
                                        {"PERFORMANCE", "monitoring"},
                                        {"MEDIA", "music_note"},
                                        {"WEATHER", "partly_cloudy_day"}};

float available_width = 1920 - frame_thickness * 2;

Size page_size(int page) {
  switch (page) {
    case 1: return {640, 215};
    case 2: return {640, 280};
    case 3: {
      const float width = std::clamp(available_width - 48, 320.0F, 960.0F);
      const int columns = std::clamp(static_cast<int>(width / 105), 3, 7);
      const auto report = weather::current();
      const int days = report ? static_cast<int>(report->forecast.size()) : 0;
      const int rows = (days + columns - 1) / columns;
      return {width, 38 + 14 + 112 + 12 + 58 + 14 + 13 + 8 + rows * 66.0F + std::max(0, rows - 1) * 10.0F};
    }
    default: return {839, 450};
  }
}

float tab_width(const Destination& destination) {
  return 18 + 7 + draw::measure(destination.label, 9, vw::weight::semibold, font::display) + 26;
}

float tabs_width() {
  float width = 8;
  for (const auto& destination : destinations) width += tab_width(destination) + 2;
  return width - 2;
}

Size dashboard_size(int page) {
  const Size content = page_size(page);
  return {std::max(content.width + 48, tabs_width() + 48), content.height + content_top + 24};
}

void card(vw::Ui& ui, float x, float y, float width, float height, vw::Color colour, float radius = card_radius) {
  draw::rect(ui, x, y, width, height, radius, colour);
}

draw::TextStyle label(vw::Color colour, float size = 9, const char* family = font::display) {
  return {.size = size, .color = colour, .weight = vw::weight::semibold, .family = family};
}

std::string upper(std::string text) {
  std::ranges::transform(text, text.begin(), [](unsigned char ch) { return std::toupper(ch); });
  return text;
}

std::string clock_text(double seconds) {
  const auto total = static_cast<long>(std::max(0.0, seconds));
  return std::format("{}:{:02}", total / 60, total % 60);
}

// The first readable image in ~/.bprofile (User.qml).
std::string avatar() {
  static const std::string path = [] {
    const char* home = std::getenv("HOME");
    std::error_code error;
    for (const auto& entry :
         std::filesystem::directory_iterator(std::string(home ? home : ".") + "/.bprofile", error)) {
      std::string extension = entry.path().extension().string();
      std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return std::tolower(c); });
      if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".webp" ||
          extension == ".gif")
        return entry.path().string();
    }
    return std::string();
  }();
  return path;
}

struct Player {
  bool available = false;
  bool playing = false;
  vw::mpris::Player data;
};

Player player() {
  Player result;
  if (const auto active = vw::mpris::active()) {
    result.available = true;
    result.data = *active;
    result.playing = active->playback_status == "Playing";
  }
  return result;
}

void seek(double fraction) {
  if (const auto active = vw::mpris::active(); active && active->length > 0)
    vw::mpris::set_position(fraction * active->length);
}

// TransportButton from Media.qml (filled) or MediaPage.qml (plain).
void transport(vw::Ui& ui, float x, float y, float size, const std::string& glyph, bool primary, bool enabled,
               float icon_size, std::function<void()> run, bool filled) {
  const Colours& c = colours();
  const float alpha = enabled ? 1 : (filled ? 0.32F : 0.3F);
  if (primary) draw::rect(ui, x, y, size, size, size / 2, with_alpha(c.primary, alpha));
  else if (filled) draw::rect(ui, x, y, size, size, size / 2, with_alpha(c.surface_variant, alpha));
  draw::glyph(ui, glyph, x + size / 2, y + size / 2, icon_size, with_alpha(primary ? c.primary_text : c.text, alpha));
  if (!enabled) return;
  vw::Input input{.on_click = [run](const vw::ClickEvent&) { run(); }};
  if (filled && !primary) input.hover = vw::Hover{.color = c.surface_container_high, .duration = 0.12F};
  draw::state_layer(ui, x, y, size, size, size / 2, primary ? c.primary_text : c.text, std::move(input));
}

// --- Overview cards -------------------------------------------------------------

void small_weather(vw::Ui& ui, float x, float y) {
  const Colours& c = colours();
  card(ui, x, y, 275, 106, c.surface_container);
  const auto report = weather::current();
  const bool loading = weather::loading();
  shapes::draw(ui, "weather.small", report ? "sunny" : "cloud", x + 16, y + 24, 58, c.primary_container);
  draw::glyph(ui, loading ? "sync" : report ? report->glyph : "cloud_off", x + 45, y + 53, 25, c.primary);
  const float tx = x + 88;
  const float tw = 275 - 88 - 16;
  draw::text(ui, "WEATHER", tx, y + 27, label(c.secondary));
  draw::text(ui, loading ? "Loading weather"
                         : report ? std::format("{} · {}", report->temperature, report->condition)
                                  : "Weather unavailable",
             tx, y + 42, {.size = 17, .color = c.text, .weight = vw::weight::semibold, .family = font::display,
                          .max_width = tw});
  draw::text(ui, report ? report->location : loading ? "Contacting weather service" : "Check network connection",
             tx, y + 67, {.size = 9, .color = c.text_muted, .family = font::display, .max_width = tw});
}

void user(vw::Ui& ui, float x, float y) {
  const Colours& c = colours();
  card(ui, x, y, 340, 106, c.surface_container, 45);
  const std::string image = avatar();
  if (image.empty()) {
    shapes::draw(ui, "user.logo", "gem", x + 28, y + 28, 54, c.primary_container);
    draw::glyph(ui, "desktop_windows", x + 55, y + 55, 24, c.primary);
  }
  const float tx = x + 32;
  const float tw = 242 - 12 - 32;
  float ty = y + 26;
  draw::text(ui, "LOCAL NODE / ONLINE", tx, ty, label(c.primary));
  ty += 14;
  draw::text(ui, upper(system::hostname()), tx, ty,
             {.size = 18, .color = c.text, .weight = vw::weight::black, .family = font::display, .max_width = tw});
  ty += 25;
  draw::text(ui, system::os_name(), tx, ty, {.size = 10, .color = c.text_muted, .family = font::display, .max_width = tw});
  ty += 15;
  draw::text(ui, "UP " + system::uptime(), tx, ty, label(c.text_muted));
  // The avatar in a pill shape, overflowing the right padding.
  const float ax = x + 242;
  const float ay = y + (106 - 85) / 2;
  shapes::draw(ui, "user.avatar", "pill", ax, ay, 85, c.secondary_container, 2, with_alpha(c.secondary, 0.55F));
  if (!image.empty()) draw::icon(ui, image, "", ax + 6, ay + 6, 73, std::nullopt, 0, vw::ImageFit::cover, {}, shapes::mask("pill"));
  else draw::glyph(ui, "account_circle", ax + 42.5F, ay + 42.5F, 30, c.text);
}

void date_time(vw::Ui& ui, float x, float y) {
  const Colours& c = colours();
  card(ui, x, y, 110, 332, c.primary_container);
  const auto now = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()};
  const draw::TextStyle big{.size = 45, .color = c.primary, .weight = vw::weight::black, .family = font::display};
  draw::text_centered(ui, std::format("{:%H}", now), x + 55, y + 166 - 47, big);
  draw::text_centered(ui, "•••", x + 55, y + 166, {.size = 19, .color = c.secondary});
  draw::text_centered(ui, std::format("{:%M}", now), x + 55, y + 166 + 47, big);
  const auto seconds = std::chrono::floor<std::chrono::seconds>(now.get_local_time()).time_since_epoch().count() % 60;
  draw::text_centered(ui, std::format("{:02} SEC", seconds), x + 55, y + 332 - 18, label(c.text_muted, 8));
}

void calendar(vw::Ui& ui, float x, float y) {
  using namespace std::chrono;
  const Colours& c = colours();
  card(ui, x, y, 417, 332, c.surface_container);
  const auto now = zoned_time{current_zone(), system_clock::now()};
  const year_month_day today{floor<days>(now.get_local_time())};
  const year_month shown = year_month{today.year(), today.month()} + months{shell().calendar_offset};
  const float ix = x + 14;
  const float iy = y + 14;
  const float iw = 417 - 28;
  const auto button = [&](const char* glyph, float bx, int delta) {
    ui.rectangle({.width = 32, .height = 32, .radius = 16, .color = with_alpha(c.surface_container_high, 0),
                  .place = {.x = bx, .y = iy},
                  .input = {.on_click = [delta](const vw::ClickEvent&) { shell().calendar_offset += delta; },
                            .hover = vw::Hover{.color = c.surface_container_high, .duration = 0.12F}}});
    draw::glyph(ui, glyph, bx + 16, iy + 16, 19, c.primary);
  };
  button("chevron_left", ix, -1);
  button("chevron_right", ix + iw - 32, 1);
  draw::text_centered(ui, std::format("{:%B}", shown), ix + iw / 2, iy + 9,
                      {.size = 16, .color = c.text, .weight = vw::weight::semibold, .family = font::display});
  draw::text_centered(ui, std::to_string(static_cast<int>(shown.year())), ix + iw / 2, iy + 26, label(c.text_muted));
  const float cell_width = (iw - 12) / 7;
  const float weekdays_y = iy + 32 + 7;
  constexpr const char* names[] = {"M", "T", "W", "T", "F", "S", "S"};
  for (int day = 0; day < 7; ++day)
    draw::text_centered(ui, names[day], ix + (cell_width + 2) * static_cast<float>(day) + cell_width / 2,
                        weekdays_y + 5, label(c.text_muted, 8));
  const float grid_y = weekdays_y + 10 + 7;
  const float footer_y = y + 332 - 14 - 10;
  const float cell_height = (footer_y - 7 - grid_y - 10) / 6;
  const sys_days first{shown / 1};
  const int lead = static_cast<int>(weekday{first}.iso_encoding()) - 1;
  const int count = static_cast<int>(static_cast<unsigned>((shown / last).day()));
  for (int index = 0; index < 42; ++index) {
    const int day = index - lead + 1;
    const bool in_month = day > 0 && day <= count;
    const bool is_today = in_month && year_month_day{shown / day} == today;
    const bool weekend = index % 7 > 4;
    const float cx = ix + (cell_width + 2) * static_cast<float>(index % 7) + cell_width / 2;
    const float cy = grid_y + (cell_height + 2) * static_cast<float>(index / 7) + cell_height / 2;
    if (is_today) draw::rect(ui, cx - 18, cy - 18, 36, 36, 18, c.primary);
    else if (in_month && weekend) draw::rect(ui, cx - 18, cy - 18, 36, 36, 18, with_alpha(c.secondary, 0.08F));
    if (in_month)
      draw::text_centered(ui, std::to_string(day), cx, cy,
                          {.size = 16, .color = is_today ? c.primary_text : weekend ? c.secondary : c.text,
                           .weight = is_today ? vw::weight::black : vw::weight::regular, .family = font::display});
  }
  draw::text_centered(ui, "MONTH VIEW", ix + iw / 2, footer_y + 5, label(c.text_muted, 8));
}

void resources(vw::Ui& ui, float x, float y) {
  const Colours& c = colours();
  card(ui, x, y, 76, 332, c.surface_container_high);
  draw::text_centered(ui, "RESOURCES", x + 38, y + 15, label(c.text, 8));
  struct Entry {
    const char* label;
    double value;
    vw::Color accent;
    const char* icon;
  };
  const Entry entries[] = {{"CPU", system::cpu(), c.primary, "memory"},
                           {"MEM", system::memory(), c.secondary, "memory_alt"},
                           {"DISK", system::disk(), c.tertiary, "hard_drive"}};
  const float top = y + 32;
  const float slot = (y + 332 - 10 - top - 24) / 3;
  for (int index = 0; index < 3; ++index) {
    const Entry& entry = entries[index];
    const float sy = top + (slot + 12) * static_cast<float>(index) + (slot - 48 - 3 - 15 - 3 - 11) / 2;
    widgets::circular_progress(ui, std::string("resources.") + entry.label, x + 14, sy, 48,
                               {.value = entry.value, .accent = entry.accent, .track = with_alpha(entry.accent, 0.2F),
                                .stroke = 4, .icon = entry.icon});
    draw::text_centered(ui, std::format("{}%", std::lround(entry.value * 100)), x + 38, sy + 58,
                        {.size = 12, .color = entry.accent, .weight = vw::weight::black, .family = font::display});
    draw::text_centered(ui, entry.label, x + 38, sy + 75, label(c.text_muted));
  }
}

void media_card(vw::Ui& ui, float x, float y) {
  const Colours& c = colours();
  constexpr float width = 200;
  constexpr float height = 450;
  card(ui, x, y, width, height, c.surface_container);
  const Player p = player();
  const auto& m = p.data;
  const float ix = x + 14;
  const float iw = width - 28;
  float ty = y + 14;
  draw::text(ui, "NOW PLAYING", ix, ty, label(c.text_muted, 10));
  ty += 13 + 8;
  const float art_x = x + (width - 112) / 2;
  widgets::circular_progress(ui, "media.card", art_x + 1, ty + 1, 110,
                             {.value = m.length > 0 ? m.position / m.length : 0, .accent = c.primary,
                              .track = c.surface_variant, .stroke = 5, .start_angle = -225, .sweep_angle = 270,
                              .active = p.playing, .wavy = p.playing, .show_label = false});
  const std::string art = art::path(m.art_url);
  draw::rect(ui, art_x + 13, ty + 13, 86, 86, 43, c.primary_container);
  if (!draw::art(ui, art, art_x + 13, ty + 13, 86, 43))
    draw::glyph(ui, p.available ? (p.playing ? "graphic_eq" : "music_note") : "music_off", art_x + 56, ty + 56, 34,
                c.primary);
  ty += 112 + 8;
  draw::text(ui, p.available ? (m.title.empty() ? "Untitled track" : m.title) : "No active player", ix, ty,
             {.size = 17, .color = c.text, .weight = vw::weight::black, .family = font::display, .max_width = iw,
              .align = "center"});
  ty += 22 + 8;
  std::string detail = "Start a compatible player to bring this space to life.";
  if (p.available) {
    detail = m.artist.empty() ? "Unknown artist" : m.artist;
    if (!m.album.empty()) detail += "  /  " + m.album;
  }
  draw::text(ui, detail, ix, ty, {.size = 12, .color = c.text_muted, .family = font::display, .max_width = iw,
                                  .max_lines = p.available ? 1 : 2, .align = "center"});

  // From the bottom: waiting label, transport, times, slider.
  float by = y + height - 14;
  if (!p.available) {
    draw::text_centered(ui, "WAITING FOR MPRIS", x + width / 2, by - 5, label(c.text_muted, 8));
    by -= 18;
  }
  const float transport_y = by - 42;
  const float rx = x + (width - (32 + 12 + 42 + 12 + 32)) / 2;
  transport(ui, rx, transport_y + 5, 32, "skip_previous", false, m.can_go_previous, 20,
            [] { vw::mpris::previous(); }, true);
  transport(ui, rx + 44, transport_y, 42, p.playing ? "pause" : "play_arrow", true, m.can_play || m.can_pause, 25,
            [] { vw::mpris::play_pause(); }, true);
  transport(ui, rx + 98, transport_y + 5, 32, "skip_next", false, m.can_go_next, 20, [] { vw::mpris::next(); }, true);
  const float times_y = transport_y - 8 - 12;
  const float faded = p.available && m.length > 0 ? 1 : 0.35F;
  draw::text(ui, m.length > 0 ? clock_text(m.position) : "--:--", ix, times_y,
             label(with_alpha(c.text_muted, faded)));
  const std::string end = m.length > 0 ? clock_text(m.length) : "LIVE";
  draw::text(ui, end, ix + iw - draw::measure(end, 9, vw::weight::semibold, font::display), times_y,
             label(with_alpha(c.text_muted, faded)));
  widgets::material_slider(ui, "media.card", ix, times_y - 24, iw, 20,
                           {.value = m.length > 0 ? m.position / m.length : 0, .track_thickness = 2,
                            .handle_diameter = 14, .endpoint_padding = 3, .wave = p.playing,
                            .track = with_alpha(c.surface_variant, faded), .fill = with_alpha(c.primary, faded),
                            .interactive = p.available && m.length > 0, .released = seek});
}

void overview(vw::Ui& ui, float x, float y) {
  small_weather(ui, x, y);
  user(ui, x + 287, y);
  date_time(ui, x, y + 118);
  calendar(ui, x + 122, y + 118);
  resources(ui, x + 551, y + 118);
  media_card(ui, x + 639, y);
}

// --- Performance ---------------------------------------------------------------

void performance(vw::Ui& ui, float x, float y) {
  const Colours& c = colours();
  struct Metric {
    const char* label;
    double usage;
    vw::Color accent;
    const char* icon;
    const char* context;
    double critical;
  };
  const Metric metrics[] = {{"PROCESSOR", system::cpu(), c.primary, "memory", "CURRENT LOAD", 0.85},
                            {"MEMORY", system::memory(), c.secondary, "memory_alt", "IN USE", 0.9},
                            {"ROOT VOLUME", system::disk(), c.tertiary, "hard_drive", "DISK UTILIZATION", 0.9}};
  const float card_width = (640 - 16) / 3.0F;
  for (int index = 0; index < 3; ++index) {
    const Metric& m = metrics[index];
    const float cx = x + (card_width + 8) * static_cast<float>(index);
    card(ui, cx, y, card_width, 110, with_alpha(m.accent, 0.1F));
    draw::text(ui, m.label, cx + 10, y + 10, label(c.text_muted, 9, font::utility));
    widgets::circular_progress(ui, std::string("performance.") + m.label, cx + card_width - 54, y + 10, 44,
                               {.value = m.usage, .accent = m.accent, .track = with_alpha(m.accent, 0.2F), .stroke = 4,
                                .critical_threshold = m.critical, .critical = c.error, .icon = m.icon});
    const float meter_y = y + 110 - 16;
    draw::text(ui, m.context, cx + 10, meter_y - 18, label(c.text_muted, 8, font::utility));
    draw::text(ui, std::format("{}%", std::lround(m.usage * 100)), cx + 10, meter_y - 52,
               {.size = 24, .color = m.accent, .weight = vw::weight::black, .family = font::utility});
    widgets::progress_bar(ui, cx + 10, meter_y, card_width - 20, 6, m.usage, m.accent, with_alpha(m.accent, 0.2F));
  }
  // Utilization history.
  const float hy = y + 120;
  const float hw = 640 - 168 - 10;
  card(ui, x, hy, hw, 100, c.surface_container);
  draw::text(ui, "UTILIZATION HISTORY", x + 10, hy + 10, label(c.text, 10, font::utility));
  const auto cpu = system::cpu_history();
  const auto memory = system::memory_history();
  const std::size_t samples = std::max(cpu.size(), memory.size());
  draw::text(ui, samples > 1 ? std::format("LAST {} SAMPLES", samples) : "AWAITING SAMPLES", x + 10, hy + 26,
             label(c.text_muted, 8, font::utility));
  const float mem_x = x + hw - 10 - draw::measure("MEM", 8, vw::weight::semibold, font::utility);
  draw::text(ui, "MEM", mem_x, hy + 12, label(c.secondary, 8, font::utility));
  draw::text(ui, "CPU", mem_x - 11 - draw::measure("CPU", 8, vw::weight::semibold, font::utility), hy + 12,
             label(c.primary, 8, font::utility));
  const float graph_y = hy + 43;
  const float graph_height = hy + 90 - graph_y;
  if (samples > 1)
    widgets::usage_graph(ui, "performance", x + 10, graph_y, hw - 20, graph_height, cpu, memory, c.primary,
                         c.secondary, c.surface_container_high);
  else
    draw::text_centered(ui, "COLLECTING UTILIZATION HISTORY", x + hw / 2, graph_y + graph_height / 2,
                        label(c.text_muted, 8));
  // System state.
  const float sx = x + 640 - 168;
  card(ui, sx, hy, 168, 100, c.surface_container);
  draw::text(ui, "SYSTEM STATE", sx + 10, hy + 10, label(c.text, 10, font::utility));
  draw::glyph(ui, "dns", sx + 168 - 18.5F, hy + 16, 17, c.tertiary);
  const std::pair<const char*, std::string> rows[] = {
      {"HOST", upper(system::hostname())}, {"PLATFORM", system::os_name()}, {"KERNEL", system::kernel()}};
  float ry = hy + 33;
  for (const auto& [name, value] : rows) {
    draw::text(ui, name, sx + 10, ry, label(c.text_muted, 8, font::utility));
    draw::text(ui, value, sx + 71, ry, {.size = 9, .color = c.text, .weight = vw::weight::semibold, .family = font::utility,
                                         .max_width = 168 - 81});
    ry += 19;
  }
}

// --- Media page --------------------------------------------------------------------

float& lyrics_scroll() {
  static float offset = 0;
  return offset;
}

void media_page(vw::Ui& ui, float x, float y) {
  const Colours& c = colours();
  const Player p = player();
  const auto& m = p.data;
  constexpr float art_size = 150;
  const float art_x = x + 18;
  const float art_y = y + (280 - art_size) / 2;
  const auto bars = cava::values();
  widgets::radial_bars(ui, "media.page", art_x - 25, art_y - 25, art_size + 50, 73, bars, c.primary,
                       p.playing && !bars.empty() ? 0.9F : 0.2F);
  const std::string art = art::path(m.art_url);
  draw::rect(ui, art_x + 12, art_y + 12, 126, 126, 63, c.primary_container);
  if (!draw::art(ui, art, art_x + 12, art_y + 12, 126, 63))
    draw::glyph(ui, p.available ? (p.playing ? "graphic_eq" : "music_note") : "music_off", art_x + 75, art_y + 75, 58,
                c.primary);

  const float cx = x + 18 + art_size + 18;
  const float cw = 380 - (cx - x) - 18;
  const bool album = p.available && !m.album.empty();
  const float column_height = 13 + 6 + 30 + 6 + 16 + (album ? 19 : 0) + 6 + 5 + 6 + 20 + 6 + 13 + 6 + 58;
  float ty = y + (280 - column_height) / 2;
  draw::text(ui, p.available ? (p.playing ? "NOW PLAYING" : "PAUSED") : "MEDIA OFFLINE", cx, ty,
             label(p.available ? c.text_muted : c.error, 10, font::utility));
  ty += 19;
  widgets::marquee(ui, "media.title", p.available ? (m.title.empty() ? "Untitled track" : m.title) : "No player connected",
                   cx, ty, cw, 30, 22, c.text, vw::weight::black, font::utility);
  ty += 36;
  draw::text(ui, p.available ? (m.artist.empty() ? "Unknown artist" : m.artist)
                             : "Launch an MPRIS-compatible music, podcast, or video player.",
             cx, ty, {.size = 13, .color = c.text, .weight = vw::weight::semibold, .family = font::utility, .max_width = cw});
  ty += 22;
  if (album) {
    draw::text(ui, m.album, cx, ty, {.size = 11, .color = c.text_muted, .family = font::utility, .max_width = cw});
    ty += 19;
  }
  ty += 11;
  const float faded = m.length > 0 ? 1 : 0.32F;
  widgets::material_slider(ui, "media.page", cx, ty, cw, 20,
                           {.value = m.length > 0 ? m.position / m.length : 0, .track_thickness = 8,
                            .handle_diameter = 25, .handle_width = 12, .handle_progress_gap = 12,
                            .endpoint_padding = 8, .wave = p.playing, .track = with_alpha(c.surface_variant, faded),
                            .fill = with_alpha(c.primary, faded), .interactive = p.available && m.length > 0,
                            .released = seek});
  ty += 26;
  draw::text(ui, m.length > 0 ? clock_text(m.position) : "--:--", cx, ty, label(c.text_muted, 10, font::utility));
  const std::string end = m.length > 0 ? clock_text(m.length) : "LIVE";
  draw::text(ui, end, cx + cw - draw::measure(end, 10, vw::weight::semibold, font::utility), ty,
             label(c.text_muted, 10, font::utility));
  ty += 19;
  transport(ui, cx, ty + 7, 44, "skip_previous", false, m.can_go_previous, 25, [] { vw::mpris::previous(); }, false);
  transport(ui, cx + 58, ty, 58, p.playing ? "pause" : "play_arrow", true, m.can_play || m.can_pause, 32,
            [] { vw::mpris::play_pause(); }, false);
  transport(ui, cx + 130, ty + 7, 44, "skip_next", false, m.can_go_next, 25, [] { vw::mpris::next(); }, false);

  // Lyrics.
  const float lx = x + 392;
  const float lw = 640 - 392;
  draw::rect(ui, lx + 16, y + 16, 30, 30, 15, c.primary_container);
  draw::glyph(ui, "lyrics", lx + 31, y + 31, 18, c.primary);
  draw::text_middle(ui, "LYRICS", lx + 54, y + 31, label(c.text, 10, font::utility));
  const auto lyrics = lyrics::current();
  if (lyrics) {
    const std::string badge = (lyrics->synced.empty() ? std::string() : "SYNCED / ") + upper(lyrics->backend);
    const float bw = draw::measure(badge, 8, vw::weight::semibold, font::utility) + 16;
    draw::rect(ui, lx + lw - 16 - bw, y + 19, bw, 24, 12, c.primary_container);
    draw::text_middle(ui, badge, lx + lw - 16 - bw + 8, y + 31, label(c.primary, 8, font::utility));
  }
  const float vx = lx + 16;
  const float vy = y + 58;
  const float vwid = lw - 32;
  const float vh = y + 280 - 16 - vy;
  const vw::Clip clip{.x = vx, .y = vy, .width = vwid, .height = vh};
  if (lyrics && !lyrics->synced.empty()) {
    const int active = std::max(0, lyrics::line_at(*lyrics, m.position));
    const float column_y = static_cast<float>(vw::animate(
        "lyrics.offset", vh / 2 - (static_cast<float>(active) + 0.5F) * 42, {.duration = 0.3, .easing = vw::Easing::out_cubic}));
    for (std::size_t index = 0; index < lyrics->synced.size(); ++index) {
      const float ly = vy + column_y + static_cast<float>(index) * 42;
      if (ly + 42 < vy || ly > vy + vh) continue;
      const int distance = std::abs(static_cast<int>(index) - active);
      const bool current = distance == 0;
      const float alpha = current ? 1 : distance == 1 ? 0.58F : distance == 2 ? 0.3F : 0.12F;
      const auto widget = ui.text({.text = lyrics->synced[index].text, .font = font::utility, .size = current ? 17.0F : 12.0F,
                                   .weight = current ? vw::weight::black : vw::weight::medium,
                                   .color = with_alpha(current ? c.primary : c.text_muted, alpha),
                                   .max_width = vwid - 8, .max_lines = 2, .align = "center",
                                   .place = {.x = vx + 4, .y = ly, .clip = clip}});
      ui.move(widget, 0, (42 - widget.height) / 2);
      ui.hit_area({.width = vwid, .height = 42, .place = {.x = vx, .y = ly, .clip = clip},
                   .input = {.on_click = [time = lyrics->synced[index].time](const vw::ClickEvent&) {
                     vw::mpris::set_position(time);
                   }}});
    }
  } else if (lyrics && !lyrics->plain.empty()) {
    float& offset = lyrics_scroll();
    ui.hit_area({.width = vwid, .height = vh, .place = {.x = vx, .y = vy},
                 .input = {.on_scroll = [](const vw::ScrollEvent& e) {
                   lyrics_scroll() = std::max(0.0F, lyrics_scroll() + static_cast<float>(e.delta));
                 }}});
    const auto widget = ui.text({.text = lyrics->plain, .font = font::utility, .size = 13, .color = c.text,
                                 .max_width = vwid, .max_lines = 0,
                                 .place = {.x = vx, .y = vy - 2 - offset, .clip = clip}});
    offset = std::min(offset, std::max(0.0F, widget.height - vh));
  } else if (lyrics::loading()) {
    static int shape_index = 0;
    static auto last = Clock::now();
    if (Clock::now() - last > std::chrono::milliseconds(420)) {
      last = Clock::now();
      shape_index = (shape_index + 1) % 6;
    }
    constexpr const char* loader_shapes[] = {"circle", "gem", "sunny", "flower", "softburst", "cookie6sided"};
    const float lcx = vx + vwid / 2;
    const float lcy = y + 140;
    widgets::circular_progress(ui, "lyrics.loader", lcx - 33, lcy - 33, 66,
                               {.value = 0.78, .accent = c.primary, .track = c.surface_variant, .stroke = 4,
                                .start_angle = -220, .sweep_angle = 280, .wavy = true, .indeterminate = true,
                                .show_label = false});
    shapes::draw(ui, "lyrics.loader", loader_shapes[shape_index], lcx - 20, lcy - 20, 40, c.primary_container, 1,
                 c.primary, 0.34, 48);
    draw::text_centered(ui, "SEARCHING LYRICS", lcx, lcy + 49, label(c.text_muted, 9, font::utility));
    vw::request_animation_continuation();
  } else {
    const std::string error = lyrics::error();
    draw::text(ui, !error.empty() ? error : p.available ? "Lyrics will appear here" : "Start a player to load lyrics",
               lx + 16, y + 132, {.size = 11, .color = error.empty() ? c.text_muted : c.error,
                                  .weight = vw::weight::semibold, .family = font::utility, .max_width = lw - 32,
                                  .align = "center"});
  }
  if (p.playing) vw::request_animation_continuation();
}

// --- Weather page ------------------------------------------------------------------

void detail_card(vw::Ui& ui, float x, float y, float width, float height, const char* icon, const char* name,
                 const std::string& value, vw::Color accent) {
  const Colours& c = colours();
  card(ui, x, y, width, height, with_alpha(accent, 0.09F));
  draw::glyph(ui, icon, x + 27.5F, y + height / 2, 27, accent);
  draw::text(ui, name, x + 53, y + height / 2 - 14, label(c.text_muted, 9, font::utility));
  draw::text(ui, value, x + 53, y + height / 2 - 1,
             {.size = 14, .color = c.text, .weight = vw::weight::semibold, .family = font::utility});
}

void weather_page(vw::Ui& ui, float x, float y, float width) {
  const Colours& c = colours();
  const auto report = weather::current();
  const bool loading = weather::loading();
  draw::text(ui, report ? report->location : "WEATHER", x, y,
             {.size = 22, .color = c.text, .weight = vw::weight::semibold, .family = font::utility});
  const auto now = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()};
  const auto today = std::chrono::year_month_day{std::chrono::floor<std::chrono::days>(now.get_local_time())};
  draw::text(ui, std::format("{:%A, %B} {}", now, static_cast<unsigned>(today.day())), x, y + 27,
             label(c.text_muted, 10, font::utility));
  detail_card(ui, x + width - 264, y - 4, 125, 46, "wb_twilight", "SUNRISE", report ? report->sunrise : "--:--", c.tertiary);
  detail_card(ui, x + width - 125, y - 4, 125, 46, "bedtime", "SUNSET", report ? report->sunset : "--:--", c.tertiary);
  const float cy = y + 52;
  card(ui, x, cy, width, 112, c.surface_container);
  draw::glyph(ui, loading && !report ? "sync" : report ? report->glyph : "cloud_off", x + 54, cy + 56, 72, c.secondary);
  draw::text(ui, report ? report->temperature : loading ? "--" : "Unavailable", x + 118, cy + 18,
             {.size = 48, .color = c.primary, .weight = vw::weight::medium, .family = font::utility});
  const std::string error = weather::error();
  draw::text(ui, report ? report->condition : (!error.empty() ? error : "Contacting weather service"), x + 118,
             cy + 74, label(c.text_muted, 13, font::utility));
  if (report && report->updated != std::chrono::system_clock::time_point{}) {
    const auto updated = std::chrono::zoned_time{std::chrono::current_zone(),
                                                 std::chrono::floor<std::chrono::minutes>(report->updated)};
    const auto local = updated.get_local_time();
    const auto time = std::chrono::hh_mm_ss{local - std::chrono::floor<std::chrono::days>(local)};
    const int hour = static_cast<int>(time.hours().count());
    const std::string text = std::format("UPDATED {}:{:02} {}", hour % 12 == 0 ? 12 : hour % 12,
                                         time.minutes().count(), hour >= 12 ? "PM" : "AM");
    draw::text(ui, text, x + width - 18 - draw::measure(text, 9, vw::weight::semibold, font::utility), cy + 83,
               label(c.text_muted, 9, font::utility));
  }
  const float dy = cy + 124;
  const float dw = (width - 20) / 3;
  detail_card(ui, x, dy, dw, 58, "water_drop", "HUMIDITY", report ? report->humidity : "--", c.secondary);
  detail_card(ui, x + dw + 10, dy, dw, 58, "thermostat", "FEELS LIKE", report ? report->feels_like : "--", c.primary);
  detail_card(ui, x + (dw + 10) * 2, dy, dw, 58, "air", "WIND", report ? report->wind : "--", c.tertiary);
  const float fy = dy + 72;
  draw::text(ui, "FORECAST", x, fy, label(c.text, 10, font::utility));
  if (!report) return;
  const int columns = std::clamp(static_cast<int>(width / 105), 3, 7);
  const float cell = (width - 10 * static_cast<float>(columns - 1)) / static_cast<float>(columns);
  for (std::size_t index = 0; index < report->forecast.size(); ++index) {
    const auto& day = report->forecast[index];
    const float fx = x + (cell + 10) * static_cast<float>(index % static_cast<std::size_t>(columns));
    const float fyy = fy + 21 + 76 * static_cast<float>(index / static_cast<std::size_t>(columns));
    card(ui, fx, fyy, cell, 66, c.surface_container);
    std::string name = "TODAY";
    int year = 0;
    unsigned month = 0;
    unsigned dom = 0;
    if (index > 0 && std::sscanf(day.date.c_str(), "%d-%u-%u", &year, &month, &dom) == 3) {
      const std::chrono::sys_days date =
          std::chrono::year_month_day{std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{dom}};
      name = upper(std::format("{:%a}", std::chrono::weekday{date}));
    }
    draw::text(ui, name, fx + 10, fyy + 10, label(c.text_muted, 9, font::utility));
    draw::text(ui, day.min + " / " + day.max, fx + 10, fyy + 42, label(c.tertiary, 11, font::utility));
    draw::glyph(ui, day.glyph, fx + cell - 24.5F, fyy + 33, 29, c.secondary);
  }
}

}  // namespace

Panel dashboard_panel(const Screen& screen) {
  available_width = screen.width - frame_thickness * 2;
  Shell& state = shell();
  // Popout.qml: offsetScale 1 -> 0 over 280 ms, OutCubic.
  const float offset = static_cast<float>(vw::animate("dashboard.offset", state.dashboard_hovered ? 0 : 1,
                                                      {.duration = 0.28, .easing = vw::Easing::out_cubic}));
  if (offset > 0.999F) return {};
  const Size size = dashboard_size(state.dashboard_page);
  const float width = static_cast<float>(vw::animate("dashboard.width", size.width, motion::bounce()));
  const float height = static_cast<float>(vw::animate("dashboard.height", size.height, motion::bounce()));
  Panel panel;
  panel.reveal = 1 - offset;
  panel.box = {(screen.width - width) / 2, (-height - 5) * offset, width, height, blob::Radii::all(frame_radius)};
  return panel;
}

void dashboard_trigger(vw::Ui& ui, const Screen& screen, const Panel& panel) {
  if (!config().dashboard_enabled) {
    shell().dashboard_hovered = false;
    return;
  }
  // Each opening can start on the chosen tab.
  static bool was_open = false;
  if (panel.visible() && !was_open && !config().dashboard_remember_tab)
    shell().dashboard_page = std::clamp(config().dashboard_tab, 0, 3);
  was_open = panel.visible();
  const float width = panel.visible() ? panel.box.width : dashboard_size(shell().dashboard_page).width;
  const float left = (screen.width - width) / 2 - frame_radius;
  const float bottom = panel.visible() ? std::max(frame_thickness, frame_thickness + panel.box.height * panel.reveal)
                                       : frame_thickness;
  track_hover(ui, {left, 0, width + frame_radius * 2, bottom}, [](bool inside) { shell().dashboard_hovered = inside; });
}

void draw_dashboard(vw::Ui& ui, const Panel& panel) {
  if (!panel.visible()) return;
  const Colours& c = colours();
  const blob::Box& box = panel.box;
  Shell& state = shell();

  const float tabs = tabs_width();
  const float tx = box.x + (box.width - tabs) / 2;
  const float ty = box.y + 14;
  draw::rect(ui, tx, ty, tabs, 44, 22, c.surface_container_high);
  float dx = tx + 4;
  for (int index = 0; index < 4; ++index) {
    const Destination& destination = destinations[index];
    const float dw = tab_width(destination);
    const bool selected = state.dashboard_page == index;
    if (selected) draw::rect(ui, dx, ty + 4, dw, 36, 18, c.primary_container);
    draw::glyph(ui, destination.icon, dx + 22, ty + 22, 18, selected ? c.primary : c.text_muted);
    draw::text_middle(ui, destination.label, dx + 38, ty + 22, label(selected ? c.text : c.text_muted));
    draw::state_layer(ui, dx, ty + 4, dw, 36, 18, selected ? c.primary : c.text,
                      {.on_click = [index](const vw::ClickEvent&) { shell().dashboard_page = index; },
                       .hover = vw::Hover{.color = with_alpha(selected ? c.primary : c.text, 0.07F), .duration = 0.12F}});
    dx += dw + 2;
  }

  const Size target = dashboard_size(state.dashboard_page);
  if (std::abs(box.width - target.width) > 6 || std::abs(box.height - target.height) > 6) return;
  const Size page = page_size(state.dashboard_page);
  const float x = box.x + 24;
  const float y = box.y + content_top;
  switch (state.dashboard_page) {
    case 1: performance(ui, x, y); break;
    case 2: media_page(ui, x, y); break;
    case 3: weather_page(ui, x, y, page.width); break;
    default: overview(ui, x, y); break;
  }
}

}  // namespace blx
