import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Quickshell
import Quickshell.Io
import Quickshell.Widgets
import qs.widgets
import qs.services

Item {
	id: lookFeelPage

	property int darkModeIndex: 0
	property int schemeIndex: 7
	property var colorSchemes: ["Content","Expressive","Fidelity","Fruit Salad","Monochrome","Neutral","Rainbow","Tonal Spot"]
	property var schemeMapping: ["content","expressive","fidelity","fruit-salad","monochrome","neutral","rainbow","tonal-spot"]

	// Provider switcher
	property string wallpaperProvider: "wallhaven"  // "wallhaven" | "pexels"

	// Pexels state
	property var pexelsResults: []
	property bool pexelsLoading: false
	property string pexelsQuery: ""
	property string pexelsRawData: ""
	property int pexelsPage: 1
	property bool pexelsHasMore: false

	// Pexels masonry
	property var pexelsLayout: []
	property real pexelsMasonryHeight: 0

	function computePexelsLayout() {
		var cols = whColCount
		var cw = (whGrid.width - whGap * (cols - 1)) / cols
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
			positions.push({ x: shortest * (cw + whGap), y: colHeights[shortest], w: cw, h: th })
			colHeights[shortest] += th + whGap
		}
		var maxH = 0
		for (var c3 = 0; c3 < cols; c3++) {
			if (colHeights[c3] > maxH) maxH = colHeights[c3]
		}
		pexelsLayout = positions
		pexelsMasonryHeight = maxH
	}

	onPexelsResultsChanged: computePexelsLayout()

	Process {
		id: pexelsSearchProc
		stdout: SplitParser {
			splitMarker: ""
			onRead: data => { lookFeelPage.pexelsRawData += data }
		}
		onExited: (code, status) => {
			lookFeelPage.pexelsLoading = false
			if (code === 0 && lookFeelPage.pexelsRawData.length > 0) {
				try {
					var parsed = JSON.parse(lookFeelPage.pexelsRawData)
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
					if (lookFeelPage.pexelsPage === 1) {
						lookFeelPage.pexelsResults = newResults
					} else {
						lookFeelPage.pexelsResults = lookFeelPage.pexelsResults.concat(newResults)
					}
					lookFeelPage.pexelsHasMore = (parsed.next_page !== undefined && parsed.next_page !== null && parsed.next_page !== "")
				} catch(e) {
					console.error("Pexels parse error:", e)
				}
			}
		}
	}

	function pexelsSearch(q) {
		var key = setupCfg ? setupCfg.pexelsApiKey : ""
		if (key.length === 0) return
		pexelsQuery = q
		pexelsPage = 1
		pexelsResults = []
		pexelsRawData = ""
		pexelsLoading = true
		pexelsSearchProc.command = [
			"curl", "-s", "--max-time", "10",
			"-H", "Authorization: " + key,
			"https://api.pexels.com/v1/search?query=" + encodeURIComponent(q) +
			"&per_page=20&page=1&orientation=landscape"
		]
		pexelsSearchProc.running = true
	}

	function pexelsLoadMore() {
		var key = setupCfg ? setupCfg.pexelsApiKey : ""
		if (pexelsLoading || !pexelsHasMore || key.length === 0) return
		pexelsPage++
		pexelsRawData = ""
		pexelsLoading = true
		pexelsSearchProc.command = [
			"curl", "-s", "--max-time", "10",
			"-H", "Authorization: " + key,
			"https://api.pexels.com/v1/search?query=" + encodeURIComponent(pexelsQuery) +
			"&per_page=20&page=" + pexelsPage + "&orientation=landscape"
		]
		pexelsSearchProc.running = true
	}

	// Wallhaven state -- same fields as Launcher
	property var whResults: []
	property bool whLoading: false
	property string whQuery: ""
	property string whRawData: ""
	property int whPage: 1
	property int whLastPage: 1
	property bool whHasMore: false

	// Masonry layout state -- same as wallhavenSection in Launcher
	property int whColCount: 4
	property int whGap: 4
	property real whColWidth: (whGrid.width - whGap * (whColCount - 1)) / whColCount
	property var whLayout: []
	property real whMasonryHeight: 0

	function computeWhLayout() {
		var cols = whColCount
		var cw = whColWidth
		var g = whGap
		var colHeights = []
		for (var c = 0; c < cols; c++) colHeights[c] = 0
		var positions = []
		for (var i = 0; i < whResults.length; i++) {
			var item = whResults[i]
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
		whLayout = positions
		whMasonryHeight = maxH
	}

	onWhResultsChanged: computeWhLayout()

	// Fetch process -- identical to wallhavenSearchProcess in Launcher
	Process {
		id: whSearchProc
		stdout: SplitParser {
			splitMarker: ""
			onRead: data => { lookFeelPage.whRawData += data }
		}
		onExited: (code, status) => {
			lookFeelPage.whLoading = false
			if (code === 0 && lookFeelPage.whRawData.length > 0) {
				try {
					var parsed = JSON.parse(lookFeelPage.whRawData)
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
					if (lookFeelPage.whPage === 1) {
						lookFeelPage.whResults = newResults
					} else {
						lookFeelPage.whResults = lookFeelPage.whResults.concat(newResults)
					}
					lookFeelPage.whLastPage = meta.last_page || 1
					lookFeelPage.whHasMore = lookFeelPage.whPage < lookFeelPage.whLastPage
				} catch(e) {
					console.error("WH parse error:", e)
				}
			}
		}
	}

	function whSearch(q) {
		whQuery = q
		whPage = 1
		whResults = []
		whRawData = ""
		whLoading = true
		whSearchProc.command = [
			"curl", "-s", "--max-time", "10",
			"https://wallhaven.cc/api/v1/search?q=" + encodeURIComponent(q) +
			"&categories=111&purity=100&sorting=relevance&order=desc&ratios=16x9,16x10&page=1"
		]
		whSearchProc.running = true
	}

	function whLoadMore() {
		if (whLoading || !whHasMore) return
		whPage++
		whRawData = ""
		whLoading = true
		whSearchProc.command = [
			"curl", "-s", "--max-time", "10",
			"https://wallhaven.cc/api/v1/search?q=" + encodeURIComponent(whQuery) +
			"&categories=111&purity=100&sorting=relevance&order=desc&ratios=16x9,16x10&page=" + whPage
		]
		whSearchProc.running = true
	}

	// Download + apply wallpaper
	Process { id: applyWpProc }

	function applyWallpaper(wallpaper) {
		var ext = wallpaper.path.split(".").pop()
		var dest = Quickshell.env("HOME") + "/.local/wallpapers/wallhaven-" + wallpaper.id + "." + ext
		var genScript = Qt.resolvedUrl("../col_gen/generate").toString().replace("file://", "")
		setupCfg.matugenMode = darkModeIndex === 0 ? "dark" : "light"
		setupCfg.matugenScheme = schemeMapping[schemeIndex]
		applyWpProc.command = [
			"sh", "-c",
			"curl -sL --max-time 60 -o " + JSON.stringify(dest) + " " + JSON.stringify(wallpaper.path) +
			" && " + JSON.stringify(genScript) + " image " + JSON.stringify(dest) +
			" -m " + setupCfg.matugenMode + " -s " + setupCfg.matugenScheme + " -c 0.0"
		]
		applyWpProc.running = true
	}

	Timer {
		id: whDebounce
		interval: 500
		repeat: false
		onTriggered: {
			var q = whSearchField.text.trim()
			if (q.length === 0) return
			if (wallpaperProvider === "pexels") {
				lookFeelPage.pexelsSearch(q)
			} else {
				lookFeelPage.whSearch(q)
			}
		}
	}

	ColumnLayout {
		anchors.fill: parent
		spacing: 16

		// Top strip: theme + scheme chips side by side
		RowLayout {
			Layout.fillWidth: true
			spacing: 12

			// Theme card
			Rectangle {
				height: modeCol.implicitHeight + 24
				Layout.preferredWidth: 220
				radius: 14
				color: col.surfaceContainer

				ColumnLayout {
					id: modeCol
					anchors {
						left: parent.left
						right: parent.right
						top: parent.top
						margins: 12
					}
					spacing: 8

					RowLayout {
						spacing: 8
						MaterialSymbol { icon: "dark_mode"; iconSize: 18; color: col.primary }
						Text {
							text: "Theme"
							font.pixelSize: 13
							font.weight: 700
							font.family: cfg ? cfg.fontFamily : "Rubik"
							color: col.onSurface
						}
					}

					RowLayout {
						spacing: 6
						Repeater {
							model: [{ n: "Dark", i: "dark_mode" }, { n: "Light", i: "light_mode" }]
							Rectangle {
								height: 32
								width: modeChipRow.implicitWidth + 16
								radius: index === darkModeIndex ? 16 : 8
								color: index === darkModeIndex ? col.primary : col.surfaceContainerHigh
								Behavior on color { ColorAnimation { duration: 150 } }
								Behavior on radius { NumberAnimation { duration: 150 } }
								RowLayout {
									id: modeChipRow
									anchors.centerIn: parent
									spacing: 5
									MaterialSymbol {
										icon: modelData.i
										iconSize: 14
										color: index === darkModeIndex ? col.onPrimary : col.onSurfaceVariant
									}
									Text {
										text: modelData.n
										font.pixelSize: 12
										font.family: cfg ? cfg.fontFamily : "Rubik"
										font.weight: 600
										color: index === darkModeIndex ? col.onPrimary : col.onSurfaceVariant
									}
								}
								MouseArea {
									anchors.fill: parent
									cursorShape: Qt.PointingHandCursor
									onClicked: {
										darkModeIndex = index
										setupCfg.matugenMode = index === 0 ? "dark" : "light"
										if (col && col.wallpaper && col.wallpaper !== "") {
											var genScript = Qt.resolvedUrl("../col_gen/generate").toString().replace("file://", "")
											matugenProcess.command = [genScript, "image", col.wallpaper, "-m", setupCfg.matugenMode, "-s", setupCfg.matugenScheme, "-c", "0.0"]
											matugenProcess.running = true
										}
									}
								}
							}
						}
					}
				}
			}

			// Scheme card
			Rectangle {
				Layout.fillWidth: true
				height: schemeCol.implicitHeight + 24
				radius: 14
				color: col.surfaceContainer

				ColumnLayout {
					id: schemeCol
					anchors {
						left: parent.left
						right: parent.right
						top: parent.top
						margins: 12
					}
					spacing: 8

					RowLayout {
						spacing: 8
						MaterialSymbol { icon: "palette"; iconSize: 18; color: col.primary }
						Text {
							text: "Color Scheme"
							font.pixelSize: 13
							font.weight: 700
							font.family: cfg ? cfg.fontFamily : "Rubik"
							color: col.onSurface
						}
					}

					Flow {
						Layout.fillWidth: true
						spacing: 5
						Repeater {
							model: colorSchemes
							Rectangle {
								height: 28
								width: schemeChipText.implicitWidth + 14
								radius: index === schemeIndex ? 14 : 7
								color: index === schemeIndex ? col.primaryContainer : col.surfaceContainerHigh
								Behavior on color { ColorAnimation { duration: 150 } }
								Behavior on radius { NumberAnimation { duration: 150 } }
								Text {
									id: schemeChipText
									anchors.centerIn: parent
									text: modelData
									font.pixelSize: 11
									font.family: cfg ? cfg.fontFamily : "Rubik"
									font.weight: 600
									color: index === schemeIndex ? col.onPrimaryContainer : col.onSurfaceVariant
								}
								MouseArea {
									anchors.fill: parent
									cursorShape: Qt.PointingHandCursor
									onClicked: {
										schemeIndex = index
										setupCfg.matugenScheme = schemeMapping[index]
										if (col && col.wallpaper && col.wallpaper !== "") {
											var genScript = Qt.resolvedUrl("../col_gen/generate").toString().replace("file://", "")
											matugenProcess.command = [genScript, "image", col.wallpaper, "-m", setupCfg.matugenMode, "-s", setupCfg.matugenScheme, "-c", "0.0"]
											matugenProcess.running = true
										}
									}
								}
							}
						}
					}
				}
			}
		}

		// Provider chips + search bar row
		RowLayout {
			Layout.fillWidth: true
			spacing: 8

			// Provider chips
			Repeater {
				model: [
					{ label: "Wallhaven", value: "wallhaven", icon: "wallpaper" },
					{ label: "Pexels",  value: "pexels",  icon: "photo_camera" }
				]
				Rectangle {
					height: 44
					width: providerRow.implicitWidth + 20
					radius: 22
					color: wallpaperProvider === modelData.value ? col.primary : col.surfaceContainer
					Behavior on color { ColorAnimation { duration: 150 } }
					RowLayout {
						id: providerRow
						anchors.centerIn: parent
						spacing: 6
						MaterialSymbol {
							icon: modelData.icon
							iconSize: 16
							color: wallpaperProvider === modelData.value ? col.onPrimary : col.onSurfaceVariant
						}
						Text {
							text: modelData.label
							font.pixelSize: 13
							font.family: cfg ? cfg.fontFamily : "Rubik"
							font.weight: 600
							color: wallpaperProvider === modelData.value ? col.onPrimary : col.onSurfaceVariant
						}
					}
					MouseArea {
						anchors.fill: parent
						cursorShape: Qt.PointingHandCursor
						onClicked: {
							wallpaperProvider = modelData.value
							whDebounce.restart()
						}
					}
				}
			}

			// Search field
			Rectangle {
				Layout.fillWidth: true
				height: 44
				radius: 22
				color: col.surfaceContainer
				border.width: whSearchField.activeFocus ? 2 : 0
				border.color: col.primary
				Behavior on border.width { NumberAnimation { duration: 150 } }

				RowLayout {
					anchors {
						fill: parent
						leftMargin: 14
						rightMargin: 14
					}
					spacing: 8
					MaterialSymbol {
						icon: (whLoading || pexelsLoading) ? "hourglass_empty" : "search"
						iconSize: 20
						color: col.onSurfaceVariant
					}
					TextField {
						id: whSearchField
						Layout.fillWidth: true
						placeholderText: wallpaperProvider === "pexels" ? "Search Pexels..." : "Search Wallhaven..."
						placeholderTextColor: col.onSurfaceVariant
						color: col.onSurface
						font.pixelSize: 14
						font.family: cfg ? cfg.fontFamily : "Rubik"
						background: null
						verticalAlignment: Text.AlignVCenter
						onTextChanged: whDebounce.restart()
					}
				}
			}
		}

		// Wallpaper grid area
		Item {
			id: whGrid
			Layout.fillWidth: true
			Layout.fillHeight: true

			onWidthChanged: {
				if (whResults.length > 0) lookFeelPage.computeWhLayout()
				if (pexelsResults.length > 0) lookFeelPage.computePexelsLayout()
			}

			// Empty / loading state
			Text {
				anchors.centerIn: parent
				text: {
					if (wallpaperProvider === "pexels") {
						var key = setupCfg ? setupCfg.pexelsApiKey : ""
						if (key.length === 0) return "Add Pexels API key in Settings"
						if (pexelsLoading) return "Searching Pexels..."
						if (pexelsQuery === "") return "Type to search wallpapers"
						return pexelsResults.length === 0 ? "No results" : ""
					}
					if (whLoading) return "Searching Wallhaven..."
					if (whQuery === "") return "Type to search wallpapers"
					return whResults.length === 0 ? "No results" : ""
				}
				color: col.onSurfaceVariant
				font.pixelSize: 14
				font.family: cfg ? cfg.fontFamily : "Rubik"
				visible: wallpaperProvider === "pexels" ? pexelsResults.length === 0 : whResults.length === 0
			}

			Flickable {
				id: whFlickable
				anchors.fill: parent
				clip: true
				visible: wallpaperProvider === "wallhaven"
				contentWidth: width
				contentHeight: whMasonryHeight + 60
				boundsBehavior: Flickable.StopAtBounds
				ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

				// Masonry canvas
				Item {
					width: whFlickable.contentWidth
					height: whMasonryHeight

					Repeater {
						model: whResults

						delegate: Item {
							id: whDelegate
							property var pos: whLayout[index] || { x: 0, y: 0, w: 0, h: 0 }
							x: pos.x
							y: pos.y
							width: pos.w
							height: pos.h

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

								// Placeholder while loading
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
										NumberAnimation { duration: 150 }
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
									onClicked: lookFeelPage.applyWallpaper(modelData)
								}

								scale: whMouse.containsMouse ? 0.95 : 1.0
								Behavior on scale {
									NumberAnimation { duration: 150; easing.type: Easing.OutBack }
								}
							}
						}
					}
				}

				// Load more / loading row
				Item {
					y: whMasonryHeight + 8
					width: whFlickable.contentWidth
					height: 44
					visible: whResults.length > 0

					Rectangle {
						anchors.centerIn: parent
						width: 130
						height: 34
						radius: 17
						color: col.surfaceContainer
						visible: whHasMore && !whLoading
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
							onClicked: lookFeelPage.whLoadMore()
						}
					}

					Text {
						anchors.centerIn: parent
						text: "Loading..."
						color: col.onSurfaceVariant
						font.pixelSize: 13
						font.family: cfg ? cfg.fontFamily : "Rubik"
						visible: whLoading && whResults.length > 0
					}
				}
			}

			// Pexels Flickable
			Flickable {
				id: pexelsFlickable
				anchors.fill: parent
				clip: true
				visible: wallpaperProvider === "pexels"
				contentWidth: width
				contentHeight: pexelsMasonryHeight + 60
				boundsBehavior: Flickable.StopAtBounds
				ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

				Item {
					width: pexelsFlickable.contentWidth
					height: pexelsMasonryHeight

					Repeater {
						model: pexelsResults

						delegate: Item {
							id: pexelsDelegate
							property var pos: pexelsLayout[index] || { x: 0, y: 0, w: 0, h: 0 }
							x: pos.x
							y: pos.y
							width: pos.w
							height: pos.h

							transformOrigin: Item.BottomRight
							scale: 0
							Component.onCompleted: {
								pexelsTimer.interval = index * 30
								pexelsTimer.start()
							}
							Timer { id: pexelsTimer; repeat: false; onTriggered: pexelsAnim.start() }
							NumberAnimation {
								id: pexelsAnim
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
									Behavior on opacity { NumberAnimation { duration: 150 } }

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
											text: modelData.author.length > 0 ? "by " + modelData.author : ""
											color: Qt.rgba(1, 1, 1, 0.7)
											font.pixelSize: 9
											font.family: cfg ? cfg.fontFamily : "Rubik"
											visible: modelData.author.length > 0
										}
									}
								}

								MouseArea {
									id: pexelsMouse
									anchors.fill: parent
									hoverEnabled: true
									cursorShape: Qt.PointingHandCursor
									onClicked: lookFeelPage.applyWallpaper(modelData)
								}

								scale: pexelsMouse.containsMouse ? 0.95 : 1.0
								Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }
							}
						}
					}
				}

				Item {
					y: pexelsMasonryHeight + 8
					width: pexelsFlickable.contentWidth
					height: 44
					visible: pexelsResults.length > 0

					Rectangle {
						anchors.centerIn: parent
						width: 130; height: 34; radius: 17
						color: col.surfaceContainer
						visible: pexelsHasMore && !pexelsLoading
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
							onClicked: lookFeelPage.pexelsLoadMore()
						}
					}

					Text {
						anchors.centerIn: parent
						text: "Loading..."
						color: col.onSurfaceVariant
						font.pixelSize: 13
						font.family: cfg ? cfg.fontFamily : "Rubik"
						visible: pexelsLoading && pexelsResults.length > 0
					}
				}
			}
		}
	}
}
