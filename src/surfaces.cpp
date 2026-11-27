#include "surfaces.hpp"

#include <vw/vw.hpp>

#include <algorithm>
#include <cstdio>
#include <chrono>
#include <functional>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

#include "config.hpp"
#include "draw.hpp"
#include "parts.hpp"
#include "plugins.hpp"
#include "state.hpp"
#include "system.hpp"
#include "services.hpp"
#include "style.hpp"

namespace blx {
namespace {

Screen screen_size() {
  const vw::Size size = vw::surface_size();
  return {size.width > 0 ? static_cast<float>(size.width) : 1920.0F,
          size.height > 0 ? static_cast<float>(size.height) : 1080.0F};
}

// An input region rounded outwards and clipped to the surface: vw rejects
// regions that reach past the surface edge.
std::array<std::int32_t, 4> region(const Screen& screen, float x, float y, float width,
                                   float height) {
  const auto left = static_cast<std::int32_t>(std::clamp(std::floor(x), 0.0F, screen.width));
  const auto top = static_cast<std::int32_t>(std::clamp(std::floor(y), 0.0F, screen.height));
  const auto right = static_cast<std::int32_t>(std::clamp(std::ceil(x + width), 0.0F, screen.width));
  const auto bottom = static_cast<std::int32_t>(std::clamp(std::ceil(y + height), 0.0F, screen.height));
  return {left, top, std::max(0, right - left), std::max(0, bottom - top)};
}

std::string home() {
  const char* value = std::getenv("HOME");
  return value != nullptr ? value : ".";
}

// The wallpaper that ships with blxshell: <prefix>/share/blxshell/wallpapers,
// or the source tree while developing. The greeter finds it the same way.
std::string default_wallpaper() {
  static const std::string path = [] {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;
    std::error_code error;
    const auto executable = fs::read_symlink("/proc/self/exe", error);
    if (!error) candidates.push_back(executable.parent_path().parent_path() / "share" / "blxshell" / "wallpapers");
#ifdef BLXSHELL_SOURCE_WALLPAPER_DIR
    candidates.emplace_back(BLXSHELL_SOURCE_WALLPAPER_DIR);
#endif
    for (const auto& directory : candidates)
      if (fs::exists(directory / "runix-m3.png", error)) return (directory / "runix-m3.png").string();
    return std::string();
  }();
  return path;
}

std::string read_wallpaper_path() {
  std::ifstream input(home() + "/.local/state/blxshell/wallpaper");
  std::string path;
  std::getline(input, path);
  // Until one is picked, and when the picked one is gone, the default.
  std::error_code error;
  if (path.empty() || !std::filesystem::exists(path, error)) path = default_wallpaper();
  return path;
}

// Regenerates the colour scheme from a new wallpaper, with the user's Matugen
// config when there is one; the scheme file watcher picks up the result.
void generate_scheme(const std::string& wallpaper) {
  const std::string state = home() + "/.local/state/blxshell";
  if (config().theme_source == "colour") {
    vw::spawn({"sh", "-c",
               "command -v matugen >/dev/null || exit 0; mkdir -p \"$2\" && "
               "tmp=\"$2/matugen-colours.json.$$\"; "
               "file=\"$HOME/.config/matugen/config.toml\"; [ -f \"$file\" ] && found=1; "
               "matugen color hex \"$1\" ${found:+-c} ${found:+\"$file\"} -m \"$3\" "
               "-t \"$4\" --contrast \"$5\" -j hex > \"$tmp\" && "
               "mv \"$tmp\" \"$2/matugen-colours.json\" || rm -f \"$tmp\"",
               "sh", config().theme_seed, state, config().matugen_mode, config().scheme_type(),
               std::format("{}", config().scheme_contrast())});
    return;
  }
  vw::spawn({"sh", "-c",
             "command -v matugen >/dev/null || exit 0; mkdir -p \"$2\" && "
             "tmp=\"$2/matugen-colours.json.$$\"; "
             "file=\"$HOME/.config/matugen/config.toml\"; [ -f \"$file\" ] && found=1; "
               "matugen image \"$1\" ${found:+-c} ${found:+\"$file\"} -m \"$3\" "
             "-t \"$4\" --contrast \"$5\" --prefer \"$6\" -j hex > \"$tmp\" && "
             "mv \"$tmp\" \"$2/matugen-colours.json\" || rm -f \"$tmp\"",
             "sh", wallpaper, state, config().matugen_mode, config().scheme_type(),
             std::format("{}", config().scheme_contrast()), config().matugen_prefer});
}

void reserve(vw::Ui& ui, std::uint32_t edge, float zone) {
  // A fullscreen window gets the whole output: drawing nothing unmaps the
  // reservation and gives its space back.
  if (system::fullscreen()) return;
  const bool vertical = edge == vw::anchor::left || edge == vw::anchor::right;
  ui.rectangle({.width = 1, .height = 1, .color = vw::rgba(0)});
  ui.surface({.width = vertical ? 1.0F : 0.0F, .height = vertical ? 0.0F : 1.0F, .anchor = edge,
              .exclusive_zone = static_cast<std::int32_t>(zone),
              .surface_namespace = "blxshell-reserve",
              .input_regions = std::vector<std::array<std::int32_t, 4>>{},
              // Only a config change (an event, which rebuilds every surface) moves these.
              .refresh_interval = 3600});
}

}  // namespace

// BLXSHELL_DEBUG_FRAMES=1 reports slow frames during a sidebar pull.
struct DragFrameLog {
  using Clock = std::chrono::steady_clock;
  Clock::time_point begin = Clock::now();
  ~DragFrameLog() {
    static const bool enabled = std::getenv("BLXSHELL_DEBUG_FRAMES") != nullptr;
    if (!enabled || (!shell().sidebar_dragging && !shell().stack_dragging)) return;
    static Clock::time_point last{};
    const auto end = Clock::now();
    const auto ms = [](auto d) { return std::chrono::duration<double, std::milli>(d).count(); };
    if (last != Clock::time_point{} && ms(end - last) > 40)
      std::fprintf(stderr, "blxshell: drag frame gap %.1f ms (build %.1f ms)\n", ms(end - last), ms(end - begin));
    last = end;
  }
};

// The wallpaper, small and blurred, for the shell's surface (and the lock
// screen) to show through. It is copied into every blob draw, so it stays
// small; the blur runs once per wallpaper or setting change.
constexpr int frost_width = 240;
constexpr int frost_height = 135;
// The blur works on a larger copy so its first passes still see detail.
constexpr int frost_scale = 4;

struct Frost {
  std::string key;
  std::vector<float> pixels;
  std::uint64_t generation = ~0ULL;
  int passes = -1;
  float offset = -1;
  std::string source;
};

// Linear RGB-ish working image: three floats per pixel.
struct Plane {
  int width = 0;
  int height = 0;
  std::vector<float> rgb;
};

// Bilinear, clamped to the edge; x and y in pixels, centres at i + 0.5.
void sample(const Plane& plane, float x, float y, float weight, float* out) {
  x = std::clamp(x - 0.5F, 0.0F, static_cast<float>(plane.width - 1));
  y = std::clamp(y - 0.5F, 0.0F, static_cast<float>(plane.height - 1));
  const int x0 = static_cast<int>(x);
  const int y0 = static_cast<int>(y);
  const int x1 = std::min(x0 + 1, plane.width - 1);
  const int y1 = std::min(y0 + 1, plane.height - 1);
  const float tx = x - static_cast<float>(x0);
  const float ty = y - static_cast<float>(y0);
  const auto at = [&](int px, int py) { return &plane.rgb[(static_cast<std::size_t>(py) * plane.width + px) * 3]; };
  const float* a = at(x0, y0);
  const float* b = at(x1, y0);
  const float* c = at(x0, y1);
  const float* d = at(x1, y1);
  for (int channel = 0; channel < 3; ++channel) {
    const float top = a[channel] + (b[channel] - a[channel]) * tx;
    const float bottom = c[channel] + (d[channel] - c[channel]) * tx;
    out[channel] += (top + (bottom - top) * ty) * weight;
  }
}

// Dual Kawase downsample: half the size, centre plus four diagonal taps.
Plane kawase_down(const Plane& source, float offset) {
  Plane result{std::max(1, source.width / 2), std::max(1, source.height / 2), {}};
  result.rgb.assign(static_cast<std::size_t>(result.width) * result.height * 3, 0.0F);
  const float sx = static_cast<float>(source.width) / static_cast<float>(result.width);
  const float sy = static_cast<float>(source.height) / static_cast<float>(result.height);
  for (int y = 0; y < result.height; ++y)
    for (int x = 0; x < result.width; ++x) {
      float* out = &result.rgb[(static_cast<std::size_t>(y) * result.width + x) * 3];
      const float cx = (static_cast<float>(x) + 0.5F) * sx;
      const float cy = (static_cast<float>(y) + 0.5F) * sy;
      sample(source, cx, cy, 4.0F / 8.0F, out);
      sample(source, cx - offset, cy - offset, 1.0F / 8.0F, out);
      sample(source, cx + offset, cy - offset, 1.0F / 8.0F, out);
      sample(source, cx - offset, cy + offset, 1.0F / 8.0F, out);
      sample(source, cx + offset, cy + offset, 1.0F / 8.0F, out);
    }
  return result;
}

// Dual Kawase upsample to width x height: a ring of eight taps.
Plane kawase_up(const Plane& source, int width, int height, float offset) {
  Plane result{width, height, {}};
  result.rgb.assign(static_cast<std::size_t>(width) * height * 3, 0.0F);
  const float sx = static_cast<float>(source.width) / static_cast<float>(width);
  const float sy = static_cast<float>(source.height) / static_cast<float>(height);
  const float o = offset * 0.5F;
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      float* out = &result.rgb[(static_cast<std::size_t>(y) * width + x) * 3];
      const float cx = (static_cast<float>(x) + 0.5F) * sx;
      const float cy = (static_cast<float>(y) + 0.5F) * sy;
      sample(source, cx - o * 2, cy, 1.0F / 12.0F, out);
      sample(source, cx + o * 2, cy, 1.0F / 12.0F, out);
      sample(source, cx, cy - o * 2, 1.0F / 12.0F, out);
      sample(source, cx, cy + o * 2, 1.0F / 12.0F, out);
      sample(source, cx - o, cy - o, 2.0F / 12.0F, out);
      sample(source, cx + o, cy - o, 2.0F / 12.0F, out);
      sample(source, cx - o, cy + o, 2.0F / 12.0F, out);
      sample(source, cx + o, cy + o, 2.0F / 12.0F, out);
    }
  return result;
}

