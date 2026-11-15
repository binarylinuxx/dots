#version 450
#include "zwwm.glsl"

layout(set = 1, binding = 0, std140) uniform Config {
  int radius;
} config;

void main() {
  vec4 color = vec4(zwwm.color.rgb * zwwm.color.a, zwwm.color.a);
  if ((zwwm.flags & ZWWM_HAS_TEXTURE) != 0u) color *= zwwm_window(zwwm_texture_rect_uv());
  if ((zwwm.state & ZWWM_STATE_POPUP) != 0u) {
    zwwm_out = color * zwwm.opacity;
    return;
  }
  vec3 backdrop = zwwm_blurred_backdrop(clamp(zwwm_screen_uv(), vec2(0.0), vec2(1.0))).rgb;
  color += vec4(backdrop, 1.0) * (1.0 - color.a);
  float coverage = clamp(0.5 - zwwm_rounded_rect_distance(zwwm_position - zwwm.clip_rect.xy, zwwm.clip_rect.zw,
                                                          zwwm.clip_radius), 0.0, 1.0);
  zwwm_out = color * zwwm.opacity * coverage;
}
