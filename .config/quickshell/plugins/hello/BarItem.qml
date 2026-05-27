// Bar plugin item. Host sets `plugin` to the PluginRegistry entry for this
// instance. We do our own config via FileView pointed at the plugin dir.
import QtQuick
import Quickshell.Io
import qs.services
import qs.widgets

Item {
	id: root
	// Populated by host via Loader.
	property var plugin: null

	// Convenience
	readonly property string pluginPath: plugin ? plugin.path : ""
	readonly property string configPath: pluginPath + "/config.json"

	implicitWidth: row.implicitWidth + 16
	implicitHeight: 28

	// Per-plugin config (isolated). Writes live here, not in the shell config.
	FileView {
		id: configFile
		path: pluginPath ? "file://" + pluginPath + "/config.json" : ""
		watchChanges: true
		onFileChanged: reload()
		onAdapterUpdated: writeAdapter()

		adapter: JsonAdapter {
			id: cfgAdapter
			property string greeting: "Hello"
			property bool showIcon: true
			property int refreshSec: 60
			property int clickCount: 0
		}
	}

	Rectangle {
		anchors.fill: parent
		radius: cfg ? Math.max(8, Math.round(cfg.barRadius * 0.7)) : 14
		color: mouse.containsMouse ? col.primaryContainer : col.surfaceContainer
		Behavior on color { ColorAnimation { duration: Gstate.animDuration } }

		Row {
			id: row
			anchors.centerIn: parent
			spacing: 6

			MaterialSymbol {
				visible: cfgAdapter.showIcon
				icon: plugin ? plugin.icon : "extension"
				iconSize: 16
				color: mouse.containsMouse ? col.onPrimaryContainer : col.primary
				anchors.verticalCenter: parent.verticalCenter
			}

			Text {
				text: cfgAdapter.greeting + " (" + cfgAdapter.clickCount + ")"
				font.family: cfg ? cfg.fontFamily : "Rubik"
				font.pixelSize: 13
				font.weight: 600
				color: mouse.containsMouse ? col.onPrimaryContainer : col.onSurface
				anchors.verticalCenter: parent.verticalCenter
			}
		}

		MouseArea {
			id: mouse
			anchors.fill: parent
			hoverEnabled: true
			cursorShape: Qt.PointingHandCursor
			onClicked: cfgAdapter.clickCount += 1
		}
	}
}
