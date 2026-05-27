// Background/desktop widget. Same pattern: host sets `plugin`.
import QtQuick
import qs.services
import qs.widgets

Item {
	id: root
	property var plugin: null
	anchors.fill: parent

	Rectangle {
		anchors.fill: parent
		radius: 16
		color: col.surfaceContainer
		opacity: 0.9

		Column {
			anchors.centerIn: parent
			spacing: 8
			MaterialSymbol {
				icon: plugin ? plugin.icon : "waving_hand"
				iconSize: 48
				color: col.primary
				anchors.horizontalCenter: parent.horizontalCenter
			}
			Text {
				text: plugin ? plugin.name : "Plugin"
				font.family: cfg ? cfg.fontFamily : "Rubik"
				font.pixelSize: 18
				font.weight: 700
				color: col.onSurface
				anchors.horizontalCenter: parent.horizontalCenter
			}
			Text {
				text: plugin ? plugin.description : ""
				font.family: cfg ? cfg.fontFamily : "Rubik"
				font.pixelSize: 12
				color: col.onSurfaceVariant
				opacity: 0.8
				anchors.horizontalCenter: parent.horizontalCenter
			}
		}
	}
}
