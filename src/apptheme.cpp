#include "apptheme.hpp"

#include <vw/process.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include "config.hpp"
#include "style.hpp"

namespace blx::apptheme {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

fs::path home() {
  const char* value = std::getenv("HOME");
  return value != nullptr ? fs::path(value) : fs::path("/");
}

fs::path config_home() {
  const char* value = std::getenv("XDG_CONFIG_HOME");
  return value != nullptr && *value ? fs::path(value) : home() / ".config";
}

fs::path state_file() {
  const char* value = std::getenv("XDG_STATE_HOME");
  return (value != nullptr && *value ? fs::path(value) : home() / ".local/state") / "blxshell/apptheme.json";
}

std::optional<std::string> slurp(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return std::nullopt;
  return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

// Never writes through a symlink: one from a config manager (home-manager,
bool put(const fs::path& path, const std::string& text) {
  std::error_code error;
  if (fs::is_symlink(path, error)) return false;
  if (const auto old = slurp(path); old && *old == text) return false;
  fs::create_directories(path.parent_path(), error);
  const fs::path temporary = path.string() + ".blxshell-tmp";
  {
    std::ofstream output(temporary, std::ios::binary);
    output << text;
    if (!output) return false;
  }
  fs::rename(temporary, path, error);
  return !error;
}

void remove_file(const fs::path& path) {
  std::error_code error;
  if (!fs::is_symlink(path, error)) fs::remove(path, error);
}

bool on_path(const std::string& program) {
  const char* path = std::getenv("PATH");
  std::stringstream stream(path != nullptr ? path : "");
  for (std::string dir; std::getline(stream, dir, ':');)
    if (!dir.empty() && ::access((fs::path(dir) / program).c_str(), X_OK) == 0) return true;
  return false;
}

bool gtk_theme_installed(const std::string& name) {
  std::vector<fs::path> dirs{home() / ".themes", home() / ".local/share/themes"};
  if (const char* data_home = std::getenv("XDG_DATA_HOME"); data_home != nullptr && *data_home)
    dirs.emplace_back(fs::path(data_home) / "themes");
  const char* list = std::getenv("XDG_DATA_DIRS");
  std::stringstream stream(list != nullptr && *list ? list : "/usr/local/share:/usr/share");
  for (std::string dir; std::getline(stream, dir, ':');)
    if (!dir.empty()) dirs.emplace_back(fs::path(dir) / "themes");
  std::error_code error;
  return std::ranges::any_of(dirs, [&](const fs::path& dir) { return fs::is_directory(dir / name, error); });
}

json load_state() {
  if (const auto text = slurp(state_file())) {
    json state = json::parse(*text, nullptr, false);
    if (state.is_object()) return state;
  }
  return json::object();
}

void save_state(const json& state) { put(state_file(), state.dump(2) + '\n'); }

void back_up(json& record, const fs::path& path) {
  const std::string key = path.string();
  if (record.contains(key)) return;
  std::error_code error;
  const bool existed = fs::exists(path, error) && !fs::is_symlink(path, error);
  if (existed) fs::copy_file(path, path.string() + ".blxshell-backup", fs::copy_options::overwrite_existing, error);
  record[key] = existed;
}

void restore(json& record) {
  for (const auto& [key, existed] : record.items()) {
    const fs::path path = key;
    std::error_code error;
    if (existed.is_boolean() && existed.get<bool>()) fs::rename(path.string() + ".blxshell-backup", path, error);
    else remove_file(path);
  }
  record = json::object();
}

int byte(float v) { return static_cast<int>(std::lround(std::clamp(v, 0.0F, 1.0F) * 255)); }

std::string hex(vw::Color c) { return std::format("#{:02x}{:02x}{:02x}", byte(c[0]), byte(c[1]), byte(c[2])); }

std::string rgba(vw::Color c, float alpha) {
  return std::format("rgba({},{},{},{:.2f})", byte(c[0]), byte(c[1]), byte(c[2]), alpha);
}

vw::Color mix(vw::Color a, vw::Color b, float t) {
  return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t, 1};
}

bool dark() { return config().matugen_mode != "light"; }

std::string gtk_colours(const Colours& c) {
  std::string css;
  const auto define = [&](std::string_view name, const std::string& value) {
    css += std::format("@define-color {} {};\n", name, value);
  };
  const std::string clear = "rgba(0,0,0,0)";
  const std::string shade = rgba({0, 0, 0, 1}, dark() ? 0.25F : 0.07F);
  define("accent_color", hex(c.primary));
  define("accent_bg_color", hex(c.primary));
  define("accent_fg_color", hex(c.primary_text));
  define("destructive_color", hex(c.error));
  define("destructive_bg_color", hex(c.error));
  define("destructive_fg_color", hex(c.error_text));
  define("error_color", hex(c.error));
  define("error_bg_color", hex(c.error));
  define("error_fg_color", hex(c.error_text));
  define("success_color", hex(c.tertiary));
  define("success_bg_color", hex(c.tertiary_container));
  define("success_fg_color", hex(c.text));
  define("warning_color", hex(mix(c.primary, c.error, 0.5F)));
  define("warning_bg_color", hex(c.secondary_container));
  define("warning_fg_color", hex(c.text));
  define("window_bg_color", hex(c.surface));
  define("window_fg_color", hex(c.text));
  define("view_bg_color", hex(c.surface));
  define("view_fg_color", hex(c.text));
  define("headerbar_bg_color", hex(c.surface));
  define("headerbar_fg_color", hex(c.text));
  define("headerbar_backdrop_color", hex(c.surface));
  define("headerbar_border_color", clear);
  define("headerbar_shade_color", clear);
  define("headerbar_darker_shade_color", clear);
  define("sidebar_bg_color", hex(c.surface_container_low));
  define("sidebar_fg_color", hex(c.text));
  define("sidebar_backdrop_color", hex(c.surface_container_low));
  define("sidebar_border_color", clear);
  define("sidebar_shade_color", clear);
  define("secondary_sidebar_bg_color", hex(c.surface_container_low));
  define("secondary_sidebar_fg_color", hex(c.text));
  define("secondary_sidebar_backdrop_color", hex(c.surface_container_low));
  define("secondary_sidebar_border_color", clear);
  define("secondary_sidebar_shade_color", clear);
  define("card_bg_color", hex(c.surface_container));
  define("card_fg_color", hex(c.text));
  define("card_shade_color", clear);
  define("dialog_bg_color", hex(c.surface_container_high));
  define("dialog_fg_color", hex(c.text));
  define("popover_bg_color", hex(c.surface_container));
  define("popover_fg_color", hex(c.text));
  define("popover_shade_color", shade);
  define("thumbnail_bg_color", hex(c.surface_container));
  define("thumbnail_fg_color", hex(c.text));
  define("shade_color", shade);
  define("scrollbar_outline_color", clear);
  define("m3_field", hex(c.surface_container_highest));
  define("m3_outline", hex(c.outline));
  define("m3_outline_variant", hex(c.outline_variant));
  define("m3_secondary_container", hex(c.secondary_container));
  define("m3_container_highest", hex(c.surface_container_highest));
  define("m3_inverse_surface", hex(c.text));
  define("m3_inverse_text", hex(c.surface));
  define("theme_bg_color", hex(c.surface));
  define("theme_fg_color", hex(c.text));
  define("theme_base_color", hex(c.surface));
  define("theme_text_color", hex(c.text));
  define("theme_selected_bg_color", hex(c.primary));
  define("theme_selected_fg_color", hex(c.primary_text));
  define("theme_unfocused_bg_color", hex(c.surface));
  define("theme_unfocused_fg_color", hex(c.text));
  define("theme_unfocused_base_color", hex(c.surface));
  define("theme_unfocused_text_color", hex(c.text));
  define("theme_unfocused_selected_bg_color", hex(c.primary));
  define("theme_unfocused_selected_fg_color", hex(c.primary_text));
  define("insensitive_bg_color", hex(c.surface_container_low));
  define("insensitive_fg_color", hex(c.text_muted));
  define("borders", hex(c.outline_variant));
  define("unfocused_borders", hex(c.outline_variant));
  return css;
}

constexpr const char* shapes_common = R"(
button { border-radius: 9999px; }
.linked:not(.vertical) > button,
.linked:not(.vertical) > menubutton > button,
.linked:not(.vertical) > dropdown > button,
.linked:not(.vertical) > combobox > box > button { border-radius: 8px; }
.linked:not(.vertical) > button:first-child,
.linked:not(.vertical) > :first-child > button,
.linked:not(.vertical) > :first-child > box > button { border-top-left-radius: 9999px; border-bottom-left-radius: 9999px; }
.linked:not(.vertical) > button:last-child,
.linked:not(.vertical) > :last-child > button,
.linked:not(.vertical) > :last-child > box > button { border-top-right-radius: 9999px; border-bottom-right-radius: 9999px; }
button.suggested-action { background-color: @accent_bg_color; color: @accent_fg_color; }
button.destructive-action { background-color: @destructive_bg_color; color: @destructive_fg_color; }

entry, spinbutton:not(.vertical) {
  border-radius: 4px 4px 0 0;
  background-color: @m3_field;
  box-shadow: inset 0 -1px @m3_outline;
  outline: none;
}
entry:focus-within, entry:focus, spinbutton:focus-within { box-shadow: inset 0 -2px @accent_bg_color; }
entry.search, searchbar entry, .search entry { border-radius: 9999px; box-shadow: none; }

check, checkbutton check { border-radius: 2px; }
radio, checkbutton radio { border-radius: 9999px; }

tooltip, tooltip.background { border-radius: 4px; background-color: @m3_inverse_surface; box-shadow: none; }
tooltip label { color: @m3_inverse_text; }

scrollbar slider { border-radius: 9999px; }
progressbar trough, progressbar progress { border-radius: 9999px; }
scale trough, scale highlight, scale fill { border-radius: 9999px; }

headerbar { box-shadow: none; border: none; }

.navigation-sidebar > row, placessidebar row, stacksidebar row { border-radius: 9999px; }
.navigation-sidebar > row:selected, placessidebar row:selected, stacksidebar row:selected {
  background-color: @m3_secondary_container; color: @window_fg_color;
}
)";

