#version 450
#include "zwwm.glsl"

layout(set = 1, binding = 0, std140) uniform Config {
  int radius;
  int rounding_power;
  int glass_width;
  int glass_height;
  int glass_radius;
  int distortion_depth_per_mille;
  int distortion_strength_per_mille;
  int chromatic_shift;
  int glass_tint_per_mille;
  int client_opacity;
} config;

float power_length(vec2 value, float power) {
  return pow(pow(value.x, power) + pow(value.y, power), 1.0 / power);
}

float sdf(vec2 point, vec2 bounds, float radius, float power) {
  vec2 distance = abs(point) - bounds + vec2(radius);
  return min(max(distance.x, distance.y), 0.0) +
      power_length(max(distance, vec2(0.0)), power) - radius;
}

vec2 safe_normalize(vec2 value) {
  float scale = max(max(abs(value.x), abs(value.y)), 0.0001);
  vec2 scaled = value / scale;
  return scaled / max(length(scaled), 0.0001);
}

float rounded_rect_coverage(vec2 point, vec2 size, float radius, float power) {
  float clamped_radius = clamp(radius, 0.0, 0.5 * min(size.x, size.y));
  vec2 distance = abs(point - size * 0.5) - size * 0.5 + vec2(clamped_radius);
  float signed_distance = power_length(max(distance, vec2(0.0)), power) +
      min(max(distance.x, distance.y), 0.0) - clamped_radius;
  return clamp(0.5 - signed_distance, 0.0, 1.0);
}

vec3 backdrop_color(vec2 output_position) {
  vec2 uv = clamp(output_position / max(zwwm.output_size, vec2(1.0)), vec2(0.0), vec2(1.0));
  return zwwm_blurred_backdrop(uv).rgb;
}

void main() {
  bool fullscreen = (zwwm.state & ZWWM_STATE_FULLSCREEN) != 0u;
  float geometry_scale = config.radius > 0 ? zwwm.clip_radius / float(config.radius) : 1.0;
  if (fullscreen) geometry_scale = 1.0;
  vec4 client = vec4(zwwm.color.rgb * zwwm.color.a, zwwm.color.a);
  if ((zwwm.flags & ZWWM_HAS_TEXTURE) != 0u) client *= zwwm_window(zwwm_texture_rect_uv());
  if ((zwwm.state & ZWWM_STATE_POPUP) != 0u) {
    zwwm_out = client * zwwm.opacity;
    return;
  }

  vec2 rect_size = max(zwwm.clip_rect.zw, vec2(1.0));
  vec2 fragment_position = zwwm_position - zwwm.clip_rect.xy;
  float glass_width_px = float(config.glass_width) * geometry_scale;
  float glass_height_px = float(config.glass_height) * geometry_scale;
  float glass_radius_px = fullscreen ? 0.0 : float(config.glass_radius) * geometry_scale;
  float distortion_depth = float(config.distortion_depth_per_mille) / 1000.0;
  float distortion_strength = float(config.distortion_strength_per_mille) / 1000.0;
  float chromatic_shift_px = float(config.chromatic_shift) * geometry_scale;
  float glass_tint = float(config.glass_tint_per_mille) / 1000.0;
  float rounding_power = clamp(float(config.rounding_power), 1.0, 16.0);
  vec2 glass_size = vec2(glass_width_px > 0.0 ? glass_width_px : rect_size.x,
                         glass_height_px > 0.0 ? glass_height_px : rect_size.y);
  vec2 glass_coordinate = fragment_position - rect_size * 0.5;
  float size = max(min(glass_size.x, glass_size.y), 1.0);
  float sdf_scale = max(max(glass_size.x, glass_size.y), 1.0);
  float inverse_sdf = -sdf(glass_coordinate / sdf_scale, glass_size * 0.5 / sdf_scale,
                           glass_radius_px / sdf_scale, rounding_power) * sdf_scale / size;

  vec3 glass_color;
  if (inverse_sdf < 0.0) {
    glass_color = backdrop_color(zwwm_position);
  } else {
    vec2 direction = safe_normalize(glass_coordinate);
    float center_distance = 1.0 - clamp(inverse_sdf / max(distortion_depth, 0.0001), 0.0, 1.0);
    float distortion = 1.0 - sqrt(max(1.0 - center_distance * center_distance, 0.0));
    vec2 offset = distortion * direction * glass_size * 0.5 * distortion_strength;
    vec2 glass_position = zwwm_position - offset;
    vec2 shift = direction * center_distance * chromatic_shift_px;
    glass_color = vec3(backdrop_color(glass_position - shift).r,
                       backdrop_color(glass_position).g,
                       backdrop_color(glass_position + shift).b) * glass_tint;
  }

  // client_opacity mirrors the window rule so animation fades stay separate.
  float rule_opacity = clamp(float(config.client_opacity) / 100.0, 0.0, 1.0);
  float animation_opacity = rule_opacity > 0.0 ? clamp(zwwm.opacity / rule_opacity, 0.0, 1.0) : 0.0;
  vec4 color = client * rule_opacity + vec4(glass_color, 1.0) * (1.0 - client.a * rule_opacity);
  float coverage = rounded_rect_coverage(fragment_position, zwwm.clip_rect.zw, zwwm.clip_radius, rounding_power);
  zwwm_out = color * coverage * animation_opacity;
}
