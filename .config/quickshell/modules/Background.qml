import Quickshell 
import Quickshell.Wayland
import qs.modules
import qs.services
import QtQuick
import PluginManager

PanelWindow {
	id: bgWindow
	WlrLayershell.layer: WlrLayer.Background
	exclusionMode: ExclusionMode.Ignore
	WlrLayershell.namespace: "quickshell:background"
	anchors {
		top: true
		bottom: true
		left: true
		right: true
	}
	color: "transparent"

	// Config-driven properties with fallbacks
	property bool enableParallax: cfg ? cfg.wallpaperParallax : true
	property real parallaxStrength: enableParallax ? (cfg ? cfg.wallpaperParallaxStrength : 0.1) : 0.0
	property int transitionDuration: cfg ? cfg.wallpaperTransitionDuration : 600

	// Parallax configuration
	property int totalWorkspaces: cfg ? cfg.workspaceCount : 10

	// Wallpaper transition configuration
	property bool isTransitioning: false
	property bool pluginTransitionPending: false
	property bool finishTransitionPending: false
	property var transitionPlugin: {
		var _rescan = PluginRegistry.count
		var disabled = cfg && cfg.disabledPlugins ? cfg.disabledPlugins : []
		var plugins = PluginRegistry.byKind("wallpaper-transition")
		var selected = cfg ? cfg.wallpaperTransitionPlugin : ""
		for (var i = 0; i < plugins.length; i++) {
			if (plugins[i].id === selected && disabled.indexOf(plugins[i].id) === -1)
				return plugins[i]
		}
		if (selected !== "") return null
		for (var j = 0; j < plugins.length; j++) {
			if (disabled.indexOf(plugins[j].id) === -1)
				return plugins[j]
		}
		return null
	}

	// Persist previousWallpaper across config reloads
	PersistentProperties {
		id: persist
		reloadableId: "backgroundWallpaper"
		property string lastWallpaper: ""
	}

	// React when col.wallpaper binding delivers a value (after FileView loads)
	property string currentWallpaper: col.wallpaper
	onCurrentWallpaperChanged: {
		if (currentWallpaper === "") return

		if (persist.lastWallpaper === "") {
			// First ever load
			persist.lastWallpaper = currentWallpaper
		} else if (currentWallpaper !== persist.lastWallpaper) {
			// Wallpaper changed (either live or after config reload)
			startWallpaperTransition()

		}
	}

	// Current workspace (1-indexed)
	property int currentWorkspace: Hyprland.activeWorkspaceId || 1

	// Calculate offset: center workspace = no offset, edges = max offset
	property real normalizedPosition: (currentWorkspace - 1) / Math.max(1, totalWorkspaces - 1)
	property real parallaxOffset: (normalizedPosition - 0.5) * parallaxStrength
	property real sidebarOffset: Gstate.sidebarOpen ? 0 : 0

	// Calculate the centered X position with parallax applied
	property real centeredX: (width - width * (1 + parallaxStrength)) / 2
	property real parallaxX: centeredX + (-parallaxOffset * width) + sidebarOffset

	function startWallpaperTransition() {
		if (isTransitioning) return

		var usePluginTransition = transitionPlugin !== null
		if (usePluginTransition) {
			isTransitioning = true
			pluginTransitionPending = true
		}

		// These are texture providers for transition plugins as well as the
		// built-in slide effect. Set them before selecting either implementation.
		oldWallpaperImage.source = persist.lastWallpaper
		newWallpaperImage.source = currentWallpaper
		if (usePluginTransition) {
			tryStartPluginTransition()
			return
		}
		isTransitioning = true

		// Set up old wallpaper image at current parallax position
		oldWallpaperImage.x = parallaxX
		oldWallpaperImage.opacity = 1

		// Set up new wallpaper starting from left (off-screen) with parallax offset
		newWallpaperImage.x = parallaxX - bgWindow.width
		newWallpaperImage.opacity = 1

		// Update animation targets to include parallax
		slideOutAnim.to = parallaxX + bgWindow.width
		slideInAnim.to = parallaxX

		// Start the slide animation
		slideOutAnim.start()
		slideInAnim.start()
	}

	function tryStartPluginTransition() {
		if (!pluginTransitionPending
			|| oldWallpaperImage.status !== Image.Ready
			|| newWallpaperImage.status !== Image.Ready)
			return

		pluginTransitionPending = false
		transitionLayer.transition = {
			oldSource: oldTransitionTexture,
			newSource: newTransitionTexture,
			oldWallpaper: persist.lastWallpaper,
			newWallpaper: currentWallpaper,
			duration: transitionDuration
		}
	}

	function finishWallpaperTransition() {
		wallpaperImage.source = currentWallpaper
		persist.lastWallpaper = currentWallpaper
		finishTransitionPending = true
		if (wallpaperImage.status === Image.Ready)
			completeWallpaperTransition()
	}

	function completeWallpaperTransition() {
		oldWallpaperImage.source = ""
		newWallpaperImage.source = ""
		pluginTransitionPending = false
		finishTransitionPending = false
		isTransitioning = false
	}

	Timer {
		id: pluginTransitionTimer
		// Plugins signal completion after presenting their final frame. This only
		// prevents a broken plugin from leaving the wallpaper transition open.
		interval: transitionDuration + 500
		onTriggered: bgWindow.finishWallpaperTransition()
	}

	// Old wallpaper (slides out to the right)
	Image {
		id: oldWallpaperImage
		fillMode: Image.PreserveAspectCrop
		asynchronous: true
		visible: isTransitioning
		onStatusChanged: bgWindow.tryStartPluginTransition()

		width: parent.width * (1 + parallaxStrength)
		height: parent.height * (1 + parallaxStrength)
		y: (parent.height - height) / 2

		x: parallaxX
		opacity: 1
	}

	// New wallpaper (slides in from the left)
	Image {
		id: newWallpaperImage
		fillMode: Image.PreserveAspectCrop
		asynchronous: true
		visible: isTransitioning
		onStatusChanged: bgWindow.tryStartPluginTransition()

		width: parent.width * (1 + parallaxStrength)
		height: parent.height * (1 + parallaxStrength)
		y: (parent.height - height) / 2

		x: parallaxX - parent.width
		opacity: 1
	}

	// ShaderEffectSource keeps the decoded wallpaper available as a GPU texture
	// while hideSource prevents it from painting over a plugin transition.
	ShaderEffectSource {
		id: oldTransitionTexture
		sourceItem: oldWallpaperImage
		hideSource: bgWindow.transitionPlugin !== null
		live: true
	}

	ShaderEffectSource {
		id: newTransitionTexture
		sourceItem: newWallpaperImage
		hideSource: bgWindow.transitionPlugin !== null
		live: true
	}

	// A transition plugin replaces the built-in slide while remaining active.
	Loader {
		id: transitionLayer
		anchors.fill: parent
		active: bgWindow.isTransitioning && bgWindow.transitionPlugin !== null
		source: active ? bgWindow.transitionPlugin.kindData.componentUrl : ""
		property var transition: ({})

		function deliverTransition() {
			if (!item || !transition.oldSource || !transition.newSource)
				return
			if ("transition" in item) item.transition = transition
			pluginTransitionTimer.restart()
		}

		onLoaded: {
			if (item && "plugin" in item) item.plugin = bgWindow.transitionPlugin
			if (item && "finished" in item) {
				item.finished.connect(function() {
					pluginTransitionTimer.stop()
					bgWindow.finishWallpaperTransition()
				})
			}
			deliverTransition()
		}
		onTransitionChanged: deliverTransition()
	}

	// Main wallpaper image (shown when not transitioning)
	Image {
		id: wallpaperImage
		fillMode: Image.PreserveAspectCrop
		source: col.wallpaper
		asynchronous: true
		visible: !isTransitioning
		opacity: status === Image.Ready ? 1 : 0
		onStatusChanged: {
			if (finishTransitionPending && status === Image.Ready)
				bgWindow.completeWallpaperTransition()
		}

		// Oversized to allow parallax movement
		width: parent.width * (1 + parallaxStrength)
		height: parent.height * (1 + parallaxStrength)

		// Center by default, shift based on workspace
		anchors.centerIn: parent
		anchors.horizontalCenterOffset: (-parallaxOffset * parent.width) + sidebarOffset

		Behavior on anchors.horizontalCenterOffset {
			enabled: !isTransitioning && enableParallax
			NumberAnimation {
				duration: 300
				easing.type: Easing.OutCubic
			}
		}
	}

	// Slide out animation (old wallpaper moves right)
	NumberAnimation {
		id: slideOutAnim
		target: oldWallpaperImage
		property: "x"
		to: parallaxX + bgWindow.width
		duration: transitionDuration
		easing.type: Easing.InOutCubic
	}

	// Slide in animation (new wallpaper follows from left)
	NumberAnimation {
		id: slideInAnim
		target: newWallpaperImage
		property: "x"
		to: parallaxX
		duration: transitionDuration
		easing.type: Easing.InOutCubic

		onFinished: {
			bgWindow.finishWallpaperTransition()
		}
	}

}
