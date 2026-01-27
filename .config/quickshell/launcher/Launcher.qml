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
	width: cfg ? cfg.launcherWidth + 20 : 420
	height: cfg ? cfg.launcherHeight : 540
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
		filteredApps = apps
		visibleAppCount = apps.length
	}
	
	// Mode flags
	property bool isWallpaperMode: false
	property bool isEmojiMode: false
	property bool isClipboardMode: false
	property bool isMathMode: false
	property string mathResult: ""
	
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
	property string appSearchTerm: (isWallpaperMode || isEmojiMode || isClipboardMode) ? "" : searchTerm

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
		matugenProcess.command = ["matugen", "image", "-t", "scheme-tonal-spot", fullPath]
		matugenProcess.running = true
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
		onExited: (exitCode, exitStatus) => {
			if (exitCode === 0) {
				console.log("Wallpaper applied successfully")
				searchField.text = ""
				Gstate.appsOpen = false
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

	function matchesSearch(app, term) {
		if (!term || term === "") return true
		var search = term.toLowerCase()
		if (app.name && app.name.toLowerCase().includes(search)) return true
		if (app.genericName && app.genericName.toLowerCase().includes(search)) return true
		if (app.comment && app.comment.toLowerCase().includes(search)) return true
		return false
	}

	onVisibleChanged: {
		if (visible) {
			searchField.forceActiveFocus()
			updateFilteredApps()
			appsListView.currentIndex = 0
		} else {
			searchField.text = ""
		}
	}

	function getContainerHeight() {
		var searchBarHeight = 50 // search bar + margins
		var maxHeight = maxItems * itemHeight + searchBarHeight
		
		if (isWallpaperMode) {
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
		width: launcherWidth
		height: getContainerHeight()
		radius: launcherRadius
		color: col.background
		anchors.centerIn: parent

		Behavior on height {
			NumberAnimation {
				duration: 200
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
					icon: isWallpaperMode ? "image" : (isEmojiMode ? "emoji_emotions" : (isClipboardMode ? "content_paste" : "search"))
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
				placeholderText: isWallpaperMode ? "Search wallpapers..." : (isEmojiMode ? "Search emojis..." : (isClipboardMode ? "Search clipboard..." : "Search apps..."))
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
			
			// Check for commands
			if (trimmed.startsWith("/wallpaper") && wallpaperModeEnabled) {
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
			visible: !isWallpaperMode && !isEmojiMode && !isClipboardMode
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
							duration: 200
							easing.type: Easing.OutBack
						}
					}

					Behavior on color {
						ColorAnimation { duration: 150 }
					}

					Behavior on radius {
						NumberAnimation { duration: 150 }
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
							duration: 150
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
					Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
					Behavior on color { ColorAnimation { duration: 150 } }
					Behavior on radius { NumberAnimation { duration: 150 } }

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
					Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
					Behavior on color { ColorAnimation { duration: 150 } }
					Behavior on radius { NumberAnimation { duration: 150 } }

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