// Rebuilt when the wallpaper, passes or offset change; nullptr without one.
// Called on every build (each animation frame): touches the filesystem only
// when the state directory or the settings changed.
const Frost* frosted_wallpaper(Frost& frost, int passes, float offset, const std::string& source = {}) {
  passes = std::clamp(passes, 1, 6);
  offset = std::clamp(offset, 0.25F, 8.0F);
  if (frost.generation == state_generation() && frost.passes == passes && frost.offset == offset && frost.source == source)
    return frost.pixels.empty() ? nullptr : &frost;
  frost.generation = state_generation();
  frost.passes = passes;
  frost.offset = offset;
  frost.source = source;
  const std::string path = source.empty() ? read_wallpaper_path() : source;
  std::error_code error;
  const auto modified = std::filesystem::last_write_time(path, error);
  if (error) {
    frost.key.clear();
    frost.pixels.clear();
    return nullptr;
  }
  const std::string key = std::format("{}|{}|{}|{}", path, passes, offset, modified.time_since_epoch().count());
  if (key == frost.key) return frost.pixels.empty() ? nullptr : &frost;
  frost.key = key;
  frost.pixels.clear();
  try {
    // vw's image cache does the decoding and area-averaged downscale.
    constexpr int source_width = frost_width * frost_scale;
    constexpr int source_height = frost_height * frost_scale;
    vw::Scene scene;
    vw::Ui image_ui(scene);
    image_ui.image({.path = path, .width = source_width, .height = source_height, .fit = vw::ImageFit::cover});
    if (scene.rasters.empty() || scene.rasters[0].rgba == nullptr) return nullptr;
    const vw::Raster& raster = scene.rasters[0];
    if (static_cast<int>(raster.width) != source_width || static_cast<int>(raster.height) != source_height) return nullptr;
    Plane plane{source_width, source_height, {}};
    const std::size_t count = static_cast<std::size_t>(source_width) * source_height;
    plane.rgb.resize(count * 3);
    for (std::size_t index = 0; index < count; ++index)
      for (int channel = 0; channel < 3; ++channel)
        plane.rgb[index * 3 + channel] = static_cast<float>(((*raster.rgba)[index] >> (channel * 8)) & 255U);

    // Down `passes` times, then back up through the same sizes.
    std::vector<std::pair<int, int>> sizes;
    for (int pass = 0; pass < passes && plane.width > 1 && plane.height > 1; ++pass) {
      sizes.emplace_back(plane.width, plane.height);
      plane = kawase_down(plane, offset);
    }
    for (auto size = sizes.rbegin(); size != sizes.rend(); ++size)
      plane = kawase_up(plane, size->first, size->second, offset);

    // Area-average down to the backdrop size.
    frost.pixels.resize(static_cast<std::size_t>(frost_width) * frost_height);
    constexpr float area = static_cast<float>(frost_scale * frost_scale);
    for (int y = 0; y < frost_height; ++y)
      for (int x = 0; x < frost_width; ++x) {
        std::array<float, 3> sum{};
        for (int dy = 0; dy < frost_scale; ++dy)
          for (int dx = 0; dx < frost_scale; ++dx) {
            const std::size_t at =
                (static_cast<std::size_t>(y * frost_scale + dy) * source_width + x * frost_scale + dx) * 3;
            for (int channel = 0; channel < 3; ++channel) sum[channel] += plane.rgb[at + channel];
          }
        const auto byte = [&](int channel) {
          return static_cast<std::uint32_t>(std::clamp(std::lround(sum[channel] / area), 0L, 255L));
        };
        // Exact in a float, and unlike raw bits can never be a NaN (which would
        // never compare equal, so every rebuild would render).
        frost.pixels[static_cast<std::size_t>(y) * frost_width + x] =
            static_cast<float>(byte(0) | byte(1) << 8U | byte(2) << 16U);
      }
  } catch (const std::exception&) {
    frost.pixels.clear();
  }
  return frost.pixels.empty() ? nullptr : &frost;
}

