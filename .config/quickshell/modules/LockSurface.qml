import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Quickshell
import Quickshell.Io
import qs.services

Rectangle {
	id: root
	required property LockContext context
	color: col.background || "#111318"
	focus: true

	// ── Config ───────────────────────────────────────────────────────────────
	FileView {
		id: lockConfig
		path: (Quickshell.env("XDG_CONFIG_HOME") || Quickshell.env("HOME") + "/.config") + "/blxshell/config.json"
		watchChanges: true
		JsonAdapter {
			id: lcfg
			property string fontFamily: "Rubik"
		}
	}

	// ── Shared time ──────────────────────────────────────────────────────────
	property var currentTime: new Date()
	property var currentDate: new Date()

	Timer { interval: 1000;  repeat: true; running: true; onTriggered: root.currentTime = new Date() }
	Timer { interval: 60000; repeat: true; running: true; onTriggered: root.currentDate = new Date() }

	// ── State ────────────────────────────────────────────────────────────────
	property bool   authVisible:     false
	property bool   passwordVisible: false
	property string userName:        Quickshell.env("USER") ?? ""

	function greeting(): string {
		var h = root.currentTime.getHours()
		if (h <  5) return "Good night"
		if (h < 12) return "Good morning"
		if (h < 17) return "Good afternoon"
		if (h < 21) return "Good evening"
		return "Good night"
	}

	Timer {
		id: idleTimer
		interval: 30000
		repeat: false
		onTriggered: {
			if (root.context.currentText === "") {
				root.authVisible     = false
				root.passwordVisible = false
			}
		}
	}

	function activateAuth() {
		root.authVisible = true
		idleTimer.restart()
		passwordInput.forceActiveFocus()
	}

	// ── Key handling ─────────────────────────────────────────────────────────
	Keys.onPressed: function(event) {
		if (event.key === Qt.Key_Escape) {
			if (root.authVisible && root.context.currentText === "") {
				root.authVisible     = false
				root.passwordVisible = false
				idleTimer.stop()
			}
			event.accepted = true
			return
		}
		if (!root.authVisible) {
			root.activateAuth()
			event.accepted = true
		}
	}

	// ── Wallpaper ─────────────────────────────────────────────────────────────
	Image {
		id: wallpaperBg
		anchors.fill: parent
		source: col.wallpaper || ""
		fillMode: Image.PreserveAspectCrop
		opacity: 0
		onStatusChanged: if (status === Image.Ready) opacity = 1
		Behavior on opacity { NumberAnimation { duration: 600 } }
	}

	MultiEffect {
		anchors.fill: parent
		source: wallpaperBg
		blurEnabled: true
		blur: root.authVisible ? 0.85 : 0.0
		opacity: root.authVisible ? wallpaperBg.opacity : 0.0
		autoPaddingEnabled: false
		Behavior on blur    { NumberAnimation { duration: 450; easing.type: Easing.InOutQuad } }
		Behavior on opacity { NumberAnimation { duration: 450; easing.type: Easing.InOutQuad } }
	}

	Rectangle {
		anchors.fill: parent
		color: "#000000"
		opacity: wallpaperBg.opacity > 0 ? (root.authVisible ? 0.55 : 0.25) : 0
		Behavior on opacity { NumberAnimation { duration: 400 } }
	}

	// ══════════════════════════════════════════════════════════════════════════
	// IDLE STATE
	// ══════════════════════════════════════════════════════════════════════════
	Item {
		id: idleView
		anchors.fill: parent
		opacity: root.authVisible ? 0 : 1
		visible: opacity > 0
		Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.InOutQuad } }

		Column {
			anchors.centerIn: parent
			anchors.verticalCenterOffset: -16
			spacing: 0
			scale: root.authVisible ? 0.92 : 1.0
			Behavior on scale { NumberAnimation { duration: 480; easing.type: Easing.OutBack; easing.overshoot: 1.4 } }

			// Time-based greeting
			Text {
				anchors.horizontalCenter: parent.horizontalCenter
				text: root.greeting()
				font.family: lcfg.fontFamily || "Rubik"
				font.pixelSize: 15
				font.letterSpacing: 4
				font.capitalization: Font.AllUppercase
				color: col.primary || "#adc6ff"
				opacity: 0.85
			}

			Item { height: 14 }

			// Large clock
			Text {
				anchors.horizontalCenter: parent.horizontalCenter
				text: Qt.formatDateTime(root.currentTime, "hh:mm")
				font.family: lcfg.fontFamily || "Rubik"
				font.pixelSize: 140
				font.weight: Font.Light
				font.letterSpacing: -5
				color: col.onSurface || "#e2e2e9"
			}

			// AM/PM pill
			Rectangle {
				anchors.horizontalCenter: parent.horizontalCenter
				width: amPmLabel.implicitWidth + 24
				height: 24
				radius: 12
				color: Qt.rgba(
					Qt.color(col.primary || "#adc6ff").r,
					Qt.color(col.primary || "#adc6ff").g,
					Qt.color(col.primary || "#adc6ff").b,
					0.14
				)

				Text {
					id: amPmLabel
					anchors.centerIn: parent
					text: Qt.formatDateTime(root.currentTime, "AP")
					font.family: lcfg.fontFamily || "Rubik"
					font.pixelSize: 12
					font.letterSpacing: 4
					color: col.primary || "#adc6ff"
				}
			}

			Item { height: 24 }

			// Date row
			Row {
				anchors.horizontalCenter: parent.horizontalCenter
				spacing: 7

				Text {
					anchors.verticalCenter: parent.verticalCenter
					text: "calendar_today"
					font.family: "Material Symbols Rounded"
					font.pixelSize: 14
					color: col.onSurfaceVariant || "#c4c6d0"
					opacity: 0.65
				}
				Text {
					anchors.verticalCenter: parent.verticalCenter
					text: Qt.formatDateTime(root.currentDate, "dddd, MMMM d")
					font.family: lcfg.fontFamily || "Rubik"
					font.pixelSize: 14
					color: col.onSurfaceVariant || "#c4c6d0"
					opacity: 0.65
				}
			}
		}

		// Pulsing "press any key" hint
		Text {
			anchors {
				bottom: parent.bottom
				horizontalCenter: parent.horizontalCenter
				bottomMargin: 68
			}
			text: "Press any key to unlock"
			font.family: lcfg.fontFamily || "Rubik"
			font.pixelSize: 12
			font.letterSpacing: 2
			font.capitalization: Font.AllUppercase
			color: col.onSurfaceVariant || "#c4c6d0"

			SequentialAnimation on opacity {
				running: !root.authVisible
				loops: Animation.Infinite
				NumberAnimation { to: 0.18; duration: 1800; easing.type: Easing.InOutSine }
				NumberAnimation { to: 0.65; duration: 1800; easing.type: Easing.InOutSine }
			}
		}

		MouseArea {
			anchors.fill: parent
			property real pressY: 0
			property bool swiped: false

			onPressed:  { pressY = mouse.y; swiped = false }
			onPositionChanged: {
				if (!swiped && Math.abs(mouse.y - pressY) > 60) {
					swiped = true
					root.activateAuth()
				}
			}
			onClicked: root.activateAuth()
		}
	}

	// ══════════════════════════════════════════════════════════════════════════
	// AUTH STATE — centred floating glass card
	// ══════════════════════════════════════════════════════════════════════════
	Item {
		id: authView
		anchors.fill: parent
		opacity: root.authVisible ? 1 : 0
		visible: opacity > 0
		Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.InOutQuad } }

		// Mini clock at top
		Column {
			anchors.top: parent.top
			anchors.horizontalCenter: parent.horizontalCenter
			anchors.topMargin: 44
			spacing: 4
			opacity: 0.55

			Text {
				anchors.horizontalCenter: parent.horizontalCenter
				text: Qt.formatDateTime(root.currentTime, "hh:mm") + "  " + Qt.formatDateTime(root.currentTime, "AP")
				font.family: lcfg.fontFamily || "Rubik"
				font.pixelSize: 18
				font.weight: Font.Light
				color: col.onSurface || "#e2e2e9"
			}
			Text {
				anchors.horizontalCenter: parent.horizontalCenter
				text: Qt.formatDateTime(root.currentDate, "dddd, MMMM d")
				font.family: lcfg.fontFamily || "Rubik"
				font.pixelSize: 12
				color: col.onSurfaceVariant || "#c4c6d0"
			}
		}

		// Floating card
		Rectangle {
			id: authCard
			anchors.centerIn: parent
			width: 360
			implicitHeight: cardContent.implicitHeight + 56
			height: implicitHeight
			radius: 28
			color: Qt.rgba(
				Qt.color(col.surfaceContainer || "#1d2024").r,
				Qt.color(col.surfaceContainer || "#1d2024").g,
				Qt.color(col.surfaceContainer || "#1d2024").b,
				wallpaperBg.opacity > 0 ? 0.82 : 1.0
			)
			border.color: Qt.rgba(
				Qt.color(col.outlineVariant || "#44474f").r,
				Qt.color(col.outlineVariant || "#44474f").g,
				Qt.color(col.outlineVariant || "#44474f").b,
				0.4
			)
			border.width: 1

			property real slideOffset: root.authVisible ? 0 : 36
			Behavior on slideOffset { NumberAnimation { duration: 500; easing.type: Easing.OutBack; easing.overshoot: 1.3 } }
			transform: Translate { y: authCard.slideOffset }

			scale: root.authVisible ? 1.0 : 0.93
			transformOrigin: Item.Center
			Behavior on scale { NumberAnimation { duration: 500; easing.type: Easing.OutBack; easing.overshoot: 1.3 } }

			// Swipe-down to return to idle (only when field is empty)
			MouseArea {
				anchors.fill: parent
				property real pressY: 0
				property bool swiped: false

				onPressed:  { pressY = mouse.y; swiped = false }
				onPositionChanged: {
					if (!swiped && (mouse.y - pressY) > 60 && root.context.currentText === "") {
						swiped = true
						root.authVisible     = false
						root.passwordVisible = false
						idleTimer.stop()
					}
				}
			}

			ColumnLayout {
				id: cardContent
				anchors {
					left: parent.left; right: parent.right; top: parent.top
					leftMargin: 28; rightMargin: 28; topMargin: 28
				}
				spacing: 18

				// Avatar + name
				ColumnLayout {
					Layout.alignment: Qt.AlignHCenter
					spacing: 14

					Rectangle {
						Layout.alignment: Qt.AlignHCenter
						width: 72; height: 72; radius: 36
						color: col.primaryContainer || "#0a305f"

						Text {
							anchors.centerIn: parent
							text: root.userName !== "" ? root.userName.charAt(0).toUpperCase() : "?"
							font.family: lcfg.fontFamily || "Rubik"
							font.pixelSize: 30
							font.weight: Font.Medium
							color: col.onPrimaryContainer || "#adc6ff"

							property real shakeX: 0
							x: shakeX
							SequentialAnimation on shakeX {
								id: shakeAnim
								running: false
								NumberAnimation { to: 8;  duration: 50 }
								NumberAnimation { to: -8; duration: 50 }
								NumberAnimation { to: 5;  duration: 50 }
								NumberAnimation { to: -5; duration: 50 }
								NumberAnimation { to: 0;  duration: 50 }
							}
						}
					}

					Text {
						Layout.alignment: Qt.AlignHCenter
						text: root.userName !== "" ? root.greeting() + ", " + root.userName : root.greeting()
						font.family: lcfg.fontFamily || "Rubik"
						font.pixelSize: 20
						font.weight: Font.Medium
						color: col.onSurface || "#e2e2e9"
					}

					Text {
						Layout.alignment: Qt.AlignHCenter
						text: "Enter your password to continue"
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
					opacity: 0.35
				}

				// Password field
				Rectangle {
					Layout.fillWidth: true
					height: 52
					radius: 26
					color: col.surfaceContainerHigh || "#282a2f"
					border.width: passwordInput.activeFocus ? 2 : 1
					border.color: passwordInput.activeFocus
						? (col.primary || "#adc6ff")
						: Qt.rgba(
							Qt.color(col.outlineVariant || "#44474f").r,
							Qt.color(col.outlineVariant || "#44474f").g,
							Qt.color(col.outlineVariant || "#44474f").b,
							0.45
						)
					Behavior on border.color { ColorAnimation { duration: 200 } }

					RowLayout {
						anchors.fill: parent
						anchors.leftMargin: 16
						anchors.rightMargin: 10
						spacing: 8

						TextInput {
							id: passwordInput
							Layout.fillWidth: true
							clip: true
							focus: root.authVisible
							echoMode: root.passwordVisible ? TextInput.Normal : TextInput.Password
							inputMethodHints: Qt.ImhSensitiveData
							color: col.onSurface || "#e2e2e9"
							font.family: lcfg.fontFamily || "Rubik"
							font.pixelSize: 22
							verticalAlignment: TextInput.AlignVCenter
							enabled: root.authVisible && !root.context.unlockInProgress
							onTextChanged: {
								root.context.currentText = text
								if (text !== "") idleTimer.restart()
							}
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

						// Eye toggle
						Text {
							text: root.passwordVisible ? "visibility_off" : "visibility"
							font.family: "Material Symbols Rounded"
							font.pixelSize: 18
							color: eyeMouse.containsMouse
								? (col.primary || "#adc6ff")
								: (col.onSurfaceVariant || "#c4c6d0")
							opacity: passwordInput.text !== "" ? 1.0 : 0.0
							Behavior on opacity { NumberAnimation { duration: 150 } }
							Behavior on color   { ColorAnimation  { duration: 150 } }

							MouseArea {
								id: eyeMouse
								anchors.fill: parent
								hoverEnabled: true
								cursorShape: Qt.PointingHandCursor
								onClicked: root.passwordVisible = !root.passwordVisible
							}
						}

						// Submit
						Rectangle {
							width: 36; height: 36; radius: 18
							visible: passwordInput.text !== ""
							color: submitMouse.containsMouse
								? (col.primary || "#adc6ff")
								: (col.primaryContainer || "#0a305f")
							Behavior on color { ColorAnimation { duration: 150 } }

							Text {
								anchors.centerIn: parent
								text: root.context.unlockInProgress ? "pending" : "arrow_forward"
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

				// Esc hint
				Text {
					Layout.alignment: Qt.AlignHCenter
					Layout.bottomMargin: 4
					text: "Esc to return"
					font.family: lcfg.fontFamily || "Rubik"
					font.pixelSize: 11
					color: col.onSurfaceVariant || "#c4c6d0"
					opacity: 0.35
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
