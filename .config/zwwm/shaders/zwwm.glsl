// zwwm shader ABI; include it directly after `#version 450`.
// Pixels and gl_FragCoord have a top-left origin; screen textures are top-down.
#ifndef ZWWM_GLSL
#define ZWWM_GLSL

#extension GL_EXT_nonuniform_qualifier : require

// ZwwmDraw.flags
const uint ZWWM_HAS_TEXTURE = 1u;
const uint ZWWM_HAS_TEXTURE_RECT = 2u;
const uint ZWWM_BACKGROUND_ONLY = 4u;
const uint ZWWM_HAS_CLIP = 8u;
const uint ZWWM_STRICT_CLIP = 16u;

// ZwwmDraw.state
const uint ZWWM_STATE_FOCUSED = 1u;
const uint ZWWM_STATE_FULLSCREEN = 2u;
const uint ZWWM_STATE_POPUP = 4u;

layout(set = 0, binding = 0, std140) uniform ZwwmDraw {
  vec4 draw_rect;      // this quad in output pixels: x, y, width, height
  vec4 toplevel_rect;  // root surface rectangle shared by a window's draws
  vec4 texture_rect;   // destination of the client buffer
  vec4 clip_rect;      // effective clip (draw_rect when unclipped)
  vec4 source_uv;      // client buffer source: left, top, right, bottom
  vec4 color;          // straight-alpha draw color
  vec4 input_to_clip_0;
  vec4 input_to_clip_1;
  vec4 glass_tint;
  vec2 output_size;    // render target size in pixels
  float opacity;
  float time;          // monotonic seconds, wrapping after 1000 seconds
  float corner_radius; // already scaled for animation and canvas zoom
  float border_width;
  float clip_radius;
  float texture_corner_radius;
  float mask_threshold;
  uint flags;
  int texture_transform;  // 0 normal, 1 90deg, 2 180deg, 3 270deg
  uint state;
  int window_texture;     // indices into zwwm_textures; 0 is opaque black
  int backdrop_texture;   // sharp scene below this draw
  int blurred_backdrop_texture;
  int background_texture; // configured wallpaper
  float glass_refraction;
  float glass_dispersion;
  float glass_frost;
  float glass_ior;
  float glass_thickness;
  float glass_roughness;
  vec2 texel_size;
  float sample_offset;
  float brightness;
  float contrast;
  float saturation;
  float noise;
  float reserved;
} zwwm;

layout(set = 0, binding = 1) uniform sampler2D zwwm_textures[];

vec4 zwwm_sample(int index, vec2 uv) { return texture(zwwm_textures[max(index, 0)], uv); }
vec4 zwwm_window(vec2 uv) { return zwwm_sample(zwwm.window_texture, uv); }
vec4 zwwm_backdrop(vec2 uv) { return zwwm_sample(zwwm.backdrop_texture, uv); }
vec4 zwwm_blurred_backdrop(vec2 uv) { return zwwm_sample(zwwm.blurred_backdrop_texture, uv); }
vec4 zwwm_background(vec2 uv) { return zwwm_sample(zwwm.background_texture, uv); }
ivec2 zwwm_background_size() { return textureSize(zwwm_textures[max(zwwm.background_texture, 0)], 0); }

vec2 zwwm_transform(vec2 position) {
  if (zwwm.texture_transform == 1) return vec2(position.y, 1.0 - position.x);
  if (zwwm.texture_transform == 2) return vec2(1.0) - position;
  if (zwwm.texture_transform == 3) return vec2(1.0 - position.y, position.x);
  return position;
}

float zwwm_rounded_rect_distance(vec2 point, vec2 size, float radius) {
  float r = clamp(radius, 0.0, 0.5 * min(size.x, size.y));
  vec2 half_size = size * 0.5;
  vec2 corner = abs(point - half_size) - half_size + vec2(r);
  return length(max(corner, vec2(0.0))) + min(max(corner.x, corner.y), 0.0) - r;
}

#ifdef ZWWM_FRAGMENT_STAGE
layout(location = 0) in vec2 zwwm_uv;        // client texture coordinates
layout(location = 1) in vec2 zwwm_local;     // pixels within zwwm.draw_rect
layout(location = 2) in vec2 zwwm_position;  // output pixels
layout(location = 0) out vec4 zwwm_out;      // premultiplied alpha

vec2 zwwm_screen_uv() { return gl_FragCoord.xy / max(zwwm.output_size, vec2(1.0)); }

// Keeps undersized client buffers from stretching over decorations.
vec2 zwwm_texture_rect_uv_at(vec2 output_position) {
  vec2 position = clamp((output_position - zwwm.texture_rect.xy) / max(zwwm.texture_rect.zw, vec2(1.0)),
                        vec2(0.0), vec2(1.0));
  vec2 uv = mix(zwwm.source_uv.xy, zwwm.source_uv.zw, zwwm_transform(position));
  // Half a texel inside, so filtering skips pixels outside the window geometry.
  vec2 half_texel = 0.5 / vec2(max(textureSize(zwwm_textures[max(zwwm.window_texture, 0)], 0), ivec2(1)));
  vec2 low = min(zwwm.source_uv.xy, zwwm.source_uv.zw) + half_texel;
  vec2 high = max(zwwm.source_uv.xy, zwwm.source_uv.zw) - half_texel;
  return clamp(uv, min(low, high), max(low, high));
}
vec2 zwwm_texture_rect_uv() { return zwwm_texture_rect_uv_at(zwwm_position); }

float zwwm_coverage(float distance) {
  float smoothing = max(fwidth(distance), 0.0001);
  return 1.0 - smoothstep(-0.5 * smoothing, 0.5 * smoothing, distance);
}
#endif

#endif
