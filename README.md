<div align="center">
<br>
<h1>blxshell</h1>
<h3>Material You Desktop Environment for Hyprland</h3>
<p>


https://github.com/user-attachments/assets/a82b691f-ba58-4d4f-b756-543f4b52b5cb


A fully custom Wayland desktop shell built with
<a href="https://quickshell.outfoxxed.me">Quickshell</a>,
<a href="https://github.com/InioX/matugen">Matugen</a> and
<a href="https://hyprland.org">Hyprland</a>.<br>
Dynamic Material You color theming generated from your wallpaper.
</p>

<a href="https://github.com/binarylinuxx/dots/commits/main"><img src="https://img.shields.io/github/last-commit/binarylinuxx/dots?style=for-the-badge&labelColor=1e1b1e&color=e1b8f2" alt="last commit"></a>
<a href="https://github.com/binarylinuxx/dots"><img src="https://img.shields.io/github/repo-size/binarylinuxx/dots?style=for-the-badge&labelColor=1e1b1e&color=e1b8f2" alt="repo size"></a>
<a href="https://github.com/binarylinuxx/dots/stargazers"><img src="https://img.shields.io/github/stars/binarylinuxx/dots?style=for-the-badge&labelColor=1e1b1e&color=e1b8f2" alt="stars"></a>

</div>

---

## Shell Components

| Component | Description |
|-----------|-------------|
| **Bar** | Top panel -- workspaces, system tray, clock, battery, audio, network |
| **Launcher** | App launcher with emoji, clipboard and wallpaper modes |
| **Lockscreen** | Material You bold lockscreen with PAM authentication |
| **Power Menu** | Shutdown, reboot, suspend, logout, lock |
| **Notifications** | Built-in notification daemon |
| **Background** | Wallpaper with parallax effect and startup zoom animation |
| **Settings** | GUI settings panel (`Win+S`) |
| **Audio OSD** | Volume and brightness overlay |

## Design

- **Material You** color scheme generated from wallpaper via Matugen
- **Material Symbols Rounded** icons throughout the entire shell
- All colors are reactive -- change wallpaper, the entire shell updates
- Smooth animations and transitions on every interaction
- Fully configurable via `config.json`

## Stack

| Component | Tool |
|-----------|------|
| Compositor | [Hyprland](https://hyprland.org) |
| Shell / Bar / Widgets | [Quickshell](https://quickshell.outfoxxed.me) (QML) |
| Color generation | [Matugen](https://github.com/InioX/matugen) |
| Terminal | [Ghostty](https://ghostty.org) |
| Shell | [Fish](https://fishshell.com) |
| Prompt | [Starship](https://starship.rs) |
| Fetch | [Fastfetch](https://github.com/fastfetch-cli/fastfetch) |

## IPC Control

The shell exposes IPC handlers for external control via `qs ipc`. Bind these to Hyprland keybinds or call from terminal.

```bash
# Lockscreen
qs ipc call lockscreen lock
qs ipc call lockscreen unlock
qs ipc call lockscreen isLocked

# Power Menu
qs ipc call -- powermenu toggle
qs ipc call -- powermenu show
qs ipc call -- powermenu hide
```

Hyprland keybind examples:

```ini
bind = SUPER, L, exec, qs ipc call lockscreen lock
bind = SUPER, Escape, exec, qs ipc call -- powermenu toggle
```

## Keybinds You Must Know

| Keybind | Action |
|---------|--------|
| `Super + Return` | Open terminal |
| `Super + Space` | Open launcher |
| `Super + I` | Switch workspace |
| `Super + S` | Open settings |
| `Super + L` | Lock screen |
| `Super + P` | Power menu |

## Project Structure

```
.config/quickshell/
  shell.qml                 # Main entry -- all components, IPC, colors
  config.json               # User settings (editable via Settings panel)
  Colors.json               # Matugen generated Material You colors

  bar/
    Bar.qml                 # Top bar layout
    widgets/                # Clock, Audio, Battery, Network, Workspaces, etc.

  modules/
    Background.qml          # Wallpaper + parallax + fallback gradient
    BackgroundClock.qml     # Desktop clock overlay
    LockContext.qml          # PAM authentication logic
    LockSurface.qml          # Lockscreen UI
    PowerMenu.qml            # Power/logout menu

  launcher/                 # App launcher
  notifications/            # Notification daemon
  widgets/                  # Settings, Audio OSD, Screen corners
  services/                 # NetworkManager, Sway, OS detection
  lockscreen/pam/           # PAM config for lockscreen auth
```

## Installation

Arch Linux only. The install script handles all dependencies via custom metapackages.

```bash
# One-liner (remote)
bash <(curl -fsSL https://raw.githubusercontent.com/binarylinuxx/dots/main/install.sh)

# Or clone and run locally
git clone https://github.com/binarylinuxx/dots.git
cd dots
./install.sh
```

The installer will:

1. Install `yay` if not present
2. Build and install metapackages (`blxshell-shell`, `blxshell-audio`, `blxshell-hyprland`, fonts)
3. Optionally backup and replace `~/.config`

## Configuration

All settings live in `~/.config/quickshell/config.json` and can be changed from the Settings panel (`Win+S`).

```jsonc
{
    "barFloating": false,
    "barOnTop": true,
    "barHeight": 35,
    "barRadius": 30,
    "screenCorners": true,
    "workspaceCount": 10,
    "workspaceStyle": "dots",
    "wallpaperParallax": true,
    "wallpaperStartupZoom": true,
    "fontFamily": "Rubik",
    "fontSize": 14
}
```

## Wallpapers

Place wallpapers in `~/.local/wallpapers/`. Change wallpaper from the Settings panel or Launcher wallpaper mode.

If no wallpaper is set, a gradient fallback background is shown using your current theme colors with a hint to get started.

