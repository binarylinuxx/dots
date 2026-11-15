#version 450
#include "zwwm.glsl"

layout(set = 1, binding = 0, std140) uniform Config {
  int width;
  int radius;
  int rounding_power;
  vec4 gradient_colors[20];
  int gradient_colors_count;
  vec4 focused_gradient_colors[20];
  int focused_gradient_colors_count;
} config;

float power_length(vec2 value, float power) {
  return pow(pow(value.x, power) + pow(value.y, power), 1.0 / power);
}

float rounded_rect_distance(vec2 point, vec2 size, float radius, float power) {
  float r = clamp(radius, 0.0, 0.5 * min(size.x, size.y));
  vec2 q = abs(point - size * 0.5) - size * 0.5 + vec2(r);
  return power_length(max(q, vec2(0.0)), power) +
      min(max(q.x, q.y), 0.0) - r;
}

vec4 gradient_color(vec4 colors[20], int count, float position) {
  if (count <= 1) return colors[0];
  float offset = fract(position) * float(count);
  int first = min(int(floor(offset)), count - 1);
  return mix(colors[first], colors[(first + 1) % count], fract(offset));
}

float rounded_perimeter_position(vec2 point, vec2 size, float radius, float width) {
  const float pi = 3.14159265358979323846;
  float inset = width * 0.5;
  float outer_radius = clamp(radius, width, 0.5 * min(size.x, size.y));
  float center_radius = max(outer_radius - inset, 0.001);
  float horizontal = max(size.x - 2.0 * outer_radius, 0.0);
  float vertical = max(size.y - 2.0 * outer_radius, 0.0);
  float quarter = 0.5 * pi * center_radius;
  float perimeter = 2.0 * horizontal + 2.0 * vertical + 4.0 * quarter;

  float best_distance = 1e20;
  float best_position = 0.0;

  vec2 nearest = vec2(clamp(point.x, outer_radius, size.x - outer_radius), inset);
  float distance = length(point - nearest);
  if (distance < best_distance) {
    best_distance = distance;
    best_position = nearest.x - outer_radius;
  }

  vec2 center = vec2(size.x - outer_radius, outer_radius);
  float angle = clamp(atan(point.y - center.y, point.x - center.x), -0.5 * pi, 0.0);
  nearest = center + vec2(cos(angle), sin(angle)) * center_radius;
  distance = length(point - nearest);
  if (distance < best_distance) {
    best_distance = distance;
    best_position = horizontal + (angle + 0.5 * pi) * center_radius;
  }

  nearest = vec2(size.x - inset, clamp(point.y, outer_radius, size.y - outer_radius));
  distance = length(point - nearest);
  if (distance < best_distance) {
    best_distance = distance;
    best_position = horizontal + quarter + nearest.y - outer_radius;
  }

  center = vec2(size.x - outer_radius, size.y - outer_radius);
  angle = clamp(atan(point.y - center.y, point.x - center.x), 0.0, 0.5 * pi);
  nearest = center + vec2(cos(angle), sin(angle)) * center_radius;
  distance = length(point - nearest);
  if (distance < best_distance) {
    best_distance = distance;
    best_position = horizontal + quarter + vertical + angle * center_radius;
  }

  nearest = vec2(clamp(point.x, outer_radius, size.x - outer_radius), size.y - inset);
  distance = length(point - nearest);
  if (distance < best_distance) {
    best_distance = distance;
    best_position = horizontal + 2.0 * quarter + vertical + size.x - outer_radius - nearest.x;
  }

  center = vec2(outer_radius, size.y - outer_radius);
  angle = clamp(atan(point.y - center.y, point.x - center.x), 0.5 * pi, pi);
  nearest = center + vec2(cos(angle), sin(angle)) * center_radius;
  distance = length(point - nearest);
  if (distance < best_distance) {
    best_distance = distance;
    best_position = 2.0 * horizontal + 2.0 * quarter + vertical +
                    (angle - 0.5 * pi) * center_radius;
  }

  nearest = vec2(inset, clamp(point.y, outer_radius, size.y - outer_radius));
  distance = length(point - nearest);
  if (distance < best_distance) {
    best_distance = distance;
    best_position = 2.0 * horizontal + 3.0 * quarter + vertical +
                    size.y - outer_radius - nearest.y;
  }

  center = vec2(outer_radius);
  angle = atan(point.y - center.y, point.x - center.x);
  if (angle < 0.0) angle += 2.0 * pi;
  angle = clamp(angle, pi, 1.5 * pi);
  nearest = center + vec2(cos(angle), sin(angle)) * center_radius;
  distance = length(point - nearest);
  if (distance < best_distance) {
    best_position = 2.0 * horizontal + 2.0 * vertical + 3.0 * quarter +
                    (angle - pi) * center_radius;
  }
  return best_position / max(perimeter, 0.001);
}

void main() {
  vec2 size = zwwm.draw_rect.zw;
  float width = zwwm.border_width;
  float radius = zwwm.corner_radius;
  float rounding_power = clamp(float(config.rounding_power), 1.0, 16.0);
  // Keeps the ring even below power 2.
  float outer_radius = radius - width * (sqrt(2.0) - 1.0) * max(2.0 - rounding_power, 0.0);
  float outer = clamp(0.5 - rounded_rect_distance(zwwm_local, size, outer_radius, rounding_power), 0.0, 1.0);
  // The window's edge, so both antialiasing ramps are complementary.
  // Same corner as window.frag, so no gap shows.
  float inner = clamp(0.5 - zwwm_rounded_rect_distance(zwwm_position - zwwm.texture_rect.xy, zwwm.texture_rect.zw,
                                                       zwwm.texture_corner_radius), 0.0, 1.0);
  float gradient_position = rounded_perimeter_position(zwwm_local, size, radius, width);
  vec4 selected = (zwwm.state & ZWWM_STATE_FOCUSED) != 0u
      ? gradient_color(config.focused_gradient_colors, config.focused_gradient_colors_count, gradient_position)
      : gradient_color(config.gradient_colors, config.gradient_colors_count, gradient_position);
  vec4 color = vec4(selected.rgb * selected.a, selected.a);
  zwwm_out = color * zwwm.opacity * max(outer - inner, 0.0);
}
