#include "config.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sys/inotify.h>
#include <unistd.h>

namespace blx {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

fs::path directory() {
  const char* home = std::getenv("HOME");
  return fs::path(home != nullptr ? home : ".") / ".config/blxshell";
}

template <typename T>
void read(const json& object, const char* key, T& target) {
  if (!object.is_object() || !object.contains(key)) return;
  try {
    target = object.at(key).get<T>();
  } catch (const std::exception&) {
  }
}

json section(const json& object, const char* key) {
  return object.is_object() && object.contains(key) && object.at(key).is_object() ? object.at(key)
                                                                                    : json::object();
}

void apply(Config& c, const json& root) {
  const json launcher = section(root, "launcher");
  const json wallpaper = section(launcher, "wallpaper");
  const json bar = section(root, "bar");
  const json rail = section(bar, "rail");
  const json audio = section(section(root, "osd"), "audio");
  const json button = section(section(root, "powerMenu"), "button");
  const json matugen = section(root, "matugen");
  const json background = section(root, "background");
  const json appearance = section(root, "appearance");
  const json weather = section(root, "weather");
  read(section(root, "bar"), "left", c.bar_left);
  read(section(root, "bar"), "center", c.bar_center);
  read(section(root, "bar"), "right", c.bar_right);
  read(section(root, "bar"), "osIcon", c.bar_os_icon);
  read(section(root, "bar"), "clockFormat", c.bar_clock_format);
  read(section(root, "bar"), "clockSize", c.bar_clock_size);
  read(section(root, "bar"), "workspaceStyle", c.bar_workspace_style);
  read(section(root, "bar"), "pills", c.bar_pills);
  read(section(root, "bar"), "titleWidth", c.bar_title_width);
  read(section(root, "bar"), "traySize", c.bar_tray_size);
  read(section(root, "sidebar"), "width", c.sidebar_width);
  read(section(root, "sidebar"), "tileHeight", c.sidebar_tile_height);
  read(section(root, "sidebar"), "tileRadius", c.sidebar_tile_radius);
  read(section(root, "sidebar"), "pullArea", c.sidebar_pull_area);
  read(section(root, "sidebar"), "tileWifi", c.sidebar_tile_wifi);
  read(section(root, "sidebar"), "tileBluetooth", c.sidebar_tile_bluetooth);
  read(section(root, "sidebar"), "tileMic", c.sidebar_tile_mic);
  read(section(root, "sidebar"), "tileDnd", c.sidebar_tile_dnd);
  read(section(root, "sidebar"), "tileShare", c.sidebar_tile_share);
  read(section(root, "sidebar"), "tileSettings", c.sidebar_tile_settings);
  read(section(root, "sidebar"), "tileLock", c.sidebar_tile_lock);
  read(section(root, "notifications"), "side", c.notification_side);
  read(section(root, "notifications"), "width", c.notification_width);
  read(section(root, "notifications"), "max", c.notification_max);
  read(section(root, "notifications"), "images", c.notification_images);
  read(section(root, "island"), "width", c.island_width);
  read(section(root, "island"), "expandedWidth", c.island_expanded_width);
  read(section(root, "island"), "announceSeconds", c.island_announce_seconds);
  read(section(root, "dashboard"), "tab", c.dashboard_tab);
  read(section(root, "dashboard"), "rememberTab", c.dashboard_remember_tab);
  read(section(root, "weather"), "unit", c.weather_unit);
  read(section(root, "osd"), "holdMs", c.osd_hold_ms);
  read(section(root, "powerMenu"), "lock", c.power_lock);
  read(section(root, "powerMenu"), "logout", c.power_logout);
  read(section(root, "powerMenu"), "suspend", c.power_suspend);
  read(section(root, "powerMenu"), "hibernate", c.power_hibernate);
  read(section(root, "powerMenu"), "restart", c.power_restart);
  read(section(root, "powerMenu"), "firmware", c.power_firmware);
  read(section(root, "powerMenu"), "off", c.power_off);
  read(section(root, "powerMenu"), "lockCommand", c.power_lock_command);
  read(section(root, "powerMenu"), "logoutCommand", c.power_logout_command);
  read(section(root, "powerMenu"), "suspendCommand", c.power_suspend_command);
  read(section(root, "powerMenu"), "hibernateCommand", c.power_hibernate_command);
  read(section(root, "powerMenu"), "restartCommand", c.power_restart_command);
  read(section(root, "powerMenu"), "firmwareCommand", c.power_firmware_command);
  read(section(root, "powerMenu"), "offCommand", c.power_off_command);
  read(section(root, "background"), "clockFormat", c.background_clock_format);
  read(section(root, "background"), "dateFormat", c.background_date_format);
  read(section(root, "background"), "clockSize", c.background_clock_size);
  read(section(root, "background"), "widgetOpacity", c.background_widget_opacity);
  read(section(root, "appearance"), "motionStyle", c.motion_style);
  read(section(root, "appearance"), "hoverLingerMs", c.hover_linger_ms);
  read(section(root, "appearance"), "fontScale", c.font_scale);
  read(section(root, "appearance"), "cardRadius", c.card_radius);
  read(section(root, "appearance"), "shadowEnabled", c.shadow_enabled);
  read(section(root, "appearance"), "shadowStrength", c.shadow_strength);
  read(section(root, "appearance"), "shadowBlur", c.shadow_blur);
  read(section(root, "appearance"), "themeSource", c.theme_source);
  read(section(root, "appearance"), "themeSeed", c.theme_seed);
  read(section(root, "appearance"), "colourOverrides", c.colour_overrides);
  read(section(root, "appearance"), "colourPreset", c.colour_preset);
  const json lock = section(root, "lock");
  read(lock, "clockFormat", c.lock_clock_format);
  read(lock, "dateFormat", c.lock_date_format);
  read(lock, "clockSize", c.lock_clock_size);
  read(lock, "tint", c.lock_tint);
  read(lock, "blurPasses", c.lock_blur_passes);
  read(lock, "blurOffset", c.lock_blur_offset);
  read(lock, "showUser", c.lock_show_user);
  read(lock, "clockStyle", c.lock_clock_style);
  read(lock, "showStatus", c.lock_show_status);
  read(lock, "showWeather", c.lock_show_weather);
  read(lock, "showNotifications", c.lock_show_notifications);
  read(lock, "showMedia", c.lock_show_media);
  read(launcher, "normalWidth", c.launcher_width);
  read(launcher, "tintAppIcons", c.launcher_tint_app_icons);
  read(launcher, "sort", c.launcher_sort);
  read(launcher, "favoritesFirst", c.launcher_favorites_first);
  read(launcher, "showComments", c.launcher_show_comments);
  read(launcher, "showFavoriteStar", c.launcher_show_favorite_star);
  read(launcher, "searchComments", c.launcher_search_comments);
  read(launcher, "boostFavorites", c.launcher_boost_favorites);
  read(launcher, "recordUsage", c.launcher_record_usage);
  read(launcher, "closeOnLaunch", c.launcher_close_on_launch);
  read(launcher, "rowHeight", c.launcher_row_height);
  read(launcher, "iconSize", c.launcher_icon_size);
  read(launcher, "maxHeight", c.launcher_max_height);
  read(launcher, "sink", c.launcher_sink);
  read(launcher, "favorites", c.launcher_favorites);
  read(launcher, "preferred", c.launcher_preferred);
  read(launcher, "hidden", c.launcher_hidden);
  read(launcher, "names", c.launcher_names);
  for (const auto& [id, entry] : section(launcher, "usage").items()) {
    AppUsage usage;
    read(entry, "count", usage.count);
    read(entry, "last", usage.last);
    c.launcher_usage[id] = usage;
  }
  read(wallpaper, "widthRatio", c.wallpaper_launcher_width_ratio);
  read(wallpaper, "maxWidth", c.wallpaper_launcher_max_width);
  read(wallpaper, "itemSpacing", c.wallpaper_item_spacing);
  read(wallpaper, "carouselHeight", c.wallpaper_carousel_height);
  read(wallpaper, "path", c.wallpaper_path);
  read(audio, "minimumSliderHeight", c.osd_slider_minimum_height);
  read(audio, "maximumSliderHeight", c.osd_slider_maximum_height);
  read(audio, "sliderHeightRatio", c.osd_slider_height_ratio);
  read(button, "heightRatio", c.power_menu_height_ratio);
  read(button, "minimumSize", c.power_menu_minimum_button_size);
  read(button, "maximumSize", c.power_menu_maximum_button_size);
  read(rail, "bottomGap", c.rail_bottom_gap);
  read(bar, "tintTrayIcons", c.bar_tint_tray_icons);
  const json status = section(bar, "status");
  read(status, "wifi", c.bar_status_wifi);
  read(status, "bluetooth", c.bar_status_bluetooth);
  read(status, "audio", c.bar_status_audio);
  read(status, "battery", c.bar_status_battery);
  read(status, "quickShare", c.bar_status_quickshare);
  read(status, "widgets", c.bar_status_widgets);
  const json sidebar = section(root, "sidebar");
  read(sidebar, "showMedia", c.sidebar_show_media);
  read(sidebar, "showBrightness", c.sidebar_show_brightness);
  read(sidebar, "showKeepAwake", c.sidebar_show_keep_awake);
  read(sidebar, "showRecorder", c.sidebar_show_recorder);
  read(section(root, "island"), "enabled", c.island_enabled);
  read(section(root, "dashboard"), "enabled", c.dashboard_enabled);
  read(section(root, "notifications"), "popups", c.notification_popups);
  read(section(root, "osd"), "showOnVolumeChange", c.osd_on_change);
  read(matugen, "mode", c.matugen_mode);
  read(matugen, "scheme", c.matugen_scheme);
  read(matugen, "contrast", c.matugen_contrast);
  read(matugen, "prefer", c.matugen_prefer);
  read(weather, "location", c.weather_location);
  read(weather, "preferWeatherLocationFromConfig", c.prefer_weather_location_from_config);
  read(background, "wallpaperParallax", c.wallpaper_parallax);
  read(background, "wallpaperParallaxStrength", c.wallpaper_parallax_strength);
  read(background, "wallpaperTransition", c.wallpaper_transition);
  read(background, "clockEnabled", c.background_clock_enabled);
  read(background, "weatherEnabled", c.background_weather_enabled);
  read(background, "clockX", c.background_clock_x);
  read(background, "clockY", c.background_clock_y);
  read(background, "weatherX", c.background_weather_x);
  read(background, "weatherY", c.background_weather_y);
  read(appearance, "globalOpacity", c.global_opacity);
  read(appearance, "frosted", c.frosted);
  read(appearance, "frostedStrength", c.frosted_strength);
  read(appearance, "frameThickness", c.frame_thickness);
  read(appearance, "frameRadius", c.frame_radius);
  read(appearance, "barHeight", c.bar_height);
  read(appearance, "blobSmoothing", c.blob_smoothing);
  read(appearance, "panelRadius", c.panel_radius);
  read(appearance, "islandRadius", c.island_radius);
  read(appearance, "launcherRadius", c.launcher_radius);
  read(appearance, "animationSpeed", c.animation_speed);
  read(appearance, "language", c.language);
  read(appearance, "fontDisplay", c.font_display);
  read(appearance, "fontUtility", c.font_utility);
  read(appearance, "iconTint", c.icon_tint);
  read(appearance, "artTint", c.art_tint);
  read(appearance, "artHue", c.art_hue);
  read(appearance, "theme", c.theme);
  read(section(root, "plugins"), "enabled", c.plugins_enabled);
  read(section(root, "plugins"), "repositories", c.plugin_repositories);
  for (const auto& [id, values] : section(section(root, "plugins"), "settings").items())
    if (values.is_object()) c.plugin_settings[id] = values.dump();
  read(appearance, "sdfOutlineEnabled", c.sdf_outline_enabled);
  read(appearance, "sdfOutlineWidth", c.sdf_outline_width);
  read(appearance, "sdfOutlineColorRole", c.sdf_outline_color_role);
}

json serialize(const Config& c) {
  json usage = json::object();
  for (const auto& [id, entry] : c.launcher_usage) usage[id] = {{"count", entry.count}, {"last", entry.last}};
  json out = {
      {"bar",
       {{"tintTrayIcons", c.bar_tint_tray_icons},
        {"rail", {{"bottomGap", c.rail_bottom_gap}}},
        {"status",
         {{"wifi", c.bar_status_wifi},
          {"bluetooth", c.bar_status_bluetooth},
          {"audio", c.bar_status_audio},
          {"battery", c.bar_status_battery},
          {"quickShare", c.bar_status_quickshare},
          {"widgets", c.bar_status_widgets}}}}},
      {"sidebar",
       {{"showMedia", c.sidebar_show_media},
        {"showBrightness", c.sidebar_show_brightness},
        {"showKeepAwake", c.sidebar_show_keep_awake},
        {"showRecorder", c.sidebar_show_recorder}}},
      {"island", {{"enabled", c.island_enabled}}},
      {"dashboard", {{"enabled", c.dashboard_enabled}}},
      {"notifications", {{"popups", c.notification_popups}}},
      {"launcher",
       {{"normalWidth", c.launcher_width},
        {"tintAppIcons", c.launcher_tint_app_icons},
        {"sort", c.launcher_sort},
        {"favoritesFirst", c.launcher_favorites_first},
        {"showComments", c.launcher_show_comments},
        {"showFavoriteStar", c.launcher_show_favorite_star},
        {"searchComments", c.launcher_search_comments},
        {"boostFavorites", c.launcher_boost_favorites},
        {"recordUsage", c.launcher_record_usage},
        {"closeOnLaunch", c.launcher_close_on_launch},
        {"rowHeight", c.launcher_row_height},
        {"iconSize", c.launcher_icon_size},
        {"maxHeight", c.launcher_max_height},
        {"sink", c.launcher_sink},
        {"favorites", c.launcher_favorites},
        {"preferred", c.launcher_preferred},
        {"hidden", c.launcher_hidden},
        {"names", c.launcher_names},
        {"usage", usage},
        {"wallpaper",
         {{"widthRatio", c.wallpaper_launcher_width_ratio},
          {"maxWidth", c.wallpaper_launcher_max_width},
          {"itemSpacing", c.wallpaper_item_spacing},
          {"carouselHeight", c.wallpaper_carousel_height},
          {"path", c.wallpaper_path}}}}},
      {"matugen",
       {{"mode", c.matugen_mode},
        {"scheme", c.matugen_scheme},
        {"contrast", c.matugen_contrast},
        {"prefer", c.matugen_prefer}}},
      {"osd",
       {{"showOnVolumeChange", c.osd_on_change},
        {"audio",
         {{"minimumSliderHeight", c.osd_slider_minimum_height},
          {"maximumSliderHeight", c.osd_slider_maximum_height},
          {"sliderHeightRatio", c.osd_slider_height_ratio}}}}},
      {"powerMenu",
       {{"button",
         {{"heightRatio", c.power_menu_height_ratio},
          {"minimumSize", c.power_menu_minimum_button_size},
          {"maximumSize", c.power_menu_maximum_button_size}}}}},
      {"weather",
       {{"location", c.weather_location},
        {"preferWeatherLocationFromConfig", c.prefer_weather_location_from_config}}},
      {"background",
       {{"wallpaperParallax", c.wallpaper_parallax},
        {"wallpaperParallaxStrength", c.wallpaper_parallax_strength},
        {"wallpaperTransition", c.wallpaper_transition},
        {"clockEnabled", c.background_clock_enabled},
        {"weatherEnabled", c.background_weather_enabled},
        {"clockX", c.background_clock_x},
        {"clockY", c.background_clock_y},
        {"weatherX", c.background_weather_x},
        {"weatherY", c.background_weather_y}}},
      {"appearance",
       {{"globalOpacity", c.global_opacity},
        {"frosted", c.frosted},
        {"frostedStrength", c.frosted_strength},
        {"frameThickness", c.frame_thickness},
        {"frameRadius", c.frame_radius},
        {"barHeight", c.bar_height},
        {"blobSmoothing", c.blob_smoothing},
        {"panelRadius", c.panel_radius},
        {"islandRadius", c.island_radius},
        {"launcherRadius", c.launcher_radius},
        {"animationSpeed", c.animation_speed},
        {"language", c.language},
        {"fontDisplay", c.font_display},
        {"fontUtility", c.font_utility},
        {"iconTint", c.icon_tint},
        {"artTint", c.art_tint},
        {"artHue", c.art_hue},
        {"theme", c.theme},
        {"sdfOutlineEnabled", c.sdf_outline_enabled},
        {"sdfOutlineWidth", c.sdf_outline_width},
        {"sdfOutlineColorRole", c.sdf_outline_color_role}}},
  };
  out["lock"] = {{"clockFormat", c.lock_clock_format}, {"dateFormat", c.lock_date_format}, {"clockSize", c.lock_clock_size},
                 {"tint", c.lock_tint}, {"blurPasses", c.lock_blur_passes}, {"blurOffset", c.lock_blur_offset}, {"showUser", c.lock_show_user}, {"clockStyle", c.lock_clock_style},
                 {"showStatus", c.lock_show_status}, {"showWeather", c.lock_show_weather},
                 {"showNotifications", c.lock_show_notifications}, {"showMedia", c.lock_show_media}};
  out["bar"]["left"] = c.bar_left;
  out["bar"]["center"] = c.bar_center;
  out["bar"]["right"] = c.bar_right;
  out["bar"]["osIcon"] = c.bar_os_icon;
  out["bar"]["clockFormat"] = c.bar_clock_format;
  out["bar"]["clockSize"] = c.bar_clock_size;
  out["bar"]["workspaceStyle"] = c.bar_workspace_style;
  out["bar"]["pills"] = c.bar_pills;
  out["bar"]["titleWidth"] = c.bar_title_width;
  out["bar"]["traySize"] = c.bar_tray_size;
  out["sidebar"]["width"] = c.sidebar_width;
  out["sidebar"]["tileHeight"] = c.sidebar_tile_height;
  out["sidebar"]["tileRadius"] = c.sidebar_tile_radius;
  out["sidebar"]["pullArea"] = c.sidebar_pull_area;
  out["sidebar"]["tileWifi"] = c.sidebar_tile_wifi;
  out["sidebar"]["tileBluetooth"] = c.sidebar_tile_bluetooth;
  out["sidebar"]["tileMic"] = c.sidebar_tile_mic;
  out["sidebar"]["tileDnd"] = c.sidebar_tile_dnd;
  out["sidebar"]["tileShare"] = c.sidebar_tile_share;
  out["sidebar"]["tileSettings"] = c.sidebar_tile_settings;
  out["sidebar"]["tileLock"] = c.sidebar_tile_lock;
  out["notifications"]["side"] = c.notification_side;
  out["notifications"]["width"] = c.notification_width;
  out["notifications"]["max"] = c.notification_max;
  out["notifications"]["images"] = c.notification_images;
  out["island"]["width"] = c.island_width;
  out["island"]["expandedWidth"] = c.island_expanded_width;
  out["island"]["announceSeconds"] = c.island_announce_seconds;
  out["dashboard"]["tab"] = c.dashboard_tab;
  out["dashboard"]["rememberTab"] = c.dashboard_remember_tab;
  out["weather"]["unit"] = c.weather_unit;
  out["osd"]["holdMs"] = c.osd_hold_ms;
  out["powerMenu"]["lock"] = c.power_lock;
  out["powerMenu"]["logout"] = c.power_logout;
  out["powerMenu"]["suspend"] = c.power_suspend;
  out["powerMenu"]["hibernate"] = c.power_hibernate;
  out["powerMenu"]["restart"] = c.power_restart;
  out["powerMenu"]["firmware"] = c.power_firmware;
  out["powerMenu"]["off"] = c.power_off;
  out["powerMenu"]["lockCommand"] = c.power_lock_command;
  out["powerMenu"]["logoutCommand"] = c.power_logout_command;
  out["powerMenu"]["suspendCommand"] = c.power_suspend_command;
  out["powerMenu"]["hibernateCommand"] = c.power_hibernate_command;
  out["powerMenu"]["restartCommand"] = c.power_restart_command;
  out["powerMenu"]["firmwareCommand"] = c.power_firmware_command;
  out["powerMenu"]["offCommand"] = c.power_off_command;
  out["background"]["clockFormat"] = c.background_clock_format;
  out["background"]["dateFormat"] = c.background_date_format;
  out["background"]["clockSize"] = c.background_clock_size;
  out["background"]["widgetOpacity"] = c.background_widget_opacity;
  out["appearance"]["motionStyle"] = c.motion_style;
  out["appearance"]["hoverLingerMs"] = c.hover_linger_ms;
  out["appearance"]["fontScale"] = c.font_scale;
  out["appearance"]["cardRadius"] = c.card_radius;
  out["appearance"]["shadowEnabled"] = c.shadow_enabled;
  out["appearance"]["shadowStrength"] = c.shadow_strength;
  out["appearance"]["shadowBlur"] = c.shadow_blur;
  out["appearance"]["themeSource"] = c.theme_source;
  out["appearance"]["themeSeed"] = c.theme_seed;
  out["appearance"]["colourOverrides"] = c.colour_overrides;
  out["appearance"]["colourPreset"] = c.colour_preset;
  out["plugins"]["enabled"] = c.plugins_enabled;
  out["plugins"]["repositories"] = c.plugin_repositories;
  out["plugins"]["settings"] = json::object();
  for (const auto& [id, values] : c.plugin_settings) {
    try {
      out["plugins"]["settings"][id] = json::parse(values);
    } catch (const std::exception&) {
    }
  }
  return out;
}

Config load() {
  Config result;
  std::ifstream input(directory() / "config.json");
  if (!input) return result;
  try {
    apply(result, json::parse(input));
  } catch (const std::exception&) {
  }
  return result;
}

int watcher() {
  static const int fd = [] {
    const int descriptor = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (descriptor < 0) return -1;
    std::error_code error;
    fs::create_directories(directory(), error);
    inotify_add_watch(descriptor, directory().c_str(), IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE);
    return descriptor;
  }();
  return fd;
}

}  // namespace