constexpr const char* shapes_gtk4 = R"(
switch {
  min-width: 52px; min-height: 32px; border-radius: 9999px; padding: 0;
  background-color: @m3_container_highest; box-shadow: inset 0 0 0 2px @m3_outline; border: none;
}
switch:checked { background-color: @accent_bg_color; box-shadow: none; }
switch > slider {
  min-width: 16px; min-height: 16px; margin: 8px 5px; border-radius: 9999px;
  background-color: @m3_outline; box-shadow: none; border: none;
}
switch:checked > slider { min-width: 24px; min-height: 24px; margin: 4px 1px; background-color: @accent_fg_color; }

popover > contents, popover.menu > contents { border-radius: 16px; }
popover.menu modelbutton, popover modelbutton { border-radius: 12px; }

window.dialog, window.messagedialog, dialog-host > dialog.floating sheet, dialog.floating sheet,
window.dialog.message { border-radius: 28px; }
dialog.bottom sheet { border-top-left-radius: 28px; border-top-right-radius: 28px; }

.card { border-radius: 12px; box-shadow: none; }

list.boxed-list, list.boxed-list-separate { background-color: transparent; box-shadow: none; border: none; }
list.boxed-list > row, list.boxed-list-separate > row {
  background-color: @card_bg_color; border-radius: 4px; margin-bottom: 2px; border: none; box-shadow: none;
}
list.boxed-list > row:first-child, list.boxed-list-separate > row:first-child { border-top-left-radius: 20px; border-top-right-radius: 20px; }
list.boxed-list > row:last-child, list.boxed-list-separate > row:last-child {
  border-bottom-left-radius: 20px; border-bottom-right-radius: 20px; margin-bottom: 0;
}
list.boxed-list > row:only-child { border-radius: 20px; }

