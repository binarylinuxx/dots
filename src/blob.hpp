#pragma once

#include <vw/ui.hpp>

#include <array>
#include <cstdint>
#include <vector>

// The shell's single SDF surface, after ~/shell's BlobGroup: an inverted
// screen frame plus rounded member boxes, joined by circular smooth-min
// fillets wherever two come within the smoothing distance, evaluated as one
// signed distance. Members are re-added every build from the panels' current
// (animated) geometry, so the joins follow the motion.
namespace blx::blob {

struct Radii {
  float top_left = 0;
  float top_right = 0;
  float bottom_right = 0;
  float bottom_left = 0;

  static Radii all(float radius) { return {radius, radius, radius, radius}; }
};

struct Box {
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;
  Radii radii;

  bool empty() const { return width < 1 || height < 1; }
  bool contains(float px, float py) const {
    return px >= x && px < x + width && py >= y && py < y + height;
  }
};

class Group {
 public:
  Group(vw::Color color, float smoothing) : color_(color), smoothing_(smoothing) {}

  // The frame is everything outside `hole`.
  void frame(const Box& hole) { hole_ = hole; }
  void add(const Box& member) {
    if (!member.empty()) members_.push_back(member);
  }
  const std::vector<Box>& members() const { return members_; }

  // An outline `width` px wide outside the whole surface (0 for none).
  void outline(vw::Color color, float width) {
    outline_color_ = color;
    outline_width_ = width;
  }

  // A width x height image (r | g << 8 | b << 16 per float) shown through the
  // surface colour's alpha, instead of the surface being translucent. It must
  // outlive draw().
  void backdrop(const std::vector<float>* pixels, int width, int height) {
    backdrop_ = pixels;
    backdrop_width_ = width;
    backdrop_height_ = height;
  }

  // Off for a translucent surface, where they would show through.
  void shadows(bool enabled, float strength = 0.65F, float blur = 34) {
    shadows_ = enabled;
    shadow_strength_ = strength;
    shadow_blur_ = blur;
  }

  // Shadows, then the merged surface and its outline. Draw contents afterwards.
  void draw(vw::Ui& ui, float surface_width, float surface_height) const;

 private:
  vw::Color color_;
  float smoothing_;
  Box hole_;
  std::vector<Box> members_;
  vw::Color outline_color_{};
  float outline_width_ = 0;
  const std::vector<float>* backdrop_ = nullptr;
  int backdrop_width_ = 0;
  int backdrop_height_ = 0;
  bool shadows_ = true;
  float shadow_strength_ = 0.65F;
  float shadow_blur_ = 34;
};

// A progress ring inscribed in `area`: `sweep` (0..1) of it from `start`
// (0..1, clockwise from the top) in full colour, the rest at track_alpha.
void ring(vw::Ui& ui, const Box& area, float stroke, float start, float sweep, vw::Color color,
          float track_alpha = 0.18F);

}  // namespace blx::blob
