#include "widgets.hpp"

#include <vw/animation.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <map>
#include <numbers>
#include <optional>

#include "draw.hpp"

namespace blx::widgets {
namespace {

using Clock = std::chrono::steady_clock;
constexpr double pi = std::numbers::pi;

std::string num(double value) { return std::format("{:.3f}", value); }

void canvas(vw::Ui& ui, const std::string& id, float x, float y, float width, float height,
            const std::string& code, bool animate = false) {
  if (width < 1 || height < 1) return;
  try {
    ui.canvas({.id = id, .code = code, .width = std::ceil(width), .height = std::ceil(height),
               .animate = animate, .place = {.x = x, .y = y}});
  } catch (const std::exception&) {
    // A canvas that fails leaves its area empty rather than the shell.
  }
}

double unit(double value) { return std::clamp(value, 0.0, 1.0); }

// shaders/wave.comp: BLXSHELL_SHADER_DIR, the build tree, then
// <prefix>/share/blxshell.
const std::string& wave_shader() {
  static const std::string path = [] {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;
    if (const char* directory = std::getenv("BLXSHELL_SHADER_DIR"); directory && *directory)
      candidates.emplace_back(directory);
#ifdef BLXSHELL_BUILD_SHADER_DIR
    candidates.emplace_back(BLXSHELL_BUILD_SHADER_DIR);
#endif
    std::error_code error;
    const auto executable = fs::read_symlink("/proc/self/exe", error);
    if (!error) candidates.push_back(executable.parent_path().parent_path() / "share" / "blxshell");
    for (const auto& directory : candidates)
      if (fs::exists(directory / "wave.vws", error)) return (directory / "wave.vws").string();
    return std::string();
  }();
  return path;
}

// Seconds on a steady clock, for wave phases.
double seconds() {
  static const Clock::time_point start = Clock::now();
  return std::chrono::duration<double>(Clock::now() - start).count();
}

// A full turn of phase every `period` seconds.
float phase(double period) { return static_cast<float>(std::fmod(seconds(), period) / period * 2 * pi); }

// One wave.comp stroke in the rectangle (x, y, width, height); `data` as
// the shader reads it, positions from the rectangle's corner.
void wave_stroke(vw::Ui& ui, float x, float y, float width, float height, vw::Color colour,
                 std::vector<float> data) {
  if (wave_shader().empty() || width < 1 || height < 1 || colour[3] <= 0) return;
  ui.rectangle({.width = width, .height = height, .color = colour, .shader = wave_shader(),
                .shader_data = std::move(data), .place = {.x = x, .y = y}});
}

// An arc of a circle of `radius` centred in a size x size square at (x, y),
// from `start` for `sweep` degrees (clockwise from +x); wavy with amplitude.
void arc(vw::Ui& ui, float x, float y, float size, float radius, float stroke, double start, double sweep,
         vw::Color colour, float amplitude = 0, float waves_per_radian = 0, float wave_phase = 0) {
  if (sweep <= 0.05) return;
  constexpr double degrees = pi / 180;
  wave_stroke(ui, x, y, size, size, colour,
              {1, stroke, amplitude, wave_phase, waves_per_radian, size / 2, size / 2, radius,
               static_cast<float>(start * degrees), static_cast<float>(std::min(sweep, 359.99) * degrees)});
}

// A slider's drag, so it shows where the pointer is while dragging and keeps
// that value after release until the source (a player's position) catches up.
struct SliderDrag {
  bool dragging = false;
  double value = 0;
  Clock::time_point released{};
};
std::map<std::string, SliderDrag>& slider_drags() {
  static std::map<std::string, SliderDrag> drags;
  return drags;
}

// Material shapes as smooth polar profiles: r(t) between 1-depth and 1.
struct Profile {
  int lobes;
  double depth;
  double sharpness;  // >1 pinches the peaks, <1 rounds them
  double rotation;   // radians
};

std::optional<Profile> profile(const std::string& type) {
  if (type == "sunny") return Profile{8, 0.2, 1.0, pi / 8};
  if (type == "verysunny") return Profile{8, 0.3, 1.6, 0};
  if (type == "cookie4sided") return Profile{4, 0.16, 1.0, pi / 4};
  if (type == "cookie6sided") return Profile{6, 0.14, 1.0, 0};
  if (type == "cookie7sided") return Profile{7, 0.2, 1.0, 0};
  if (type == "cookie9sided") return Profile{9, 0.16, 1.0, 0};
  if (type == "cookie12sided") return Profile{12, 0.14, 1.0, 0};
  if (type == "clover4leaf") return Profile{4, 0.42, 0.7, pi / 4};
  if (type == "clover8leaf") return Profile{8, 0.26, 0.8, 0};
  if (type == "burst") return Profile{12, 0.24, 2.4, 0};
  if (type == "softburst") return Profile{10, 0.22, 1.5, 0};
  if (type == "flower") return Profile{8, 0.3, 0.9, pi / 8};
  if (type == "puffy") return Profile{9, 0.1, 0.8, 0};
  if (type == "boom") return Profile{15, 0.3, 2.0, 0};
  if (type == "softboom") return Profile{16, 0.2, 1.2, 0};
  return std::nullopt;
}

// Canvas path code for a shape in a size x size box, or empty.
std::string shape_path(const std::string& type, double size) {
  const double c = size / 2;
  std::string code = "ctx.beginPath();";
  if (const auto p = profile(type)) {
    for (int step = 0; step <= 360; ++step) {
      const double t = step * 2 * pi / 360;
      const double wave = 0.5 + 0.5 * std::cos(p->lobes * (t - p->rotation));
      const double r = c * (1 - p->depth + p->depth * std::pow(wave, p->sharpness));
      code += std::format("ctx.{}({},{});", step == 0 ? "moveTo" : "lineTo",
                          num(c + r * std::sin(t)), num(c - r * std::cos(t)));
    }
  } else if (type == "gem") {
    // MaterialShapes gem: a tall rounded hexagon.
    const double points[][2] = {{1.005, 0.792}, {0.5, 1.023}, {-0.005, 0.792},
                                {0.073, 0.258}, {0.5, 0.0},   {0.927, 0.258}};
    const double round = size * 0.18;
    const auto at = [&](int index) {
      const auto& point = points[(index % 6 + 6) % 6];
      return std::pair{point[0] * size, point[1] * size};
    };
    for (int index = 0; index < 6; ++index) {
      const auto [x0, y0] = at(index - 1);
      const auto [x1, y1] = at(index);
      const auto [x2, y2] = at(index + 1);
      const auto towards = [round](double ax, double ay, double bx, double by) {
        const double length = std::hypot(bx - ax, by - ay);
        const double t = std::min(0.5, round / std::max(length, 1e-6));
        return std::pair{ax + (bx - ax) * t, ay + (by - ay) * t};
      };
      const auto [sx, sy] = towards(x1, y1, x0, y0);
      const auto [ex, ey] = towards(x1, y1, x2, y2);
      code += std::format("ctx.{}({},{});ctx.quadraticCurveTo({},{},{},{});",
                          index == 0 ? "moveTo" : "lineTo", num(sx), num(sy), num(x1), num(y1),
                          num(ex), num(ey));
    }
  } else if (type == "cloud") {
    // Overlapping circles, like MaterialShapes' cloud.
    const double circles[][3] = {{0.32, 0.6, 0.22}, {0.52, 0.45, 0.27}, {0.74, 0.58, 0.2}, {0.5, 0.66, 0.2}};
    for (const auto& circle : circles)
      code += std::format("ctx.moveTo({},{});ctx.arc({},{},{},0,Math.PI*2);",
                          num((circle[0] + circle[2]) * size), num(circle[1] * size),
                          num(circle[0] * size), num(circle[1] * size), num(circle[2] * size));
  } else if (type == "pill") {
    // A rounded square rotated 45°, as the pill avatar mask reads.
    const double r = size * 0.36;
    const double inset = size * 0.04;
    code += std::format(
        "ctx.moveTo({0},{1});ctx.arcTo({2},{1},{2},{3},{4});ctx.arcTo({2},{5},{0},{5},{4});"
        "ctx.arcTo({6},{5},{6},{3},{4});ctx.arcTo({6},{1},{0},{1},{4});",
        num(c), num(inset), num(size - inset), num(c), num(r), num(size - inset), num(inset));
  } else {
    code += std::format("ctx.arc({0},{0},{0},0,Math.PI*2);", num(c));
  }
  return code + "ctx.closePath();";
}

}  // namespace

std::string css(vw::Color color) {
  return std::format("rgba({},{},{},{:.3f})", std::lround(std::clamp(color[0], 0.0F, 1.0F) * 255),
                     std::lround(std::clamp(color[1], 0.0F, 1.0F) * 255),
                     std::lround(std::clamp(color[2], 0.0F, 1.0F) * 255), color[3]);
}

void circular_progress(vw::Ui& ui, const std::string& id, float x, float y, float size,
                       const CircularOptions& o) {
  const double progress = unit(vw::animate("circular." + id, unit(o.value),
                                           {.duration = 0.42, .easing = vw::Easing::out_cubic}));
  const double threshold = unit(o.critical_threshold);
  const double normal = std::min(progress, threshold);
  const double critical = std::max(0.0, progress - threshold);
  const double radius = std::max(0.0, size / 2.0 - o.stroke);
  const double gap = radius > 0 ? (o.stroke + 3) / radius * 180 / pi : 0;
  const double sweep = std::clamp<double>(o.sweep_angle, 0, 360);
  const vw::Color endpoint = progress > threshold ? o.critical : o.accent;
  const bool spinning = o.indeterminate || (o.wavy && o.active);
  const double begin = o.start_angle + (o.indeterminate ? std::fmod(seconds(), 1.3) / 1.3 * 360 : 0);
  const auto r = static_cast<float>(radius);
  const bool partial = progress > 0 && progress < 1;
  if (progress < 0.999)
    arc(ui, x, y, size, r, o.stroke, begin + progress * sweep + (partial ? gap : 0),
        std::max(0.01, sweep * (1 - progress)) - (partial ? gap : 0),
        o.active ? o.track : with_alpha(o.track, o.track[3] * 0.55F));
  if (o.wavy && progress > 0) {
    // Eight waves round a full sweep, the phase a full turn every 1.5 s.
    arc(ui, x, y, size, r, o.stroke, begin, progress * sweep, endpoint, o.stroke * 0.38F,
        static_cast<float>(8 / (sweep * pi / 180)), o.active ? phase(1.5) : 0);
  } else {
    arc(ui, x, y, size, r, o.stroke, begin, std::max(0.01, std::min(progress, threshold) * sweep), o.accent);
    if (progress > threshold)
      arc(ui, x, y, size, r, o.stroke, begin + threshold * sweep, (progress - threshold) * sweep, o.critical);
  }
  if (o.show_end_indicator && progress < 0.995) {
    const double a = (begin + sweep) * pi / 180;
    const float dot = std::min(4.0F, o.stroke);
    draw::rect(ui, x + size / 2 + static_cast<float>(std::cos(a) * radius) - dot / 2,
               y + size / 2 + static_cast<float>(std::sin(a) * radius) - dot / 2, dot, dot, dot / 2, endpoint);
  }
  (void)normal;
  (void)critical;
  if (spinning) vw::request_animation_continuation();
  if (o.show_label) {
    if (!o.icon.empty())
      draw::glyph(ui, o.icon, x + size / 2, y + size / 2, 18, o.active ? endpoint : vw::Color{0.6F, 0.6F, 0.6F, 1});
    else
      draw::text_centered(ui, std::format("{}%", std::lround(progress * 100)), x + size / 2,
                          y + size / 2, {.size = 9, .color = endpoint, .weight = vw::weight::semibold,
                                         .family = font::display});
  }
}

void progress_bar(vw::Ui& ui, float x, float y, float width, float height, double value,
                  vw::Color accent, vw::Color track) {
  const double progress = unit(value);
  const float split = width * static_cast<float>(progress);
  constexpr float gap = 4;
  if (progress > 0)
    draw::rect(ui, x, y, std::max(height, split - gap / 2), height, height / 2, accent);
  if (progress < 1) {
    const float track_width = std::max(0.0F, width - split - gap / 2);
    draw::rect(ui, x + width - track_width, y, track_width, height, height / 2, track);
  }
  if (progress < 0.98) {
    const float dot = std::min(4.0F, height);
    draw::rect(ui, x + width - 1 - dot, y + (height - dot) / 2, dot, dot, dot / 2, accent);
  }
}

void usage_graph(vw::Ui& ui, const std::string& id, float x, float y, float width, float height,
                 const std::vector<double>& primary, const std::vector<double>& secondary,
                 vw::Color primary_colour, vw::Color secondary_colour, vw::Color grid) {
  std::string code = std::format(
      "ctx.clearRect(0,0,canvas.width,canvas.height);ctx.strokeStyle='{}';ctx.lineWidth=1;"
      "ctx.globalAlpha=0.72;const rows=4,cell=canvas.height/rows,cols=Math.ceil(canvas.width/cell);"
      "for(let r=0;r<=rows;r++){{ctx.beginPath();ctx.moveTo(0,r*cell);ctx.lineTo(canvas.width,r*cell);ctx.stroke();}}"
      "for(let c=1;c<cols;c++){{ctx.beginPath();ctx.moveTo(c*cell,0);ctx.lineTo(c*cell,canvas.height);ctx.stroke();}}"
      "ctx.globalAlpha=1;",
      css(grid));
  const auto series = [&](const std::vector<double>& samples, vw::Color colour) {
    if (samples.size() < 2) return;
    code += std::format("ctx.strokeStyle='{}';ctx.lineWidth=2;ctx.lineJoin='round';ctx.lineCap='round';ctx.beginPath();",
                        css(colour));
    for (std::size_t index = 0; index < samples.size(); ++index) {
      const double px = static_cast<double>(index) * width / static_cast<double>(samples.size() - 1);
      const double py = height - unit(samples[index]) * height;
      code += std::format("ctx.{}({},{});", index == 0 ? "moveTo" : "lineTo", num(px), num(py));
    }
    code += "ctx.stroke();";
  };
  series(primary, primary_colour);
  series(secondary, secondary_colour);
  canvas(ui, "graph." + id, x, y, width, height, code);
}

void shape(vw::Ui& ui, const std::string& id, const std::string& type, float x, float y,
           float size, vw::Color fill, float stroke_width, vw::Color stroke) {
  std::string code = "ctx.clearRect(0,0,canvas.width,canvas.height);";
  const double inset = stroke_width / 2;
  code += std::format("ctx.save();ctx.translate({0},{0});", num(inset));
  code += shape_path(type, size - stroke_width);
  code += std::format("ctx.fillStyle='{}';ctx.fill();", css(fill));
  if (stroke_width > 0)
    code += std::format("ctx.strokeStyle='{}';ctx.lineWidth={};ctx.stroke();", css(stroke), num(stroke_width));
  code += "ctx.restore();";
  canvas(ui, "shape." + id, x, y, size, size, code);
}

void material_slider(vw::Ui& ui, const std::string& id, float x, float y, float width,
                     float height, const SliderOptions& o) {
  // While dragging, where the pointer is; just after, that value until the
  // source reports something close to it (or 1.5 s pass).
  SliderDrag& drag = slider_drags()[id];
  if (!drag.dragging && drag.released != Clock::time_point{} &&
      (std::abs(unit(o.value) - drag.value) < 0.01 || Clock::now() - drag.released > std::chrono::milliseconds(1500)))
    drag.released = {};
  const bool held = drag.dragging || drag.released != Clock::time_point{};
  const double value = held ? drag.value : unit(o.value);
  const float inset = o.handle_diameter / 2 + o.endpoint_padding;
  const float track_x = x + inset;
  const float track_width = std::max(0.0F, width - inset * 2);
  const float centre_y = y + height / 2;
  const float fill = std::max(0.0F, track_width * static_cast<float>(value) -
                                        (value > 0 ? o.handle_progress_gap : 0));
  // Track after the handle.
  const float rest_x = track_x + track_width * static_cast<float>(value) + (value > 0 ? o.handle_progress_gap : 0);
  if (rest_x < track_x + track_width)
    draw::rect(ui, rest_x - o.track_thickness / 2, centre_y - o.track_thickness / 2,
               track_x + track_width - rest_x + o.track_thickness, o.track_thickness,
               o.track_thickness / 2, o.track);
  if (fill > 0) {
    if (o.wave) {
      // wave.comp: the fill length and phase are data, so neither playback
      // nor a drag rebuilds anything; the phase runs a full turn every 0.9 s.
      const float pad = o.wave_amplitude + o.track_thickness;
      wave_stroke(ui, track_x - o.track_thickness / 2, centre_y - pad, fill + o.track_thickness, pad * 2, o.fill,
                  {0, o.track_thickness, o.wave_amplitude, phase(0.9), o.wave_length, o.track_thickness / 2, fill, pad});
      vw::request_animation_continuation();
    } else {
      draw::rect(ui, track_x - o.track_thickness / 2, centre_y - o.track_thickness / 2,
                 fill + o.track_thickness, o.track_thickness, o.track_thickness / 2, o.fill);
    }
  }
  // Handle bar and end dot.
  const float handle_x = track_x + track_width * static_cast<float>(value);
  draw::rect(ui, handle_x - o.handle_width / 2, centre_y - o.handle_diameter / 2, o.handle_width,
             o.handle_diameter, std::min(o.handle_width / 2, o.handle_diameter / 2), o.fill);
  if (value < 0.99) {
    const float dot = std::min(4.0F, o.track_thickness);
    draw::rect(ui, track_x + track_width - dot / 2, centre_y - dot / 2, dot, dot, dot / 2, o.fill);
  }
  if (!o.interactive) return;
  const auto at = [track_x, track_width](double pointer) {
    return track_width <= 0 ? 0.0 : unit((pointer - track_x) / track_width);
  };
  const auto hold = [id](double to, bool dragging) {
    SliderDrag& state = slider_drags()[id];
    state.value = to;
    state.dragging = dragging;
    state.released = dragging ? Clock::time_point{} : Clock::now();
  };
  ui.hit_area({.width = width, .height = height, .place = {.x = x, .y = y},
               .input = {.on_click = [at, hold, moved = o.moved, released = o.released](const vw::ClickEvent& e) {
                           hold(at(e.x), false);
                           if (moved) moved(at(e.x));
                           if (released) released(at(e.x));
                         },
                         .on_drag_start = [at, hold, moved = o.moved](const vw::DragEvent& e) {
                           hold(at(e.x), true);
                           if (moved) moved(at(e.x));
                         },
                         .on_drag = [at, hold, moved = o.moved](const vw::DragEvent& e) {
                           hold(at(e.x), true);
                           if (moved) moved(at(e.x));
                         },
                         .on_drag_end = [at, hold, released = o.released](const vw::DragEvent& e) {
                           hold(at(e.x), false);
                           if (released) released(at(e.x));
                         }}});
}

void circle_handle_slider(vw::Ui& ui, const std::string& id, float x, float y, float height,
                          const HandleSliderOptions& o) {
  static std::map<std::string, std::pair<double, Clock::time_point>> last_values;
  const double value = unit(o.value);
  auto& [previous, changed] = last_values[id];
  if (std::abs(previous - value) > 0.0005) {
    if (changed != Clock::time_point{}) changed = Clock::now();
    else changed = Clock::time_point{} + std::chrono::nanoseconds(1);
    previous = value;
  }
  const bool show_value = Clock::now() - changed < std::chrono::milliseconds(900);
  if (show_value) vw::request_animation_continuation();

  const float thickness = o.handle_diameter;
  const float width = thickness + o.endpoint_padding * 2;
  const float inset = o.handle_diameter / 2 + o.endpoint_padding;
  const float track_x = x + (width - thickness) / 2;
  const float track_y = y + inset;
  const float track_height = std::max(0.0F, height - inset * 2);
  // Round caps reach thickness/2 past each end, like the QML stroke.
  draw::rect(ui, track_x, track_y - thickness / 2, thickness, track_height + thickness, thickness / 2, o.track);
  const float handle_y = track_y + track_height * (1 - static_cast<float>(value));
  draw::rect(ui, track_x, handle_y - thickness / 2, thickness,
             track_y + track_height - handle_y + thickness, thickness / 2, o.fill);
  draw::rect(ui, track_x, handle_y - thickness / 2, thickness, thickness, thickness / 2, o.handle);
  if (show_value)
    draw::text_centered(ui, o.value_text, track_x + thickness / 2, handle_y,
                        {.size = 9, .color = o.track, .weight = vw::weight::black, .family = font::display});
  else
    draw::text_centered(ui, o.icon, track_x + thickness / 2, handle_y,
                        {.size = 15, .color = o.track, .weight = vw::weight::black, .family = font::icon});
  const auto at = [track_y, track_height](double pointer) {
    return track_height <= 0 ? 0.0 : unit(1 - (pointer - track_y) / track_height);
  };
  ui.rectangle({.width = width, .height = height, .radius = width / 2, .color = with_alpha(o.fill, 0),
                .place = {.x = x, .y = y},
                .input = {.on_click = [at, moved = o.moved](const vw::ClickEvent& e) { if (moved) moved(at(e.y)); },
                          .hover = vw::Hover{.color = with_alpha(o.fill, 0.08F)},
                          .on_drag_start = [at, moved = o.moved](const vw::DragEvent& e) { if (moved) moved(at(e.y)); },
                          .on_drag = [at, moved = o.moved](const vw::DragEvent& e) { if (moved) moved(at(e.y)); }}});
}

void radial_bars(vw::Ui& ui, const std::string& id, float x, float y, float size, float radius,
                 const std::vector<float>& values, vw::Color colour, float alpha) {
  if (values.empty()) return;
  std::string code = std::format(
      "ctx.clearRect(0,0,canvas.width,canvas.height);ctx.fillStyle='{}';ctx.globalAlpha={};"
      "const C={};",
      css(colour), num(alpha), num(size / 2.0));
  for (std::size_t index = 0; index < values.size(); ++index) {
    const double magnitude = std::clamp<double>(values[index], 0.03, 1.0);
    const double length = 4 + magnitude * 20;
    const double angle = static_cast<double>(index) * 2 * pi / static_cast<double>(values.size());
    code += std::format(
        "ctx.save();ctx.translate(C,C);ctx.rotate({});ctx.beginPath();ctx.rect(-1.5,-{}-{},3,{});ctx.fill();ctx.restore();",
        num(angle), num(radius), num(length), num(length));
  }
  canvas(ui, "bars." + id, x, y, size, size, code);
}

void marquee(vw::Ui& ui, const std::string& id, const std::string& text, float x, float y,
             float width, float height, float size, vw::Color colour, int weight,
             const char* family) {
  const float text_width = draw::measure(text, size, weight, family);
  const vw::Clip clip{.x = x, .y = y, .width = width, .height = height};
  float offset = 0;
  if (text_width > width) {
    // Back and forth: hold, scroll to the end, hold, scroll back (MarqueeText).
    const double overflow = text_width - width;
    const double travel = overflow / 30.0;  // 30 px/s
    const double period = 2 * (travel + 1.2);
    const double t = std::fmod(std::chrono::duration<double>(Clock::now().time_since_epoch()).count(), period);
    double position = 0;
    if (t < 1.2) position = 0;
    else if (t < 1.2 + travel) position = (t - 1.2) / travel;
    else if (t < 2.4 + travel) position = 1;
    else position = 1 - (t - 2.4 - travel) / travel;
    offset = -static_cast<float>(overflow * position);
    vw::request_animation_continuation();
  }
  ui.text({.text = text, .font = family, .size = size, .weight = weight, .color = colour,
           .place = {.x = x + offset, .y = y + (height - size * 1.32F) / 2, .clip = clip}});
  (void)id;
}

}  // namespace blx::widgets
