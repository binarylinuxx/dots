// The settings app: a navigation rail and pages of grouped Material 3 rows.
// Rows edit config() through member pointers, so a page can also reset itself
// to the defaults of a fresh Config. Every change is written to config.json,
// which the running shell watches.
#include "settings.hpp"

#include <vw/vw.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <functional>
#include <sstream>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include "config.hpp"
#include "ipc.hpp"
#include "packages.hpp"
#include "plugins.hpp"
#include "draw.hpp"
#include "style.hpp"
#include "system.hpp"

namespace blx {
namespace {

using Clock = std::chrono::steady_clock;
using Field = std::variant<std::monostate, bool Config::*, float Config::*, int Config::*, std::string Config::*>;

constexpr const char* list_id = "settings-list";
constexpr const char* tabs_id = "settings-tabs";
constexpr float pane_width = 352;
constexpr float content_padding = 28;
constexpr float content_limit = 760;
// Android 16's grouped lists: a hairline between items of a group, large outer
// corners, small inner ones.
constexpr float row_gap = 3;
constexpr float group_gap = 14;
constexpr float outer_radius = 24;
constexpr float inner_radius = 5;

enum class Kind { header, toggle, slider, choice, text, action, filter, app, module, colour, plugin, plugin_setting };

struct Option {
  std::string value;
  std::string label;
};

struct Row {
  Kind kind = Kind::header;
  std::string label;
  std::string detail;
  Field field;
  // Sliders.
  float minimum = 0;
  float maximum = 1;
  float step = 1;
  int decimals = 0;
  std::string unit;
  // Applied only on release: changing it is expensive (it runs matugen).
  bool on_release = false;
  // Choices.
  std::vector<Option> options;
  // Actions.
  std::string glyph;
  std::function<void()> run;
  // Apps: the desktop entry id; `glyph` is its icon file. Plugins: the
  // plugin's id.
  std::string id;
  // Plugin settings: the key, its type ("bool", "number", "string",
  // "choice") and its default as JSON text.
  std::string key;
  std::string type;
  std::string fallback;
  // Plugins: why it can't run here, if it can't.
  std::string problem;
};

Row header(std::string label) { return {.kind = Kind::header, .label = std::move(label)}; }

Row toggle(std::string label, std::string detail, bool Config::*field) {
  return {.kind = Kind::toggle, .label = std::move(label), .detail = std::move(detail), .field = field};
}

Row slider(std::string label, Field field, float minimum, float maximum, float step, std::string unit = "", int decimals = 0) {
  return {.kind = Kind::slider, .label = std::move(label), .field = field, .minimum = minimum, .maximum = maximum,
          .step = step, .decimals = decimals, .unit = std::move(unit)};
}

Row choice(std::string label, std::string Config::*field, std::vector<Option> options) {
  return {.kind = Kind::choice, .label = std::move(label), .field = field, .options = std::move(options)};
}

Row text(std::string label, std::string detail, std::string Config::*field) {
  return {.kind = Kind::text, .label = std::move(label), .detail = std::move(detail), .field = field};
}

Row action(std::string label, std::string detail, std::string glyph, std::function<void()> run) {
  return {.kind = Kind::action, .label = std::move(label), .detail = std::move(detail), .glyph = std::move(glyph), .run = std::move(run)};
}

std::string self_path() { return system::self_path(); }

struct Page {
  const char* glyph;
  const char* title;
  const char* summary;
  int group;  // tabs of one group sit together
  int tone;   // the colour family of its icon
  std::vector<Row> rows;
  bool lists_apps = false;
  std::function<void()> on_reset = {};
  bool lists_plugins = false;  // rows come from the installed plugins
};

struct Tone {
  vw::Color container;
  vw::Color content;
};

Tone tone(int index) {
  const Colours& c = colours();
  if (index == 1) return {c.secondary_container, c.secondary};
  if (index == 2) return {c.tertiary_container, c.tertiary};
  return {c.primary_container, c.primary};
}

void commit(bool force);

const std::vector<Page>& pages() {
  static const std::vector<Page> list = [] {
    const std::vector<Option> roles{{"primary", "Primary"}, {"secondary", "Secondary"}, {"tertiary", "Tertiary"},
                                    {"outline", "Outline"}, {"outlineVariant", "Subtle"}, {"error", "Error"}};
    std::vector<Page> result;
    result.push_back({"palette", "Theme", "Colours, wallpaper, opacity", 0, 0, {
        header("Colours"),
        choice("Mode", &Config::matugen_mode, {{"dark", "Dark"}, {"light", "Light"}}),
        choice("Palette style", &Config::matugen_scheme,
               {{"scheme-tonal-spot", "Tonal"}, {"scheme-content", "Content"}, {"scheme-expressive", "Expressive"},
                {"scheme-fidelity", "Fidelity"}, {"scheme-fruit-salad", "Fruit salad"}, {"scheme-monochrome", "Mono"},
                {"scheme-neutral", "Neutral"}, {"scheme-rainbow", "Rainbow"}, {"scheme-vibrant", "Vibrant"}}),
        choice("Colour from wallpaper", &Config::matugen_prefer,
               {{"saturation", "Most vivid"}, {"darkness", "Darkest"}, {"lightness", "Lightest"}}),
        [] {
          Row row = slider("Contrast", &Config::matugen_contrast, -1, 1, 0.1F, "", 1);
          row.on_release = true;
          return row;
        }(),
        action("Change wallpaper", "Opens the carousel in the launcher", "wallpaper",
               [] { vw::spawn({self_path(), "wallpapers"}); }),
        header("Apps"),
        toggle("GTK apps", "GTK 3 and 4 in these colours, with Material 3 shapes", &Config::theme_gtk_apps),
        toggle("Qt apps", "Through qt6ct or qt5ct and KDE's colours, with Material 3 shapes", &Config::theme_qt_apps),
        header("Images"),
        slider("Icon tint", &Config::icon_tint, 0, 1, 0.05F, "%"),
        slider("Album art tint", &Config::art_tint, 0, 1, 0.05F, "%"),
        slider("Album art hue", &Config::art_hue, -180, 180, 5, "\u00b0"),
        header("Source"),
        choice("Build colours from", &Config::theme_source, {{"wallpaper", "Wallpaper"}, {"colour", "A colour"}}),
        choice("Preset", &Config::colour_preset,
               {{"generated", "All generated"}, {"neutral", "Neutral surfaces"}, {"amoled", "AMOLED"}, {"mono", "Mono"}}),
        text("Seed colour", "Used with A colour, like #6750a4", &Config::theme_seed),
        header("Surface"),
        slider("Opacity", &Config::global_opacity, 0.3F, 1, 0.01F, "%"),
        toggle("Frosted wallpaper", "Hides windows. Off: blurred desktop through a zwwm rule", &Config::frosted),
        slider("Frost", &Config::frosted_strength, 1, 12, 1),
        toggle("Outline", "A thin line around the whole shell", &Config::sdf_outline_enabled),
        slider("Outline width", &Config::sdf_outline_width, 0.5F, 6, 0.5F, " px", 1),
        choice("Outline colour", &Config::sdf_outline_color_role, roles),
        header("Shadow"),
        toggle("Shadow", "Only while fully opaque", &Config::shadow_enabled),
        slider("Strength", &Config::shadow_strength, 0, 1, 0.05F, "%"),
        slider("Softness", &Config::shadow_blur, 8, 72, 2, " px"),
    }});
    result.push_back({"format_paint", "Colours", "Override any colour role", 0, 2, {
        header("Accents"),
        {.kind = Kind::colour, .label = "Primary", .id = "primary"},
        {.kind = Kind::colour, .label = "On primary", .id = "primaryText"},
        {.kind = Kind::colour, .label = "Primary container", .id = "primaryContainer"},
        {.kind = Kind::colour, .label = "Secondary", .id = "secondary"},
        {.kind = Kind::colour, .label = "Secondary container", .id = "secondaryContainer"},
        {.kind = Kind::colour, .label = "Tertiary", .id = "tertiary"},
        {.kind = Kind::colour, .label = "Tertiary container", .id = "tertiaryContainer"},
        {.kind = Kind::colour, .label = "Error", .id = "error"},
        header("Surfaces"),
        {.kind = Kind::colour, .label = "Surface", .id = "surface"},
        {.kind = Kind::colour, .label = "Container low", .id = "surfaceContainerLow"},
        {.kind = Kind::colour, .label = "Container", .id = "surfaceContainer"},
        {.kind = Kind::colour, .label = "Container high", .id = "surfaceContainerHigh"},
        {.kind = Kind::colour, .label = "Container highest", .id = "surfaceContainerHighest"},
        {.kind = Kind::colour, .label = "Surface variant", .id = "surfaceVariant"},
        header("Text and lines"),
        {.kind = Kind::colour, .label = "Text", .id = "text"},
        {.kind = Kind::colour, .label = "Muted text", .id = "textMuted"},
        {.kind = Kind::colour, .label = "Outline", .id = "outline"},
        {.kind = Kind::colour, .label = "Subtle outline", .id = "outlineVariant"},
    }, false, [] {
      for (const auto& [role, value] : config().colour_overrides) vw::reset_text_field("colour." + role);
      config().colour_overrides.clear();
    }});
    result.push_back({"rounded_corner", "Shape", "Frame, corners, bar height", 0, 1, {
        header("Frame"),
        slider("Frame thickness", &Config::frame_thickness, 2, 32, 1, " px"),
        slider("Frame corners", &Config::frame_radius, 0, 48, 1, " px"),
        slider("Bar height", &Config::bar_height, 40, 72, 1, " px"),
        header("Panels"),
        slider("Melt between panels", &Config::blob_smoothing, 1, 60, 1, " px"),
        slider("Popout corners", &Config::panel_radius, 0, 32, 1, " px"),
        slider("Sidebar corners", &Config::island_radius, 0, 40, 1, " px"),
        slider("Launcher corners", &Config::launcher_radius, 0, 48, 1, " px"),
        slider("Card corners", &Config::card_radius, 0, 32, 1, " px"),
    }});
    result.push_back({"text_fields", "Type & motion", "Fonts, text size, animation", 0, 2, {
        header("Fonts"),
        text("Language", "auto follows LANG; or en, ru", &Config::language),
        text("Display font", "Titles, the clock and the bar", &Config::font_display),
        text("Interface font", "Controls and small text", &Config::font_utility),
        slider("Text size", &Config::font_scale, 0.75F, 1.5F, 0.05F, "%"),
        header("Motion"),
        slider("Animation speed", &Config::animation_speed, 0.25F, 3, 0.05F, "x", 2),
        choice("Panel motion", &Config::motion_style, {{"bounce", "Springy"}, {"smooth", "Smooth"}, {"snappy", "Snappy"}}),
        slider("Popouts stay after leaving", &Config::hover_linger_ms, 0, 1000, 20, " ms"),
    }});
    result.push_back({"apps", "Apps", "Favourites, hidden apps, ranking", 1, 2, {
        header("Order"),
        choice("Sort the list by", &Config::launcher_sort, {{"name", "Name"}, {"used", "Most used"}, {"recent", "Recent"}}),
        toggle("Favourites first", "Pinned above everything else", &Config::launcher_favorites_first),
        header("Search"),
        toggle("Search descriptions", "Match what apps do, not only their names", &Config::launcher_search_comments),
        toggle("Favourites rank higher", "Preferred apps always win first", &Config::launcher_boost_favorites),
        header("Looks"),
        toggle("Show descriptions", "", &Config::launcher_show_comments),
        toggle("Star on favourites", "", &Config::launcher_show_favorite_star),
        toggle("Tint app icons", "Match them to the theme", &Config::launcher_tint_app_icons),
        slider("Row height", &Config::launcher_row_height, 36, 80, 2, " px"),
        slider("Icon size", &Config::launcher_icon_size, 16, 56, 2, " px"),
        slider("Width", &Config::launcher_width, 480, 1000, 10, " px"),
        slider("Maximum height", &Config::launcher_max_height, 240, 900, 10, " px"),
        slider("Sink into the bar", &Config::launcher_sink, 0, 48, 1, " px"),
        header("Behaviour"),
        toggle("Close after launching", "", &Config::launcher_close_on_launch),
        toggle("Remember what you open", "Needed for Most used and Recent", &Config::launcher_record_usage),
        action("Forget usage history", "", "history", [] {
          config().launcher_usage.clear();
          commit(true);
        }),
        header("Your apps"),
        {.kind = Kind::filter, .label = "Search apps"},
    }, true, [] {
      config().launcher_favorites.clear();
      config().launcher_preferred.clear();
      config().launcher_hidden.clear();
      config().launcher_names.clear();
    }});
    result.push_back({"bottom_navigation", "Bar", "Layout, clock, workspaces", 1, 0, {
        header("Layout"),
        {.kind = Kind::module, .label = "OS button", .id = "os"},
        {.kind = Kind::module, .label = "Workspaces", .id = "workspaces"},
        {.kind = Kind::module, .label = "Active window", .id = "window"},
        {.kind = Kind::module, .label = "System tray", .id = "tray"},
        {.kind = Kind::module, .label = "Keyboard layout", .id = "keyboard"},
        {.kind = Kind::module, .label = "Clock", .id = "clock"},
        {.kind = Kind::module, .label = "Status icons", .id = "status"},
        {.kind = Kind::module, .label = "Power button", .id = "power"},
        header("Clock"),
        text("Clock format", "strftime, e.g. %H:%M or %a %d  %I:%M %p", &Config::bar_clock_format),
        slider("Clock size", &Config::bar_clock_size, 10, 26, 1, " px"),
        header("Workspaces"),
        choice("Show", &Config::bar_workspace_style, {{"apps", "App icons"}, {"dots", "Dots"}, {"numbers", "Numbers"}}),
        header("Look"),
        toggle("Pill backgrounds", "Behind workspaces and status icons", &Config::bar_pills),
        text("OS icon", "runix, or any icon theme name", &Config::bar_os_icon),
        slider("Window title width", &Config::bar_title_width, 100, 640, 10, " px"),
        slider("Tray icon size", &Config::bar_tray_size, 14, 32, 1, " px"),
        toggle("Tint tray icons", "Match them to the theme", &Config::bar_tint_tray_icons),
        header("Status icons"),
        toggle("Wi-Fi", "", &Config::bar_status_wifi),
        toggle("Bluetooth", "", &Config::bar_status_bluetooth),
        toggle("Audio", "", &Config::bar_status_audio),
        toggle("Battery and power profile", "", &Config::bar_status_battery),
        toggle("Quick Share", "", &Config::bar_status_quickshare),
        toggle("Edit desktop widgets", "", &Config::bar_status_widgets),
    }, false, [] {
      const Config defaults;
      config().bar_left = defaults.bar_left;
      config().bar_center = defaults.bar_center;
      config().bar_right = defaults.bar_right;
    }});
    result.push_back({"side_navigation", "Sidebar", "Size, tiles, cards", 1, 1, {
        header("Size"),
        slider("Width", &Config::sidebar_width, 300, 540, 10, " px"),
        slider("Pull area", &Config::sidebar_pull_area, 24, 140, 4, " px"),
        header("Cards"),
        toggle("Media player", "Shown while a player is open", &Config::sidebar_show_media),
        toggle("Brightness", "Laptops and other screens with a backlight", &Config::sidebar_show_brightness),
        toggle("Keep awake", "", &Config::sidebar_show_keep_awake),
        toggle("Screen recorder", "", &Config::sidebar_show_recorder),
        header("Quick tiles"),
        slider("Tile height", &Config::sidebar_tile_height, 44, 80, 2, " px"),
        slider("Corners when on", &Config::sidebar_tile_radius, 0.1F, 0.5F, 0.01F, "%"),
        toggle("Wi-Fi", "", &Config::sidebar_tile_wifi),
        toggle("Bluetooth", "", &Config::sidebar_tile_bluetooth),
        toggle("Microphone", "", &Config::sidebar_tile_mic),
        toggle("Do Not Disturb", "", &Config::sidebar_tile_dnd),
        toggle("Quick Share", "", &Config::sidebar_tile_share),
        toggle("Settings", "Also opens with: blxshell settings", &Config::sidebar_tile_settings),
        toggle("Lock", "Locks the session", &Config::sidebar_tile_lock),
    }});
    result.push_back({"dashboard", "Panels", "Popups, volume, power, dashboard", 1, 2, {
        header("Notifications"),
        toggle("Popups", "When off, notifications only reach the sidebar", &Config::notification_popups),
        choice("Side of the screen", &Config::notification_side, {{"left", "Left"}, {"right", "Right"}}),
        slider("Width", &Config::notification_width, 280, 600, 10, " px"),
        slider("Most shown at once", &Config::notification_max, 1, 10, 1),
        toggle("Show images", "Avatars and covers instead of app icons", &Config::notification_images),
        header("Dashboard"),
        toggle("Dashboard", "Opens when hovering the top edge", &Config::dashboard_enabled),
        toggle("Remember last tab", "", &Config::dashboard_remember_tab),
        slider("Otherwise open tab", &Config::dashboard_tab, 0, 3, 1),
        header("Media island"),
        toggle("Media island", "Pops up on track changes", &Config::island_enabled),
        slider("Compact width", &Config::island_width, 140, 340, 10, " px"),
        slider("Expanded width", &Config::island_expanded_width, 280, 560, 10, " px"),
        slider("Shows for", &Config::island_announce_seconds, 1, 20, 1, " s"),
        header("Volume"),
        toggle("Also show on volume change", "Otherwise only when tugged from the edge", &Config::osd_on_change),
        slider("Stays for", &Config::osd_hold_ms, 500, 6000, 100, " ms"),
        slider("Slider size", &Config::osd_slider_height_ratio, 0.1F, 0.4F, 0.01F, "%"),
    }});
    result.push_back({"power_settings_new", "Power menu", "Actions and their commands", 1, 0, {
        header("Layout"),
        slider("Button size", &Config::power_menu_height_ratio, 0.05F, 0.2F, 0.005F, "%", 1),
        header("Actions"),
        toggle("Lock", "", &Config::power_lock),
        toggle("Log out", "", &Config::power_logout),
        toggle("Suspend", "", &Config::power_suspend),
        toggle("Hibernate", "", &Config::power_hibernate),
        toggle("Restart", "", &Config::power_restart),
        toggle("Firmware setup", "", &Config::power_firmware),
        toggle("Power off", "", &Config::power_off),
        header("Custom commands"),
        text("Lock command", "Empty uses the built-in lock screen", &Config::power_lock_command),
        text("Log out command", "Empty uses loginctl terminate-session self", &Config::power_logout_command),
        text("Suspend command", "Empty uses systemctl suspend", &Config::power_suspend_command),
        text("Hibernate command", "Empty uses systemctl hibernate", &Config::power_hibernate_command),
        text("Restart command", "Empty uses systemctl reboot", &Config::power_restart_command),
        text("Firmware command", "Empty uses systemctl reboot --firmware-setup", &Config::power_firmware_command),
        text("Power off command", "Empty uses systemctl poweroff", &Config::power_off_command),
    }});
    result.push_back({"lock", "Lock screen", "Clock, tint, user", 2, 0, {
        header("Clock"),
        text("Time format", "strftime", &Config::lock_clock_format),
        text("Date format", "strftime", &Config::lock_date_format),
        choice("Clock layout", &Config::lock_clock_style, {{"stacked", "Stacked"}, {"single", "One line"}}),
        slider("Clock size", &Config::lock_clock_size, 64, 220, 2, " px"),
        header("Look"),
        slider("Tint over the wallpaper", &Config::lock_tint, 0, 1, 0.01F, "%"),
        slider("Blur passes", &Config::lock_blur_passes, 1, 6, 1),
        slider("Blur spread", &Config::lock_blur_offset, 0.5F, 6, 0.1F),
        toggle("Show picture and name", "", &Config::lock_show_user),
        toggle("Status icons", "Battery and connections", &Config::lock_show_status),
        toggle("Weather", "", &Config::lock_show_weather),
        toggle("Notification icons", "Icons only, never their text", &Config::lock_show_notifications),
        toggle("Media player", "Play and skip without unlocking", &Config::lock_show_media),
        header("Try it"),
        action("Lock now", "Also: blxshell lock", "lock", [] { vw::spawn({self_path(), "lock"}); }),
    }});
    result.push_back({"login", "Login screen", "Greeter clock, wallpaper, text", 2, 1, {
        header("Clock and look"),
        toggle("Same as lock screen", "Off: the settings below instead of the Lock screen page's", &Config::greeter_match_lock),
        text("Time format", "strftime", &Config::greeter_clock_format),
        text("Date format", "strftime", &Config::greeter_date_format),
        choice("Clock layout", &Config::greeter_clock_style, {{"stacked", "Stacked"}, {"single", "One line"}}),
        slider("Clock size", &Config::greeter_clock_size, 64, 220, 2, " px"),
        slider("Tint over the wallpaper", &Config::greeter_tint, 0, 1, 0.01F, "%"),
        slider("Blur passes", &Config::greeter_blur_passes, 1, 6, 1),
        slider("Blur spread", &Config::greeter_blur_offset, 0.5F, 6, 0.1F),
        toggle("Status icons", "Battery and connections", &Config::greeter_show_status),
        toggle("Weather", "", &Config::greeter_show_weather),
        header("Login screen"),
        text("Wallpaper", "Image path; empty uses yours", &Config::greeter_wallpaper),
        choice("Picture shape", &Config::greeter_avatar_shape,
               {{"cookie9sided", "Cookie"}, {"circle", "Circle"}, {"square", "Square"},
                {"flower", "Flower"}, {"softburst", "Burst"}, {"pentagon", "Pentagon"}}),
        toggle("Power buttons", "Power off and Restart", &Config::greeter_show_power),
        toggle("Hint", "", &Config::greeter_show_hint),
        text("Hint text", "Under the clock before the sheet rises", &Config::greeter_hint),
        slider("Lower the sheet after", &Config::greeter_idle_seconds, 10, 300, 5, " s"),
        toggle("Remember last user and session", "", &Config::greeter_remember),
        header("Apply"),
        action("Preview", "The login screen in a window, with these settings", "visibility", [] {
          vw::spawn({"sh", "-c", "BLXSHELL_GREETER_DIR=\"$HOME\" exec \"$(dirname \"$1\")/blxshell-greeter\"", "sh", self_path()});
        }),
        action("Copy to login screen", "Asks for your password; the NixOS module's syncUser does it for you", "upload", [] {
          vw::spawn({"sh", "-c",
                     "pkexec \"$(dirname \"$1\")/blxshell-greeter\" --export /var/lib/blxshell-greeter --from \"$HOME\" --user \"$(id -un)\"",
                     "sh", self_path()});
        }),
    }});
    result.push_back({"wallpaper_slideshow", "Desktop", "Parallax, widgets, weather", 2, 1, {
        header("Wallpaper"),
        toggle("Parallax", "Shifts with the workspace", &Config::wallpaper_parallax),
        slider("Parallax amount", &Config::wallpaper_parallax_strength, 0, 0.2F, 0.01F, "%"),
        choice("Transition", &Config::wallpaper_transition,
               {{"iris", "Iris"}, {"fade", "Fade"}, {"rise", "Rise"}, {"none", "None"}}),
        header("Clock widget"),
        toggle("Clock", "", &Config::background_clock_enabled),
        text("Time format", "strftime", &Config::background_clock_format),
        text("Date format", "strftime", &Config::background_date_format),
        slider("Size", &Config::background_clock_size, 28, 160, 2, " px"),
        header("Widgets"),
        toggle("Weather", "", &Config::background_weather_enabled),
        slider("Background opacity", &Config::background_widget_opacity, 0, 1, 0.02F, "%"),
        header("Weather"),
        choice("Temperature", &Config::weather_unit, {{"celsius", "Celsius"}, {"fahrenheit", "Fahrenheit"}}),
        toggle("Use this location", "Otherwise it is found from your connection", &Config::prefer_weather_location_from_config),
        text("Location", "City name", &Config::weather_location),
    }});
    result.push_back({"extension", "Plugins", "Installed plugins and their settings", 3, 0, {}, false, {}, true});
    result.push_back({"info", "About", "Config file", 3, 2, {
        header("Configuration"),
        action("Open config file", config_path(), "description", [] { vw::spawn({"xdg-open", config_path()}); }),
    }});
    return result;
  }();
  return list;
}

// --- Saving --------------------------------------------------------------------

Clock::time_point& last_save() {
  static Clock::time_point when{};
  return when;
}

// Written at most 20 times a second while dragging; `force` for the last value.
void commit(bool force) {
  const auto now = Clock::now();
  if (!force && now - last_save() < std::chrono::milliseconds(50)) return;
  last_save() = now;
  save_config();
  sync_style();
}

float get_number(const Field& field) {
  if (const auto* member = std::get_if<float Config::*>(&field)) return config().**member;
  if (const auto* member = std::get_if<int Config::*>(&field)) return static_cast<float>(config().**member);
  return 0;
}

void set_number(const Field& field, float value) {
  if (const auto* member = std::get_if<float Config::*>(&field)) config().**member = value;
  else if (const auto* member = std::get_if<int Config::*>(&field)) config().**member = static_cast<int>(std::lround(value));
}

int& selected_page();

// Text field ids: the same label sits on several pages.
std::string field_id(const Row& row) { return std::format("{}.{}", selected_page(), row.label); }

void reset(const Row& row) {
  static const Config defaults;
  std::visit([](auto member) {
    if constexpr (!std::is_same_v<decltype(member), std::monostate>) config().*member = defaults.*member;
  }, row.field);
  if (row.kind == Kind::text) vw::reset_text_field(field_id(row));
}

std::string number_label(const Row& row, float value) {
  if (row.unit == "%") return std::format("{}%", std::lround(value * 100));
  return std::format("{:.{}f}{}", value, row.decimals, row.unit);
}

// --- Rows ----------------------------------------------------------------------

std::string& expanded_app() {
  static std::string id;
  return id;
}

constexpr int choices_per_line = 3;
constexpr float chip_height = 44;

int choice_lines(const Row& row) {
  return (static_cast<int>(row.options.size()) + choices_per_line - 1) / choices_per_line;
}

// A plugin's own settings page: the Settings view in its Facet file, drawn
// here with its store. Writing the store is all it takes: the shell sees the
// file change and tells the plugin.
struct SettingsView {
  std::unique_ptr<facet::Plugin> plugin;
  std::string id;
  float height = 120;  // as last drawn
};

std::map<std::string, SettingsView>& settings_views() {
  static std::map<std::string, SettingsView> views;
  return views;
}

float height_of(const Row& row) {
  switch (row.kind) {
    case Kind::header: return 44;
    case Kind::toggle:
    case Kind::action: return row.detail.empty() ? 64 : 76;
    case Kind::slider: return 88;
    case Kind::choice: return 52 + static_cast<float>(choice_lines(row)) * (chip_height + 3) + 13;
    case Kind::text: return 118;
    case Kind::filter: return 68;
    case Kind::module: return 72;
    case Kind::colour: return 68;
    case Kind::app: return row.id == expanded_app() ? 130 : 72;
    case Kind::plugin: return 84;
    case Kind::plugin_setting: {
      const auto it = settings_views().find(row.id);
      return it == settings_views().end() ? 120 : it->second.height;
    }
  }
  return 60;
}

void row_body(vw::Ui& ui, float width, float height, bool first, bool last) {
  const float top = first ? outer_radius : inner_radius;
  const float bottom = last ? outer_radius : inner_radius;
  ui.rectangle({.width = width, .height = height, .color = colours().surface_container,
                .corner_radii = {top, top, bottom, bottom}});
}

// Labels on the left, vertically centred as a block.
void row_label(vw::Ui& ui, const Row& row, float width, float height) {
  const Colours& c = colours();
  if (row.detail.empty()) {
    draw::text_middle(ui, row.label, 20, height / 2, {.size = 15, .color = c.text, .family = font::utility, .max_width = width});
    return;
  }
  draw::text_middle(ui, row.label, 20, height / 2 - 10, {.size = 15, .color = c.text, .family = font::utility, .max_width = width});
  draw::text_middle(ui, row.detail, 20, height / 2 + 11,
                    {.size = 12, .color = c.text_muted, .family = font::utility, .max_width = width});
}

// An M3 switch at the right of a row; `id` keys its animation.
void draw_switch(vw::Ui& ui, const std::string& id, bool on, float width, float height) {
  const Colours& c = colours();
  constexpr float track_width = 52;
  constexpr float track_height = 32;
  const float amount = std::clamp(static_cast<float>(vw::animate("settings.switch." + id, on ? 1 : 0, motion::quick())), 0.0F, 1.0F);
  const float x = width - 20 - track_width;
  const float y = (height - track_height) / 2;
  // M3: outlined when off, filled when on, with a check in the larger thumb.
  ui.rectangle({.width = track_width, .height = track_height, .radius = track_height / 2,
                .color = mix(c.surface_container_highest, c.primary, amount), .border_width = 2,
                .border_color = mix(c.outline, c.primary, amount), .place = {.x = x, .y = y}});
  const float thumb = 16 + 10 * amount;
  const float centre_x = x + 16 + (track_width - 32) * amount;
  draw::rect(ui, centre_x - thumb / 2, y + (track_height - thumb) / 2, thumb, thumb, thumb / 2,
             mix(c.outline, c.primary_text, amount));
  if (amount > 0.5F) draw::glyph(ui, "check", centre_x, y + track_height / 2, 16, c.primary);
}

void draw_toggle(vw::Ui& ui, const Row& row, float width, float height) {
  const Colours& c = colours();
  const auto member = std::get<bool Config::*>(row.field);
  row_label(ui, row, width - 100, height);
  draw_switch(ui, row.label, config().*member, width, height);
  draw::state_layer(ui, 0, 0, width, height, 6, c.text, {.on_click = [member](const vw::ClickEvent&) {
    config().*member = !(config().*member);
    commit(true);
  }});
}

void draw_slider(vw::Ui& ui, const Row& row, float width, float height, float list_x) {
  const Colours& c = colours();
  const float value = get_number(row.field);
  draw::text_middle(ui, row.label, 20, 24, {.size = 15, .color = c.text, .family = font::utility});
  const std::string label = number_label(row, value);
  const float label_width = draw::measure(label, 13, vw::weight::medium, font::utility);
  draw::text_middle(ui, label, width - 20 - label_width, 24,
                    {.size = 13, .color = c.primary, .weight = vw::weight::medium, .family = font::utility});

  // The expressive slider: a bar, a thin handle standing clear of it, the rest.
  constexpr float bar = 24;
  constexpr float gap = 3;
  constexpr float handle = 4;
  const float track_x = 20;
  const float track_width = width - 40;
  const float travel = track_width - handle - gap * 2;
  const float centre_y = height - 28;
  const float fraction = std::clamp((value - row.minimum) / (row.maximum - row.minimum), 0.0F, 1.0F);
  const float filled = travel * fraction;
  const float handle_x = track_x + filled + gap;
  const float rest_x = handle_x + handle + gap;
  const float rest = track_x + track_width - rest_x;
  if (filled >= 1)
    ui.rectangle({.width = filled, .height = bar, .color = c.primary, .corner_radii = {12, 3, 3, 12},
                  .place = {.x = track_x, .y = centre_y - bar / 2}});
  if (rest >= 1)
    ui.rectangle({.width = rest, .height = bar, .color = c.surface_container_highest, .corner_radii = {3, 12, 12, 3},
                  .place = {.x = rest_x, .y = centre_y - bar / 2}});
  draw::rect(ui, handle_x, centre_y - 16, handle, 32, handle / 2, c.primary);
  // The stop indicator at the track's end.
  if (rest > 14) draw::rect(ui, track_x + track_width - 10, centre_y - 2, 4, 4, 2, c.primary);

  // Pointer positions are in surface coordinates, this row is not.
  const auto set = [field = row.field, minimum = row.minimum, maximum = row.maximum, step = row.step,
                    origin = list_x + track_x + gap, travel](double pointer) {
    const float position = std::clamp(static_cast<float>((pointer - origin) / travel), 0.0F, 1.0F);
    const float raw = minimum + (maximum - minimum) * position;
    set_number(field, std::clamp(std::round(raw / step) * step, minimum, maximum));
  };
  const bool on_release = row.on_release;
  ui.hit_area({.width = track_width, .height = 38, .place = {.x = track_x, .y = centre_y - 19},
               .input = {.on_click = [set](const vw::ClickEvent& e) {
                           set(e.x);
                           commit(true);
                         },
                         .on_drag_start = [set](const vw::DragEvent& e) { set(e.x); },
                         .on_drag = [set, on_release](const vw::DragEvent& e) {
                           set(e.x);
                           if (!on_release) commit(false);
                         },
                         .on_drag_end = [](const vw::DragEvent&) { commit(true); }}});
}

// M3 Expressive's connected button group: small inner corners, large outer
// ones, and the selected button rounds off completely.
// Connected chips, one per option; `current` is the selected value.
void draw_chips(vw::Ui& ui, const Row& row, float width, const std::string& current,
                const std::function<void(const std::string&)>& pick) {
  const Colours& c = colours();
  draw::text_middle(ui, row.label, 20, 28, {.size = 15, .color = c.text, .family = font::utility});
  constexpr float gap = 3;
  const float inner = width - 40;
  const std::size_t total = row.options.size();
  for (std::size_t index = 0; index < total; ++index) {
    const Option& option = row.options[index];
    const bool selected = current == option.value;
    const std::size_t column = index % choices_per_line;
    const std::size_t line = index / choices_per_line;
    const std::size_t in_line = std::min<std::size_t>(choices_per_line, total - line * choices_per_line);
    const float cell = (inner - gap * static_cast<float>(in_line - 1)) / static_cast<float>(in_line);
    const float x = 20 + static_cast<float>(column) * (cell + gap);
    const float y = 54 + static_cast<float>(line) * (chip_height + gap);
    const float amount = std::clamp(static_cast<float>(vw::animate(std::format("settings.chip.{}.{}", row.label, index),
                                                                   selected ? 1 : 0, {.duration = 0.3, .easing = vw::Easing::out_back})), 0.0F, 1.2F);
    const float tint = std::min(amount, 1.0F);
    const auto corner = [amount](bool outer) {
      const float resting = outer ? 22.0F : inner_radius;
      return std::max(0.0F, resting + (chip_height / 2 - resting) * amount);
    };
    const bool first_line = line == 0;
    const bool last_line = (line + 1) * choices_per_line >= total;
    const bool first = column == 0;
    const bool last = column + 1 == in_line;
    ui.rectangle({.width = cell, .height = chip_height, .color = mix(c.surface_container_highest, c.primary, tint),
                  .corner_radii = {corner(first_line && first), corner(first_line && last),
                                   corner(last_line && last), corner(last_line && first)},
                  .place = {.x = x, .y = y},
                  .input = {.on_click = [pick, value = option.value](const vw::ClickEvent&) { pick(value); },
                            .hover = vw::Hover{.color = mix(c.surface_container_highest, c.text, 0.12F), .duration = 0.12F}}});
    draw::text_centered(ui, option.label, x + cell / 2, y + chip_height / 2,
                        {.size = 13, .color = mix(c.text, c.primary_text, tint), .weight = vw::weight::medium,
                         .family = font::utility, .max_width = cell - 16});
  }
}

void draw_choice(vw::Ui& ui, const Row& row, float width) {
  const auto member = std::get<std::string Config::*>(row.field);
  draw_chips(ui, row, width, config().*member, [member](const std::string& value) {
    config().*member = value;
    commit(true);
  });
}

void draw_text(vw::Ui& ui, const Row& row, float width) {
  const Colours& c = colours();
  draw::text_middle(ui, row.label, 20, 24, {.size = 15, .color = c.text, .family = font::utility});
  draw::text_middle(ui, row.detail, 20, 44, {.size = 12, .color = c.text_muted, .family = font::utility});
  const auto member = std::get<std::string Config::*>(row.field);
  ui.text_field({.id = field_id(row), .value = config().*member, .width = width - 40, .height = 44, .radius = 16,
                 .border_width = 1, .color = c.surface_container_highest, .border_color = c.surface_container_highest,
                 .focus_color = c.primary, .text_color = c.text, .placeholder_color = c.text_muted,
                 .caret_color = c.primary, .font = font::utility, .size = 14,
                 .on_change = [member](const std::string& value) {
                   config().*member = value;
                   commit(true);
                 },
                 .place = {.x = 20, .y = 60}});
}

void draw_action(vw::Ui& ui, const Row& row, float width, float height) {
  const Colours& c = colours();
  row_label(ui, row, width - 96, height);
  draw::rect(ui, width - 60, (height - 40) / 2, 40, 40, 20, c.secondary_container);
  draw::glyph(ui, row.glyph.empty() ? "chevron_right" : row.glyph, width - 40, height / 2, 20, c.secondary);
  draw::state_layer(ui, 0, 0, width, height, inner_radius, c.text,
                    {.on_click = [run = row.run](const vw::ClickEvent&) { run(); }});
}

std::string& app_filter() {
  static std::string text;
  return text;
}

void draw_filter(vw::Ui& ui, const Row& row, float width, float height) {
  const Colours& c = colours();
  ui.text_field({.id = "settings-app-filter", .placeholder = row.label, .width = width - 40, .height = 44, .radius = 22,
                 .border_width = 1, .color = c.surface_container_highest, .border_color = c.surface_container_highest,
                 .focus_color = c.primary, .text_color = c.text, .placeholder_color = c.text_muted, .caret_color = c.primary,
                 .font = font::utility, .size = 14,
                 .on_change = [](const std::string& value) { app_filter() = value; },
                 .place = {.x = 20, .y = (height - 44) / 2}});
}

// A round toggle: tonal while off, filled while on.
void app_button(vw::Ui& ui, float x, float y, const char* glyph, bool on, std::function<void()> run) {
  const Colours& c = colours();
  constexpr float size = 40;
  ui.rectangle({.width = size, .height = size, .radius = on ? 13.0F : size / 2, .color = on ? c.primary : c.surface_container_highest,
                .place = {.x = x, .y = y},
                .input = {.on_click = [run = std::move(run)](const vw::ClickEvent&) {
                            run();
                            commit(true);
                          },
                          .hover = vw::Hover{.color = mix(on ? c.primary : c.surface_container_highest, c.text, 0.12F), .duration = 0.12F}}});
  draw::glyph(ui, glyph, x + size / 2, y + size / 2, 20, on ? c.primary_text : c.text_muted);
}

void draw_app(vw::Ui& ui, const Row& row, float width) {
  const Colours& c = colours();
  Config& settings = config();
  constexpr float rest = 72;
  const std::string id = row.id;
  const bool hidden = listed(settings.launcher_hidden, id);
  draw::state_layer(ui, 0, 0, width, rest, inner_radius, c.text, {.on_click = [id](const vw::ClickEvent&) {
    expanded_app() = expanded_app() == id ? "" : id;
  }});
  if (!draw::icon(ui, row.glyph, "", 18, 18, 36)) draw::glyph(ui, "apps", 36, 36, 26, c.text_muted);
  const auto custom = settings.launcher_names.find(id);
  const std::string name = custom != settings.launcher_names.end() && !custom->second.empty() ? custom->second : row.label;
  const float text_width = width - 68 - 3 * 44 - 16;
  draw::text_middle(ui, name, 68, 26, {.size = 15, .color = hidden ? c.text_muted : c.text, .family = font::utility, .max_width = text_width});
  const auto usage = settings.launcher_usage.find(id);
  draw::text_middle(ui, usage == settings.launcher_usage.end() ? row.label : std::format("Opened {} time{}", usage->second.count, usage->second.count == 1 ? "" : "s"),
                    68, 47, {.size = 12, .color = c.text_muted, .family = font::utility, .max_width = text_width});
  const float y = (rest - 40) / 2;
  const float x = width - 16 - 3 * 40 - 2 * 4;
  app_button(ui, x, y, "star", listed(settings.launcher_favorites, id), [id] {
    toggle_listed(config().launcher_favorites, id);
    std::erase(config().launcher_hidden, id);
  });
  app_button(ui, x + 44, y, "north", listed(settings.launcher_preferred, id), [id] {
    toggle_listed(config().launcher_preferred, id);
    std::erase(config().launcher_hidden, id);
  });
  app_button(ui, x + 88, y, "visibility_off", hidden, [id] {
    toggle_listed(config().launcher_hidden, id);
    std::erase(config().launcher_favorites, id);
    std::erase(config().launcher_preferred, id);
  });
  if (expanded_app() != id) return;
  ui.text_field({.id = "rename." + id, .value = custom == settings.launcher_names.end() ? "" : custom->second,
                 .placeholder = "Custom name for " + row.label, .width = width - 40, .height = 44, .radius = 16,
                 .border_width = 1, .color = c.surface_container_highest, .border_color = c.surface_container_highest,
                 .focus_color = c.primary, .text_color = c.text, .placeholder_color = c.text_muted, .caret_color = c.primary,
                 .font = font::utility, .size = 14,
                 .on_change = [id](const std::string& value) {
                   if (value.empty()) config().launcher_names.erase(id);
                   else config().launcher_names[id] = value;
                   commit(true);
                 },
                 .place = {.x = 20, .y = rest + 2}});
}

std::vector<std::string>& bar_zone(int zone) {
  Config& c = config();
  return zone == 0 ? c.bar_left : zone == 1 ? c.bar_center : c.bar_right;
}

// Which zone holds a module: 0 left, 1 centre, 2 right, 3 off.
int zone_of(const std::string& id) {
  for (int zone = 0; zone < 3; ++zone)
    if (listed(bar_zone(zone), id)) return zone;
  return 3;
}

void place_module(const std::string& id, int target) {
  for (int zone = 0; zone < 3; ++zone) std::erase(bar_zone(zone), id);
  if (target < 3) bar_zone(target).push_back(id);
  commit(true);
}

void shift_module(const std::string& id, int by) {
  const int zone = zone_of(id);
  if (zone == 3) return;
  auto& list = bar_zone(zone);
  const auto index = std::ranges::find(list, id) - list.begin();
  const auto other = index + by;
  if (other < 0 || other >= static_cast<std::ptrdiff_t>(list.size())) return;
  std::swap(list[static_cast<std::size_t>(index)], list[static_cast<std::size_t>(other)]);
  commit(true);
}

// A module's zone as a connected group, and arrows for its place in that zone.
void draw_module(vw::Ui& ui, const Row& row, float width, float height) {
  const Colours& c = colours();
  const int current = zone_of(row.id);
  draw::text_middle(ui, row.label, 20, height / 2, {.size = 15, .color = c.text, .family = font::utility, .max_width = width - 330});
  constexpr float segment = 58;
  constexpr float segment_height = 38;
  constexpr float gap = 2;
  constexpr const char* labels[] = {"Left", "Centre", "Right", "Off"};
  const float group_x = width - 20 - 2 * 34 - 8 - 4 * segment - 3 * gap;
  const float y = (height - segment_height) / 2;
  for (int zone = 0; zone < 4; ++zone) {
    const bool on = zone == current;
    const float x = group_x + static_cast<float>(zone) * (segment + gap);
    const float left = on ? segment_height / 2 : zone == 0 ? 19.0F : inner_radius;
    const float right = on ? segment_height / 2 : zone == 3 ? 19.0F : inner_radius;
    ui.rectangle({.width = segment, .height = segment_height, .color = on ? c.primary : c.surface_container_highest,
                  .corner_radii = {left, right, right, left}, .place = {.x = x, .y = y},
                  .input = {.on_click = [id = row.id, zone](const vw::ClickEvent&) { place_module(id, zone); },
                            .hover = vw::Hover{.color = mix(on ? c.primary : c.surface_container_highest, c.text, 0.1F), .duration = 0.12F}}});
    draw::text_centered(ui, labels[zone], x + segment / 2, y + segment_height / 2,
                        {.size = 12, .color = on ? c.primary_text : c.text, .weight = vw::weight::medium, .family = font::utility});
  }
  const auto arrow = [&](float x, const char* glyph, int by) {
    const vw::Color tint = current == 3 ? c.outline_variant : c.text;
    draw::glyph(ui, glyph, x + 17, height / 2, 20, tint);
    draw::state_layer(ui, x, (height - 34) / 2, 34, 34, 17, c.text,
                      {.on_click = [id = row.id, by](const vw::ClickEvent&) { shift_module(id, by); }});
  };
  arrow(width - 20 - 2 * 34 - 2, "arrow_back", -1);
  arrow(width - 20 - 34, "arrow_forward", 1);
}

// A role's colour, and a hex field that overrides it while filled in.
void draw_colour(vw::Ui& ui, const Row& row, float width, float height) {
  const Colours& c = colours();
  const auto override_value = config().colour_overrides.find(row.id);
  const bool custom = override_value != config().colour_overrides.end();
  constexpr float swatch = 36;
  ui.rectangle({.width = swatch, .height = swatch, .radius = custom ? 12.0F : swatch / 2, .color = role_colour(row.id),
                .border_width = 1, .border_color = c.outline_variant, .place = {.x = 18, .y = (height - swatch) / 2}});
  draw::text_middle(ui, row.label, 68, height / 2, {.size = 15, .color = c.text, .family = font::utility, .max_width = width - 240});
  constexpr float field = 132;
  ui.text_field({.id = "colour." + row.id, .value = custom ? override_value->second : "", .placeholder = "From theme",
                 .width = field, .height = 40, .radius = 14, .border_width = 1, .color = c.surface_container_highest,
                 .border_color = custom ? c.primary : c.surface_container_highest, .focus_color = c.primary,
                 .text_color = c.text, .placeholder_color = c.text_muted, .caret_color = c.primary, .font = font::utility, .size = 13,
                 .on_change = [id = row.id](const std::string& value) {
                   if (value.empty()) config().colour_overrides.erase(id);
                   else config().colour_overrides[id] = value;
                   commit(true);
                 },
                 .place = {.x = width - 20 - field, .y = (height - 40) / 2}});
}

// --- Plugins -------------------------------------------------------------------

// The plugin, its version and what it does, with a switch to run it.
void draw_plugin(vw::Ui& ui, const Row& row, float width, float height) {
  const Colours& c = colours();
  const bool usable = row.problem.empty();
  const bool on = listed(config().plugins_enabled, row.id);
  const float text_width = width - (usable ? 110 : 40);
  draw::text_middle(ui, row.label, 20, height / 2 - 20, {.size = 15, .color = c.text, .family = font::utility, .max_width = text_width});
  draw::text_middle(ui, row.detail, 20, height / 2 + 1,
                    {.size = 12, .color = c.text_muted, .family = font::utility, .max_width = text_width});
  draw::text_middle(ui, usable ? row.glyph : row.problem, 20, height / 2 + 21,
                    {.size = 12, .color = usable ? c.text_muted : c.error, .family = font::utility, .max_width = text_width});
  if (!usable) return;
  draw_switch(ui, "plugin." + row.id, on, width, height);
  draw::state_layer(ui, 0, 0, width, height, 6, c.text, {.on_click = [id = row.id](const vw::ClickEvent&) {
    toggle_listed(config().plugins_enabled, id);
    commit(true);
  }});
}

// Loads (or reloads, when the file changed) a plugin's Facet file; nullptr
// when it has none, it doesn't parse, or it has no Settings view.
SettingsView* settings_view(const packages::Manifest& m) {
  if (m.facet.empty()) return nullptr;
  auto& views = settings_views();
  auto it = views.find(m.id);
  try {
    if (it == views.end()) {
      SettingsView view{.plugin = std::make_unique<facet::Plugin>(facet::Plugin::load(m.directory / m.facet)), .id = m.id};
      plugins::register_components(*view.plugin);
      it = views.emplace(m.id, std::move(view)).first;
    } else {
      it->second.plugin->reload();
    }
  } catch (const std::exception&) {
    return nullptr;
  }
  const auto names = it->second.plugin->views();
  return std::ranges::find(names, "Settings") != names.end() ? &it->second : nullptr;
}

facet::Value settings_actions(const std::string& id) {
  facet::Value::Object actions;
  actions.emplace("get", facet::Value([id](facet::Arguments arguments) -> facet::Value {
    const facet::Value fallback = arguments.size() > 1 ? arguments[1] : facet::Value();
    if (arguments.empty()) return fallback;
    const nlohmann::json store = plugins::load_store(id);
    const auto it = store.find(arguments[0].to_string());
    return it == store.end() || it->is_null() ? fallback : plugins::to_value(*it);
  }));
  actions.emplace("set", facet::Value([id](facet::Arguments arguments) -> facet::Value {
    if (arguments.empty()) return facet::Value();
    nlohmann::json store = plugins::load_store(id);
    store[arguments[0].to_string()] = arguments.size() > 1 ? plugins::to_json(arguments[1]) : nlohmann::json(nullptr);
    plugins::save_store(id, store);
    return facet::Value();
  }));
  // The rest only make sense in the shell: here they do nothing.
  for (const char* name : {"send", "refresh", "log", "run", "notify", "spawn", "open"})
    actions.emplace(name, facet::Value([](facet::Arguments) { return facet::Value(); }));
  return facet::Value(std::move(actions));
}

void draw_plugin_setting(vw::Ui& ui, const Row& row, float width, float) {
  auto& views = settings_views();
  const auto it = views.find(row.id);
  if (it == views.end()) return;
  facet::Value::Object state{{"store", plugins::to_value(plugins::load_store(row.id))},
                             {"backend", facet::Value(facet::Value::Object{})},
                             {"colours", plugins::colour_values()},
                             {"fonts", plugins::font_values()},
                             {"width", facet::Value(width - 40)},
                             {"plugin", plugins::to_value({{"id", row.id}})}};
  try {
    const facet::Value arguments[] = {facet::Value(std::move(state)), settings_actions(row.id)};
    const vw::Widget widget = it->second.plugin->view(ui, "Settings", arguments);
    ui.row({widget}, {.place = {.x = 20, .y = 16}});
    // Its height decides the row's next time round.
    if (std::abs(widget.height + 32 - it->second.height) > 0.5F) {
      it->second.height = widget.height + 32;
      vw::request_animation_continuation();
    }
  } catch (const std::exception& error) {
    draw::text(ui, error.what(), 20, 16, {.size = 12, .color = colours().error, .max_width = width - 40, .max_lines = 3});
  }
}

// "<plugin id>:<view>" for every view the running shell's plugins have.
std::vector<std::string> running_views() {
  std::vector<std::string> lines;
  std::istringstream reply(ipc::request("plugin views"));
  for (std::string line; std::getline(reply, line);)
    if (line.find(':') != std::string::npos && !line.starts_with("error")) lines.push_back(line);
  return lines;
}

// One group per installed plugin: the plugin, then (while it's on) its bar
// modules and settings. Rescanned when the config changes and every few
// seconds, so installs and removals show up.
const std::vector<Row>& plugin_rows(const Page& page) {
  static std::vector<Row> rows;
  static std::uint64_t generation = ~0ULL;
  static Clock::time_point scanned{};
  if (generation == config_generation() && Clock::now() - scanned < std::chrono::seconds(3)) return rows;
  generation = config_generation();
  scanned = Clock::now();
  rows = page.rows;
  const auto installed = packages::installed(packages::Kind::plugin);
  for (const packages::Manifest& m : installed) {
    std::string problem;
    if (!m.valid()) problem = "Broken: " + (m.errors.empty() ? std::string("unreadable") : m.errors.front());
    else if (const auto why = packages::incompatibility(m)) problem = "Won't run here: " + *why;
    std::string permissions;
    for (const std::string& permission : m.permissions) permissions += (permissions.empty() ? "" : ", ") + permission;
    rows.push_back(header(m.name.empty() ? m.id : m.name));
    rows.push_back({.kind = Kind::plugin,
                    .label = m.description.empty() ? m.id : m.description,
                    .detail = std::format("Version {} · {}", m.version, m.id),
                    .glyph = permissions.empty() ? "No permissions" : "Permissions: " + permissions,
                    .id = m.id,
                    .problem = problem});
    if (!problem.empty() || !listed(config().plugins_enabled, m.id)) continue;
    // Its views, from the running shell (Facet and C++ alike).
    for (const std::string& line : running_views())
      if (line.starts_with(m.id + ":") && line.substr(m.id.size() + 1).starts_with("Bar"))
        rows.push_back({.kind = Kind::module, .label = line.substr(m.id.size() + 1) + " in the bar", .id = line});
    if (settings_view(m) != nullptr) rows.push_back({.kind = Kind::plugin_setting, .label = "Settings", .id = m.id});
  }
  rows.push_back(header("More"));
  rows.push_back(action("Open plugins folder",
                        installed.empty() ? "None installed yet. Install with: blxshell install file.blx"
                                          : "Install more with: blxshell install file.blx",
                        "folder_open", [] {
                          std::error_code error;
                          std::filesystem::create_directories(packages::user_directory(packages::Kind::plugin), error);
                          vw::spawn({"xdg-open", packages::user_directory(packages::Kind::plugin).string()});
                        }));
  return rows;
}

bool joins_above(const std::vector<Row>& rows, std::size_t index) {
  return index > 0 && rows[index - 1].kind != Kind::header;
}

bool joins_below(const std::vector<Row>& rows, std::size_t index) {
  return index + 1 < rows.size() && rows[index + 1].kind != Kind::header;
}

void draw_row(vw::Ui& ui, const std::vector<Row>& rows, std::size_t index, float width, float list_x) {
  const Row& row = rows[index];
  const float height = height_of(row);
  if (row.kind == Kind::header) {
    draw::text_middle(ui, row.label, 20, height - 14,
                      {.size = 13, .color = colours().primary, .weight = vw::weight::semibold, .family = font::utility});
    return;
  }
  row_body(ui, width, height, !joins_above(rows, index), !joins_below(rows, index));
  switch (row.kind) {
    case Kind::toggle: draw_toggle(ui, row, width, height); break;
    case Kind::slider: draw_slider(ui, row, width, height, list_x); break;
    case Kind::choice: draw_choice(ui, row, width); break;
    case Kind::text: draw_text(ui, row, width); break;
    case Kind::action: draw_action(ui, row, width, height); break;
    case Kind::filter: draw_filter(ui, row, width, height); break;
    case Kind::app: draw_app(ui, row, width); break;
    case Kind::module: draw_module(ui, row, width, height); break;
    case Kind::colour: draw_colour(ui, row, width, height); break;
    case Kind::plugin: draw_plugin(ui, row, width, height); break;
    case Kind::plugin_setting: draw_plugin_setting(ui, row, width, height); break;
    case Kind::header: break;
  }
}

// --- Window --------------------------------------------------------------------

int& selected_page() {
  static int page = 0;
  return page;
}

// The page's rows; the Apps page ends with one row per matching app.
const std::vector<Row>& page_rows(const Page& page) {
  if (page.lists_plugins) return plugin_rows(page);
  if (!page.lists_apps) return page.rows;
  static const std::vector<vw::applications::Application> apps = [] {
    auto list = vw::applications::search("", 0);
    const auto key = [](const auto& app) {
      std::string name = app.display_name;
      std::ranges::transform(name, name.begin(), [](unsigned char c) { return std::tolower(c); });
      return name;
    };
    std::ranges::stable_sort(list, [&](const auto& a, const auto& b) { return key(a) < key(b); });
    return list;
  }();
  static std::vector<Row> rows;
  static std::string filtered = "\x01";
  if (app_filter() != filtered) {
    filtered = app_filter();
    std::string needle = filtered;
    std::ranges::transform(needle, needle.begin(), [](unsigned char c) { return std::tolower(c); });
    rows = page.rows;
    for (const auto& app : apps) {
      std::string name = app.display_name;
      std::ranges::transform(name, name.begin(), [](unsigned char c) { return std::tolower(c); });
      if (!needle.empty() && name.find(needle) == std::string::npos) continue;
      rows.push_back({.kind = Kind::app, .label = app.display_name, .glyph = app.icon_path, .id = app.id});
    }
  }
  return rows;
}

// The tab list: a page index, or -1 for the space between two groups.
const std::vector<int>& tab_items() {
  static const std::vector<int> items = [] {
    std::vector<int> result;
    const auto& list = pages();
    for (std::size_t index = 0; index < list.size(); ++index) {
      if (index > 0 && list[index].group != list[index - 1].group) result.push_back(-1);
      result.push_back(static_cast<int>(index));
    }
    return result;
  }();
  return items;
}

constexpr float tab_height = 72;

// One tab of a group. Its corners depend on where it sits in the group, and
// selecting it rounds them all off.
void draw_tab(vw::Ui& ui, std::size_t item, float width) {
  const Colours& c = colours();
  const auto& items = tab_items();
  const int index = items[item];
  const Page& page = pages()[static_cast<std::size_t>(index)];
  const bool first = item == 0 || items[item - 1] < 0;
  const bool last = item + 1 == items.size() || items[item + 1] < 0;
  const bool active = index == selected_page();
  const float amount = std::clamp(static_cast<float>(vw::animate(std::format("settings.tab.{}", index), active ? 1 : 0,
                                                                 {.duration = 0.32, .easing = vw::Easing::out_back})), 0.0F, 1.2F);
  const float tint = std::min(amount, 1.0F);
  const auto corner = [amount](bool outer) {
    const float resting = outer ? outer_radius : inner_radius;
    return std::max(0.0F, resting + (outer_radius - resting) * amount);
  };
  ui.rectangle({.width = width, .height = tab_height, .color = mix(c.surface_container, c.secondary_container, tint),
                .corner_radii = {corner(first), corner(first), corner(last), corner(last)},
                .input = {.on_click = [index](const vw::ClickEvent&) {
                            selected_page() = index;
                            vw::set_scroll_offset(list_id, 0);
                          },
                          .hover = active ? std::nullopt
                                          : std::optional(vw::Hover{.color = c.surface_container_high, .duration = 0.12F})}});
  const Tone colours_of = tone(page.tone);
  // The icon's disc squares off on the selected tab, like an active tile.
  const float disc = 20 + (13 - 20) * tint;
  draw::rect(ui, 14, 16, 40, 40, disc, active ? colours_of.content : colours_of.container);
  draw::glyph(ui, page.glyph, 34, 36, 22, active ? colours_of.container : colours_of.content);
  draw::text_middle(ui, page.title, 68, 26, {.size = 15, .color = c.text, .weight = vw::weight::medium,
                                             .family = font::utility, .max_width = width - 84});
  draw::text_middle(ui, page.summary, 68, 47, {.size = 12, .color = c.text_muted, .family = font::utility,
                                               .max_width = width - 84});
}

void draw_tabs(vw::Ui& ui, float height) {
  const Colours& c = colours();
  draw::text(ui, "Settings", 28, 26, {.size = 30, .color = c.text, .weight = vw::weight::semibold, .family = font::display});
  constexpr float list_x = 14;
  constexpr float list_y = 86;
  const float list_width = pane_width - list_x * 2;
  const auto& items = tab_items();
  ui.listview({.id = tabs_id, .width = list_width, .height = std::max(1.0F, height - list_y), .gap = row_gap, .scroll_step = 90,
               .count = items.size(),
               .render = [&](std::size_t item) -> std::optional<vw::Widget> {
                 if (items[item] < 0) return std::nullopt;
                 return draw::capture(ui, list_width, tab_height, [&] { draw_tab(ui, item, list_width); });
               },
               .row_height = [&items](std::size_t item) { return items[item] < 0 ? group_gap - row_gap * 2 : tab_height; },
               .place = {.x = list_x, .y = list_y}});
}

void build(vw::Ui& ui) {
  const Colours& c = colours();
  ui.toplevel({.width = 1080, .height = 760, .title = "Shell Settings", .app_id = "blxshell-settings"});
  const vw::Size size = vw::toplevel_size();
  const float width = size.width > 0 ? static_cast<float>(size.width) : 1080;
  const float height = size.height > 0 ? static_cast<float>(size.height) : 760;
  ui.rectangle({.width = width, .height = height, .color = c.surface});
  draw_tabs(ui, height);

  const Page& page = pages()[static_cast<std::size_t>(selected_page())];
  const float available = width - pane_width - content_padding * 2;
  const float list_width = std::min(available, content_limit);
  const float list_x = pane_width + content_padding + (available - list_width) / 2;
  draw::text(ui, page.title, list_x + 8, 26, {.size = 30, .color = c.text, .weight = vw::weight::semibold, .family = font::display});

  // Back to this page's defaults: a tonal button. The Plugins page has no
  // defaults of its own, so none there.
  constexpr float reset_width = 104;
  constexpr float reset_height = 40;
  const float reset_x = list_x + list_width - reset_width;
  if (!page.lists_plugins) {
    ui.rectangle({.width = reset_width, .height = reset_height, .radius = reset_height / 2, .color = c.secondary_container,
                  .place = {.x = reset_x, .y = 26},
                  .input = {.on_click = [&page](const vw::ClickEvent&) {
                              for (const Row& row : page.rows) reset(row);
                              if (page.on_reset) page.on_reset();
                              commit(true);
                            },
                            .hover = vw::Hover{.color = mix(c.secondary_container, c.text, 0.08F), .duration = 0.12F}}});
    draw::glyph(ui, "restart_alt", reset_x + 24, 46, 18, c.text);
    draw::text_middle(ui, "Reset", reset_x + 40, 46, {.size = 13, .color = c.text, .weight = vw::weight::medium, .family = font::utility});
  }

  constexpr float list_y = 80;
  const std::vector<Row>& rows = page_rows(page);
  ui.listview({.id = list_id, .width = list_width, .height = std::max(1.0F, height - list_y), .gap = row_gap, .scroll_step = 90,
               .count = rows.size(),
               .render = [&, list_x](std::size_t index) -> std::optional<vw::Widget> {
                 return draw::capture(ui, list_width, height_of(rows[index]),
                                      [&] { draw_row(ui, rows, index, list_width, list_x); });
               },
               .row_height = [&rows](std::size_t index) { return height_of(rows[index]); },
               .place = {.x = list_x, .y = list_y}});
}

}  // namespace

int run_settings(std::string_view page) {
  if (!page.empty()) {
    const auto& list = pages();
    for (std::size_t index = 0; index < list.size(); ++index) {
      std::string title = list[index].title;
      std::ranges::transform(title, title.begin(), [](unsigned char c) { return std::tolower(c); });
      if (title == page || title.starts_with(page)) {
        selected_page() = static_cast<int>(index);
        break;
      }
    }
  }
  // One window. A second run takes over: the open one may be on another
  // workspace, or an older build, so it closes and this one opens where
  // you are. Settings save as they change, so nothing is lost.
  const char* runtime = std::getenv("XDG_RUNTIME_DIR");
  const std::string lock = std::string(runtime != nullptr ? runtime : "/tmp") + "/blxshell-settings.lock";
  const int fd = open(lock.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
  if (fd >= 0 && flock(fd, LOCK_EX | LOCK_NB) != 0) {
    char text[32] = {};
    const ssize_t got = pread(fd, text, sizeof text - 1, 0);
    const pid_t other = got > 0 ? static_cast<pid_t>(std::atol(text)) : 0;
    if (other > 0) kill(other, SIGTERM);
    // Wait up to a second for it to let go.
    bool locked = false;
    for (int attempt = 0; attempt < 20 && !(locked = flock(fd, LOCK_EX | LOCK_NB) == 0); ++attempt) usleep(50'000);
    if (!locked) return EXIT_SUCCESS;
  }
  if (fd >= 0) {
    const std::string pid = std::to_string(getpid()) + "\n";
    if (ftruncate(fd, 0) == 0 && pwrite(fd, pid.data(), pid.size(), 0) < 0) {}
  }

  sync_style();
  vw::App app(build);
  app.on_event(colours_event_fd(), reload_colours)
      .on_event(config_event_fd(), [] {
        // Our own writes echo back here, older than what is on screen.
        const bool ours = Clock::now() - last_save() < std::chrono::milliseconds(400);
        const bool changed = reload_config(!ours);
        if (!ours) sync_style();
        return changed && !ours;
      });
  return app.run();
}

}  // namespace blx
