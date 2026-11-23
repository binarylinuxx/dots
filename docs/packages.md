# Themes and plugins

Themes and plugins ship as **`.blx` packages**: a tar archive compressed with
zstd, with `manifest.json` at its root. The manifest's `"type"` says whether
it is a `"theme"` or a `"plugin"`.

```
my-theme-1.0.0.blx            (tar.zst)
├── manifest.json             required
├── preview.png               optional, listed in "previews"
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

Prebuilt packages come from repositories. The official one,
[binarylinuxx/dots-plugins](https://github.com/binarylinuxx/dots-plugins), is
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
| `blxshell` | | version range: `">=0.1.0"`, `">=0.1.0 <0.3.0"`, `"^0.1.0"`, `"~0.1.2"`, `"*"`, groups joined by `\|\|` |
| `description`, `license` (SPDX), `homepage`, `repository` | | |
| `authors` | | `["Name"]` or `[{"name", "email", "url"}]` |
| `tags` | | up to 12, for store search |
| `icon`, `previews` | | image paths inside the package |
| `colours` | themes | below |
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

### Roles

`primary`, `primaryText`, `primaryContainer`, `secondary`,
`secondaryContainer`, `tertiary`, `tertiaryContainer`, `error`, `errorText`,
`errorContainer`, `errorContainerText`, `surface`, `surfaceContainer`,
`surfaceContainerLow`, `surfaceContainerHigh`, `surfaceContainerHighest`,
`surfaceVariant`, `text`, `textMuted`, `outline`, `outlineVariant`.
`…Text` roles are the colour of text on that role.

## Plugins

A plugin is a program in any language, run by the shell as a child process.
It talks to the shell in **JSON lines**: one JSON object per line on its
stdin (from the shell) and stdout (to the shell). stderr goes to the
shell's log.

```json
{
  "schema": 1,
  "type": "plugin",
  "id": "io.github.you.cpu",
  "name": "CPU",
  "version": "1.0.0",
  "compositors": ["any"],
  "plugin": {
    "api": 1,
    "exec": ["bin/cpu.py", "--fast"],
    "restart": "on-failure",
    "permissions": ["shell"],
    "barModules": [{ "id": "usage", "name": "CPU usage" }],
    "commands": ["refresh"],
    "settings": [
      { "key": "interval", "type": "number", "label": "Update every (s)", "default": 2 },
      { "key": "unit", "type": "choice", "choices": ["percent", "load"], "default": "percent" }
    ]
  }
}
```

| `plugin` key | |
|---|---|
| `api` | protocol version, `1` |
| `exec` | program inside the package, then its arguments. It runs from the package folder, in its own process group, with `BLXSHELL_PLUGIN_ID`, `BLXSHELL_PLUGIN_DIR` and `BLXSHELL_PLUGIN_API` set. Mark it executable; give scripts a `#!` line. |
| `restart` | `"on-failure"` (default), `"always"` or `"never"`. More than 5 restarts in a minute and it stays stopped until `blxshell plugin restart <id>`. |
| `permissions` | below |
| `barModules` | modules it fills; users place them in the bar as `"<plugin id>:<module id>"` |
| `desktop` | desktop areas it fills with icons on the wallpaper: `[{ "id", "name", "description" }]` |
| `commands` | names `blxshell plugin call <id> <name> [args]` accepts |
| `settings` | `key`, `type` (`bool`, `number`, `string`, `choice`), `label`, `description`, `default`, `choices` |

Plugins are installed off. `blxshell plugin enable <id>` adds the id to
`plugins.enabled` in config.json, and the running shell starts it. User
values for settings go in `plugins.settings.<id>` in config.json:

```json
"plugins": {
  "enabled": ["io.github.you.cpu"],
  "settings": { "io.github.you.cpu": { "interval": 5 } }
}
```

### Desktop items

The shell lays a plugin's desktop items out on the wallpaper in a grid from
the top left. A click opens a popup with the item's actions; a double-click
runs its first. Users can drag items to another cell (dropping on an item
swaps them); the cells are remembered in `~/.local/state/blxshell/desktop.json`,
keyed by area and item id, so keep ids stable (a file name, not an index).

### Permissions