// The shell's frost: the old 1-12 strength, as Kawase passes and spread.
const Frost* frosted_wallpaper() {
  static Frost frost;
  const int strength = std::clamp(config().frosted_strength, 1, 12);
  return frosted_wallpaper(frost, 1 + (strength + 1) / 3, 1.0F + static_cast<float>(strength) * 0.15F);
}

bool apply_lock_frost(blob::Group& group) {
  return apply_lock_frost(group, config().lock_blur_passes, config().lock_blur_offset);
}

bool apply_lock_frost(blob::Group& group, int passes, float offset, const std::string& wallpaper) {
  static Frost frost;
  const Frost* blurred = frosted_wallpaper(frost, passes, offset, wallpaper);
  if (blurred == nullptr) return false;
  group.backdrop(&blurred->pixels, frost_width, frost_height);
  return true;
}

void shell_surface(vw::Ui& ui) {
  const DragFrameLog log;
  const Screen screen = screen_size();
  // shell.qml: a fullscreen window closes the popups, hides the contents and
  // input, and animates the frame away.
  const bool fullscreen = system::fullscreen();
  static bool was_fullscreen = false;
  if (fullscreen && !was_fullscreen) {
    Shell& state = shell();
    state.launcher_open = false;
    state.power_pinned = false;
    state.quick_share_open = false;
    state.sidebar_reveal = 0;
    state.stack_reveal = 0;
    state.audio_until = {};
    state.audio_tug_until = {};
    state.audio_dragging = false;
  }
  was_fullscreen = fullscreen;
  const float hidden = static_cast<float>(vw::animate("shell.fullscreen", fullscreen ? 1 : 0, motion::standard()));
  const float frame = props::frame_thickness * (1 - hidden);
  const float bar = props::bar_height * (1 - hidden);
  const Panel launcher = launcher_panel(screen);
  const Panel dashboard = dashboard_panel(screen);
  const Panel notifications = notifications_panel(screen);
  const Panel power = power_panel(screen);
  const Panel status = status_panel(screen);
  const Panel tray = tray_panel(screen);
  const Sidebar sidebar = sidebar_panels(screen);
  // Where the sidebar meets the audio OSD's height, if it does.
  const float osd_middle = middle(screen);
  float sidebar_edge = screen.width;
  for (const Panel* section : {&sidebar.centre, &sidebar.stack})
    if (section->visible() && osd_middle >= section->box.y && osd_middle < section->box.y + section->box.height)
      sidebar_edge = section->box.x;
  const Panel audio = audio_panel(screen, power, sidebar_edge);
  const Panel island = island_panel(screen, shell().dashboard_hovered || !config().island_enabled);
  const Panel share = share_panel(screen);
  const Panel* panels[] = {&dashboard, &notifications, &launcher,     &power,      &status,
                           &tray,      &audio,         &sidebar.centre, &sidebar.stack, &sidebar.menu,
                           &island,    &share};

  // One SDF surface: the frame and every open panel, melted together.
  const float opacity = std::clamp(config().global_opacity, 0.0F, 1.0F);
  blob::Group group(with_alpha(colours().surface, opacity), props::blob_smoothing);
  const Frost* frost = config().frosted ? frosted_wallpaper() : nullptr;
  if (frost != nullptr) group.backdrop(&frost->pixels, frost_width, frost_height);
  group.shadows((opacity >= 0.999F || frost != nullptr) && config().shadow_enabled, std::clamp(config().shadow_strength, 0.0F, 1.0F),
                static_cast<float>(std::clamp(config().shadow_blur, 4, 80)));
  if (config().sdf_outline_enabled) group.outline(role_colour(config().sdf_outline_color_role), config().sdf_outline_width);
  group.frame({frame, frame, screen.width - frame * 2, screen.height - frame - bar,
               blob::Radii::all(props::frame_radius * (1 - hidden))});
  if (!fullscreen)
    for (const Panel* panel : panels) group.add(panel->box);
  if (hidden < 0.999F) group.draw(ui, screen.width, screen.height);
  if (fullscreen) {
    // Once the frame has slid away, the whole surface goes: nothing of the
    // shell stays mapped over a fullscreen window.
    if (hidden >= 0.999F) return;
    ui.surface({.width = 0, .height = 0, .anchor = vw::anchor::fill, .layer = vw::Layer::top, .exclusive_zone = -1,
                .surface_namespace = "blxshell", .input_regions = std::vector<std::array<std::int32_t, 4>>{}});
    return;
  }

  // Hover regions go under everything that can be clicked.
  dashboard_trigger(ui, screen, dashboard);
  // The OSD stays out of a sidebar in motion: the pointer crossing its zone
  // mid-pull would open it and redraw its sliders every frame.
  const bool sidebar_moving = shell().sidebar_dragging || shell().stack_dragging ||
                              (sidebar.centre.visible() && std::abs(sidebar.centre.reveal - 1) > 0.01F) ||
                              (sidebar.stack.visible() && std::abs(sidebar.stack.reveal - 1) > 0.01F);
  if (sidebar_moving) shell().audio_hovered = false;
  else audio_trigger(ui, screen, audio, sidebar_edge);
  sidebar_tug(ui, screen);

  draw_bar(ui, screen);
  draw_dashboard(ui, dashboard);
  draw_notifications(ui, notifications);
  draw_launcher(ui, launcher);
  draw_power(ui, power);
  draw_status(ui, status);
  draw_tray_actions(ui, tray);
  draw_audio(ui, audio);
  draw_sidebar(ui, screen, sidebar);
  draw_island(ui, island);
  draw_share(ui, share);

  // The middle stays click-through: only the frame, the open panels and the
  // audio tug strip take input.
  const auto pull = static_cast<float>(config().sidebar_pull_area);
  const blob::Box tug = audio_tug_area(screen, sidebar_edge);
  std::vector<std::array<std::int32_t, 4>> input{
      region(screen, 0, 0, screen.width, frame),
      region(screen, 0, 0, frame + 12, screen.height),
      region(screen, screen.width - frame - 12, 0, frame + 12, screen.height),
      region(screen, 0, screen.height - bar, screen.width, bar),
      region(screen, screen.width - pull, 0, pull, pull),
      region(screen, screen.width - pull, screen.height - bar - pull, pull, pull),
      region(screen, tug.x, tug.y, tug.width, tug.height)};
  for (const Panel* panel : panels)
    if (panel->visible())
      if (const auto box = region(screen, panel->box.x, panel->box.y, panel->box.width, panel->box.height);
          box[2] > 0 && box[3] > 0)
        input.push_back(box);

  // A pull must not lose the pointer when the panel moves out from under it:
  // the compositor would stop sending motion to the click-through area.
  if (shell().sidebar_dragging || shell().stack_dragging) input = {region(screen, 0, 0, screen.width, screen.height)};

  ui.surface({.width = 0, .height = 0, .anchor = vw::anchor::fill, .layer = vw::Layer::top,
              .exclusive_zone = -1, .surface_namespace = "blxshell",
              .input_regions = std::move(input)});
}

