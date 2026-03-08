import QtQuick
import Quickshell
import qs.services

Item {
	anchors.fill: parent

	Rectangle {
		anchors.right: parent.right
		anchors.rightMargin: 1
		anchors.verticalCenter: parent.verticalCenter
		height: 33
		width: trayRow.width + 5
		radius: 20
		color: col.background

		Row {
			id: trayRow
			anchors.centerIn: parent
			spacing: 3
			height: parent.height

			QuickButtons {
				id: quickButtons
				height: 33
			}
			Clock {
				id: clockWidget
				height: parent.height
				sidebarOpen: Gstate.sidebarOpen
				onClicked: Gstate.sidebarOpen = !Gstate.sidebarOpen
			}
			Network {
				id: networkWidget
				height: parent.height
				taskbarOpen: Gstate.sidebarOpen
			}
			Audio {
				id: audioWidget
				height: parent.height
				taskbarOpen: Gstate.sidebarOpen
			}
			Battery {
				id: batteryWidget
				height: parent.height
			}
		}
	}
}
