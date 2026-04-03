import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import QtQuick.Controls
import QtQuick
import qs.bar
import qs.widgets
import qs.modules
import qs.modules as Modules
import qs.launcher
import qs.notifications
import qs.services

ShellRoot {
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
			property string barPosition: "bottom"  // top / bottom / left / right
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
			property bool dndEnabled: false
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
		// Weather settings
		property bool weatherUseApiProvider: false
		property string weatherProvider: "wttr"
		property string weatherCity: ""
		property string weatherApiKey: ""
		// Screen recorder settings
		property string recordingDir: ""       // empty = ~/Videos
		property string recordingMonitor: "HDMI-A-1"
		property int    recordingFps: 60
		property string recordingQuality: "very_high"  // very_high|high|medium|low
		property string recordingCodec: "av1"          // h264|hevc|av1
		property string recordingAudio: "default_output"
			// Sidebar
			property int sidebarTopPadding: 120*2
		// Night Light
		property bool nightLightEnabled: false
		property real nightLightTemperature: 0.6
		property real nightLightStrength: 0.45
		// Idle
		property bool idleEnabled: true
		property int  idleTimeout: 300
		property bool idleInhibitRecording: true
		property bool idleDpmsEnabled: true
		property int  idleDpmsDelay: 300
		property bool idleSuspendEnabled: true
		property int  idleSuspendDelay: 600
		// Wallhaven
		property string wallhavenApiKey: ""
		property string wallhavenPurityMode: "sfw"
		property string pexelsApiKey: ""
	}
}

	// Using matugen generated colors
	FileView {
	    id: colorWatcher
	    path: Qt.resolvedUrl("./Colors.json")
	    watchChanges: true
	    onFileChanged: reload()

	    JsonAdapter {
	        id: col
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

	// Sync dndEnabled between cfg and Gstate (cfg is not accessible in Singletons)
	Binding { target: Gstate; property: "dndEnabled"; value: cfg.dndEnabled }
	Connections {
		target: Gstate
		function onDndEnabledChanged() { cfg.dndEnabled = Gstate.dndEnabled }
	}

	// Sync animDuration from cfg.animationSpeed into Gstate
	Binding {
		target: Gstate
		property: "animDuration"
		value: {
			const s = cfg ? cfg.animationSpeed : "normal"
			if (s === "disabled") return 0
			if (s === "fast")     return 130
			if (s === "slow")     return 500
			return 250  // normal
		}
	}

	// Sync nightLightEnabled between cfg and Gstate
	Binding { target: Gstate; property: "nightLightEnabled"; value: cfg.nightLightEnabled }
	Connections {
		target: Gstate
		function onNightLightEnabledChanged() {
			cfg.nightLightEnabled = Gstate.nightLightEnabled
			configWatcher.writeAdapter()
		}
	}

	// Sync recording config into ScreenRecorder singleton (cfg not accessible inside singletons)
	Binding { target: ScreenRecorder; property: "monitor";     value: cfg.recordingMonitor }
	Binding { target: ScreenRecorder; property: "fps";         value: cfg.recordingFps }
	Binding { target: ScreenRecorder; property: "quality";     value: cfg.recordingQuality }
	Binding { target: ScreenRecorder; property: "codec";       value: cfg.recordingCodec }
	Binding { target: ScreenRecorder; property: "audioDevice"; value: cfg.recordingAudio }
	Binding { target: ScreenRecorder; property: "outputDir";   value: cfg.recordingDir }

	// ── Idle tracking ──────────────────────────────────────────────────────
	Binding { target: IdleService; property: "enabled";              value: cfg.idleEnabled }
	Binding { target: IdleService; property: "timeout";              value: cfg.idleTimeout }
	Binding { target: IdleService; property: "inhibitWhenRecording"; value: cfg.idleInhibitRecording }
	Binding { target: IdleService; property: "dpmsEnabled";          value: cfg.idleDpmsEnabled }
	Binding { target: IdleService; property: "dpmsDelay";            value: cfg.idleDpmsDelay }
	Binding { target: IdleService; property: "suspendEnabled";       value: cfg.idleSuspendEnabled }
	Binding { target: IdleService; property: "suspendDelay";         value: cfg.idleSuspendDelay }


	// Re-apply IdleMonitor.enabled after compositor seat is ready
	Timer {
		id: enableRetryTimer
		interval: 2000
		repeat: false
		onTriggered: {
			idleMonitor.enabled = false
			idleMonitor.enabled = cfg.idleEnabled
			console.log("[IdleMonitor] re-applied enabled:", idleMonitor.enabled)
		}
	}

	IdleMonitor {
		id: idleMonitor
		timeout: cfg.idleTimeout > 0 ? cfg.idleTimeout : 300
		// NOTE: remove hardcode after confirming works
		enabled: cfg.idleEnabled === true
		respectInhibitors: true
		onIsIdleChanged: {
			console.log("[IdleMonitor] isIdle changed:", isIdle, "— timeout was:", timeout)
			Gstate.idle = isIdle
			if (isIdle) IdleService.onIdle()
			else        IdleService.onResume()
		}
		onEnabledChanged: console.log("[IdleMonitor] enabled changed to:", enabled)
		onTimeoutChanged: console.log("[IdleMonitor] timeout changed to:", timeout)
		Component.onCompleted: {
			console.log("[IdleMonitor] cfg.idleEnabled =", cfg.idleEnabled, "type =", typeof cfg.idleEnabled)
			// Force re-apply enabled after short delay in case protocol seat isn't ready
			enableRetryTimer.start()
		}
	}

	// Handle IdleService signals — executed here where lock/Process are in scope
	Connections {
		target: IdleService

		function onRequestLock() {
			console.log("[shell] Locking screen")
			IdleService._locked = true
			lock.locked = true
		}
		function onRequestDpmsOff() {
			idleDpmsOffProcess.running = true
		}
		function onRequestDpmsOn() {
			idleDpmsOnProcess.running = true
		}
		function onRequestSuspend() {
			idleSuspendProcess.running = true
		}
	}

	// Track lock state directly in IdleService (no Binding lag)
	Connections {
		target: lock
		function onLockedChanged() {
			IdleService._locked = lock.locked
			console.log("[shell] lock.locked →", lock.locked)
			if (!lock.locked) {
				IdleService.onUnlocked()
			}
		}
	}

	Process {
		id: idleDpmsOffProcess
		command: ["hyprctl", "dispatch", "dpms", "off"]
		onRunningChanged: if (!running) console.log("[shell] DPMS off done")
	}
	Process {
		id: idleDpmsOnProcess
		command: ["hyprctl", "dispatch", "dpms", "on"]
		onRunningChanged: if (!running) console.log("[shell] DPMS on done")
	}
	Process {
		id: idleSuspendProcess
		command: ["systemctl", "suspend"]
		onRunningChanged: if (!running) console.log("[shell] Suspend done")
	}

	// Inhibit idle while recording (if enabled in settings)
	IdleInhibitor {
		enabled: cfg.idleInhibitRecording && ScreenRecorder.recording
		window: barWindow
	}

	Bar {
		id: barWindow
	}
	Sidebar {}
	NightLight {}
	AudioOsd {}
	Background {
		id: background
	}
	BackgroundClock {
		id: backgroundGrid
	}
	
	HotCornerTrigger {
		onTriggered: backgroundGrid.toggleEditMode()
	}
	
	Launcher {}
	Notifications {}
	Settings {}
	WeatherDetail {}
	FirstLaunch {}

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

}