scale > trough { min-height: 12px; }
scale > trough > slider {
  min-width: 4px; min-height: 32px; margin: -10px 0; border-radius: 2px;
  background-color: @accent_bg_color; box-shadow: none; border: none;
}
)";

constexpr const char* shapes_gtk3 = R"(
switch { border-radius: 9999px; background-color: @m3_container_highest; box-shadow: inset 0 0 0 2px @m3_outline; border: none; }
switch:checked { background-color: @accent_bg_color; box-shadow: none; }
switch slider { border-radius: 9999px; background-color: @m3_outline; box-shadow: none; border: 4px solid transparent; background-clip: padding-box; }
switch:checked slider { background-color: @accent_fg_color; border-width: 2px; }

menu, .menu, .context-menu, popover, popover.background { border-radius: 16px; }
menu menuitem, popover modelbutton { border-radius: 12px; }

window.dialog, messagedialog, .csd.dialog decoration { border-radius: 28px; }

.card, frame > border { border-radius: 12px; box-shadow: none; }
)";

std::string gtk_css(const Colours& c, bool gtk4) {
  return "/* Written by blxshell from its colours (Settings → Theme → GTK apps); changes are overwritten. */\n\n" +
         gtk_colours(c) + shapes_common + (gtk4 ? shapes_gtk4 : shapes_gtk3);
}

