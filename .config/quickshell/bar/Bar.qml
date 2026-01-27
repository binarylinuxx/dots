import Quickshell
import QtQuick
import qs.bar.widgets
import qs.widgets

PanelWindow {
	id: barWindow

	// Config values with defaults
	property int barHeight: cfg ? cfg.barHeight : 35
	property int barRadius: cfg ? cfg.barRadius : 20
	property int barGap: cfg ? cfg.barGap : 5
	property bool barFloating: cfg ? cfg.barFloating : false
	property bool barOnTop: cfg ? cfg.barOnTop : true
	property int cornerSize: cfg ? cfg.screenCornerSize : 25

	// Dynamic anchors based on config
	anchors {
		top: barOnTop
		bottom: !barOnTop
		left: true
		right: true
	}

	// Dynamic margins for floating mode
	margins {
		top: barFloating && barOnTop ? barGap : 0
		bottom: barFloating && !barOnTop ? barGap : 0
		left: barFloating ? barGap : 0
		right: barFloating ? barGap : 0
	}

	height: barHeight
	color: barFloating ? "transparent" : "black"

	Rectangle {
		id: panelBackground
		anchors.fill: parent
		color: col.background

		// Floating: all corners rounded
		// Top: bottom corners rounded
		// Bottom: top corners rounded
		topLeftRadius: barFloating ? barRadius : (barOnTop ? cornerSize : 0)
		topRightRadius: barFloating ? barRadius : (barOnTop ? cornerSize : 0)
		bottomLeftRadius: barFloating ? barRadius : (!barOnTop ? cornerSize : 0)
		bottomRightRadius: barFloating ? barRadius : (!barOnTop ? cornerSize : 0)

		Workspaces {
			anchors.centerIn: parent
		}
		
		SystemTray {
			anchors.fill: parent
		}
		UserProfile {
			anchors.fill: parent
		}
	}

	// Screen corners - independent from bar position (hidden when bar is floating)
	ScreenCorner {
		cornerDirection: ScreenCorner.TopLeft
		cornerWidth: cornerSize
		cornerHeight: cornerSize
		cornerColor: col.background
		visible: cfg ? (cfg.screenCorners && !barFloating) : true
	}

	ScreenCorner {
		cornerDirection: ScreenCorner.TopRight
		cornerWidth: cornerSize
		cornerHeight: cornerSize
		cornerColor: col.background
		visible: cfg ? (cfg.screenCorners && !barFloating) : true
	}

	ScreenCorner {
		cornerDirection: ScreenCorner.BottomLeft
		cornerWidth: cornerSize
		cornerHeight: cornerSize
		cornerColor: col.background
		visible: cfg ? (cfg.screenCorners && !barFloating) : true
	}

	ScreenCorner {
		cornerDirection: ScreenCorner.BottomRight
		cornerWidth: cornerSize
		cornerHeight: cornerSize
		cornerColor: col.background
		visible: cfg ? (cfg.screenCorners && !barFloating) : true
	}
}