A plugin runs as the user, like anything else they start. **Permissions
are not a sandbox.** They do two things: gate what the plugin may ask *the
shell* to do, and tell users and store reviewers what it does.

| Permission | Enforced | |
|---|---|---|
| `shell` | yes | may send `shell` messages (run shell commands) |
| `notifications` | yes | may send `notify` messages |
| `network` | declared | talks to the network |
| `filesystem` | declared | reads or writes files outside its folder |
| `spawn` | declared | starts other programs |

### Protocol, api 1

Every message is one line holding one JSON object with a `"type"`. Key
order is not fixed, so don't match on it. Unknown types and keys are
ignored, so later versions can add to either side.

**Shell → plugin**

| `type` | Fields | When |
|---|---|---|
| `hello` | `api`, `shell` (version), `compositor`, `plugin` {`id`, `version`, `dir`}, `modules` (bar module ids), `settings` (defaults with the user's values on top), `colours` (role → `#rrggbb`) | first line after start |
| `settings` | `settings` | the user changed them |
| `colours` | `colours` | the scheme changed |
| `event` | `module`, `event`: `"click"` with `button` (`left`, `right`, `middle`), or `"scroll"` with `delta` | pointer on one of its bar modules |
| `event` | `desktop` (area id), `item`, `event`: `"action"`, `action` | the user picked an action on one of its desktop items (a double-click picks the first) |
| `command` | `name`, `args` (strings) | `blxshell plugin call <id> <name> [args…]` |

When its stdin closes, the plugin should exit. On disable or shutdown the
shell closes it and sends SIGTERM to the plugin's process group.

**Plugin → shell**

| `type` | Fields | |
|---|---|---|
| `module` | `id`, and any of `text`, `icon` (a Material Symbols name), `tone` (a colour role, `""` for the default), `progress` (0–1 draws a ring around the icon; `null` removes it), `visible` | updates a bar module. Fields left out keep their last value. `id` must be in `barModules`. |
| `desktop` | `id`, `items`: `[{ "id", "label", "icon", "image", "glyph", "actions": [{ "id", "label", "glyph", "danger" }] }]` | replaces a desktop area's items (up to 500). `icon` is a theme icon name or a list tried in order; `image` an absolute path shown as a thumbnail; `glyph` the Material Symbols fallback; `danger` draws an action in the error colour. `id` must be in `desktop`. |
| `shell` | `command`: `["launcher", "toggle"]` or `"launcher toggle"` | runs a `blxshell` command (see `blxshell help`). Needs `shell`. `plugin …` commands are refused. |
| `notify` | `summary`, `body` | desktop notification. Needs `notifications`. |
| `log` | `message` | a line in the shell's log |

Lines over 64 KiB are dropped. If the plugin stops reading, messages to it
are dropped rather than the shell waiting.

A minimal plugin in Python:

```python
#!/usr/bin/env python3
import json, sys, threading, time

def send(**message):
    print(json.dumps(message), flush=True)

def tick():
    n = 0
    while True:
        n += 1
        send(type="module", id="counter", icon="timer", text=str(n))
        time.sleep(1)

for line in sys.stdin:            # ends when the shell closes stdin
    message = json.loads(line)
    if message["type"] == "hello":
        threading.Thread(target=tick, daemon=True).start()
    elif message["type"] == "event" and message.get("button") == "right":
        send(type="shell", command=["sidebar", "both"])   # needs "shell"
```

`packages/plugins/dev.blxshell.cpu` is a complete example in POSIX sh.

### Shell commands for plugins

| | |
|---|---|
| `blxshell plugin list` | installed plugins and whether they're enabled or can run here |
| `blxshell plugin enable\|disable <id>` | |
| `blxshell plugin status` | what the running shell has running, with pids |
| `blxshell plugin restart <id>` | restarts it and picks up an update |
| `blxshell plugin call <id> <command> [args]` | sends it a `command` message |

## For the store

- Validate uploads with `schemas/manifest.schema.json`, then run
  `blxshell inspect file.blx`, which also checks archive safety and that the
  files the manifest names exist.
- `id` + `version` identify a release. Packages are reproducible, so a hash
  of the `.blx` identifies its contents.
- Filter listings by `compositors`, `blxshell` and `type`, and show
  `permissions` before install.
