import Quickshell
import Quickshell.Io
import QtQuick
import qs.widgets
import qs.services


Item {
	width: buttonsRow.width
	property int moduleRadius: cfg ? Math.max(8, Math.round(cfg.barRadius * 0.7)) : 14

	// Screenshot process
	Process {
		id: screenshotProcess
		running: false
	}

	// Color picker process
	Process {
		id: colorPickerProcess
		running: false
	}

	// Terminal process
	Process {
		id: terminalProcess
		running: false
	}

	// ── Pill background ───────────────────────────────────────────────────
	Rectangle {
		anchors.centerIn: parent
		width: parent.width
		height: 29
		radius: moduleRadius
		color: col.surfaceContainer
	}

	Row {
		id: buttonsRow
		anchors.centerIn: parent
			
			// Screenshot button
			Rectangle {
				width: 28
				height: 28
				color: screenshotHover.opacity > 0 ? col.surfaceContainerHigh : "transparent"
				radius: moduleRadius
				
				Rectangle {
					id: screenshotHover
					anchors.fill: parent
					radius: moduleRadius
					color: col.surfaceContainerHighest
					opacity: 0
					Behavior on opacity {
						NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
					}
				}
				
				MaterialSymbol {
					anchors.centerIn: parent
					icon: "crop_free"
					color: col.primary
					iconSize: 19
				}
				
				MouseArea {
					anchors.fill: parent
					hoverEnabled: true
					onEntered: screenshotHover.opacity = 0.3
					onExited: screenshotHover.opacity = 0
					onClicked: {
						screenshotProcess.command = ["sh", "-c", 
							"if command -v grimblast >/dev/null 2>&1; then " +
							"grimblast --notify copy area; " +
							"elif command -v grim >/dev/null 2>&1 && command -v slurp >/dev/null 2>&1; then " +
							"grim -g \"$(slurp)\" - | wl-copy; " +
							"elif command -v flameshot >/dev/null 2>&1; then " +
							"flameshot gui; " +
							"elif command -v spectacle >/dev/null 2>&1; then " +
							"spectacle -r; " +
							"else " +
							"notify-send 'Screenshot' 'No screenshot tool found'; " +
							"fi"
						]
						screenshotProcess.running = true
					}
				}
			}
			
			// Color picker button
			Rectangle {
				width: 28
				height: 28
				color: colorPickerHover.opacity > 0 ? col.surfaceContainerHigh : "transparent"
				radius: moduleRadius
				
				Rectangle {
					id: colorPickerHover
					anchors.fill: parent
					radius: moduleRadius
					color: col.surfaceContainerHighest
					opacity: 0
					Behavior on opacity {
						NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
					}
				}
				
				MaterialSymbol {
					anchors.centerIn: parent
					icon: "colorize"
					color: col.primary
					iconSize: 19
				}
				
				MouseArea {
					anchors.fill: parent
					hoverEnabled: true
					onEntered: colorPickerHover.opacity = 0.3
					onExited: colorPickerHover.opacity = 0
					onClicked: {
						colorPickerProcess.command = ["sh", "-c",
							"if command -v hyprpicker >/dev/null 2>&1; then " +
							"hyprpicker -a; " +
							"elif command -v matugen >/dev/null 2>&1; then " +
							"matugen color pick; " +
							"elif command -v gpick >/dev/null 2>&1; then " +
							"gpick -p -o; " +
							"else " +
							"notify-send 'Color Picker' 'No color picker tool found'; " +
							"fi"
						]
						colorPickerProcess.running = true
					}
				}
			}
			
			// Terminal button
			Rectangle {
				width: 28
				height: 28
				color: terminalHover.opacity > 0 ? col.surfaceContainerHigh : "transparent"
				radius: moduleRadius
				
				Rectangle {
					id: terminalHover
					anchors.fill: parent
					radius: moduleRadius
					color: col.surfaceContainerHighest
					opacity: 0
					Behavior on opacity {
						NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
					}
				}
				
				MaterialSymbol {
					anchors.centerIn: parent
					icon: "terminal"
					color: col.primary
					iconSize: 19
				}
				
				MouseArea {
					anchors.fill: parent
					hoverEnabled: true
					onEntered: terminalHover.opacity = 0.3
					onExited: terminalHover.opacity = 0
					onClicked: {
						terminalProcess.command = ["sh", "-c",
							"if command -v ghostty >/dev/null 2>&1; then " +
							"ghostty & " +
							"elif command -v alacritty >/dev/null 2>&1; then " +
							"alacritty & " +
							"elif command -v foot >/dev/null 2>&1; then " +
							"foot & " +
							"elif command -v wezterm >/dev/null 2>&1; then " +
							"wezterm & " +
							"elif command -v konsole >/dev/null 2>&1; then " +
							"konsole & " +
							"else " +
							"notify-send 'Terminal' 'No terminal emulator found'; " +
							"fi"
						]
						terminalProcess.running = true
					}
				}
			}

			// Record button
			Rectangle {
				id: recordBtn
				width: 28
				height: 28
				radius: moduleRadius
				color: ScreenRecorder.recording
					? Qt.rgba(col.error ? Qt.darker(col.error, 1.2) : 0.6, 0, 0, 0.25)
					: (recordHover.opacity > 0 ? col.surfaceContainerHigh : "transparent")

				Behavior on color { ColorAnimation { duration: 150 } }

				// Pulsing red dot while recording
				Rectangle {
					id: recordDot
					anchors.centerIn: parent
					width: ScreenRecorder.recording ? 10 : 0
					height: width
					radius: width / 2
					color: col.error || "#ff5449"
					visible: ScreenRecorder.recording

					SequentialAnimation on opacity {
						running: ScreenRecorder.recording
						loops: Animation.Infinite
						NumberAnimation { to: 0.3; duration: 700; easing.type: Easing.InOutSine }
						NumberAnimation { to: 1.0; duration: 700; easing.type: Easing.InOutSine }
					}
				}

				// Mic icon when not recording
				MaterialSymbol {
					anchors.centerIn: parent
					icon: "radio_button_checked"
					color: ScreenRecorder.recording ? "transparent" : col.primary
					iconSize: 19
					Behavior on color { ColorAnimation { duration: 150 } }
				}

				Rectangle {
					id: recordHover
					anchors.fill: parent
					radius: moduleRadius
					color: col.surfaceContainerHighest
					opacity: 0
					Behavior on opacity {
						NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
					}
				}

				MouseArea {
					anchors.fill: parent
					hoverEnabled: true
					cursorShape: Qt.PointingHandCursor
					onEntered: recordHover.opacity = ScreenRecorder.recording ? 0 : 0.3
					onExited:  recordHover.opacity = 0
					onClicked: ScreenRecorder.toggleRecording()
				}
			}

			// ── Timer — appears after record button, grows rightward ──────
			Item {
				id: timerExtension
				width: ScreenRecorder.recording ? timerLabel.implicitWidth + 12 : 0
				height: 28
				clip: true
				Behavior on width { NumberAnimation { duration: Gstate.animDuration * 1.5; easing.type: Easing.OutCubic } }

				Text {
					id: timerLabel
					anchors.left: parent.left
					anchors.leftMargin: 4
					anchors.verticalCenter: parent.verticalCenter
					text: ScreenRecorder.elapsedFormatted
					color: col.error || "#ff5449"
					font.pixelSize: 11
					font.weight: Font.Medium
					font.family: cfg ? cfg.fontFamily : "Rubik"
					opacity: ScreenRecorder.recording ? 1.0 : 0.0
					Behavior on opacity { NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic } }
				}
			}
		}
}
