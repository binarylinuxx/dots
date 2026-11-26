#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// ~/.config/blxshell/config.json, same schema as ~/shell's Services/Config.
namespace blx {

struct AppUsage {
  int count = 0;
  std::int64_t last = 0;  // Unix seconds
};

struct Config {
  int launcher_width = 700;
  bool launcher_tint_app_icons = false;
  // How the app list is ordered with an empty search: "name", "used", "recent".
  std::string launcher_sort = "name";
  bool launcher_favorites_first = true;
  bool launcher_show_comments = true;
  bool launcher_show_favorite_star = true;
  bool launcher_search_comments = true;
  bool launcher_boost_favorites = true;
  bool launcher_record_usage = true;
  bool launcher_close_on_launch = true;
  int launcher_row_height = 54;
  int launcher_icon_size = 30;
  int launcher_max_height = 550;
  int launcher_sink = 18;  // px lowered into the bar
  // Desktop entry ids. Favourites top the list, preferred apps win searches.
  std::vector<std::string> launcher_favorites;
  std::vector<std::string> launcher_preferred;
  std::vector<std::string> launcher_hidden;
  std::map<std::string, std::string> launcher_names;  // id -> custom name
  std::map<std::string, AppUsage> launcher_usage;
  float wallpaper_launcher_width_ratio = 0.64F;
  int wallpaper_launcher_max_width = 1120;
  int wallpaper_item_gap = 16;  // between the carousel's cards
  int wallpaper_carousel_height = 145;
  std::string wallpaper_path;
  int osd_slider_minimum_height = 120;
  int osd_slider_maximum_height = 240;
  float osd_slider_height_ratio = 0.22F;
  float power_menu_height_ratio = 0.1F;
  int power_menu_minimum_button_size = 68;
  int power_menu_maximum_button_size = 124;
  int rail_bottom_gap = 0;
  bool bar_tint_tray_icons = true;
  // Module ids in each zone, in order; a module in none of them is off.
  std::vector<std::string> bar_left{"os", "workspaces"};
  std::vector<std::string> bar_center{"window"};
  std::vector<std::string> bar_right{"tray", "keyboard", "clock", "status", "power"};
  // "runix" is the logo built into blxshell; anything else is an icon theme name.
  std::string bar_os_icon = "runix";
  std::string bar_clock_format = "%H:%M";  // strftime
  int bar_clock_size = 16;
  std::string bar_workspace_style = "apps";  // "apps", "dots", "numbers"
  bool bar_pills = true;
  int bar_title_width = 320;
  int bar_tray_size = 22;
  bool bar_status_wifi = true;
  bool bar_status_bluetooth = true;
  bool bar_status_audio = true;
  bool bar_status_battery = true;
  bool bar_status_quickshare = true;
  bool bar_status_widgets = true;
  bool sidebar_show_media = true;
  bool sidebar_show_brightness = true;
  bool sidebar_show_keep_awake = true;
  bool sidebar_show_recorder = true;
  int sidebar_width = 380;
  int sidebar_tile_height = 58;
  float sidebar_tile_radius = 0.29F;  // of the tile height, while on
  int sidebar_pull_area = 64;
  bool sidebar_tile_wifi = true;
  bool sidebar_tile_bluetooth = true;
  bool sidebar_tile_mic = true;
  bool sidebar_tile_dnd = true;
  bool sidebar_tile_share = true;
  bool sidebar_tile_settings = true;
  bool sidebar_tile_lock = true;
  bool island_enabled = true;
  bool dashboard_enabled = true;
  bool notification_popups = true;
  std::string notification_side = "right";
  int notification_width = 390;
  int notification_max = 5;
  bool notification_images = true;
  int island_width = 210;
  int island_expanded_width = 360;
  int island_announce_seconds = 5;
  int dashboard_tab = 0;
  bool dashboard_remember_tab = true;
  std::string weather_unit = "celsius";
  int osd_hold_ms = 1800;
  // Power menu; an empty command keeps the default.
  bool power_lock = false;
  bool power_logout = false;
  bool power_suspend = true;
  bool power_hibernate = true;
  bool power_restart = true;
  bool power_firmware = true;
  bool power_off = true;
  std::string power_lock_command;
  std::string power_logout_command;
  std::string power_suspend_command;
  std::string power_hibernate_command;
  std::string power_restart_command;
  std::string power_firmware_command;
  std::string power_off_command;
  std::string lock_clock_format = "%H:%M";
  std::string lock_date_format = "%A, %-d %B";
  int lock_clock_size = 132;
  float lock_tint = 0.45F;  // surface colour over the blurred wallpaper
  // Dual Kawase blur of the lock wallpaper: more passes blur wider, the
  // offset spreads each pass's taps.
  int lock_blur_passes = 4;
  float lock_blur_offset = 2.0F;
  bool lock_show_user = true;
  std::string lock_clock_style = "stacked";  // or "single"
  bool lock_show_status = true;
  bool lock_show_weather = true;
  bool lock_show_notifications = true;
  bool lock_show_media = true;
  std::string background_clock_format = "%H:%M";
  std::string background_date_format = "%A, %-d %B";
  int background_clock_size = 68;
  float background_widget_opacity = 0.78F;
  // The OSD comes out on a tug; this also shows it when the volume changes.
  bool osd_on_change = false;
  std::string matugen_mode = "dark";
  std::string matugen_scheme = "scheme-tonal-spot";
  float matugen_contrast = 0;
  std::string matugen_prefer = "saturation";
  std::string weather_location;
  bool prefer_weather_location_from_config = true;
  bool wallpaper_parallax = true;
  float wallpaper_parallax_strength = 0.06F;
  // How the wallpaper appears on launch and after unlocking, and whether the
  // lock screen animates in: "iris", "fade", "rise" or "none".
  std::string wallpaper_transition = "iris";
  bool background_clock_enabled = true;
  bool background_weather_enabled = true;
  float background_clock_x = 0.04F;
  float background_clock_y = 0.84F;
  float background_weather_x = 0.77F;
  float background_weather_y = 0.87F;
  // How much of the surface colour covers what is behind it: the frosted
  // wallpaper, or with that off the desktop (and shadows go).
  float global_opacity = 0.8F;
  bool frosted = false;
  int frosted_strength = 5;
  float frame_thickness = 12;
  float frame_radius = 25;
  float bar_height = 48;
  float blob_smoothing = 30;
  float panel_radius = 14;
  float island_radius = 24;
  float launcher_radius = 36;
  float animation_speed = 1;
  std::string motion_style = "bounce";  // "bounce", "smooth", "snappy"
  int hover_linger_ms = 180;
  float font_scale = 1;
  float card_radius = 16;
  bool shadow_enabled = true;
  float shadow_strength = 0.65F;
  int shadow_blur = 34;
  // "wallpaper", or "colour" to build the palette from theme_seed.
  std::string theme_source = "wallpaper";
  std::string theme_seed = "#6750a4";
  std::map<std::string, std::string> colour_overrides;  // role -> #rrggbb
  // Roles taken out of the generated scheme: "generated" (none), "neutral"
  // (surfaces, text, outlines), "amoled" (those, with black or white
  // surfaces) or "mono" (everything but error).
  std::string colour_preset = "generated";
  // The shell's language: "auto" follows LANG; "en", "ru"... pick one.
  std::string language = "auto";
  std::string font_display = "Rubik";
  std::string font_utility = "Google Sans Flex";
  // Theme tint on icons (where enabled), and on album art.
  float icon_tint = 1;
  float art_tint = 0;
  float art_hue = 0;
  // The applied theme's id; the theme's values themselves are in here.
  std::string theme;
  // GTK 3/4 and Qt apps in the shell's colours and Material 3 shapes
  // (apptheme.hpp).
  bool theme_gtk_apps = false;
  bool theme_qt_apps = false;
  // Plugin ids that run. Installed plugins stay off until listed.
  std::vector<std::string> plugins_enabled;
  // Extra package repositories, after the official ones: "owner/name" on
  // GitHub, or the URL of an index.json.
  std::vector<std::string> plugin_repositories;
  bool sdf_outline_enabled = false;
  float sdf_outline_width = 1;
  std::string sdf_outline_color_role = "outline";

  // Not persisted.
  bool background_widgets_editing = false;

  // Matugen's -t and --contrast for the chosen scheme (Config.schemeType()).
  std::string scheme_type() const;
  float scheme_contrast() const;
};

Config& config();
// Writes the file; the watcher then reloads the same values.
void save_config();
// Bumped whenever the values change, for caches built from them.
std::uint64_t config_generation();
bool listed(const std::vector<std::string>& list, const std::string& id);
// Adds id, or removes it when already there.
void toggle_listed(std::vector<std::string>& list, const std::string& id);
std::string config_path();
// Readable when the config directory changes; pass to vw::App::on_event.
int config_event_fd();
// Drains the watcher; with apply false the file is left unread.
bool reload_config(bool apply = true);

}  // namespace blx