constexpr std::string_view import_begin = "/* blxshell: begin. Managed by blxshell (Settings → Theme → GTK apps). */";
constexpr std::string_view import_end = "/* blxshell: end */";

std::string with_import(std::string text, bool present) {
  if (const auto begin = text.find(import_begin); begin != std::string::npos) {
    const auto end = text.find(import_end, begin);
    if (end != std::string::npos) {
      auto stop = end + import_end.size();
      if (stop < text.size() && text[stop] == '\n') ++stop;
      std::size_t start = begin;
      if (start > 0 && text[start - 1] == '\n' && (start < 2 || text[start - 2] == '\n')) --start;
      text.erase(start, stop - start);
    }
  }
  if (present) {
    if (!text.empty() && text.back() != '\n') text += '\n';
    if (!text.empty()) text += '\n';
    text += std::string(import_begin) + "\n@import url(\"blxshell.css\");\n" + std::string(import_end) + '\n';
  }
  return text;
}

std::string gsettings_get(const std::string& key) {
  std::string command = "gsettings get org.gnome.desktop.interface " + key + " 2>/dev/null";
  std::string out;
  if (FILE* pipe = ::popen(command.c_str(), "r")) {
    char buffer[256];
    while (std::fgets(buffer, sizeof buffer, pipe) != nullptr) out += buffer;
    ::pclose(pipe);
  }
  while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) out.pop_back();
  if (out.size() >= 2 && out.front() == '\'' && out.back() == '\'') out = out.substr(1, out.size() - 2);
  return out;
}

void gsettings_apply(const std::string& theme, const std::string& scheme) {
  if (!on_path("gsettings")) return;
  const std::string set = "gsettings set org.gnome.desktop.interface ";
  std::string script;
  if (!scheme.empty()) script += set + "color-scheme '" + scheme + "'; ";
  if (!theme.empty()) script += set + "gtk-theme ''; sleep 0.1; " + set + "gtk-theme '" + theme + "'";
  if (!script.empty()) vw::spawn({"sh", "-c", script});
}

void apply_gtk(const Colours& c, json& state) {
  json& gtk = state["gtk"];
  if (!gtk.is_object()) gtk = json::object();
  const bool first = !gtk.value("applied", false);
  if (first) {
    gtk["theme"] = gsettings_get("gtk-theme");
    gtk["scheme"] = gsettings_get("color-scheme");
    gtk["applied"] = true;
  }
  bool changed = first;
  for (const char* version : {"gtk-3.0", "gtk-4.0"}) {
    const fs::path folder = config_home() / version;
    changed |= put(folder / "blxshell.css", gtk_css(c, std::string_view(version) == "gtk-4.0"));
    const fs::path main = folder / "gtk.css";
    changed |= put(main, with_import(slurp(main).value_or(""), true));
  }
  std::string theme;
  if (gtk_theme_installed("adw-gtk3")) theme = dark() && gtk_theme_installed("adw-gtk3-dark") ? "adw-gtk3-dark" : "adw-gtk3";
  const std::string scheme = dark() ? "prefer-dark" : "prefer-light";
  const bool settings_differ = (!theme.empty() && gsettings_get("gtk-theme") != theme) || gsettings_get("color-scheme") != scheme;
  if (changed || settings_differ) gsettings_apply(theme.empty() ? gsettings_get("gtk-theme") : theme, scheme);
}

void undo_gtk(json& state) {
  json& gtk = state["gtk"];
  if (!gtk.is_object() || !gtk.value("applied", false)) return;
  for (const char* version : {"gtk-3.0", "gtk-4.0"}) {
    const fs::path folder = config_home() / version;
    const fs::path main = folder / "gtk.css";
    if (const auto text = slurp(main)) {
      const std::string without = with_import(*text, false);
      if (without.find_first_not_of(" \n\t") == std::string::npos) remove_file(main);
      else put(main, without);
    }
    remove_file(folder / "blxshell.css");
  }
  gsettings_apply(gtk.value("theme", std::string()), gtk.value("scheme", std::string()));
  gtk = json::object();
}

