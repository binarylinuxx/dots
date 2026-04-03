import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Quickshell
import Quickshell.Io
import Quickshell.Widgets
import Quickshell.Wayland
import qs.widgets
import qs.services

PanelWindow {
	id: root

	exclusionMode: ExclusionMode.Ignore
	WlrLayershell.layer: WlrLayer.Overlay
	WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
	color: "transparent"

	anchors { top: true; left: true; bottom: true; right: true }

	property bool firstStartFileExists: false
	visible: firstStartFileExists

	FileView {
		id: firstStartFile
		path: "/tmp/blx-shell-first-start"
		watchChanges: true
		onFileChanged: root.firstStartFileExists = true
		onLoadFailed: root.firstStartFileExists = false
		Component.onCompleted: {
			root.firstStartFileExists = (firstStartFile.text !== undefined && firstStartFile.text !== null)
		}
	}

	FileView {
		id: configFile
		path: Qt.resolvedUrl("../config.json")
		watchChanges: false
		adapter: JsonAdapter {
			id: setupCfg
			property bool barFloating: cfg ? cfg.barFloating : false
			property bool barOnTop: cfg ? cfg.barOnTop : true
			property string barPosition: cfg ? cfg.barPosition : "bottom"
			property int barHeight: cfg ? cfg.barHeight : 35
			property int barRadius: cfg ? cfg.barRadius : 20
			property int barGap: cfg ? cfg.barGap : 5
			property bool screenCorners: cfg ? cfg.screenCorners : true
			property int screenCornerSize: cfg ? cfg.screenCornerSize : 25
			property string matugenMode: cfg ? cfg.matugenMode : "dark"
			property string matugenScheme: cfg ? cfg.matugenScheme : "tonal-spot"
			property real matugenContrast: cfg ? cfg.matugenContrast : 0.0
			property int workspaceCount: cfg ? cfg.workspaceCount : 10
			property string workspaceStyle: cfg ? cfg.workspaceStyle : "dots"
			property bool showSystemTray: cfg ? cfg.showSystemTray : true
			property bool dndEnabled: cfg ? cfg.dndEnabled : false
			property string clockFormat: cfg ? cfg.clockFormat : "HH:mm"
			property string clockPreset: cfg ? cfg.clockPreset : "time24"
			property string fontFamily: cfg ? cfg.fontFamily : "Rubik"
			property int fontSize: cfg ? cfg.fontSize : 14
			property string animationSpeed: cfg ? cfg.animationSpeed : "normal"
			property bool wallpaperParallax: cfg ? cfg.wallpaperParallax : true
			property real wallpaperParallaxStrength: cfg ? cfg.wallpaperParallaxStrength : 0.1
			property int wallpaperTransitionDuration: cfg ? cfg.wallpaperTransitionDuration : 600
			property string launcherPreset: cfg ? cfg.launcherPreset : "default"
			property int launcherWidth: cfg ? cfg.launcherWidth : 400
			property int launcherItemHeight: cfg ? cfg.launcherItemHeight : 55
			property int launcherRadius: cfg ? cfg.launcherRadius : 25
			property int launcherMaxItems: cfg ? cfg.launcherMaxItems : 8
			property bool launcherShowIcons: cfg ? cfg.launcherShowIcons : true
			property bool launcherShowDescriptions: cfg ? cfg.launcherShowDescriptions : true
			property bool launcherSearchAtTop: cfg ? cfg.launcherSearchAtTop : true
			property bool launcherEmojiMode: cfg ? cfg.launcherEmojiMode : true
			property bool launcherClipboardMode: cfg ? cfg.launcherClipboardMode : true
			property bool launcherWallpaperMode: cfg ? cfg.launcherWallpaperMode : true
			property int launcherHeight: cfg ? cfg.launcherHeight : 510
			property bool desktopWidgets: cfg ? cfg.desktopWidgets : true
			property int gridColumns: cfg ? cfg.gridColumns : 16
			property int gridRows: cfg ? cfg.gridRows : 9
			property int widgetRadius: cfg ? cfg.widgetRadius : 12
			property int widgetBorderWidth: cfg ? cfg.widgetBorderWidth : 1
			property string widgetBorderColor: cfg ? cfg.widgetBorderColor : ""
			property string widgetBackgroundColor: cfg ? cfg.widgetBackgroundColor : ""
			property real widgetOpacity: cfg ? cfg.widgetOpacity : 0.85
			property bool weatherUseApiProvider: cfg ? cfg.weatherUseApiProvider : false
			property string weatherProvider: cfg ? cfg.weatherProvider : "wttr"
			property string weatherCity: cfg ? cfg.weatherCity : ""
			property string weatherApiKey: cfg ? cfg.weatherApiKey : ""
			property int sidebarTopPadding: cfg ? cfg.sidebarTopPadding : 240
			property bool nightLightEnabled: cfg ? cfg.nightLightEnabled : false
			property real nightLightTemperature: cfg ? cfg.nightLightTemperature : 0.6
			property real nightLightStrength: cfg ? cfg.nightLightStrength : 0.45
		property string wallhavenApiKey: cfg ? cfg.wallhavenApiKey : ""
		property string wallhavenPurityMode: cfg ? cfg.wallhavenPurityMode : "sfw"
		property string pexelsApiKey: cfg ? cfg.pexelsApiKey : ""
			property bool dynamicWorkspaces: cfg ? cfg.dynamicWorkspaces : false
		}
	}

	property int currentPage: 0
	property int totalPages: 4

	Process {
		id: matugenProcess
		stderr: SplitParser { onRead: data => console.log("[matugen]", data) }
	}

	Process {
		id: deleteFirstStartProcess
		command: ["rm", "-f", "/tmp/blx-shell-first-start"]
		onExited: root.firstStartFileExists = false
	}

	function finish() {
		configFile.writeAdapter()
		deleteFirstStartProcess.running = true
	}

	Rectangle {
		anchors.fill: parent
		color: col.background

		ColumnLayout {
			anchors.fill: parent
			anchors.margins: 32
			spacing: 0

			// Header
			RowLayout {
				Layout.fillWidth: true
				spacing: 14

				Rectangle {
					width: 42; height: 42; radius: 14
					color: col.primaryContainer
					MaterialSymbol {
						anchors.centerIn: parent
						icon: ["wifi", "palette", "schedule", "verified"][currentPage]
						iconSize: 24
						color: col.onPrimaryContainer
					}
				}

				ColumnLayout {
					spacing: 2
					Text {
						text: ["Network", "Look & Feel", "Time Format", "System Check"][currentPage]
						font.pixelSize: 22; font.weight: 700
						font.family: cfg ? cfg.fontFamily : "Rubik"
						color: col.onSurface
					}
					Text {
						text: ["Step 1 of 4", "Step 2 of 4", "Step 3 of 4", "Step 4 of 4"][currentPage]
						font.pixelSize: 12
						font.family: cfg ? cfg.fontFamily : "Rubik"
						color: col.onSurfaceVariant
					}
				}

				Item { Layout.fillWidth: true }

				Row {
					spacing: 8
					Repeater {
						model: 4
						Rectangle {
							width: index === currentPage ? 24 : 8; height: 8; radius: 4
							color: index <= currentPage ? col.primary : col.surfaceContainerHigh
							Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
							Behavior on color { ColorAnimation { duration: 200 } }
						}
					}
				}
			}

			Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.4; Layout.topMargin: 18; Layout.bottomMargin: 18 }

			StackLayout {
				Layout.fillWidth: true
				Layout.fillHeight: true
				currentIndex: currentPage

				FLNetworkPage {}
				FLLookFeelPage {}
				FLTimePage {}
				FLSysCheckPage {}
			}

			Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.4; Layout.topMargin: 18; Layout.bottomMargin: 18 }

			RowLayout {
				Layout.fillWidth: true
				spacing: 10

				Rectangle {
					width: backRow.implicitWidth + 24; height: 40; radius: 20
					color: backMouse.containsMouse ? col.surfaceContainerHigh : col.surfaceContainer
					visible: currentPage > 0
					Behavior on color { ColorAnimation { duration: 150 } }
					RowLayout { id: backRow; anchors.centerIn: parent; spacing: 6
						MaterialSymbol { icon: "arrow_back"; iconSize: 18; color: col.onSurfaceVariant }
						Text { text: "Back"; font.pixelSize: 13; font.family: cfg ? cfg.fontFamily : "Rubik"; font.weight: 600; color: col.onSurfaceVariant }
					}
					MouseArea { id: backMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: currentPage-- }
				}

				Item { Layout.fillWidth: true }

				Rectangle {
					width: skipRow.implicitWidth + 24; height: 40; radius: 20
					color: skipMouse.containsMouse ? col.surfaceContainerHigh : "transparent"
					visible: currentPage < totalPages - 1
					Behavior on color { ColorAnimation { duration: 150 } }
					RowLayout { id: skipRow; anchors.centerIn: parent; spacing: 6
						Text { text: "Skip"; font.pixelSize: 13; font.family: cfg ? cfg.fontFamily : "Rubik"; font.weight: 600; color: col.onSurfaceVariant }
					}
					MouseArea { id: skipMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: currentPage++ }
				}

				Rectangle {
					width: nextRow.implicitWidth + 28; height: 40; radius: 20
					color: nextMouse.containsMouse ? col.primaryContainer : col.primary
					Behavior on color { ColorAnimation { duration: 150 } }
					RowLayout {
						id: nextRow; anchors.centerIn: parent; spacing: 6
						Text {
							text: currentPage === totalPages - 1 ? "Finish" : "Next"
							font.pixelSize: 13; font.family: cfg ? cfg.fontFamily : "Rubik"; font.weight: 700
							color: nextMouse.containsMouse ? col.onPrimaryContainer : col.onPrimary
						}
						MaterialSymbol {
							icon: currentPage === totalPages - 1 ? "check" : "arrow_forward"
							iconSize: 18
							color: nextMouse.containsMouse ? col.onPrimaryContainer : col.onPrimary
						}
					}
					MouseArea {
						id: nextMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
						onClicked: {
							if (currentPage === totalPages - 1) finish()
							else currentPage++
						}
					}
				}
			}
		}
	}
}
