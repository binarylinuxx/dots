#include "style.hpp"

#include <algorithm>
#include <set>
#include <vector>

#include "config.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
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
  static const Pinned night{{"surface", "#000000"}, {"surfaceContainerLow", "#0a0a0a"}, {"surfaceContainer", "#121212"},
                            {"surfaceContainerHigh", "#1c1c1c"}, {"surfaceContainerHighest", "#262626"},
                            {"surfaceVariant", "#2e2e2e"}};
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

Colours& current() {
  static Colours instance = load();
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
    current() = load();
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
  current() = load();
}

}  // namespace blx