std::string Config::scheme_type() const {
  if (matugen_scheme == "dynamic-hard") return "scheme-vibrant";
  if (matugen_scheme == "dynamic-soft") return "scheme-tonal-spot";
  const std::string scheme = matugen_scheme == "scheme-fidelety" ? "scheme-fidelity" : matugen_scheme;
  return scheme.starts_with("scheme-") ? scheme : "scheme-" + scheme;
}

float Config::scheme_contrast() const {
  if (matugen_scheme == "dynamic-hard") return 0.35F;
  if (matugen_scheme == "dynamic-soft") return -0.15F;
  return matugen_contrast;
}

Config& config() {
  static Config instance = load();
  return instance;
}

std::string config_path() { return (directory() / "config.json").string(); }

std::uint64_t generation = 0;
std::uint64_t config_generation() { return generation; }

bool listed(const std::vector<std::string>& list, const std::string& id) {
  return std::ranges::find(list, id) != list.end();
}

void toggle_listed(std::vector<std::string>& list, const std::string& id) {
  if (const auto found = std::ranges::find(list, id); found != list.end()) list.erase(found);
  else list.push_back(id);
}

void save_config() {
  ++generation;
  std::error_code error;
  fs::create_directories(directory(), error);
  const fs::path temporary = directory() / "config.json.tmp";
  {
    std::ofstream output(temporary);
    output << serialize(config()).dump(2) << '\n';
  }
  fs::rename(temporary, directory() / "config.json", error);
}

int config_event_fd() { return watcher(); }

bool reload_config(bool apply) {
  alignas(inotify_event) char buffer[4096];
  bool changed = false;
  while (::read(watcher(), buffer, sizeof(buffer)) > 0) changed = true;
  if (changed && apply) {
    ++generation;
    const bool editing = config().background_widgets_editing;
    config() = load();
    config().background_widgets_editing = editing;
  }
  return changed;
}

}  // namespace blx
