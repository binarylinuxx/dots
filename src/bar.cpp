// The bottom bar, after ~/shell's blxshell/bar laid out horizontally.
#include <vw/vw.hpp>

#include <algorithm>
#include <filesystem>
#include <vector>
#include <chrono>
#include <format>
#include <functional>
#include <string>

#include "net.hpp"
#include "config.hpp"
#include "draw.hpp"
#include "parts.hpp"
#include "state.hpp"
#include "system.hpp"

namespace blx {
namespace {

constexpr float pill_height = 36;
constexpr float status_icon = 20;
constexpr float status_gap = 4;
constexpr int status_count = 6;
constexpr float power_size = 36;
constexpr float tray_gap = 3;
constexpr float tray_padding = 5;

float status_width() {
  const Config& c = config();
  const int count = c.bar_status_wifi + c.bar_status_bluetooth + c.bar_status_audio + c.bar_status_battery +
                    c.bar_status_quickshare + c.bar_status_widgets;
  if (count == 0) return 0;
  return static_cast<float>(count) * status_icon + static_cast<float>(count - 1) * status_gap + 10;
}

float tray_item() { return static_cast<float>(std::clamp(config().bar_tray_size, 14, 34)); }

float bar_centre(const Screen& screen) { return screen.height - props::bar_height / 2; }

std::string app_glyph(std::string name) {
  std::ranges::transform(name, name.begin(), [](unsigned char c) { return std::tolower(c); });
  const auto has = [&name](std::initializer_list<const char*> words) {
    return std::ranges::any_of(words, [&name](const char* word) {
      return name.find(word) != std::string::npos;
    });
  };
  if (has({"terminal", "ghostty", "kitty", "alacritty", "foot", "wezterm", "konsole"}))
    return "terminal";
  if (has({"code", "zed", "emacs", "neovim", "sublime", "jetbrains"})) return "code";
  if (has({"firefox", "zen", "chrom", "brave", "vivaldi", "opera", "edge", "qutebrowser"}))
    return "language";
  if (has({"discord", "telegram", "materialgram", "signal", "slack", "teams", "element"}))
    return "chat";
  if (has({"steam", "lutris", "heroic", "prism", "game"})) return "sports_esports";
  if (has({"music", "spotify", "vlc", "mpv"})) return "music_note";
  if (has({"nautilus", "dolphin", "thunar", "nemo", "pcmanfm"})) return "folder";
  return "desktop_windows";
}

std::size_t tray_count() {
  return static_cast<std::size_t>(std::ranges::count_if(vw::tray::items(), [](const auto& item) {
    return item.status != vw::tray::Status::passive;
  }));
}

float tray_width() {
  const auto count = static_cast<float>(tray_count());
  return count == 0 ? 0 : count * tray_item() + (count - 1) * tray_gap + tray_padding * 2;
}

// The Runix logo that ships with blxshell: <prefix>/share/blxshell/icons, or
// the source tree while developing.
const std::string& runix_logo() {
  static const std::string path = [] {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;
    std::error_code error;
    const auto executable = fs::read_symlink("/proc/self/exe", error);
    if (!error) candidates.push_back(executable.parent_path().parent_path() / "share" / "blxshell" / "icons");
#ifdef BLXSHELL_SOURCE_ICON_DIR
    candidates.emplace_back(BLXSHELL_SOURCE_ICON_DIR);
#endif
    for (const auto& directory : candidates)
      if (fs::exists(directory / "runix-logo.svg", error)) return (directory / "runix-logo.svg").string();
    return std::string();
  }();
  return path;
}

// What the workspaces pill shows, per the config's style.
std::vector<system::Workspace> shown_workspaces() {
  auto list = system::workspaces();
  if (config().bar_workspace_style != "apps")
    for (auto& workspace : list) workspace.apps.clear();
  return list;
}

float cell_width(const system::Workspace& workspace) {
  return 28.0F + static_cast<float>(workspace.apps.size()) * 15;
}

float workspaces_width(const std::vector<system::Workspace>& list) {
  if (list.empty()) return 0;
  float width = 8;
  for (const auto& workspace : list) width += cell_width(workspace) + 2;
  return width - 2;
}

void workspaces(vw::Ui& ui, const Screen& screen, const std::vector<system::Workspace>& list, float left) {
  const Colours& c = colours();
  constexpr float spacing = 2;
  constexpr float indicator = 28;
  const float y = bar_centre(screen) - pill_height / 2;
  const float centre_y = bar_centre(screen);
  if (config().bar_pills)
    draw::rect(ui, left, y, workspaces_width(list), pill_height, pill_height / 2, c.surface_container);

  // The indicator glides to the active workspace and stretches over its icons.
  float active_x = left + 4;
  float active_width = indicator;
  for (const auto& w : list) {
    if (w.active) {
      active_width = w.apps.empty() ? indicator : cell_width(w) - 4;
      active_x += (cell_width(w) - active_width) / 2;
      break;
    }
    active_x += cell_width(w) + spacing;
  }
  const vw::AnimationOptions glide{.duration = 0.3, .easing = vw::Easing::out_cubic};
  const auto indicator_x = static_cast<float>(vw::animate("bar.workspace", active_x, glide));
  const auto indicator_width = static_cast<float>(vw::animate("bar.workspace.width", active_width, glide));
  draw::rect(ui, indicator_x, centre_y - indicator / 2, indicator_width, indicator, indicator / 2, c.primary);

  const bool numbers = config().bar_workspace_style == "numbers";
  float x = left + 4;
  for (std::size_t index = 0; index < list.size(); ++index) {
    const auto& workspace = list[index];
    const float w = cell_width(workspace);
    const bool current = workspace.active;
    if (numbers) {
      draw::text_centered(ui, std::to_string(index + 1), x + w / 2, centre_y,
                          {.size = 13, .color = current ? c.primary_text : c.text_muted, .weight = vw::weight::bold,
                           .family = font::display});
    } else if (workspace.apps.empty()) {
      draw::text_centered(ui, current ? "\u25cf" : "\u00b7", x + w / 2, centre_y - 1,
                          {.size = current ? 17.0F : 20.0F, .color = current ? c.primary_text : c.text_muted,
                           .weight = vw::weight::black, .family = font::display});
    } else {
      const float icons = static_cast<float>(workspace.apps.size()) * 17 - 1;
      for (std::size_t app = 0; app < workspace.apps.size(); ++app)
        draw::glyph(ui, app_glyph(workspace.apps[app]), x + (w - icons) / 2 + 8 + static_cast<float>(app) * 17, centre_y, 16,
                    current ? c.primary_text : c.primary);
    }
    draw::state_layer(ui, x, y + 4, w, pill_height - 8, (pill_height - 8) / 2, current ? c.primary_text : c.primary,
                      {.on_click = [focus = workspace.focus](const vw::ClickEvent&) { focus(); }});
    x += w + spacing;
  }
}

void status_pill(vw::Ui& ui, const Screen& screen, float left) {
  const Colours& c = colours();
  const float centre_y = bar_centre(screen);
  if (config().bar_pills)
    draw::rect(ui, left, centre_y - pill_height / 2, status_width(), pill_height, pill_height / 2, c.surface_container);

  const auto network = net::status();
  std::string wifi = "wifi_off";
  if (network.primary == vw::network::Kind::wired) wifi = "cable";
  else if (network.primary == vw::network::Kind::wifi)
    wifi = network.strength < 30 ? "network_wifi_1_bar"
           : network.strength < 60 ? "network_wifi_2_bar"
                                   : "wifi";

  std::string bluetooth = "bluetooth_disabled";
  if (const auto adapter = vw::bluetooth::adapter(); adapter && adapter->powered) {
    const auto devices = vw::bluetooth::devices();
    bluetooth = std::ranges::any_of(devices, &vw::bluetooth::Device::connected)
                    ? "bluetooth_connected" : "bluetooth";
  }

  std::string audio = "volume_up";
  if (const auto sink = vw::pipewire::default_sink())
    audio = sink->muted ? "volume_off" : sink->volume() < 0.34 ? "volume_down" : "volume_up";

  std::string power = "balance";
  vw::Color power_tint = c.primary;
  if (const auto battery = vw::upower::display_device()) {
    power = battery->percentage < 20 ? "battery_alert" : "battery_full";
    if (battery->percentage < 20) power_tint = c.error;
  } else if (const auto profile = vw::upower::profile(); profile == "power-saver") {
    power = "energy_savings_leaf";
  } else if (profile == "performance") {
    power = "rocket_launch";
  }

  struct Entry {
    const char* kind;
    std::string glyph;
    vw::Color tint;
    bool shown;
  };
  const Config& settings = config();
  const Entry entries[status_count] = {{"wifi", wifi, c.primary, settings.bar_status_wifi},
                                       {"bluetooth", bluetooth, c.primary, settings.bar_status_bluetooth},
                                       {"audio", audio, c.primary, settings.bar_status_audio},
                                       {"battery", power, power_tint, settings.bar_status_battery},
                                       {"quickshare", "phone_android", c.primary, settings.bar_status_quickshare},
                                       {"widgets", settings.background_widgets_editing ? "done" : "edit", c.primary,
                                        settings.bar_status_widgets}};
  float x = left + 5;
  for (const auto& entry : entries) {
    if (!entry.shown) continue;
    const float centre_x = x + status_icon / 2;
    draw::glyph(ui, entry.glyph, centre_x, centre_y, 18, entry.tint);
    draw::state_layer(
        ui, x, centre_y - status_icon / 2, status_icon, status_icon, status_icon / 2, entry.tint,
        {.on_click = [kind = std::string(entry.kind), centre_x](const vw::ClickEvent& click) {
           HoverPopout& status = shell().status;
           if (kind == "quickshare" && click.button != 273) {
             shell().quick_share_open = true;
             return;
           }
           if (kind == "widgets" && click.button != 273) {
             config().background_widgets_editing = !config().background_widgets_editing;
             return;
           }
           if (kind == "quickshare" || kind == "widgets") return;
           if (click.button == 273) {  // right click: the quick action
             if (kind == "wifi") net::set_wifi_enabled(!net::status().wifi_enabled);
             else if (kind == "bluetooth") {
               if (const auto adapter = vw::bluetooth::adapter())
                 vw::bluetooth::set_powered(!adapter->powered);
             } else if (kind == "audio") {
               if (const auto sink = vw::pipewire::default_sink()) vw::pipewire::toggle_mute(sink->id);
             } else {
               const auto profile = vw::upower::profile();
               vw::upower::set_profile(profile == "power-saver" ? "balanced"
                                       : profile == "balanced"  ? "performance"
                                                                : "power-saver");
             }
             return;
           }
           status.anchor = centre_x;
           status.pinned = status.pinned == kind ? "" : kind;
         },
         .on_hover = [kind = std::string(entry.kind), centre_x](bool inside) {
           if (kind == "quickshare" || kind == "widgets") return;  // no popout
           if (inside) shell().status.enter(kind, centre_x);
           else shell().status.leave();
         }});
    x += status_icon + status_gap;
  }
}

void tray(vw::Ui& ui, const Screen& screen, float left) {
  const float y = bar_centre(screen) - tray_item() / 2;
  float x = left + tray_padding;
  for (const auto& item : vw::tray::items()) {
    if (item.status == vw::tray::Status::passive) continue;
    const float centre_x = x + tray_item() / 2;
    draw::icon(ui, item.icon_path, item.icon, x, y, tray_item(),
               std::nullopt, 0, vw::ImageFit::contain, config().bar_tint_tray_icons ? draw::icon_tint() : vw::ImageEffect{});
    draw::state_layer(ui, x - 2, y - 2, tray_item() + 4, tray_item() + 4, 6, colours().primary,
                      {.on_click = [id = item.id](const vw::ClickEvent& click) {
                         if (click.button == 273) vw::tray::secondary_activate(id);
                         else vw::tray::activate(id);
                       },
                       .on_hover = [id = item.id, centre_x](bool inside) {
                         if (inside) shell().tray.enter(id, centre_x);
                         else shell().tray.leave();
                       }});
    x += tray_item() + tray_gap;
  }
}

float window_width(const std::string& title) {
  return 20 + 8 + std::min(draw::measure(title, 13, vw::weight::semibold, font::display),
                           static_cast<float>(config().bar_title_width));
}

void active_window(vw::Ui& ui, const Screen& screen, const std::string& app, const std::string& title, float x) {
  const Colours& c = colours();
  const float centre_y = bar_centre(screen);
  draw::glyph(ui, app_glyph(app), x + 10, centre_y, 20, c.primary);
  draw::text_middle(ui, title, x + 28, centre_y, {.size = 13, .color = c.primary, .weight = vw::weight::semibold,
                                                  .family = font::display, .max_width = static_cast<float>(config().bar_title_width)});
}

// One thing on the bar: how wide it is, and how to draw it from an x.
struct Module {
  float width = 0;
  std::function<void(float x)> draw;
};

Module module_for(vw::Ui& ui, const Screen& screen, const std::string& id) {
  const Colours& c = colours();
  const Config& settings = config();
  const float centre_y = bar_centre(screen);
  if (id == "os")
    return {30, [&ui, &c, &settings, centre_y](float x) {
              const bool builtin = settings.bar_os_icon == "runix";
              draw::icon(ui, builtin ? runix_logo() : "", builtin ? "" : settings.bar_os_icon, x + 4, centre_y - 11, 22,
                         std::nullopt, 0, vw::ImageFit::contain, draw::icon_tint());
              draw::state_layer(ui, x, centre_y - 15, 30, 30, 15, c.primary, {.on_click = [](const vw::ClickEvent&) {
                shell().launcher_open = !shell().launcher_open;
              }});
            }};
  if (id == "workspaces") {
    auto list = shown_workspaces();
    const float width = workspaces_width(list);
    return {width, [&ui, &screen, list = std::move(list)](float x) { workspaces(ui, screen, list, x); }};
  }
  if (id == "window") {
    const auto window = system::active_window();
    std::string title = window && !window->title.empty() ? window->title : "Desktop";
    std::string app = window ? window->app : "";
    const float width = window_width(title);
    return {width, [&ui, &screen, title = std::move(title), app = std::move(app)](float x) { active_window(ui, screen, app, title, x); }};
  }
  if (id == "tray" && tray_count() > 0)
    return {tray_width(), [&ui, &screen](float x) { tray(ui, screen, x); }};
  if (id == "keyboard") {
    std::string layout = system::keyboard_layout();
    if (layout.empty()) return {};
    return {37, [&ui, &c, centre_y, layout = std::move(layout)](float x) {
              draw::text_centered(ui, layout, x + 18.5F, centre_y,
                                  {.size = 11, .color = c.primary, .weight = vw::weight::bold, .family = font::display});
              draw::state_layer(ui, x, centre_y - 12, 37, 24, 12, c.text, {.on_click = [](const vw::ClickEvent&) {
                vw::hyprland::dispatch("switchxkblayout current next");
              }});
            }};
  }
  if (id == "clock") {
    std::string time = system::format_time(settings.bar_clock_format);
    const auto size = static_cast<float>(std::clamp(settings.bar_clock_size, 10, 28));
    const float width = draw::measure(time, size, vw::weight::black, font::display);
    return {width, [&ui, &c, centre_y, size, time = std::move(time)](float x) {
              draw::text_middle(ui, time, x, centre_y, {.size = size, .color = c.primary, .weight = vw::weight::black, .family = font::display});
            }};
  }
  if (id == "status" && status_width() > 0)
    return {status_width(), [&ui, &screen](float x) { status_pill(ui, screen, x); }};
  if (id == "power")
    return {power_size, [&ui, &c, centre_y](float x) {
              draw::glyph(ui, "power_settings_new", x + power_size / 2, centre_y, 26, mix(c.error, vw::rgb(0x000000), 0.26F));
              draw::state_layer(ui, x, centre_y - power_size / 2, power_size, power_size, power_size / 2, c.error,
                                {.on_click = [](const vw::ClickEvent&) { shell().power_pinned = !shell().power_pinned; }});
            }};
  return {};
}

}  // namespace

void draw_bar(vw::Ui& ui, const Screen& screen) {
  const Config& settings = config();
  constexpr float gap = 10;
  const auto modules = [&](const std::vector<std::string>& ids) {
    std::vector<Module> result;
    for (const std::string& id : ids)
      if (Module module = module_for(ui, screen, id); module.draw) result.push_back(std::move(module));
    return result;
  };
  // The left side sits close to the edge, workspaces right beside the icon.
  constexpr float left_gap = 4;
  float left = 4;
  for (const Module& module : modules(settings.bar_left)) {
    module.draw(left);
    left += module.width + left_gap;
  }
  float right = screen.width - 8;
  const auto end = modules(settings.bar_right);
  for (auto module = end.rbegin(); module != end.rend(); ++module) {
    right -= module->width;
    module->draw(right);
    right -= gap;
  }
  // Centred on the screen, unless that would run into the left zone.
  const auto middle_modules = modules(settings.bar_center);
  float total = 0;
  for (const Module& module : middle_modules) total += module.width + gap;
  float x = std::max(left + 6, (screen.width - (total - gap)) / 2);
  for (const Module& module : middle_modules) {
    module.draw(x);
    x += module.width + gap;
  }
}

void track_hover(vw::Ui& ui, const blob::Box& box, vw::HoverHandler handler) {
  if (box.empty()) return;
  ui.hit_area({.width = box.width, .height = box.height, .place = {.x = box.x, .y = box.y},
               .input = {.on_hover = std::move(handler)}});
}

}  // namespace blx
