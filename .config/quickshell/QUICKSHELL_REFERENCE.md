# Quickshell Reference Guide

This document contains reference information about Quickshell components and patterns for future development.

## Hyprland Integration

### Workspaces

#### HyprlandWorkspace Properties

Based on [official documentation](https://quickshell.org/docs/v0.2.1/types/Quickshell.Hyprland/HyprlandWorkspace/):

**Window/Toplevel Related:**
- `toplevels` - List of toplevels (windows) on this workspace (ObjectModel, readonly)
  - Use `workspace.toplevels.length` to check if workspace has windows
  - Example: `workspace.toplevels.length > 0` returns true if workspace has any windows

**Workspace State:**
- `focused` - Whether the workspace is active on a monitor that's currently focused (bool, readonly)
- `active` - Whether the workspace is currently active on its monitor (bool, readonly)
- `hasFullscreen` - If this workspace currently has a fullscreen client (bool, readonly)
- `urgent` - Indicates if any window requires attention; resets when workspace becomes focused (bool, readonly)

**Identification:**
- `name` - Workspace identifier (string, readonly)
- `id` - Numeric workspace identifier (int, readonly)

**Context:**
- `monitor` - Associated HyprlandMonitor object (readonly)
- `lastIpcObject` - Most recent JSON data from Hyprland; requires manual refresh for dynamic values (readonly)

#### HyprlandWorkspace Methods

- `activate()` - Switches to this workspace, equivalent to dispatching `workspace ${workspace.name}`

#### Accessing Active Workspace

```qml
Hyprland.focusedMonitor.activeWorkspace
Hyprland.focusedMonitor.activeWorkspace.id  // Get workspace ID (1-10)
```

### Toplevels (Windows)

#### HyprlandToplevel Properties

Based on [official documentation](https://quickshell.org/docs/v0.2.1/types/Quickshell.Hyprland/HyprlandToplevel/):

**Core Properties:**
- `address` (string, readonly) - Hexadecimal Hyprland window address. Will be an empty string until the address is reported
- `title` (string, readonly) - The window title
- `activated` (bool, readonly) - Whether the window is currently active
- `urgent` (bool, readonly) - Whether the client requires attention

**Workspace-Related Properties:**
- `workspace` (HyprlandWorkspace, readonly) - The current workspace of the toplevel (might be null)
- `monitor` (HyprlandMonitor, readonly) - The current monitor of the toplevel (might be null)

**Integration Properties:**
- `wayland` (Toplevel, readonly) - The wayland toplevel handle. Will be null until the address is reported
- `handle` (HyprlandToplevel, readonly) - The toplevel handle, exposing the Hyprland toplevel. Will be null until the address is reported
- `lastIpcObject` (unknown, readonly) - The last JSON response from Hyprland for this window

### Common Patterns

#### Check if workspace has windows

```qml
Repeater {
    model: Hyprland.workspaces
    Rectangle {
        property var workspace: modelData
        property bool hasToplevels: workspace.toplevels.length > 0
        visible: hasToplevels
    }
}
```

#### Switch to workspace (static workspaces)

```qml
MouseArea {
    onClicked: {
        Hyprland.dispatch("workspace " + (index + 1))
    }
}
```

#### Switch to workspace (dynamic workspaces)

```qml
MouseArea {
    onClicked: {
        modelData.activate()  // modelData is the workspace
    }
}
```

#### Iterate through all workspaces

```qml
Repeater {
    model: Hyprland.workspaces
    // modelData contains the HyprlandWorkspace object
}
```

## UI Patterns

### Hover Effects

```qml
Rectangle {
    Rectangle {
        id: hoverIndicator
        anchors.fill: parent
        opacity: 0
        Behavior on opacity {
            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onEntered: hoverIndicator.opacity = 0.3
        onExited: hoverIndicator.opacity = 0
    }
}
```

### Smooth Position Animations

```qml
Rectangle {
    x: someValue
    Behavior on x {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }
}
```

## Font Loading

### Using Custom Fonts

```qml
// In shell.qml or component file
FontLoader {
    id: rubikFont
    source: "qrc:/fonts/Rubik-Regular.ttf"
}

// Usage
Text {
    font.family: rubikFont.name
    font.weight: 800
    font.pixelSize: 15
}
```

## Color Scheme Access

Access Material Design 3 colors via the `col` object:

```qml
color: col.primary
color: col.onPrimary
color: col.secondary
color: col.onSecondary
color: col.background
color: col.foreground
color: col.surfaceContainerHighest
// ... and more (see Colors.json)
```

## Z-Index Layering

When overlaying multiple elements, use z-index to control stacking order:

```qml
Rectangle {
    // Bottom layer
    z: 1
}

Rectangle {
    // Middle layer
    z: 2
}

Rectangle {
    // Top layer
    z: 3
}
```

Higher z values appear on top.

## PipeWire Integration

Based on [official documentation](https://quickshell.org/docs/v0.2.1/types/Quickshell.Services.Pipewire/)

Import: `import Quickshell.Services.Pipewire`

### Pipewire Service

The main Pipewire singleton object provides access to all PipeWire audio infrastructure.

#### Pipewire Properties

**Default Devices (readonly):**
- `defaultAudioSource` (PwNode) - The default audio source (input) currently in use by pipewire
- `defaultAudioSink` (PwNode) - The default sink currently in use by pipewire, and the one applications are currently using

**Collections (readonly):**
- `nodes` (ObjectModel<PwNode>) - All system nodes, filterable by audio status and stream type
- `links` (ObjectModel<PwLink>) - All connections between pipewire nodes
- `linkGroups` (ObjectModel<PwLinkGroup>) - Deduplicated link connections

**Status (readonly):**
- `ready` (bool) - Indicates if quickshell has synchronized with the pipewire server

**Preferences (read/write):**
- `preferredDefaultAudioSource` (PwNode) - A hint to pipewire telling it which source should be the default when possible
- `preferredDefaultAudioSink` (PwNode) - A hint to pipewire telling it which sink should be the default when possible

### PwNode

Represents a node in the pipewire connection graph (audio devices or streams).

#### PwNode Properties

**Basic Information (readonly):**
- `id` (int) - The pipewire object id of the node
- `name` (string) - Corresponds to `node.name` property
- `nickname` (string) - Corresponds to `node.nickname` property
- `description` (string) - Corresponds to `node.description` property
- `type` (unknown) - Reflects Pipewire's media.class

**State (readonly):**
- `ready` (bool) - Indicates if the node is fully bound and operational
- `isSink` (bool) - Determines if the node accepts input (true) or outputs audio (false)
- `isStream` (bool) - Identifies program nodes (true) versus hardware devices (false)

**Audio (readonly):**
- `audio` (PwNodeAudio) - Extra information present only if the node sends or receives audio
- `properties` (object) - Key-value pairs including potential fields like:
  - `application.name`
  - `application.icon-name`
  - `media.name`
  - `media.title`
  - `media.artist`

### PwLink

Represents a connection between pipewire nodes (individual audio channel).

#### PwLink Properties (all readonly)

- `id` (int) - The pipewire object id of the link (useful for debugging with `pw-cli i <id>`)
- `source` (PwNode) - The originating node transmitting audio information
- `target` (PwNode) - The destination node receiving audio information
- `state` (PwLinkState) - Current link status (requires PwObjectTracker binding to function properly)

**Note:** Each link represents one channel of a multi-channel connection. For managing complete connections across all channels, use `PwLinkGroup` instead.

### Common Patterns

#### Access default audio sink/source

```qml
import Quickshell.Services.Pipewire

Text {
    text: "Default Sink: " + Pipewire.defaultAudioSink.description
}

Text {
    text: "Default Source: " + Pipewire.defaultAudioSource.description
}
```

#### List all audio nodes

```qml
import Quickshell.Services.Pipewire

Repeater {
    model: Pipewire.nodes
    Text {
        text: modelData.description + " (Sink: " + modelData.isSink + ")"
    }
}
```

#### Filter audio sinks only

```qml
import Quickshell.Services.Pipewire

Repeater {
    model: Pipewire.nodes
    Text {
        visible: modelData.isSink && !modelData.isStream
        text: modelData.description
    }
}
```

#### Set preferred default sink

```qml
import Quickshell.Services.Pipewire

MouseArea {
    onClicked: {
        Pipewire.preferredDefaultAudioSink = someNode
    }
}
```

## Windows & Popups

Based on [PopupWindow documentation](https://quickshell.org/docs/v0.2.1/types/Quickshell/PopupWindow/) and [HyprlandFocusGrab documentation](https://quickshell.org/docs/v0.2.1/types/Quickshell.Hyprland/HyprlandFocusGrab/)

### PopupWindow

Used to create floating windows/sidebars that can be anchored to positions.

#### PopupWindow Properties

- `anchor` (PopupAnchor, readonly) - The popup's anchor/positioner relative to another item or window
  - Configure via `anchor.window` and `anchor.rect.x/y`
- `visible` (bool) - Controls whether the window displays or hides (defaults to false)
- `screen` (ShellScreen, readonly) - Which screen the window currently occupies

**Note:** The popup requires both a valid anchor and `visible: true` to display.

### HyprlandFocusGrab

Manages exclusive input focus for windows using the `hyprland_focus_grab_v1` Wayland protocol. Essential for creating click-outside-to-close functionality.

#### HyprlandFocusGrab Properties

- `active` (bool) - Controls whether the focus grab is enabled (defaults to false)
  - Changes to false when dismissed by the compositor
  - Requires at least one visible window to become true
- `windows` (list<QtObject>) - List of windows that should receive exclusive input focus

#### HyprlandFocusGrab Signals

- `cleared()` - Emitted when the compositor clears the focus grab (user clicked outside, window hidden, etc.)

#### How It Works

When active, listed windows:
- Receive input normally
- Retain keyboard focus even if the mouse moves off them
- Automatically deactivate when users click or touch areas outside the specified windows

### Common Patterns

#### Create sidebar with click-outside-to-close

```qml
import Quickshell
import Quickshell.Hyprland

PopupWindow {
    id: sidebar
    visible: false

    // Anchor to screen edge
    anchor.window: barWindow
    anchor.rect.x: 0
    anchor.rect.y: 0

    Rectangle {
        width: 300
        height: 600
        color: col.background

        // Sidebar content here
    }

    HyprlandFocusGrab {
        id: focusGrab
        windows: [sidebar]
        active: sidebar.visible

        onCleared: {
            // User clicked outside - close the sidebar
            sidebar.visible = false
        }
    }
}
```

#### Toggle sidebar visibility

```qml
MouseArea {
    onClicked: {
        sidebar.visible = !sidebar.visible
    }
}
```

#### Multi-window focus grab

```qml
HyprlandFocusGrab {
    windows: [popup1, popup2, popup3]
    active: popup1.visible || popup2.visible || popup3.visible

    onCleared: {
        popup1.visible = false
        popup2.visible = false
        popup3.visible = false
    }
}
```
