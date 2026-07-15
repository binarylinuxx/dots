# qs.PluginManager

Tiny C++ QML module. Only job: parse `plugin.json` manifests from a
plugins directory and expose them to QML. Everything else (loading
components, reading/writing per-plugin config, watching files) stays
in QML/Quickshell.

## Build

```sh
./build.sh
```

Then launch Quickshell with:

```sh
QML_IMPORT_PATH=$PWD/plugin_manager/build/qml quickshell -p shell.qml
```

## Use from QML

```qml
import qs.PluginManager

// Singleton. Defaults to BLXSHELL_PLUGIN_DIRS, then user data/runtime plugin dirs.
Component.onCompleted: {
    console.log(PluginRegistry.count)

    const bars = PluginRegistry.byKind("bar")
    // [{ id, name, icon, kindData: { component, componentUrl, ... }, ... }]

    const plugin = PluginRegistry.get("hello")
}
```

Use as a model:

```qml
Repeater {
    model: PluginRegistry
    delegate: Loader {
        source: model.provides.bar ? model.provides.bar.componentUrl : ""
    }
}
```

Or filtered:

```qml
Repeater {
    model: PluginRegistry.byKind("bar")  // returns array
    delegate: Loader { source: modelData.kindData.componentUrl }
}
```

## Manifest format

`plugins/<id>/plugin.json`:

```json
{
  "id": "hello",
  "name": "Hello World",
  "version": "1.0",
  "author": "blx",
  "description": "...",
  "icon": "waving_hand",
  "provides": {
    "bar":        { "component": "BarItem.qml", "position": "right", "order": 10 },
    "background": "Widget.qml",
    "settings":   "SettingsPage.qml"
  },
  "config": {
    "schema": [
      { "key": "refreshSec", "type": "int", "default": 60, "min": 5 }
    ]
  }
}
```

`component` (string shorthand) is equivalent to `{ "component": "..." }`.
Every component path is resolved to an absolute `componentUrl` (a
`file://` URL) alongside the original field.

## Shell Extension Points

The shell loads these `provides` keys when present:

| Key | Component placement and API |
|-----|-----------------------------|
| `bar` | Right side of a horizontal bar. Receives `plugin` when declared. |
| `background` | A desktop-widget grid cell. Receives `plugin` when declared. |
| `wallpaper-transition` | Full-screen wallpaper replacement while wallpaper changes. Declare `property var transition`; it receives `{ oldSource, newSource, oldWallpaper, newWallpaper, duration }`. `oldSource` and `newSource` are preloaded full-screen texture providers that already include the built-in zoom and parallax crop. It also receives `plugin` when declared. Emit `finished()` after presenting the final frame; a host timeout is only a fallback. The built-in slide is disabled while this component is active. |
| `lockscreen` | Full-screen layer behind the unlock card. Declare `property var lockContext` to inspect the lock state, and `property var plugin` for manifest data. |
| `settings` | Embedded on the plugin's card in the Settings app. Receives `plugin` when declared. Its `implicitHeight` controls the expanded page height. |

Set `wallpaperTransitionPlugin` in `~/.config/blxshell/config.json` to a
plugin ID to select it. Leave it empty to use the first enabled transition
plugin. Bar, background, and lockscreen plugins can be loaded together.
Disabled plugin IDs in
`~/.config/blxshell/config.json` apply to every extension point.

## API

| Member                         | Notes |
|--------------------------------|-------|
| `pluginsDir` (string, rw)      | Colon-separated plugin dirs; setting this triggers a rescan |
| `pluginDirs` (list<string>, rw) | Plugin directories scanned in order |
| `count` (int, ro)              | Number of parsed manifests |
| `rescan()`                     | Reparse everything |
| `get(id) -> map`               | One plugin as a map |
| `byKind(kind) -> list<map>`    | All plugins providing `kind`, with `kindData` injected |
| Model roles                    | `id, name, version, author, description, icon, path, provides, providesKinds, configSchema, settingsUrl, valid, error` |

## What this does NOT do

- No dlopen of .so plugins. QML-only plugins.
- No file watching. Call `PluginRegistry.rescan()` from QML on demand.
- No config read/write. Each plugin uses its own `FileView { path: "config.json" }`.
- No component loading. Use `Loader { source: componentUrl }` in QML.
- No validation of `provides` kinds. Any key is accepted; the host decides what it consumes.
