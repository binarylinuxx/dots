#pragma once

#include <vw/ui.hpp>

#include <string>

// The shell's layer surfaces, after ~/shell's shell.qml.
#include "blob.hpp"

namespace blx {

// The single visual surface: frame, bar and every panel as one blob.
void shell_surface(vw::Ui& ui);
// Gives the group the frosted wallpaper; false when there is none.
// The wallpaper, dual-Kawase blurred with the lock screen's settings.
bool apply_lock_frost(blob::Group& group);
// Regenerates the colour scheme from the current wallpaper with the
// config's Matugen settings (Colours.refresh()).
void regenerate_colours();
// Sets the wallpaper (Wallpaper.setWallpaper + Config.setWallpaperPath).
void set_wallpaper(const std::string& path);
// The wallpaper on the background layer.
void wallpaper_surface(vw::Ui& ui);
// Invisible surfaces that only reserve the frame's space (EdgeExclusion).
void reserve_left(vw::Ui& ui);
void reserve_top(vw::Ui& ui);
void reserve_right(vw::Ui& ui);
void reserve_bottom(vw::Ui& ui);

}  // namespace blx
