import Quickshell 
import Quickshell.Wayland
import Quickshell.Hyprland
import qs.modules
import QtQuick

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
	property int currentWorkspace: Hyprland.focusedMonitor?.activeWorkspace?.id ?? 1

	// Calculate offset: center workspace = no offset, edges = max offset
	property real normalizedPosition: (currentWorkspace - 1) / Math.max(1, totalWorkspaces - 1)
	property real parallaxOffset: (normalizedPosition - 0.5) * parallaxStrength

	// Calculate the centered X position with parallax applied
	property real centeredX: (width - width * (1 + parallaxStrength)) / 2
	property real parallaxX: centeredX + (-parallaxOffset * width)

	function startWallpaperTransition() {
		if (isTransitioning) return

		isTransitioning = true

		// Set up old wallpaper image at current parallax position
		oldWallpaperImage.source = persist.lastWallpaper
		oldWallpaperImage.x = parallaxX
		oldWallpaperImage.opacity = 1

		// Set up new wallpaper starting from left (off-screen) with parallax offset
		newWallpaperImage.source = currentWallpaper
		newWallpaperImage.x = parallaxX - bgWindow.width
		newWallpaperImage.opacity = 1

		// Update animation targets to include parallax
		slideOutAnim.to = parallaxX + bgWindow.width
		slideInAnim.to = parallaxX

		// Start the slide animation
		slideOutAnim.start()
		slideInAnim.start()
	}

	// Old wallpaper (slides out to the right)
	Image {
		id: oldWallpaperImage
		fillMode: Image.PreserveAspectCrop
		asynchronous: true
		visible: isTransitioning

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

		width: parent.width * (1 + parallaxStrength)
		height: parent.height * (1 + parallaxStrength)
		y: (parent.height - height) / 2

		x: parallaxX - parent.width
		opacity: 1
	}

	// Main wallpaper image (shown when not transitioning)
	Image {
		id: wallpaperImage
		fillMode: Image.PreserveAspectCrop
		source: col.wallpaper
		asynchronous: true
		visible: !isTransitioning
		opacity: status === Image.Ready ? 1 : 0

		// Oversized to allow parallax movement
		width: parent.width * (1 + parallaxStrength)
		height: parent.height * (1 + parallaxStrength)

		// Center by default, shift based on workspace
		anchors.centerIn: parent
		anchors.horizontalCenterOffset: -parallaxOffset * parent.width

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
			// Transition complete - update main wallpaper and clean up
			wallpaperImage.source = currentWallpaper
			persist.lastWallpaper = currentWallpaper

			// Reset transition images
			oldWallpaperImage.source = ""
			newWallpaperImage.source = ""

			isTransitioning = false
		}
	}

}
