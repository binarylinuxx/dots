import QtQuick
import Quickshell.Io
import qs.services
import qs.widgets
import qs.RoundedPolygon

Item {
	id: root
	property var plugin: null
	property string pluginPath: plugin ? plugin.path : ""

	FileView {
		id: configFile
		path: pluginPath ? "file://" + pluginPath + "/config.json" : ""
		watchChanges: true
		onFileChanged: reload()
		onAdapterUpdated: writeAdapter()

		adapter: JsonAdapter {
			id: cfgAdapter
			property int sides: 7
			property real innerRatio: 0.84
			property real outerRounding: 0.45
			property real innerRounding: 0.45
			property bool morphWithTime: true
			property bool showSeconds: true
			property bool showTicks: true
			property int clockSize: 280
		}
	}

	property int hours:   0
	property int minutes: 0
	property int seconds: 0

	function updateTime() {
		var now = new Date()
		root.hours   = now.getHours() % 12
		root.minutes = now.getMinutes()
		root.seconds = now.getSeconds()
	}

	Component.onCompleted: updateTime()

	Timer {
		interval: 1000
		running: true
		repeat: true
		triggeredOnStart: true
		onTriggered: updateTime()
	}

	readonly property real hourAngle:   -90 + (360 / 12) * (hours + minutes / 60)
	readonly property real minuteAngle: -90 + (360 / 60) * (minutes + seconds / 60)
	readonly property real secondAngle: -90 + (360 / 60) * seconds

	readonly property int sz: cfgAdapter ? cfgAdapter.clockSize : 280

	readonly property int cookieSides: {
		if (!cfgAdapter || !cfgAdapter.morphWithTime) return cfgAdapter ? cfgAdapter.sides : 7
		return cfgAdapter.sides + Math.floor(minutes / 15)
	}

	Item {
		id: clockItem
		anchors.centerIn: parent
		width: sz
		height: sz

		// Cookie face
		ParametricCookie {
			anchors.centerIn: parent
			size: sz
			sides:         cookieSides
			innerRatio:    cfgAdapter ? cfgAdapter.innerRatio    : 0.84
			outerRounding: cfgAdapter ? cfgAdapter.outerRounding : 0.45
			innerRounding: cfgAdapter ? cfgAdapter.innerRounding : 0.45
			color:         col.primaryContainer
			morphDuration: 1200
			strokeWidth:   2
			strokeColor:   col.primary
		}

		// ── Minute tick lines (60) ──
		Repeater {
			model: 60
			delegate: Item {
				visible: cfgAdapter && cfgAdapter.showTicks
				anchors.fill: parent
				rotation: 360 / 60 * index
				Rectangle {
					anchors.left: parent.left
					anchors.verticalCenter: parent.verticalCenter
					anchors.leftMargin: sz * 0.06
					implicitWidth: 6
					implicitHeight: 2
					radius: 1
					color: col.onPrimaryContainer
					opacity: 0.4
				}
			}
		}

		// ── Hour tick lines (12, longer + thicker) ──
		Repeater {
			model: 12
			delegate: Item {
				visible: cfgAdapter && cfgAdapter.showTicks
				anchors.fill: parent
				rotation: 360 / 12 * index
				Rectangle {
					anchors.left: parent.left
					anchors.verticalCenter: parent.verticalCenter
					anchors.leftMargin: sz * 0.06
					implicitWidth: 16
					implicitHeight: 4
					radius: 2
					color: col.onPrimaryContainer
					opacity: 0.85
				}
			}
		}

		// ── Minute hand: tertiary, medium pill ──
		Item {
			id: minutePivot
			anchors.fill: parent
			rotation: minuteAngle
			z: 1
			Behavior on rotation {
				RotationAnimation { duration: 500; direction: RotationAnimation.Clockwise; easing.type: Easing.InOutQuad }
			}
			Rectangle {
				anchors.verticalCenter: parent.verticalCenter
				x: parent.width / 2 - height / 2
				width: sz * 0.40
				height: 10
				radius: height / 2
				color: col.tertiary
				antialiasing: true
			}
		}

		// ── Hour hand: primary, thick pill, fill with border ──
		Item {
			id: hourPivot
			anchors.fill: parent
			rotation: hourAngle
			z: 2
			Behavior on rotation {
				RotationAnimation { duration: 500; direction: RotationAnimation.Clockwise; easing.type: Easing.InOutQuad }
			}
			Rectangle {
				anchors.verticalCenter: parent.verticalCenter
				x: parent.width / 2 - height / 2
				width: sz * 0.28
				height: 18
				radius: height / 2
				color: col.primary
				border.color: col.primaryContainer
				border.width: 3
				antialiasing: true
			}
		}

		// ── Second hand: secondary, thin line + dot ──
		Item {
			id: secondPivot
			visible: cfgAdapter && cfgAdapter.showSeconds
			anchors.fill: parent
			rotation: secondAngle
			z: 3
			Behavior on rotation {
				RotationAnimation { duration: 200; direction: RotationAnimation.Clockwise; easing.type: Easing.OutCubic }
			}
			// Thin line
			Rectangle {
				anchors.verticalCenter: parent.verticalCenter
				x: parent.width / 2 - height / 2
				width: sz * 0.44
				height: 3
				radius: height / 2
				color: col.secondary
				antialiasing: true
			}
			// Classic dot along the hand
			Rectangle {
				anchors.verticalCenter: parent.verticalCenter
				x: parent.width / 2 + 36
				width: 12
				height: 12
				radius: 6
				color: col.secondary
				antialiasing: true
			}
		}

		// ── Date bubble at 3 o'clock (pentagon shape) ──
		Item {
			z: 2
			anchors.verticalCenter: parent.verticalCenter
			anchors.left: parent.horizontalCenter
			anchors.leftMargin: sz * 0.20
			width: 36
			height: 36

			RoundedPolygon {
				anchors.centerIn: parent
				type: "pentagon"
				size: 36
				color: col.tertiaryContainer
			}

			Text {
				anchors.centerIn: parent
				anchors.verticalCenterOffset: 2
				text: Qt.formatDate(new Date(), "d")
				color: col.onTertiaryContainer
				font.pixelSize: 12
				font.weight: Font.Bold
				font.family: "Rubik"
			}
		}

		// ── Center jewel (on top of everything) ──
		Rectangle {
			anchors.centerIn: parent
			width: 12; height: 12; radius: 6
			color: col.secondary
			border.width: 3
			border.color: col.primaryContainer
			z: 10
			antialiasing: true
		}
	}
}
