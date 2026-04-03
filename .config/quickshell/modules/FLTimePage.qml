import QtQuick
import QtQuick.Layouts
import qs.widgets
import qs.services

Item {
	id: timePage
	property int timeChoice: 1

	onTimeChoiceChanged: {
		if (timeChoice === 0) {
			setupCfg.clockFormat = "hh:mm AP"
			setupCfg.clockPreset = "time12"
		} else {
			setupCfg.clockFormat = "HH:mm"
			setupCfg.clockPreset = "time24"
		}
		configFile.writeAdapter()
	}

	ColumnLayout {
		anchors.centerIn: parent
		spacing: 28
		width: parent.width * 0.6

		Text {
			Layout.alignment: Qt.AlignHCenter
			text: "How should time be displayed?"
			font.pixelSize: 16
			font.family: cfg ? cfg.fontFamily : "Rubik"
			font.weight: 600
			color: col.onSurfaceVariant
		}

		Text {
			Layout.alignment: Qt.AlignHCenter
			property var now: new Date()
			Timer { running: true; repeat: true; interval: 1000; onTriggered: parent.now = new Date() }
			text: {
				var h = now.getHours()
				var m = now.getMinutes().toString().padStart(2, "0")
				if (timeChoice === 0) {
					var h12 = h % 12 || 12
					return h12 + ":" + m + " " + (h < 12 ? "AM" : "PM")
				}
				return h.toString().padStart(2, "0") + ":" + m
			}
			font.pixelSize: 72
			font.family: "JetBrains Mono"
			font.weight: 700
			color: col.primary
		}

		RowLayout {
			Layout.alignment: Qt.AlignHCenter
			spacing: 16
			Repeater {
				model: [{ label: "12-hour", sub: "3:45 PM", value: 0 }, { label: "24-hour", sub: "15:45", value: 1 }]
				Rectangle {
					width: 180
					height: 90
					radius: 18
					color: timeChoice === modelData.value ? col.primaryContainer : col.surfaceContainer
					border.width: timeChoice === modelData.value ? 2 : 0
					border.color: col.primary
					Behavior on color { ColorAnimation { duration: 150 } }
					ColumnLayout { anchors.centerIn: parent; spacing: 4
						Text { Layout.alignment: Qt.AlignHCenter; text: modelData.label; font.pixelSize: 16; font.weight: 700; font.family: cfg ? cfg.fontFamily : "Rubik"; color: timeChoice === modelData.value ? col.onPrimaryContainer : col.onSurface }
						Text { Layout.alignment: Qt.AlignHCenter; text: modelData.sub; font.pixelSize: 14; font.family: "JetBrains Mono"; color: timeChoice === modelData.value ? col.primary : col.onSurfaceVariant }
					}
					MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: timeChoice = modelData.value }
				}
			}
		}

		Text {
			Layout.alignment: Qt.AlignHCenter
			text: "You can change this later in Settings > Clock."
			font.pixelSize: 11
			font.family: cfg ? cfg.fontFamily : "Rubik"
			color: col.onSurfaceVariant
			opacity: 0.7
		}
	}
}
