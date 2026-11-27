# blxshell

A Wayland desktop shell written in C++ on [vw](https://github.com/binarylinuxx/vw):
its own renderer and engine, no Qt or GTK. This repo also holds the dotfiles
around it in `.config/`.

The earlier Quickshell (QML) blxshell lives on the
[`blxshell-qs-archive`](https://github.com/binarylinuxx/dots/tree/blxshell-qs-archive) branch.

## Bar

| Left | Center | Right |
|---|---|---|
| Launcher, workspaces (zwwm tags or Hyprland), now playing, window title (Hyprland) | Clock and date | Tray, network, Bluetooth, volume, brightness, battery, power profile, power menu |

Every module hides itself when its service is missing, so the same bar works
on a desktop and a laptop.

| Module | Click | Right click | Scroll |
|---|---|---|---|
| Launcher | runs `$BLXSHELL_LAUNCHER` | | |
| Workspaces | switch | | next / previous |
| Media | play / pause | next track | next / previous |
| Clock | calendar popup (month navigation) | | |
| Tray icon | activate (menu-only items open their menu) | menu popup | forwarded to the item |
| Network | `nm-connection-editor` | Wi-Fi on / off | |
| Bluetooth | power on / off | `blueman-manager` | |
| Volume | mute | `pavucontrol` | ±5 % |
| Brightness | | | ±5 % |
| Power profile | cycle profiles | | |
| Power | menu: lock, suspend, log out, reboot, shut down | | |

Popups close with Escape, by clicking their button again, or by clicking
empty bar space.

## Build

```sh
nix develop    # builds vw from ~/vw alongside (VW_SOURCE_DIR)
cmake -S . -B build -G Ninja
cmake --build build
./build/blxshell
```

Without `VW_SOURCE_DIR` it uses an installed libvw (`find_package(vw)`);
`nix build` does that through the flake's `vw` input. Always clang.

## Commands

The running shell listens on `$XDG_RUNTIME_DIR/blxshell.sock`. `blxshell
<command>` sends one there, so binds and scripts drive the shell with it:

```sh
blxshell launcher [toggle|open|close]
blxshell powermenu [toggle|open|close]
blxshell sidebar [both|top|bottom|corner]
blxshell wallpapers
blxshell lock
blxshell lockscreen edit [lock|greeter]   # the layout editor
blxshell reload                 # re-read config and colours
blxshell media play-pause|pause|next|previous
blxshell ping                   # exits 0 when a shell is running
blxshell settings               # the settings app (runs on its own)
blxshell help
```

Quickshell's form still works, so old binds keep going:
`blxshell ipc call lockscreen lock`, `… powermenu toggle`, `… shell reload`,
`… mpris pauseAll`.

The protocol is one line of words in, one reply out (`ok`, `error: …` or
text), so anything that speaks Unix sockets can use it:

```sh
echo "launcher toggle" | socat - UNIX-CONNECT:$XDG_RUNTIME_DIR/blxshell.sock
```

## Login screen

`blxshell-greeter` is a [greetd](https://sr.ht/~kennylevinsen/greetd/) greeter in the lock screen's style: the stacked clock and date over your frosted wallpaper, and the sheet rising from the bottom with your avatar, the password field and the session to start.

| Key | Does |
| --- | --- |
| any key, or a click | raises the sheet |
| Enter | logs in |
| Up / Down, or the arrows | another user |
| Tab, or the session chip | another session |
| Escape | lowers the sheet |

Power off and Restart sit top left. The last user and session are remembered.

It looks like your lock screen unless you change it under **Settings → Login screen** (`blxshell settings login`): its own clock, date, tint and blur, a separate wallpaper, the hint text, the picture's shape, the power buttons, the idle timeout and whether to remember the last login. **Preview** there shows the result in a window; the settings reach the real login screen with the next export.

greetd runs the greeter as its own user, which cannot read your home, so copy your theme where it can:

```sh
sudo blxshell-greeter --export /var/lib/blxshell-greeter --from ~ --user $USER
```

That copies your config, colour scheme, wallpaper and `~/.face`. Run it again after changing them. Rubik, Google Sans Flex and Material Symbols Rounded must be installed system-wide.

`/etc/greetd/config.toml`, with cage as the greeter's compositor:

```toml
[default_session]
command = "cage -s -- blxshell-greeter"
user = "greeter"
```

On NixOS, the flake's module sets up greetd, a minimal zwwm for the greeter
and the theme sync:

```nix
imports = [ inputs.blxshell.nixosModules.default ];

programs.blxshell.greeter = {
  enable = true;
  syncUser = "alice";   # wear alice's colours, wallpaper and picture
  cursor = { package = pkgs.bibata-cursors; name = "Bibata-Modern-Classic"; size = 24; };
  fonts = [ … ];        # Rubik, Google Sans Flex, Material Symbols Rounded
};
```

Under a compositor with layer shell, `blxshell-greeter --layer` draws one login per monitor instead of a window. Sessions come from `wayland-sessions` in `$XDG_DATA_DIRS` (on NixOS, the system profile).

To try it without logging out, greetd's `fakegreet` stands in for the daemon (user `user`, password `password`, then `9`): `fakegreet ./build/blxshell-greeter`. Run on its own, it shows the screen as a preview.

### Layout editor

`blxshell lockscreen edit` (or **Edit layout** under Settings → Lock screen, or
Login screen) opens the lock screen full screen with everything editable:

- **Move** by dragging. Items snap to the centre lines, the margins and each
  other; hold Shift to drag freely. Arrows nudge by a pixel, Shift+arrows by 10.
- **Resize** with the corner handle, the scroll wheel, `+`/`-`, or the Size
  slider. The side panel also sets opacity, centres the item, and moves it
  in front of or behind the others.
- **Grid** (`G`): a grid of square cells, 6 to 48 across (`[` and `]`). On it
  items move, nudge and resize by whole cells, and the cells under the one
  being dragged light up. Alt+arrows still nudge by a pixel.
- **Add** (`A`): the clock, date, weather, status icons, hint, notifications,
  media player, your picture and name, power buttons (login screen), text
  (strftime codes work, in five colours), and plugins' `Lock…` views, as
  many of each as you like (Ctrl+D duplicates). A plugin's widget shows its
  own settings in the side panel, separately for each copy.
  Delete hides an item; text and plugin items go.
- **Backdrop**: each item's Background slider sets how solid its backdrop is,
  and its swatches pick its colour. Settings → Lock screen sets the colour for
  all of them, including any `#rrggbb`, and turns the wallpaper's blur off.
- **Sheet** (`P`) previews the screen with the password sheet up: items
  shrink into the room above it, and glance-only ones fade.
- Ctrl+Z / Ctrl+Y undo and redo, Ctrl+S saves, Escape twice throws changes away.

The toolbar switches between the lock screen and the login screen. The login
screen uses the lock screen's layout until you save one of its own. Layouts are
saved in `config.json` under `lock.layout` and `greeter.layout`. **Automatic
layout** in settings goes back to the classic arrangement.

## GTK and Qt apps

**Settings → Theme → Apps** themes GTK and Qt apps in the shell's colours and
follows every change. Qt needs `qt6ct`/`qt5ct`, GTK 3 looks best with
`adw-gtk3`; on NixOS, `programs.blxshell.enable = true` installs both.

## Themes and plugins

Themes and plugins are `.blx` packages (zstd-compressed tar with a
`manifest.json`), each flagged with the compositors it works under. A theme
sets colours: a fixed palette, or a matugen generation preset.

```sh
blxshell install forest-1.0.0.blx
blxshell theme apply io.github.you.forest
blxshell theme reset                 # back to your own colours
blxshell plugin enable io.github.you.cpu
blxshell pack ./my-theme             # make a package
```

The format, the plugin protocol and the store rules are in
[docs/packages.md](docs/packages.md). Examples are in `packages/`.

## Configuration

| Variable | Default |
|---|---|
| `BLXSHELL_BAR_POSITION` | `bottom` (or `top`) |
| `BLXSHELL_FONT` | `Google Sans Flex` |
| `BLXSHELL_LAUNCHER` | `fuzzel` |
| `BLXSHELL_LOCK` | `loginctl lock-session` |

Icons are Material Symbols Rounded glyphs, so no icon theme is needed except
for tray and app icons.

## Layout

| File | |
|---|---|
| `src/theme.*` | palette (Catppuccin Mocha), fonts, metrics, environment settings |
| `src/kit.*` | `Item`: pre-measured widgets (icon, label, image, row, button, pill) so backgrounds draw before contents and segments align in one pass |
| `src/modules.*` | one function per bar module |
| `src/bar.*` | composes the modules into left / center / right segments |
| `src/popups.*` | the popup surface: calendar, power menu, tray menus |
| `src/state.hpp` | which popup is open and where |
| `src/archive.*` | `.blx` packages: zstd and a strict tar reader / writer |
| `src/packages.*` | manifests, compatibility checks, install, theme apply, the package CLI |
| `src/plugins.*` | running plugins and their JSON-lines protocol |
| `schemas/` | the manifest JSON Schema |
| `packages/` | built-in themes and an example plugin |

## License

BSD 3-Clause, see [LICENSE](LICENSE). The fonts blxshell uses keep their own
licenses (see `nix/fonts.nix`).
