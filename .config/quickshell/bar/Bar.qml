import Quickshell
import Quickshell.Wayland
import QtQuick
import qs.bar.widgets
import qs.widgets
import qs.services
import PluginManager

PanelWindow {
	id: barWindow
	WlrLayershell.layer: WlrLayer.Bottom

	property int barHeight: cfg ? cfg.barHeight : 35
	property int barWidth: cfg && cfg.barWidth ? cfg.barWidth : 46
	property int barRadius: cfg ? cfg.barRadius : 20
	property int barGap: cfg ? cfg.barGap : 5
	property bool barFloating: cfg ? cfg.barFloating : false
	property string barPosition: cfg ? (cfg.barPosition || (cfg.barOnTop ? "top" : "bottom")) : "top"
	property string barPreset: cfg ? (cfg.barPreset || "horizontal") : "horizontal"
	property bool verticalBar: barPreset.indexOf("vertical") === 0 || barPosition === "left" || barPosition === "right"
	property bool barOnTop: !verticalBar && (barPosition === "top" || (cfg ? cfg.barOnTop : true))
	property int cornerSize: cfg ? cfg.screenCornerSize : 25
	property int dockedRadius: barRadius

	anchors {
		top: verticalBar || barOnTop
		bottom: verticalBar || !barOnTop
		left: !verticalBar || barPosition === "left"
		right: !verticalBar || barPosition === "right"
	}

	margins {
		top: barFloating ? barGap : 0
		bottom: barFloating ? barGap : 0
		left: barFloating ? barGap : 0
		right: barFloating ? barGap : 0
	}

	implicitHeight: verticalBar ? 1 : barHeight
	implicitWidth: verticalBar ? barWidth : 1
	color: barFloating ? "transparent" : "black"

	Rectangle {
		id: panelBackground
		anchors.fill: parent
		color: col.background

		// Docked vertical bars round their side corners; floating bars round all corners.
		topLeftRadius: barFloating ? barRadius : (verticalBar ? (barPosition === "left" ? dockedRadius : 0) : (barOnTop ? dockedRadius : 0))
		topRightRadius: barFloating ? barRadius : (verticalBar ? (barPosition === "right" ? dockedRadius : 0) : (barOnTop ? dockedRadius : 0))
		bottomLeftRadius: barFloating ? barRadius : (verticalBar ? (barPosition === "left" ? dockedRadius : 0) : (!barOnTop ? dockedRadius : 0))
		bottomRightRadius: barFloating ? barRadius : (verticalBar ? (barPosition === "right" ? dockedRadius : 0) : (!barOnTop ? dockedRadius : 0))
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

	// Screen corners
	ScreenCorner {
		cornerDirection: ScreenCorner.TopLeft
		cornerWidth: cornerSize; cornerHeight: cornerSize
		cornerColor: col.background
		visible: cfg ? (cfg.screenCorners && !barFloating) : true
	}
	ScreenCorner {
		cornerDirection: ScreenCorner.TopRight
		cornerWidth: cornerSize; cornerHeight: cornerSize
		cornerColor: col.background
		visible: cfg ? (cfg.screenCorners && !barFloating) : true
	}
	ScreenCorner {
		cornerDirection: ScreenCorner.BottomLeft
		cornerWidth: cornerSize; cornerHeight: cornerSize
		cornerColor: col.background
		visible: cfg ? (cfg.screenCorners && !barFloating) : true
	}
	ScreenCorner {
		cornerDirection: ScreenCorner.BottomRight
		cornerWidth: cornerSize; cornerHeight: cornerSize
		cornerColor: col.background
		visible: cfg ? (cfg.screenCorners && !barFloating) : true
	}
}
