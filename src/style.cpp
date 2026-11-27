#include "style.hpp"

#include <vw/vw.hpp>

#include <algorithm>
#include <set>
#include <vector>

#include "config.hpp"

#include <nlohmann/json.hpp>

#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <iterator>
#include <format>
#include <optional>
#include <sys/inotify.h>
#include <unistd.h>

namespace blx {
namespace {

namespace fs = std::filesystem;

fs::path state_directory() {
  const char* home = std::getenv("HOME");
  return fs::path(home != nullptr ? home : ".") / ".local/state/blxshell";
}

std::optional<vw::Color> parse_hex(const std::string& text) {
  if (text.size() != 7 || text[0] != '#') return std::nullopt;
  try {
    return vw::rgb(static_cast<std::uint32_t>(std::stoul(text.substr(1), nullptr, 16)));
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

// Accepts both Matugen's JSON ({"colors": {"primary": {"dark": {"color": ...}}}})
// and ~/shell's flat file ({"primary": "#..."}), like Colours.applyScheme.
vw::Color* role_slot(Colours& c, const std::string& role) {
  const std::pair<const char*, vw::Color*> roles[] = {
      {"primary", &c.primary}, {"primaryText", &c.primary_text}, {"primaryContainer", &c.primary_container},
      {"secondary", &c.secondary}, {"secondaryContainer", &c.secondary_container}, {"tertiary", &c.tertiary},
      {"tertiaryContainer", &c.tertiary_container}, {"error", &c.error}, {"errorText", &c.error_text}, {"errorContainer", &c.error_container},
      {"errorContainerText", &c.error_container_text}, {"surface", &c.surface},
      {"surfaceContainer", &c.surface_container}, {"surfaceContainerLow", &c.surface_container_low},
      {"surfaceContainerHigh", &c.surface_container_high}, {"surfaceContainerHighest", &c.surface_container_highest},
      {"surfaceVariant", &c.surface_variant}, {"text", &c.text}, {"textMuted", &c.text_muted}, {"outline", &c.outline},
      {"outlineVariant", &c.outline_variant}};
  for (const auto& [name, slot] : roles)
    if (role == name) return slot;
  return nullptr;
}

Colours load_scheme() {
  Colours result;
  std::ifstream input(state_directory() / "matugen-colours.json");
  if (!input) return result;
  nlohmann::json scheme;
  try {
    input >> scheme;
  } catch (const std::exception&) {
    return result;
  }
  const bool matugen = scheme.contains("colors");
  const auto read = [&](const char* flat, const char* role, vw::Color& target) {
    std::string value;
    if (matugen) {
      const auto& colors = scheme["colors"];
      // Every file holds both palettes; the config says which one is in use.
      if (colors.contains(role)) value = colors[role][config().matugen_mode == "light" ? "light" : "dark"].value("color", "");
    } else {
      value = scheme.value(flat, "");
    }
    if (auto colour = parse_hex(value)) target = *colour;
  };
  read("primary", "primary", result.primary);
  read("primaryText", "on_primary", result.primary_text);
  read("primaryContainer", "primary_container", result.primary_container);
  read("secondary", "secondary", result.secondary);
  read("secondaryContainer", "secondary_container", result.secondary_container);
  read("tertiary", "tertiary", result.tertiary);
  read("tertiaryContainer", "tertiary_container", result.tertiary_container);
  read("error", "error", result.error);
  read("errorText", "on_error", result.error_text);
  read("errorContainer", "error_container", result.error_container);
  read("errorContainerText", "on_error_container", result.error_container_text);
  read("surface", "surface", result.surface);
  read("surfaceContainer", "surface_container", result.surface_container);
  read("surfaceContainerLow", "surface_container_low", result.surface_container_low);
  read("surfaceContainerHigh", "surface_container_high", result.surface_container_high);
  read("surfaceContainerHighest", "surface_container_highest", result.surface_container_highest);
  read("surfaceVariant", "surface_variant", result.surface_variant);
  read("text", "on_surface", result.text);
  read("textMuted", "on_surface_variant", result.text_muted);
  read("outline", "outline", result.outline);
  read("outlineVariant", "outline_variant", result.outline_variant);
  // Flat files may omit these; fall back like Colours.qml does.
  if (!matugen) {
    // Without the error roles: dark or light text by the error colour's
    // brightness, and error itself as the container.
    const auto readable_on = [](vw::Color colour) {
      const float luminance = 0.2126F * colour[0] + 0.7152F * colour[1] + 0.0722F * colour[2];
      return luminance > 0.5F ? vw::rgb(0x1a1112) : vw::rgb(0xfff8f7);
    };
    if (!scheme.contains("errorText")) result.error_text = readable_on(result.error);
    if (!scheme.contains("errorContainer")) result.error_container = result.error;
    if (!scheme.contains("errorContainerText")) result.error_container_text = readable_on(result.error_container);
    if (!scheme.contains("secondaryContainer")) result.secondary_container = result.primary_container;
    if (!scheme.contains("tertiaryContainer")) result.tertiary_container = result.primary_container;
    if (!scheme.contains("outlineVariant")) result.outline_variant = result.outline;
    if (!scheme.contains("surfaceContainerLow")) result.surface_container_low = result.surface;
    if (!scheme.contains("surfaceContainerHighest"))
      result.surface_container_highest = result.surface_variant;
  }
  return result;
}

// Fixed values per preset and mode, for the roles a preset takes out of the
// generated scheme. The rest keep following the wallpaper.
using Pinned = std::vector<std::pair<const char*, const char*>>;

const Pinned& neutral_surfaces(bool dark) {
  static const Pinned night{{"surface", "#131314"}, {"surfaceContainerLow", "#1b1b1c"}, {"surfaceContainer", "#1f1f20"},
                            {"surfaceContainerHigh", "#2a2a2b"}, {"surfaceContainerHighest", "#353536"},
                            {"surfaceVariant", "#444748"}, {"text", "#e5e2e1"}, {"textMuted", "#c4c7c7"},
                            {"outline", "#8e9192"}, {"outlineVariant", "#444748"}};
  static const Pinned day{{"surface", "#fdf8f8"}, {"surfaceContainerLow", "#f7f2f2"}, {"surfaceContainer", "#f1edec"},
                          {"surfaceContainerHigh", "#ebe7e6"}, {"surfaceContainerHighest", "#e5e2e1"},
                          {"surfaceVariant", "#e0e3e3"}, {"text", "#1c1b1b"}, {"textMuted", "#444748"},
                          {"outline", "#747878"}, {"outlineVariant", "#c4c7c7"}};
  return dark ? night : day;
}

const Pinned& amoled_surfaces(bool dark) {
  static const Pinned night{{"surface", "#000000"}, {"surfaceContainerLow", "#050505"}, {"surfaceContainer", "#0b0b0b"},
                            {"surfaceContainerHigh", "#131313"}, {"surfaceContainerHighest", "#1b1b1b"},
                            {"surfaceVariant", "#222222"}, {"outlineVariant", "#2c2c2c"}};
  static const Pinned day{{"surface", "#ffffff"}, {"surfaceContainerLow", "#fafafa"}, {"surfaceContainer", "#f4f4f4"},
                          {"surfaceContainerHigh", "#eeeeee"}, {"surfaceContainerHighest", "#e8e8e8"},
                          {"surfaceVariant", "#e2e2e2"}};
  return dark ? night : day;
}

const Pinned& mono_accents(bool dark) {
  static const Pinned night{{"primary", "#c6c6c6"}, {"primaryText", "#303030"}, {"primaryContainer", "#474747"},
                            {"secondary", "#c8c6c6"}, {"secondaryContainer", "#474646"},
                            {"tertiary", "#c9c6c5"}, {"tertiaryContainer", "#484646"}};
  static const Pinned day{{"primary", "#5e5e5e"}, {"primaryText", "#ffffff"}, {"primaryContainer", "#e2e2e2"},
                          {"secondary", "#5f5e5e"}, {"secondaryContainer", "#e4e2e1"},
                          {"tertiary", "#605e5d"}, {"tertiaryContainer", "#e6e1e0"}};
  return dark ? night : day;
}

void pin(Colours& colours, const Pinned& roles) {
  for (const auto& [role, hex] : roles)
    if (vw::Color* slot = role_slot(colours, role)) *slot = *parse_hex(hex);
}

// The scheme, the preset's fixed roles, then the user's overrides: those apply
// with no scheme file too.
Colours load() {
  Colours result = load_scheme();
  const Config& settings = config();
  const bool dark = settings.matugen_mode != "light";
  if (settings.colour_preset == "neutral") {
    pin(result, neutral_surfaces(dark));
  } else if (settings.colour_preset == "amoled") {
    pin(result, neutral_surfaces(dark));
    pin(result, amoled_surfaces(dark));
    // Accent containers most of the way to black too, keeping their hue.
    if (dark)
      for (vw::Color* container : {&result.primary_container, &result.secondary_container, &result.tertiary_container,
                                   &result.error_container})
        for (int channel = 0; channel < 3; ++channel) (*container)[channel] *= 0.55F;
  } else if (settings.colour_preset == "mono") {
    pin(result, neutral_surfaces(dark));
    pin(result, mono_accents(dark));
  }
  for (const auto& [role, value] : config().colour_overrides) {
    const auto colour = parse_hex(value);
    vw::Color* target = colour ? role_slot(result, role) : nullptr;
    if (target != nullptr) *target = *colour;
  }
  return result;
}

std::string hex_of(vw::Color colour) {
  const auto channel = [](float value) { return static_cast<int>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255)); };
  return std::format("#{:02x}{:02x}{:02x}", channel(colour[0]), channel(colour[1]), channel(colour[2]));
}

vw::Color blend_colour(vw::Color from, vw::Color to, float t) {
  return {from[0] + (to[0] - from[0]) * t, from[1] + (to[1] - from[1]) * t, from[2] + (to[2] - from[2]) * t, 1};
}

// Ghostty's colours from the shell's own, presets, themes and overrides
// included: ~/.config/ghostty/themes/blxshell, for `theme = blxshell`. Only
// written when Ghostty is configured here and the colours changed; Ghostty
// reloads on SIGUSR2.
// SIGUSR2 reloads Ghostty's config. Matched by /proc comm: a Nix-wrapped
// Ghostty runs as ".ghostty-wrappe" (comm keeps 15 characters).
void reload_ghostty() {
  std::error_code error;
  for (const auto& entry : fs::directory_iterator("/proc", error)) {
    const std::string pid = entry.path().filename().string();
    if (pid.empty() || !std::ranges::all_of(pid, [](char c) { return c >= '0' && c <= '9'; })) continue;
    std::ifstream input(entry.path() / "comm");
    std::string comm;
    std::getline(input, comm);
    if (comm == "ghostty" || comm.starts_with(".ghostty-wrap")) ::kill(std::stoi(pid), SIGUSR2);
  }
}

void export_ghostty(const Colours& c) {
  const char* home = std::getenv("HOME");
  if (home == nullptr) return;
  const fs::path folder = fs::path(home) / ".config/ghostty";
  std::error_code error;
  if (!fs::is_directory(folder, error)) return;
  const bool dark = config().matugen_mode != "light";
  const vw::Color white{1, 1, 1, 1};
  const vw::Color bright = dark ? white : vw::Color{0, 0, 0, 1};
  std::string text = "# Written by blxshell from its colours; changes are overwritten.\n\n";
  const auto line = [&](std::string_view key, vw::Color colour) { text += std::format("{} = {}\n", key, hex_of(colour)); };
  const auto slot = [&](int index, vw::Color colour) { text += std::format("palette = {}={}\n", index, hex_of(colour)); };
  line("background", c.surface);
  line("foreground", c.text);
  line("selection-background", c.primary);
  line("selection-foreground", c.primary_text);
  line("cursor-color", c.primary);
  line("cursor-text", c.primary_text);
  text += "\n";
  slot(0, c.surface_container_high);
  slot(1, c.error);
  slot(2, c.primary);
  slot(3, blend_colour(c.primary, c.tertiary, 0.5F));
  slot(4, c.secondary);
  slot(5, c.tertiary);
  slot(6, blend_colour(c.secondary, c.tertiary, 0.5F));
  slot(7, c.text);
  slot(8, c.outline_variant);
  slot(9, blend_colour(c.error, bright, 0.25F));
  slot(10, blend_colour(c.primary, bright, 0.25F));
  slot(11, blend_colour(blend_colour(c.primary, c.tertiary, 0.5F), bright, 0.25F));
  slot(12, blend_colour(c.secondary, bright, 0.25F));
  slot(13, blend_colour(c.tertiary, bright, 0.25F));
  slot(14, blend_colour(blend_colour(c.secondary, c.tertiary, 0.5F), bright, 0.25F));
  slot(15, blend_colour(c.text, bright, 0.5F));
  const fs::path file = folder / "themes/blxshell";
  {
    std::ifstream existing(file);
    const std::string old((std::istreambuf_iterator<char>(existing)), std::istreambuf_iterator<char>());
    if (old == text) return;
  }
  fs::create_directories(file.parent_path(), error);
  const fs::path temporary = file.string() + ".tmp";
  {
    std::ofstream output(temporary);
    output << text;
    if (!output) return;
  }
  fs::rename(temporary, file, error);
  if (!error) reload_ghostty();
}

Colours load_and_export() {
  Colours result = load();
  export_ghostty(result);
  return result;
}

Colours& current() {
  static Colours instance = load_and_export();
  return instance;
}

// Watches the state directory, since writers replace the file.
int watcher() {
  static const int fd = [] {
    const int descriptor = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (descriptor < 0) return -1;
    std::error_code error;
    fs::create_directories(state_directory(), error);
    inotify_add_watch(descriptor, state_directory().c_str(),
                      IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE);
    return descriptor;
  }();
  return fd;
}

}  // namespace

const Colours& colours() { return current(); }
int colours_event_fd() { return watcher(); }

namespace {
std::uint64_t generation = 0;
}  // namespace

std::uint64_t state_generation() { return generation; }

bool reload_colours() {
  alignas(inotify_event) char buffer[4096];
  bool changed = false;
  while (read(watcher(), buffer, sizeof(buffer)) > 0) changed = true;
  if (changed) {
    current() = load_and_export();
    ++generation;
  }
  return changed;
}

vw::Color role_colour(const std::string& role) {
  const Colours& c = colours();
  if (role == "primary") return c.primary;
  if (role == "primaryText") return c.primary_text;
  if (role == "primaryContainer") return c.primary_container;
  if (role == "secondary") return c.secondary;
  if (role == "secondaryContainer") return c.secondary_container;
  if (role == "tertiary") return c.tertiary;
  if (role == "tertiaryContainer") return c.tertiary_container;
  if (role == "error") return c.error;
  if (role == "errorText") return c.error_text;
  if (role == "errorContainer") return c.error_container;
  if (role == "errorContainerText") return c.error_container_text;
  if (role == "surface") return c.surface;
  if (role == "surfaceContainer") return c.surface_container;
  if (role == "surfaceContainerLow") return c.surface_container_low;
  if (role == "surfaceContainerHigh") return c.surface_container_high;
  if (role == "surfaceContainerHighest") return c.surface_container_highest;
  if (role == "surfaceVariant") return c.surface_variant;
  if (role == "text") return c.text;
  if (role == "textMuted") return c.text_muted;
  if (role == "outlineVariant") return c.outline_variant;
  return c.outline;
}

vw::Color with_alpha(vw::Color color, float alpha) {
  color[3] = alpha;
  return color;
}

vw::Color mix(vw::Color from, vw::Color to, float t) {
  for (std::size_t channel = 0; channel < 4; ++channel)
    from[channel] += (to[channel] - from[channel]) * t;
  return from;
}

void sync_style() {
  // Handed out as const char*: keep every family alive.
  static std::set<std::string> families;
  const Config& c = config();
  const auto family = [](const std::string& name, const char* fallback) {
    return name.empty() ? fallback : families.insert(name).first->c_str();
  };
  font::display = family(c.font_display, "Rubik");
  font::utility = family(c.font_utility, "Google Sans Flex");
  props::frame_thickness = std::clamp(c.frame_thickness, 2.0F, 40.0F);
  props::frame_radius = std::clamp(c.frame_radius, 0.0F, 60.0F);
  props::bar_height = std::clamp(c.bar_height, 40.0F, 80.0F);
  props::blob_smoothing = std::clamp(c.blob_smoothing, 1.0F, 80.0F);
  props::popout_radius = std::clamp(c.panel_radius, 0.0F, 40.0F);
  props::island_radius = std::clamp(c.island_radius, 0.0F, 48.0F);
  props::launcher_radius = std::clamp(c.launcher_radius, 0.0F, 60.0F);
  props::popout_frame_overlap = std::max(0.0F, props::frame_thickness - 2);
  props::animation_speed = std::clamp(c.animation_speed, 0.25F, 4.0F);
  props::motion_style = c.motion_style == "smooth" ? 1 : c.motion_style == "snappy" ? 2 : 0;
  props::hover_linger_ms = std::clamp(c.hover_linger_ms, 0, 2000);
  props::font_scale = std::clamp(c.font_scale, 0.7F, 1.6F);
  props::utility_radius_large = std::clamp(c.card_radius, 0.0F, 36.0F);
  current() = load_and_export();
}

}  // namespace blx