// QPalette's roles in order: WindowText, Button, Light, Midlight, Dark, Mid,
std::string qt_palette_line(const Colours& c, bool disabled, int count) {
  const auto argb = [](vw::Color colour, float alpha = 1) {
    return std::format("#{:02x}{:02x}{:02x}{:02x}", byte(alpha), byte(colour[0]), byte(colour[1]), byte(colour[2]));
  };
  const vw::Color text = disabled ? c.text_muted : c.text;
  const float fade = disabled ? 0.6F : 1;
  const std::vector<std::string> roles{
      argb(text),                                // WindowText
      argb(c.surface_container_high),            // Button
      argb(c.surface_container_highest),         // Light
      argb(c.surface_container_high),            // Midlight
      argb(mix(c.surface, {0, 0, 0, 1}, 0.35F)), // Dark
      argb(c.surface_container),                 // Mid
      argb(text),                                // Text
      argb(c.error),                             // BrightText
      argb(text),                                // ButtonText
      argb(c.surface_container_low),             // Base
      argb(c.surface),                           // Window
      argb({0, 0, 0, 1}, 0.4F),                  // Shadow
      argb(c.primary, fade),                     // Highlight
      argb(c.primary_text),                      // HighlightedText
      argb(c.primary),                           // Link
      argb(c.tertiary),                          // LinkVisited
      argb(c.surface_container),                 // AlternateBase
      argb(c.surface),                           // NoRole
      argb(c.text),                              // ToolTipBase: inverse surface
      argb(c.surface),                           // ToolTipText
      argb(c.text_muted),                        // PlaceholderText
      argb(c.primary, fade),                     // Accent
  };
  std::string line;
  for (int index = 0; index < count && index < static_cast<int>(roles.size()); ++index)
    line += (index ? ", " : "") + roles[static_cast<std::size_t>(index)];
  return line;
}

std::string qt_colours(const Colours& c, int count) {
  return std::format("[ColorScheme]\nactive_colors={0}\ndisabled_colors={1}\ninactive_colors={0}\n",
                     qt_palette_line(c, false, count), qt_palette_line(c, true, count));
}