namespace {

// The active workspace number, for the wallpaper parallax.
int active_workspace() {
  const auto list = system::workspaces();
  for (std::size_t index = 0; index < list.size(); ++index)
    if (list[index].active) {
      try {
        return std::stoi(list[index].name);
      } catch (const std::exception&) {
        return static_cast<int>(index) + 1;
      }
    }
  return 1;
}

struct WidgetDrag {
  bool active = false;
  float start_x = 0;
  float start_y = 0;
  float x = 0;
  float y = 0;
};

// A background widget card (clock or weather) at its saved fractional
// position; draggable while widgets are being edited.
void background_widget(vw::Ui& ui, const Screen& screen, const char* name, float& fraction_x, float& fraction_y,
                       float width, float height, WidgetDrag& drag,
                       std::vector<std::array<std::int32_t, 4>>& input,
                       const std::function<void(float x, float y)>& paint) {
  const Colours& c = colours();
  float x = fraction_x * (screen.width - width);
  float y = fraction_y * (screen.height - height);
  if (drag.active) {
    x = std::clamp(drag.x, 0.0F, screen.width - width);
    y = std::clamp(drag.y, 0.0F, screen.height - height);
  }
  ui.rectangle({.width = width, .height = height, .radius = props::card_radius,
                .color = with_alpha(c.surface_container, std::clamp(config().background_widget_opacity, 0.0F, 1.0F)), .place = {.x = x, .y = y}});
  paint(x, y);
  if (!config().background_widgets_editing) return;
  input.push_back({static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), static_cast<std::int32_t>(width),
                   static_cast<std::int32_t>(height)});
  ui.hit_area({.width = width, .height = height, .place = {.x = x, .y = y},
               .input = {.on_drag_start = [&drag, x, y](const vw::DragEvent& e) {
                           drag = {true, static_cast<float>(e.start_x) - x, static_cast<float>(e.start_y) - y, x, y};
                         },
                         .on_drag = [&drag](const vw::DragEvent& e) {
                           drag.x = static_cast<float>(e.x) - drag.start_x;
                           drag.y = static_cast<float>(e.y) - drag.start_y;
                         },
                         .on_drag_end = [&drag, &fraction_x, &fraction_y, screen, width, height, name](const vw::DragEvent&) {
                           // Config.setBackgroundWidgetPosition
                           fraction_x = std::clamp(drag.x, 0.0F, screen.width - width) / std::max(1.0F, screen.width - width);
                           fraction_y = std::clamp(drag.y, 0.0F, screen.height - height) / std::max(1.0F, screen.height - height);
                           drag.active = false;
                           (void)name;
                           save_config();
                         }}});
}

}  // namespace

