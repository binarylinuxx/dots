import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Io
import Quickshell.Wayland

PanelWindow {
	id: powerMenu
	property bool showing: false
	property int selectedIndex: 0
	property string fontFamily: cfg ? cfg.fontFamily : "Rubik"

	visible: showing
	exclusionMode: ExclusionMode.Ignore
	WlrLayershell.layer: WlrLayer.Overlay
	WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
	color: "transparent"

	anchors {
		top: true
		left: true
		bottom: true
		right: true
	}

	onShowingChanged: {
		if (showing) selectedIndex = 0
	}

	contentItem {
		focus: true
		Keys.onPressed: event => {
			if (event.key === Qt.Key_Escape) {
				powerMenu.showing = false
			} else if (event.key === Qt.Key_Left) {
				selectedIndex = (selectedIndex - 1 + buttonModel.length) % buttonModel.length
			} else if (event.key === Qt.Key_Right) {
				selectedIndex = (selectedIndex + 1) % buttonModel.length
			} else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
				runCommand(buttonModel[selectedIndex].command)
			} else {
				for (let i = 0; i < buttonModel.length; i++) {
					if (event.key === buttonModel[i].key) {
						runCommand(buttonModel[i].command)
					}
				}
			}
		}
	}

	// Button definitions
	property var buttonModel: [
		{ icon: "lock",          label: "Lock",      key: Qt.Key_L, hint: "L", command: "qs ipc call lockscreen lock" },
		{ icon: "logout",        label: "Logout",    key: Qt.Key_E, hint: "E", command: "loginctl terminate-user $USER" },
		{ icon: "bedtime",       label: "Suspend",   key: Qt.Key_U, hint: "U", command: "systemctl suspend" },
		{ icon: "power_settings_new", label: "Shutdown", key: Qt.Key_S, hint: "S", command: "systemctl poweroff" },
		{ icon: "restart_alt",   label: "Reboot",    key: Qt.Key_R, hint: "R", command: "systemctl reboot" },
	]

	function runCommand(cmd: string): void {
		execProcess.command = ["sh", "-c", cmd];
		execProcess.startDetached();
		powerMenu.showing = false;
	}

	Process {
		id: execProcess
	}

	// ── Background: wallpaper + dark overlay ──
	Image {
		id: wallpaperBg
		anchors.fill: parent
		source: col.wallpaper || ""
		fillMode: Image.PreserveAspectCrop
		visible: col.wallpaper && col.wallpaper !== "" && status === Image.Ready
	}

	Rectangle {
		anchors.fill: parent
		color: col.background || "#151216"
		opacity: wallpaperBg.visible ? 0.7 : 1.0
	}

	// ── Click outside to close ──
	MouseArea {
		anchors.fill: parent
		onClicked: powerMenu.showing = false
	}

	// ── Buttons grid ──
	Row {
		anchors.centerIn: parent
		spacing: 20

		Repeater {
			model: powerMenu.buttonModel
			delegate: Rectangle {
				id: btnRect
				property bool isSelected: index === powerMenu.selectedIndex
				property bool isHovered: btnMouse.containsMouse
				property bool isActive: isSelected || isHovered

				width: 140
				height: 140
				radius: isActive ? 100 : 28
				color: isActive
					? (col.primaryContainer || "#331443")
					: (col.surfaceContainer || "#221f22")

				Behavior on color { ColorAnimation { duration: 200 } }
				Behavior on radius { NumberAnimation { duration: 200 } }

				scale: btnMouse.pressed ? 0.92 : (isActive ? 1.05 : 1.0)
				Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

				Column {
					anchors.centerIn: parent
					spacing: 12

					Text {
						anchors.horizontalCenter: parent.horizontalCenter
						text: modelData.icon
						font.family: "Material Symbols Rounded"
						font.pixelSize: 40
						color: isActive
							? (col.onPrimaryContainer || "#c79fd7")
							: (col.onSurfaceVariant || "#cec3ce")

						Behavior on color { ColorAnimation { duration: 200 } }
					}

					Text {
						anchors.horizontalCenter: parent.horizontalCenter
						text: modelData.label
						font.family: fontFamily
						font.pixelSize: 14
						font.weight: Font.Medium
						color: isActive
							? (col.onPrimaryContainer || "#c79fd7")
							: (col.onSurface || "#e8e0e5")

						Behavior on color { ColorAnimation { duration: 200 } }
					}
				}

				MouseArea {
					id: btnMouse
					anchors.fill: parent
					hoverEnabled: true
					cursorShape: Qt.PointingHandCursor
					onClicked: powerMenu.runCommand(modelData.command)
					onEntered: powerMenu.selectedIndex = index
				}
			}
		}
	}

	// ── Tooltip at bottom center ──
	Column {
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.bottom: parent.bottom
		anchors.bottomMargin: 40
		spacing: 8

		// Selected action tooltip
		Text {
			anchors.horizontalCenter: parent.horizontalCenter
			text: buttonModel[selectedIndex].label + " (" + buttonModel[selectedIndex].hint + ")"
			font.family: fontFamily
			font.pixelSize: 16
			font.weight: Font.Medium
			color: col.primary || "#adc6ff"
		}

		// Navigation hint
		Row {
			anchors.horizontalCenter: parent.horizontalCenter
			spacing: 16

			Row {
				spacing: 6
				Text {
					text: "arrow_back"
					font.family: "Material Symbols Rounded"
					font.pixelSize: 14
					color: col.onSurfaceVariant || "#cec3ce"
					opacity: 0.6
				}
				Text {
					text: "arrow_forward"
					font.family: "Material Symbols Rounded"
					font.pixelSize: 14
					color: col.onSurfaceVariant || "#cec3ce"
					opacity: 0.6
				}
				Text {
					text: "Navigate"
					font.family: fontFamily
					font.pixelSize: 12
					color: col.onSurfaceVariant || "#cec3ce"
					opacity: 0.6
				}
			}

			Text {
				text: "|"
				font.family: fontFamily
				font.pixelSize: 12
				color: col.onSurfaceVariant || "#cec3ce"
				opacity: 0.3
			}

			Row {
				spacing: 6
				Text {
					text: "keyboard_return"
					font.family: "Material Symbols Rounded"
					font.pixelSize: 14
					color: col.onSurfaceVariant || "#cec3ce"
					opacity: 0.6
				}
				Text {
					text: "Select"
					font.family: fontFamily
					font.pixelSize: 12
					color: col.onSurfaceVariant || "#cec3ce"
					opacity: 0.6
				}
			}

			Text {
				text: "|"
				font.family: fontFamily
				font.pixelSize: 12
				color: col.onSurfaceVariant || "#cec3ce"
				opacity: 0.3
			}

			Row {
				spacing: 6
				Text {
					text: "ESC"
					font.family: "JetBrains Mono"
					font.pixelSize: 11
					color: col.onSurfaceVariant || "#cec3ce"
					opacity: 0.6
				}
				Text {
					text: "Cancel"
					font.family: fontFamily
					font.pixelSize: 12
					color: col.onSurfaceVariant || "#cec3ce"
					opacity: 0.6
				}
			}
		}
	}
}
