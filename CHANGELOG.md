# Changelog

## [3.0.0] - 2026-05-27

### Added
- Hyprland Lua config tree for Hyprland `0.55+`
  - New `hyprland.lua` entrypoint
  - Lua modules for monitors, variables, autostart, visuals, binds, rules, and generated colors
- Quickshell now works via cli default dir $HOME/.local/blxshell
- NixOS flake (`github:binarylinuxx/dots`) with packages:
  - `blxshell` — caelestia-style wrapper (`writeShellApplication` wrapping `qs -p`)
  - `blxshell-custom-fonts` and individual font packages
  - `blxshell-col-gen` — Python color generator as Nix environment
  - `blxshell-plugin-manager` — compiled QML plugin
  - `blxshell-bundle` — all packages combined
- NixOS module (`nixosModules.default`) with:
  - `services.blxshellMprisFixForZen` — single option for MPRIS setup
- All QML paths switched from `~/.local/blxshell` to XDG (`Quickshell.env("XDG_*_HOME")`)
- `playerctl` and `pulseaudio` in wrapper `runtimeInputs` (available to all QML Process calls)
- Writable paths created on first use via JsonAdapter defaults
### Changed
- blxshell now uses the CLI/runtime layout by default
  - Runtime path: `~/.local/blxshell`
  - CLI path: `~/.local/bin/blxshell`
  - Installer now installs both runtime and CLI and prints a migration notice
- Hyprland configs are now lua migrated
- Quickshell config synced from the live `~/.local/blxshell` runtime and cleaned for repository use
- Added plugins system(beta)
- `restart` command uses `.quickshell-wra` process name (NixOS Qt wrapper naming)
- Bar corner fix: screen-edge corners rounded for both top and bottom bar positions
### Fixed
- Installer no longer leaves `blxshell start` pointing at a missing runtime path
- Plugin manager now installs into the active blxshell runtime path instead of stale `~/.config/quickshell`
- `Invalid write to global property "removedTypes"` warning in Settings.qml
- StyledSlider.qml deprecated signal handler parameter syntax

## [2.5.1] - 2026-03-13

### Added
- openSUSE Tumbleweed support
  - New `opensuse-deps/` directory with RPM spec files mirroring all `arch-deps/` metapackages
  - Specs: `blxshell-shell`, `blxshell-audio`, `blxshell-hyprland`, `blxshell-quickshell-git`, `blxshell-fonts` and all font sub-packages
  - Four new font specs for packages missing from Tumbleweed repos: `blxshell-font-material-symbols`, `blxshell-font-readex-pro`, `blxshell-font-rubik`, `blxshell-font-space-grotesk`
  - Packages published to OBS at `home:binarylinuxx:blxshell`
- OS detection now recognises openSUSE Tumbleweed (`IS_OPENSUSE=true`)
- Installer automatically adds `home:binarylinuxx:blxshell` OBS repo and installs via `zypper`

## [2.5.0] - 2026-03-07

### Added

- **Technical**
  - since now blx-sshell was heavily tested and reworked to work with portable-devices(LapTops)
  - implemented simple easy to use WiFi menu so you can be connect to network out of the box

- **Desktop Widgets**
  - weather widget now can use API from openweathermap if you want more precise location than automatic by API

- **Desktop**
  - now added sidebar with quick toggle's

### Changed
- Improved NetworkManager service so it works now perfectly
- Baterry service are now custom

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