std::string qt_stylesheet(const Colours& c) {
  std::string qss = R"(/* Written by blxshell from its colours (Settings → Theme → Qt apps); changes are overwritten. */

QPushButton, QToolButton[popupMode="1"] {
  border-radius: 18px; padding: 8px 20px; min-height: 20px;
  background-color: @secondary_container; color: @text; border: none;
}
QPushButton:hover { background-color: @secondary_container_hover; }
QPushButton:pressed { background-color: @secondary_container_pressed; }
QPushButton:default { background-color: @primary; color: @on_primary; }
QPushButton:disabled { background-color: @disabled_container; color: @disabled_text; }
QPushButton:flat { background-color: transparent; color: @primary; }
QToolButton { border-radius: 18px; padding: 6px; border: none; background: transparent; }
QToolButton:hover { background-color: @hover; }
QToolButton:checked { background-color: @secondary_container; }

QLineEdit, QSpinBox, QDoubleSpinBox, QDateEdit, QTimeEdit, QDateTimeEdit {
  border: none; border-bottom: 1px solid @outline;
  border-top-left-radius: 4px; border-top-right-radius: 4px;
  background-color: @field; color: @text; padding: 6px 10px;
  selection-background-color: @primary; selection-color: @on_primary;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-bottom: 2px solid @primary; }
QComboBox {
  border: none; border-radius: 18px; padding: 6px 16px; min-height: 22px;
  background-color: @secondary_container; color: @text;
}
QComboBox::drop-down { border: none; width: 24px; }
QComboBox QAbstractItemView {
  background-color: @container; border-radius: 12px; padding: 6px; border: none;
  selection-background-color: @secondary_container; selection-color: @text;
}

QMenu { background-color: @container; border: none; padding: 8px 6px; }
QMenu::item { padding: 8px 24px 8px 14px; border-radius: 12px; }
QMenu::item:selected { background-color: @secondary_container; color: @text; }
QMenu::separator { height: 8px; background: transparent; }

QToolTip { background-color: @text; color: @surface; border: none; border-radius: 4px; padding: 4px 8px; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle { background: @outline_variant; border-radius: 3px; min-height: 32px; min-width: 32px; }
QScrollBar::handle:hover { background: @outline; }
QScrollBar::add-line, QScrollBar::sub-line, QScrollBar::add-page, QScrollBar::sub-page { background: none; border: none; width: 0; height: 0; }

QProgressBar { border: none; border-radius: 4px; background-color: @field; max-height: 8px; text-align: center; color: transparent; }
QProgressBar::chunk { border-radius: 4px; background-color: @primary; }

QSlider::groove:horizontal { height: 12px; border-radius: 6px; background: @field; }
QSlider::sub-page:horizontal { border-radius: 6px; background: @primary; }
QSlider::handle:horizontal { width: 4px; margin: -10px 4px; border-radius: 2px; background: @primary; }

QTabBar::tab { padding: 8px 18px; border: none; border-radius: 18px; background: transparent; color: @text_muted; margin: 2px; }
QTabBar::tab:selected { background: @secondary_container; color: @text; }

QGroupBox { border: none; border-radius: 12px; background-color: @container; margin-top: 18px; padding: 12px; }
QGroupBox::title { subcontrol-origin: margin; left: 12px; color: @text_muted; }
QHeaderView::section { background-color: @surface; color: @text_muted; border: none; padding: 6px; }
QTreeView, QListView, QTableView { border: none; background-color: @surface; alternate-background-color: @container_low; }
QTreeView::item:selected, QListView::item:selected { background-color: @secondary_container; color: @text; border-radius: 12px; }
)";
  const std::vector<std::pair<std::string, std::string>> tokens{
      {"@secondary_container_pressed", hex(mix(c.secondary_container, c.text, 0.16F))},
      {"@secondary_container_hover", hex(mix(c.secondary_container, c.text, 0.08F))},
      {"@secondary_container", hex(c.secondary_container)},
      {"@disabled_container", rgba(c.text, 0.12F)},
      {"@disabled_text", rgba(c.text, 0.38F)},
      {"@outline_variant", hex(c.outline_variant)},
      {"@container_low", hex(c.surface_container_low)},
      {"@on_primary", hex(c.primary_text)},
      {"@text_muted", hex(c.text_muted)},
      {"@container", hex(c.surface_container)},
      {"@primary", hex(c.primary)},
      {"@surface", hex(c.surface)},
      {"@outline", hex(c.outline)},
      {"@field", hex(c.surface_container_highest)},
      {"@hover", rgba(c.text, 0.08F)},
      {"@text", hex(c.text)},
  };
  for (const auto& [token, value] : tokens) {
    for (std::size_t at = qss.find(token); at != std::string::npos; at = qss.find(token, at + value.size()))
      qss.replace(at, token.size(), value);
  }
  return qss;
}

std::string ini_with(const std::string& text, const std::string& section,
                     const std::vector<std::pair<std::string, std::optional<std::string>>>& keys) {
  std::vector<std::string> lines;
  std::stringstream stream(text);
  for (std::string line; std::getline(stream, line);) lines.push_back(line);
  auto start = std::ranges::find(lines, "[" + section + "]");
  if (start == lines.end()) {
    if (!lines.empty() && !lines.back().empty()) lines.emplace_back();
    lines.push_back("[" + section + "]");
    start = lines.end() - 1;
  }
  auto end = std::find_if(start + 1, lines.end(), [](const std::string& line) { return line.starts_with('['); });
  for (const auto& [key, value] : keys) {
    const auto found = std::find_if(start + 1, end, [&](const std::string& line) { return line.starts_with(key + "="); });
    if (found != end) {
      if (value) *found = key + "=" + *value;
      else {
        lines.erase(found);
        --end;
      }
    } else if (value) {
      auto at = end;
      while (at > start + 1 && (at - 1)->empty()) --at;
      lines.insert(at, key + "=" + *value);
    }
    start = std::ranges::find(lines, "[" + section + "]");
    end = std::find_if(start + 1, lines.end(), [](const std::string& line) { return line.starts_with('['); });
  }
  std::string result;
  for (const std::string& line : lines) result += line + '\n';
  return result;
}

