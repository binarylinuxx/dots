// Side popouts, after ~/shell's PowerMenu, StatusPopout, TrayActions,
// AudioOsd and Notifications. Each slides out of the frame edge it belongs to.
#include <vw/vw.hpp>

#include <algorithm>
#include <functional>
#include <filesystem>
#include <chrono>
#include <cmath>
#include <ctime>
#include <format>
#include <string>
#include <vector>

#include "config.hpp"
#include "draw.hpp"
#include "parts.hpp"
#include "state.hpp"
#include "widgets.hpp"

namespace blx {
namespace {

const vw::AnimationOptions quick{.duration = 0.18, .easing = vw::Easing::out_cubic};
const vw::AnimationOptions bounce{.duration = 0.46, .easing = vw::Easing::out_back};

// --- Power menu --------------------------------------------------------------

struct PowerAction {
  const char* glyph;
  const char* label;
  int role;  // 0 primary, 1 secondary, 2 error
  std::vector<std::string> command;
  // Runs in-process instead of command when set.
  std::function<void()> run;
};

std::vector<PowerAction> power_actions() {
  const Config& c = config();
  std::vector<PowerAction> actions;
  const auto add = [&actions](bool shown, const char* glyph, const char* label, int role, const std::string& custom,
                              std::vector<std::string> fallback) {
    if (!shown) return;
    actions.push_back({glyph, label, role, custom.empty() ? std::move(fallback) : std::vector<std::string>{"sh", "-c", custom}});
  };
  add(c.power_lock, "lock", "Lock", 0, c.power_lock_command, {});
  // Locking is the shell's own job: no need to round-trip through a process.
  if (c.power_lock && c.power_lock_command.empty()) actions.back().run = [] { lock_session(); };
  add(c.power_logout, "logout", "Log out", 1, c.power_logout_command, {"loginctl", "terminate-session", "self"});
  add(c.power_suspend, "bedtime", "Suspend", 0, c.power_suspend_command, {"systemctl", "suspend"});
  add(c.power_hibernate, "bed", "Hibernate", 0, c.power_hibernate_command, {"systemctl", "hibernate"});
  add(c.power_restart, "restart_alt", "Restart", 1, c.power_restart_command, {"systemctl", "reboot"});
  add(c.power_firmware, "developer_board", "Firmware setup", 1, c.power_firmware_command, {"systemctl", "reboot", "--firmware-setup"});
  add(c.power_off, "power_settings_new", "Power off", 2, c.power_off_command, {"systemctl", "poweroff"});
  return actions;
}

float power_button(const Screen& screen) {
  const Config& c = config();
  return std::clamp(screen.height * c.power_menu_height_ratio, static_cast<float>(c.power_menu_minimum_button_size),
                    static_cast<float>(c.power_menu_maximum_button_size));
}

int& hovered_power_action() {
  static int index = -1;
  return index;
}

// --- Status popout (StatusPopout.qml) ------------------------------------------

float status_width(const std::string& kind) { return kind == "wifi" || kind == "audio" ? 260 : 184; }

float status_height(const std::string& kind) {
  // max(minimumHeight, infoColumn + padding): the minimum always wins.
  return kind == "audio" ? 164 : kind == "wifi" ? 218 : kind == "bluetooth" ? 80 : kind == "battery" ? 96 : 72;
}

std::string signal_glyph(int strength) {
  return strength < 30 ? "network_wifi_1_bar" : strength < 60 ? "network_wifi_2_bar" : "wifi";
}

struct StatusText {
  std::string icon;
  std::string title;
  std::string detail;
  std::string action;  // empty: no action button
  std::function<void()> run;
};

StatusText status_text(const std::string& kind) {
  if (kind == "wifi") {
    const auto status = vw::network::status();
    const bool wired = status.primary == vw::network::Kind::wired;
    const bool wifi = status.primary == vw::network::Kind::wifi;
    return {wired ? "cable" : wifi ? "wifi" : "wifi_off", "Network",
            wired ? "Wired connection" : wifi ? std::format("{}% signal", status.strength) : "Disconnected",
            wifi ? "wifi_off" : "wifi",
            [on = status.wifi_enabled] { vw::network::set_wifi_enabled(!on); }};
  }
  if (kind == "bluetooth") {
    const auto adapter = vw::bluetooth::adapter();
    const bool on = adapter && adapter->powered;
    long connected = 0;
    for (const auto& device : vw::bluetooth::devices()) connected += device.connected ? 1 : 0;
    return {on ? "bluetooth" : "bluetooth_disabled", "Bluetooth",
            !on ? "Disabled"
                : connected > 0 ? std::format("{} device{} connected", connected, connected == 1 ? "" : "s")
                                : "Ready to connect",
            on ? "bluetooth_disabled" : "bluetooth", [on] { vw::bluetooth::set_powered(!on); }};
  }
  if (kind == "audio") {
    const auto sink = vw::pipewire::default_sink();
    const bool muted = sink && sink->muted;
    return {muted ? "volume_off" : "volume_up", "Audio output",
            muted ? "Muted" : std::format("{}% volume", std::lround((sink ? sink->volume() : 0) * 100)),
            muted ? "volume_up" : "volume_off",
            [id = sink ? sink->id : 0U] { if (id != 0) vw::pipewire::toggle_mute(id); }};
  }
  const auto battery = vw::upower::display_device();
  const auto profile = vw::upower::profile();
  std::string icon = "balance";
  if (battery) icon = battery->percentage < 20 ? "battery_alert" : "battery_full";
  else if (profile == "power-saver") icon = "energy_savings_leaf";
  else if (profile == "performance") icon = "rocket_launch";
  const std::string name = profile == "power-saver" ? "PowerSaver" : profile == "performance" ? "Performance" : "Balanced";
  return {icon, battery ? "Battery" : "Power profile",
          battery ? std::format("{}% remaining", std::lround(battery->percentage)) : "Profile: " + name, "", {}};
}

// Draws the popout's contents with its top-left at (x, y).
void status_content(vw::Ui& ui, const std::string& kind, float x, float y) {
  const Colours& c = colours();
  const float width = status_width(kind);
  const float height = status_height(kind);
  const StatusText text = status_text(kind);
  const float offset = kind == "wifi" ? -66 : kind == "audio" ? -52 : kind == "battery" ? -25 : 0;
  const float centre = y + height / 2 + offset;
  draw::glyph(ui, text.icon, x + 14 + 12, centre, 24, c.primary);
  draw::text(ui, text.title, x + 50, centre - 15,
             {.size = 12, .color = c.text, .weight = vw::weight::semibold, .family = font::display,
              .max_width = width - 98});
  draw::text(ui, text.detail, x + 50, centre + 2,
             {.size = 10, .color = c.text_muted, .family = font::display, .max_width = width - 98, .max_lines = 2});
  if (kind != "battery") {
    const float bx = x + width - 10 - 28;
    const float by = (kind == "wifi" || kind == "audio" ? centre : y + height / 2) - 14;
    ui.rectangle({.width = 28, .height = 28, .radius = 14, .color = c.surface_container,
                  .place = {.x = bx, .y = by},
                  .input = {.on_click = [run = text.run](const vw::ClickEvent&) { if (run) run(); },
                            .hover = vw::Hover{.color = c.surface_container_high, .duration = 0.12F}}});
    draw::glyph(ui, text.action, bx + 14, by + 14, 17, c.primary);
  }

  if (kind == "audio") {
    const auto sink = vw::pipewire::default_sink();
    const double level = sink ? sink->volume() : 0;
    draw::text(ui, std::format("Volume {}%", std::lround(level * 100)), x + 14, y + 57,
               {.size = 10, .color = c.text_muted, .family = font::display});
    widgets::material_slider(ui, "status.volume", x + 14, y + 70, width - 28, 20,
                             {.value = level, .track_thickness = 7, .handle_diameter = 19, .handle_width = 4,
                              .handle_progress_gap = 7, .endpoint_padding = 5, .track = c.secondary_container,
                              .fill = sink && sink->muted ? c.outline : c.primary,
                              .moved = [id = sink ? sink->id : 0U](double value) {
                                if (id != 0) vw::pipewire::set_volume(id, value);
                              }});
    const auto outputs = vw::pipewire::nodes(vw::pipewire::Filter::sinks);
    const float list_y = y + 94;
    const float list_height = y + height - 8 - list_y;
    ui.listview({.id = "status-outputs", .width = width - 20, .height = list_height, .item_height = 27, .gap = 3,
                 .count = outputs.size(),
                 .render = [&](std::size_t index) -> std::optional<vw::Widget> {
                   const auto& node = outputs[index];
                   const bool active = sink && node.id == sink->id;
                   return draw::capture(ui, width - 20, 27, [&] {
                     ui.rectangle({.width = width - 20, .height = 27, .radius = 8,
                                   .color = active ? c.primary_container : with_alpha(c.surface_container_high, 0),
                                   .input = {.on_click = [id = node.id](const vw::ClickEvent&) {
                                               vw::pipewire::set_default_sink(id);
                                             },
                                             .hover = active ? std::nullopt
                                                             : std::optional(vw::Hover{.color = c.surface_container_high,
                                                                                       .duration = 0.12F})}});
                     draw::glyph(ui, active ? "volume_up" : "speaker", 16, 13.5F, 16, active ? c.primary : c.text_muted);
                     draw::text_middle(ui, node.description.empty() ? (node.name.empty() ? "Audio output" : node.name)
                                                                    : node.description,
                                       31, 13.5F, {.size = 10, .color = c.text, .family = font::display,
                                                   .max_width = width - 20 - 39});
                   });
                 },
                 .place = {.x = x + 10, .y = list_y}});
  } else if (kind == "wifi") {
    draw::rect(ui, x + 14, y + 64, width - 28, 1, 0, c.surface_container_high);
    static bool scanning = false;
    static Clock::time_point scan_started{};
    if (scanning && Clock::now() - scan_started > std::chrono::seconds(4)) scanning = false;
    draw::text(ui, scanning ? "Scanning networks..." : "Available networks", x + 14, y + 76,
               {.size = 10, .color = c.text_muted, .family = font::display});
    const float rx = x + width - 12 - 78;
    ui.rectangle({.width = 78, .height = 24, .radius = 12, .color = c.surface_container, .place = {.x = rx, .y = y + 70},
                  .input = {.on_click = [](const vw::ClickEvent&) {
                              scanning = vw::network::scan();
                              scan_started = Clock::now();
                            },
                            .hover = vw::Hover{.color = c.surface_container_high, .duration = 0.12F}}});
    const float label_width = 15 + 3 + draw::measure("Reload", 10, vw::weight::regular, font::display);
    draw::glyph(ui, "refresh", rx + (78 - label_width) / 2 + 7.5F, y + 82, 15, c.primary);
    draw::text_middle(ui, "Reload", rx + (78 - label_width) / 2 + 18, y + 82,
                      {.size = 10, .color = c.primary, .family = font::display});
    const auto points = vw::network::access_points();
    const float list_y = y + 102;
    const float list_height = y + height - list_y;
    if (points.empty()) {
      if (!scanning)
        draw::text_centered(ui, "No Wi-Fi networks found", x + width / 2, list_y + 12,
                            {.size = 11, .color = c.text_muted, .family = font::display});
      return;
    }
    ui.listview({.id = "status-networks", .width = width - 20, .height = list_height, .item_height = 31, .gap = 3,
                 .count = points.size(),
                 .render = [&](std::size_t index) -> std::optional<vw::Widget> {
                   const auto& point = points[index];
                   return draw::capture(ui, width - 20, 31, [&] {
                     ui.rectangle({.width = width - 20, .height = 31, .radius = 8,
                                   .color = point.active ? c.primary_container : with_alpha(c.surface_container_high, 0),
                                   .input = {.on_click = [point](const vw::ClickEvent&) {
                                               if (point.active) vw::network::disconnect_wifi();
                                               else vw::network::connect(point.ssid);
                                             },
                                             .hover = point.active ? std::nullopt
                                                                   : std::optional(vw::Hover{.color = c.surface_container_high,
                                                                                             .duration = 0.12F})}});
                     draw::glyph(ui, point.active ? "wifi" : signal_glyph(point.strength), 16.5F, 15.5F, 17,
                                 point.active ? c.primary : c.text_muted);
                     draw::text_middle(ui, point.ssid, 33, 15.5F,
                                       {.size = 11, .color = c.text, .family = font::display,
                                        .max_width = width - 20 - 33 - 29});
                     draw::glyph(ui, point.active ? "link_off" : point.secured ? "lock" : "link", width - 20 - 16, 15.5F,
                                 16, c.primary);
                   });
                 },
                 .place = {.x = x + 10, .y = list_y}});
  } else if (kind == "battery") {
    const auto profile = vw::upower::profile();
    const int active = profile == "power-saver" ? 0 : profile == "performance" ? 2 : 1;
    const float px = x + (width - 118) / 2;
    const float py = y + height - 10 - 40;
    draw::rect(ui, px, py, 118, 40, 20, c.surface_container);
    const float pill_x = px + 3 + 38.0F * static_cast<float>(vw::animate("status.profile", active,
                                                        {.duration = 0.22, .easing = vw::Easing::out_cubic}));
    draw::rect(ui, pill_x, py + 3, 35, 34, 17, c.primary);
    constexpr std::pair<const char*, const char*> profiles[] = {
        {"power-saver", "energy_savings_leaf"}, {"balanced", "balance"}, {"performance", "rocket_launch"}};
    for (int index = 0; index < 3; ++index) {
      const float cx = px + 3 + index * 38.0F;
      draw::glyph(ui, profiles[index].second, cx + 17.5F, py + 20, 22, index == active ? c.primary_text : c.text_muted);
      ui.hit_area({.width = 35, .height = 34, .place = {.x = cx, .y = py + 3},
                   .input = {.on_click = [name = profiles[index].first](const vw::ClickEvent&) {
                     vw::upower::set_profile(name);
                   }}});
    }
  }
}

}  // namespace

// --- Power menu ----------------------------------------------------------------

Panel power_panel(const Screen& screen) {
  Shell& state = shell();
  const bool open = state.power_pinned || state.power_hovered;
  // PowerMenu.qml: reveal and slideOffset over 180 ms OutCubic.
  const float reveal = static_cast<float>(vw::animate("power.reveal", open ? 1 : 0, quick));
  const float slide = static_cast<float>(vw::animate("power.slide", open ? 0 : 32, quick));
  if (reveal < 0.002F) {
    state.power_hovered = false;
    hovered_power_action() = -1;
    return {};
  }
  if (power_actions().empty()) return {};
  const float button = power_button(screen);
  const float width = button + 12;
  const float height = static_cast<float>(power_actions().size()) * (button + 2) - 2 + 12;
  const float x = screen.width - props::frame_thickness - width + props::popout_frame_overlap;
  const float top = screen.height - props::bar_height - height * reveal - slide;
  Panel panel;
  panel.reveal = reveal;
  // Rises from the bar in the corner: bottom and right corners stay behind it.
  panel.box = {x + width * (1 - reveal), top, (width + props::popout_radius) * reveal,
               (height + props::popout_radius) * reveal + slide, blob::Radii::all(props::popout_radius)};
  return panel;
}

void draw_power(vw::Ui& ui, const Panel& panel) {
  if (!panel.visible()) return;
  track_hover(ui, panel.box, [](bool inside) { shell().power_hovered = inside; });
  if (panel.reveal < 0.9F) return;
  const Colours& c = colours();
  const auto actions = power_actions();
  const float button = (panel.box.height - props::popout_radius - 12 + 2) / static_cast<float>(actions.size()) - 2;
  const float x = panel.box.x + 6;
  const float width = panel.box.width - props::popout_radius - 12;
  float y = panel.box.y + 6;
  for (std::size_t index = 0; index < actions.size(); ++index) {
    const auto& action = actions[index];
    const vw::Color tint = action.role == 0 ? c.primary : action.role == 1 ? c.secondary : c.error;
    const bool hovered = hovered_power_action() == static_cast<int>(index);
    // Hover rounds the button into a pill (180 ms OutCubic).
    const float side = button - 4;
    const float radius = static_cast<float>(vw::animate(std::format("power.radius.{}", index),
                                                        hovered ? std::min(width - 4, side) / 2 : 20, quick));
    ui.rectangle({.width = width - 4, .height = side, .radius = radius,
                  .color = hovered ? c.surface_container_high : c.surface_container,
                  .place = {.x = x + 2, .y = y + 2},
                  .input = {.on_click = [command = action.command, run = action.run](const vw::ClickEvent&) {
                              shell().power_pinned = false;
                              if (run) run();
                              else vw::spawn(command);
                            },
                            .on_hover = [index](bool inside) {
                              if (inside) hovered_power_action() = static_cast<int>(index);
                              else if (hovered_power_action() == static_cast<int>(index)) hovered_power_action() = -1;
                            }}});
    draw::glyph(ui, action.glyph, x + width / 2, y + button / 2, button * 0.56F, tint);
    y += button + 2;
  }
}

// --- Status popout -------------------------------------------------------------

Panel status_panel(const Screen& screen) {
  HoverPopout& status = shell().status;
  const std::string kind = status.shown();
  static std::string displayed;
  if (!kind.empty() && kind != displayed) {
    displayed = kind;
    if (kind == "wifi") vw::network::scan();
  }
  const float reveal = static_cast<float>(vw::animate("status.reveal", kind.empty() ? 0 : 1,
                                                      {.duration = 0.18, .easing = vw::Easing::out_cubic}));
  if (status.pending()) vw::request_animation_continuation();
  if (reveal < 0.002F || displayed.empty()) {
    displayed.clear();
    return {};
  }
  const float width = static_cast<float>(vw::animate("status.width", status_width(displayed), motion::bounce()));
  const float height = static_cast<float>(vw::animate("status.height", status_height(displayed), motion::bounce()));
  const float slide = static_cast<float>(vw::animate("status.slide", kind.empty() ? 18 : 0,
                                                     {.duration = 0.18, .easing = vw::Easing::out_cubic}));
  // Rises from the bar over its icon; the bottom corners stay behind the bar.
  const float x = std::clamp(status.anchor - width / 2, props::frame_thickness + 4,
                             std::max(props::frame_thickness + 4, screen.width - props::frame_thickness - 4 - width));
  Panel panel;
  panel.reveal = reveal;
  panel.box = {x, screen.height - props::bar_height - height * reveal + slide, width * reveal,
               (height + props::popout_radius) * reveal, blob::Radii::all(props::popout_radius)};
  return panel;
}

void draw_status(vw::Ui& ui, const Panel& panel) {
  if (!panel.visible()) return;
  track_hover(ui, panel.box, [](bool inside) {
    if (inside) shell().status.popout_hovered = true;
    else shell().status.leave_popout();
  });
  if (panel.reveal < 0.9F) return;
  HoverPopout& status = shell().status;
  const std::string& kind = status.pinned.empty() ? status.kind : status.pinned;
  if (kind.empty()) return;
  if (std::abs(panel.box.width - status_width(kind)) > 4 || std::abs(panel.box.height - props::popout_radius - status_height(kind)) > 4) return;
  status_content(ui, kind, panel.box.x, panel.box.y);
}

// --- Tray actions --------------------------------------------------------------

namespace {

float tray_menu_height(const std::vector<vw::tray::MenuEntry>& entries) {
  float height = 12;
  int count = 0;
  for (const auto& entry : entries) {
    if (!entry.visible) continue;
    height += (entry.separator ? 1 : 28) + (count++ > 0 ? 2 : 0);
  }
  return height;
}

}  // namespace

Panel tray_panel(const Screen& screen) {
  HoverPopout& tray = shell().tray;
  const std::string id = tray.shown();
  static std::string displayed;
  if (!id.empty()) displayed = id;
  const float reveal = static_cast<float>(vw::animate(
      "tray.reveal", id.empty() ? 0 : 1, {.duration = 0.28, .easing = vw::Easing::out_cubic}));
  if (tray.pending()) vw::request_animation_continuation();
  if (reveal < 0.002F || displayed.empty()) {
    displayed.clear();
    return {};
  }
  const auto entries = vw::tray::menu(displayed);
  if (entries.empty()) return {};
  constexpr float width = 190;
  const float height = static_cast<float>(vw::animate("tray.height", tray_menu_height(entries), bounce));
  // Rises from the bar over its icon.
  const float x = std::clamp(tray.anchor - width / 2, props::frame_thickness + 4,
                             std::max(props::frame_thickness + 4, screen.width - props::frame_thickness - 4 - width));
  Panel panel;
  panel.reveal = reveal;
  panel.box = {x, screen.height - props::bar_height - height * reveal, width, (height + props::popout_radius) * reveal,
               blob::Radii::all(props::popout_radius)};
  return panel;
}

void draw_tray_actions(vw::Ui& ui, const Panel& panel) {
  if (!panel.visible()) return;
  track_hover(ui, panel.box, [](bool inside) {
    if (inside) shell().tray.popout_hovered = true;
    else shell().tray.leave_popout();
  });
  if (panel.reveal < 0.95F) return;
  const std::string id = shell().tray.pinned.empty() ? shell().tray.kind : shell().tray.pinned;
  const auto entries = vw::tray::menu(id);
  const Colours& c = colours();
  // TrayActions.qml: a column 6 px in from the panel's edges, centred.
  const float x = panel.box.x + 6;
  const float width = panel.box.width - 12;
  float y = panel.box.y + 6;
  for (const auto& entry : entries) {
    if (!entry.visible) continue;
    if (entry.separator) {
      draw::rect(ui, x, y, width, 1, 0, c.surface_variant);
      y += 1 + 2;
      continue;
    }
    const bool enabled = entry.enabled && entry.children.empty();
    ui.rectangle({.width = width, .height = 28, .radius = 8, .color = with_alpha(c.primary_container, 0),
                  .place = {.x = x, .y = y},
                  .input = {.on_click = [id, entry = entry.id, enabled](const vw::ClickEvent&) {
                              if (enabled) vw::tray::activate_menu(id, entry);
                            },
                            .hover = enabled ? std::optional(vw::Hover{.color = c.primary_container, .duration = 0.12F})
                                             : std::nullopt}});
    if (!entry.icon.empty()) draw::icon(ui, "", entry.icon, x, y + 6, 16);
    draw::text_middle(ui, entry.label, x + 24, y + 14,
                      {.size = 11, .color = entry.enabled ? c.text : c.text_muted, .family = font::display,
                       .max_width = width - 24});
    y += 28 + 2;
  }
}

// --- Audio OSD -----------------------------------------------------------------

namespace {

float osd_slider_height(const Screen& screen) {
  const Config& c = config();
  return std::clamp(screen.height * c.osd_slider_height_ratio, static_cast<float>(c.osd_slider_minimum_height),
                    static_cast<float>(c.osd_slider_maximum_height));
}

float osd_height(const Screen& screen) { return (osd_slider_height(screen) + 20) * 2 - 7; }

}  // namespace

Panel audio_panel(const Screen& screen, const Panel& power, float edge) {
  Shell& state = shell();
  // Volume or mute changes show the OSD for 1.8 s (not the initial state:
  // shell.qml waits 300 ms before listening).
  static const auto started = Clock::now();
  static double last_volume = -1;
  static bool last_muted = false;
  if (const auto sink = vw::pipewire::default_sink(); sink && sink->volume_known) {
    if (Clock::now() - started > std::chrono::milliseconds(300) && last_volume >= 0 &&
        (std::abs(sink->volume() - last_volume) > 0.0005 || sink->muted != last_muted))
      state.audio_until = Clock::now() + std::chrono::milliseconds(config().osd_hold_ms);
    last_volume = sink->volume();
    last_muted = sink->muted;
  }
  const bool automatic = config().osd_on_change && Clock::now() < state.audio_until;
  const bool tugged = Clock::now() < state.audio_tug_until;
  if (automatic || tugged) vw::request_animation_continuation();
  const bool open = automatic || tugged || state.audio_hovered;
  // While dragged it tracks the pointer exactly, and release carries on from there.
  vw::AnimationOptions options = motion::quick();
  if (state.audio_dragging) options.duration = 0;
  const float target = state.audio_dragging ? state.audio_tug : open ? 1 : 0;
  const float reveal = static_cast<float>(vw::animate("audio.reveal", target, options));
  const float slide = static_cast<float>(vw::animate("audio.slide", 32 * (1 - target), options));
  state.audio_shown = reveal;
  if (reveal < 0.002F) {
    state.audio_hovered = false;
    return {};
  }
  constexpr float width = 60;
  const float height = osd_height(screen);
  const float shift = power.visible() ? power.box.width + 4 : 0;
  // Out from under the sidebar, melted into its edge.
  const float resting = std::min(screen.width - props::frame_thickness, edge) + props::popout_frame_overlap;
  const float right = resting - shift + slide;
  Panel panel;
  panel.reveal = reveal;
  panel.box = {right - width * reveal, middle(screen) - height / 2, width * reveal, height,
               blob::Radii::all(25)};  // osdRadius
  return panel;
}

blob::Box audio_tug_area(const Screen& screen, float edge) {
  const float width = props::frame_thickness + 12;
  const float height = osd_height(screen);
  // On the screen's edge, or straddling the sidebar's while that is out.
  const float x = edge < screen.width ? edge - width / 2 : screen.width - width;
  return {x, middle(screen) - height / 2, width, height, {}};
}

void audio_trigger(vw::Ui& ui, const Screen& screen, const Panel& panel, float edge) {
  const blob::Box area = audio_tug_area(screen, edge);
  ui.hit_area({.width = area.width, .height = area.height, .place = {.x = area.x, .y = area.y},
               .input = {.on_drag_start = [](const vw::DragEvent&) {
                           Shell& state = shell();
                           state.audio_tug_start = state.audio_tug = std::clamp(state.audio_shown, 0.0F, 1.0F);
                           state.audio_dragging = true;
                         },
                         .on_drag = [](const vw::DragEvent& drag) {
                           Shell& state = shell();
                           state.audio_tug = std::clamp(state.audio_tug_start + static_cast<float>(drag.start_x - drag.x) / 70, 0.0F, 1.0F);
                         },
                         .on_drag_end = [](const vw::DragEvent&) {
                           Shell& state = shell();
                           state.audio_dragging = false;
                           state.audio_tug_until = state.audio_tug >= 0.35F
                                                       ? Clock::now() + std::chrono::milliseconds(config().osd_hold_ms)
                                                       : Clock::time_point{};
                         }}});
  // Stays open while in use, and leaves shortly after the pointer does.
  if (panel.visible())
    track_hover(ui, panel.box, [](bool inside) {
      shell().audio_hovered = inside;
      if (!inside) shell().audio_tug_until = Clock::now() + std::chrono::milliseconds(600);
    });
}

void draw_audio(vw::Ui& ui, const Panel& panel) {
  if (!panel.visible() || panel.reveal < 0.5F) return;
  const Colours& c = colours();
  const Screen screen{0, 0};
  (void)screen;
  const float slider_height = (panel.box.height + 7) / 2 - 20;
  const float control_height = slider_height + 20;
  const float controls_top = panel.box.y + (panel.box.height - control_height * 2 - 4) / 2;
  // The slider column sits 25 px in from the panel's left edge (the rest is
  // under the frame).
  const float centre_x = panel.box.x + panel.box.width - 60 + 25;
  const vw::Clip clip{.x = panel.box.x, .y = panel.box.y, .width = panel.box.width, .height = panel.box.height};
  (void)clip;
  const auto control = [&](const std::optional<vw::pipewire::Node>& node, bool microphone, float top) {
    if (!node) return;
    const double level = std::clamp(node->volume(), 0.0, 1.0);
    const std::string icon = node->muted ? (microphone ? "mic_off" : "volume_off") : (microphone ? "mic" : "volume_up");
    widgets::circle_handle_slider(
        ui, microphone ? "osd.source" : "osd.sink", centre_x - (35 + 8) / 2.0F, top + 10, slider_height,
        {.value = level, .handle_diameter = 35, .endpoint_padding = 4, .track = c.secondary_container,
         .fill = node->muted ? c.outline : microphone ? c.secondary : c.primary,
         .handle = node->muted ? c.text_muted : c.text, .icon = icon,
         .value_text = std::format("{}%", std::lround(node->volume() * 100)),
         .moved = [id = node->id](double value) { vw::pipewire::set_volume(id, value); }});
  };
  control(vw::pipewire::default_sink(), false, controls_top);
  control(vw::pipewire::default_source(), true, controls_top + control_height + 4);
}

}  // namespace blx
