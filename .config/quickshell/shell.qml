import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import QtQuick.Controls
import QtQuick
import qs.bar
import qs.widgets
import qs.modules
import qs.launcher
import qs.notifications

ShellRoot {
	Bar {}
	AudioOsd {}
	Background {}
	BackgroundClock {
		id: backgroundGrid
	}
	HotCornerTrigger {
		onTriggered: backgroundGrid.toggleEditMode()
	}
	Launcher {}
	Notifications {}
	Settings {}

	// ── Lockscreen ──
	LockContext {
		id: lockContext

		onUnlocked: {
			lock.locked = false;
			lockContext.currentText = "";
		}
	}

	WlSessionLock {
		id: lock
		locked: false

		WlSessionLockSurface {
			LockSurface {
				anchors.fill: parent
				context: lockContext
			}
		}
	}

	// IPC handler: `qs ipc call lockscreen lock`
	IpcHandler {
		target: "lockscreen"

		function lock(): void {
			lock.locked = true;
		}

		function unlock(): void {
			lock.locked = false;
		}

		function isLocked(): bool {
			return lock.locked;
		}
	}

	// ── Power Menu ──
	PowerMenu {
		id: powerMenu
	}

	// IPC handler: `qs ipc call powermenu toggle`
	IpcHandler {
		target: "powermenu"

		function toggle(): void {
			powerMenu.showing = !powerMenu.showing;
		}

		function show(): void {
			powerMenu.showing = true;
		}

		function hide(): void {
			powerMenu.showing = false;
		}
	}

	// IPC handler: `qs ipc call grid toggle`
	IpcHandler {
		target: "grid"

		function toggle(): void {
			backgroundGrid.toggleEditMode();
		}

		function enable(): void {
			backgroundGrid.setEditMode(true);
		}

		function disable(): void {
			backgroundGrid.setEditMode(false);
		}

		function isEditMode(): bool {
			return backgroundGrid.editMode;
		}
	}

	// Config file for settings
	FileView {
		id: configWatcher
		path: Qt.resolvedUrl("./config.json")
		watchChanges: true
		onFileChanged: reload()

		JsonAdapter {
			id: cfg
			property bool barFloating: false
			property bool barOnTop: true
			property int barHeight: 35
			property int barRadius: 20
			property int barGap: 5
			property bool screenCorners: true
			property int screenCornerSize: 25
			property string matugenMode: "dark"
			property string matugenScheme: "tonal-spot"
			property real matugenContrast: 0.0
			property int workspaceCount: 10
			property bool dynamicWorkspaces: false
			property string workspaceStyle: "dots"
			property bool showSystemTray: true
			property string clockFormat: "hh:mm AP"
			property string clockPreset: "time12"
			property string fontFamily: "Rubik"
			property int fontSize: 14
			property string animationSpeed: "normal"
			// Wallpaper effects
			property bool wallpaperParallax: true
			property real wallpaperParallaxStrength: 0.1
			property int wallpaperTransitionDuration: 600
			// Launcher settings
			property string launcherPreset: "default"
			property int launcherWidth: 400
			property int launcherItemHeight: 55
			property int launcherRadius: 25
			property int launcherMaxItems: 8
			property bool launcherShowIcons: true
			property bool launcherShowDescriptions: true
			property bool launcherSearchAtTop: true
			property bool launcherEmojiMode: true
			property bool launcherClipboardMode: true
			property bool launcherWallpaperMode: true
			property int launcherHeight: 510
			// Advanced / Desktop Widgets
			property bool desktopWidgets: true
			property int gridColumns: 16
			property int gridRows: 9
			property int widgetRadius: 12
			property int widgetBorderWidth: 1
			property string widgetBorderColor: ""
			property string widgetBackgroundColor: ""
			property real widgetOpacity: 0.85
		}
	}

	// Using matugen generated colors
	FileView {
	    id: colorWatcher
	    path: Qt.resolvedUrl("./Colors.json")
	    watchChanges: true
	    onFileChanged: reload()

		// Mapping Colors
	    JsonAdapter {
	        id: col // Short variation for 'color'
	        property string background
	        property string foreground
	        property string primary
	        property string primaryFixed
	        property string primaryFixedDim
	        property string onPrimary
	        property string onPrimaryFixed
	        property string onPrimaryFixedVariant
	        property string primaryContainer
	        property string onPrimaryContainer
	        property string secondary
	        property string secondaryFixed
	        property string secondaryFixedDim
	        property string onSecondary
	        property string onSecondaryFixed
	        property string onSecondaryFixedVariant
	        property string secondaryContainer
	        property string onSecondaryContainer
	        property string tertiary
	        property string tertiaryFixed
	        property string tertiaryFixedDim
	        property string onTertiary
	        property string onTertiaryFixed
	        property string onTertiaryFixedVariant
	        property string tertiaryContainer
	        property string onTertiaryContainer
	        property string error
	        property string onError
	        property string errorContainer
	        property string onErrorContainer
	        property string surface
	        property string onSurface
	        property string onSurfaceVariant
	        property string outline
	        property string outlineVariant
	        property string shadow
	        property string scrim
	        property string inverseSurface
	        property string inverseOnSurface
	        property string inversePrimary
	        property string surfaceDim
	        property string surfaceBright
	        property string surfaceContainerLowest
	        property string surfaceContainerLow
	        property string surfaceContainer
	        property string surfaceContainerHigh
	        property string surfaceContainerHighest
	        property string wallpaper
	    }
	}
}
