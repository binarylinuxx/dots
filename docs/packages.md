# Themes and plugins

Themes and plugins ship as **`.blx` packages**: a tar archive compressed with
zstd, with `manifest.json` at its root. The manifest's `"type"` says whether
it is a `"theme"` or a `"plugin"`.

```
my-theme-1.0.0.blx            (tar.zst)
├── manifest.json             required
├── preview.png               optional, listed in "previews"
├── wallpaper.png             a theme's wallpaper, optional
└── bin/plugin                a plugin's program
```

```sh
blxshell pack ./my-theme              # writes <id>-<version>.blx, then reads it back to check it
blxshell inspect my-theme-1.0.0.blx   # shows it and lists every problem
blxshell install my-theme-1.0.0.blx   # installs or updates it
blxshell theme apply <id>             # or: blxshell theme apply my-theme-1.0.0.blx
blxshell theme reset                  # back to your own colour settings
blxshell plugin enable <id>
blxshell remove <id>
```

`tar --zstd -cf x.blx -C folder .` makes a valid package too; `blxshell pack`
also checks it and gives reproducible output (sorted entries, mtime 0, owner
0).

## Repositories

Prebuilt packages come from repositories. The official ones,
[binarylinuxx/dots-plugins](https://github.com/binarylinuxx/dots-plugins) and
[binarylinuxx/dots-themes](https://github.com/binarylinuxx/dots-themes), are
always used first; more go in `plugins.repositories` in config.json.

```sh
blxshell search [words]               # what the repositories offer
blxshell install <id> [--force]       # download, check and install one
blxshell update [id…]                 # newer versions of what you installed
blxshell repo list
blxshell repo add owner/name          # a GitHub repository
blxshell repo add https://…/index.json
blxshell repo add ~/code/my-plugins/build
blxshell repo remove owner/name
```

A repository publishes an `index.json` next to its `.blx` files. For a GitHub
repository `owner/name`, blxshell reads
`https://github.com/owner/name/releases/download/packages/index.json`:

```json
{
  "schema": 1,
  "name": "blxshell official",
  "repository": "binarylinuxx/dots-plugins",
  "packages": [
    {
      "id": "dev.blxshell.pomodoro", "type": "plugin", "name": "Pomodoro",
      "version": "1.0.0", "description": "…", "compositors": ["any"],
      "permissions": ["notifications"], "blxshell": ">=0.1.0",
      "url": "https://github.com/…/releases/download/packages/dev.blxshell.pomodoro-1.0.0.blx",
      "sha256": "…", "size": 16074
    }
  ]
}
```

- `install <id>` checks `compositors` and `blxshell` from the index before
  downloading anything, shows the permissions, downloads with curl over HTTPS,
  and refuses a file whose size or SHA-256 doesn't match the index. The file
  then goes through the same checks as any `.blx`.
- The first repository listing an id wins, so a custom repository can't
  replace an official package.
- `update` only touches packages in `~/.local/share/blxshell`; system-wide
  ones are the distribution's. An updated plugin that's on is restarted.
- Downloads are kept in `~/.cache/blxshell/packages/`.
- A local repository (a folder, or `file://`) may point at local files; a
  remote one may only use HTTPS.

dots-plugins' `tools/build.py` and `.github/workflows/release.yml` build and
publish a repository this way; copy them for your own.

## Where packages live

| Folder | |
|---|---|
| `~/.local/share/blxshell/{themes,plugins}/<id>/` | what `blxshell install` unpacks, first in line |
| `$XDG_DATA_DIRS/blxshell/{themes,plugins}/<id>/` | system-wide; blxshell's own themes install here |

A user package hides a system one with the same id.

## Safety rules for a package

`install` refuses the whole package if any of these fail, because packages
come from the internet:

- It must be zstd-compressed tar (ustar, GNU or PAX).
- Only regular files and folders: no symlinks, hard links, devices or FIFOs.
- No absolute paths and no `..`.
- At most 64 MiB unpacked and 4096 entries.
- `manifest.json` must be valid (below). Paths it names (`icon`, `previews` and
  `plugin.exec[0]`) must exist in the package.

Installing swaps the folder in one rename, so a running plugin never sees a
half-written update.

## manifest.json

The JSON Schema is `schemas/manifest.schema.json` (also installed to
`share/blxshell/schemas/`). Put `"$schema"` in your manifest for editor
completion. blxshell validates the same rules itself, so the schema and
`blxshell inspect` agree.

| Key | Required | |
|---|---|---|
| `schema` | yes | `1`. A newer schema is refused rather than half understood. |
| `type` | yes | `"theme"` or `"plugin"` |
| `id` | yes | reverse-DNS, lowercase: `io.github.you.name`. Unique in the store; the install folder's name. |
| `name` | yes | display name |
| `version` | yes | semver, `1.2.3` |
| `compositors` | yes | where it works: any of `"zwwm"`, `"hyprland"`, `"halley"`, or `["any"]` |
| `project` | | the project it's for; `"blxshell"` when left out. See [Project ids](#project-ids). |
| `blxshell` | | version range of that project: `">=0.1.0"`, `">=0.1.0 <0.3.0"`, `"^0.1.0"`, `"~0.1.2"`, `"*"`, groups joined by `\|\|` |
| `description`, `license` (SPDX), `homepage`, `repository` | | |
| `authors` | | `["Name"]` or `[{"name", "email", "url"}]` |
| `tags` | | up to 12, for store search |
| `icon`, `previews` | | image paths inside the package |
| `colours` | themes | below |
| `wallpaper` | | themes: an image in the package, see [Wallpapers](#wallpapers) |
| `plugin` | plugins | below |

### Compatibility

`compositors` is enforced. Under a compositor that isn't listed:

- `theme apply` refuses (`--force` applies anyway);
- an enabled plugin isn't started, and `plugin enable` refuses;
- `install` still installs it and prints why it won't run.

A plugin that only reads `/proc` can say `["any"]`. One that drives
workspaces should list the compositors it actually speaks to. Unknown
compositor names only warn, so a package for a newer blxshell still
installs. The running compositor is detected the same way the shell
detects it: Halley, then zwwm, then Hyprland.

`blxshell` is checked against the shell's version (`blxshell inspect` shows
both).

## Themes

A theme sets colours only. It is one of two kinds:

- **static**: a fixed colour for every role. The wallpaper no longer
  changes anything.
- **generated**: a generation preset. The palette is generated with matugen
  from the wallpaper or a seed colour, using the theme's settings, and can
  pin a few roles on top.

Applying one edits the colour settings in `~/.config/blxshell/config.json`
(the previous file is kept as `config.json.bak`), and the running shell
updates live. `blxshell theme reset` puts back the colour settings you had
before your first theme.

### Static

```json
{
  "schema": 1,
  "type": "theme",
  "id": "io.github.you.forest",
  "name": "Forest",
  "version": "1.0.0",
  "compositors": ["any"],
  "colours": {
    "kind": "static",
    "roles": {
      "primary": "#a8d08d", "primaryText": "#173807", "primaryContainer": "#2d4f1d",
      "secondary": "#bccbb0", "secondaryContainer": "#3d4b35",
      "tertiary": "#a0cfcf", "tertiaryContainer": "#1f4e4e",
      "error": "#ffb4ab", "errorText": "#690005", "errorContainer": "#93000a", "errorContainerText": "#ffdad6",
      "surface": "#11140f", "surfaceContainer": "#1e201b", "surfaceContainerLow": "#1a1c17",
      "surfaceContainerHigh": "#282b25", "surfaceContainerHighest": "#33362f", "surfaceVariant": "#43483e",
      "text": "#e2e3da", "textMuted": "#c3c8bb", "outline": "#8d9286", "outlineVariant": "#43483e"
    }
  }
}
```

All 21 roles are required. A light theme is just a list with light
surfaces and dark text.

### Generated

```json
{
  "schema": 1,
  "type": "theme",
  "id": "io.github.you.ember",
  "name": "Ember",
  "version": "1.0.0",
  "compositors": ["any"],
  "colours": {
    "kind": "generated",
    "source": "seed",
    "seed": "#ff6d3a",
    "mode": "dark",
    "scheme": "vibrant",
    "contrast": 0.2,
    "prefer": "saturation",
    "preset": "amoled",
    "roles": { "error": "#ff5449" }
  }
}
```

| Key | | Default |
|---|---|---|
| `source` | `"wallpaper"` or `"seed"` | `seed` when `seed` is given, else `wallpaper` |
| `seed` | `#rrggbb` | |
| `mode` | `"dark"` or `"light"` | `dark` |
| `scheme` | `tonal-spot`, `content`, `expressive`, `fidelity`, `fruit-salad`, `monochrome`, `neutral`, `rainbow`, `vibrant` | `tonal-spot` |
| `contrast` | -1 to 1 | 0 |
| `prefer` | which wallpaper colour: `saturation`, `darkness`, `lightness` | `saturation` |
| `preset` | `generated`, `neutral` (neutral surfaces), `amoled` (black surfaces), `mono` (grey accents) | `generated` |
| `roles` | some roles pinned over the generated ones | none |

Settings a generated theme leaves out go back to their defaults, so the
same theme always looks the same.

### Wallpapers

A theme can ship a wallpaper: put the image in the package and name it in
`"wallpaper"` (`.png`, `.jpg`, `.webp` or `.svg`). Applying the theme sets it
as the wallpaper. With a generated theme whose `source` is `"wallpaper"`, the
palette then comes from that image, so the two always match:

```json
{
  "schema": 1,
  "type": "theme",
  "id": "io.github.you.dune",
  "name": "Dune",
  "version": "1.0.0",
  "compositors": ["any"],
  "wallpaper": "wallpaper.png",
  "previews": ["preview.png"],
  "colours": { "kind": "generated", "source": "wallpaper", "scheme": "tonal-spot" }
}
```

`blxshell theme reset` puts back the wallpaper you had before your first
theme too, unless you've picked another one since. A static theme with a
wallpaper keeps its fixed colours whatever the image is.

### Roles

`primary`, `primaryText`, `primaryContainer`, `secondary`,
`secondaryContainer`, `tertiary`, `tertiaryContainer`, `error`, `errorText`,
`errorContainer`, `errorContainerText`, `surface`, `surfaceContainer`,
`surfaceContainerLow`, `surfaceContainerHigh`, `surfaceContainerHighest`,
`surfaceVariant`, `text`, `textMuted`, `outline`, `outlineVariant`.
`…Text` roles are the colour of text on that role.

## Plugins

A plugin (api 2) has any of three entry points, at least one:

| | | |
|---|---|---|
| `facet` | its views in [Facet](https://github.com/binarylinuxx/vw/tree/main/facet), interpreted by the shell | the usual way: draws natively, reloads live, sees only what the shell hands it |
| `exec` | a process in any language behind the views, over JSON lines | logic that needs a real language: reading `/proc`, timers, files, the network |
| `native` | a C++ library drawing with vw (`<blxshell/plugin.hpp>`) | when Facet isn't enough; runs inside the shell, so it needs the `native` permission |

The manifest only says who the plugin is and what it may do. What it shows,
and its settings, live in its code:

```json
{
  "schema": 1,
  "type": "plugin",
  "id": "io.github.you.cpu",
  "name": "CPU",
  "version": "1.0.0",
  "compositors": ["any"],
  "plugin": {
    "api": 2,
    "facet": "plugin.facet",
    "exec": ["bin/cpu.py"],
    "restart": "on-failure",
    "permissions": ["shell"]
  }
}
```

Plugins are installed off. `blxshell plugin enable <id>` (or the switch in
**Settings → Plugins**) turns one on.

### Views

Views named `Bar…` are bar modules, placed as `"<plugin id>:<view>"` in
`bar.left`, `bar.center` or `bar.right` (or from **Settings → Plugins**). A
view named `Settings` is the plugin's page in **Settings → Plugins**. A view
named `Wallpaper` is drawn full-screen over the wallpaper, under the desktop
icons and widgets; the first enabled plugin that has one wins. Views named
`Lock…` are lock screen widgets: the user adds them from the layout editor
(`blxshell lockscreen edit`, **Add**), then moves and resizes them like the
clock. They size themselves from `state.lock.scale`, and while locked they
are shown only: clicks on them never reach the plugin. The login screen runs
no plugins, so they never appear there. A C++ plugin's `Lock…` views read the same
through `Host::context()`, JSON text like `{"lock": {"scale": 1.3, …}}`.

### Lock materials

A plugin can restyle every backdrop on the lock screen (the pills, cards,
widgets and the password sheet) with a rectangle shader that sees the
wallpaper behind it:

```json
"plugin": {
  "api": 2,
  "facet": "plugin.facet",
  "lockMaterial": {
    "shader": "lib/glass.vws",
    "params": [{"key": "refraction", "default": 28}, {"key": "bezel", "default": 22}]
  }
}
```

The shader is a vw rectangle shader (see vw's shaders guide) with
`VW_USE_BACKDROP`, as a `.vws` built by `build.sh` (or GLSL, compiled at
runtime when glslc is installed). `shader_params` 0–2 are the store values
named in `params` (their defaults until set), and 3 is the item's opacity.
The rectangle's colour is the theme's backdrop colour, and its alpha is the
user's Background setting for that item, and its colour is the backdrop
colour the user picked (the item's, else **Settings → Lock screen → Backdrop
colour**, else the theme's). The first running plugin with a
material wins. Lock views get it as `state.lock.material` (`shader`,
`params`) to draw their own cards in it, and the picked colour as
`state.lock.backdropColour` (`[r, g, b]`, 0–1; absent for the theme's). The login screen runs no plugins.

```facet
view LockQuote(state, actions) {
    ui.Text(text: actions.get("quote", "Stay curious"), font: state.fonts.display,
            size: 24 * state.lock.scale, color: state.colours.tertiary)
}
```

```facet
let [ vw.ui as ui, vw.layout as layout ] in

view Bar(state, actions) {
    let tick = actions.refresh(1000)
    Native(name: Pill, color: state.colours.surfaceContainer,
           on_click: () => actions.set("clicks", actions.get("clicks", 0) + 1)) {
        Native(name: Ring, progress: (state.time.second / 60), size: 22, color: state.colours.primary)
        ui.Text(text: state.time.text, font: state.fonts.display, size: 14, color: state.colours.primary)
    }
}

view Settings(state, actions) {
    ui.Toggle(id: "seconds", value: actions.get("seconds", false),
              on_change: (on) => actions.set("seconds", on))
}
```

Every view gets `(state, actions)`:

| | |
|---|---|
| `state.store` | the plugin's store (below) |
| `state.backend` | what its process last sent as `{"type": "state", "state": …}` |
| `state.ready` | true once it has: read `state.backend` only then, since a missing field is an error in Facet |
| `state.colours` | the 21 colour roles, as colours |
| `state.fonts` | `text`, `display`, `icon`: the shell's fonts (Facet's own default is monospace) |
| `state.time` | `hour`, `minute`, `second`, `text` ("14:05") |
| `state.plugin` | `id`, `version` |
| `state.width` | Settings views only: the width they have |
| `state.screen` | Wallpaper views only: `width`, `height` |
| `state.workspace` | Wallpaper views only: `index` (from 0) and `count` |
| `state.lock` | Lock views only: `scale` (draw at it), `editing` (in the layout editor), `greeter`, `sheet` (0 down to 1 up) |
| `actions.get(key, fallback)` | a stored value, or the fallback |
| `actions.set(key, value)` | store a value |
| `actions.send(value)` | to its process, as `{"type": "action", "data": value}` |
| `actions.refresh(ms)` | draw again after `ms` (clocks, timers) |
| `actions.run("launcher toggle")` | a `blxshell` command; needs `shell` |
| `actions.notify(summary, body)` | a notification; needs `notifications` |
| `actions.spawn(["prog", "arg"])`, `actions.open(path)` | needs `spawn` |
| `actions.log(text)` | a line in the shell's log |

The shell adds three components, used as `Native(name: …)`:

| | |
|---|---|
| `Pill` | children in a row on a rounded background sized to them, like the bar's modules; takes `color`, `padding`, `gap`, `height`, `radius`, `on_click`, `on_right_click`, `on_middle_click`, `on_scroll` |
| `Icon` | a Material Symbols icon: `glyph`, `size`, `color` |
| `Ring` | the bar's progress ring: `progress` (0–1), `size`, `stroke`, `color` |
| `Parallax` | full-screen image `layers` (`[{path, depth, opacity}]`, back first) for Wallpaper views: each slides by `depth` × `strength` of the screen width as `shift` goes 0 to 1, gliding there over `glide` ms; `opacity` crossfades |

Edits to a plugin's `.facet` file show up within a second. Wrap arithmetic in
a named argument in parentheses (`progress: (a / 60)`): Facet's parser
currently reads only the first operand otherwise.

### The store

Every plugin has one store, a JSON object kept in
`~/.local/state/blxshell/plugins/<id>.json`. It holds its settings and
anything else it wants to keep. Views read it with `actions.get` and write it
with `actions.set`; its Settings view edits it from the settings app; its
process gets it in `hello` and again in a `store` message whenever it
changes, and can write to it with `set`. Defaults live in the code
(`actions.get("interval", 2)`), not in the manifest.

### Process protocol, api 2

One JSON object per line on the process's stdin (from the shell) and stdout
(to the shell); stderr goes to the shell's log. It runs from the package
folder with `BLXSHELL_PLUGIN_ID`, `BLXSHELL_PLUGIN_DIR` and
`BLXSHELL_PLUGIN_API` set, and should exit when its stdin closes.

**Shell → process**

| `type` | Fields | When |
|---|---|---|
| `hello` | `api`, `shell`, `compositor`, `plugin` {`id`, `version`, `dir`}, `store`, `colours` | first line after start |
| `store` | `store` | the store changed (a view, the settings app) |
| `action` | `data` | a view called `actions.send(data)` |
| `command` | `name`, `args` | `blxshell plugin call <id> <name> [args…]` |
| `colours` | `colours` | the scheme changed |
| `event` | `desktop`, `item`, `event: "action"`, `action` | the user picked an action on one of its desktop items |

**Process → shell**

| `type` | Fields | |
|---|---|---|
| `state` | `state` | data for its views, as `state.backend` |
| `set` | `key`, `value` | a value for its store |
| `desktop` | `id`, `items` | its desktop items (below) |
| `shell` | `command`: `["launcher", "toggle"]` or `"launcher toggle"` | needs `shell`; `plugin …` commands are refused |
| `notify` | `summary`, `body` | needs `notifications` |
| `log` | `message` | a line in the shell's log |

Lines over 64 KiB are dropped. If the process stops reading, messages to it
are dropped rather than the shell waiting. `restart` decides what happens when
it exits: `on-failure` (default) restarts it after a crash, `always` after any
exit, `never` doesn't; more than 5 restarts in a minute and it stays stopped
until `blxshell plugin restart <id>`.

### C++ plugins

```cpp
#include <blxshell/plugin.hpp>

class Clicks final : public blxshell::Plugin {
 public:
  std::vector<std::string> views() const override { return {"Bar"}; }
  vw::Widget view(vw::Ui& ui, std::string_view, blxshell::Host& host) override {
    const std::string stored = host.get("clicks");
    const int clicks = stored.empty() ? 0 : std::stoi(stored);
    return blxshell::pill(ui, host.colour("surfaceContainer"),
                          {.on_click = [&host, clicks](const vw::ClickEvent&) { host.set("clicks", std::to_string(clicks + 1)); }},
                          [&] { return ui.text({.text = std::to_string(clicks), .font = host.font("text"),
                                                .size = 13, .color = host.colour("text")}); });
  }
};
BLXSHELL_PLUGIN(Clicks)
```

`blxshell::Host` offers what Facet views get: `get`/`set` on the store (JSON
text), `backend()`, `send`, `refresh`, `colour`, `font`, `run`, `notify`,
`log`. vw draws in the order widgets are built, so build backgrounds first;
`blxshell::pill` does that for a background sized to its content.

The library must be built against the same blxshell version with the same
compiler: `BLXSHELL_PLUGIN_ABI` carries both, and blxshell refuses a library
whose tag differs ("rebuild it") rather than risk a crash. The manifest lists
it as `"native": "lib/plugin.so"` with the `native` permission. Native code
runs inside the shell with no limits, so the official repository only takes it
as source plus a `build.sh`, built by its CI. `packages/plugins/dev.blxshell.native`
is a complete example; blxshell's CMake builds it.

### Desktop items

A process sends `{"type": "desktop", "id": "files", "items": [...]}` to put
icons on the wallpaper (up to 8 areas, 500 items each):

```json
{ "id": "notes.txt", "label": "notes.txt", "icon": ["text-plain", "text-x-generic"],
  "image": "/abs/path/to/thumbnail.png", "glyph": "description",
  "actions": [{ "id": "open", "label": "Open", "glyph": "open_in_new" },
              { "id": "trash", "label": "Move to trash", "glyph": "delete", "danger": true }] }
```

The shell lays them out in a grid from the top left. A click opens a popup
with the item's actions; a double-click runs the first. Users can drag items to
another cell (dropping on an item swaps them); the cells are remembered in
`~/.local/state/blxshell/desktop.json`, keyed by area and item id, so keep ids
stable (a file name, not an index).

`"style": {"shape": "cookie9sided", "tint": "themed"}` beside `items` sets how
every icon is drawn; an item can carry its own `shape` and `tint`:

- `shape`: a Material shape the icon sits on, inset like an Android adaptive
  icon (a photo `image` fills it instead): `circle`, `square`, `slanted`,
  `arch`, `pill`, `diamond`, `gem`, `pentagon`, `sunny`, `verysunny`,
  `cookie4sided`, `cookie6sided`, `cookie7sided`, `cookie9sided`,
  `cookie12sided`, `clover4leaf`, `clover8leaf`, `softburst`, `flower`,
  `puffy`, `ghostish`, `bun`, `heart` and the rest of MaterialShapes.
  `"none"` or leaving it out draws the icon as it is. Changing it morphs.
  Facet views can draw one with `Native(name: Shape, shape:, size:, color:)`.
- `tint`: `"colour"` leans icons toward the accent, keeping their shading;
  `"themed"` makes them one colour, the accent on its container, like
  Android's themed icons. Leaving it out keeps their own colours.

### Permissions

A plugin's process runs as the user, like anything else they start.
**Permissions are not a sandbox** for processes or native code: they gate
what a plugin may ask *the shell* to do, and tell users and reviewers what it
does. Facet views are the exception: they can only reach what the shell hands
them, so for them the gates are real.

| Permission | Enforced | |
|---|---|---|
| `shell` | yes | run `blxshell` commands |
| `notifications` | yes | post notifications |
| `spawn` | for views | start programs (`actions.spawn`, `actions.open`) |
| `native` | yes | required for a `native` library |
| `network` | declared | talks to the network |
| `filesystem` | declared | reads or writes files outside its folder |

### Shell commands for plugins

| | |
|---|---|
| `blxshell plugin list` | installed plugins and whether they're on or can run here |
| `blxshell plugin enable\|disable <id>` | |
| `blxshell plugin status` | what the running shell has running: views, native, process |
| `blxshell plugin views` | every running view, as `<plugin id>:<view>` |
| `blxshell plugin restart <id>` | restarts it and picks up an update |
| `blxshell plugin call <id> <command> [args]` | sends its process a `command` message |

## Project ids

This package system can be reused by another shell without the two
ecosystems mixing. Every build has a project id, `blxshell` by default:

```sh
cmake -S . -B build -DBLXSHELL_PROJECT_ID=friendshell
```

- A package names its project in `"project"`. A build installs and loads
  only its own project's packages; others are refused with "it's a package
  for X, and this is Y". A manifest without the field is `blxshell`.
- A repository names its project at the top of `list.json`, and `build.py`
  copies it into `index.json` (and into each package entry). A build skips
  repositories for another project, and any package of another project in
  a mixed index. `validate.py` refuses a package whose project differs from
  its repository's.
- C++ plugins carry the project in their ABI tag
  (`blxshell-plugin-2/<project>/<version>/<compiler>`), so a library built
  for one project never loads in another.

Ids are lowercase letters, digits and `-`, starting with a letter.

## For the store

- Validate uploads with `schemas/manifest.schema.json`, then run
  `blxshell inspect file.blx`, which also checks archive safety and that the
  files the manifest names exist.
- `id` + `version` identify a release. Packages are reproducible, so a hash
  of the `.blx` identifies its contents.
- Filter listings by `compositors`, `blxshell` and `type`, and show
  `permissions` before install.
