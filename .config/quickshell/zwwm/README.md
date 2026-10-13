# zwwm compositor plugin

`BlxShell.Zwwm` is a Qt 6 C++ QML module using a dedicated, event-driven
Wayland connection. It supplies the existing shell compositor service with
tags, active tag, outputs, window titles, focus state and workspace occupancy.
Workspace clicks and scrolling select tags; taskbar clicks focus windows.
The shared shell surface uses the top layer and automatically hides, together
with its frame/bar reservations, while a fullscreen window is on that output's
active tag. It returns when the window exits fullscreen or the active tag changes.
No command-line polling is used. Connection failures trigger reconnection.

## Build and run

Dependencies: a C/C++ compiler, CMake, pkg-config, Qt 6.5+ Core/Qml,
libwayland-client, wayland-scanner and wayland-protocols (xdg-shell).
The installer builds the plugin into the runtime's `qml` directory. The Nix
wrapper includes the `blxshell-zwwm` package in its import path automatically.

For development, from the repository root:

```sh
cmake -S .config/quickshell/zwwm -B .config/quickshell/zwwm/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/.config/quickshell" -DINSTALL_QML_PREFIX=qml
cmake --build .config/quickshell/zwwm/build --parallel
cmake --install .config/quickshell/zwwm/build
```

On Nix, enter `nix develop` first to get the build dependencies.
Launch the normal `shell.qml` entrypoint with `blxshell start`.
The service loads zwwm when there is no `HYPRLAND_INSTANCE_SIGNATURE`.
Use `BLXSHELL_COMPOSITOR=zwwm blxshell start` to explicitly select it,
including when a stale Hyprland environment variable is present.

## Protocols and API

The XML files under `protocols/` preserve the wire declarations from zwwm:

- `ext-zwwm-manager-v1` (bind up to v6): event subscriptions, typed state
  snapshots, keyboard layout and dispatch actions.
- `zwwm-tags-unstable-v1` (v1): tag names, atomic tag-list updates,
  active tags and selection on the focused output. The current compositor
  accepts zero or one selected tag; invalid IDs are rejected locally.
- `zwwm-window-manager-v1` (optional v2): complete window tag membership,
  output state and focus-by-ID. The plugin never claims window policy.
  Another policy owner may reject focus requests; failures are reported.

The tags protocol's `xdg_toplevel` objects belong to the connecting client,
so it cannot observe other applications' tag membership. Without the optional
window-manager v2 global, titles/focus still work through the ext manager,
but per-window membership is unknown (`workspace.id = 0`) and taskbar
activation is unavailable. IDs are decimal strings to preserve 64-bit precision.

The QML `Zwwm` object exposes `available`, `ready`, `tags`, `activeTags`,
`clients`, `monitors`, `keyboardLayout`, `canActivate` and `lastError`.
Methods: `refresh()`, `setActiveTags(ids)`, `activateClient(id)` and
`dispatch(action, argument)`. Errors emit `commandFailed(message)`.

Hyprland-specific screen-shader night light and `hyprctl` DPMS/screenshot
helpers are separate from this compositor-state integration.
