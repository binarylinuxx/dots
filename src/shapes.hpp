#pragma once

#include <vw/ui.hpp>

#include <array>
#include <string>
#include <vector>

// Material 3 expressive shapes, ported from ~/shell's roundedpolygon plugin
// (itself after androidx.graphics.shapes): rounded polygons as cubic Bézier
// outlines, with feature-aligned morphing between them. The GPU draws them:
// the outline is flattened and handed to shaders/shape.comp, which computes a
// signed distance per pixel for an anti-aliased fill and a centred stroke.
namespace blx::shapes {

struct Point {
  double x = 0;
  double y = 0;
};

struct Rounding {
  double radius = 0;
  double smoothing = 0;
};

using Cubic = std::array<Point, 4>;
using Shape = std::vector<Cubic>;

Shape build(const std::vector<Point>& vertices, const std::vector<Rounding>& roundings = {});
Shape circle(int vertices = 8, double radius = 1, double cx = 0, double cy = 0);
Shape rectangle(double width, double height, const std::vector<Rounding>& roundings = {},
                double cx = 0, double cy = 0);
Shape star(int points, double outer_radius, double inner_radius,
           const std::vector<Rounding>& roundings = {}, double cx = 0, double cy = 0);
Shape regular_polygon(int vertices, double radius, double cx, double cy,
                      const std::vector<Rounding>& roundings = {});
Shape rotated(const Shape& shape, double degrees);
Shape scaled(const Shape& shape, double sx, double sy);
// Fitted into the unit square, centred.
Shape normalized(const Shape& shape);

struct Morph {
  Shape from;
  Shape to;
};
Morph prepare_morph(const Shape& from, const Shape& to, int samples);
Shape interpolate(const Morph& morph, double progress);

// A named MaterialShapes shape ("circle", "sunny", "cookie6sided", "gem",
// "pill", "cloud", "heart", ...), normalized; unknown names give a circle.
const Shape& material(const std::string& name);

// The named shape as a polygon for vw::ImageProps::mask.
const std::vector<float>& mask(const std::string& name);

// RoundedPolygon.qml: the named shape in a size x size square at (x, y).
// Changing `type` for the same id morphs to it over morph_duration seconds
// (InOutQuad), resampled to morph_samples cubics.
void draw(vw::Ui& ui, const std::string& id, const std::string& type, float x, float y, float size,
          vw::Color fill, float stroke_width = 0, vw::Color stroke = {0, 0, 0, 0},
          double morph_duration = 0.4, int morph_samples = 64);

}  // namespace blx::shapes
