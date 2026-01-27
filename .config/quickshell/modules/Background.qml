import Quickshell 
import Quickshell.Wayland
import Quickshell.Hyprland
import qs.modules
import QtQuick

PanelWindow {
	id: bgWindow
	WlrLayershell.layer: WlrLayer.Background
	exclusionMode: ExclusionMode.Ignore
	exclusiveZone: 0
	width: Screen.width
	height: Screen.height
	color: "black"

	Component.onCompleted: {
		console.log("Quickshell config fully loaded and ready!")
		
		// Now safe to start animations when image is ready
		if (wallpaperImage.status === Image.Ready && !startupComplete) {
			startStartupAnimations()
		}
	}

	function startStartupAnimations() {
		if (!startupComplete) {
			console.log("Starting startup zoom and fade animations")
			if (enableStartupZoom) {
				zoomAnim.start()
			} else {
				wallpaperImage.scale = 1.0
			}
			fadeAnim.start()
			startupComplete = true
			// Initialize previousWallpaper after startup
			previousWallpaper = col.wallpaper
		}
	}

	// Config-driven properties with fallbacks
	property bool enableParallax: cfg ? cfg.wallpaperParallax : true
	property real parallaxStrength: enableParallax ? (cfg ? cfg.wallpaperParallaxStrength : 0.1) : 0.0
	property bool enableStartupZoom: cfg ? cfg.wallpaperStartupZoom : true
	property real startupZoomScale: cfg ? cfg.wallpaperStartupZoomScale : 1.4
	property int startupZoomDuration: cfg ? cfg.wallpaperStartupZoomDuration : 1200
	property int transitionDuration: cfg ? cfg.wallpaperTransitionDuration : 600

	// Parallax configuration
	property int totalWorkspaces: cfg ? cfg.workspaceCount : 10
	
	// Startup state
	property bool startupComplete: false
	
	// Wallpaper transition configuration
	property string currentWallpaper: col.wallpaper
	property string previousWallpaper: ""
	property bool isTransitioning: false
	
	// Current workspace (1-indexed)
	property int currentWorkspace: Hyprland.focusedMonitor?.activeWorkspace?.id ?? 1
	
	// Calculate offset: center workspace = no offset, edges = max offset
	property real normalizedPosition: (currentWorkspace - 1) / Math.max(1, totalWorkspaces - 1)  // 0.0 to 1.0
	property real parallaxOffset: (normalizedPosition - 0.5) * parallaxStrength
	
	// Calculate the centered X position with parallax applied
	// Images are oversized, so we need to center them and then apply parallax
	property real centeredX: (width - width * (1 + parallaxStrength)) / 2
	property real parallaxX: centeredX + (-parallaxOffset * width)

	// Watch for wallpaper changes
	onCurrentWallpaperChanged: {
		if (startupComplete && previousWallpaper !== "" && previousWallpaper !== currentWallpaper) {
			startWallpaperTransition()
		}
	}

	Connections {
		target: col
		function onWallpaperChanged() {
			if (startupComplete && bgWindow.previousWallpaper !== "" && col.wallpaper !== bgWindow.previousWallpaper) {
				bgWindow.currentWallpaper = col.wallpaper
			}
		}
	}

	function startWallpaperTransition() {
		if (isTransitioning) return
		
		isTransitioning = true
		
		// Set up old wallpaper image at current parallax position
		oldWallpaperImage.source = previousWallpaper
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
		y: (parent.height - height) / 2  // Vertically centered
		
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
		y: (parent.height - height) / 2  // Vertically centered
		
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
		
		// Oversized to allow parallax movement
		width: parent.width * (1 + parallaxStrength)
		height: parent.height * (1 + parallaxStrength)
		
		// Center by default, shift based on workspace
		anchors.centerIn: parent
		anchors.horizontalCenterOffset: startupComplete ? (-parallaxOffset * parent.width) : 0
		
		// Initial state: zoomed in (if enabled) and invisible
		scale: enableStartupZoom ? startupZoomScale : 1.0
		opacity: 0
		
		// Wait for image to fully load before animating
		onStatusChanged: {
			if (status === Image.Ready && bgWindow.startupComplete === false) {
				// Check if ShellRoot.onCompleted has already run
				startStartupAnimations()
			}
		}
		
		Behavior on anchors.horizontalCenterOffset {
			enabled: startupComplete && !isTransitioning && enableParallax
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
		to: parallaxX + bgWindow.width  // Default, updated in startWallpaperTransition
		duration: transitionDuration
		easing.type: Easing.InOutCubic
	}
	
	// Slide in animation (new wallpaper follows from left)
	NumberAnimation {
		id: slideInAnim
		target: newWallpaperImage
		property: "x"
		to: parallaxX  // Default, updated in startWallpaperTransition
		duration: transitionDuration
		easing.type: Easing.InOutCubic
		
		onFinished: {
			// Transition complete - update main wallpaper and clean up
			wallpaperImage.source = currentWallpaper
			wallpaperImage.opacity = 1
			wallpaperImage.scale = 1.0
			
			// Store current as previous for next transition
			previousWallpaper = currentWallpaper
			
			// Reset transition images
			oldWallpaperImage.source = ""
			newWallpaperImage.source = ""
			
			isTransitioning = false
		}
	}
	
	// Explicit animations (not Behaviors) so we control exactly when they run
	NumberAnimation {
		id: zoomAnim
		target: wallpaperImage
		property: "scale"
		to: 1.0
		duration: startupZoomDuration
		easing.type: Easing.OutExpo
	}
	
	NumberAnimation {
		id: fadeAnim
		target: wallpaperImage
		property: "opacity"
		to: 1.0
		duration: startupZoomDuration * 0.4
		easing.type: Easing.OutQuad
	}
	BackgroundClock {}
}
