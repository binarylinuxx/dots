import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Io

Rectangle {
	id: root
	required property LockContext context
	color: col.background || "#111318"

	// ── Config ──
	FileView {
		id: lockConfigWatcher
		path: Qt.resolvedUrl("../config.json")
		watchChanges: true
		JsonAdapter {
			id: lcfg
			property string fontFamily: "Rubik"
		}
	}

	// ── Colors ──
	FileView {
		id: lockColorWatcher
		path: Qt.resolvedUrl("./Colors.json")
		watchChanges: true
		JsonAdapter {
			id: col
			property string background
			property string foreground
			property string primary
			property string onPrimary
			property string primaryContainer
			property string onPrimaryContainer
			property string secondary
			property string secondaryContainer
			property string onSecondaryContainer
			property string tertiary
			property string onSurface
			property string onSurfaceVariant
			property string outline
			property string outlineVariant
			property string error
			property string surface
			property string surfaceContainer
			property string surfaceContainerHigh
			property string surfaceContainerHighest
			property string wallpaper
		}
	}

	// ── Wallpaper ──
	Image {
		id: wallpaperBg
		anchors.fill: parent
		source: col.wallpaper || ""
		fillMode: Image.PreserveAspectCrop
		visible: col.wallpaper && col.wallpaper !== "" && status === Image.Ready
		opacity: 0
		onStatusChanged: if (status === Image.Ready) opacity = 1
		Behavior on opacity { NumberAnimation { duration: 600 } }
	}

	// Scrim — same darkness as sidebar/settings backdrop
	Rectangle {
		anchors.fill: parent
		color: "#000000"
		opacity: wallpaperBg.visible ? 0.55 : 0
		Behavior on opacity { NumberAnimation { duration: 400 } }
	}

	// ══════════════════════════════════════
	// ── MAIN CARD ──
	// Same style as sidebar panel: surface color, radius 20, subtle border
	// ══════════════════════════════════════
	Rectangle {
		id: card
		anchors.centerIn: parent
		width: 360
		height: cardContent.implicitHeight + 56
		radius: 24
		color: Qt.rgba(
			Qt.color(col.surface || "#1c1b1f").r,
			Qt.color(col.surface || "#1c1b1f").g,
			Qt.color(col.surface || "#1c1b1f").b,
			wallpaperBg.visible ? 0.72 : 1.0
		)
		border.color: col.outlineVariant || "#44474f"
		border.width: 1

		// Fade in on load
		opacity: 0
		Component.onCompleted: opacity = 1
		Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

		ColumnLayout {
			id: cardContent
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.top: parent.top
			anchors.margins: 28
			spacing: 0

			// ── Clock ──
			Text {
				id: clockText
				Layout.alignment: Qt.AlignHCenter
				Layout.topMargin: 4
				property var currentTime: new Date()

				Timer {
					interval: 1000
					repeat: true
					running: true
					onTriggered: clockText.currentTime = new Date()
				}

				text: Qt.formatDateTime(currentTime, "hh:mm")
				font.family: lcfg.fontFamily || "Rubik"
				font.pixelSize: 72
				font.weight: Font.Light
				letterSpacing: -1
				color: col.onSurface || "#e2e2e9"
			}

			// ── Date ──
			Text {
				Layout.alignment: Qt.AlignHCenter
				Layout.topMargin: 2
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
				font.weight: Font.Normal
				color: col.onSurfaceVariant || "#c4c6d0"
			}

			// ── Divider ──
			Rectangle {
				Layout.fillWidth: true
				Layout.topMargin: 24
				Layout.bottomMargin: 24
				height: 1
				color: col.outlineVariant || "#44474f"
				opacity: 0.5
			}

			// ── Password field — same style as Settings text fields ──
			Rectangle {
				id: passwordField
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

					// Lock icon with shake
					Text {
						id: lockIcon
						text: root.context.unlockInProgress ? "lock_clock"
							: (root.context.showFailure ? "lock" : "lock_open")
						font.family: "Material Symbols Rounded"
						font.pixelSize: 20
						color: passwordInput.activeFocus
							? (col.primary || "#adc6ff")
							: (col.onSurfaceVariant || "#8d9199")
						Behavior on color { ColorAnimation { duration: 200 } }

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

					// Input
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

					// Placeholder
					Text {
						text: "Enter password"
						font.family: lcfg.fontFamily || "Rubik"
						font.pixelSize: 14
						color: col.onSurfaceVariant || "#8d9199"
						opacity: 0.6
						visible: passwordInput.text === "" && !passwordInput.activeFocus
					}

					// Submit button — same style as sidebar action buttons
					Rectangle {
						width: 36
						height: 36
						radius: 18
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

			// ── Error pill ──
			Rectangle {
				Layout.alignment: Qt.AlignHCenter
				Layout.topMargin: 10
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

			// Bottom padding
			Item { Layout.preferredHeight: 4 }
		}
	}

	// Trigger shake on failure
	Connections {
		target: root.context
		function onShowFailureChanged() {
			if (root.context.showFailure) shakeAnim.start()
		}
	}
}
