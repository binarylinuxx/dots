import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Quickshell
import Quickshell.Io
import qs.widgets
import qs.services

Item {
	id: netPage

	property string pingStatus: "idle"
	property string pendingSsid: ""
	property bool passwordDialogOpen: false

	readonly property bool isEthernet: NetworkManager.primaryConnectionType === "ethernet"
	readonly property bool isWifi:     NetworkManager.primaryConnectionType === "wifi"
	readonly property bool isConnected: NetworkManager.connectionStatus === "connected"

	Component.onCompleted: NetworkManager.scanWifi()

	Process {
		id: pingProcess
		command: ["ping", "-c", "1", "-W", "3", "1.1.1.1"]
		onExited: (code) => { netPage.pingStatus = code === 0 ? "ok" : "fail" }
	}

	ColumnLayout {
		anchors.fill: parent
		spacing: 12

		// Current connection card
		Rectangle {
			Layout.fillWidth: true
			height: connRow.implicitHeight + 28
			radius: 14
			color: isConnected ? col.primaryContainer : col.surfaceContainer
			border.width: 1
			border.color: isConnected ? col.primary : col.outlineVariant
			Behavior on color { ColorAnimation { duration: 200 } }

			RowLayout {
				id: connRow
				anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 16 }
				spacing: 14

				Rectangle {
					width: 42
					height: 42
					radius: 12
					color: isConnected ? col.primary : col.surfaceContainerHigh
					Behavior on color { ColorAnimation { duration: 200 } }
					MaterialSymbol {
						anchors.centerIn: parent
						icon: isEthernet ? "lan" : (isWifi ? "wifi" : "signal_wifi_off")
						iconSize: 22
						color: isConnected ? col.onPrimary : col.onSurfaceVariant
					}
				}

				ColumnLayout {
					Layout.fillWidth: true
					spacing: 2
					Text {
						text: isEthernet ? "Ethernet" : (isWifi ? (NetworkManager.wifiSsid || "Wi-Fi") : "No Connection")
						font.pixelSize: 14
						font.weight: 700
						font.family: cfg ? cfg.fontFamily : "Rubik"
						color: isConnected ? col.onPrimaryContainer : col.onSurface
					}
					Text {
						text: isEthernet ? "Wired - connected"
							: (isWifi ? NetworkManager.wifiSignalStrength + "% signal" : "Not connected")
						font.pixelSize: 12
						font.family: cfg ? cfg.fontFamily : "Rubik"
						color: isConnected ? col.onPrimaryContainer : col.onSurfaceVariant
						opacity: 0.8
					}
				}

				Rectangle {
					width: pingRow.implicitWidth + 20
					height: 34
					radius: 17
					color: pingMouse.containsMouse ? col.primary : col.surfaceContainerHigh
					visible: isConnected
					Behavior on color { ColorAnimation { duration: 150 } }
					RowLayout {
						id: pingRow
						anchors.centerIn: parent
						spacing: 6
						MaterialSymbol {
							icon: pingStatus === "ok" ? "check_circle" : (pingStatus === "fail" ? "error" : "network_check")
							iconSize: 16
							color: pingMouse.containsMouse ? col.onPrimary
								: (pingStatus === "ok" ? col.primary : (pingStatus === "fail" ? col.error : col.onSurfaceVariant))
						}
						Text {
							text: pingStatus === "running" ? "Testing..." : (pingStatus === "ok" ? "Online" : (pingStatus === "fail" ? "No reply" : "Test ping"))
							font.pixelSize: 12
							font.family: cfg ? cfg.fontFamily : "Rubik"
							font.weight: 600
							color: pingMouse.containsMouse ? col.onPrimary : col.onSurfaceVariant
						}
					}
					MouseArea {
						id: pingMouse
						anchors.fill: parent
						hoverEnabled: true
						cursorShape: Qt.PointingHandCursor
						enabled: pingStatus !== "running"
						onClicked: { netPage.pingStatus = "running"; pingProcess.running = true }
					}
				}
			}
		}

		// Error
		Rectangle {
			Layout.fillWidth: true
			height: errRow.implicitHeight + 16
			radius: 12
			color: col.errorContainer
			visible: NetworkManager.connectError !== ""
			RowLayout {
				id: errRow
				anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 12 }
				spacing: 8
				MaterialSymbol { icon: "error"; iconSize: 18; color: col.onErrorContainer }
				Text { Layout.fillWidth: true; text: NetworkManager.connectError; font.pixelSize: 12; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onErrorContainer; wrapMode: Text.WordWrap }
			}
		}

		// Wi-Fi header
		RowLayout {
			Layout.fillWidth: true
			spacing: 8
			MaterialSymbol { icon: "wifi_find"; iconSize: 18; color: col.primary }
			Text { text: "Wi-Fi Networks"; font.pixelSize: 13; font.weight: 700; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurface; Layout.fillWidth: true }
			Rectangle {
				width: rescanRow.implicitWidth + 16
				height: 30
				radius: 15
				color: rescanMouse.containsMouse ? col.primaryContainer : col.surfaceContainerHigh
				Behavior on color { ColorAnimation { duration: 150 } }
				RowLayout {
					id: rescanRow
					anchors.centerIn: parent
					spacing: 5
					MaterialSymbol { icon: "refresh"; iconSize: 15; color: rescanMouse.containsMouse ? col.onPrimaryContainer : col.onSurfaceVariant }
					Text { text: NetworkManager.scanning ? "Scanning..." : "Rescan"; font.pixelSize: 11; font.family: cfg ? cfg.fontFamily : "Rubik"; font.weight: 600; color: rescanMouse.containsMouse ? col.onPrimaryContainer : col.onSurfaceVariant }
				}
				MouseArea { id: rescanMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; enabled: !NetworkManager.scanning; onClicked: NetworkManager.rescanWifi() }
			}
		}

		// Wi-Fi list
		ScrollView {
			Layout.fillWidth: true
			Layout.fillHeight: true
			clip: true

			ColumnLayout {
				width: parent.parent.width
				spacing: 6

				Item {
					Layout.fillWidth: true
					height: 60
					visible: NetworkManager.scanning && NetworkManager.wifiNetworks.length === 0
					Text { anchors.centerIn: parent; text: "Scanning for networks..."; font.pixelSize: 13; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurfaceVariant }
				}
				Item {
					Layout.fillWidth: true
					height: 60
					visible: !NetworkManager.scanning && NetworkManager.wifiNetworks.length === 0
					Text { anchors.centerIn: parent; text: "No Wi-Fi networks found"; font.pixelSize: 13; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurfaceVariant }
				}

				Repeater {
					model: NetworkManager.wifiNetworks
					Rectangle {
						Layout.fillWidth: true
						height: 52
						radius: 12
						color: modelData.connected ? col.primaryContainer : (netRowMouse.containsMouse ? col.surfaceContainerHigh : col.surfaceContainer)
						border.width: NetworkManager.connectingTo === modelData.ssid ? 1 : 0
						border.color: col.primary
						Behavior on color { ColorAnimation { duration: 150 } }

						RowLayout {
							anchors { fill: parent; margins: 12 }
							spacing: 10
							MaterialSymbol {
								icon: modelData.signal > 75 ? "network_wifi" : (modelData.signal > 50 ? "network_wifi_3_bar" : (modelData.signal > 25 ? "network_wifi_2_bar" : "network_wifi_1_bar"))
								iconSize: 20
								color: modelData.connected ? col.onPrimaryContainer : col.onSurfaceVariant
							}
							ColumnLayout {
								Layout.fillWidth: true
								spacing: 1
								Text { text: modelData.ssid; font.pixelSize: 13; font.weight: 600; font.family: cfg ? cfg.fontFamily : "Rubik"; color: modelData.connected ? col.onPrimaryContainer : col.onSurface; elide: Text.ElideRight; Layout.fillWidth: true }
								Text {
									text: NetworkManager.connectingTo === modelData.ssid ? "Connecting..." : (modelData.connected ? "Connected" : (modelData.signal + "% - " + (modelData.security || "Open")))
									font.pixelSize: 11
									font.family: cfg ? cfg.fontFamily : "Rubik"
									color: modelData.connected ? col.onPrimaryContainer : col.onSurfaceVariant
								}
							}
							Rectangle {
								width: netChipText.implicitWidth + 16
								height: 26
								radius: 13
								color: modelData.connected ? col.error : col.primary
								visible: netRowMouse.containsMouse || modelData.connected || NetworkManager.connectingTo === modelData.ssid
								Text { id: netChipText; anchors.centerIn: parent; text: modelData.connected ? "Disconnect" : (NetworkManager.connectingTo === modelData.ssid ? "..." : "Connect"); font.pixelSize: 11; font.family: cfg ? cfg.fontFamily : "Rubik"; font.weight: 700; color: "white" }
								MouseArea {
									anchors.fill: parent
									cursorShape: Qt.PointingHandCursor
									onClicked: {
										NetworkManager.connectError = ""
										if (modelData.connected) {
											NetworkManager.disconnectWifi()
										} else if (NetworkManager.savedProfiles.indexOf(modelData.ssid) >= 0 || !modelData.security || modelData.security === "--") {
											NetworkManager.connectTo(modelData.ssid)
										} else {
											netPage.pendingSsid = modelData.ssid
											netPage.passwordDialogOpen = true
										}
									}
								}
							}
						}
						MouseArea { id: netRowMouse; anchors.fill: parent; hoverEnabled: true; z: -1 }
					}
				}
			}
		}

		// Password dialog
		Rectangle {
			Layout.fillWidth: true
			height: pwdCol.implicitHeight + 24
			radius: 14
			color: col.surfaceContainerHigh
			visible: passwordDialogOpen
			border.width: 1
			border.color: col.primary

			ColumnLayout {
				id: pwdCol
				anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
				spacing: 10

				Text {
					text: 'Password for "' + netPage.pendingSsid + '"'
					font.pixelSize: 13
					font.weight: 700
					font.family: cfg ? cfg.fontFamily : "Rubik"
					color: col.onSurface
				}

				Rectangle {
					Layout.fillWidth: true
					height: 42
					radius: 10
					color: col.surfaceContainer
					border.width: pwdField.activeFocus ? 2 : 0
					border.color: col.primary
					Behavior on border.width { NumberAnimation { duration: 150 } }
					TextField {
						id: pwdField
						anchors { fill: parent; margins: 4 }
						placeholderText: "Enter password..."
						placeholderTextColor: col.onSurfaceVariant
						color: col.onSurface
						font.pixelSize: 13
						font.family: "JetBrains Mono"
						background: null
						echoMode: TextInput.Password
						verticalAlignment: Text.AlignVCenter
						onAccepted: {
							if (text.length > 0) {
								NetworkManager.connectWithPassword(netPage.pendingSsid, text)
								text = ""
								netPage.passwordDialogOpen = false
							}
						}
					}
				}

				RowLayout {
					spacing: 8
					Item { Layout.fillWidth: true }
					Rectangle {
						width: 70
						height: 30
						radius: 15
						color: col.surfaceContainer
						Text { anchors.centerIn: parent; text: "Cancel"; font.pixelSize: 12; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurfaceVariant }
						MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { netPage.passwordDialogOpen = false; pwdField.text = "" } }
					}
					Rectangle {
						width: 80
						height: 30
						radius: 15
						color: col.primary
						Text { anchors.centerIn: parent; text: "Connect"; font.pixelSize: 12; font.family: cfg ? cfg.fontFamily : "Rubik"; font.weight: 700; color: col.onPrimary }
						MouseArea {
							anchors.fill: parent
							cursorShape: Qt.PointingHandCursor
							onClicked: {
								if (pwdField.text.length > 0) {
									NetworkManager.connectWithPassword(netPage.pendingSsid, pwdField.text)
									pwdField.text = ""
									netPage.passwordDialogOpen = false
								}
							}
						}
					}
				}
			}
		}
	}
}