std::string kde_colours(const std::string& text, const Colours& c) {
  const auto rgb = [](vw::Color colour) {
    return std::format("{},{},{}", byte(colour[0]), byte(colour[1]), byte(colour[2]));
  };
  const auto set = [&](const std::string& current, const std::string& section, vw::Color background,
                       vw::Color alternate, vw::Color foreground) {
    return ini_with(current, section,
                    {{"BackgroundNormal", rgb(background)},
                     {"BackgroundAlternate", rgb(alternate)},
                     {"ForegroundNormal", rgb(foreground)},
                     {"ForegroundInactive", rgb(c.text_muted)},
                     {"ForegroundActive", rgb(c.primary)},
                     {"ForegroundLink", rgb(c.primary)},
                     {"ForegroundVisited", rgb(c.tertiary)},
                     {"ForegroundNegative", rgb(c.error)},
                     {"ForegroundNeutral", rgb(mix(c.primary, c.error, 0.5F))},
                     {"ForegroundPositive", rgb(c.tertiary)},
                     {"DecorationFocus", rgb(c.primary)},
                     {"DecorationHover", rgb(c.primary)}});
  };
  std::string result = text;
  result = set(result, "Colors:Window", c.surface, c.surface_container_low, c.text);
  result = set(result, "Colors:View", c.surface, c.surface_container_low, c.text);
  result = set(result, "Colors:Button", c.surface_container_high, c.surface_container, c.text);
  result = set(result, "Colors:Selection", c.primary, c.primary, c.primary_text);
  result = set(result, "Colors:Tooltip", c.text, c.text, c.surface);
  result = set(result, "Colors:Complementary", c.surface_container, c.surface_container_low, c.text);
  result = set(result, "Colors:Header", c.surface, c.surface, c.text);
  result = ini_with(result, "General", {{"ColorScheme", std::string("blxshell")}});
  return result;
}

// platform theme already. Qt 6's qt6ct also answers to "qt5ct".
void qt_environment() {
  const char* chosen = std::getenv("QT_QPA_PLATFORMTHEME");
  if (chosen != nullptr && *chosen) return;
  if (!on_path("qt6ct") && !on_path("qt5ct")) return;
  ::setenv("QT_QPA_PLATFORMTHEME", "qt5ct", 1);
  if (on_path("dbus-update-activation-environment"))
    vw::spawn({"dbus-update-activation-environment", "--systemd", "QT_QPA_PLATFORMTHEME=qt5ct"});
}

void apply_qt(const Colours& c, json& state) {
  json& qt = state["qt"];
  if (!qt.is_object()) qt = json::object();
  json& files = qt["files"];
  if (!files.is_object()) files = json::object();
  qt["applied"] = true;
  bool changed = false;
  for (const auto& [name, count] : {std::pair{"qt6ct", 22}, std::pair{"qt5ct", 21}}) {
    const fs::path folder = config_home() / name;
    const fs::path colours_file = folder / "colors/blxshell.conf";
    const fs::path stylesheet = folder / "qss/blxshell.qss";
    const fs::path conf = folder / (std::string(name) + ".conf");
    changed |= put(colours_file, qt_colours(c, count));
    changed |= put(stylesheet, qt_stylesheet(c));
    back_up(files, conf);
    std::string text = slurp(conf).value_or("");
    text = ini_with(text, "Appearance",
                    {{"color_scheme_path", colours_file.string()}, {"custom_palette", std::string("true")},
                     {"style", std::string("Fusion")}});
    text = ini_with(text, "Interface", {{"stylesheets", stylesheet.string()}});
    if (!put(conf, text) && changed) {
      std::error_code error;
      fs::last_write_time(conf, fs::file_time_type::clock::now(), error);
    }
  }
  const fs::path kdeglobals = config_home() / "kdeglobals";
  back_up(files, kdeglobals);
  put(kdeglobals, kde_colours(slurp(kdeglobals).value_or(""), c));
  qt_environment();
}

void undo_qt(json& state) {
  json& qt = state["qt"];
  if (!qt.is_object() || !qt.value("applied", false)) return;
  if (qt.contains("files") && qt["files"].is_object()) restore(qt["files"]);
  for (const char* name : {"qt6ct", "qt5ct"}) {
    remove_file(config_home() / name / "colors/blxshell.conf");
    remove_file(config_home() / name / "qss/blxshell.qss");
  }
  qt = json::object();
}

}  // namespace

void apply() {
  const Config& c = config();
  json state = load_state();
  const std::string before = state.dump();
  try {
    if (c.theme_gtk_apps) apply_gtk(colours(), state);
    else undo_gtk(state);
    if (c.theme_qt_apps) apply_qt(colours(), state);
    else undo_qt(state);
  } catch (const std::exception&) {
  }
  if (state.dump() != before) save_state(state);
}

}  // namespace blx::apptheme
