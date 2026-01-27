import Quickshell
import Quickshell.Io
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
	Launcher {}
	Notifications {}
	Settings {}

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
			property string matugenScheme: "scheme-tonal-spot"
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
			property bool wallpaperStartupZoom: true
			property real wallpaperStartupZoomScale: 1.4
			property int wallpaperStartupZoomDuration: 1200
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
