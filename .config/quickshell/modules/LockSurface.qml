import QtQuick
import QtQuick.Layouts
import Quickshell

Rectangle {
	id: root
	required property LockContext context
	color: col.background || "#151216"

	// ── Wallpaper ──
	Image {
		id: wallpaperBg
		anchors.fill: parent
		source: col.wallpaper || ""
		fillMode: Image.PreserveAspectCrop
		visible: col.wallpaper && col.wallpaper !== "" && status === Image.Ready
	}

	Rectangle {
		anchors.fill: parent
		color: "black"
		opacity: wallpaperBg.visible ? 0.45 : 0
	}

	// ══════════════════════════════════════
	// ── BOLD CLOCK ──
	// ══════════════════════════════════════
	Column {
		id: clockBlock
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.top: parent.top
		anchors.topMargin: parent.height * 0.18
		spacing: 0

		Text {
			id: hoursText
			anchors.horizontalCenter: parent.horizontalCenter
			property var currentTime: new Date()

			Timer {
				interval: 1000
				repeat: true
				running: true
				onTriggered: hoursText.currentTime = new Date()
			}

			text: Qt.formatDateTime(currentTime, "hh")
			font.family: "Rubik"
			font.pixelSize: 180
			font.weight: Font.Bold
			lineHeight: 0.85
			color: col.primary || "#e1b8f2"
		}

		Text {
			id: minutesText
			anchors.horizontalCenter: parent.horizontalCenter
			text: Qt.formatDateTime(hoursText.currentTime, "mm")
			font.family: "Rubik"
			font.pixelSize: 180
			font.weight: Font.Bold
			lineHeight: 0.85
			color: col.tertiary || "#ffb0cd"
		}
	}

	// ── Date chip ──
	Rectangle {
		id: dateChip
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.top: clockBlock.bottom
		anchors.topMargin: 20
		width: dateRow.implicitWidth + 24
		height: dateRow.implicitHeight + 12
		radius: height / 2
		color: col.surfaceContainerHigh || "#2c292d"

		Row {
			id: dateRow
			anchors.centerIn: parent
			spacing: 8

			Text {
				anchors.verticalCenter: parent.verticalCenter
				text: "calendar_today"
				font.family: "Material Symbols Rounded"
				font.pixelSize: 16
				color: col.primary || "#e1b8f2"
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
				font.family: "Rubik"
				font.pixelSize: 14
				font.weight: Font.Medium
				color: col.onSurface || "#e8e0e5"
			}
		}
	}

	// ══════════════════════════════════════
	// ── ANIMATED LOCK ICON ──
	// ══════════════════════════════════════
	Rectangle {
		id: lockIcon
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.top: dateChip.bottom
		anchors.topMargin: parent.height * 0.08
		width: 64
		height: 64
		radius: 32
		color: col.primaryContainer || "#331443"

		scale: lockIconMouse.pressed ? 0.9 : (lockIconMouse.containsMouse ? 1.08 : 1.0)
		Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

		Text {
			anchors.centerIn: parent
			text: root.context.unlockInProgress ? "lock_clock"
				: (root.context.showFailure ? "lock" : "lock_open")
			font.family: "Material Symbols Rounded"
			font.pixelSize: 28
			color: col.onPrimaryContainer || "#c79fd7"

			x: 0
			SequentialAnimation on x {
				id: shakeAnim
				running: false
				NumberAnimation { to: 8; duration: 50 }
				NumberAnimation { to: -8; duration: 50 }
				NumberAnimation { to: 6; duration: 50 }
				NumberAnimation { to: -6; duration: 50 }
				NumberAnimation { to: 0; duration: 50 }
			}
		}

		MouseArea {
			id: lockIconMouse
			anchors.fill: parent
			hoverEnabled: true
			cursorShape: Qt.PointingHandCursor
			onClicked: {
				if (!root.context.unlockInProgress && root.context.currentText !== "") {
					root.context.tryUnlock()
				}
			}
		}
	}

	// ══════════════════════════════════════
	// ── PASSWORD FIELD ──
	// ══════════════════════════════════════
	Rectangle {
		id: passwordField
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.top: lockIcon.bottom
		anchors.topMargin: 20
		width: Math.max(280, passwordInnerRow.implicitWidth + 48)
		height: 52
		radius: 16
		color: col.surfaceContainer || "#221f22"
		border.color: passwordInput.activeFocus
			? (col.primary || "#e1b8f2")
			: "transparent"
		border.width: 2

		Behavior on border.color { ColorAnimation { duration: 200 } }
		Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

		Row {
			id: passwordInnerRow
			anchors.centerIn: parent
			spacing: 10

			Text {
				anchors.verticalCenter: parent.verticalCenter
				text: "password"
				font.family: "Material Symbols Rounded"
				font.pixelSize: 20
				color: passwordInput.activeFocus
					? (col.primary || "#e1b8f2")
					: (col.onSurfaceVariant || "#cec3ce")
				Behavior on color { ColorAnimation { duration: 200 } }
			}

			TextInput {
				id: passwordInput
				anchors.verticalCenter: parent.verticalCenter
				width: Math.max(200, contentWidth + 10)
				clip: true
				focus: true
				echoMode: TextInput.Password
				inputMethodHints: Qt.ImhSensitiveData
				color: col.onSurface || "#e8e0e5"
				font.family: "Rubik"
				font.pixelSize: 16
				enabled: !root.context.unlockInProgress

				onTextChanged: root.context.currentText = this.text
				onAccepted: root.context.tryUnlock()

				Connections {
					target: root.context
					function onCurrentTextChanged() {
						passwordInput.text = root.context.currentText;
					}
				}
			}
		}

		Text {
			anchors.centerIn: parent
			anchors.horizontalCenterOffset: 15
			text: "Enter password"
			font.family: "Rubik"
			font.pixelSize: 15
			color: col.onSurfaceVariant || "#cec3ce"
			opacity: 0.5
			visible: passwordInput.text === "" && !passwordInput.activeFocus
		}
	}

	// ── Error pill ──
	Rectangle {
		id: errorPill
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.top: passwordField.bottom
		anchors.topMargin: 12
		width: errorRow.implicitWidth + 24
		height: errorRow.implicitHeight + 12
		radius: height / 2
		color: Qt.rgba(Qt.color(col.error || "#ffb4ab").r,
		               Qt.color(col.error || "#ffb4ab").g,
		               Qt.color(col.error || "#ffb4ab").b, 0.15)
		visible: root.context.showFailure
		opacity: root.context.showFailure ? 1 : 0
		Behavior on opacity { NumberAnimation { duration: 200 } }

		Row {
			id: errorRow
			anchors.centerIn: parent
			spacing: 6

			Text {
				anchors.verticalCenter: parent.verticalCenter
				text: "error"
				font.family: "Material Symbols Rounded"
				font.pixelSize: 16
				color: col.error || "#ffb4ab"
			}

			Text {
				anchors.verticalCenter: parent.verticalCenter
				text: "Wrong password"
				font.family: "Rubik"
				font.pixelSize: 13
				color: col.error || "#ffb4ab"
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