void wallpaper_surface(vw::Ui& ui) {
  const Screen screen = screen_size();
  const Colours& c = colours();
  Config& settings = config();
  // Re-read only when the state directory changed (the colours watcher).
  static std::uint64_t seen = ~0ULL;
  static std::string path;
  if (seen != state_generation()) {
    seen = state_generation();
    path = read_wallpaper_path();
  }
  static std::string last;
  if (path != last) {
    last = path;
    generate_scheme(path);
  }
  ui.rectangle({.width = screen.width, .height = screen.height, .color = c.surface});

  // Entrance: on launch and again each time the session unlocks. A fresh id
  // per reveal restarts the tween from 0. Only clips, offsets and an overlay
  // move, never the image size, so every frame reuses the scaled wallpaper.
  static bool was_locked = false;
  static int reveals = 0;
  if (was_locked && !shell().locked) ++reveals;
  was_locked = shell().locked;
  const std::string& transition = settings.wallpaper_transition;
  const bool animated = transition == "iris" || transition == "fade" || transition == "rise";
  const float reveal =
      animated ? std::clamp(static_cast<float>(vw::animate(
                                std::format("wallpaper.reveal.{}", reveals), 1,
                                {.duration = transition == "iris" ? 1.2 : 0.9, .initial = 0,
                                 .easing = transition == "iris" ? vw::Easing::in_out_cubic : vw::Easing::out_expo})),
                            0.0F, 1.0F)
               : 1.0F;
  std::optional<vw::Clip> iris;
  float rise = 0;
  if (reveal < 1 && transition == "iris") {
    // A circle growing from the centre until it covers the corners.
    const float radius = std::max(1.0F, reveal * std::hypot(screen.width, screen.height) / 2);
    iris = vw::Clip{screen.width / 2 - radius, screen.height / 2 - radius, radius * 2, radius * 2, radius};
  } else if (reveal < 1 && transition == "rise") {
    rise = (1 - reveal) * screen.height * 0.08F;
  }

  // Parallax: an oversized wallpaper shifted by workspace (Background.qml).
  const float strength = settings.wallpaper_parallax ? settings.wallpaper_parallax_strength : 0;
  const float width = screen.width * (1 + strength * 2);
  const float height = screen.height * (1 + strength * 2);
  const float target = static_cast<float>((active_workspace() - 1) % 5) / 4 - 0.5F;
  // Glide to the new workspace's offset. Only the position animates: the
  // size stays put, so the scaled wallpaper is reused on every frame.
  const auto position = static_cast<float>(
      vw::animate("wallpaper.parallax", target, {.duration = 0.7, .easing = vw::Easing::out_expo}));
  try {
    ui.image({.path = path, .width = width, .height = height, .fit = vw::ImageFit::cover,
              .place = {.x = (screen.width - width) / 2 + position * screen.width * strength,
                        .y = (screen.height - height) / 2 + rise, .clip = iris}});
  } catch (const std::exception&) {
  }

  std::vector<std::array<std::int32_t, 4>> input;
  if (settings.background_clock_enabled) {
    const auto clock_size = static_cast<float>(std::clamp(settings.background_clock_size, 24, 200));
    static WidgetDrag drag;
    const auto now = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()};
    const std::string time = system::format_time(settings.background_clock_format);
    const auto today = std::chrono::year_month_day{std::chrono::floor<std::chrono::days>(now.get_local_time())};
    const std::string date = system::format_time(settings.background_date_format);
    const float content_width = std::max(draw::measure(time, clock_size, vw::weight::black, font::display),
                                         draw::measure(date, 13, vw::weight::semibold, font::display));
    const float content_height = 82 - 4 + 17;
    background_widget(ui, screen, "clock", settings.background_clock_x, settings.background_clock_y,
                      std::max(230.0F, content_width + 36), content_height + 34, drag, input,
                      [&](float x, float y) {
                        const float bottom = y + content_height + 34 - 17;
                        draw::text(ui, date, x + 18, bottom - 17,
                                   {.size = 13, .color = with_alpha(c.secondary, 0.92F), .weight = vw::weight::semibold,
                                    .family = font::display});
                        draw::text(ui, time, x + 18, bottom - 17 + 4 - 82,
                                   {.size = clock_size, .color = with_alpha(c.primary, 0.92F), .weight = vw::weight::black,
                                    .family = font::display});
                      });
  }
  if (const auto report = weather::current(); settings.background_weather_enabled && report) {
    static WidgetDrag drag;
    const std::string line = report->location.empty() ? report->condition
                                                      : report->condition + "  ·  " + report->location;
    const float top_width = 26 + 8 + draw::measure(report->temperature, 25, vw::weight::black, font::display);
    const float line_width = draw::measure(line, 11, vw::weight::semibold, font::display);
    const float content_width = std::max(top_width, line_width);
    const float content_height = 31 + 2 + 14;
    background_widget(ui, screen, "weather", settings.background_weather_x, settings.background_weather_y,
                      std::max(220.0F, content_width + 36), content_height + 34, drag, input,
                      [&](float x, float y) {
                        const float right = x + std::max(220.0F, content_width + 36) - 18;
                        const float bottom = y + content_height + 34 - 17;
                        const float temperature_width = draw::measure(report->temperature, 25, vw::weight::black, font::display);
                        draw::text(ui, report->temperature, right - temperature_width, bottom - 14 - 2 - 31,
                                   {.size = 25, .color = with_alpha(c.primary, 0.92F), .weight = vw::weight::black,
                                    .family = font::display});
                        draw::glyph(ui, report->glyph, right - temperature_width - 8 - 13, bottom - 14 - 2 - 15, 26,
                                    with_alpha(c.tertiary, 0.92F));
                        draw::text(ui, line, right - line_width, bottom - 14,
                                   {.size = 11, .color = with_alpha(c.secondary, 0.92F), .weight = vw::weight::semibold,
                                    .family = font::display});
                      });
  }
  // Plugins' desktop icons, over the wallpaper and under the widgets' veil.
  // A plugin's Wallpaper view (dynamic or layered wallpapers) over the image.
  {
    const auto list = system::workspaces();
    int index = 0;
    for (std::size_t i = 0; i < list.size(); ++i)
      if (list[i].active) index = static_cast<int>(i);
    plugins::draw_wallpaper(ui, screen.width, screen.height, index, static_cast<int>(list.size()));
  }
  draw_desktop(ui, screen, input);
  // Fade and rise come up out of the surface colour, widgets included (the
  // iris only clips the wallpaper).
  const float veil = transition == "iris" ? 0.0F : 1 - reveal;
  if (veil > 0.001F)
    ui.rectangle({.width = screen.width, .height = screen.height, .color = with_alpha(c.surface, veil),
                  .place = {.x = 0, .y = 0}});
  ui.surface({.width = 0, .height = 0, .anchor = vw::anchor::fill, .layer = vw::Layer::background,
              .exclusive_zone = -1, .surface_namespace = "blxshell-background",
              .input_regions = std::move(input)});
}

void regenerate_colours() { generate_scheme(read_wallpaper_path()); }

void set_wallpaper(const std::string& path) {
  const std::string state = home() + "/.local/state/blxshell";
  std::error_code error;
  std::filesystem::create_directories(state, error);
  {
    std::ofstream output(state + "/wallpaper.tmp");
    output << path << '\n';
  }
  std::filesystem::rename(state + "/wallpaper.tmp", state + "/wallpaper", error);
  config().wallpaper_path = path;
  save_config();
}

void reserve_left(vw::Ui& ui) { reserve(ui, vw::anchor::left, props::frame_thickness); }
void reserve_top(vw::Ui& ui) { reserve(ui, vw::anchor::top, props::frame_thickness); }
void reserve_right(vw::Ui& ui) { reserve(ui, vw::anchor::right, props::frame_thickness); }
void reserve_bottom(vw::Ui& ui) { reserve(ui, vw::anchor::bottom, props::bar_height); }

}  // namespace blx
