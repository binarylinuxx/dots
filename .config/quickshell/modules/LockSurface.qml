import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Io
import qs.services

Rectangle {
	id: root
	required property LockContext context
	color: col.background || "#111318"

	FileView {
		id: lockConfig
		path: Qt.resolvedUrl("../config.json")
		watchChanges: true
		JsonAdapter {
			id: lcfg
			property string fontFamily: "Rubik"
		}
	}

	// ── Wallpaper ──
	Image {
		id: wallpaperBg
		anchors.fill: parent
		source: col.wallpaper || ""
		fillMode: Image.PreserveAspectCrop
		opacity: 0
		onStatusChanged: if (status === Image.Ready) opacity = 1
		Behavior on opacity { NumberAnimation { duration: 600 } }
	}

	Rectangle {
		anchors.fill: parent
		color: "#000000"
		opacity: wallpaperBg.opacity > 0 ? 0.5 : 0
		Behavior on opacity { NumberAnimation { duration: 400 } }
	}

	// ══════════════════════════════════════
	// ── SPLIT LAYOUT ──
	// Left: clock + date   Right: auth card
	// ══════════════════════════════════════
	Row {
		anchors.fill: parent

		// ── LEFT HALF — Clock ──
		Item {
			width: parent.width * 0.5
			height: parent.height

			Column {
				anchors.centerIn: parent
				spacing: 8

				Text {
					id: clockText
					anchors.horizontalCenter: parent.horizontalCenter
					property var currentTime: new Date()

					Timer {
						interval: 1000
						repeat: true
						running: true
						onTriggered: clockText.currentTime = new Date()
					}

					text: Qt.formatDateTime(currentTime, "hh:mm")
					font.family: lcfg.fontFamily || "Rubik"
					font.pixelSize: 100
					font.weight: Font.Light
					font.letterSpacing: -2
					color: col.onSurface || "#e2e2e9"
				}

				Text {
					anchors.horizontalCenter: parent.horizontalCenter
					text: Qt.formatDateTime(clockText.currentTime, "AP")
					font.family: lcfg.fontFamily || "Rubik"
					font.pixelSize: 14
					font.weight: Font.Normal
					font.letterSpacing: 4
					color: col.primary || "#adc6ff"
				}

				Item { height: 4 }

				Row {
					anchors.horizontalCenter: parent.horizontalCenter
					spacing: 6

					Text {
						anchors.verticalCenter: parent.verticalCenter
						text: "calendar_today"
						font.family: "Material Symbols Rounded"
						font.pixelSize: 13
						color: col.onSurfaceVariant || "#c4c6d0"
					}

					Text {
						anchors.verticalCenter: parent.verticalCenter
						property var currentDate: new Date()

						Timer {
							interval: 60000
							repeat: true
							running: true
							onTriggered: parent.currentDate = new Date()
						}

						text: Qt.formatDateTime(currentDate, "dddd, MMMM d")
						font.family: lcfg.fontFamily || "Rubik"
						font.pixelSize: 13
						color: col.onSurfaceVariant || "#c4c6d0"
					}
				}
			}
		}

		// ── RIGHT HALF — Auth ──
		Item {
			width: parent.width * 0.5
			height: parent.height

			// Frosted vertical divider
			Rectangle {
				anchors.left: parent.left
				anchors.top: parent.top
				anchors.bottom: parent.bottom
				width: 1
				color: col.outlineVariant || "#44474f"
				opacity: 0.3
			}

			// Frosted right panel
			Rectangle {
				anchors.fill: parent
				color: Qt.rgba(
					Qt.color(col.surface || "#1c1b1f").r,
					Qt.color(col.surface || "#1c1b1f").g,
					Qt.color(col.surface || "#1c1b1f").b,
					wallpaperBg.opacity > 0 ? 0.65 : 1.0
				)
			}

			// Auth content — vertically centered in right half
			ColumnLayout {
				id: authContent
				anchors.centerIn: parent
				width: 300
				spacing: 20

				// Lock icon + label
				ColumnLayout {
					Layout.alignment: Qt.AlignHCenter
					spacing: 12

					Rectangle {
						Layout.alignment: Qt.AlignHCenter
						width: 56; height: 56; radius: 28
						color: col.primaryContainer || "#0a305f"

						Text {
							anchors.centerIn: parent
							text: root.context.unlockInProgress ? "lock_clock"
								: (root.context.showFailure ? "lock" : "lock_open")
							font.family: "Material Symbols Rounded"
							font.pixelSize: 26
							color: col.onPrimaryContainer || "#adc6ff"

							property real shakeX: 0
							x: shakeX
							SequentialAnimation on shakeX {
								id: shakeAnim
								running: false
								NumberAnimation { to: 7;  duration: 50 }
								NumberAnimation { to: -7; duration: 50 }
								NumberAnimation { to: 5;  duration: 50 }
								NumberAnimation { to: -5; duration: 50 }
								NumberAnimation { to: 0;  duration: 50 }
							}
						}
					}

					Text {
						Layout.alignment: Qt.AlignHCenter
						text: "Welcome back"
						font.family: lcfg.fontFamily || "Rubik"
						font.pixelSize: 20
						font.weight: Font.Normal
						color: col.onSurface || "#e2e2e9"
					}

					Text {
						Layout.alignment: Qt.AlignHCenter
						text: "Enter your password to unlock"
						font.family: lcfg.fontFamily || "Rubik"
						font.pixelSize: 13
						color: col.onSurfaceVariant || "#c4c6d0"
					}
				}

				// Divider
				Rectangle {
					Layout.fillWidth: true
					height: 1
					color: col.outlineVariant || "#44474f"
					opacity: 0.4
				}

				// Password field
				Rectangle {
					Layout.fillWidth: true
					height: 52
					radius: 16
					color: col.surfaceContainerHigh || "#282a2f"
					border.color: passwordInput.activeFocus
						? (col.primary || "#adc6ff")
						: "transparent"
					border.width: 2
					Behavior on border.color { ColorAnimation { duration: 200 } }

					RowLayout {
						anchors.fill: parent
						anchors.leftMargin: 16
						anchors.rightMargin: 12
						spacing: 10

						Text {
							text: "password"
							font.family: "Material Symbols Rounded"
							font.pixelSize: 18
							color: passwordInput.activeFocus
								? (col.primary || "#adc6ff")
								: (col.onSurfaceVariant || "#8d9199")
							Behavior on color { ColorAnimation { duration: 200 } }
						}

						TextInput {
							id: passwordInput
							Layout.fillWidth: true
							clip: true
							focus: true
							echoMode: TextInput.Password
							inputMethodHints: Qt.ImhSensitiveData
							color: col.onSurface || "#e2e2e9"
							font.family: lcfg.fontFamily || "Rubik"
							font.pixelSize: 15
							verticalAlignment: TextInput.AlignVCenter
							enabled: !root.context.unlockInProgress
							onTextChanged: root.context.currentText = text
							onAccepted: root.context.tryUnlock()

							Connections {
								target: root.context
								function onCurrentTextChanged() {
									passwordInput.text = root.context.currentText
								}
							}
						}

						Text {
							text: "Password"
							font.family: lcfg.fontFamily || "Rubik"
							font.pixelSize: 14
							color: col.onSurfaceVariant || "#8d9199"
							opacity: 0.6
							visible: passwordInput.text === "" && !passwordInput.activeFocus
						}

						Rectangle {
							width: 36; height: 36; radius: 18
							visible: passwordInput.text !== ""
							color: submitMouse.containsMouse
								? (col.primary || "#adc6ff")
								: (col.primaryContainer || "#0a305f")
							Behavior on color { ColorAnimation { duration: 150 } }

							Text {
								anchors.centerIn: parent
								text: "arrow_forward"
								font.family: "Material Symbols Rounded"
								font.pixelSize: 18
								color: submitMouse.containsMouse
									? (col.onPrimary || "#002b6e")
									: (col.onPrimaryContainer || "#adc6ff")
								Behavior on color { ColorAnimation { duration: 150 } }
							}

							MouseArea {
								id: submitMouse
								anchors.fill: parent
								hoverEnabled: true
								cursorShape: Qt.PointingHandCursor
								onClicked: {
									if (!root.context.unlockInProgress && root.context.currentText !== "")
										root.context.tryUnlock()
								}
							}
						}
					}
				}

				// Error pill
				Rectangle {
					Layout.alignment: Qt.AlignHCenter
					width: errorRow.implicitWidth + 24
					height: 28
					radius: 14
					color: Qt.rgba(
						Qt.color(col.error || "#ffb4ab").r,
						Qt.color(col.error || "#ffb4ab").g,
						Qt.color(col.error || "#ffb4ab").b,
						0.15
					)
					opacity: root.context.showFailure ? 1.0 : 0.0
					Behavior on opacity { NumberAnimation { duration: 200 } }

					Row {
						id: errorRow
						anchors.centerIn: parent
						spacing: 6
						Text {
							anchors.verticalCenter: parent.verticalCenter
							text: "error"
							font.family: "Material Symbols Rounded"
							font.pixelSize: 14
							color: col.error || "#ffb4ab"
						}
						Text {
							anchors.verticalCenter: parent.verticalCenter
							text: "Wrong password"
							font.family: lcfg.fontFamily || "Rubik"
							font.pixelSize: 12
							color: col.error || "#ffb4ab"
						}
					}
				}
			}
		}
	}

	Connections {
		target: root.context
		function onShowFailureChanged() {
			if (root.context.showFailure) shakeAnim.start()
		}
	}
}
