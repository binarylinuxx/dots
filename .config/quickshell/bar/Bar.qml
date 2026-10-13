import Quickshell
import QtQuick
import qs.bar.widgets
import qs.widgets
import qs.services
import PluginManager

Item {
	id: barWindow
	z: 100
	signal taskbarToggled()

	property int barHeight: Gstate.barHeight
	property int barWidth: Gstate.barWidth
	property int barRadius: cfg ? cfg.barRadius : 20
	property int barGap: Gstate.barGap
	property bool barFloating: Gstate.barFloating
	property string barPosition: Gstate.barPosition
	property string barPreset: Gstate.barPreset
	property bool verticalBar: Gstate.verticalBar
	property string barEdge: Gstate.barEdge
	property bool barOnTop: barEdge === "top"
	property int cornerSize: cfg ? cfg.screenCornerSize : 25
	property int dockedRadius: barRadius
	readonly property int blobTopLeftRadius: barFloating ? barRadius : (verticalBar ? (barEdge === "left" ? dockedRadius : 0) : (barOnTop ? dockedRadius : 0))
	readonly property int blobTopRightRadius: barFloating ? barRadius : (verticalBar ? (barEdge === "right" ? dockedRadius : 0) : (barOnTop ? dockedRadius : 0))
	readonly property int blobBottomRightRadius: barFloating ? barRadius : (verticalBar ? (barEdge === "right" ? dockedRadius : 0) : (!barOnTop ? dockedRadius : 0))
	readonly property int blobBottomLeftRadius: barFloating ? barRadius : (verticalBar ? (barEdge === "left" ? dockedRadius : 0) : (!barOnTop ? dockedRadius : 0))

	x: verticalBar ? (barEdge === "left" ? (barFloating ? barGap : 0) : parent.width - barWidth - (barFloating ? barGap : 0)) : (barFloating ? barGap : 0)
	y: verticalBar ? (barFloating ? barGap : 0) : (barOnTop ? (barFloating ? barGap : 0) : parent.height - barHeight - (barFloating ? barGap : 0))
	width: verticalBar ? barWidth : parent.width - (barFloating ? barGap * 2 : 0)
	height: verticalBar ? parent.height - (barFloating ? barGap * 2 : 0) : barHeight

	// BlobGroup draws the contiguous outer panel; this item retains only content.
	Rectangle {
		id: panelBackground
		anchors.fill: parent
		color: "transparent"

		// Docked vertical bars round their side corners; floating bars round all corners.
		topLeftRadius: barWindow.blobTopLeftRadius
		topRightRadius: barWindow.blobTopRightRadius
		bottomLeftRadius: barWindow.blobBottomLeftRadius
		bottomRightRadius: barWindow.blobBottomRightRadius
	}

	Workspaces {
		anchors.centerIn: parent
		visible: !barWindow.verticalBar
	}
	SystemTray {
		id: systemTray
		anchors.fill: parent
		visible: !barWindow.verticalBar
	}
	UserProfile {
		anchors.fill: parent
		visible: !barWindow.verticalBar
	}
	VerticalBarModules {
		anchors.fill: parent
		visible: barWindow.verticalBar
	}

	// Left-positioned plugin bar items.
	Row {
		id: leftPluginBarRow
		anchors.left: parent.left
		anchors.leftMargin: 8
		anchors.verticalCenter: parent.verticalCenter
		visible: !barWindow.verticalBar
		spacing: 6

		Repeater {
			model: {
				var all = PluginRegistry.byKind("bar")
				var disabled = (cfg && cfg.disabledPlugins) ? cfg.disabledPlugins : []
				return all.filter(function(p) {
					return p.kindData.position === "left" && disabled.indexOf(p.id) === -1
				})
			}
			delegate: Loader {
				anchors.verticalCenter: parent.verticalCenter
				source: modelData.kindData.componentUrl
				onLoaded: if (item) item.plugin = modelData
			}
		}
	}

	// Right-positioned plugin bar items sit just left of SystemTray.
	Row {
		id: pluginBarRow
		anchors.right: parent.right
		anchors.rightMargin: systemTray.contentWidth + 1
		anchors.verticalCenter: parent.verticalCenter
		visible: !barWindow.verticalBar
		spacing: 6

		Repeater {
			model: {
				var all = PluginRegistry.byKind("bar")
				var disabled = (cfg && cfg.disabledPlugins) ? cfg.disabledPlugins : []
				return all.filter(function(p) {
					return p.kindData.position !== "left" && disabled.indexOf(p.id) === -1
				})
			}
			delegate: Loader {
				anchors.verticalCenter: parent.verticalCenter
				source: modelData.kindData.componentUrl
				onLoaded: if (item) item.plugin = modelData
			}
		}
	}

}
