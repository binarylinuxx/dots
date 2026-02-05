import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Io
import Quickshell.Wayland

PanelWindow {
	id: powerMenu
	property bool showing: false

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

	contentItem {
		focus: true
		Keys.onPressed: event => {
			if (event.key === Qt.Key_Escape) {
				powerMenu.showing = false;
			} else {
				for (let i = 0; i < buttonModel.length; i++) {
					if (event.key === buttonModel[i].key) {
						runCommand(buttonModel[i].command);
					}
				}
			}
		}
	}

	// Button definitions
	property var buttonModel: [
		{ icon: "lock",          label: "Lock",      key: Qt.Key_L, command: "qs ipc call lockscreen lock" },
		{ icon: "logout",        label: "Logout",    key: Qt.Key_E, command: "loginctl terminate-user $USER" },
		{ icon: "bedtime",       label: "Suspend",   key: Qt.Key_U, command: "systemctl suspend" },
		{ icon: "power_settings_new", label: "Shutdown", key: Qt.Key_S, command: "systemctl poweroff" },
		{ icon: "restart_alt",   label: "Reboot",    key: Qt.Key_R, command: "systemctl reboot" },
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
				width: 140
				height: 140
				radius: btnMouse.containsMouse ? 100 : 28
				color: btnMouse.containsMouse
					? (col.primaryContainer || "#331443")
					: (col.surfaceContainer || "#221f22")

				Behavior on color { ColorAnimation { duration: 200 } }
				Behavior on radius { NumberAnimation { duration: 200 } }

				scale: btnMouse.pressed ? 0.92 : (btnMouse.containsMouse ? 1.05 : 1.0)
				Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

				Column {
					anchors.centerIn: parent
					spacing: 12

					Text {
						anchors.horizontalCenter: parent.horizontalCenter
						text: modelData.icon
						font.family: "Material Symbols Rounded"
						font.pixelSize: 40
						color: btnMouse.containsMouse
							? (col.onPrimaryContainer || "#c79fd7")
							: (col.onSurfaceVariant || "#cec3ce")

						Behavior on color { ColorAnimation { duration: 200 } }
					}

					Text {
						anchors.horizontalCenter: parent.horizontalCenter
						text: modelData.label
						font.family: "Rubik"
						font.pixelSize: 14
						font.weight: Font.Medium
						color: btnMouse.containsMouse
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
				}
			}
		}
	}

	// ── Hint text at bottom ──
	Text {
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.bottom: parent.bottom
		anchors.bottomMargin: 40
		text: "Press ESC to cancel"
		font.family: "Rubik"
		font.pixelSize: 14
		color: col.onSurfaceVariant || "#cec3ce"
		opacity: 0.5
	}
}
