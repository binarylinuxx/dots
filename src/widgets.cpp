#include "widgets.hpp"

#include <vw/animation.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
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
  // Quantised so an unchanged value reuses the cached canvas.
  const double shown = std::round(progress * 400) / 400;
  std::string code = std::format(
      "const P=Math.PI/180,R={},C={},S0={},W={};"
      "function arc(a,b,col,w){{if(b<=0.05)return;ctx.beginPath();ctx.strokeStyle=col;ctx.lineWidth=w;"
      "ctx.lineCap='round';ctx.arc(C,C,R,a*P,(a+b)*P,false);ctx.stroke();}}"
      "function draw(ctx,canvas,time){{ctx.clearRect(0,0,canvas.width,canvas.height);{}",
      num(radius), num(size / 2.0), num(o.start_angle), num(o.stroke),
      o.indeterminate ? "const S=S0+(time%1.3)/1.3*360;" : "const S=S0;");
  const bool partial = shown > 0 && shown < 1;
  if (shown < 0.999)
    code += std::format("arc(S+{}+{},{}-{},'{}',W);", num(shown * sweep), num(partial ? gap : 0),
                        num(std::max(0.01, sweep * (1 - shown))), num(partial ? gap : 0),
                        css(o.active ? o.track : with_alpha(o.track, o.track[3] * 0.55F)));
  if (o.wavy && shown > 0) {
    // A sine-modulated arc whose phase runs a full turn every 1.5 s.
    code += std::format(
        "ctx.beginPath();ctx.strokeStyle='{0}';ctx.lineWidth=W;ctx.lineCap='round';ctx.lineJoin='round';"
        "const p={1},sw={2},N=Math.max(8,Math.ceil(p*sw/4)),ph=(time%1.5)/1.5*Math.PI*2;"
        "for(let i=0;i<=N;i++){{const f=i/N,a=(S+sw*p*f)*P,r=R+Math.sin(f*p*8*Math.PI*2+ph)*W*0.38;"
        "const px=C+Math.cos(a)*r,py=C+Math.sin(a)*r;if(i===0)ctx.moveTo(px,py);else ctx.lineTo(px,py);}}"
        "ctx.stroke();",
        css(endpoint), num(shown), num(sweep));
  } else {
    code += std::format("arc(S,{},'{}',W);", num(std::max(0.01, std::min(shown, threshold) * sweep)),
                        css(o.accent));
    if (shown > threshold)
      code += std::format("arc(S+{},{},'{}',W);", num(threshold * sweep),
                          num((shown - threshold) * sweep), css(o.critical));
  }
  if (o.show_end_indicator && shown < 0.995)
    code += std::format(
        "{{const a=(S+{})*P,d=Math.min(4,W);ctx.beginPath();ctx.fillStyle='{}';"
        "ctx.arc(C+Math.cos(a)*R,C+Math.sin(a)*R,d/2,0,Math.PI*2);ctx.fill();}}",
        num(sweep), css(endpoint));
  code += "}";
  (void)normal;
  (void)critical;
  canvas(ui, "circular." + id, x, y, size, size, code, (o.wavy || o.indeterminate) && o.active);
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
  const double value = unit(o.value);
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
      const float pad = o.wave_amplitude + o.track_thickness;
      const std::string code = std::format(
          "function draw(ctx,canvas,time){{ctx.clearRect(0,0,canvas.width,canvas.height);"
          "ctx.strokeStyle='{}';ctx.lineWidth={};ctx.lineCap='round';ctx.lineJoin='round';ctx.beginPath();"
          "const cy={},L={},A={},E={},ph=(time%0.9)/0.9*Math.PI*2;ctx.moveTo({},cy);"
          "for(let x=2;x<E;x+=2)ctx.lineTo({}+x,cy+Math.sin(x/L*Math.PI*2+ph)*A);"
          "ctx.lineTo({}+E,cy);ctx.stroke();}}",
          css(o.fill), num(o.track_thickness), num(pad), num(o.wave_length), num(o.wave_amplitude),
          num(std::round(fill)), num(o.track_thickness / 2), num(o.track_thickness / 2),
          num(o.track_thickness / 2));
      canvas(ui, "slider." + id, track_x - o.track_thickness / 2, centre_y - pad,
             std::round(fill) + o.track_thickness, pad * 2, code, true);
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
  ui.hit_area({.width = width, .height = height, .place = {.x = x, .y = y},
               .input = {.on_click = [at, moved = o.moved, released = o.released](const vw::ClickEvent& e) {
                           if (moved) moved(at(e.x));
                           if (released) released(at(e.x));
                         },
                         .on_drag_start = [at, moved = o.moved](const vw::DragEvent& e) {
                           if (moved) moved(at(e.x));
                         },
                         .on_drag = [at, moved = o.moved](const vw::DragEvent& e) {
                           if (moved) moved(at(e.x));
                         },
                         .on_drag_end = [at, released = o.released](const vw::DragEvent& e) {
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
