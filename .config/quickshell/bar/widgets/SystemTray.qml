import QtQuick
import Quickshell

Item {
	anchors.fill: parent
	property bool sidebarOpen: false

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
			/*
			TaskbarButton {
				id: taskbarBtn
				isOpen: sidebarOpen
				onClicked: sidebarOpen = !sidebarOpen
			}
			*/
			QuickButtons {
				id: quickButtons
				height: 33
			}
			Clock {
				id: clockWidget
				height: parent.height
			}
			Network {
				id: networkWidget
				height: parent.height
				taskbarOpen: sidebarOpen
			}

			Audio {
				id: audioWidget
				height: parent.height
				taskbarOpen: sidebarOpen
			}

			Battery {
				id: batteryWidget
				height: parent.height
			}
		}
	}
	
	Taskbar {
		isOpen: sidebarOpen
		onClose: sidebarOpen = false
	}
}
