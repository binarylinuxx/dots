#version 450
#include "zwwm.glsl"

// zwwm.border_width carries `size` scaled for animation and zoom.
layout(set = 1, binding = 0, std140) uniform Config {
  int size;
  int offset_x;
  int offset_y;
  int rounding_power;
  int strength;  // percent
  vec4 color;
  vec4 focused_color;
} config;

float power_length(vec2 value, float power) {
  return pow(pow(value.x, power) + pow(value.y, power), 1.0 / power);
}

// Same squircle as border.frag.
float rounded_rect_distance(vec2 point, vec2 size, float radius, float power) {
  float r = clamp(radius, 0.0, 0.5 * min(size.x, size.y));
  vec2 q = abs(point - size * 0.5) - size * 0.5 + vec2(r);
  return power_length(max(q, vec2(0.0)), power) + min(max(q.x, q.y), 0.0) - r;
}

// Abramowitz-Stegun approximation of erf, good to ~5e-4.
float erf_approx(float x) {
  float a = abs(x);
  float t = 1.0 + (0.278393 + (0.230389 + 0.078108 * a * a) * a) * a;
  t *= t;
  return sign(x) * (1.0 - 1.0 / (t * t));
}

void main() {
  float spread = max(zwwm.border_width, 1.0);
  float scale = spread / max(float(config.size), 1.0);
  float power = clamp(float(config.rounding_power), 1.0, 16.0);
  vec2 offset = vec2(config.offset_x, config.offset_y) * scale;
  vec2 window = zwwm_position - zwwm.toplevel_rect.xy;
  vec2 window_size = zwwm.toplevel_rect.zw;

  float sigma = spread / 3.0;
  float distance = rounded_rect_distance(window - offset, window_size, zwwm.corner_radius, power);
  float shadow = 0.5 - 0.5 * erf_approx(distance / (sigma * 1.41421356));

  // Cut only where the window fully covers the pixel, so edges never seam.
  float outside = rounded_rect_distance(window, window_size, zwwm.corner_radius, power) > -0.5 ? 1.0 : 0.0;
  vec4 tint = (zwwm.state & ZWWM_STATE_FOCUSED) != 0u ? config.focused_color : config.color;
  // Stacks the shadow; 0 keeps a single one.
  float layers = config.strength > 0 ? float(config.strength) / 100.0 : 1.0;
  float alpha = 1.0 - pow(1.0 - clamp(shadow * tint.a, 0.0, 1.0), layers);
  zwwm_out = vec4(tint.rgb, 1.0) * alpha * outside * zwwm.opacity;
}
