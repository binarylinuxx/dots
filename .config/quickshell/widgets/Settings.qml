import Quickshell
import Quickshell.Io
import Quickshell.Widgets
import Quickshell.Hyprland
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import qs.widgets
import qs.services

FloatingWindow {
	id: root
	width: 750
	height: 620
	color: "transparent"
	title: "Settings"
	visible: Gstate.settingsOpen

	property int selectedIndex: 0
	property var pageNames: ["Appearance", "Wallpaper", "Bar", "Launcher", "Clock", "Advanced", "About"]
	property int darkModeIndex: 0
	property int colorSchemeIndex: 7
	property int workspaceCount: 10

	property var colorSchemes: ["Content", "Expressive", "Fidelity", "Fruit Salad", 
								"Monochrome", "Neutral", "Rainbow", "Tonal Spot"]
	// col_gen scheme names (no "scheme-" prefix)
	property var schemeMapping: ["content", "expressive", "fidelity",
								"fruit-salad", "monochrome", "neutral",
								"rainbow", "tonal-spot", "vibrant"]

	property var clockPresets: [
		{ name: "12h Time", format: "hh:mm AP", id: "time12" },
		{ name: "24h Time", format: "HH:mm", id: "time24" },
		{ name: "12h + Date", format: "hh:mm AP | MMM d", id: "time12date" },
		{ name: "24h + Date", format: "HH:mm | MMM d", id: "time24date" },
		{ name: "Full Date", format: "ddd, MMM d | hh:mm AP", id: "full" },
		{ name: "Custom", format: "", id: "custom" }
	]

	property var animationSpeeds: [
		{ name: "Disabled", value: "disabled", multiplier: 0 },
		{ name: "Fast", value: "fast", multiplier: 0.5 },
		{ name: "Normal", value: "normal", multiplier: 1.0 },
		{ name: "Slow", value: "slow", multiplier: 1.5 }
	]

	property var workspaceStyles: [
		{ name: "Dots", value: "dots", icon: "radio_button_checked" },
		{ name: "Numbers", value: "numbers", icon: "123" },
		{ name: "Bars", value: "bars", icon: "view_week" }
	]

	property var fontFamilies: ["Bitcount Single", "Rubik", "Google Sans Flex", "Cantarell", "Fira Sans", "JetBrains Mono", "Noto Sans"]

	// Config file management
	FileView {
		id: configFile
		path: Qt.resolvedUrl("../config.json")
		watchChanges: true
		onFileChanged: reload()

		adapter: JsonAdapter {
			id: configAdapter
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
			property int launcherHeight: launcherMaxItems * launcherItemHeight + 70
			// Advanced / Desktop Widgets
			property bool desktopWidgets: true
			property int gridColumns: 16
			property int gridRows: 9
			property int widgetRadius: 12
			property int widgetBorderWidth: 1
			property string widgetBorderColor: ""
			property string widgetBackgroundColor: ""
			property real widgetOpacity: 0.85

			onLauncherMaxItemsChanged: launcherHeight = launcherMaxItems * launcherItemHeight + 70
			onLauncherItemHeightChanged: launcherHeight = launcherMaxItems * launcherItemHeight + 70
			onWorkspaceCountChanged: root.workspaceCount = workspaceCount
			onMatugenModeChanged: root.darkModeIndex = (matugenMode === "dark") ? 0 : 1
			onMatugenSchemeChanged: {
				for (var i = 0; i < schemeMapping.length; i++) {
					if (schemeMapping[i] === matugenScheme) {
						root.colorSchemeIndex = i
						break
					}
				}
			}
		}
	}

	Component.onCompleted: {
		configFile.reload()
	}

	function applyTheme() {
		if (col && col.wallpaper) {
			var mode = darkModeIndex === 0 ? "dark" : "light"
			var scheme = schemeMapping[colorSchemeIndex]
			
			if (configAdapter) {
				configAdapter.matugenMode = mode
				configAdapter.matugenScheme = scheme
				configFile.writeAdapter()
			}
			
			// Use col_gen instead of matugen
			var contrast = configAdapter ? configAdapter.matugenContrast : 0.0
			var genScript = Qt.resolvedUrl("../col_gen/generate").toString().replace("file://", "")
			matugenProcess.command = [
				genScript, "image", col.wallpaper,
				"-m", mode, "-s", scheme, "-c", contrast.toString()
			]
			matugenProcess.running = true
		}
	}

	function saveConfig() {
		if (configAdapter) {
			configFile.writeAdapter()
		}
	}

	function incrementWorkspaces() {
		if (configAdapter && workspaceCount < 20) {
			workspaceCount++
			configAdapter.workspaceCount = workspaceCount
			saveConfig()
		}
	}

	function decrementWorkspaces() {
		if (configAdapter && workspaceCount > 1) {
			workspaceCount--
			configAdapter.workspaceCount = workspaceCount
			saveConfig()
		}
	}

	Process {
		id: matugenProcess
	}

	Process {
		id: openUrlProcess
	}

	// Main container
	Rectangle {
		anchors.fill: parent
		radius: 20
		color: col.background
		clip: true

		RowLayout {
			anchors.fill: parent
			spacing: 0

			// Sidebar
			Rectangle {
				Layout.preferredWidth: 180
				Layout.fillHeight: true
				color: col.surfaceContainerLowest
				radius: 20

				Rectangle {
					anchors.right: parent.right
					width: 20
					height: parent.height
					color: parent.color
				}

				ColumnLayout {
					anchors.fill: parent
					anchors.margins: 12
					spacing: 6

					RowLayout {
						spacing: 10
						Layout.bottomMargin: 10

						Rectangle {
							width: 36
							height: 36
							radius: 10
							color: col.primaryContainer

							MaterialSymbol {
								icon: "settings"
								iconSize: 22
								anchors.centerIn: parent
								color: col.onPrimaryContainer
							}
						}

						Text {
							text: "Settings"
							font.pixelSize: 20
							font.family: configAdapter.fontFamily 
							font.weight: 700
							color: col.onSurface
						}
					}

					Repeater {
						model: root.pageNames

						Rectangle {
							Layout.fillWidth: true
							Layout.preferredHeight: 44
							radius: 12
							color: index === root.selectedIndex ? col.primaryContainer : (navMouseArea.containsMouse ? col.surfaceContainer : "transparent")

							Behavior on color {
								ColorAnimation { duration: 150 }
							}

							RowLayout {
								anchors.fill: parent
								anchors.leftMargin: 14
								spacing: 12

								MaterialSymbol {
									icon: index === 0 ? "palette" : index === 1 ? "wallpaper" : index === 2 ? "view_sidebar" : index === 3 ? "rocket_launch" : index === 4 ? "schedule" : index === 5 ? "science" : "info"
									iconSize: 22
									color: index === root.selectedIndex ? col.onPrimaryContainer : col.onSurfaceVariant
								}

								Text {
									text: modelData
									font.pixelSize: 14
									font.family: configAdapter.fontFamily 
									font.weight: 700
									color: index === root.selectedIndex ? col.onPrimaryContainer : col.onSurfaceVariant
								}
							}

							MouseArea {
								id: navMouseArea
								anchors.fill: parent
								cursorShape: Qt.PointingHandCursor
								hoverEnabled: true
								onClicked: root.selectedIndex = index
							}
						}
					}

					Item { Layout.fillHeight: true }

					Rectangle {
						Layout.fillWidth: true
						Layout.preferredHeight: 40
						radius: 20
						color: closeMouseArea.containsMouse ? col.errorContainer : col.surfaceContainer

						Behavior on color {
							ColorAnimation { duration: 150 }
						}

						RowLayout {
							anchors.centerIn: parent
							spacing: 8

							MaterialSymbol {
								icon: "close"
								iconSize: 18
								color: closeMouseArea.containsMouse ? col.onErrorContainer : col.onSurfaceVariant
							}

							Text {
								text: "Close"
								font.pixelSize: 13
								font.family: configAdapter.fontFamily 
								font.weight: 700
								color: closeMouseArea.containsMouse ? col.onErrorContainer : col.onSurfaceVariant
							}
						}

						MouseArea {
							id: closeMouseArea
							anchors.fill: parent
							cursorShape: Qt.PointingHandCursor
							hoverEnabled: true
							onClicked: Gstate.settingsOpen = false
						}
					}
				}
			}

			// Content area
			Rectangle {
				Layout.fillWidth: true
				Layout.fillHeight: true
				color: "transparent"

				StackLayout {
					id: stackLayout
					anchors.fill: parent
					anchors.margins: 25
					currentIndex: root.selectedIndex

					// === Appearance Page ===
					ScrollView {
						clip: true

						ColumnLayout {
							width: stackLayout.width - 50
							spacing: 25

							Text {
								text: "Appearance"
								font.pixelSize: 26
								font.family: configAdapter.fontFamily 
								font.weight: 700
								color: col.onSurface
							}

							// Wallpaper section
							ClippingRectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: wallpaperContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: wallpaperContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									RowLayout {
										spacing: 10
										MaterialSymbol { icon: "wallpaper"; iconSize: 22; color: col.primary }
										Text {
											text: "Wallpaper"
											font.pixelSize: 16
											font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
											font.weight: 700
											color: col.onSurface
										}
									}

									RowLayout {
										spacing: 20

										ClippingRectangle {
											Layout.preferredWidth: 180
											Layout.preferredHeight: 100
											radius: 12
											color: col.surfaceContainerHigh
											clip: true
											layer.enabled: true
											layer.smooth: true
											border.width: 1
											border.color: col.primary

											Image {
												anchors.fill: parent
												source: col.wallpaper ?? ""
												fillMode: Image.PreserveAspectCrop
												smooth: true

												Rectangle {
													anchors.fill: parent
													radius: 12
													color: col.surfaceContainerHigh
													visible: parent.status !== Image.Ready

													MaterialSymbol { 
														icon: "image"; 
														iconSize: 36; 
														anchors.centerIn: parent; 
														color: col.onSurfaceVariant 
													}
												}
											}

											MouseArea {
												anchors.fill: parent
												cursorShape: Qt.PointingHandCursor
												hoverEnabled: true
												onClicked: wallpaperDialog.open()

												Rectangle {
													anchors.fill: parent
													color: "black"
													opacity: parent.containsMouse ? 0.5 : 0
													radius: 12
													Behavior on opacity { NumberAnimation { duration: 150 } }
													MaterialSymbol { icon: "edit"; iconSize: 28; anchors.centerIn: parent; color: "white" }
												}
											}
										}

										ColumnLayout {
											spacing: 10

											Rectangle {
												Layout.preferredWidth: browseRow.width + 28
												Layout.preferredHeight: 40
												radius: 20
												color: col.primary

												RowLayout {
													id: browseRow
													anchors.centerIn: parent
													spacing: 8
													MaterialSymbol { icon: "folder_open"; iconSize: 18; color: col.onPrimary }
													Text { text: "Browse"; color: col.onPrimary; font.pixelSize: 13; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700 }
												}

												MouseArea {
													anchors.fill: parent
													cursorShape: Qt.PointingHandCursor
													onClicked: wallpaperDialog.open()
												}
											}

											Text {
												text: "Supports JPG, PNG, WebP"
												color: col.onSurfaceVariant
												font.pixelSize: 11
												font.family: configAdapter.fontFamily
												opacity: 0.8
											}
										}
									}
								}
							}

							// Theme Mode section
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: modeContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: modeContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									RowLayout {
										spacing: 10
										MaterialSymbol { icon: "dark_mode"; iconSize: 22; color: col.primary }
										Text { text: "Theme Mode"; font.pixelSize: 16; font.family: configAdapter.fontFamily; font.weight: 700; color: col.onSurface }
									}

									RowLayout {
										spacing: 10

										Repeater {
											model: [{ name: "Dark", icon: "dark_mode" }, { name: "Light", icon: "light_mode" }]

											Rectangle {
												width: modeItemRow.width + 24
												height: 42
												radius: index === root.darkModeIndex ? 21 : 10
												color: index === root.darkModeIndex ? col.primary : col.surfaceContainerHigh

												Behavior on radius { NumberAnimation { duration: 150 } }
												Behavior on color { ColorAnimation { duration: 150 } }

												RowLayout {
													id: modeItemRow
													anchors.centerIn: parent
													spacing: 8
													MaterialSymbol { icon: modelData.icon; iconSize: 20; color: index === root.darkModeIndex ? col.onPrimary : col.onSurfaceVariant }
													Text { text: modelData.name; color: index === root.darkModeIndex ? col.onPrimary : col.onSurfaceVariant; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700 }
												}

												MouseArea {
													anchors.fill: parent
													cursorShape: Qt.PointingHandCursor
													onClicked: { root.darkModeIndex = index; root.applyTheme() }
												}
											}
										}
									}
								}
							}

							// Color Scheme section
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: schemeContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: schemeContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									RowLayout {
										spacing: 10
										MaterialSymbol { icon: "format_paint"; iconSize: 22; color: col.primary }
										Text { text: "Color Scheme"; font.pixelSize: 16; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
									}

									Flow {
										Layout.fillWidth: true
										spacing: 8

										Repeater {
											model: root.colorSchemes

											Rectangle {
												width: schemeItemText.width + 22
												height: 36
												radius: index === root.colorSchemeIndex ? 18 : 10
												color: index === root.colorSchemeIndex ? col.primary : col.surfaceContainerHigh

												Behavior on radius { NumberAnimation { duration: 150 } }
												Behavior on color { ColorAnimation { duration: 150 } }

												Text {
													id: schemeItemText
													text: modelData
													anchors.centerIn: parent
													color: index === root.colorSchemeIndex ? col.onPrimary : col.onSurfaceVariant
													font.pixelSize: 13
													font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
													font.weight: 700
												}

												MouseArea {
													anchors.fill: parent
													cursorShape: Qt.PointingHandCursor
													onClicked: { root.colorSchemeIndex = index; root.applyTheme() }
												}
											}
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									// Contrast slider
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Contrast"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 500; color: col.onSurface }
											Text { 
												text: {
													var c = configAdapter ? configAdapter.matugenContrast : 0.0
													if (c < -0.3) return "Low contrast"
													if (c > 0.3) return "High contrast"
													return "Standard"
												}
												font.pixelSize: 11
												font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
												color: col.onSurfaceVariant
												opacity: 0.8
											}
										}

										StyledSlider {
											sliderWidth: 180
											from: -1.0
											to: 1.0
											stepSize: 0.1
											value: configAdapter ? configAdapter.matugenContrast : 0.0
											onValueChanged: {
												if (configAdapter && Math.abs(configAdapter.matugenContrast - value) > 0.01) {
													configAdapter.matugenContrast = value
													saveConfig()
												}
											}
										}

										Text {
											text: (configAdapter ? configAdapter.matugenContrast.toFixed(1) : "0.0")
											font.pixelSize: 12
											font.family: "JetBrains Mono"
											color: col.onSurfaceVariant
											Layout.preferredWidth: 35
										}
									}

									Text {
										text: "Powered by col_gen - Material You color generation"
										color: col.onSurfaceVariant
										font.pixelSize: 11
										font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
										opacity: 0.7
									}
								}
							}

							// Font & Animation section
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: fontContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: fontContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									RowLayout {
										spacing: 10
										MaterialSymbol { icon: "text_fields"; iconSize: 22; color: col.primary }
										Text { text: "Typography & Motion"; font.pixelSize: 16; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
									}

									// Font Family
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Font Family"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
											Text { text: "UI text font"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										Flow {
											Layout.preferredWidth: 280
											spacing: 6

											Repeater {
												model: fontFamilies

												Rectangle {
													width: Math.max(fontFamilyText.contentWidth + 18, 60)
													height: 32
													radius: configAdapter && configAdapter.fontFamily === modelData ? 16 : 8
													color: configAdapter && configAdapter.fontFamily === modelData ? col.primary : col.surfaceContainerHigh

													Behavior on radius { NumberAnimation { duration: 150 } }
													Behavior on color { ColorAnimation { duration: 150 } }

													Text {
														id: fontFamilyText
														text: modelData
														anchors.centerIn: parent
														color: configAdapter && configAdapter.fontFamily === modelData ? col.onPrimary : col.onSurfaceVariant
														font.pixelSize: 12
														font.family: modelData === "System" ? Qt.application.font.family : modelData
														font.weight: 700
														renderType: Text.NativeRendering
													}

													MouseArea {
														anchors.fill: parent
														cursorShape: Qt.PointingHandCursor
														onClicked: {
															if (configAdapter) {
																configAdapter.fontFamily = modelData
																saveConfig()
															}
														}
													}
												}
											}
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									// Font Size
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Font Size"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
											Text { text: "Base size: " + (configAdapter ? configAdapter.fontSize : 14) + "px"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										RowLayout {
											spacing: 0

											Rectangle {
												width: 40
												height: 40
												color: fontDecMouse.containsMouse ? col.primary : col.surfaceContainerHigh
												topLeftRadius: 20
												bottomLeftRadius: 20
												opacity: configAdapter && configAdapter.fontSize <= 10 ? 0.5 : 1.0

												Behavior on color { ColorAnimation { duration: 150 } }

												MaterialSymbol {
													icon: "remove"
													iconSize: 20
													anchors.centerIn: parent
													color: fontDecMouse.containsMouse ? col.onPrimary : col.onSurfaceVariant
												}

												MouseArea {
													id: fontDecMouse
													anchors.fill: parent
													cursorShape: Qt.PointingHandCursor
													hoverEnabled: true
													enabled: configAdapter && configAdapter.fontSize > 10
													onClicked: {
														if (configAdapter) {
															configAdapter.fontSize--
															saveConfig()
														}
													}
												}
											}

											Rectangle {
												width: 45
												height: 40
												color: col.surfaceContainerHighest

												Text {
													text: configAdapter ? configAdapter.fontSize : 14
													anchors.centerIn: parent
													font.pixelSize: 16
													font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
													font.weight: 700
													color: col.onSurface
												}
											}

											Rectangle {
												width: 40
												height: 40
												color: fontIncMouse.containsMouse ? col.primary : col.surfaceContainerHigh
												topRightRadius: 20
												bottomRightRadius: 20
												opacity: configAdapter && configAdapter.fontSize >= 20 ? 0.5 : 1.0

												Behavior on color { ColorAnimation { duration: 150 } }

												MaterialSymbol {
													icon: "add"
													iconSize: 20
													anchors.centerIn: parent
													color: fontIncMouse.containsMouse ? col.onPrimary : col.onSurfaceVariant
												}

												MouseArea {
													id: fontIncMouse
													anchors.fill: parent
													cursorShape: Qt.PointingHandCursor
													hoverEnabled: true
													enabled: configAdapter && configAdapter.fontSize < 20
													onClicked: {
														if (configAdapter) {
															configAdapter.fontSize++
															saveConfig()
														}
													}
												}
											}
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									// Animation Speed
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Animation Speed"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
											Text { text: "UI transition speed"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										Flow {
											Layout.preferredWidth: 260
											spacing: 6

											Repeater {
												model: animationSpeeds

												Rectangle {
													width: animSpeedText.width + 18
													height: 32
													radius: configAdapter && configAdapter.animationSpeed === modelData.value ? 16 : 8
													color: configAdapter && configAdapter.animationSpeed === modelData.value ? col.primary : col.surfaceContainerHigh

													Behavior on radius { NumberAnimation { duration: 150 } }
													Behavior on color { ColorAnimation { duration: 150 } }

													Text {
														id: animSpeedText
														text: modelData.name
														anchors.centerIn: parent
														color: configAdapter && configAdapter.animationSpeed === modelData.value ? col.onPrimary : col.onSurfaceVariant
														font.pixelSize: 12
														font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
														font.weight: 700
													}

													MouseArea {
														anchors.fill: parent
														cursorShape: Qt.PointingHandCursor
														onClicked: {
															if (configAdapter) {
																configAdapter.animationSpeed = modelData.value
																saveConfig()
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}

					// === Wallpaper Page ===
					ScrollView {
						clip: true

						ColumnLayout {
							width: stackLayout.width - 50
							spacing: 25

							Text {
								text: "Wallpaper"
								font.pixelSize: 26
								font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
								font.weight: 700
								color: col.onSurface
							}

							// Parallax Effect section
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: parallaxContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: parallaxContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									RowLayout {
										spacing: 10
										MaterialSymbol { icon: "swipe"; iconSize: 22; color: col.primary }
										Text { text: "Parallax Effect"; font.pixelSize: 16; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
									}

									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Enable Parallax"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 500; color: col.onSurface }
											Text { text: "Wallpaper shifts when switching workspaces"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										ToggleSwitch {
											checked: configAdapter ? configAdapter.wallpaperParallax : true
											onToggled: (state) => {
												if (configAdapter) {
													configAdapter.wallpaperParallax = state
													saveConfig()
												}
											}
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5; visible: configAdapter ? configAdapter.wallpaperParallax : true }

									// Parallax Strength
									RowLayout {
										Layout.fillWidth: true
										spacing: 15
										visible: configAdapter ? configAdapter.wallpaperParallax : true

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Parallax Strength"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 500; color: col.onSurface }
											Text { text: Math.round((configAdapter ? configAdapter.wallpaperParallaxStrength : 0.1) * 100) + "%"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										StyledSlider {
											sliderWidth: 180
											from: 0.02
											to: 0.2
											stepSize: 0.01
											value: configAdapter ? configAdapter.wallpaperParallaxStrength : 0.1
											onValueChanged: {
												if (configAdapter && Math.abs(configAdapter.wallpaperParallaxStrength - value) > 0.001) {
													configAdapter.wallpaperParallaxStrength = value
													saveConfig()
												}
											}
										}
									}
								}
							}

								// Transition section
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: transitionContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: transitionContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									RowLayout {
										spacing: 10
										MaterialSymbol { icon: "swap_horiz"; iconSize: 22; color: col.primary }
										Text { text: "Wallpaper Transition"; font.pixelSize: 16; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
									}

									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Transition Duration"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 500; color: col.onSurface }
											Text { text: (configAdapter ? configAdapter.wallpaperTransitionDuration : 600) + "ms"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										StyledSlider {
											sliderWidth: 180
											from: 200
											to: 1500
											stepSize: 50
											value: configAdapter ? configAdapter.wallpaperTransitionDuration : 600
											onValueChanged: {
												if (configAdapter && configAdapter.wallpaperTransitionDuration !== value) {
													configAdapter.wallpaperTransitionDuration = value
													saveConfig()
												}
											}
										}
									}

									Text {
										text: "Slide animation when changing wallpapers"
										color: col.onSurfaceVariant
										font.pixelSize: 11
										font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
										opacity: 0.7
									}
								}
							}
						}
					}

					// === Bar Page ===
					ScrollView {
						clip: true

						ColumnLayout {
							width: stackLayout.width - 50
							spacing: 25

							Text {
								text: "Bar"
								font.pixelSize: 26
								font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
								font.weight: 700
								color: col.onSurface
							}

							// Bar Dimensions
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: barDimContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: barDimContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									RowLayout {
										spacing: 10
										MaterialSymbol { icon: "straighten"; iconSize: 22; color: col.primary }
										Text { text: "Dimensions"; font.pixelSize: 16; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
									}

									// Bar Height
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Bar Height"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
											Text { text: (configAdapter ? configAdapter.barHeight : 35) + "px"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										StyledSlider {
											id: barHeightSlider
											sliderWidth: 180
											from: 25
											to: 50
											stepSize: 1
											value: configAdapter ? configAdapter.barHeight : 35
											onValueChanged: {
												if (configAdapter && configAdapter.barHeight !== value) {
													configAdapter.barHeight = value
													saveConfig()
												}
											}
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									// Bar Corner Radius
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Corner Radius"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
											Text { text: (configAdapter ? configAdapter.barRadius : 20) + "px"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										StyledSlider {
											id: barRadiusSlider
											sliderWidth: 180
											from: 0
											to: 30
											stepSize: 1
											value: configAdapter ? configAdapter.barRadius : 20
											onValueChanged: {
												if (configAdapter && configAdapter.barRadius !== value) {
													configAdapter.barRadius = value
													saveConfig()
												}
											}
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									// Gap Size
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Floating Gap"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
											Text { text: (configAdapter ? configAdapter.barGap : 5) + "px margin"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										StyledSlider {
											id: barGapSlider
											sliderWidth: 180
											from: 0
											to: 20
											stepSize: 1
											value: configAdapter ? configAdapter.barGap : 5
											onValueChanged: {
												if (configAdapter && configAdapter.barGap !== value) {
													configAdapter.barGap = value
													saveConfig()
												}
											}
										}
									}
								}
							}

							// Bar Style
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: barStyleContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: barStyleContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									RowLayout {
										spacing: 10
										MaterialSymbol { icon: "view_sidebar"; iconSize: 22; color: col.primary }
										Text { text: "Bar Style"; font.pixelSize: 16; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
									}

									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Floating Bar"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
											Text { text: "Bar floats with margin from edge"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										ToggleSwitch {
											checked: configAdapter ? configAdapter.barFloating : false
											onToggled: (state) => {
												if (configAdapter) {
													configAdapter.barFloating = state
													saveConfig()
												}
											}
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Bar Position"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
											Text { text: configAdapter && configAdapter.barOnTop ? "Bar at top of screen" : "Bar at bottom of screen"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										RowLayout {
											spacing: 8
											Text { text: "Bottom"; font.pixelSize: 12; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant }
											ToggleSwitch {
												checked: configAdapter ? configAdapter.barOnTop : true
												onToggled: (state) => {
													if (configAdapter) {
														configAdapter.barOnTop = state
														saveConfig()
													}
												}
											}
											Text { text: "Top"; font.pixelSize: 12; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant }
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "System Tray"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
											Text { text: "Show system tray icons"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										ToggleSwitch {
											checked: configAdapter ? configAdapter.showSystemTray : true
											onToggled: (state) => {
												if (configAdapter) {
													configAdapter.showSystemTray = state
													saveConfig()
												}
											}
										}
									}
								}
							}

							// Workspaces section
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: wsContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: wsContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									RowLayout {
										spacing: 10
										MaterialSymbol { icon: "grid_view"; iconSize: 22; color: col.primary }
										Text { text: "Workspaces"; font.pixelSize: 16; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
									}

									RowLayout {
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Workspace Count"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
											Text { text: "Number of workspaces shown (1-20)"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										RowLayout {
											spacing: 0

											Rectangle {
												width: 44
												height: 44
												color: decMouse.containsMouse ? col.primary : col.surfaceContainerHigh
												topLeftRadius: 22
												bottomLeftRadius: 22
												opacity: root.workspaceCount <= 1 ? 0.5 : 1.0

												Behavior on color { ColorAnimation { duration: 150 } }

												MaterialSymbol {
													icon: "remove"
													iconSize: 22
													anchors.centerIn: parent
													color: decMouse.containsMouse ? col.onPrimary : col.onSurfaceVariant
												}

												MouseArea {
													id: decMouse
													anchors.fill: parent
													cursorShape: Qt.PointingHandCursor
													hoverEnabled: true
													enabled: root.workspaceCount > 1
													onClicked: root.decrementWorkspaces()
												}
											}

											Rectangle {
												width: 50
												height: 44
												color: col.surfaceContainerHighest

												Text {
													text: root.workspaceCount
													anchors.centerIn: parent
													font.pixelSize: 18
													font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
													font.weight: 700
													color: col.onSurface
												}
											}

											Rectangle {
												width: 44
												height: 44
												color: incMouse.containsMouse ? col.primary : col.surfaceContainerHigh
												topRightRadius: 22
												bottomRightRadius: 22
												opacity: root.workspaceCount >= 20 ? 0.5 : 1.0

												Behavior on color { ColorAnimation { duration: 150 } }

												MaterialSymbol {
													icon: "add"
													iconSize: 22
													anchors.centerIn: parent
													color: incMouse.containsMouse ? col.onPrimary : col.onSurfaceVariant
												}

												MouseArea {
													id: incMouse
													anchors.fill: parent
													cursorShape: Qt.PointingHandCursor
													hoverEnabled: true
													enabled: root.workspaceCount < 20
													onClicked: root.incrementWorkspaces()
												}
											}
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									// Workspace Style
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Indicator Style"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
											Text { text: "Workspace indicator appearance"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										Flow {
											Layout.preferredWidth: 200
											spacing: 8

											Repeater {
												model: workspaceStyles

												Rectangle {
													width: wsStyleRow.width + 18
													height: 36
													radius: configAdapter && configAdapter.workspaceStyle === modelData.value ? 18 : 10
													color: configAdapter && configAdapter.workspaceStyle === modelData.value ? col.primary : col.surfaceContainerHigh

													Behavior on radius { NumberAnimation { duration: 150 } }
													Behavior on color { ColorAnimation { duration: 150 } }

													RowLayout {
														id: wsStyleRow
														anchors.centerIn: parent
														spacing: 6
														MaterialSymbol {
															icon: modelData.icon
															iconSize: 18
															color: configAdapter && configAdapter.workspaceStyle === modelData.value ? col.onPrimary : col.onSurfaceVariant
														}
														Text {
															text: modelData.name
															color: configAdapter && configAdapter.workspaceStyle === modelData.value ? col.onPrimary : col.onSurfaceVariant
															font.pixelSize: 12
															font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
															font.weight: 700
														}
													}

													MouseArea {
														anchors.fill: parent
														cursorShape: Qt.PointingHandCursor
														onClicked: {
															if (configAdapter) {
																configAdapter.workspaceStyle = modelData.value
																saveConfig()
															}
														}
													}
												}
											}
										}
									}

								      //Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									// Dynamic workspaces toggle
									/*
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Dynamic Workspaces"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
											Text { text: "Show only active workspaces"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										ToggleSwitch {
											checked: configAdapter ? configAdapter.dynamicWorkspaces : false
											onToggled: (state) => {
												if (configAdapter) {
													configAdapter.dynamicWorkspaces = state
													saveConfig()
												}
											}
										}
									}
									*/
								}
							}

							// Screen Corners
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: cornersContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: cornersContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									RowLayout {
										spacing: 10
										MaterialSymbol { icon: "rounded_corner"; iconSize: 22; color: col.primary }
										Text { text: "Screen Corners"; font.pixelSize: 16; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
									}

									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Rounded Corners"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
											Text { text: "Add rounded corners overlay"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										ToggleSwitch {
											checked: configAdapter ? configAdapter.screenCorners : true
											onToggled: (state) => {
												if (configAdapter) {
													configAdapter.screenCorners = state
													saveConfig()
												}
											}
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									// Corner Size
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Corner Size"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
											Text { text: (configAdapter ? configAdapter.screenCornerSize : 25) + "px"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										StyledSlider {
											id: cornerSizeSlider
											sliderWidth: 180
											from: 10
											to: 40
											stepSize: 1
											value: configAdapter ? configAdapter.screenCornerSize : 25
											onValueChanged: {
												if (configAdapter && configAdapter.screenCornerSize !== value) {
													configAdapter.screenCornerSize = value
													saveConfig()
												}
											}
										}
									}
								}
							}
						}
					}

					// === Launcher Page ===
					ScrollView {
						clip: true

						ColumnLayout {
							width: stackLayout.width - 50
							spacing: 25

							Text {
								text: "Launcher"
								font.pixelSize: 26
								font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
								font.weight: 700
								color: col.onSurface
							}

							// Launcher Presets
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: presetContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: presetContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 12

									Text {
										text: "Preset"
										font.pixelSize: 14
										font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
										font.weight: 600
										color: col.onSurface
									}

									RowLayout {
										Layout.fillWidth: true
										spacing: 10

										Repeater {
											model: [
												{ name: "Compact", value: "compact", icon: "density_small" },
												{ name: "Default", value: "default", icon: "density_medium" },
												{ name: "Expanded", value: "expanded", icon: "density_large" }
											]

											Rectangle {
												Layout.fillWidth: true
												Layout.preferredHeight: 70
												radius: 12
												color: configAdapter && configAdapter.launcherPreset === modelData.value 
													? col.primaryContainer : col.surfaceContainerHigh

												ColumnLayout {
													anchors.centerIn: parent
													spacing: 6

													MaterialSymbol {
														Layout.alignment: Qt.AlignHCenter
														icon: modelData.icon
														iconSize: 24
														color: configAdapter && configAdapter.launcherPreset === modelData.value 
															? col.onPrimaryContainer : col.onSurfaceVariant
													}

													Text {
														Layout.alignment: Qt.AlignHCenter
														text: modelData.name
														font.pixelSize: 12
														font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
														color: configAdapter && configAdapter.launcherPreset === modelData.value 
															? col.onPrimaryContainer : col.onSurface
													}
												}

												MouseArea {
													anchors.fill: parent
													cursorShape: Qt.PointingHandCursor
													onClicked: {
														configAdapter.launcherPreset = modelData.value
														// Apply preset values
														if (modelData.value === "compact") {
															configAdapter.launcherWidth = 350
															configAdapter.launcherItemHeight = 45
															configAdapter.launcherRadius = 20
															configAdapter.launcherMaxItems = 6
														} else if (modelData.value === "expanded") {
															configAdapter.launcherWidth = 450
															configAdapter.launcherItemHeight = 65
															configAdapter.launcherRadius = 30
															configAdapter.launcherMaxItems = 10
														} else {
															configAdapter.launcherWidth = 400
															configAdapter.launcherItemHeight = 55
															configAdapter.launcherRadius = 25
															configAdapter.launcherMaxItems = 8
														}
														saveConfig()
													}
												}
											}
										}
									}
								}
							}

							// Launcher Dimensions
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: dimensionsContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: dimensionsContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									Text {
										text: "Dimensions"
										font.pixelSize: 14
										font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
										font.weight: 600
										color: col.onSurface
									}

									// Width
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										Text {
											text: "Width"
											font.pixelSize: 13
											font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
											color: col.onSurfaceVariant
											Layout.preferredWidth: 100
										}

										StyledSlider {
											Layout.fillWidth: true
											from: 300
											to: 500
											stepSize: 10
											value: configAdapter ? configAdapter.launcherWidth : 400
											onMoved: newValue => {
												configAdapter.launcherWidth = newValue
												configAdapter.launcherPreset = "custom"
												saveConfig()
											}
										}

										Text {
											text: (configAdapter ? configAdapter.launcherWidth : 400) + "px"
											font.pixelSize: 12
											font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
											color: col.onSurfaceVariant
											Layout.preferredWidth: 45
										}
									}

									// Item Height
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										Text {
											text: "Item Height"
											font.pixelSize: 13
											font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
											color: col.onSurfaceVariant
											Layout.preferredWidth: 100
										}

										StyledSlider {
											Layout.fillWidth: true
											from: 40
											to: 80
											stepSize: 5
											value: configAdapter ? configAdapter.launcherItemHeight : 55
											onMoved: newValue => {
												configAdapter.launcherItemHeight = newValue
												configAdapter.launcherPreset = "custom"
												saveConfig()
											}
										}

										Text {
											text: (configAdapter ? configAdapter.launcherItemHeight : 55) + "px"
											font.pixelSize: 12
											font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
											color: col.onSurfaceVariant
											Layout.preferredWidth: 45
										}
									}

									// Corner Radius
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										Text {
											text: "Radius"
											font.pixelSize: 13
											font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
											color: col.onSurfaceVariant
											Layout.preferredWidth: 100
										}

										StyledSlider {
											Layout.fillWidth: true
											from: 10
											to: 40
											stepSize: 5
											value: configAdapter ? configAdapter.launcherRadius : 25
											onMoved: newValue => {
												configAdapter.launcherRadius = newValue
												configAdapter.launcherPreset = "custom"
												saveConfig()
											}
										}

										Text {
											text: (configAdapter ? configAdapter.launcherRadius : 25) + "px"
											font.pixelSize: 12
											font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
											color: col.onSurfaceVariant
											Layout.preferredWidth: 45
										}
									}

									// Max Items
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										Text {
											text: "Max Items"
											font.pixelSize: 13
											font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
											color: col.onSurfaceVariant
											Layout.preferredWidth: 100
										}

										StyledSlider {
											Layout.fillWidth: true
											from: 4
											to: 15
											stepSize: 1
											value: configAdapter ? configAdapter.launcherMaxItems : 8
											onMoved: newValue => {
												configAdapter.launcherMaxItems = newValue
												configAdapter.launcherPreset = "custom"
												saveConfig()
											}
										}

										Text {
											text: configAdapter ? configAdapter.launcherMaxItems : 8
											font.pixelSize: 12
											font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
											color: col.onSurfaceVariant
											Layout.preferredWidth: 45
										}
									}
								}
							}

							// Display Options
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: displayContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: displayContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 12

									Text {
										text: "Display"
										font.pixelSize: 14
										font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
										font.weight: 600
										color: col.onSurface
									}

									// Show Icons
									RowLayout {
										Layout.fillWidth: true

										Text {
											text: "Show App Icons"
											font.pixelSize: 13
											font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
											color: col.onSurfaceVariant
											Layout.fillWidth: true
										}

										ToggleSwitch {
											checked: configAdapter ? configAdapter.launcherShowIcons : true
											onToggled: {
												configAdapter.launcherShowIcons = checked
												saveConfig()
											}
										}
									}

									// Show Descriptions
									RowLayout {
										Layout.fillWidth: true

										Text {
											text: "Show Descriptions"
											font.pixelSize: 13
											font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
											color: col.onSurfaceVariant
											Layout.fillWidth: true
										}

										ToggleSwitch {
											checked: configAdapter ? configAdapter.launcherShowDescriptions : true
											onToggled: {
												configAdapter.launcherShowDescriptions = checked
												saveConfig()
											}
										}
									}
								}
							}

							// Enabled Modes
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: modesContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: modesContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 12

									Text {
										text: "Enabled Modes"
										font.pixelSize: 14
										font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
										font.weight: 600
										color: col.onSurface
									}

									// Emoji Mode
									RowLayout {
										Layout.fillWidth: true

										MaterialSymbol {
											icon: "emoji_emotions"
											iconSize: 20
											color: col.onSurfaceVariant
										}

										Text {
											text: "Emoji Picker (:emoji)"
											font.pixelSize: 13
											font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
											color: col.onSurfaceVariant
											Layout.fillWidth: true
										}

										ToggleSwitch {
											checked: configAdapter ? configAdapter.launcherEmojiMode : true
											onToggled: {
												configAdapter.launcherEmojiMode = checked
												saveConfig()
											}
										}
									}

									// Clipboard Mode
									RowLayout {
										Layout.fillWidth: true

										MaterialSymbol {
											icon: "content_paste"
											iconSize: 20
											color: col.onSurfaceVariant
										}

										Text {
											text: "Clipboard History (/clip)"
											font.pixelSize: 13
											font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
											color: col.onSurfaceVariant
											Layout.fillWidth: true
										}

										ToggleSwitch {
											checked: configAdapter ? configAdapter.launcherClipboardMode : true
											onToggled: {
												configAdapter.launcherClipboardMode = checked
												saveConfig()
											}
										}
									}

									// Wallpaper Mode
									RowLayout {
										Layout.fillWidth: true

										MaterialSymbol {
											icon: "image"
											iconSize: 20
											color: col.onSurfaceVariant
										}

										Text {
											text: "Wallpaper Picker (/wallpaper)"
											font.pixelSize: 13
											font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
											color: col.onSurfaceVariant
											Layout.fillWidth: true
										}

										ToggleSwitch {
											checked: configAdapter ? configAdapter.launcherWallpaperMode : true
											onToggled: {
												configAdapter.launcherWallpaperMode = checked
												saveConfig()
											}
										}
									}
								}
							}
						}
					}

					// === Clock Page ===
					ScrollView {
						clip: true

						ColumnLayout {
							width: stackLayout.width - 50
							spacing: 25

							Text {
								text: "Clock"
								font.pixelSize: 26
								font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
								font.weight: 700
								color: col.onSurface
							}

							// Clock Format
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: clockContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: clockContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									RowLayout {
										spacing: 10
										MaterialSymbol { icon: "schedule"; iconSize: 22; color: col.primary }
										Text { text: "Clock Format"; font.pixelSize: 16; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
									}

									// Preview
									Rectangle {
										Layout.fillWidth: true
										Layout.preferredHeight: 60
										radius: 12
										color: col.surfaceContainerHigh

										Text {
											id: clockPreview
											property var currentTime: new Date()
											Timer {
												interval: 1000
												repeat: true
												running: true
												onTriggered: clockPreview.currentTime = new Date()
											}
											text: Qt.formatDateTime(currentTime, configAdapter ? configAdapter.clockFormat : "hh:mm AP")
											anchors.centerIn: parent
											font.pixelSize: 24
											font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
											font.weight: 700
											color: col.primary
										}
									}

									// Presets
									Text { text: "Presets"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }

									Flow {
										Layout.fillWidth: true
										spacing: 8

										Repeater {
											model: clockPresets

											Rectangle {
												width: presetText.width + 22
												height: 36
												radius: configAdapter && configAdapter.clockPreset === modelData.id ? 18 : 10
												color: configAdapter && configAdapter.clockPreset === modelData.id ? col.primary : col.surfaceContainerHigh
												visible: modelData.id !== "custom"

												Behavior on radius { NumberAnimation { duration: 150 } }
												Behavior on color { ColorAnimation { duration: 150 } }

												Text {
													id: presetText
													text: modelData.name
													anchors.centerIn: parent
													color: configAdapter && configAdapter.clockPreset === modelData.id ? col.onPrimary : col.onSurfaceVariant
													font.pixelSize: 13
													font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
													font.weight: 700
												}

												MouseArea {
													anchors.fill: parent
													cursorShape: Qt.PointingHandCursor
													onClicked: {
														if (configAdapter) {
															configAdapter.clockPreset = modelData.id
															configAdapter.clockFormat = modelData.format
															saveConfig()
														}
													}
												}
											}
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									// Custom Format
									Text { text: "Custom Format"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }

									RowLayout {
										Layout.fillWidth: true
										spacing: 10

										Rectangle {
											Layout.fillWidth: true
											Layout.preferredHeight: 44
											radius: 12
											color: col.surfaceContainerHigh
											border.width: customFormatField.activeFocus ? 2 : 0
											border.color: col.primary

											TextField {
												id: customFormatField
												anchors.fill: parent
												anchors.margins: 4
												text: configAdapter ? configAdapter.clockFormat : "hh:mm AP"
												placeholderText: "Enter format..."
												placeholderTextColor: col.onSurfaceVariant
												background: null
												color: col.onSurface
												font.pixelSize: 14
												font.family: "JetBrains Mono"
												verticalAlignment: Text.AlignVCenter
												
												onTextChanged: {
													if (configAdapter && text !== configAdapter.clockFormat) {
														configAdapter.clockPreset = "custom"
														configAdapter.clockFormat = text
														saveConfig()
													}
												}
											}
										}
									}

									// Format Help
									Rectangle {
										Layout.fillWidth: true
										Layout.preferredHeight: formatHelpContent.height + 20
										radius: 12
										color: col.tertiaryContainer
										opacity: 0.7

										ColumnLayout {
											id: formatHelpContent
											anchors.left: parent.left
											anchors.right: parent.right
											anchors.verticalCenter: parent.verticalCenter
											anchors.margins: 12
											spacing: 6

											RowLayout {
												spacing: 8
												MaterialSymbol { icon: "help"; iconSize: 18; color: col.onTertiaryContainer }
												Text { text: "Format Tokens"; font.pixelSize: 12; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onTertiaryContainer }
											}

											Text {
												Layout.fillWidth: true
												text: "hh = 12h hour | HH = 24h hour | mm = minutes | ss = seconds\nAP = AM/PM | ap = am/pm | ddd = day name | MMM = month\nd = day | yyyy = year"
												font.pixelSize: 11
												font.family: "JetBrains Mono"
												color: col.onTertiaryContainer
												wrapMode: Text.Wrap
												lineHeight: 1.4
											}
										}
									}
								}
							}
						}
					}

					// === Advanced Page ===
					ScrollView {
						clip: true

						ColumnLayout {
							width: stackLayout.width - 50
							spacing: 25

							Text {
								text: "Advanced"
								font.pixelSize: 26
								font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
								font.weight: 700
								color: col.onSurface
							}

							// Beta badge
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: betaBannerContent.height + 24
								radius: 16
								color: col.tertiaryContainer

								RowLayout {
									id: betaBannerContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.verticalCenter: parent.verticalCenter
									anchors.margins: 15
									spacing: 12

									Rectangle {
										width: 36; height: 36; radius: 10
										color: col.tertiary

										MaterialSymbol {
											icon: "science"
											iconSize: 22
											anchors.centerIn: parent
											color: col.onTertiary
										}
									}

									ColumnLayout {
										Layout.fillWidth: true
										spacing: 2
										Text { text: "Experimental Features"; font.pixelSize: 15; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onTertiaryContainer }
										Text { text: "These features are in beta and may change or break"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onTertiaryContainer; opacity: 0.8 }
									}
								}
							}

							// Desktop Widgets toggle
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: widgetToggleContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: widgetToggleContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									RowLayout {
										spacing: 10
										MaterialSymbol { icon: "widgets"; iconSize: 22; color: col.primary }
										Text { text: "Desktop Widgets"; font.pixelSize: 16; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }

										Item { Layout.fillWidth: true }

										Rectangle {
											width: betaTag.width + 12; height: 20; radius: 10
											color: col.tertiaryContainer

											Text {
												id: betaTag
												text: "BETA"
												anchors.centerIn: parent
												font.pixelSize: 10
												font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
												font.weight: 700
												color: col.onTertiaryContainer
											}
										}
									}

									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Enable Desktop Widgets"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 500; color: col.onSurface }
											Text { text: "Show clock, weather and custom widgets on desktop"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										ToggleSwitch {
											checked: configAdapter ? configAdapter.desktopWidgets : true
											onToggled: (state) => {
												if (configAdapter) {
													configAdapter.desktopWidgets = state
													saveConfig()
												}
											}
										}
									}
								}
							}

							// Grid Layout section
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: gridLayoutContent.height + 30
								radius: 16
								color: col.surfaceContainer
								opacity: configAdapter && configAdapter.desktopWidgets ? 1.0 : 0.5

								ColumnLayout {
									id: gridLayoutContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									RowLayout {
										spacing: 10
										MaterialSymbol { icon: "grid_on"; iconSize: 22; color: col.primary }
										Text { text: "Grid Layout"; font.pixelSize: 16; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
									}

									// Columns
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Columns"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 500; color: col.onSurface }
											Text { text: (configAdapter ? configAdapter.gridColumns : 16) + " columns"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										StyledSlider {
											sliderWidth: 180
											from: 8
											to: 24
											stepSize: 1
											value: configAdapter ? configAdapter.gridColumns : 16
											onValueChanged: {
												if (configAdapter && configAdapter.gridColumns !== value) {
													configAdapter.gridColumns = value
													saveConfig()
												}
											}
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									// Rows
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Rows"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 500; color: col.onSurface }
											Text { text: (configAdapter ? configAdapter.gridRows : 9) + " rows"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										StyledSlider {
											sliderWidth: 180
											from: 4
											to: 16
											stepSize: 1
											value: configAdapter ? configAdapter.gridRows : 9
											onValueChanged: {
												if (configAdapter && configAdapter.gridRows !== value) {
													configAdapter.gridRows = value
													saveConfig()
												}
											}
										}
									}

									// Info text
									Text {
										text: "Grid defines snap positions for desktop widgets. Bar space is excluded automatically."
										color: col.onSurfaceVariant
										font.pixelSize: 11
										font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
										opacity: 0.7
										wrapMode: Text.Wrap
										Layout.fillWidth: true
									}
								}
							}

							// Widget Style section
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: widgetStyleContent.height + 30
								radius: 16
								color: col.surfaceContainer
								opacity: configAdapter && configAdapter.desktopWidgets ? 1.0 : 0.5

								ColumnLayout {
									id: widgetStyleContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									RowLayout {
										spacing: 10
										MaterialSymbol { icon: "style"; iconSize: 22; color: col.primary }
										Text { text: "Widget Style"; font.pixelSize: 16; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
									}

									// Widget Preview
									Rectangle {
										Layout.fillWidth: true
										Layout.preferredHeight: 80
										radius: configAdapter ? configAdapter.widgetRadius : 12
										color: configAdapter && configAdapter.widgetBackgroundColor !== "" ? configAdapter.widgetBackgroundColor : (col ? col.background : "#111318")
										opacity: configAdapter ? configAdapter.widgetOpacity : 0.85
										border.width: configAdapter ? configAdapter.widgetBorderWidth : 1
										border.color: configAdapter && configAdapter.widgetBorderColor !== "" ? configAdapter.widgetBorderColor : (col ? col.outline : "#8e9099")

										Row {
											anchors.centerIn: parent
											spacing: 12
											MaterialSymbol { icon: "widgets"; iconSize: 24; color: col ? col.primary : "#adc6ff"; anchors.verticalCenter: parent.verticalCenter }
											Text { text: "Widget Preview"; color: col ? col.onSurface : "#e2e2e9"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; anchors.verticalCenter: parent.verticalCenter }
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									// Corner Radius
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Corner Radius"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 500; color: col.onSurface }
											Text { text: (configAdapter ? configAdapter.widgetRadius : 12) + "px"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										StyledSlider {
											sliderWidth: 180
											from: 0
											to: 30
											stepSize: 1
											value: configAdapter ? configAdapter.widgetRadius : 12
											onValueChanged: {
												if (configAdapter && configAdapter.widgetRadius !== value) {
													configAdapter.widgetRadius = value
													saveConfig()
												}
											}
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									// Border Width
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Border Width"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 500; color: col.onSurface }
											Text { text: (configAdapter ? configAdapter.widgetBorderWidth : 1) + "px"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										StyledSlider {
											sliderWidth: 180
											from: 0
											to: 4
											stepSize: 1
											value: configAdapter ? configAdapter.widgetBorderWidth : 1
											onValueChanged: {
												if (configAdapter && configAdapter.widgetBorderWidth !== value) {
													configAdapter.widgetBorderWidth = value
													saveConfig()
												}
											}
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									// Widget Opacity
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Background Opacity"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 500; color: col.onSurface }
											Text { text: Math.round((configAdapter ? configAdapter.widgetOpacity : 0.85) * 100) + "%"; font.pixelSize: 11; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										StyledSlider {
											sliderWidth: 180
											from: 0.1
											to: 1.0
											stepSize: 0.05
											value: configAdapter ? configAdapter.widgetOpacity : 0.85
											onValueChanged: {
												if (configAdapter && Math.abs(configAdapter.widgetOpacity - value) > 0.01) {
													configAdapter.widgetOpacity = value
													saveConfig()
												}
											}
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									// Border Color
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Border Color"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 500; color: col.onSurface }
											Text { text: configAdapter && configAdapter.widgetBorderColor !== "" ? configAdapter.widgetBorderColor : "Theme default"; font.pixelSize: 11; font.family: "JetBrains Mono"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										Row {
											spacing: 6

											Repeater {
												model: [
													{ color: "", label: "Auto" },
													{ color: "#adc6ff", label: "Blue" },
													{ color: "#8e9099", label: "Gray" },
													{ color: "#ffb4ab", label: "Red" },
													{ color: "#c5c6d0", label: "Light" },
													{ color: "transparent", label: "None" }
												]

												Rectangle {
													width: 28; height: 28; radius: 14
													color: modelData.color === "" ? col.outline : (modelData.color === "transparent" ? col.surfaceContainerHigh : modelData.color)
													border.width: configAdapter && configAdapter.widgetBorderColor === modelData.color ? 3 : 1
													border.color: configAdapter && configAdapter.widgetBorderColor === modelData.color ? col.primary : col.outlineVariant

													Rectangle {
														anchors.centerIn: parent
														width: 8; height: 8; radius: 4
														color: col.primary
														visible: configAdapter && configAdapter.widgetBorderColor === modelData.color
													}

													MouseArea {
														anchors.fill: parent
														cursorShape: Qt.PointingHandCursor
														onClicked: {
															if (configAdapter) {
																configAdapter.widgetBorderColor = modelData.color
																saveConfig()
															}
														}
													}
												}
											}
										}
									}

									Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

									// Background Color
									RowLayout {
										Layout.fillWidth: true
										spacing: 15

										ColumnLayout {
											Layout.fillWidth: true
											spacing: 2
											Text { text: "Background Color"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 500; color: col.onSurface }
											Text { text: configAdapter && configAdapter.widgetBackgroundColor !== "" ? configAdapter.widgetBackgroundColor : "Theme default"; font.pixelSize: 11; font.family: "JetBrains Mono"; color: col.onSurfaceVariant; opacity: 0.8 }
										}

										Row {
											spacing: 6

											Repeater {
												model: [
													{ color: "", label: "Auto" },
													{ color: "#1e1f25", label: "Dark" },
													{ color: "#2b2d35", label: "Mid" },
													{ color: "#111318", label: "Darker" },
													{ color: "#000000", label: "Black" },
													{ color: "transparent", label: "Clear" }
												]

												Rectangle {
													width: 28; height: 28; radius: 14
													color: modelData.color === "" ? col.surfaceContainer : (modelData.color === "transparent" ? "transparent" : modelData.color)
													border.width: configAdapter && configAdapter.widgetBackgroundColor === modelData.color ? 3 : 1
													border.color: configAdapter && configAdapter.widgetBackgroundColor === modelData.color ? col.primary : col.outlineVariant

													Rectangle {
														anchors.centerIn: parent
														width: 8; height: 8; radius: 4
														color: col.primary
														visible: configAdapter && configAdapter.widgetBackgroundColor === modelData.color
													}

													// Checkerboard for transparent
													Canvas {
														anchors.fill: parent
														visible: modelData.color === "transparent"
														onPaint: {
															var ctx = getContext("2d")
															ctx.clearRect(0, 0, width, height)
															var s = 4
															for (var y = 0; y < height; y += s) {
																for (var x = 0; x < width; x += s) {
																	ctx.fillStyle = ((x / s + y / s) % 2 === 0) ? "#333" : "#555"
																	ctx.fillRect(x, y, s, s)
																}
															}
														}
														Component.onCompleted: requestPaint()
													}

													MouseArea {
														anchors.fill: parent
														cursorShape: Qt.PointingHandCursor
														onClicked: {
															if (configAdapter) {
																configAdapter.widgetBackgroundColor = modelData.color
																saveConfig()
															}
														}
													}
												}
											}
										}
									}
								}
							}

							// Smart Widget Placement section
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: smartPlacementContent.height + 30
								radius: 16
								color: col.surfaceContainer
								opacity: configAdapter && configAdapter.desktopWidgets ? 1.0 : 0.5

								ColumnLayout {
									id: smartPlacementContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 15

									RowLayout {
										spacing: 10
										MaterialSymbol { icon: "auto_fix_high"; iconSize: 22; color: col.primary }
										Text { text: "Smart Widget Placement"; font.pixelSize: 16; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
									}

									Text {
										text: "Analyzes your wallpaper to find calm areas and positions widgets where they won't obscure important content."
										color: col.onSurfaceVariant
										font.pixelSize: 12
										font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
										wrapMode: Text.Wrap
										Layout.fillWidth: true
									}

									// Status text
									Text {
										id: smartStatusText
										text: ""
										color: col.tertiary
										font.pixelSize: 11
										font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
										visible: text !== ""
									}

									Rectangle {
										Layout.fillWidth: true
										Layout.preferredHeight: 44
										radius: 22
										color: smartPlacementMouse.containsMouse ? col.primary : col.primaryContainer
										opacity: smartAnalyzeProcess.running ? 0.6 : 1.0

										Behavior on color { ColorAnimation { duration: 150 } }

										RowLayout {
											anchors.centerIn: parent
											spacing: 10

											MaterialSymbol {
												icon: smartAnalyzeProcess.running ? "hourglass_empty" : "auto_fix_high"
												iconSize: 20
												color: smartPlacementMouse.containsMouse ? col.onPrimary : col.onPrimaryContainer

												RotationAnimation on rotation {
													from: 0
													to: 360
													duration: 1500
													loops: Animation.Infinite
													running: smartAnalyzeProcess.running
												}
											}

											Text {
												text: smartAnalyzeProcess.running ? "Analyzing..." : "Auto-position widgets"
												font.pixelSize: 14
												font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
												font.weight: 700
												color: smartPlacementMouse.containsMouse ? col.onPrimary : col.onPrimaryContainer
											}
										}

										MouseArea {
											id: smartPlacementMouse
											anchors.fill: parent
											cursorShape: Qt.PointingHandCursor
											hoverEnabled: true
											enabled: !smartAnalyzeProcess.running && configAdapter && configAdapter.desktopWidgets

											onClicked: {
												if (col && col.wallpaper) {
													smartStatusText.text = "Analyzing wallpaper..."
													var analyzeScript = Qt.resolvedUrl("../col_gen/analyze").toString().replace("file://", "")
													var cols = configAdapter ? configAdapter.gridColumns : 16
													var rows = configAdapter ? configAdapter.gridRows : 9
													smartAnalyzeProcess.command = [
														analyzeScript, col.wallpaper,
														"--cols", cols.toString(),
														"--rows", rows.toString(),
														"--apply"
													]
													smartAnalyzeProcess.running = true
												} else {
													smartStatusText.text = "No wallpaper set"
												}
											}
										}
									}
								}
							}

							// Process for smart analysis
							Process {
								id: smartAnalyzeProcess
								stdout: StdioCollector {
									onStreamFinished: {
										if (text.indexOf("Applied to:") !== -1) {
											smartReloadWidgetsProcess.running = true
											smartStatusText.text = "Widgets repositioned!"
											smartStatusClearTimer.start()
										}
									}
								}
								stderr: StdioCollector {
									onStreamFinished: {
										if (text.trim() !== "") {
											smartStatusText.text = "Error: " + text.trim().substring(0, 50)
										}
									}
								}
							}

							// Process to reload widgets via IPC
							Process {
								id: smartReloadWidgetsProcess
								command: ["qs", "ipc", "call", "widgets", "reload"]
							}

							Timer {
								id: smartStatusClearTimer
								interval: 5000
								onTriggered: smartStatusText.text = ""
							}
						}
					}

				// === About Page ===
					ScrollView {
						clip: true

						ColumnLayout {
							width: stackLayout.width - 50
							spacing: 25

							Item { Layout.preferredHeight: 10 }

							// OS Info Section
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: osInfoContent.height + 40
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: osInfoContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 20
									spacing: 15

									// Distro Logo
									Rectangle {
										Layout.preferredWidth: 80
										Layout.preferredHeight: 80
										Layout.alignment: Qt.AlignHCenter
										radius: 20
										color: col.primaryContainer

										Image {
											id: distroLogo
											anchors.centerIn: parent
											width: 56
											height: 56
											source: OsRelease.logo ? "image://icon/" + OsRelease.logo : ""
											sourceSize: Qt.size(56, 56)
											visible: status === Image.Ready
										}

										MaterialSymbol {
											icon: "computer"
											iconSize: 40
											anchors.centerIn: parent
											color: col.onPrimaryContainer
											visible: distroLogo.status !== Image.Ready
										}
									}

									// Distro Name
									Text {
										text: OsRelease.prettyName || OsRelease.name || "Unknown OS"
										Layout.alignment: Qt.AlignHCenter
										font.pixelSize: 22
										font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
										font.weight: 700
										color: col.onSurface
									}

									// Website Link
									Rectangle {
										Layout.alignment: Qt.AlignHCenter
										width: osLinkRow.width + 20
										height: 36
										radius: 18
										color: osLinkMouse.containsMouse ? col.primaryContainer : col.surfaceContainerHigh
										visible: OsRelease.homeUrl !== ""

										Behavior on color { ColorAnimation { duration: 150 } }

										RowLayout {
											id: osLinkRow
											anchors.centerIn: parent
											spacing: 8

											MaterialSymbol {
												icon: "language"
												iconSize: 18
												color: osLinkMouse.containsMouse ? col.onPrimaryContainer : col.primary
											}

											Text {
												text: OsRelease.homeUrl.replace(/^https?:\/\//, "").replace(/\/$/, "")
												font.pixelSize: 13
												font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
												font.weight: 500
												color: osLinkMouse.containsMouse ? col.onPrimaryContainer : col.primary
											}
										}

										MouseArea {
											id: osLinkMouse
											anchors.fill: parent
											cursorShape: Qt.PointingHandCursor
											hoverEnabled: true
											onClicked: {
												openUrlProcess.command = ["xdg-open", OsRelease.homeUrl]
												openUrlProcess.running = true
											}
										}
									}
								}
							}

							// Divider
							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: 1
								color: col.outlineVariant
							}

							Rectangle {
								Layout.preferredWidth: 100
								Layout.preferredHeight: 100
								Layout.alignment: Qt.AlignHCenter
								radius: 25
								color: col.primaryContainer

								MaterialSymbol {
									icon: "dashboard_customize"
									iconSize: 50
									anchors.centerIn: parent
									color: col.onPrimaryContainer
								}
							}

							ColumnLayout {
								Layout.alignment: Qt.AlignHCenter
								spacing: 5

								Text {
									text: "Quickshell"
									Layout.alignment: Qt.AlignHCenter
									font.pixelSize: 28
									font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
									font.weight: 700
									color: col.primary
								}

								Text {
									text: "Material Shell for Hyprland"
									Layout.alignment: Qt.AlignHCenter
									font.pixelSize: 14
									font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
									color: col.onSurfaceVariant
								}

								Rectangle {
									Layout.alignment: Qt.AlignHCenter
									width: versionText.width + 16
									height: 24
									radius: 12
									color: col.secondaryContainer

									Text {
										id: versionText
										text: "v1.0.0"
										anchors.centerIn: parent
										font.pixelSize: 12
										font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
										font.weight: 700
										color: col.onSecondaryContainer
									}
								}
							}

							Item { Layout.preferredHeight: 5 }

							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: descContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: descContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 12

									Text {
										Layout.fillWidth: true
										text: "A modern shell configuration featuring Material Design 3 theming with dynamic color generation powered by matugen."
										font.pixelSize: 13
										font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
										color: col.onSurface
										wrapMode: Text.Wrap
										lineHeight: 1.4
									}

									Flow {
										Layout.fillWidth: true
										spacing: 8

										Repeater {
											model: ["QML", "MD3", "Hyprland", "Matugen"]

											Rectangle {
												width: tagItemText.width + 16
												height: 26
												radius: 13
												color: col.secondaryContainer

												Text {
													id: tagItemText
													text: modelData
													anchors.centerIn: parent
													font.pixelSize: 11
													font.family: configAdapter ? configAdapter.fontFamily : "Rubik"
													font.weight: 700
													color: col.onSecondaryContainer
												}
											}
										}
									}
								}
							}

							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: linksContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: linksContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 12

									Text { text: "Links"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }

									Flow {
										Layout.fillWidth: true
										spacing: 10

										Repeater {
											model: [
												{ text: "Source", icon: "code", url: "https://github.com" },
												{ text: "Issues", icon: "bug_report", url: "https://github.com" },
												{ text: "Docs", icon: "menu_book", url: "https://github.com" }
											]

											Rectangle {
												width: linkItemRow.width + 24
												height: 40
												radius: 20
												color: linkItemMouse.containsMouse ? col.primaryContainer : col.surfaceContainerHigh

												Behavior on color { ColorAnimation { duration: 150 } }

												RowLayout {
													id: linkItemRow
													anchors.centerIn: parent
													spacing: 8
													MaterialSymbol { icon: modelData.icon; iconSize: 18; color: linkItemMouse.containsMouse ? col.onPrimaryContainer : col.primary }
													Text { text: modelData.text; font.pixelSize: 12; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: linkItemMouse.containsMouse ? col.onPrimaryContainer : col.onSurface }
												}

												MouseArea {
													id: linkItemMouse
													anchors.fill: parent
													cursorShape: Qt.PointingHandCursor
													hoverEnabled: true
													onClicked: { openUrlProcess.command = ["xdg-open", modelData.url]; openUrlProcess.running = true }
												}
											}
										}
									}
								}
							}

							Rectangle {
								Layout.fillWidth: true
								Layout.preferredHeight: creditsContent.height + 30
								radius: 16
								color: col.surfaceContainer

								ColumnLayout {
									id: creditsContent
									anchors.left: parent.left
									anchors.right: parent.right
									anchors.top: parent.top
									anchors.margins: 15
									spacing: 8

									Text { text: "Credits"; font.pixelSize: 14; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; font.weight: 700; color: col.onSurface }
									Text { text: "Built with Quickshell framework"; font.pixelSize: 12; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant }
									Text { text: "Color generation by matugen"; font.pixelSize: 12; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant }
									Text { text: "Icons from Material Symbols"; font.pixelSize: 12; font.family: configAdapter ? configAdapter.fontFamily : "Rubik"; color: col.onSurfaceVariant }
								}
							}

							Item { Layout.preferredHeight: 20 }
						}
					}
				}
			}
		}
	}

	FileDialog {
		id: wallpaperDialog
		title: "Select Wallpaper"
		currentFolder: "file://" + Quickshell.env("HOME") + "/.local/wallpapers"
		nameFilters: ["Images (*.jpg *.jpeg *.png *.webp)"]

		onAccepted: {
			var path = selectedFile.toString().replace("file://", "")
			var mode = darkModeIndex === 0 ? "dark" : "light"
			var scheme = schemeMapping[colorSchemeIndex]
			var contrast = configAdapter ? configAdapter.matugenContrast : 0.0
			var genScript = Qt.resolvedUrl("../col_gen/generate").toString().replace("file://", "")
			matugenProcess.command = [
				genScript, "image", path,
				"-m", mode, "-s", scheme, "-c", contrast.toString()
			]
			matugenProcess.running = true
		}
	}

	GlobalShortcut {
		name: "SettingsToggle"
		description: "Toggle settings window"
		appid: "quickshell"
		onPressed: Gstate.settingsOpen = !Gstate.settingsOpen
	}
}
