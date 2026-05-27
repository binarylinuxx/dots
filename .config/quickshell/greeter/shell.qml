import Quickshell
import Quickshell.Wayland
import QtQuick

ShellRoot {
	PanelWindow {
		anchors {
			top: true
			bottom: true
			left: true
			right: true
		}
		exclusionMode: ExclusionMode.Normal
		WlrLayershell.layer: WlrLayer.Overlay
		WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
		color: "transparent"

		Greeter { anchors.fill: parent }
	}
}
