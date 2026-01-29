import Quickshell
import Quickshell.Io
import QtQuick
import qs.widgets


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
	
	Rectangle {
		width: buttonsRow.width
		height: 29
		anchors.centerIn: parent
		radius: moduleRadius
		color: col.surfaceContainer
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
						NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
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
						NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
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
						NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
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
		}
	}
}
