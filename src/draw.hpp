#pragma once

#include <array>
#include <vw/ui.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "style.hpp"

// Absolute-position drawing helpers.
namespace blx::draw {

float measure(std::string_view text, float size, int weight = vw::weight::regular,
              const char* family = font::utility);

struct TextStyle {
  float size = 13;
  vw::Color color{1, 1, 1, 1};
  int weight = vw::weight::regular;
  const char* family = font::utility;
  float max_width = 0;
  int max_lines = 1;
  std::string align = "start";
  bool scaled = true;  // follows the font scale setting
};

// Text with its top-left at (x, y).
vw::Widget text(vw::Ui& ui, const std::string& value, float x, float y, const TextStyle& style);
// Text whose left edge is x, vertically centered on center_y.
vw::Widget text_middle(vw::Ui& ui, const std::string& value, float x, float center_y,
                       const TextStyle& style);
// Text centered on (center_x, center_y).
vw::Widget text_centered(vw::Ui& ui, const std::string& value, float center_x, float center_y,
                         const TextStyle& style);
// A Material Symbols glyph centered on (center_x, center_y).
void glyph(vw::Ui& ui, const std::string& name, float center_x, float center_y, float size,
           vw::Color color);
// An image file or theme icon in a size x size square at (x, y); nothing when
// it cannot be loaded.
// False when there was nothing to draw, for callers with a fallback.
bool icon(vw::Ui& ui, const std::string& path, const std::string& theme_icon, float x, float y,
          float size, std::optional<vw::Color> tint = std::nullopt, float radius = 0,
          vw::ImageFit fit = vw::ImageFit::contain, const vw::ImageEffect& effect = {},
          const std::vector<float>& mask = {});
// The theme tint for icons, at the strength set in the config.
vw::ImageEffect icon_tint();
// Album art filling a size x size square, cropped rather than letterboxed,
// with the config's art tint and hue. False when it cannot be drawn.
bool art(vw::Ui& ui, const std::string& path, float x, float y, float size, float radius);
// An invisible hit target that tints with `tint` on hover, like ~/shell's
// StateLayer. Draw it after what it covers.
void state_layer(vw::Ui& ui, float x, float y, float width, float height, float radius,
                 vw::Color tint, vw::Input input);
void rect(vw::Ui& ui, float x, float y, float width, float height, float radius, vw::Color color);
// Per-corner radii: top-left, top-right, bottom-right, bottom-left.
using Corners = std::array<float, 4>;
void rect(vw::Ui& ui, float x, float y, float width, float height, Corners radii, vw::Color color);
void state_layer(vw::Ui& ui, float x, float y, float width, float height, Corners radii,
                 vw::Color tint, vw::Input input);
// Everything `paint` draws as one widget of the given size, e.g. a listview
// row built from several helpers.
vw::Widget capture(vw::Ui& ui, float width, float height, const std::function<void()>& paint);

}  // namespace blx::draw
