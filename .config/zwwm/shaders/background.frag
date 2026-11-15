#version 450
#include "zwwm.glsl"

layout(set = 1, binding = 0, std140) uniform Config {
  vec4 top_color;
  vec4 bottom_color;
} config;

void main() {
  float y = zwwm_local.y / max(zwwm.draw_rect.w, 1.0);
  zwwm_out = vec4(mix(config.top_color.rgb, config.bottom_color.rgb, y), 1.0);
}
