# Changelog

## [2.0.0] - 2026-02-05

### Added

- **Desktop Widgets** - Clock and weather widgets on desktop background
  - Drag and resize widgets in edit mode (hot corner or `qs ipc call grid toggle`)
  - Weather auto-detects location via IP using wttr.in
  - Adaptive content scaling based on widget dimensions
  - Widget state persists to `widgets.json`

- **col_gen** - New Python-based Material You color generator
  - Replaces matugen with native Python implementation using `materialyoucolor`
  - Supports all MD3 scheme variants: tonal-spot, expressive, fidelity, fruit-salad, monochrome, neutral, rainbow, vibrant, content
  - New contrast setting (-1.0 to 1.0) for accessibility
  - Generates templates for: quickshell, hyprland, ghostty, gtk3/4, micro
  - Post-hooks: hyprctl reload, ghostty SIGUSR2, gsettings gtk-theme

- **Settings > Advanced** - New settings page for experimental features
  - Desktop widgets toggle (beta)
  - Grid layout customization (columns/rows)
  - Widget styling: radius, border width, opacity, colors

- **Hot Corner** - Redesigned with smooth arc progress indicator

### Changed

- Grid respects status bar exclusive zone (widgets won't overlap bar)
- Widget content adapts layout based on aspect ratio (horizontal vs vertical)
- Scheme names simplified (removed "scheme-" prefix)

### Removed

- **matugen** dependency - replaced by col_gen

### Fixed

- Widget drag/resize coordinate feedback loop
- Exit edit mode button missing MouseArea
- Hot corner using non-existent `Quickshell.execDetached` API

---

## [1.0.0] - Initial Release

- Material You desktop shell for Hyprland
- Dynamic color theming from wallpaper
- Bar, launcher, lockscreen, power menu, notifications
- Settings GUI
