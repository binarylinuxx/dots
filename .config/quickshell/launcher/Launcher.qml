import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Io
import Quickshell.Widgets
import Quickshell.Hyprland
import qs.widgets
import qs.services

PanelWindow {
	id: root
	exclusiveZone: 0
	property bool isWallpaperBrowserMode: isWallhavenMode || isPexelsMode
	width: isWallpaperBrowserMode ? 1220 : (cfg ? cfg.launcherWidth + 20 : 420)
	height: isWallpaperBrowserMode ? 700 : (cfg ? cfg.launcherHeight : 540)

	Behavior on width {
		NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
	}
	Behavior on height {
		NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
	}
	color: "transparent"
	visible: Gstate.appsOpen
	focusable: true

	// Settings from config
	readonly property int launcherWidth: cfg ? cfg.launcherWidth : 400
	readonly property int itemHeight: cfg ? cfg.launcherItemHeight : 55
	readonly property int launcherRadius: cfg ? cfg.launcherRadius : 25
	readonly property int maxItems: cfg ? cfg.launcherMaxItems : 8
	readonly property bool showIcons: cfg ? cfg.launcherShowIcons : true
	readonly property bool showDescriptions: cfg ? cfg.launcherShowDescriptions : true
	readonly property bool searchAtTop: cfg ? cfg.launcherSearchAtTop : true
	readonly property bool emojiModeEnabled: cfg ? cfg.launcherEmojiMode : true
	readonly property bool clipboardModeEnabled: cfg ? cfg.launcherClipboardMode : true
	readonly property bool wallpaperModeEnabled: cfg ? cfg.launcherWallpaperMode : true

	// Preset configurations
	function applyPreset(preset) {
		switch (preset) {
			case "compact":
				return { width: 350, itemHeight: 45, radius: 20, maxItems: 6 }
			case "expanded":
				return { width: 450, itemHeight: 65, radius: 30, maxItems: 10 }
			default: // "default"
				return { width: 400, itemHeight: 55, radius: 25, maxItems: 8 }
		}
	}

	property string searchTerm: ""
	property int visibleAppCount: 0
	property int visibleWallpaperCount: 0
	property int visibleEmojiCount: 0
	property int visibleClipboardCount: 0
	
	// Filtered apps list for proper search ordering
	property var filteredApps: []
	
	function updateFilteredApps() {
		var term = appSearchTerm.toLowerCase()
		var apps = []
		var model = DesktopEntries.applications
		// ObjectModel uses values property in Quickshell
		var appList = model.values ? model.values : model
		var count = appList.length !== undefined ? appList.length : (appList.count || 0)
		for (var i = 0; i < count; i++) {
			var app = appList[i] || (appList.get ? appList.get(i) : null)
			if (app && matchesSearch(app, term)) {
				apps.push(app)
			}
		}
		if (term) {
			apps.sort(function(a, b) {
				return appSortScore(b, term) - appSortScore(a, term)
			})
		}
		filteredApps = apps
		visibleAppCount = apps.length
	}
	
	// Mode flags
	property bool isWallpaperMode: false
	property bool isEmojiMode: false
	property bool isClipboardMode: false
	property bool isWallhavenMode: false
	property bool isPexelsMode: false
	property bool isMathMode: false
	property string mathResult: ""

	// Shared wallpaper browser state
	property string wallpaperProvider: "wallhaven"  // "wallhaven" | "pexels"

	// Wallhaven state
	property var wallhavenResults: []
	property bool isLoadingWallhaven: false
	property string wallhavenQuery: ""
	property string wallhavenRawData: ""
	property int wallhavenPage: 1
	property int wallhavenLastPage: 1
	property bool wallhavenHasMore: false

	// Pexels state
	property var pexelsResults: []
	property bool isLoadingPexels: false
	property string pexelsQuery: ""
	property string pexelsRawData: ""
	property int pexelsPage: 1
	property bool pexelsHasMore: false
	
	function evaluateMath(expression) {
		try {
			// Remove the leading semicolon
			var expr = expression.substring(1).trim()
			if (!expr) return ""
			
			// Handle special constants
			if (expr === "pi" || expr === "PI") return Math.PI.toString()
			if (expr === "e" || expr === "E") return Math.E.toString()
			
			// Replace common math functions
			expr = expr.replace(/sin\(/g, "Math.sin(")
			expr = expr.replace(/cos\(/g, "Math.cos(")
			expr = expr.replace(/tan\(/g, "Math.tan(")
			expr = expr.replace(/sqrt\(/g, "Math.sqrt(")
			expr = expr.replace(/abs\(/g, "Math.abs(")
			expr = expr.replace(/log\(/g, "Math.log(")
			expr = expr.replace(/pow\(/g, "Math.pow(")
			expr = expr.replace(/floor\(/g, "Math.floor(")
			expr = expr.replace(/ceil\(/g, "Math.ceil(")
			expr = expr.replace(/round\(/g, "Math.round(")
			expr = expr.replace(/random\(/g, "Math.random(")
			expr = expr.replace(/min\(/g, "Math.min(")
			expr = expr.replace(/max\(/g, "Math.max(")
			
			// Replace pi and e in expressions
			expr = expr.replace(/\bpi\b/gi, "Math.PI")
			expr = expr.replace(/\be\b/gi, "Math.E")
			
			// Evaluate the expression
			var result = eval(expr)
			
			// Format the result
			if (typeof result === 'number') {
				// Round to 10 decimal places to avoid floating point errors
				return result.toFixed(10).replace(/\.?0+$/, '')
			}
			return result.toString()
		} catch (e) {
			return ""
		}
	}
	
	property var wallpapers: []
	property var emojis: []
	property var clipboardItems: []
	
	property bool isLoadingWallpapers: false
	property bool isLoadingEmojis: false
	property bool isLoadingClipboard: false
	
	property string wallpaperDir: Quickshell.env("HOME") + "/.local/wallpapers"

	// Debounce timer for filtering
	Timer {
		id: filterTimer
		interval: 30
		repeat: false
		onTriggered: updateFilteredApps()
	}

	// Filtered search term (excludes commands)
	property string appSearchTerm: (isWallpaperMode || isEmojiMode || isClipboardMode || isWallhavenMode || isPexelsMode) ? "" : searchTerm

	Timer {
		id: updateVisibleCount
		interval: 50
		repeat: false
		running: false
		onTriggered: {
			if (isWallpaperMode) {
				root.visibleWallpaperCount = countVisibleWallpapers()
			} else if (isEmojiMode) {
				root.visibleEmojiCount = getFilteredEmojis().length
			} else if (isClipboardMode) {
				root.visibleClipboardCount = getFilteredClipboard().length
			}
			// App count is handled by updateFilteredApps
		}
	}

	function countVisibleWallpapers() {
		return wallpapers.filter(w => matchesWallpaperSearch(w)).length
	}

	function matchesWallpaperSearch(filename) {
		var term = searchTerm.replace(/^\/wallpaper\s*/, "").trim()
		if (!term || term === "") return true
		return filename.toLowerCase().includes(term.toLowerCase())
	}

	function trimFilename(name, maxLen) {
		if (name.length <= maxLen) return name
		var ext = name.lastIndexOf(".")
		if (ext > 0) {
			var base = name.substring(0, ext)
			var extension = name.substring(ext)
			var availableLen = maxLen - extension.length - 3
			if (availableLen > 0) {
				return base.substring(0, availableLen) + "..." + extension
			}
		}
		return name.substring(0, maxLen - 3) + "..."
	}

	function applyWallpaper(filename) {
		var fullPath = wallpaperDir + "/" + filename
		var mode = cfg ? cfg.matugenMode : "dark"
		var scheme = cfg ? cfg.matugenScheme : "tonal-spot"
		var contrast = cfg ? cfg.matugenContrast : 0.0
		var genScript = Qt.resolvedUrl("../col_gen/generate").toString().replace("file://", "")
		matugenProcess.command = [
			genScript, "image", fullPath,
			"-m", mode, "-s", scheme, "-c", contrast.toString()
		]
		matugenProcess.running = true
		// Close launcher immediately, don't wait for process
		searchField.text = ""
		Gstate.appsOpen = false
	}

	// Emoji functions
	function getFilteredEmojis() {
		var term = searchTerm.replace(/^[:/]emoji?\s*/, "").trim().toLowerCase()
		if (!term) return emojis
		return emojis.filter(e => {
			if (e.emoji.includes(term)) return true
			// Support both formats: {name, keywords} and {en} 
			if (e.name && e.name.toLowerCase().includes(term)) return true
			if (e.keywords && e.keywords.some(k => k.toLowerCase().includes(term))) return true
			if (e.en && e.en.some(k => k.toLowerCase().includes(term))) return true
			return false
		})
	}

	function copyEmoji(emoji) {
		copyProcess.command = ["wl-copy", emoji]
		copyProcess.running = true
	}

	// Clipboard functions
	function getFilteredClipboard() {
		var term = searchTerm.replace(/^\/clip(board)?\s*/, "").trim().toLowerCase()
		if (!term) return clipboardItems
		return clipboardItems.filter(item => item.content.toLowerCase().includes(term))
	}

	function pasteClipboardItem(id) {
		pasteProcess.command = ["sh", "-c", "cliphist decode " + id + " | wl-copy"]
		pasteProcess.running = true
	}

	Process {
		id: listWallpapersProcess
		command: ["sh", "-c", "ls -1 " + wallpaperDir + " | grep -iE '\\.(jpg|jpeg|png)$'"]
		stdout: SplitParser {
			onRead: data => {
				if (root.isLoadingWallpapers) {
					root.wallpapers = [data]
					root.isLoadingWallpapers = false
				} else {
					root.wallpapers = root.wallpapers.concat([data])
				}
			}
		}
	}

	Process {
		id: matugenProcess
		stderr: SplitParser {
			onRead: data => console.log("[matugen stderr]", data)
		}
		onExited: (exitCode, exitStatus) => {
			if (exitCode === 0) {
				console.log("Wallpaper applied successfully")
			} else {
				console.log("[matugen] exited with code:", exitCode)
			}
		}
	}

	// Load emojis using jq for single-line JSON output
	property string emojiRawData: ""
	Process {
		id: loadEmojisProcess
		command: ["jq", "-c", ".", Quickshell.env("HOME") + "/.config/quickshell/launcher/modes/emojis.json"]
		stdout: SplitParser {
			onRead: data => {
				root.emojiRawData = data
			}
		}
		onExited: (code, status) => {
			root.isLoadingEmojis = false
			if (code === 0 && root.emojiRawData.length > 0) {
				try {
					root.emojis = JSON.parse(root.emojiRawData)
					console.log("Loaded", root.emojis.length, "emojis")
				} catch (e) {
					console.error("Failed to parse emojis:", e)
				}
			}
		}
	}

	// List clipboard items
	Process {
		id: listClipboardProcess
		command: ["cliphist", "list"]
		stdout: SplitParser {
			onRead: data => {
				var tabIndex = data.indexOf("\t")
				if (tabIndex > 0) {
					var id = data.substring(0, tabIndex)
					var content = data.substring(tabIndex + 1)
					var isImage = content.startsWith("[[ binary data")
					root.clipboardItems = root.clipboardItems.concat([{
						"id": id,
						"content": content,
						"isImage": isImage
					}])
				}
			}
		}
		onExited: {
			root.isLoadingClipboard = false
		}
	}

	// Copy to clipboard
	Process {
		id: copyProcess
		onExited: (exitCode, exitStatus) => {
			if (exitCode === 0) {
				searchField.text = ""
				Gstate.appsOpen = false
			}
		}
	}

	// Paste from clipboard history
	Process {
		id: pasteProcess
		onExited: (exitCode, exitStatus) => {
			if (exitCode === 0) {
				searchField.text = ""
				Gstate.appsOpen = false
			}
		}
	}

	function wallhavenUrlParams() {
		var mode = cfg ? cfg.wallhavenPurityMode : "sfw"
		var purity = (mode === "sfw") ? "100" : "111"
		var key = (mode === "apikey" && cfg && cfg.wallhavenApiKey) ? cfg.wallhavenApiKey : ""
		return "&purity=" + purity + (key.length > 0 ? ("&apikey=" + key) : "")
	}

	// Wallhaven search debounce timer
	Timer {
		id: wallhavenSearchTimer
		interval: 500
		repeat: false
		onTriggered: {
			var q = searchTerm.replace(/^\/wallhaven\s*/, "").trim()
			if (q.length > 0) {
				root.wallhavenQuery = q
				root.wallhavenPage = 1
				root.wallhavenResults = []
				root.wallhavenRawData = ""
				root.isLoadingWallhaven = true
				wallhavenSearchProcess.command = [
					"curl", "-s", "--max-time", "10",
					"https://wallhaven.cc/api/v1/search?q=" + encodeURIComponent(q) +
					"&categories=111&sorting=relevance&order=desc&ratios=16x9,16x10&page=1" +
					wallhavenUrlParams()
				]
				wallhavenSearchProcess.running = true
			} else {
				root.wallhavenResults = []
				root.isLoadingWallhaven = false
			}
		}
	}

	function wallhavenLoadMore() {
		if (root.isLoadingWallhaven || !root.wallhavenHasMore) return
		var nextPage = root.wallhavenPage + 1
		root.isLoadingWallhaven = true
		root.wallhavenRawData = ""
		wallhavenSearchProcess.command = [
			"curl", "-s", "--max-time", "10",
			"https://wallhaven.cc/api/v1/search?q=" + encodeURIComponent(root.wallhavenQuery) +
			"&categories=111&sorting=relevance&order=desc&ratios=16x9,16x10&page=" + nextPage +
			wallhavenUrlParams()
		]
		wallhavenSearchProcess.running = true
		root.wallhavenPage = nextPage
	}

	Process {
		id: wallhavenSearchProcess
		stdout: SplitParser {
			splitMarker: ""
			onRead: data => { root.wallhavenRawData += data }
		}
		onExited: (code, status) => {
			root.isLoadingWallhaven = false
			if (code === 0 && root.wallhavenRawData.length > 0) {
				try {
					var parsed = JSON.parse(root.wallhavenRawData)
					var items = parsed.data || []
					var meta = parsed.meta || {}
					var newResults = items.map(function(w) {
						return {
							id: w.id,
							thumb: w.thumbs.large,
							path: w.path,
							resolution: w.resolution,
							dimX: w.dimension_x,
							dimY: w.dimension_y,
							ratio: parseFloat(w.ratio),
							fileType: w.file_type
						}
					})
					if (root.wallhavenPage === 1) {
						root.wallhavenResults = newResults
					} else {
						root.wallhavenResults = root.wallhavenResults.concat(newResults)
					}
					root.wallhavenLastPage = meta.last_page || 1
					root.wallhavenHasMore = root.wallhavenPage < root.wallhavenLastPage
				} catch (e) {
					console.error("Wallhaven parse error:", e)
				}
			}
		}
	}

	// Pexels search
	function pexelsApiKey() {
		return cfg ? cfg.pexelsApiKey : ""
	}

	Timer {
		id: pexelsSearchTimer
		interval: 500
		repeat: false
		onTriggered: {
			var q = searchTerm.replace(/^\/pexels\s*/, "").trim()
			if (q.length > 0 && pexelsApiKey().length > 0) {
				root.pexelsQuery = q
				root.pexelsPage = 1
				root.pexelsResults = []
				root.pexelsRawData = ""
				root.isLoadingPexels = true
				pexelsSearchProcess.command = [
					"curl", "-s", "--max-time", "10",
					"-H", "Authorization: " + pexelsApiKey(),
					"https://api.pexels.com/v1/search?query=" + encodeURIComponent(q) +
					"&per_page=20&page=1&orientation=landscape"
				]
				pexelsSearchProcess.running = true
			} else {
				root.pexelsResults = []
				root.isLoadingPexels = false
			}
		}
	}

	function pexelsLoadMore() {
		if (root.isLoadingPexels || !root.pexelsHasMore) return
		var nextPage = root.pexelsPage + 1
		root.isLoadingPexels = true
		root.pexelsRawData = ""
		pexelsSearchProcess.command = [
			"curl", "-s", "--max-time", "10",
			"-H", "Authorization: " + pexelsApiKey(),
			"https://api.pexels.com/v1/search?query=" + encodeURIComponent(root.pexelsQuery) +
			"&per_page=20&page=" + nextPage + "&orientation=landscape"
		]
		pexelsSearchProcess.running = true
		root.pexelsPage = nextPage
	}

	Process {
		id: pexelsSearchProcess
		stdout: SplitParser {
			splitMarker: ""
			onRead: data => { root.pexelsRawData += data }
		}
		onExited: (code, status) => {
			root.isLoadingPexels = false
			if (code === 0 && root.pexelsRawData.length > 0) {
				try {
					var parsed = JSON.parse(root.pexelsRawData)
					var items = parsed.photos || []
					var newResults = items.map(function(p) {
						return {
							id: p.id,
							thumb: p.src.medium,
							path: p.src.original,
							resolution: p.width + "x" + p.height,
							dimX: p.width,
							dimY: p.height,
							ratio: p.width / p.height,
							fileType: "image/jpeg",
							author: p.photographer || "",
							authorUrl: p.photographer_url || ""
						}
					})
					if (root.pexelsPage === 1) {
						root.pexelsResults = newResults
					} else {
						root.pexelsResults = root.pexelsResults.concat(newResults)
					}
					root.pexelsHasMore = parsed.next_page !== undefined && parsed.next_page !== null
				} catch(e) {
					console.error("Pexels parse error:", e)
				}
			}
		}
	}

	// Pexels download + apply
	property string pexelsDownloadId: ""
	Process {
		id: pexelsDownloadProcess
		onExited: (code, status) => {
			if (code === 0) {
				var dest = root.wallpaperDir + "/pexels-" + root.pexelsDownloadId + ".jpg"
				var mode = cfg ? cfg.matugenMode : "dark"
				var scheme = cfg ? cfg.matugenScheme : "tonal-spot"
				var contrast = cfg ? cfg.matugenContrast : 0.0
				var genScript = Qt.resolvedUrl("../col_gen/generate").toString().replace("file://", "")
				matugenProcess.command = [genScript, "image", dest, "-m", mode, "-s", scheme, "-c", contrast.toString()]
				matugenProcess.running = true
				searchField.text = ""
				Gstate.appsOpen = false
			}
		}
	}

	function downloadAndApplyPexels(wallpaper) {
		var dest = root.wallpaperDir + "/pexels-" + wallpaper.id + ".jpg"
		root.pexelsDownloadId = wallpaper.id
		pexelsDownloadProcess.command = ["curl", "-sL", "--max-time", "60", "-o", dest, wallpaper.path]
		pexelsDownloadProcess.running = true
	}

	// Wallhaven download + apply
	property string wallhavenDownloadUrl: ""
	property string wallhavenDownloadId: ""
	Process {
		id: wallhavenDownloadProcess
		onExited: (code, status) => {
			if (code === 0) {
				var ext = root.wallhavenDownloadUrl.split(".").pop()
				var dest = root.wallpaperDir + "/wallhaven-" + root.wallhavenDownloadId + "." + ext
				var mode = cfg ? cfg.matugenMode : "dark"
				var scheme = cfg ? cfg.matugenScheme : "tonal-spot"
				var contrast = cfg ? cfg.matugenContrast : 0.0
				var genScript = Qt.resolvedUrl("../col_gen/generate").toString().replace("file://", "")
				matugenProcess.command = [
					genScript, "image", dest,
					"-m", mode, "-s", scheme, "-c", contrast.toString()
				]
				matugenProcess.running = true
				searchField.text = ""
				Gstate.appsOpen = false
			} else {
				console.error("Wallhaven download failed, exit code:", code)
			}
		}
	}

	function downloadAndApplyWallhaven(wallpaper) {
		var ext = wallpaper.path.split(".").pop()
		var dest = root.wallpaperDir + "/wallhaven-" + wallpaper.id + "." + ext
		root.wallhavenDownloadUrl = wallpaper.path
		root.wallhavenDownloadId = wallpaper.id
		wallhavenDownloadProcess.command = ["curl", "-s", "-L", "--max-time", "60", "-o", dest, wallpaper.path]
		wallhavenDownloadProcess.running = true
	}

	function launchApp(appData) {
		console.log("Attempting to launch:", appData.name)
		
		if (typeof appData.execute === 'function') {
			try {
				appData.execute()
				console.log("Launched:", appData.name)
			} catch (error) {
				console.error("Error launching", appData.name, ":", error)
			}
		} else {
			if (appData.command && Array.isArray(appData.command) && appData.command.length > 0) {
				try {
					Quickshell.execDetached(appData.command, appData.workingDirectory ?? "")
					console.log("Launched:", appData.name)
				} catch (execError) {
					console.error("Launch failed for", appData.name, ":", execError)
				}
			}
		}
		searchField.text = ""
		Gstate.appsOpen = false
	}

	// Returns a fuzzy match score: -1 = no match, higher = better match
	function fuzzyScore(str, term) {
		if (!str) return -1
		var s = str.toLowerCase()
		var t = term.toLowerCase()
		if (s === t) return 1000
		if (s.startsWith(t)) return 900
		if (s.includes(t)) return 800
		// Check all chars of term appear in order (scattered match)
		var si = 0, ti = 0, consecutive = 0, score = 0
		while (si < s.length && ti < t.length) {
			if (s[si] === t[ti]) {
				score += consecutive * 10 + 1
				consecutive++
				ti++
			} else {
				consecutive = 0
			}
			si++
		}
		if (ti < t.length) return -1 // not all chars matched
		return score
	}

	function matchesSearch(app, term) {
		if (!term || term === "") return true
		return fuzzyScore(app.name, term) >= 0 ||
		       fuzzyScore(app.genericName, term) >= 0 ||
		       fuzzyScore(app.comment, term) >= 0
	}

	function appSortScore(app, term) {
		if (!term || term === "") return 0
		var scores = [
			fuzzyScore(app.name, term),
			fuzzyScore(app.genericName, term),
			fuzzyScore(app.comment, term)
		]
		var best = -1
		for (var i = 0; i < scores.length; i++)
			if (scores[i] > best) best = scores[i]
		return best
	}

	onVisibleChanged: {
		if (visible) {
			searchField.forceActiveFocus()
			updateFilteredApps()
			appsListView.currentIndex = 0
		} else {
			searchField.text = ""
			root.isWallhavenMode = false
			root.wallhavenResults = []
			root.wallhavenQuery = ""
			root.wallhavenPage = 1
			root.wallhavenHasMore = false
			root.isPexelsMode = false
			root.pexelsResults = []
			root.pexelsQuery = ""
			root.pexelsPage = 1
			root.pexelsHasMore = false
		}
	}

	function getContainerHeight() {
		var searchBarHeight = 50 // search bar + margins
		var maxHeight = maxItems * itemHeight + searchBarHeight
		
		if (isWallpaperBrowserMode) {
			return 680
		} else if (isWallpaperMode) {
			return Math.min(Math.max(Math.ceil(visibleWallpaperCount / 3) * 110 + searchBarHeight, 120), maxHeight)
		} else if (isEmojiMode) {
			return Math.min(Math.max(visibleEmojiCount * itemHeight + searchBarHeight, 120), maxHeight)
		} else if (isClipboardMode) {
			return Math.min(Math.max(visibleClipboardCount * itemHeight + searchBarHeight, 120), maxHeight)
		}
		return Math.min(Math.max(visibleAppCount * itemHeight + searchBarHeight, 120), maxHeight)
	}

	Rectangle {
		id: mainContainer
		width: isWallpaperBrowserMode ? 1200 : launcherWidth
		height: getContainerHeight()
		radius: launcherRadius
		color: col.background
		anchors.centerIn: parent

		Behavior on width {
			NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
		}

		Behavior on height {
			NumberAnimation {
				duration: Gstate.animDuration
				easing.type: Easing.OutCubic
			}
		}

		// Search Field
		Rectangle {
			id: searchBox
			width: parent.width - 10
			height: 40
			radius: 40
			color: col.surfaceContainer
			anchors.horizontalCenter: parent.horizontalCenter
			anchors.top: parent.top
			anchors.topMargin: 5

			Rectangle {
				height: 30
				width: 35
				radius: height * 2
				anchors.verticalCenter: parent.verticalCenter
				anchors.left: parent.left
				anchors.leftMargin: 5
				color: col.secondaryContainer
				MaterialSymbol {
					icon: isPexelsMode ? "photo_camera" : (isWallhavenMode ? "wallpaper" : (isWallpaperMode ? "image" : (isEmojiMode ? "emoji_emotions" : (isClipboardMode ? "content_paste" : "search"))))
					iconSize: 28
					anchors.centerIn: parent
					color: col.background
				}
			}

			TextField {
				id: searchField
				anchors.fill: parent
				anchors.leftMargin: 45
				anchors.rightMargin: 10
				placeholderText: isPexelsMode ? "Search Pexels..." : (isWallhavenMode ? "Search Wallhaven..." : (isWallpaperMode ? "Search wallpapers..." : (isEmojiMode ? "Search emojis..." : (isClipboardMode ? "Search clipboard..." : "Search apps..."))))
				placeholderTextColor: col.onSurfaceVariant
				background: null
				color: col.onSurface
				font.pixelSize: 14
				font.family: cfg ? cfg.fontFamily : "Rubik"
				verticalAlignment: Text.AlignVCenter

			onTextChanged: {
			root.searchTerm = text.trim()
			var trimmed = text.trim()
			
			// Reset all modes first
			root.isWallpaperMode = false
			root.isEmojiMode = false
			root.isClipboardMode = false
			root.isWallhavenMode = false
			root.isPexelsMode = false

			// Check for commands
			if (trimmed.startsWith("/pexels")) {
				root.isPexelsMode = true
				root.wallpaperProvider = "pexels"
				pexelsSearchTimer.restart()
			} else if (trimmed.startsWith("/wallhaven")) {
				root.isWallhavenMode = true
				root.wallpaperProvider = "wallhaven"
				wallhavenSearchTimer.restart()
			} else if (trimmed.startsWith("/wallpaper") && wallpaperModeEnabled) {
				root.isWallpaperMode = true
				if (!root.isLoadingWallpapers && root.wallpapers.length === 0) {
					root.isLoadingWallpapers = true
					listWallpapersProcess.running = true
				}
				updateVisibleCount.start()
			} else if ((trimmed.startsWith(":") || trimmed.startsWith("/emoji")) && emojiModeEnabled) {
				root.isEmojiMode = true
				if (!root.isLoadingEmojis && root.emojis.length === 0) {
					root.isLoadingEmojis = true
					loadEmojisProcess.running = true
				}
				updateVisibleCount.start()
			} else if (trimmed.startsWith("/clip") && clipboardModeEnabled) {
				root.isClipboardMode = true
				// Always reload clipboard when entering mode
				root.isLoadingClipboard = true
				root.clipboardItems = []
				listClipboardProcess.running = true
				updateVisibleCount.start()
			} else {
				// App mode - update filtered list
				filterTimer.restart()
				appsListView.currentIndex = 0
			}
		}

				Keys.onDownPressed: {
					if (isEmojiMode) {
						if (emojiListView.currentIndex < emojiListView.count - 1)
							emojiListView.currentIndex++
					} else if (isClipboardMode) {
						if (clipboardListView.currentIndex < clipboardListView.count - 1)
							clipboardListView.currentIndex++
					} else if (!isWallpaperMode) {
						if (appsListView.currentIndex < appsListView.count - 1)
							appsListView.currentIndex++
					}
				}
				Keys.onUpPressed: {
					if (isEmojiMode) {
						if (emojiListView.currentIndex > 0)
							emojiListView.currentIndex--
					} else if (isClipboardMode) {
						if (clipboardListView.currentIndex > 0)
							clipboardListView.currentIndex--
					} else if (!isWallpaperMode) {
						if (appsListView.currentIndex > 0)
							appsListView.currentIndex--
					}
				}
			Keys.onReturnPressed: {
				if (isEmojiMode) {
					var filtered = getFilteredEmojis()
					if (emojiListView.currentIndex >= 0 && emojiListView.currentIndex < filtered.length) {
						copyEmoji(filtered[emojiListView.currentIndex].emoji)
					}
				} else if (isClipboardMode) {
					var clipFiltered = getFilteredClipboard()
					if (clipboardListView.currentIndex >= 0 && clipboardListView.currentIndex < clipFiltered.length) {
						pasteClipboardItem(clipFiltered[clipboardListView.currentIndex].id)
					}
				} else if (!isWallpaperMode && appsListView.currentIndex >= 0 && appsListView.currentIndex < filteredApps.length) {
					launchApp(filteredApps[appsListView.currentIndex])
				}
			}
				Keys.onEscapePressed: {
					Gstate.appsOpen = false
				}
			}
		}

		// Apps List
		ScrollView {
			id: appsScrollView
			clip: true
			visible: !isWallpaperMode && !isEmojiMode && !isClipboardMode && !isWallhavenMode && !isPexelsMode
			anchors {
				top: searchBox.bottom
				topMargin: 5
				left: parent.left
				leftMargin: 5
				right: parent.right
				rightMargin: 5
				bottom: parent.bottom
				bottomMargin: 5
			}

			ListView {
				id: appsListView
				model: filteredApps
				currentIndex: 0
				highlightFollowsCurrentItem: true
				spacing: 0

			delegate: Rectangle {
				id: delegateItem
				width: ListView.view.width
				height: itemHeight
				radius: mouseArea.containsMouse ? 50 : 12
				color: ListView.isCurrentItem || mouseArea.containsMouse ? col.surfaceContainer : "transparent"

				property var appData: modelData

					scale: mouseArea.containsMouse ? 0.97 : 1.0
					Behavior on scale {
						NumberAnimation {
							duration: Gstate.animDuration
							easing.type: Easing.OutBack
						}
					}

					Behavior on color {
						ColorAnimation { duration: Gstate.animDuration }
					}

					Behavior on radius {
						NumberAnimation { duration: Gstate.animDuration }
					}

					RowLayout {
						anchors.fill: parent
						anchors.margins: 8
						spacing: 10

						// App Icon
						Item {
							Layout.preferredWidth: itemHeight - 15
							Layout.preferredHeight: itemHeight - 15
							Layout.alignment: Qt.AlignVCenter
							visible: showIcons

							IconImage {
								id: appIcon
								anchors.fill: parent
								source: Quickshell.iconPath(modelData.icon ?? "", true)
								smooth: true

								Rectangle {
									anchors.fill: parent
									radius: 8
									color: col.primaryContainer
									visible: appIcon.status !== Image.Ready

									Text {
										anchors.centerIn: parent
										text: (modelData.name && modelData.name.length > 0) ? modelData.name.charAt(0).toUpperCase() : "?"
										color: col.onPrimaryContainer
										font.pixelSize: 16
										font.bold: true
									}
								}
							}
						}

						// App Info
						ColumnLayout {
							Layout.fillWidth: true
							Layout.alignment: Qt.AlignVCenter
							spacing: 2

							Text {
								Layout.fillWidth: true
								text: modelData.name ?? "Unknown"
								color: col.onSurface
								font.pixelSize: 14
								font.family: cfg ? cfg.fontFamily : "Rubik"
								font.weight: Font.Medium
								elide: Text.ElideRight
							}

							Text {
								Layout.fillWidth: true
								text: modelData.genericName ?? modelData.comment ?? ""
								color: col.onSurfaceVariant
								font.pixelSize: 11
								font.family: cfg ? cfg.fontFamily : "Rubik"
								elide: Text.ElideRight
								visible: text !== "" && showDescriptions
							}
						}

						// Launch button on hover
						Rectangle {
							width: 60
							height: 25
							color: col.primary
							radius: 12
							visible: mouseArea.containsMouse || delegateItem.ListView.isCurrentItem

							Text {
								anchors.centerIn: parent
								text: "Launch"
								color: col.onPrimary
								font.pixelSize: 12
								font.family: cfg ? cfg.fontFamily : "Rubik"
								font.weight: Font.Medium
							}
						}
					}

					MouseArea {
						id: mouseArea
						anchors.fill: parent
						hoverEnabled: true
						cursorShape: Qt.PointingHandCursor
						onClicked: {
							appsListView.currentIndex = index
							launchApp(modelData)
						}
					}
				}

				// Empty state
				Text {
					anchors.centerIn: parent
					text: "No apps found"
					color: col.onSurfaceVariant
					font.pixelSize: 14
					font.family: cfg ? cfg.fontFamily : "Rubik"
					visible: filteredApps.length === 0 && appSearchTerm !== ""
				}
			}
		}

		// Wallhaven Grid
		Item {
			id: wallhavenSection
			visible: isWallhavenMode
			anchors {
				top: searchBox.bottom
				topMargin: 5
				left: parent.left
				leftMargin: 5
				right: parent.right
				rightMargin: 5
				bottom: parent.bottom
				bottomMargin: 5
			}

			// Empty / loading state
			Text {
				anchors.centerIn: parent
				text: isLoadingWallhaven
					? "Searching Wallhaven..."
					: (wallhavenQuery === ""
						? "Type to search wallpapers"
						: (wallhavenResults.length === 0 ? "No results" : ""))
				color: col.onSurfaceVariant
				font.pixelSize: 14
				font.family: cfg ? cfg.fontFamily : "Rubik"
				visible: wallhavenResults.length === 0
			}

			// Masonry layout engine
			// Splits results into 4 columns, each tile height = tileWidth / ratio
			// Assigns each tile to the shortest column (greedy bin-packing)
			property int colCount: 4
			property int gap: 4
			property real colWidth: (width - gap * (colCount - 1)) / colCount

			// Computed layout: array of {x, y, w, h} per result index
			property var layout: []
			property real masonryHeight: 0

			function computeLayout() {
				var cols = colCount
				var cw = colWidth
				var g = gap
				var colHeights = []
				for (var c = 0; c < cols; c++) colHeights[c] = 0

				var positions = []
				for (var i = 0; i < wallhavenResults.length; i++) {
					var item = wallhavenResults[i]
					var ratio = item.ratio > 0 ? item.ratio : 16/9
					var th = cw / ratio

					// Find shortest column
					var shortest = 0
					for (var c2 = 1; c2 < cols; c2++) {
						if (colHeights[c2] < colHeights[shortest]) shortest = c2
					}

					positions.push({
						x: shortest * (cw + g),
						y: colHeights[shortest],
						w: cw,
						h: th
					})
					colHeights[shortest] += th + g
				}

				// Total height = tallest column
				var maxH = 0
				for (var c3 = 0; c3 < cols; c3++) {
					if (colHeights[c3] > maxH) maxH = colHeights[c3]
				}
				layout = positions
				masonryHeight = maxH
			}

			onWidthChanged: if (wallhavenResults.length > 0) computeLayout()

			Connections {
				target: root
				function onWallhavenResultsChanged() { wallhavenSection.computeLayout() }
			}

			Flickable {
				id: wallhavenFlickable
				anchors.fill: parent
				clip: true
				contentWidth: width
				contentHeight: wallhavenSection.masonryHeight + 60 // extra space for load-more row
				boundsBehavior: Flickable.StopAtBounds
				ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

				// Masonry canvas
				Item {
					id: wallhavenCanvas
					width: wallhavenFlickable.contentWidth
					height: wallhavenSection.masonryHeight

					Repeater {
						id: whRepeater
						model: wallhavenResults

					delegate: Item {
						id: whDelegate
						property var pos: wallhavenSection.layout[index] || {x:0,y:0,w:0,h:0}
						x: pos.x
						y: pos.y
						width: pos.w
						height: pos.h

						// Entrance pop animation from bottom-right corner
						transformOrigin: Item.BottomRight
						scale: 0
						Component.onCompleted: {
							popInTimer.interval = index * 30
							popInTimer.start()
						}
						Timer {
							id: popInTimer
							repeat: false
							onTriggered: popInAnim.start()
						}
						NumberAnimation {
							id: popInAnim
							target: whDelegate
							property: "scale"
							from: 0
							to: 1
							duration: 220
							easing.type: Easing.OutBack
							easing.overshoot: 1.1
						}

						ClippingRectangle {
							id: whTile
							anchors.fill: parent
							radius: 8
							color: col.surfaceContainerHigh

								Image {
									id: whThumb
									anchors.fill: parent
									source: modelData.thumb
									fillMode: Image.PreserveAspectCrop
									asynchronous: true
									smooth: true
								}

								// Placeholder shown while loading
								Rectangle {
									anchors.fill: parent
									color: col.surfaceContainer
									visible: whThumb.status !== Image.Ready
									radius: parent.radius

									MaterialSymbol {
										anchors.centerIn: parent
										icon: "image"
										iconSize: 28
										color: col.onSurfaceVariant
									}
								}

								// Hover overlay
								Rectangle {
									anchors.fill: parent
									color: Qt.rgba(0, 0, 0, 0.5)
									radius: parent.radius
									opacity: whMouse.containsMouse ? 1 : 0
									Behavior on opacity {
										NumberAnimation { duration: Gstate.animDuration }
									}

									Column {
										anchors.centerIn: parent
										spacing: 5

										Rectangle {
											anchors.horizontalCenter: parent.horizontalCenter
											width: 80
											height: 26
											radius: 13
											color: col.primary

											Text {
												anchors.centerIn: parent
												text: "Apply"
												color: col.onPrimary
												font.pixelSize: 12
												font.family: cfg ? cfg.fontFamily : "Rubik"
												font.weight: Font.Medium
											}
										}

										Text {
											anchors.horizontalCenter: parent.horizontalCenter
											text: modelData.resolution
											color: "white"
											font.pixelSize: 10
											font.family: cfg ? cfg.fontFamily : "Rubik"
										}
									}
								}

								MouseArea {
									id: whMouse
									anchors.fill: parent
									hoverEnabled: true
									cursorShape: Qt.PointingHandCursor
									onClicked: downloadAndApplyWallhaven(modelData)
								}

								scale: whMouse.containsMouse ? 0.95 : 1.0
								Behavior on scale {
									NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutBack }
								}
							}
						}
					}
				}

				// Load more / loading row anchored below masonry canvas
				Item {
					y: wallhavenSection.masonryHeight + 8
					width: wallhavenFlickable.contentWidth
					height: 44
					visible: wallhavenResults.length > 0

					Rectangle {
						anchors.centerIn: parent
						width: 130
						height: 34
						radius: 17
						color: col.surfaceContainer
						visible: wallhavenHasMore && !isLoadingWallhaven

						Text {
							anchors.centerIn: parent
							text: "Load more"
							color: col.onSurfaceVariant
							font.pixelSize: 13
							font.family: cfg ? cfg.fontFamily : "Rubik"
						}

						MouseArea {
							anchors.fill: parent
							cursorShape: Qt.PointingHandCursor
							onClicked: wallhavenLoadMore()
						}
					}

					Text {
						anchors.centerIn: parent
						text: "Loading..."
						color: col.onSurfaceVariant
						font.pixelSize: 13
						font.family: cfg ? cfg.fontFamily : "Rubik"
						visible: isLoadingWallhaven
					}
				}
			}
		}

		// Pexels Grid
		Item {
			id: pexelsSection
			visible: isPexelsMode
			anchors {
				top: searchBox.bottom
				topMargin: 5
				left: parent.left
				leftMargin: 5
				right: parent.right
				rightMargin: 5
				bottom: parent.bottom
				bottomMargin: 5
			}

			Text {
				anchors.centerIn: parent
				text: isLoadingPexels
					? "Searching Pexels..."
					: (pexelsApiKey().length === 0
						? "Add Pexels API key in Settings"
						: (pexelsQuery === ""
							? "Type to search wallpapers"
							: (pexelsResults.length === 0 ? "No results" : "")))
				color: col.onSurfaceVariant
				font.pixelSize: 14
				font.family: cfg ? cfg.fontFamily : "Rubik"
				visible: pexelsResults.length === 0
			}

			property int colCount: 4
			property int gap: 4
			property real colWidth: (width - gap * (colCount - 1)) / colCount
			property var layout: []
			property real masonryHeight: 0

			function computeLayout() {
				var cols = colCount
				var cw = colWidth
				var g = gap
				var colHeights = []
				for (var c = 0; c < cols; c++) colHeights[c] = 0
				var positions = []
				for (var i = 0; i < pexelsResults.length; i++) {
					var item = pexelsResults[i]
					var ratio = item.ratio > 0 ? item.ratio : 16/9
					var th = cw / ratio
					var shortest = 0
					for (var c2 = 1; c2 < cols; c2++) {
						if (colHeights[c2] < colHeights[shortest]) shortest = c2
					}
					positions.push({ x: shortest * (cw + g), y: colHeights[shortest], w: cw, h: th })
					colHeights[shortest] += th + g
				}
				var maxH = 0
				for (var c3 = 0; c3 < cols; c3++) {
					if (colHeights[c3] > maxH) maxH = colHeights[c3]
				}
				layout = positions
				masonryHeight = maxH
			}

			onWidthChanged: if (pexelsResults.length > 0) computeLayout()

			Connections {
				target: root
				function onUnsplashResultsChanged() { pexelsSection.computeLayout() }
			}

			Flickable {
				id: pexelsFlickable
				anchors.fill: parent
				clip: true
				contentWidth: width
				contentHeight: pexelsSection.masonryHeight + 60
				boundsBehavior: Flickable.StopAtBounds
				ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

				Item {
					width: pexelsFlickable.contentWidth
					height: pexelsSection.masonryHeight

					Repeater {
						model: pexelsResults

						delegate: Item {
							id: pexelsDelegate
							property var pos: pexelsSection.layout[index] || { x: 0, y: 0, w: 0, h: 0 }
							x: pos.x
							y: pos.y
							width: pos.w
							height: pos.h

							transformOrigin: Item.BottomRight
							scale: 0
							Component.onCompleted: {
								upopInTimer.interval = index * 30
								upopInTimer.start()
							}
							Timer {
								id: upopInTimer
								repeat: false
								onTriggered: upopInAnim.start()
							}
							NumberAnimation {
								id: upopInAnim
								target: pexelsDelegate
								property: "scale"
								from: 0; to: 1
								duration: 220
								easing.type: Easing.OutBack
								easing.overshoot: 1.1
							}

							ClippingRectangle {
								anchors.fill: parent
								radius: 8
								color: col.surfaceContainerHigh

								Image {
									id: pexelsThumb
									anchors.fill: parent
									source: modelData.thumb
									fillMode: Image.PreserveAspectCrop
									asynchronous: true
									smooth: true
								}

								Rectangle {
									anchors.fill: parent
									color: col.surfaceContainer
									visible: pexelsThumb.status !== Image.Ready
									radius: parent.radius
									MaterialSymbol {
										anchors.centerIn: parent
										icon: "photo_camera"
										iconSize: 28
										color: col.onSurfaceVariant
									}
								}

								Rectangle {
									anchors.fill: parent
									color: Qt.rgba(0, 0, 0, 0.5)
									radius: parent.radius
									opacity: pexelsMouse.containsMouse ? 1 : 0
									Behavior on opacity {
										NumberAnimation { duration: Gstate.animDuration }
									}

									Column {
										anchors.centerIn: parent
										spacing: 5

										Rectangle {
											anchors.horizontalCenter: parent.horizontalCenter
											width: 80; height: 26; radius: 13
											color: col.primary
											Text {
												anchors.centerIn: parent
												text: "Apply"
												color: col.onPrimary
												font.pixelSize: 12
												font.family: cfg ? cfg.fontFamily : "Rubik"
												font.weight: Font.Medium
											}
										}

										Text {
											anchors.horizontalCenter: parent.horizontalCenter
											text: modelData.resolution
											color: "white"
											font.pixelSize: 10
											font.family: cfg ? cfg.fontFamily : "Rubik"
										}

										Text {
											anchors.horizontalCenter: parent.horizontalCenter
											text: modelData.author ? "by " + modelData.author : ""
											color: Qt.rgba(1, 1, 1, 0.7)
											font.pixelSize: 9
											font.family: cfg ? cfg.fontFamily : "Rubik"
											visible: modelData.author && modelData.author.length > 0
										}
									}
								}

								MouseArea {
									id: pexelsMouse
									anchors.fill: parent
									hoverEnabled: true
									cursorShape: Qt.PointingHandCursor
									onClicked: downloadAndApplyPexels(modelData)
								}

								scale: pexelsMouse.containsMouse ? 0.95 : 1.0
								Behavior on scale {
									NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutBack }
								}
							}
						}
					}
				}

				Item {
					y: pexelsSection.masonryHeight + 8
					width: pexelsFlickable.contentWidth
					height: 44
					visible: pexelsResults.length > 0

					Rectangle {
						anchors.centerIn: parent
						width: 130; height: 34; radius: 17
						color: col.surfaceContainer
						visible: unsplashHasMore && !isLoadingPexels
						Text {
							anchors.centerIn: parent
							text: "Load more"
							color: col.onSurfaceVariant
							font.pixelSize: 13
							font.family: cfg ? cfg.fontFamily : "Rubik"
						}
						MouseArea {
							anchors.fill: parent
							cursorShape: Qt.PointingHandCursor
							onClicked: unsplashLoadMore()
						}
					}

					Text {
						anchors.centerIn: parent
						text: "Loading..."
						color: col.onSurfaceVariant
						font.pixelSize: 13
						font.family: cfg ? cfg.fontFamily : "Rubik"
						visible: isLoadingPexels
					}
				}
			}
		}

		// Wallpaper Grid
		ScrollView {
			id: wallpaperScrollView
			clip: true
			visible: isWallpaperMode
			anchors {
				top: searchBox.bottom
				topMargin: 5
				left: parent.left
				leftMargin: 5
				right: parent.right
				rightMargin: 5
				bottom: parent.bottom
				bottomMargin: 5
			}

			GridView {
				id: wallpaperGrid
				cellWidth: 125
				cellHeight: 110
				model: wallpapers.filter(w => matchesWallpaperSearch(w))

				delegate: Rectangle {
					width: 120
					height: 100
					radius: 12
					color: wpMouseArea.containsMouse ? col.surfaceContainer : "transparent"

					scale: wpMouseArea.containsMouse ? 0.95 : 1.0
					Behavior on scale {
						NumberAnimation {
							duration: Gstate.animDuration
							easing.type: Easing.OutBack
						}
					}

					ColumnLayout {
						anchors.fill: parent
						anchors.margins: 5
						spacing: 4

						Rectangle {
							Layout.fillWidth: true
							Layout.preferredHeight: 70
							radius: 8
							color: col.surfaceContainerHigh
							clip: true

							Image {
								id: thumbImage
								anchors.fill: parent
								anchors.margins: 2
								source: wallpaperDir + "/" + modelData
								fillMode: Image.PreserveAspectCrop
								asynchronous: true
								sourceSize.width: 120
								sourceSize.height: 70

								Rectangle {
									anchors.fill: parent
									color: col.surfaceContainer
									visible: thumbImage.status !== Image.Ready

									MaterialSymbol {
										icon: "image"
										iconSize: 32
										anchors.centerIn: parent
										color: col.onSurfaceVariant
									}
								}
							}
						}

						Text {
							Layout.fillWidth: true
							text: trimFilename(modelData, 16)
							color: col.onSurface
							font.pixelSize: 10
							font.family: cfg ? cfg.fontFamily : "Rubik"
							horizontalAlignment: Text.AlignHCenter
							elide: Text.ElideRight
						}
					}

					MouseArea {
						id: wpMouseArea
						anchors.fill: parent
						hoverEnabled: true
						cursorShape: Qt.PointingHandCursor
						onClicked: applyWallpaper(modelData)
					}
				}

				// Empty state
				Text {
					anchors.centerIn: parent
					text: isLoadingWallpapers ? "Loading wallpapers..." : (wallpapers.length === 0 ? "No wallpapers found" : "No matches")
					color: col.onSurfaceVariant
					font.pixelSize: 14
					visible: wallpaperGrid.count === 0
				}
			}
		}

		// Emoji List
		ScrollView {
			id: emojiScrollView
			clip: true
			visible: isEmojiMode
			anchors {
				top: searchBox.bottom
				topMargin: 5
				left: parent.left
				leftMargin: 5
				right: parent.right
				rightMargin: 5
				bottom: parent.bottom
				bottomMargin: 5
			}

			ListView {
				id: emojiListView
				model: getFilteredEmojis()
				currentIndex: 0
				highlightFollowsCurrentItem: true
				spacing: 2

				delegate: Rectangle {
					id: emojiDelegate
					width: ListView.view.width
					height: 50
					radius: emojiMouseArea.containsMouse ? 50 : 12
					color: ListView.isCurrentItem || emojiMouseArea.containsMouse ? col.surfaceContainer : "transparent"

					scale: emojiMouseArea.containsMouse ? 0.97 : 1.0
					Behavior on scale { NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutBack } }
					Behavior on color { ColorAnimation { duration: Gstate.animDuration } }
					Behavior on radius { NumberAnimation { duration: Gstate.animDuration } }

					RowLayout {
						anchors.left: parent.left
						anchors.right: parent.right
						anchors.verticalCenter: parent.verticalCenter
						anchors.margins: 8
						spacing: 12

						Rectangle {
							Layout.preferredWidth: 36
							Layout.preferredHeight: 36
							radius: 10
							color: col.primaryContainer

							Text {
								anchors.centerIn: parent
								text: modelData.emoji
								font.pixelSize: 22
							}
						}

						ColumnLayout {
							Layout.fillWidth: true
							spacing: 2

							Text {
								Layout.fillWidth: true
								text: modelData.name ? modelData.name : (modelData.en ? modelData.en[0] : modelData.emoji)
								color: col.onSurface
								font.pixelSize: 14
								font.family: cfg ? cfg.fontFamily : "Rubik"
								font.weight: 500
								elide: Text.ElideRight
							}

							Text {
								Layout.fillWidth: true
								text: modelData.keywords ? modelData.keywords.slice(0, 5).join(", ") : (modelData.en ? modelData.en.slice(0, 5).join(", ") : "")
								color: col.onSurfaceVariant
								font.pixelSize: 11
								font.family: cfg ? cfg.fontFamily : "Rubik"
								elide: Text.ElideRight
								visible: text !== ""
							}
						}

						Rectangle {
							width: 50
							height: 25
							color: col.primary
							radius: 12
							visible: emojiMouseArea.containsMouse || emojiDelegate.ListView.isCurrentItem

							Text {
								anchors.centerIn: parent
								text: "Copy"
								color: col.onPrimary
								font.pixelSize: 12
								font.family: cfg ? cfg.fontFamily : "Rubik"
								font.weight: 500
							}
						}
					}

					MouseArea {
						id: emojiMouseArea
						anchors.fill: parent
						hoverEnabled: true
						cursorShape: Qt.PointingHandCursor
						onClicked: {
							emojiListView.currentIndex = index
							copyEmoji(modelData.emoji)
						}
					}
				}

				Text {
					anchors.centerIn: parent
					text: isLoadingEmojis ? "Loading emojis..." : (emojis.length === 0 ? "No emojis found" : "No matches")
					color: col.onSurfaceVariant
					font.pixelSize: 14
					visible: emojiListView.count === 0
				}
			}
		}

		// Clipboard List
		ScrollView {
			id: clipboardScrollView
			clip: true
			visible: isClipboardMode
			anchors {
				top: searchBox.bottom
				topMargin: 5
				left: parent.left
				leftMargin: 5
				right: parent.right
				rightMargin: 5
				bottom: parent.bottom
				bottomMargin: 5
			}

			ListView {
				id: clipboardListView
				model: getFilteredClipboard()
				currentIndex: 0
				highlightFollowsCurrentItem: true
				spacing: 2

				delegate: Rectangle {
					id: clipDelegate
					width: ListView.view.width
					height: 55
					radius: clipMouseArea.containsMouse ? 50 : 12
					color: ListView.isCurrentItem || clipMouseArea.containsMouse ? col.surfaceContainer : "transparent"

					scale: clipMouseArea.containsMouse ? 0.97 : 1.0
					Behavior on scale { NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutBack } }
					Behavior on color { ColorAnimation { duration: Gstate.animDuration } }
					Behavior on radius { NumberAnimation { duration: Gstate.animDuration } }

					RowLayout {
						anchors.fill: parent
						anchors.margins: 8
						spacing: 10

						Rectangle {
							Layout.preferredWidth: 40
							Layout.preferredHeight: 40
							radius: 8
							color: col.primaryContainer

							MaterialSymbol {
								anchors.centerIn: parent
								icon: modelData.isImage ? "image" : "content_paste"
								iconSize: 22
								color: col.onPrimaryContainer
							}
						}

						ColumnLayout {
							Layout.fillWidth: true
							spacing: 2

							Text {
								Layout.fillWidth: true
								text: modelData.isImage ? "[Image]" : modelData.content.replace(/\n/g, " ").substring(0, 60)
								color: col.onSurface
								font.pixelSize: 13
								font.family: cfg ? cfg.fontFamily : "Rubik"
								elide: Text.ElideRight
							}

							Text {
								Layout.fillWidth: true
								text: modelData.isImage ? "Binary image data" : (modelData.content.length > 60 ? modelData.content.replace(/\n/g, " ").substring(60, 120) + "..." : "")
								color: col.onSurfaceVariant
								font.pixelSize: 11
								font.family: cfg ? cfg.fontFamily : "Rubik"
								elide: Text.ElideRight
								visible: text !== ""
							}
						}

						Rectangle {
							width: 55
							height: 25
							color: col.primary
							radius: 12
							visible: clipMouseArea.containsMouse || clipDelegate.ListView.isCurrentItem

							Text {
								anchors.centerIn: parent
								text: "Paste"
								color: col.onPrimary
								font.pixelSize: 12
								font.family: cfg ? cfg.fontFamily : "Rubik"
								font.weight: 500
							}
						}
					}

					MouseArea {
						id: clipMouseArea
						anchors.fill: parent
						hoverEnabled: true
						cursorShape: Qt.PointingHandCursor
						onClicked: {
							clipboardListView.currentIndex = index
							pasteClipboardItem(modelData.id)
						}
					}
				}

				Text {
					anchors.centerIn: parent
					text: isLoadingClipboard ? "Loading clipboard..." : (clipboardItems.length === 0 ? "Clipboard empty" : "No matches")
					color: col.onSurfaceVariant
					font.pixelSize: 14
					visible: clipboardListView.count === 0
				}
			}
		}
	}

	GlobalShortcut {
		name: "LauncherToggle"
		description: "Show the application launcher"
		appid: "quickshell"
		onPressed: Gstate.appsOpen = !Gstate.appsOpen
	}

	GlobalShortcut {
		name: "LauncherUntoggle"
		description: "Close the application launcher"
		appid: "quickshell"
		onPressed: Gstate.appsOpen = false
	}

	Component.onCompleted: {
		updateFilteredApps()
	}
}
