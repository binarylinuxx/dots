import QtQuick

Item {
	width: clock.width + 7

	property int moduleRadius: cfg ? Math.max(8, Math.round(cfg.barRadius * 0.7)) : 14
	property string fontFamily: cfg ? cfg.fontFamily : "Rubik"
	property string clockFormat: cfg ? cfg.clockFormat : "hh:mm AP"

	Rectangle {
		height: 28
		width: parent.width
		anchors.centerIn: parent
		radius: moduleRadius
		color: col.surfaceContainer

		Text {
			id: clock
			property var currentTime: new Date()
			Timer {
				interval: 1000
				repeat: true
				running: true
				onTriggered: {
					clock.currentTime = new Date();
				}
			}
			text: Qt.formatDateTime(currentTime, clockFormat)
			anchors.centerIn: parent
			font.family: fontFamily
			font.weight: 600
			font.pixelSize: 16
			color: col.primary
		}
	}
}
