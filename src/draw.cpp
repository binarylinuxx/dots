#include "draw.hpp"

#include "config.hpp"

#include <vw/text.hpp>

#include <algorithm>

namespace blx::draw {

float measure(std::string_view value, float size, int weight, const char* family) {
  if (value.empty()) return 0;
  return vw::caret_stops(value, family, size * props::font_scale, weight).back().x;
}

vw::Widget text(vw::Ui& ui, const std::string& value, float x, float y, const TextStyle& style) {
  return ui.text({.text = value, .font = style.family, .size = style.size * (style.scaled ? props::font_scale : 1), .weight = style.weight,
                  .color = style.color, .max_width = style.max_width,
                  .max_lines = style.max_lines, .align = style.align,
                  .place = {.x = x, .y = y}});
}

vw::Widget text_middle(vw::Ui& ui, const std::string& value, float x, float center_y,
                       const TextStyle& style) {
  vw::Widget widget = text(ui, value, 0, 0, style);
  ui.move(widget, x, center_y - widget.height / 2);
  return widget;
}

vw::Widget text_centered(vw::Ui& ui, const std::string& value, float center_x, float center_y,
                         const TextStyle& style) {
  vw::Widget widget = text(ui, value, 0, 0, style);
  ui.move(widget, center_x - widget.width / 2, center_y - widget.height / 2);
  return widget;
}

void glyph(vw::Ui& ui, const std::string& name, float center_x, float center_y, float size,
           vw::Color color) {
  text_centered(ui, name, center_x, center_y,
                {.size = size, .color = color, .family = font::icon, .scaled = false});
}

bool icon(vw::Ui& ui, const std::string& path, const std::string& theme_icon, float x, float y,
          float size, std::optional<vw::Color> tint, float radius, vw::ImageFit fit,
          const vw::ImageEffect& effect, const std::vector<float>& mask) {
  if (path.empty() && theme_icon.empty()) return false;
  try {
    ui.image({.path = path, .icon = path.empty() ? theme_icon : "", .width = size,
              .height = size, .fit = fit, .radius = radius,
              .tint = tint.value_or(vw::Color{1, 1, 1, 1}), .effect = effect, .mask = mask, .place = {.x = x, .y = y}});
    return true;
  } catch (const std::exception&) {
    // Missing or undecodable icons leave an empty slot.
    return false;
  }
}

vw::ImageEffect icon_tint() {
  return {.colorize = colours().primary, .colorize_amount = config().icon_tint};
}

bool art(vw::Ui& ui, const std::string& path, float x, float y, float size, float radius) {
  const Config& c = config();
  return icon(ui, path, "", x, y, size, std::nullopt, radius, vw::ImageFit::cover,
              {.hue_rotate = c.art_hue, .colorize = colours().primary, .colorize_amount = c.art_tint});
}

void state_layer(vw::Ui& ui, float x, float y, float width, float height, float radius,
                 vw::Color tint, vw::Input input) {
  if (!input.hover) input.hover = vw::Hover{.color = with_alpha(tint, 0.12F), .duration = 0.12F};
  ui.rectangle({.width = width, .height = height, .radius = radius,
                .color = with_alpha(tint, 0), .place = {.x = x, .y = y},
                .input = std::move(input)});
}

void rect(vw::Ui& ui, float x, float y, float width, float height, Corners radii, vw::Color color) {
  if (width < 0.5F || height < 0.5F) return;
  const float most = std::min(width, height) / 2;
  ui.rectangle({.width = width, .height = height, .color = color,
                .corner_radii = {std::min(radii[0], most), std::min(radii[1], most), std::min(radii[2], most),
                                 std::min(radii[3], most)},
                .place = {.x = x, .y = y}});
}

void state_layer(vw::Ui& ui, float x, float y, float width, float height, Corners radii,
                 vw::Color tint, vw::Input input) {
  if (!input.hover) input.hover = vw::Hover{.color = with_alpha(tint, 0.12F), .duration = 0.12F};
  ui.rectangle({.width = width, .height = height, .color = with_alpha(tint, 0),
                .corner_radii = {radii[0], radii[1], radii[2], radii[3]}, .place = {.x = x, .y = y},
                .input = std::move(input)});
}

void rect(vw::Ui& ui, float x, float y, float width, float height, float radius, vw::Color color) {
  if (width < 0.5F || height < 0.5F) return;
  ui.rectangle({.width = width, .height = height,
                .radius = std::min(radius, std::min(width, height) / 2), .color = color,
                .place = {.x = x, .y = y}});
}

vw::Widget capture(vw::Ui& ui, float width, float height, const std::function<void()>& paint) {
  // Invisible markers bracket the scene range paint() adds.
  const vw::Widget first = ui.hit_area({.width = 1, .height = 1});
  paint();
  const vw::Widget last = ui.hit_area({.width = 1, .height = 1});
  return {first.begin, last.end, width, height};
}

}  // namespace blx::draw
