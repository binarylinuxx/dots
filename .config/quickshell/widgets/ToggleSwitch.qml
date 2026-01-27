import QtQuick

Item {
	id: toggleSwitch

	property int switchWidth: 50
	property int switchHeight: 26
	property bool checked: false
	property int radius: switchHeight / 2
	property color activeColor: col.primary
	property color inactiveColor: col.surfaceContainerHighest
	property color handleColor: checked ? col.onPrimary : col.outline
	property color borderColor: col.outline
	property int borderWidth: checked ? 0 : 2

	signal clicked()
	signal toggled(bool newState)

	implicitWidth: switchWidth
	implicitHeight: switchHeight

	Rectangle {
		id: background
		anchors.fill: parent
		radius: toggleSwitch.radius
		color: toggleSwitch.checked ? toggleSwitch.activeColor : toggleSwitch.inactiveColor
		border.color: toggleSwitch.borderColor
		border.width: toggleSwitch.borderWidth

		Behavior on color {
			ColorAnimation { duration: 150 }
		}

		Rectangle {
			id: handle
			width: parent.height - 6
			height: parent.height - 6
			y: 3
			x: toggleSwitch.checked ? (parent.width - width - 3) : 3
			radius: height / 2
			color: toggleSwitch.handleColor

			Behavior on x {
				NumberAnimation { 
					duration: 200
					easing.type: Easing.OutCubic
				}
			}
		}

		MouseArea {
			anchors.fill: parent
			cursorShape: Qt.PointingHandCursor
			onClicked: {
				toggleSwitch.checked = !toggleSwitch.checked
				toggleSwitch.clicked()
				toggleSwitch.toggled(toggleSwitch.checked)
			}
		}
	}
}
