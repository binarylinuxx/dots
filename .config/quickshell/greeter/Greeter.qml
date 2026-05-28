import QtCore
import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Quickshell
import Quickshell.Io
import Quickshell.Services.Greetd

Rectangle {
	id: root
	color: col.background || "#111318"
	focus: true

	// ── Colors — same FileView pattern as root shell.qml ────────────────────
	FileView {
		id: colorWatcher
		path: StandardPaths.writableLocation(StandardPaths.GenericCacheLocation).toString() + "/blxshell/Colors.json"
		watchChanges: true
		onFileChanged: reload()

		JsonAdapter {
			id: col
			property string background
			property string foreground
			property string primary
			property string primaryFixed
			property string primaryFixedDim
			property string onPrimary
			property string onPrimaryFixed
			property string onPrimaryFixedVariant
			property string primaryContainer
			property string onPrimaryContainer
			property string secondary
			property string secondaryFixed
			property string secondaryFixedDim
			property string onSecondary
			property string onSecondaryFixed
			property string onSecondaryFixedVariant
			property string secondaryContainer
			property string onSecondaryContainer
			property string tertiary
			property string tertiaryFixed
			property string tertiaryFixedDim
			property string onTertiary
			property string onTertiaryFixed
			property string onTertiaryFixedVariant
			property string tertiaryContainer
			property string onTertiaryContainer
			property string error
			property string onError
			property string errorContainer
			property string onErrorContainer
			property string surface
			property string onSurface
			property string onSurfaceVariant
			property string outline
			property string outlineVariant
			property string shadow
			property string scrim
			property string inverseSurface
			property string inverseOnSurface
			property string inversePrimary
			property string surfaceDim
			property string surfaceBright
			property string surfaceContainerLowest
			property string surfaceContainerLow
			property string surfaceContainer
			property string surfaceContainerHigh
			property string surfaceContainerHighest
			property string wallpaper
		}
	}

	// ── Config ───────────────────────────────────────────────────────────────
	FileView {
		path: StandardPaths.writableLocation(StandardPaths.GenericConfigLocation).toString() + "/blxshell/config.json"
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

	function greeting(): string {
		var h = root.currentTime.getHours()
		if (h <  5) return "Good night"
		if (h < 12) return "Good morning"
		if (h < 17) return "Good afternoon"
		if (h < 21) return "Good evening"
		return "Good night"
	}

	// ── User list (from /etc/passwd, uid 1000-65533) ─────────────────────────
	property var  userList:     []
	property int  selectedUser: 0

	Process {
		command: ["awk", "-F:", "$3 >= 1000 && $3 < 65534 { print $1 }", "/etc/passwd"]
		running: true
		stdout: SplitParser {
			onRead: data => {
				var u = data.trim()
				if (u !== "") root.userList = root.userList.concat([u])
			}
		}
	}

	// ── Session list (wayland + xorg .desktop files) ─────────────────────────
	property var sessionList:     []   // [{ name, exec }]
	property int selectedSession: 0

	Process {
		// Build a real session list from installed .desktop files.
		command: ["bash", "-c",
			"awk -F= 'FNR==1{n=\"\"} /^Name=/{n=$2} /^Exec=/{k=n\"|\"$2; if(n!=\"\" && $2!=\"\" && !seen[k]++) print k}'" +
			" /usr/share/wayland-sessions/*.desktop" +
			" /usr/share/xsessions/*.desktop 2>/dev/null"]
		running: true
		stdout: SplitParser {
			onRead: data => {
				var line = data.trim()
				if (line === "") return
				var sep = line.indexOf("|")
				if (sep < 0) return
				root.sessionList = root.sessionList.concat([{
					name: line.substring(0, sep).trim(),
					exec: line.substring(sep + 1).trim()
				}])
			}
		}
	}

	// ── Greetd auth flow ─────────────────────────────────────────────────────
	//
	//  tryLogin()
	//    └─ Greetd.createSession(user)
	//         └─ onAuthMessage (responseRequired) → Greetd.respond(password)
	//              ├─ onReadyToLaunch → Greetd.launch(cmd)
	//              └─ onAuthFailure   → show error, reset
	//
	property bool   authInProgress: false
	property bool   showFailure:    false
	property string errorMessage:   ""

	// Password is read straight from the TextInput via binding below.
	// We keep a snapshot in pendingPassword at submit time so the respond()
	// handler always has the right value even if the field is cleared.
	property string pendingPassword: ""

	function tryLogin() {
		if (authInProgress) return
		var pass = passwordInput.text
		if (pass === "" || root.userList.length === 0) return

		root.pendingPassword = pass
		root.authInProgress  = true
		root.showFailure     = false
		root.errorMessage    = ""

		// If a previous session is still around, cancel it first
		if (Greetd.state !== GreetdState.Inactive)
			Greetd.cancelSession()

		Greetd.createSession(root.userList[root.selectedUser])
	}

	Connections {
		target: Greetd

		function normalizeDesktopExec(command) {
			var trimmed = command.trim()
			if (trimmed === "") return ""
			// Strip .desktop field codes like %f %u %F %U %i %c %k.
			trimmed = trimmed.replace(/\s+%[a-zA-Z]/g, "")
			trimmed = trimmed.replace(/%[a-zA-Z]/g, "")
			return trimmed.trim()
		}

		function onAuthMessage(message, error, responseRequired, echoResponse) {
			if (responseRequired)
				Greetd.respond(root.pendingPassword)
		}

		function onReadyToLaunch() {
			var sessions = root.sessionList
			var cmd = sessions.length > 0
				? sessions[root.selectedSession].exec
				: "start-hyprland"

			cmd = normalizeDesktopExec(cmd)
			if (cmd === "") cmd = "start-hyprland"

			// Launch the real session command exactly as provided by
			// the selected .desktop entry.
			Greetd.launch(["/bin/sh", "-lc", cmd])
		}

		function onAuthFailure(message) {
			root.authInProgress  = false
			root.showFailure     = true
			root.errorMessage    = message !== "" ? message : "Wrong password"
			passwordInput.text   = ""
			root.pendingPassword = ""
			shakeAnim.start()
			passwordInput.forceActiveFocus()
		}

		function onError(error) {
			root.authInProgress = false
			root.showFailure    = true
			root.errorMessage   = error
		}
	}

	// ── UI state ─────────────────────────────────────────────────────────────
	property bool authVisible:     false
	property bool passwordVisible: false

	Timer {
		id: idleTimer
		interval: 30000
		repeat: false
		onTriggered: {
			if (passwordInput.text === "") {
				root.authVisible     = false
				root.passwordVisible = false
				if (Greetd.state !== GreetdState.Inactive) {
					Greetd.cancelSession()
					root.authInProgress = false
				}
			}
		}
	}

	function activateAuth() {
		root.authVisible = true
		idleTimer.restart()
		passwordInput.forceActiveFocus()
	}

	function dismissAuth() {
		root.authVisible     = false
		root.passwordVisible = false
		idleTimer.stop()
		if (Greetd.state !== GreetdState.Inactive) {
			Greetd.cancelSession()
			root.authInProgress = false
		}
	}

	Keys.onPressed: function(event) {
		if (event.key === Qt.Key_Escape) {
			if (root.authVisible && passwordInput.text === "") root.dismissAuth()
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

			Text {
				anchors.horizontalCenter: parent.horizontalCenter
				text: root.greeting()
				font.family: lcfg.fontFamily || "Rubik"
				font.pixelSize: 15; font.letterSpacing: 4
				font.capitalization: Font.AllUppercase
				color: col.primary || "#adc6ff"
				opacity: 0.85
			}

			Item { height: 14 }

			Text {
				anchors.horizontalCenter: parent.horizontalCenter
				text: Qt.formatDateTime(root.currentTime, "hh:mm")
				font.family: lcfg.fontFamily || "Rubik"
				font.pixelSize: 140; font.weight: Font.Light; font.letterSpacing: -5
				color: col.onSurface || "#e2e2e9"
			}

			Rectangle {
				anchors.horizontalCenter: parent.horizontalCenter
				width: amPmLabel.implicitWidth + 24; height: 24; radius: 12
				color: Qt.rgba(Qt.color(col.primary || "#adc6ff").r, Qt.color(col.primary || "#adc6ff").g, Qt.color(col.primary || "#adc6ff").b, 0.14)
				Text {
					id: amPmLabel
					anchors.centerIn: parent
					text: Qt.formatDateTime(root.currentTime, "AP")
					font.family: lcfg.fontFamily || "Rubik"
					font.pixelSize: 12; font.letterSpacing: 4
					color: col.primary || "#adc6ff"
				}
			}

			Item { height: 24 }

			Row {
				anchors.horizontalCenter: parent.horizontalCenter
				spacing: 7
				Text {
					anchors.verticalCenter: parent.verticalCenter
					text: "calendar_today"; font.family: "Material Symbols Rounded"; font.pixelSize: 14
					color: col.onSurfaceVariant || "#c4c6d0"; opacity: 0.65
				}
				Text {
					anchors.verticalCenter: parent.verticalCenter
					text: Qt.formatDateTime(root.currentDate, "dddd, MMMM d")
					font.family: lcfg.fontFamily || "Rubik"; font.pixelSize: 14
					color: col.onSurfaceVariant || "#c4c6d0"; opacity: 0.65
				}
			}
		}

		Text {
			anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 68 }
			text: "Press any key to continue"
			font.family: lcfg.fontFamily || "Rubik"; font.pixelSize: 12
			font.letterSpacing: 2; font.capitalization: Font.AllUppercase
			color: col.onSurfaceVariant || "#c4c6d0"
			SequentialAnimation on opacity {
				running: !root.authVisible; loops: Animation.Infinite
				NumberAnimation { to: 0.18; duration: 1800; easing.type: Easing.InOutSine }
				NumberAnimation { to: 0.65; duration: 1800; easing.type: Easing.InOutSine }
			}
		}

		MouseArea {
			anchors.fill: parent
			property real pressY: 0; property bool swiped: false
			onPressed:  { pressY = mouse.y; swiped = false }
			onPositionChanged: { if (!swiped && Math.abs(mouse.y - pressY) > 60) { swiped = true; root.activateAuth() } }
			onClicked: root.activateAuth()
		}
	}

	// ══════════════════════════════════════════════════════════════════════════
	// AUTH STATE — centred floating glass card
	// ══════════════════════════════════════════════════════════════════════════
	Item {
		anchors.fill: parent
		opacity: root.authVisible ? 1 : 0
		visible: opacity > 0
		Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.InOutQuad } }

		// Mini clock
		Column {
			anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter
			anchors.topMargin: 44; spacing: 4; opacity: 0.55
			Text {
				anchors.horizontalCenter: parent.horizontalCenter
				text: Qt.formatDateTime(root.currentTime, "hh:mm") + "  " + Qt.formatDateTime(root.currentTime, "AP")
				font.family: lcfg.fontFamily || "Rubik"; font.pixelSize: 18; font.weight: Font.Light
				color: col.onSurface || "#e2e2e9"
			}
			Text {
				anchors.horizontalCenter: parent.horizontalCenter
				text: Qt.formatDateTime(root.currentDate, "dddd, MMMM d")
				font.family: lcfg.fontFamily || "Rubik"; font.pixelSize: 12
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
			border.color: Qt.rgba(Qt.color(col.outlineVariant || "#44474f").r, Qt.color(col.outlineVariant || "#44474f").g, Qt.color(col.outlineVariant || "#44474f").b, 0.4)
			border.width: 1

			property real slideOffset: root.authVisible ? 0 : 36
			Behavior on slideOffset { NumberAnimation { duration: 500; easing.type: Easing.OutBack; easing.overshoot: 1.3 } }
			transform: Translate { y: authCard.slideOffset }
			scale: root.authVisible ? 1.0 : 0.93
			transformOrigin: Item.Center
			Behavior on scale { NumberAnimation { duration: 500; easing.type: Easing.OutBack; easing.overshoot: 1.3 } }

			// Swipe down to dismiss
			MouseArea {
				anchors.fill: parent
				property real pressY: 0; property bool swiped: false
				onPressed:  { pressY = mouse.y; swiped = false }
				onPositionChanged: {
					if (!swiped && (mouse.y - pressY) > 60 && passwordInput.text === "") {
						swiped = true; root.dismissAuth()
					}
				}
			}

			ColumnLayout {
				id: cardContent
				anchors { left: parent.left; right: parent.right; top: parent.top
					leftMargin: 28; rightMargin: 28; topMargin: 28 }
				spacing: 16

				// ── Avatar ───────────────────────────────────────────────────
				ColumnLayout {
					Layout.alignment: Qt.AlignHCenter
					spacing: 10

					Rectangle {
						Layout.alignment: Qt.AlignHCenter
						width: 68; height: 68; radius: 34
						color: col.primaryContainer || "#274777"

						Text {
							anchors.centerIn: parent
							text: root.userList.length > 0
								? root.userList[root.selectedUser].charAt(0).toUpperCase()
								: "?"
							font.family: lcfg.fontFamily || "Rubik"
							font.pixelSize: 28; font.weight: Font.Medium
							color: col.onPrimaryContainer || "#d6e3ff"

							property real shakeX: 0
							x: shakeX
							SequentialAnimation on shakeX {
								id: shakeAnim; running: false
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
						text: root.greeting()
						font.family: lcfg.fontFamily || "Rubik"
						font.pixelSize: 20; font.weight: Font.Medium
						color: col.onSurface || "#e2e2e9"
					}
				}

				// ── User picker ───────────────────────────────────────────────
				Row {
					Layout.alignment: Qt.AlignHCenter
					spacing: 6
					visible: root.userList.length > 0

					// chevron left
					Rectangle {
						width: 26; height: 26; radius: 13
						visible: root.userList.length > 1
						color: puL.containsMouse ? Qt.rgba(Qt.color(col.primary||"#adc6ff").r, Qt.color(col.primary||"#adc6ff").g, Qt.color(col.primary||"#adc6ff").b, 0.14) : "transparent"
						Behavior on color { ColorAnimation { duration: 150 } }
						Text { anchors.centerIn: parent; text: "chevron_left"; font.family: "Material Symbols Rounded"; font.pixelSize: 15; color: col.onSurfaceVariant || "#c4c6d0" }
						MouseArea { id: puL; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
							onClicked: { root.selectedUser = (root.selectedUser - 1 + root.userList.length) % root.userList.length; root.showFailure = false } }
					}

					Row {
						anchors.verticalCenter: parent.verticalCenter
						spacing: 5
						Text { anchors.verticalCenter: parent.verticalCenter; text: "person"; font.family: "Material Symbols Rounded"; font.pixelSize: 14; color: col.onSurfaceVariant || "#c4c6d0"; opacity: 0.7 }
						Text {
							anchors.verticalCenter: parent.verticalCenter
							text: root.userList.length > 0 ? root.userList[root.selectedUser] : "…"
							font.family: lcfg.fontFamily || "Rubik"; font.pixelSize: 14; font.weight: Font.Medium
							color: col.onSurface || "#e2e2e9"
						}
					}

					// chevron right
					Rectangle {
						width: 26; height: 26; radius: 13
						visible: root.userList.length > 1
						color: puR.containsMouse ? Qt.rgba(Qt.color(col.primary||"#adc6ff").r, Qt.color(col.primary||"#adc6ff").g, Qt.color(col.primary||"#adc6ff").b, 0.14) : "transparent"
						Behavior on color { ColorAnimation { duration: 150 } }
						Text { anchors.centerIn: parent; text: "chevron_right"; font.family: "Material Symbols Rounded"; font.pixelSize: 15; color: col.onSurfaceVariant || "#c4c6d0" }
						MouseArea { id: puR; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
							onClicked: { root.selectedUser = (root.selectedUser + 1) % root.userList.length; root.showFailure = false } }
					}
				}

				// ── Session picker ────────────────────────────────────────────
				Row {
					Layout.alignment: Qt.AlignHCenter
					spacing: 6
					visible: root.sessionList.length > 0

					Rectangle {
						width: 26; height: 26; radius: 13
						visible: root.sessionList.length > 1
						color: psL.containsMouse ? Qt.rgba(Qt.color(col.primary||"#adc6ff").r, Qt.color(col.primary||"#adc6ff").g, Qt.color(col.primary||"#adc6ff").b, 0.14) : "transparent"
						Behavior on color { ColorAnimation { duration: 150 } }
						Text { anchors.centerIn: parent; text: "chevron_left"; font.family: "Material Symbols Rounded"; font.pixelSize: 15; color: col.onSurfaceVariant || "#c4c6d0" }
						MouseArea { id: psL; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
							onClicked: root.selectedSession = (root.selectedSession - 1 + root.sessionList.length) % root.sessionList.length }
					}

					Row {
						anchors.verticalCenter: parent.verticalCenter
						spacing: 5
						Text { anchors.verticalCenter: parent.verticalCenter; text: "desktop_windows"; font.family: "Material Symbols Rounded"; font.pixelSize: 13; color: col.onSurfaceVariant || "#c4c6d0"; opacity: 0.7 }
						Text {
							anchors.verticalCenter: parent.verticalCenter
							text: root.sessionList.length > 0 ? root.sessionList[root.selectedSession].name : "No sessions found"
							font.family: lcfg.fontFamily || "Rubik"; font.pixelSize: 13
							color: col.onSurfaceVariant || "#c4c6d0"
						}
					}

					Rectangle {
						width: 26; height: 26; radius: 13
						visible: root.sessionList.length > 1
						color: psR.containsMouse ? Qt.rgba(Qt.color(col.primary||"#adc6ff").r, Qt.color(col.primary||"#adc6ff").g, Qt.color(col.primary||"#adc6ff").b, 0.14) : "transparent"
						Behavior on color { ColorAnimation { duration: 150 } }
						Text { anchors.centerIn: parent; text: "chevron_right"; font.family: "Material Symbols Rounded"; font.pixelSize: 15; color: col.onSurfaceVariant || "#c4c6d0" }
						MouseArea { id: psR; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
							onClicked: root.selectedSession = (root.selectedSession + 1) % root.sessionList.length }
					}
				}

				// Divider
				Rectangle {
					Layout.fillWidth: true; height: 1
					color: col.outlineVariant || "#44474f"; opacity: 0.35
				}

				// ── Password field ────────────────────────────────────────────
				Rectangle {
					Layout.fillWidth: true; height: 52; radius: 26
					color: col.surfaceContainerHigh || "#282a2f"
					border.width: passwordInput.activeFocus ? 2 : 1
					border.color: passwordInput.activeFocus
						? (col.primary || "#adc6ff")
						: Qt.rgba(Qt.color(col.outlineVariant||"#44474f").r, Qt.color(col.outlineVariant||"#44474f").g, Qt.color(col.outlineVariant||"#44474f").b, 0.45)
					Behavior on border.color { ColorAnimation { duration: 200 } }

					RowLayout {
						anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 10; spacing: 8

						TextInput {
							id: passwordInput
							Layout.fillWidth: true; clip: true; focus: root.authVisible
							echoMode: root.passwordVisible ? TextInput.Normal : TextInput.Password
							inputMethodHints: Qt.ImhSensitiveData
							color: col.onSurface || "#e2e2e9"
							font.family: lcfg.fontFamily || "Rubik"; font.pixelSize: 22
							verticalAlignment: TextInput.AlignVCenter
							enabled: root.authVisible && !root.authInProgress
							onTextChanged: { root.showFailure = false; if (text !== "") idleTimer.restart() }
							onAccepted: root.tryLogin()
						}

						Text {
							text: "Password"; font.family: lcfg.fontFamily || "Rubik"; font.pixelSize: 14
							color: col.onSurfaceVariant || "#8d9199"; opacity: 0.6
							visible: passwordInput.text === "" && !passwordInput.activeFocus
						}

						// Eye toggle
						Text {
							text: root.passwordVisible ? "visibility_off" : "visibility"
							font.family: "Material Symbols Rounded"; font.pixelSize: 18
							color: eyeMouse.containsMouse ? (col.primary || "#adc6ff") : (col.onSurfaceVariant || "#c4c6d0")
							opacity: passwordInput.text !== "" ? 1.0 : 0.0
							Behavior on opacity { NumberAnimation { duration: 150 } }
							Behavior on color   { ColorAnimation  { duration: 150 } }
							MouseArea { id: eyeMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
								onClicked: root.passwordVisible = !root.passwordVisible }
						}

						// Submit
						Rectangle {
							width: 36; height: 36; radius: 18
							visible: passwordInput.text !== ""
							color: subMouse.containsMouse ? (col.primary || "#adc6ff") : (col.primaryContainer || "#274777")
							Behavior on color { ColorAnimation { duration: 150 } }
							Text {
								anchors.centerIn: parent
								text: root.authInProgress ? "pending" : "arrow_forward"
								font.family: "Material Symbols Rounded"; font.pixelSize: 18
								color: subMouse.containsMouse ? (col.onPrimary || "#002b6e") : (col.onPrimaryContainer || "#d6e3ff")
								Behavior on color { ColorAnimation { duration: 150 } }
							}
							MouseArea { id: subMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
								onClicked: { if (!root.authInProgress && passwordInput.text !== "") root.tryLogin() } }
						}
					}
				}

				// ── Error pill ────────────────────────────────────────────────
				Rectangle {
					Layout.alignment: Qt.AlignHCenter
					width: errRow.implicitWidth + 24; height: 28; radius: 14
					color: Qt.rgba(Qt.color(col.error||"#ffb4ab").r, Qt.color(col.error||"#ffb4ab").g, Qt.color(col.error||"#ffb4ab").b, 0.15)
					opacity: root.showFailure ? 1.0 : 0.0
					Behavior on opacity { NumberAnimation { duration: 200 } }
					Row {
						id: errRow; anchors.centerIn: parent; spacing: 6
						Text { anchors.verticalCenter: parent.verticalCenter; text: "error"; font.family: "Material Symbols Rounded"; font.pixelSize: 14; color: col.error || "#ffb4ab" }
						Text { anchors.verticalCenter: parent.verticalCenter; text: root.errorMessage || "Wrong password"; font.family: lcfg.fontFamily || "Rubik"; font.pixelSize: 12; color: col.error || "#ffb4ab" }
					}
				}

				// Esc hint
				Text {
					Layout.alignment: Qt.AlignHCenter; Layout.bottomMargin: 4
					text: "Esc to return"; font.family: lcfg.fontFamily || "Rubik"; font.pixelSize: 11
					color: col.onSurfaceVariant || "#c4c6d0"; opacity: 0.35
				}
			}
		}
	}
}
