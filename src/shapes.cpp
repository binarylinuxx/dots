// Port of ~/shell/roundedpolygon (geometry.cpp, materialshapes.cpp and the
// morphing in roundedpolygon.cpp) without Qt.
#include "shapes.hpp"

#include <vw/animation.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <numbers>
#include <unordered_map>

namespace blx::shapes {
namespace {

constexpr double eps = 1e-4;
constexpr double morph_eps = 1e-6;
constexpr double pi = std::numbers::pi;

Point operator+(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
Point operator-(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
Point operator*(Point a, double s) { return {a.x * s, a.y * s}; }
Point operator/(Point a, double s) { return {a.x / s, a.y / s}; }

double dot(Point a, Point b) { return a.x * b.x + a.y * b.y; }
double length(Point p) { return std::hypot(p.x, p.y); }
Point direction(Point p) {
  const double l = length(p);
  return l > eps ? p / l : Point{};
}
Point lerp(Point a, Point b, double t) { return a + (b - a) * t; }
Cubic line_cubic(Point a, Point b) { return {a, lerp(a, b, 1.0 / 3), lerp(a, b, 2.0 / 3), b}; }
Cubic zero_cubic(Point p) { return {p, p, p, p}; }
Cubic reverse(const Cubic& c) { return {c[3], c[2], c[1], c[0]}; }

Cubic arc_cubic(Point center, Point p0, Point p1) {
  const Point d0 = p0 - center;
  const Point d1 = p1 - center;
  const double radius = length(d0);
  if (radius < eps) return line_cubic(p0, p1);
  double cosine = std::clamp(dot(d0, d1) / (radius * length(d1) + eps), -1.0, 1.0);
  if (cosine > 1 - eps) return line_cubic(p0, p1);
  const Point t0{-d0.y / radius, d0.x / radius};
  const Point t1{-d1.y / radius, d1.x / radius};
  const double sign = dot(t0, d1) >= 0 ? 1 : -1;
  const double k = radius * (4.0 / 3.0) *
                   (std::sqrt(2 * (1 - cosine)) - std::sqrt(1 - cosine * cosine)) / (1 - cosine) * sign;
  return {p0, p0 + t0 * k, p1 - t1 * k, p1};
}

std::vector<Cubic> round_corner(Point p0, Point p1, Point p2, Rounding rounding, double allowed_cut0,
                                double allowed_cut1) {
  const double radius = rounding.radius;
  const double smooth = rounding.smoothing;
  if (radius < eps || allowed_cut0 < eps || allowed_cut1 < eps) return {zero_cubic(p1)};
  const Point d1 = direction(p0 - p1);
  const Point d2 = direction(p2 - p1);
  const double cos_a = std::clamp(dot(d1, d2), -1.0, 1.0);
  const double sin_a = std::sqrt(std::max(0.0, 1 - cos_a * cos_a));
  if (sin_a < eps) return {zero_cubic(p1)};
  const double expected_round_cut = radius * (cos_a + 1) / sin_a;
  if (expected_round_cut < eps) return {zero_cubic(p1)};
  const double expected_cut = (1 + smooth) * expected_round_cut;
  const double allowed_cut = std::min(allowed_cut0, allowed_cut1);
  if (allowed_cut < eps) return {zero_cubic(p1)};
  const double actual_round_cut = std::min(allowed_cut, expected_round_cut);
  const double actual_radius = radius * actual_round_cut / expected_round_cut;
  const auto smoothing_for = [&](double cut) {
    if (cut >= expected_cut) return smooth;
    if (cut > expected_round_cut)
      return smooth * (cut - expected_round_cut) / (expected_cut - expected_round_cut);
    return 0.0;
  };
  const Point center = p1 + direction(d1 + d2) *
                                std::sqrt(actual_radius * actual_radius + actual_round_cut * actual_round_cut);
  const Point tangent0 = p1 + d1 * actual_round_cut;
  const Point tangent2 = p1 + d2 * actual_round_cut;
  const auto flank = [&](double smoothing, Point side_direction, Point side_start, Point tangent_a,
                         Point tangent_b) {
    const Point curve_start = p1 + side_direction * actual_round_cut * (1 + smoothing);
    const Point radial = direction(lerp(tangent_a, (tangent_a + tangent_b) * 0.5, smoothing) - center);
    const Point curve_end = center + radial * actual_radius;
    const Point circle_tangent{-radial.y, radial.x};
    const Point rotated_tangent{-circle_tangent.y, circle_tangent.x};
    const double denominator = dot(side_direction, rotated_tangent);
    Point anchor_end = tangent_a;
    if (std::abs(denominator) > eps)
      anchor_end = side_start + side_direction * (dot(curve_end - side_start, rotated_tangent) / denominator);
    const Point anchor_start = (curve_start + anchor_end * 2) / 3;
    return Cubic{curve_start, anchor_start, anchor_end, curve_end};
  };
  const Cubic f0 = flank(smoothing_for(allowed_cut0), d1, p0, tangent0, tangent2);
  const Cubic f2 = reverse(flank(smoothing_for(allowed_cut1), d2, p2, tangent2, tangent0));
  return {f0, arc_cubic(center, f0[3], f2[0]), f2};
}

Point cubic_point(const Cubic& c, double t) {
  const double u = 1 - t;
  return c[0] * (u * u * u) + c[1] * (3 * u * u * t) + c[2] * (3 * u * t * t) + c[3] * (t * t * t);
}

double cubic_length(const Cubic& c) {
  Point previous = c[0];
  double total = 0;
  for (int i = 1; i <= 8; ++i) {
    const Point point = cubic_point(c, i / 8.0);
    total += length(point - previous);
    previous = point;
  }
  return total;
}

std::pair<Cubic, Cubic> split_at(const Cubic& c, double t) {
  const Point m0 = lerp(c[0], c[1], t);
  const Point m1 = lerp(c[1], c[2], t);
  const Point m2 = lerp(c[2], c[3], t);
  const Point n0 = lerp(m0, m1, t);
  const Point n1 = lerp(m1, m2, t);
  const Point p = lerp(n0, n1, t);
  return {{c[0], m0, n0, p}, {p, n1, m2, c[3]}};
}

double parameter_at_distance(const Cubic& c, double distance, double total_length) {
  if (distance <= 0) return 0;
  if (distance >= total_length) return 1;
  Point previous = c[0];
  double walked = 0;
  for (int i = 1; i <= 8; ++i) {
    const Point point = cubic_point(c, i / 8.0);
    const double segment = length(point - previous);
    if (walked + segment >= distance - morph_eps) {
      const double fraction = segment > morph_eps ? (distance - walked) / segment : 0;
      return (i - 1) / 8.0 + fraction / 8;
    }
    walked += segment;
    previous = point;
  }
  return 1;
}

Shape resample(const Shape& shape, int count) {
  if (shape.empty() || count <= 0) return {};
  std::vector<double> lengths;
  std::vector<double> prefix{0};
  double total = 0;
  for (const Cubic& cubic : shape) {
    lengths.push_back(cubic_length(cubic));
    total += lengths.back();
    prefix.push_back(total);
  }
  if (total < morph_eps) return Shape(static_cast<std::size_t>(count), zero_cubic(shape.front()[0]));
  const auto location = [&](double distance) {
    if (distance <= 0) return std::pair{0, 0.0};
    if (distance >= total) return std::pair{static_cast<int>(shape.size()) - 1, 1.0};
    int index = 0;
    while (index < static_cast<int>(shape.size()) - 1 && prefix[index + 1] <= distance + morph_eps) ++index;
    return std::pair{index, parameter_at_distance(shape[index], distance - prefix[index], lengths[index])};
  };
  Shape result;
  result.reserve(static_cast<std::size_t>(count));
  const double step = total / count;
  for (int i = 0; i < count; ++i) {
    const auto [start_index, t0] = location(i * step);
    const auto [end_index, t1] = location((i + 1) * step);
    Cubic segment;
    if (start_index == end_index) {
      const Cubic right = t0 > morph_eps ? split_at(shape[start_index], t0).second : shape[start_index];
      const double relative = std::clamp(t0 < 1 - morph_eps ? (t1 - t0) / (1 - t0) : 1.0, 0.0, 1.0);
      segment = split_at(right, relative).first;
    } else {
      const Cubic tail = t0 > morph_eps ? split_at(shape[start_index], t0).second : shape[start_index];
      const Cubic head = t1 < 1 - morph_eps ? split_at(shape[end_index], t1).first : shape[end_index];
      segment = (1 - t0) * lengths[start_index] >= t1 * lengths[end_index] ? tail : head;
      segment[0] = cubic_point(shape[start_index], t0);
      segment[3] = cubic_point(shape[end_index], t1);
    }
    result.push_back(segment);
  }
  return result;
}

Point cubic_tangent(const Cubic& cubic, bool at_end) {
  Point tangent = at_end ? cubic[3] - cubic[2] : cubic[1] - cubic[0];
  if (length(tangent) < morph_eps) tangent = cubic[3] - cubic[0];
  return direction(tangent);
}

double cubic_turn(const Cubic& cubic) {
  const Point start = cubic_tangent(cubic, false);
  const Point end = cubic_tangent(cubic, true);
  return std::atan2(start.x * end.y - start.y * end.x, dot(start, end));
}

Shape feature_aligned(const Shape& from, const Shape& to) {
  if (from.size() != to.size() || from.empty()) return to;
  std::size_t best_offset = 0;
  double best_cost = std::numeric_limits<double>::infinity();
  for (std::size_t offset = 0; offset < to.size(); ++offset) {
    double cost = 0;
    for (std::size_t i = 0; i < from.size(); ++i) {
      const Cubic& a = from[i];
      const Cubic& b = to[(i + offset) % to.size()];
      const double turn_a = cubic_turn(a);
      const double turn_b = cubic_turn(b);
      const double feature_weight = 1 + 3 * (std::abs(turn_a) + std::abs(turn_b));
      const Point delta = a[0] - b[0];
      cost += feature_weight * dot(delta, delta);
      cost += 0.08 * (1 - dot(cubic_tangent(a, false), cubic_tangent(b, false)));
      cost += 0.12 * std::abs(std::abs(turn_a) - std::abs(turn_b));
      if (turn_a * turn_b < 0) cost += 0.35 * std::min(std::abs(turn_a), std::abs(turn_b));
    }
    if (cost < best_cost) {
      best_cost = cost;
      best_offset = offset;
    }
  }
  Shape aligned;
  aligned.reserve(to.size());
  for (std::size_t i = 0; i < to.size(); ++i) aligned.push_back(to[(i + best_offset) % to.size()]);
  return aligned;
}

Shape transformed(const Shape& shape, const std::function<Point(Point)>& transform) {
  Shape result = shape;
  for (Cubic& cubic : result)
    for (Point& point : cubic) point = transform(point);
  return result;
}

// --- MaterialShapes ---------------------------------------------------------------

struct Spec {
  Point point;
  Rounding rounding;
};

Rounding r(double radius = 0, double smoothing = 0) { return {radius, smoothing}; }
Spec p(double x, double y, Rounding rounding = {}) { return {{x, y}, rounding}; }
std::vector<Rounding> all(int count, double radius, double smoothing = 0) {
  return std::vector<Rounding>(static_cast<std::size_t>(count), r(radius, smoothing));
}

std::vector<Spec> repeat(const std::vector<Spec>& specs, int repetitions, double cx = 0.5, double cy = 0.5) {
  std::vector<Spec> result;
  const auto count = static_cast<int>(specs.size());
  for (int i = 0; i < count * repetitions; ++i) {
    const Spec& source = specs[static_cast<std::size_t>(i % count)];
    const double angle = std::floor(static_cast<double>(i) / count) * 2 * pi / repetitions;
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    const Point offset = source.point - Point{cx, cy};
    result.push_back(p(cx + offset.x * cosine - offset.y * sine, cy + offset.x * sine + offset.y * cosine,
                       source.rounding));
  }
  return result;
}

Shape custom_raw(const std::vector<Spec>& specs, int repetitions = 1) {
  std::vector<Point> points;
  std::vector<Rounding> roundings;
  for (const Spec& spec : repeat(specs, repetitions)) {
    points.push_back(spec.point);
    roundings.push_back(spec.rounding);
  }
  return build(points, roundings);
}

Shape custom(const std::vector<Spec>& specs, int repetitions = 1) {
  return normalized(custom_raw(specs, repetitions));
}

const std::map<std::string, std::function<Shape()>>& builders() {
  static const std::map<std::string, std::function<Shape()>> value{
      {"circle", [] { return normalized(rotated(circle(10, 1), 45)); }},
      {"square", [] { return normalized(rectangle(1, 1, all(4, 0.3))); }},
      {"slanted", [] { return custom({p(0.926, 0.970, r(0.189, 0.811)), p(-0.021, 0.967, r(0.187, 0.057))}, 2); }},
      {"arch", [] { return normalized(rectangle(1, 1, {r(0.2), r(0.2), r(1), r(1)})); }},
      {"fan", [] { return custom({p(1.004, 1, r(0.148, 0.417)), p(0, 1, r(0.151)), p(0, -0.003, r(0.148)), p(0.978, 0.020, r(0.803))}); }},
      {"arrow", [] { return custom({p(1.225, 1.060, r(0.211)), p(0.5, 0.892, r(0.313)), p(-0.216, 1.050, r(0.207)), p(0.499, -0.160, r(0.215, 1))}); }},
      {"semicircle", [] { return normalized(rectangle(1.6, 1, {r(0.2), r(0.2), r(1), r(1)})); }},
      {"oval", [] { return normalized(rotated(scaled(rotated(circle(8, 1), -90), 1, 0.64), 135)); }},
      {"pill", [] { return normalized(rotated(custom_raw({p(0.428, -0.001, r(0.426)), p(0.961, 0.039, r(0.426)), p(1.001, 0.428), p(1, 0.609, r(1))}, 2), 180)); }},
      {"triangle", [] { return normalized(rotated(regular_polygon(3, 1, 0.5, 0.5, all(3, 0.2)), 30)); }},
      {"diamond", [] { return custom({p(0.5, 1.096, r(0.151, 0.524)), p(0.040, 0.5, r(0.159))}, 2); }},
      {"clamshell", [] { return custom({p(0.829, 0.841, r(0.159)), p(0.171, 0.841, r(0.159)), p(-0.020, 0.5, r(0.140))}, 2); }},
      {"pentagon", [] { return custom({p(0.828, 0.970, r(0.169)), p(0.172, 0.970, r(0.169)), p(-0.030, 0.365, r(0.164)), p(0.5, -0.009, r(0.172)), p(1.030, 0.365, r(0.164))}); }},
      {"gem", [] { return custom({p(1.005, 0.792, r(0.208)), p(0.5, 1.023, r(0.241, 0.778)), p(-0.005, 0.792, r(0.208)), p(0.073, 0.258, r(0.228)), p(0.5, 0, r(0.241, 0.778)), p(0.927, 0.258, r(0.228))}); }},
      {"sunny", [] { return normalized(rotated(star(8, 1, 0.8, all(16, 0.15)), 45)); }},
      {"verysunny", [] { return normalized(rotated(custom_raw({p(0.5, 1.080, r(0.085)), p(0.358, 0.843, r(0.085))}, 8), -45)); }},
      {"cookie4sided", [] { return custom({p(1.237, 1.236, r(0.258)), p(0.5, 0.918, r(0.233))}, 4); }},
      {"cookie6sided", [] { return custom({p(0.723, 0.884, r(0.394)), p(0.5, 1.099, r(0.398))}, 6); }},
      {"cookie7sided", [] {
         Shape shape = normalized(star(7, 1, 0.75, all(14, 0.5)));
         for (int i = 0; i < 5; ++i) shape = rotated(shape, 360.0 / 28);
         return normalized(shape);
       }},
      {"cookie9sided", [] { return normalized(rotated(star(9, 1, 0.8, all(18, 0.5)), 30)); }},
      {"cookie12sided", [] { return normalized(rotated(star(12, 1, 0.8, all(24, 0.5)), 30)); }},
      {"ghostish", [] { return custom({p(1, 1.140, r(0.254, 0.106)), p(0.575, 0.906, r(0.253)), p(0.425, 0.906, r(0.253)), p(0, 1.140, r(0.254, 0.106)), p(0, 0, r(1)), p(0.5, 0, r(1)), p(1, 0, r(1))}); }},
      {"clover4leaf", [] { return custom({p(1.099, 0.725, r(0.476)), p(0.725, 1.099, r(0.476)), p(0.5, 0.926)}, 4); }},
      {"clover8leaf", [] { return custom({p(0.758, 1.101, r(0.209)), p(0.5, 0.964)}, 8); }},
      {"burst", [] { return normalized(rotated(rotated(custom_raw({p(0.592, 0.842, r(0.006)), p(0.5, 1.006, r(0.006))}, 12), -30), -30)); }},
      {"softburst", [] { return normalized(rotated(custom_raw({p(0.193, 0.277, r(0.053)), p(0.176, 0.055, r(0.053))}, 10), 180)); }},
      {"boom", [] { return normalized(rotated(custom_raw({p(0.457, 0.296, r(0.007)), p(0.5, -0.051, r(0.007))}, 15), 120)); }},
      {"softboom", [] {
         const Shape shape = custom_raw({p(0.733, 0.454), p(0.839, 0.437, r(0.532)), p(0.949, 0.449, r(0.439, 1)), p(0.998, 0.478, r(0.174)), p(0.998, 0.522, r(0.174)), p(0.949, 0.551, r(0.439, 1)), p(0.839, 0.563, r(0.532)), p(0.733, 0.546)}, 16);
         return normalized(rotated(rotated(shape, 45), -360.0 / 16));
       }},
      {"flower", [] { return normalized(rotated(custom_raw({p(0.370, 0.187), p(0.416, 0.049, r(0.381)), p(0.479, 0.001, r(0.095)), p(0.521, 0.001, r(0.095)), p(0.584, 0.049, r(0.381)), p(0.630, 0.187)}, 8), 135)); }},
      {"puffy", [] {
         const Shape shape = custom_raw({
             p(1.003, 0.563, r(0.255)), p(0.940, 0.656, r(0.126)), p(0.881, 0.654), p(0.926, 0.711, r(0.660)), p(0.914, 0.851, r(0.660)), p(0.777, 0.998, r(0.360)), p(0.722, 0.872), p(0.717, 0.934, r(0.574)), p(0.670, 1.035, r(0.426)), p(0.545, 1.040, r(0.405)), p(0.5, 0.947), p(0.5, 0.947),
             p(0.455, 1.040, r(0.405)), p(0.330, 1.035, r(0.426)), p(0.283, 0.934, r(0.574)), p(0.278, 0.872), p(0.223, 0.998, r(0.360)), p(0.086, 0.851, r(0.660)), p(0.074, 0.711, r(0.660)), p(0.119, 0.654), p(0.060, 0.656, r(0.126)), p(-0.003, 0.563, r(0.255))}, 2);
         return normalized(scaled(shape, 1, 0.742));
       }},
      {"puffydiamond", [] { return normalized(rotated(custom_raw({p(0.870, 0.130, r(0.146)), p(0.818, 0.357), p(1, 0.332, r(0.853)), p(1, 0.668, r(0.853)), p(0.818, 0.643)}, 4), 90)); }},
      {"cloud", [] { return custom({p(0.3636, 0.2727, r(1)), p(0.6364, 0.2727, r(1)), p(0.6364, 0.4545, r(1)), p(0.8182, 0.4545, r(1)), p(0.8182, 0.5455, r(1)), p(0.9091, 0.5455, r(1)), p(0.9091, 0.7273, r(1)), p(0, 0.7273, r(1)), p(0, 0.4545, r(1)), p(0.0909, 0.4545, r(1)), p(0.0909, 0.3636, r(1)), p(0.1818, 0.3636, r(1)), p(0.1818, 0.4545, r(1)), p(0.3636, 0.4545, r(1))}); }},
      {"pixelcircle", [] { return custom({p(1, 0.704), p(0.926, 0.704), p(0.926, 0.852), p(0.843, 0.852), p(0.843, 0.935), p(0.704, 0.935), p(0.704, 1), p(0.5, 1), p(0.296, 1), p(0.296, 0.935), p(0.157, 0.935), p(0.157, 0.852), p(0.074, 0.852), p(0.074, 0.704), p(0, 0.704)}, 2); }},
      {"pixeltriangle", [] { return custom({p(0.888, 0.561), p(0.789, 0.561), p(0.789, 0.656), p(0.675, 0.656), p(0.674, 0.735), p(0.560, 0.735), p(0.560, 0.830), p(0.421, 0.830), p(0.421, 0.913), p(0.287, 0.913), p(0.287, 1), p(0.113, 1), p(0.110, 0.5), p(0.113, 0), p(0.287, 0), p(0.287, 0.087), p(0.421, 0.087), p(0.421, 0.170), p(0.560, 0.170), p(0.560, 0.265), p(0.674, 0.265), p(0.675, 0.344), p(0.789, 0.344), p(0.789, 0.439), p(0.888, 0.439)}); }},
      {"bun", [] { return custom({p(0.796, 0.5), p(0.853, 0.518, r(1)), p(0.992, 0.631, r(1)), p(0.968, 1, r(1)), p(0.032, 1, r(1)), p(0.008, 0.631, r(1)), p(0.147, 0.518, r(1)), p(0.204, 0.5)}, 2); }},
      {"heart", [] { return custom({p(0.782, 0.611), p(0.499, 0.946, r(0)), p(0.2175, 0.611), p(-0.064, 0.276, r(1)), p(0.208, -0.066, r(0.958)), p(0.5, 0.268, r(0.016)), p(0.792, -0.066, r(0.958)), p(1.064, 0.276, r(1))}); }},
  };
  return value;
}

// --- GPU drawing ----------------------------------------------------------------

const std::string& shader() {
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
      if (fs::exists(directory / "shape.vws", error)) return (directory / "shape.vws").string();
    return std::string();
  }();
  return path;
}

// The outline as a closed polyline in unit coordinates, 6 segments per
// cubic (degenerate cubics are skipped).
std::vector<float> flatten(const Shape& shape) {
  std::vector<float> points;
  points.reserve(shape.size() * 12);
  for (const Cubic& cubic : shape) {
    if (length(cubic[3] - cubic[0]) < 1e-7 && length(cubic[1] - cubic[0]) < 1e-7) continue;
    for (int step = 0; step < 6; ++step) {
      const Point point = cubic_point(cubic, step / 6.0);
      points.push_back(static_cast<float>(point.x));
      points.push_back(static_cast<float>(point.y));
    }
  }
  return points;
}

struct Instance {
  std::string type;
  Shape shown;           // what is on screen when the morph started
  Morph morph;
  std::chrono::steady_clock::time_point started{};
  double duration = 0;
  bool morphing = false;
};

}  // namespace

Shape build(const std::vector<Point>& vertices, const std::vector<Rounding>& supplied) {
  const auto count = static_cast<int>(vertices.size());
  if (count < 3) return {};
  std::vector<Rounding> roundings(static_cast<std::size_t>(count));
  for (int i = 0; i < count && i < static_cast<int>(supplied.size()); ++i) roundings[i] = supplied[i];
  const auto at = [&](int i) { return vertices[static_cast<std::size_t>((i % count + count) % count)]; };
  const auto expected_round_cut = [&](int i) {
    const Rounding rounding = roundings[static_cast<std::size_t>(i)];
    if (rounding.radius < eps) return 0.0;
    const Point d1 = direction(at(i - 1) - at(i));
    const Point d2 = direction(at(i + 1) - at(i));
    const double cosine = std::clamp(dot(d1, d2), -1.0, 1.0);
    const double sine = std::sqrt(std::max(0.0, 1 - cosine * cosine));
    return sine < eps ? 0.0 : rounding.radius * (cosine + 1) / sine;
  };
  std::vector<Point> side_ratios;
  for (int i = 0; i < count; ++i) {
    const int next = (i + 1) % count;
    const double side = length(at(next) - at(i));
    const double rc0 = expected_round_cut(i);
    const double rc1 = expected_round_cut(next);
    const double c0 = (1 + roundings[i].smoothing) * rc0;
    const double c1 = (1 + roundings[next].smoothing) * rc1;
    if (rc0 + rc1 > side + eps) side_ratios.push_back({side / (rc0 + rc1), 0});
    else if (c0 + c1 > side + eps) side_ratios.push_back({1, (side - rc0 - rc1) / (c0 + c1 - rc0 - rc1 + eps)});
    else side_ratios.push_back({1, 1});
  }
  const auto allowed_cut = [&](int corner, int side) {
    const double rc = expected_round_cut(corner);
    const double c = (1 + roundings[corner].smoothing) * rc;
    return rc * side_ratios[side].x + (c - rc) * side_ratios[side].y;
  };
  std::vector<std::vector<Cubic>> corners;
  for (int i = 0; i < count; ++i)
    corners.push_back(round_corner(at(i - 1), at(i), at(i + 1), roundings[i],
                                   allowed_cut(i, (i + count - 1) % count), allowed_cut(i, i)));
  Shape result;
  for (int i = 0; i < count; ++i) {
    for (const Cubic& cubic : corners[i])
      if (std::abs(cubic[0].x - cubic[3].x) >= eps || std::abs(cubic[0].y - cubic[3].y) >= eps)
        result.push_back(cubic);
    const Point from = corners[i].back()[3];
    const Point to = corners[(i + 1) % count].front()[0];
    if (length(to - from) > eps) result.push_back(line_cubic(from, to));
  }
  return result;
}

Shape circle(int count, double radius, double cx, double cy) {
  const double theta = pi / count;
  const double polygon_radius = radius / std::cos(theta);
  std::vector<Point> points;
  std::vector<Rounding> roundings;
  for (int i = 0; i < count; ++i) {
    const double angle = 2 * pi * i / count;
    points.push_back({cx + std::cos(angle) * polygon_radius, cy + std::sin(angle) * polygon_radius});
    roundings.push_back({radius, 0});
  }
  return build(points, roundings);
}

Shape rectangle(double width, double height, const std::vector<Rounding>& roundings, double cx, double cy) {
  const double hw = width / 2;
  const double hh = height / 2;
  return build({{cx + hw, cy + hh}, {cx - hw, cy + hh}, {cx - hw, cy - hh}, {cx + hw, cy - hh}}, roundings);
}

Shape star(int count, double outer, double inner, const std::vector<Rounding>& roundings, double cx, double cy) {
  std::vector<Point> points;
  for (int i = 0; i < count; ++i) {
    const double outer_angle = 2 * pi * i / count;
    const double inner_angle = pi * (2 * i + 1) / count;
    points.push_back({cx + std::cos(outer_angle) * outer, cy + std::sin(outer_angle) * outer});
    points.push_back({cx + std::cos(inner_angle) * inner, cy + std::sin(inner_angle) * inner});
  }
  return build(points, roundings);
}

Shape regular_polygon(int count, double radius, double cx, double cy, const std::vector<Rounding>& roundings) {
  std::vector<Point> points;
  for (int i = 0; i < count; ++i) {
    const double angle = 2 * pi * i / count;
    points.push_back({cx + std::cos(angle) * radius, cy + std::sin(angle) * radius});
  }
  return build(points, roundings);
}

Shape rotated(const Shape& shape, double degrees) {
  const double angle = degrees * pi / 180;
  const double cosine = std::cos(angle);
  const double sine = std::sin(angle);
  return transformed(shape, [=](Point p) { return Point{p.x * cosine - p.y * sine, p.x * sine + p.y * cosine}; });
}

Shape scaled(const Shape& shape, double sx, double sy) {
  return transformed(shape, [=](Point p) { return Point{p.x * sx, p.y * sy}; });
}

Shape normalized(const Shape& shape) {
  if (shape.empty()) return shape;
  double min_x = std::numeric_limits<double>::infinity();
  double min_y = min_x;
  double max_x = -min_x;
  double max_y = -min_x;
  for (const Cubic& cubic : shape)
    for (const Point point : cubic) {
      min_x = std::min(min_x, point.x);
      min_y = std::min(min_y, point.y);
      max_x = std::max(max_x, point.x);
      max_y = std::max(max_y, point.y);
    }
  const double width = max_x - min_x;
  const double height = max_y - min_y;
  const double side = std::max(width, height);
  if (side < eps) return shape;
  const double offset_x = (side - width) / 2 - min_x;
  const double offset_y = (side - height) / 2 - min_y;
  return transformed(shape, [=](Point p) { return Point{(p.x + offset_x) / side, (p.y + offset_y) / side}; });
}

Morph prepare_morph(const Shape& from, const Shape& to, int samples) {
  const int count = std::max({static_cast<int>(from.size()), static_cast<int>(to.size()), std::max(1, samples)});
  const Shape resampled_from = resample(from, count);
  return {resampled_from, feature_aligned(resampled_from, resample(to, count))};
}

Shape interpolate(const Morph& morph, double progress) {
  Shape result;
  const std::size_t count = std::min(morph.from.size(), morph.to.size());
  result.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    Cubic cubic;
    for (std::size_t point = 0; point < 4; ++point)
      cubic[point] = lerp(morph.from[i][point], morph.to[i][point], progress);
    result.push_back(cubic);
  }
  return result;
}

const Shape& material(const std::string& name) {
  std::string canonical;
  for (char c : name)
    if (c != ' ' && c != '_' && c != '-') canonical += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  static std::unordered_map<std::string, Shape> cache;
  if (const auto found = cache.find(canonical); found != cache.end()) return found->second;
  const auto builder = builders().find(canonical);
  Shape shape = builder == builders().end() ? builders().at("circle")() : builder->second();
  return cache.emplace(canonical, std::move(shape)).first->second;
}

const std::vector<float>& mask(const std::string& name) {
  static std::map<std::string, std::vector<float>> cache;
  const auto found = cache.find(name);
  if (found != cache.end()) return found->second;
  return cache.emplace(name, flatten(material(name))).first->second;
}

void draw(vw::Ui& ui, const std::string& id, const std::string& type, float x, float y, float size,
          vw::Color fill, float stroke_width, vw::Color stroke, double morph_duration, int morph_samples) {
  if (shader().empty() || size < 1) return;
  static std::unordered_map<std::string, Instance> instances;
  using Clock = std::chrono::steady_clock;
  Instance& instance = instances[id];
  const auto now = Clock::now();
  // The shape on screen right now, mid-morph or settled.
  const auto current = [&]() -> Shape {
    if (!instance.morphing) return instance.shown;
    const double elapsed = std::chrono::duration<double>(now - instance.started).count();
    const double t = std::clamp(elapsed / std::max(instance.duration, 1e-6), 0.0, 1.0);
    // InOutQuad, as the plugin's QVariantAnimation.
    const double eased = t < 0.5 ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2) / 2;
    return interpolate(instance.morph, eased);
  };
  if (instance.type.empty()) {
    instance.type = type;
    instance.shown = material(type);
  } else if (instance.type != type) {
    const Shape from = current();
    instance.type = type;
    instance.morph = prepare_morph(from, material(type), morph_samples);
    instance.started = now;
    instance.duration = morph_duration;
    instance.morphing = morph_duration > 0;
    if (!instance.morphing) instance.shown = material(type);
  }
  Shape shape = current();
  if (instance.morphing) {
    if (now - instance.started >= std::chrono::duration<double>(instance.duration)) {
      instance.morphing = false;
      instance.shown = material(type);
      shape = instance.shown;
    } else {
      vw::request_animation_continuation();
    }
  }
  const float pad = stroke_width / 2 + 1;
  ui.rectangle({.width = size + pad * 2, .height = size + pad * 2, .color = fill,
                .color2 = stroke, .border_width = stroke_width, .shader = shader(),
                .shader_params = {x, y, size, 0}, .shader_data = flatten(shape),
                .place = {.x = x - pad, .y = y - pad}});
}

}  // namespace blx::shapes
