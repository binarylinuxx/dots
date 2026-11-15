#pragma once

#include <vw/animation.hpp>
#include <vw/ui.hpp>

#include <cstdint>
#include <string>

// Design tokens, after ~/shell's Services/Properties.qml and Colours.qml.
namespace blx {

namespace font {
// Set from the config by sync_style().
inline const char* display = "Rubik";
inline const char* utility = "Google Sans Flex";
inline constexpr const char* icon = "Material Symbols Rounded";
}  // namespace font

namespace props {
// The first group is live: sync_style() sets it from the config.
inline float frame_thickness = 12;
inline float frame_radius = 25;
inline float bar_height = 48;
inline float blob_smoothing = 30;
inline float popout_radius = 14;
inline float launcher_radius = 36;
inline float island_radius = 24;
inline float popout_frame_overlap = 10;
inline float animation_speed = 1;  // 2 is twice as fast
inline int motion_style = 0;       // 0 bounce, 1 smooth, 2 snappy
inline int hover_linger_ms = 180;
inline float font_scale = 1;
inline constexpr float card_radius = 18;
inline constexpr float notification_radius = 18;
inline constexpr float launcher_width = 700;
// Caelestia utility tokens.
inline constexpr float utility_extra_small = 4;
inline constexpr float utility_small = 8;
inline constexpr float utility_medium = 12;
inline constexpr float utility_large = 16;
inline constexpr float utility_radius_extra_small = 4;
inline constexpr float utility_radius_small = 8;
inline constexpr float utility_radius_medium = 12;
inline float utility_radius_large = 16;
}  // namespace props

// Material colour roles, from the Matugen scheme ~/shell writes to
// ~/.local/state/blxshell/matugen-colours.json.
struct Colours {
  vw::Color primary = vw::rgb(0x9ccaff);
  vw::Color primary_text = vw::rgb(0x10202e);
  vw::Color primary_container = vw::rgb(0x263b4d);
  vw::Color secondary = vw::rgb(0xd5adff);
  vw::Color secondary_container = vw::rgb(0x443653);
  vw::Color tertiary = vw::rgb(0x9de2c0);
  vw::Color tertiary_container = vw::rgb(0x244b3b);
  vw::Color error = vw::rgb(0xff8f94);
  // Text and icons on error, and the softer container for critical items.
  vw::Color error_text = vw::rgb(0x690005);
  vw::Color error_container = vw::rgb(0x93000a);
  vw::Color error_container_text = vw::rgb(0xffdad6);
  vw::Color surface = vw::rgb(0x101419);
  vw::Color surface_container = vw::rgb(0x1b2732);
  vw::Color surface_container_low = vw::rgb(0x172129);
  vw::Color surface_container_high = vw::rgb(0x2a3947);
  vw::Color surface_container_highest = vw::rgb(0x334654);
  vw::Color surface_variant = vw::rgb(0x314555);
  vw::Color text = vw::rgb(0xeaf1f6);
  vw::Color text_muted = vw::rgb(0x93a9b9);
  vw::Color outline = vw::rgb(0x71899b);
  vw::Color outline_variant = vw::rgb(0x3f5362);
};

// The current scheme; reloaded when the scheme file changes.
const Colours& colours();
// Applies the config's shape, fonts and animation speed to the tokens above.
void sync_style();
// Readable when the scheme file changes; pass to vw::App::on_event.
int colours_event_fd();
bool reload_colours();
// Bumped whenever ~/.local/state/blxshell changes (scheme or wallpaper).
std::uint64_t state_generation();

vw::Color with_alpha(vw::Color color, float alpha);
// A colour role by its QML name ("primary", "surfaceContainer", ...).
vw::Color role_colour(const std::string& role);

// Motion tokens (Properties.qml).
namespace motion {
// motionBoundedBounce: a fast arrival that settles without overshooting
// its end points.
inline vw::AnimationOptions bounce() {
  if (props::motion_style == 1) return {.duration = 0.4 / props::animation_speed, .easing = vw::Easing::in_out_cubic};
  if (props::motion_style == 2) return {.duration = 0.2 / props::animation_speed, .easing = vw::Easing::out_quint};
  return {.duration = 0.46 / props::animation_speed, .easing = vw::Easing::bezier, .bezier = {0.15, 0.65, 0.3, 1.0}};
}
inline vw::AnimationOptions quick() { return {.duration = 0.18 / props::animation_speed, .easing = vw::Easing::out_cubic}; }
inline vw::AnimationOptions standard() { return {.duration = 0.22 / props::animation_speed, .easing = vw::Easing::out_cubic}; }
inline vw::AnimationOptions emphasized() { return {.duration = 0.28 / props::animation_speed, .easing = vw::Easing::out_cubic}; }
}  // namespace motion
// Mixes towards `to` by t, e.g. hover state layers.
vw::Color mix(vw::Color from, vw::Color to, float t);

}  // namespace blx
