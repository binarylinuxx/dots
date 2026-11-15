#include "blob.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace blx::blob {
namespace {

namespace fs = std::filesystem;

// Shader modes; see shaders/blob.comp.
enum Mode { shadow = 3, frame_shadow = 4, arc = 5, merged = 6 };


// BLXSHELL_SHADER_DIR, the build tree, then <prefix>/share/blxshell.
const std::string& shader() {
  static const std::string path = [] {
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
      if (fs::exists(directory / "blob.vws", error)) return (directory / "blob.vws").string();
    return std::string();
  }();
  return path;
}

struct Rect {
  float x0, y0, x1, y1;
};

Rect expanded(const Box& box, float amount) {
  return {box.x - amount, box.y - amount, box.x + box.width + amount, box.y + box.height + amount};
}

vw::Color radii(const Radii& value) {
  const auto scale = [](float radius) { return std::clamp(radius / 512.0F, 0.0F, 1.0F); };
  return {scale(value.top_left), scale(value.top_right), scale(value.bottom_right),
          scale(value.bottom_left)};
}

// One shader pass over `area`, clipped to the surface. The mode travels in
// the rectangle radius, which vw clamps to half the smaller side, so passes
// are at least 12 px in each direction.
void pass(vw::Ui& ui, Mode mode, float amount, Rect area, const Box& a, vw::Color color,
          std::array<float, 4> extra, float width, float height,
          std::vector<float> data = {}) {
  if (shader().empty() || a.empty()) return;
  area.x0 = std::max(0.0F, std::floor(area.x0));
  area.y0 = std::max(0.0F, std::floor(area.y0));
  area.x1 = std::min(width, std::ceil(area.x1));
  area.y1 = std::min(height, std::ceil(area.y1));
  if (area.x1 - area.x0 < 1 || area.y1 - area.y0 < 1) return;
  if (area.x1 - area.x0 < 12) area.x0 = std::max(0.0F, area.x1 - 12);
  if (area.y1 - area.y0 < 12) area.y0 = std::max(0.0F, area.y1 - 12);
  ui.rectangle({.width = area.x1 - area.x0, .height = area.y1 - area.y0,
                .radius = static_cast<float>(mode), .color = color,
                .color2 = radii(a.radii), .border_width = amount,
                .shader = shader(),
                .shader_params = extra,
                .shader_data = std::move(data),
                .place = {.x = area.x0, .y = area.y0,
                          .clip = vw::Clip{.x = a.x, .y = a.y, .width = a.width,
                                           .height = a.height}}});
}

void append(std::vector<float>& data, const Box& box) {
  const Radii& r = box.radii;
  data.insert(data.end(), {box.x, box.y, box.width, box.height, r.top_left, r.top_right, r.bottom_right, r.bottom_left});
}

}  // namespace

void Group::draw(vw::Ui& ui, float width, float height) const {
  const Rect surface{0, 0, width, height};
  // Shadows first, so every body covers its own and its neighbours'.
  if (shadows_) {
    const vw::Color shadow_color{0, 0, 0, shadow_strength_};
    pass(ui, frame_shadow, shadow_blur_, expanded(hole_, 0), hole_, shadow_color, {}, width, height);
    for (const Box& member : members_)
      pass(ui, shadow, shadow_blur_, expanded(member, shadow_blur_ + 6), member, shadow_color, {}, width, height);
  }

  // The whole surface in one pass, as blob.frag: separate antialiased layers
  // would show seams where they join.
  const bool has_backdrop = backdrop_ != nullptr && !backdrop_->empty();
  std::vector<float> data{outline_color_[0], outline_color_[1], outline_color_[2], outline_color_[3], outline_width_,
                          static_cast<float>(members_.size() + 1),
                          has_backdrop ? static_cast<float>(backdrop_width_) : 0.0F,
                          has_backdrop ? static_cast<float>(backdrop_height_) : 0.0F};
  append(data, hole_);
  for (const Box& member : members_) append(data, member);
  if (has_backdrop) data.insert(data.end(), backdrop_->begin(), backdrop_->end());
  pass(ui, merged, smoothing_, surface, hole_, color_, {}, width, height, std::move(data));
}

void ring(vw::Ui& ui, const Box& area, float stroke, float start, float sweep, vw::Color color,
          float track_alpha) {
  pass(ui, arc, stroke, expanded(area, 1), area, color,
       {start, std::clamp(sweep, 0.0F, 1.0F), track_alpha, 0}, 1e6F, 1e6F);
}

}  // namespace blx::blob
