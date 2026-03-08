import Quickshell
import Quickshell.Hyprland
import Quickshell.Services.Pipewire
import Quickshell.Widgets
import Quickshell.Io
import QtQuick
import QtQuick.Layouts
import qs.widgets
import qs.services

Scope {
    id: sidebarRoot

    PwObjectTracker {
        objects: [Pipewire.defaultAudioSink, Pipewire.defaultAudioSource]
    }

    PanelWindow {
        id: sidebarWindow

        anchors.right: true
        anchors.top: true
        anchors.bottom: true
        exclusiveZone: 0
        margins.right: 5

        implicitWidth: Gstate.sidebarOpen ? 360 : 1
        color: "transparent"

        // The compositor already subtracts the bar's exclusive zone from our height.
        // We only need a small margin from the remaining edges.
        readonly property string barPos: cfg ? cfg.barPosition : "bottom"
        readonly property int edgeMargin: 5

        property real panelX: Gstate.sidebarOpen ? 0 : 360
        Behavior on panelX {
            NumberAnimation { duration: 320; easing.type: Easing.OutCubic }
        }

        HyprlandFocusGrab {
            windows: [sidebarWindow]
            active: Gstate.sidebarOpen
            onCleared: Gstate.sidebarOpen = false
        }

        Rectangle {
            id: panel
            width: 360
            x: sidebarWindow.panelX
            // The PanelWindow already excludes the bar's reserved zone.
            // Just add a small margin from the free edges.
            y: sidebarWindow.edgeMargin
            height: sidebarWindow.height - sidebarWindow.edgeMargin * 2
            color: col.surface
            radius: 20
            clip: true

            property int activeTab: 0
            // 0 = main, 1 = wifi page
            property int currentPage: 0

            // ── Page slide container ──
            Item {
                id: pageContainer
                width: panel.width * 2
                height: panel.height
                x: panel.currentPage === 0 ? 0 : -panel.width

                Behavior on x {
                    NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                }

            // ── PAGE 0: Main content ──
            Flickable {
                id: mainFlickable
                x: 0
                width: panel.width
                height: panel.height
                contentHeight: mainCol.implicitHeight
                clip: true
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds

                Column {
                    id: mainCol
                    width: panel.width
                    spacing: 0

                    // ═══════════════════════════════════════
                    // Header — date + time
                    // ═══════════════════════════════════════
                    Item {
                        id: headerItem
                        width: parent.width
                        height: 80

                        Column {
                            anchors.left: parent.left
                            anchors.leftMargin: 20
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 1

                            Text {
                                property var now: new Date()
                                Timer {
                                    interval: 1000; repeat: true; running: Gstate.sidebarOpen
                                    onTriggered: parent.now = new Date()
                                }
                                text: Qt.formatTime(now, cfg ? cfg.clockFormat : "h:mm AP")
                                font.family: cfg ? cfg.fontFamily : "Rubik"
                                font.pixelSize: 36
                                font.weight: 300
                                color: col.onSurface
                            }

                            Text {
                                property var now: new Date()
                                Timer {
                                    interval: 60000; repeat: true; running: Gstate.sidebarOpen
                                    onTriggered: parent.now = new Date()
                                }
                                text: Qt.formatDateTime(now, "dddd, MMMM d")
                                font.family: cfg ? cfg.fontFamily : "Rubik"
                                font.pixelSize: 13
                                font.weight: 400
                                color: col.onSurfaceVariant
                            }
                        }
                    }

                    // ═══════════════════════════════════════
                    // Quick setting tiles — 2-column grid
                    // ═══════════════════════════════════════
                    Item {
                        id: tilesItem
                        width: parent.width
                        height: tilesGrid.height + 16

                        Process { id: themeModeProc; command: [] }

                        Grid {
                            id: tilesGrid
                            anchors.top: parent.top
                            anchors.topMargin: 0
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8

                            // ── Network tile ──
                            Rectangle {
                                id: networkTile
                                property bool connected: NetworkManager.primaryConnectionType !== "none"
                                    && NetworkManager.primaryConnectionType !== "unknown"

                                width: (tilesGrid.width - 8) / 2
                                height: 72
                                radius: 20
                                color: connected ? col.primaryContainer : col.surfaceContainer

                                Behavior on color { ColorAnimation { duration: 250 } }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 12
                                    spacing: 12

                                    // Icon circle
                                    Rectangle {
                                        width: 40
                                        height: 40
                                        radius: 20
                                        color: networkTile.connected ? col.primary : col.surfaceContainerHighest
                                        Behavior on color { ColorAnimation { duration: 250 } }

                                        MaterialSymbol {
                                            anchors.centerIn: parent
                                            iconSize: 20
                                            fill: 1
                                            color: networkTile.connected ? col.onPrimary : col.onSurfaceVariant
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                            icon: {
                                                if (NetworkManager.primaryConnectionType === "ethernet") return "lan"
                                                if (NetworkManager.primaryConnectionType === "wifi") {
                                                    const s = NetworkManager.wifiSignalStrength
                                                    if (s > 75) return "network_wifi"
                                                    if (s > 50) return "network_wifi_3_bar"
                                                    if (s > 25) return "network_wifi_2_bar"
                                                    return "network_wifi_1_bar"
                                                }
                                                return "signal_wifi_off"
                                            }
                                        }
                                    }

                                    Column {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            width: parent.width
                                            text: {
                                                if (NetworkManager.primaryConnectionType === "ethernet") return "Internet"
                                                if (NetworkManager.primaryConnectionType === "wifi")
                                                    return NetworkManager.wifiSsid || "Wi-Fi"
                                                return "Wi-Fi"
                                            }
                                            font.family: cfg ? cfg.fontFamily : "Rubik"
                                            font.pixelSize: 14
                                            font.weight: 600
                                            color: networkTile.connected
                                                ? col.onPrimaryContainer : col.onSurface
                                            elide: Text.ElideRight
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }

                                        Text {
                                            width: parent.width
                                            text: {
                                                if (NetworkManager.primaryConnectionType === "wifi")
                                                    return NetworkManager.wifiSignalStrength + "%"
                                                if (NetworkManager.primaryConnectionType === "ethernet")
                                                    return "Connected"
                                                return "Disconnected"
                                            }
                                            font.family: cfg ? cfg.fontFamily : "Rubik"
                                            font.pixelSize: 12
                                            color: networkTile.connected
                                                ? col.onPrimaryContainer : col.onSurfaceVariant
                                            opacity: 0.8
                                            elide: Text.ElideRight
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }
                                    }

                                    // Chevron indicator
                                    MaterialSymbol {
                                        iconSize: 16
                                        icon: "chevron_right"
                                        color: networkTile.connected ? col.onPrimaryContainer : col.onSurfaceVariant
                                        opacity: 0.7
                                        Behavior on color { ColorAnimation { duration: 250 } }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        panel.currentPage = 1
                                        NetworkManager.scanWifi()
                                    }
                                }
                            }

                            // ── Night Light tile ──
                            Rectangle {
                                id: nightLightTile
                                property bool active: false
                                width: (tilesGrid.width - 8) / 2
                                height: 72
                                radius: 20
                                color: active ? col.primaryContainer : col.surfaceContainer
                                Behavior on color { ColorAnimation { duration: 250 } }

                                // Detect if wlsunset is already running on load
                                Process {
                                    id: nightLightCheck
                                    command: ["pgrep", "-x", "wlsunset"]
                                    running: true
                                    onExited: function(code) {
                                        nightLightTile.active = (code === 0)
                                    }
                                }

                                // Single process used for both kill and start via startDetached()
                                Process {
                                    id: nightLightProc
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 12
                                    spacing: 12

                                    Rectangle {
                                        width: 40
                                        height: 40
                                        radius: 20
                                        color: nightLightTile.active ? col.primary : col.surfaceContainerHighest
                                        Behavior on color { ColorAnimation { duration: 250 } }

                                        MaterialSymbol {
                                            anchors.centerIn: parent
                                            iconSize: 20
                                            fill: nightLightTile.active ? 1 : 0
                                            color: nightLightTile.active ? col.onPrimary : col.onSurfaceVariant
                                            icon: "nightlight"
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }
                                    }

                                    Column {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: "Night Light"
                                            font.family: cfg ? cfg.fontFamily : "Rubik"
                                            font.pixelSize: 14
                                            font.weight: 600
                                            color: nightLightTile.active ? col.onPrimaryContainer : col.onSurface
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }

                                        Text {
                                            text: nightLightTile.active ? "Active" : "Off"
                                            font.family: cfg ? cfg.fontFamily : "Rubik"
                                            font.pixelSize: 12
                                            color: nightLightTile.active ? col.onPrimaryContainer : col.onSurfaceVariant
                                            opacity: 0.8
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (nightLightTile.active) {
                                            nightLightTile.active = false
                                            nightLightProc.command = ["pkill", "-x", "wlsunset"]
                                            nightLightProc.startDetached()
                                        } else {
                                            nightLightTile.active = true
                                            nightLightProc.command = ["sh", "-c", "pkill -x wlsunset; wlsunset -S 00:00 -s 00:01 -t 2700"]
                                            nightLightProc.startDetached()
                                        }
                                    }
                                }
                            }

                            // ── Do Not Disturb tile ──
                            Rectangle {
                                id: dndTile
                                width: (tilesGrid.width - 8) / 2
                                height: 72
                                radius: 20
                                color: NotificationService.dndEnabled ? col.tertiaryContainer : col.surfaceContainer
                                Behavior on color { ColorAnimation { duration: 250 } }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 12
                                    spacing: 12

                                    Rectangle {
                                        width: 40
                                        height: 40
                                        radius: 20
                                        color: NotificationService.dndEnabled ? col.tertiary : col.surfaceContainerHighest
                                        Behavior on color { ColorAnimation { duration: 250 } }

                                        MaterialSymbol {
                                            anchors.centerIn: parent
                                            iconSize: 20
                                            fill: NotificationService.dndEnabled ? 1 : 0
                                            color: NotificationService.dndEnabled ? col.onTertiary : col.onSurfaceVariant
                                            icon: NotificationService.dndEnabled ? "notifications_off" : "notifications_active"
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }
                                    }

                                    Column {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            width: parent.width
                                            text: "Do Not Disturb"
                                            font.family: cfg ? cfg.fontFamily : "Rubik"
                                            font.pixelSize: 12
                                            font.weight: 600
                                            color: NotificationService.dndEnabled ? col.onTertiaryContainer : col.onSurface
                                            wrapMode: Text.WordWrap
                                            maximumLineCount: 2
                                            elide: Text.ElideRight
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }

                                        Text {
                                            text: NotificationService.dndEnabled ? "On" : "Off"
                                            font.family: cfg ? cfg.fontFamily : "Rubik"
                                            font.pixelSize: 12
                                            color: NotificationService.dndEnabled ? col.onTertiaryContainer : col.onSurfaceVariant
                                            opacity: 0.8
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: NotificationService.toggleDnd()
                                }
                            }

                            // ── Dark / Light mode tile ──
                            Rectangle {
                                id: themeModeTile
                                property bool darkMode: (cfg ? cfg.matugenMode : "dark") === "dark"
                                width: (tilesGrid.width - 8) / 2
                                height: 72
                                radius: 20
                                color: darkMode ? col.secondaryContainer : col.primaryContainer
                                Behavior on color { ColorAnimation { duration: 250 } }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 12
                                    spacing: 12

                                    Rectangle {
                                        width: 40
                                        height: 40
                                        radius: 20
                                        color: themeModeTile.darkMode ? col.secondary : col.primary
                                        Behavior on color { ColorAnimation { duration: 250 } }

                                        MaterialSymbol {
                                            anchors.centerIn: parent
                                            iconSize: 20
                                            fill: 1
                                            color: themeModeTile.darkMode ? col.onSecondary : col.onPrimary
                                            icon: themeModeTile.darkMode ? "dark_mode" : "light_mode"
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }
                                    }

                                    Column {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: "Theme"
                                            font.family: cfg ? cfg.fontFamily : "Rubik"
                                            font.pixelSize: 14
                                            font.weight: 600
                                            color: themeModeTile.darkMode ? col.onSecondaryContainer : col.onPrimaryContainer
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }

                                        Text {
                                            text: themeModeTile.darkMode ? "Dark" : "Light"
                                            font.family: cfg ? cfg.fontFamily : "Rubik"
                                            font.pixelSize: 12
                                            color: themeModeTile.darkMode ? col.onSecondaryContainer : col.onPrimaryContainer
                                            opacity: 0.8
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        themeModeTile.darkMode = !themeModeTile.darkMode
                                        const mode = themeModeTile.darkMode ? "dark" : "light"
                                        const scheme = cfg ? cfg.matugenScheme : "tonal-spot"
                                        const contrast = cfg ? cfg.matugenContrast : 0.0
                                        const wallpaper = col ? col.wallpaper : ""
                                        const genScript = Qt.resolvedUrl("../col_gen/generate").toString().replace("file://", "")

                                        if (cfg)
                                            cfg.matugenMode = mode

                                        if (wallpaper && wallpaper !== "") {
                                            themeModeProc.command = [
                                                genScript, "image", wallpaper,
                                                "-m", mode, "-s", scheme, "-c", contrast.toString()
                                            ]
                                            themeModeProc.running = true
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ═══════════════════════════════════════
                    // Segmented tab control
                    // ═══════════════════════════════════════
                    Item {
                        id: tabBarItem
                        width: parent.width
                        height: 56

                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width - 32
                            height: 40
                            radius: 20
                            color: col.surfaceContainerHigh

                            Rectangle {
                                id: tabPill
                                x: panel.activeTab === 0 ? 4 : parent.width / 2
                                y: 4
                                width: parent.width / 2 - 4
                                height: parent.height - 8
                                radius: 16
                                color: col.secondaryContainer
                                Behavior on x {
                                    NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                                }
                            }

                            Row {
                                anchors.fill: parent

                                Item {
                                    width: parent.width / 2
                                    height: parent.height

                                    Text {
                                        anchors.centerIn: parent
                                        text: "Notifications"
                                        font.family: cfg ? cfg.fontFamily : "Rubik"
                                        font.pixelSize: 13
                                        font.weight: panel.activeTab === 0 ? 600 : 400
                                        color: panel.activeTab === 0 ? col.onSecondaryContainer : col.onSurfaceVariant
                                        Behavior on color { ColorAnimation { duration: 150 } }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: panel.activeTab = 0
                                    }
                                }

                                Item {
                                    width: parent.width / 2
                                    height: parent.height

                                    Text {
                                        anchors.centerIn: parent
                                        text: "Audio"
                                        font.family: cfg ? cfg.fontFamily : "Rubik"
                                        font.pixelSize: 13
                                        font.weight: panel.activeTab === 1 ? 600 : 400
                                        color: panel.activeTab === 1 ? col.onSecondaryContainer : col.onSurfaceVariant
                                        Behavior on color { ColorAnimation { duration: 150 } }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: panel.activeTab = 1
                                    }
                                }
                            }
                        }
                    }

                    // ═══════════════════════════════════════
                    // Notifications tab content
                    // ═══════════════════════════════════════
                    Column {
                        id: notificationsTab
                        width: parent.width
                        spacing: 0
                        visible: panel.activeTab === 0

                    Item {
                        width: parent.width
                        height: Math.max(200, panel.height
                            - headerItem.height
                            - tilesItem.height
                            - tabBarItem.height
                            - 364  // calendar card (352) + bottom spacer (12)
                        )

                        ClippingRectangle {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            anchors.bottomMargin: 12
                            radius: 20
                            color: col.surfaceContainer

                            // ── Empty state ──
                            Item {
                                anchors.fill: parent
                                visible: NotificationService.sidebarHistory.length === 0

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 8

                                    MaterialSymbol {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        icon: "notifications_none"
                                        iconSize: 32
                                        color: col.onSurfaceVariant
                                        opacity: 0.5
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "No notifications"
                                        font.family: cfg ? cfg.fontFamily : "Rubik"
                                        font.pixelSize: 14
                                        color: col.onSurfaceVariant
                                        opacity: 0.5
                                    }
                                }
                            }

                            // ── Scrollable list ──
                            Flickable {
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: notifFooter.top
                                clip: true
                                contentHeight: notifColumn.implicitHeight
                                flickableDirection: Flickable.VerticalFlick
                                boundsBehavior: Flickable.StopAtBounds
                                visible: NotificationService.sidebarHistory.length > 0

                                Column {
                                    id: notifColumn
                                    width: parent.width
                                    spacing: 0
                                    topPadding: 8

                                    Repeater {
                                        model: NotificationService.sidebarHistory

                                        Item {
                                            id: notifItem
                                            width: notifColumn.width
                                            height: notifCard.height + 6
                                            clip: true

                                            property var snapId: modelData ? modelData.id : null

                                            NumberAnimation {
                                                id: collapseAnim
                                                target: notifItem
                                                property: "height"
                                                to: 0
                                                duration: 200
                                                easing.type: Easing.InCubic
                                                onFinished: NotificationService.removeSidebarItem(notifItem.snapId)
                                            }

                                            NumberAnimation {
                                                id: swipeAnim
                                                target: notifCard
                                                property: "x"
                                                to: notifColumn.width
                                                duration: 220
                                                easing.type: Easing.InCubic
                                                onFinished: collapseAnim.start()
                                            }

                                            NumberAnimation {
                                                id: snapBack
                                                target: notifCard
                                                property: "x"
                                                to: 0
                                                duration: 200
                                                easing.type: Easing.OutBounce
                                            }

                                            Rectangle {
                                                id: notifCard
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8
                                                anchors.top: parent.top
                                                height: notifCardContent.implicitHeight + 20
                                                radius: 14
                                                color: col.surfaceContainerHigh

                                                ColumnLayout {
                                                    id: notifCardContent
                                                    anchors.left: parent.left
                                                    anchors.right: parent.right
                                                    anchors.top: parent.top
                                                    anchors.margins: 12
                                                    spacing: 4

                                                    // App name + timestamp + dismiss
                                                    RowLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 6

                                                        // App icon
                                                        Rectangle {
                                                            width: 18
                                                            height: 18
                                                            radius: 5
                                                            color: "transparent"

                                                            Image {
                                                                anchors.fill: parent
                                                                fillMode: Image.PreserveAspectFit
                                                                smooth: true
                                                                source: {
                                                                    if (!modelData) return ""
                                                                    if (modelData.image && modelData.image !== "") return modelData.image
                                                                    if (modelData.appIcon && modelData.appIcon !== "") return modelData.appIcon
                                                                    return ""
                                                                }

                                                                Rectangle {
                                                                    anchors.fill: parent
                                                                    radius: 5
                                                                    visible: parent.status === Image.Error || parent.status === Image.Null
                                                                    color: col.primaryContainer

                                                                    Text {
                                                                        anchors.centerIn: parent
                                                                        text: modelData ? (modelData.appName || "?").charAt(0).toUpperCase() : "?"
                                                                        font.pixelSize: 9
                                                                        font.weight: Font.Bold
                                                                        color: col.onPrimaryContainer
                                                                    }
                                                                }
                                                            }
                                                        }

                                                        Text {
                                                            text: (modelData && modelData.appName) || "Unknown"
                                                            font.family: cfg ? cfg.fontFamily : "Rubik"
                                                            font.pixelSize: 11
                                                            font.weight: 500
                                                            color: col.onSurfaceVariant
                                                            Layout.fillWidth: true
                                                        }

                                                        Text {
                                                            text: modelData ? Qt.formatTime(modelData.time, "h:mm") : ""
                                                            font.family: cfg ? cfg.fontFamily : "Rubik"
                                                            font.pixelSize: 11
                                                            color: col.onSurfaceVariant
                                                            opacity: 0.55
                                                        }

                                                        // ── Dismiss button — z:1 so it sits above the drag MouseArea ──
                                                        Item {
                                                            width: 24
                                                            height: 24
                                                            z: 1

                                                            MaterialSymbol {
                                                                anchors.centerIn: parent
                                                                icon: "close"
                                                                iconSize: 15
                                                                color: col.onSurfaceVariant
                                                                opacity: notifDragArea.containsMouse ? 0.9 : 0.45
                                                                Behavior on opacity { NumberAnimation { duration: 100 } }
                                                            }

                                                            MouseArea {
                                                                anchors.fill: parent
                                                                anchors.margins: -4
                                                                cursorShape: Qt.PointingHandCursor
                                                                // propagateComposedEvents: false stops drag area from stealing this click
                                                                onClicked: {
                                                                    mouse.accepted = true
                                                                    swipeAnim.start()
                                                                }
                                                            }
                                                        }
                                                    }

                                                    // Summary
                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: (modelData && modelData.summary) || ""
                                                        font.family: cfg ? cfg.fontFamily : "Rubik"
                                                        font.pixelSize: 13
                                                        font.weight: 700
                                                        color: col.onSurface
                                                        wrapMode: Text.WordWrap
                                                        maximumLineCount: 2
                                                        elide: Text.ElideRight
                                                        visible: text !== ""
                                                    }

                                                    // Body
                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: (modelData && modelData.body) || ""
                                                        font.family: cfg ? cfg.fontFamily : "Rubik"
                                                        font.pixelSize: 12
                                                        color: col.onSurfaceVariant
                                                        wrapMode: Text.WordWrap
                                                        maximumLineCount: 2
                                                        elide: Text.ElideRight
                                                        visible: text !== ""
                                                    }
                                                }

                                                // Swipe-to-dismiss drag area — z:0, behind dismiss button
                                                MouseArea {
                                                    id: notifDragArea
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    z: 0
                                                    drag.target: notifCard
                                                    drag.axis: Drag.XAxis
                                                    drag.minimumX: 0
                                                    drag.maximumX: notifColumn.width

                                                    property bool wasDragged: false
                                                    onPressed: wasDragged = false
                                                    onPositionChanged: if (drag.active) wasDragged = true
                                                    onReleased: {
                                                        if (wasDragged && notifCard.x > notifColumn.width * 0.35)
                                                            swipeAnim.start()
                                                        else if (wasDragged)
                                                            snapBack.start()
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Item { width: 1; height: 8 }
                                }
                            }

                            // ── Footer ──
                            Rectangle {
                                id: notifFooter
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 40
                                radius: 20
                                // Only bottom corners rounded to match parent clip
                                color: col.surfaceContainerHigh
                                visible: NotificationService.sidebarHistory.length > 0

                                // Square off top corners
                                Rectangle {
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    height: parent.radius
                                    color: parent.color
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 10

                                    Text {
                                        text: {
                                            const n = NotificationService.sidebarHistory.length
                                            return n + (n === 1 ? " notification" : " notifications")
                                        }
                                        font.family: cfg ? cfg.fontFamily : "Rubik"
                                        font.pixelSize: 12
                                        color: col.onSurfaceVariant
                                    }

                                    Item { Layout.fillWidth: true }

                                    Rectangle {
                                        width: clearBtnRow.implicitWidth + 16
                                        height: 26
                                        radius: 13
                                        color: clearBtnHover.containsMouse ? col.errorContainer : "transparent"
                                        Behavior on color { ColorAnimation { duration: 150 } }

                                        Row {
                                            id: clearBtnRow
                                            anchors.centerIn: parent
                                            spacing: 4

                                            MaterialSymbol {
                                                anchors.verticalCenter: parent.verticalCenter
                                                icon: "delete_sweep"
                                                iconSize: 14
                                                color: clearBtnHover.containsMouse ? col.onErrorContainer : col.onSurfaceVariant
                                                Behavior on color { ColorAnimation { duration: 150 } }
                                            }

                                            Text {
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: "Clear all"
                                                font.family: cfg ? cfg.fontFamily : "Rubik"
                                                font.pixelSize: 12
                                                font.weight: 500
                                                color: clearBtnHover.containsMouse ? col.onErrorContainer : col.onSurfaceVariant
                                                Behavior on color { ColorAnimation { duration: 150 } }
                                            }
                                        }

                                        MouseArea {
                                            id: clearBtnHover
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: NotificationService.clearSidebarHistory()
                                        }
                                    }
                                }
                        }
                        }
                    }

                    // ── Calendar ──
                    Rectangle {
                        width: parent.width - 24
                        anchors.horizontalCenter: parent.horizontalCenter
                        height: 352
                        radius: 20
                        color: col.surfaceContainer
                        clip: true

                        Calendar {
                            id: calendarWidget
                            anchors.fill: parent
                        }
                    }

                    Item { width: parent.width; height: 12 }

                    }

                    // ═══════════════════════════════════════
                    // Audio tab content
                    // ═══════════════════════════════════════
                    Column {
                        id: audioTab
                        width: parent.width
                        spacing: 12
                        topPadding: 4
                        bottomPadding: 16
                        visible: panel.activeTab === 1

                        readonly property int routedSourceCount: {
                            const links = Pipewire.linkGroups && Pipewire.linkGroups.values
                                ? Pipewire.linkGroups.values : []
                            if (!Pipewire.defaultAudioSink) return 0
                            let count = 0
                            for (let i = 0; i < links.length; ++i) {
                                const l = links[i]
                                if (l && l.target === Pipewire.defaultAudioSink && l.source && l.source.audio)
                                    count += 1
                            }
                            return count
                        }

                        SidebarMediaPlayer {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - 32
                        }

                        // Volume card
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - 32
                            height: 72
                            radius: 20
                            color: col.surfaceContainer

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 18
                                anchors.rightMargin: 18
                                spacing: 14

                                // Mute toggle
                                Rectangle {
                                    width: 40
                                    height: 40
                                    radius: 20
                                    color: volIcon.muted ? col.errorContainer : col.secondaryContainer
                                    Behavior on color { ColorAnimation { duration: 200 } }

                                    MaterialSymbol {
                                        id: volIcon
                                        anchors.centerIn: parent
                                        iconSize: 20
                                        fill: 1
                                        property real vol: Pipewire.defaultAudioSink?.audio?.volume ?? 0
                                        property bool muted: Pipewire.defaultAudioSink?.audio?.muted ?? false

                                        Connections {
                                            target: Pipewire.defaultAudioSink?.audio ?? null
                                            function onVolumeChanged() { volIcon.vol = Pipewire.defaultAudioSink?.audio?.volume ?? 0 }
                                            function onMutedChanged() { volIcon.muted = Pipewire.defaultAudioSink?.audio?.muted ?? false }
                                        }

                                        color: volIcon.muted ? col.onErrorContainer : col.onSecondaryContainer
                                        Behavior on color { ColorAnimation { duration: 200 } }
                                        icon: {
                                            if (muted || vol === 0) return "volume_off"
                                            if (vol > 0.66) return "volume_up"
                                            if (vol > 0.33) return "volume_down"
                                            return "volume_mute"
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (Pipewire.defaultAudioSink?.audio)
                                                    Pipewire.defaultAudioSink.audio.muted = !Pipewire.defaultAudioSink.audio.muted
                                            }
                                        }
                                    }
                                }

                                StyledSlider {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    sliderHeight: 36
                                    radius: 18
                                    value: Pipewire.defaultAudioSink?.audio?.volume ?? 0
                                    from: 0.0; to: 1.0; stepSize: 0.01
                                    onMoved: function(v) {
                                        if (Pipewire.defaultAudioSink?.audio)
                                            Pipewire.defaultAudioSink.audio.volume = v
                                    }
                                }
                            }
                        }

                        // Mic card
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - 32
                            height: 72
                            radius: 20
                            color: col.surfaceContainer

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 18
                                anchors.rightMargin: 18
                                spacing: 14

                                Rectangle {
                                    width: 40
                                    height: 40
                                    radius: 20
                                    color: micIcon.micMuted ? col.errorContainer : col.secondaryContainer
                                    Behavior on color { ColorAnimation { duration: 200 } }

                                    MaterialSymbol {
                                        id: micIcon
                                        anchors.centerIn: parent
                                        iconSize: 20
                                        fill: 1
                                        property bool micMuted: Pipewire.defaultAudioSource?.audio?.muted ?? false

                                        Connections {
                                            target: Pipewire.defaultAudioSource?.audio ?? null
                                            function onMutedChanged() { micIcon.micMuted = Pipewire.defaultAudioSource?.audio?.muted ?? false }
                                        }

                                        color: micIcon.micMuted ? col.onErrorContainer : col.onSecondaryContainer
                                        Behavior on color { ColorAnimation { duration: 200 } }
                                        icon: micMuted ? "mic_off" : "mic"

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (Pipewire.defaultAudioSource?.audio)
                                                    Pipewire.defaultAudioSource.audio.muted = !Pipewire.defaultAudioSource.audio.muted
                                            }
                                        }
                                    }
                                }

                                StyledSlider {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    sliderHeight: 36
                                    radius: 18
                                    value: Pipewire.defaultAudioSource?.audio?.volume ?? 0
                                    from: 0.0; to: 1.0; stepSize: 0.01
                                    onMoved: function(v) {
                                        if (Pipewire.defaultAudioSource?.audio)
                                            Pipewire.defaultAudioSource.audio.volume = v
                                    }
                                }
                            }
                        }

                        // Audio sources (app streams routed to current sink)
                        Rectangle {
                            id: sourcesCard
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - 32
                            height: Math.min(250, sourcesContent.implicitHeight + 50)
                            radius: 20
                            color: col.surfaceContainer
                            clip: true

                            Column {
                                id: sourcesContent
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 12
                                spacing: 10

                                RowLayout {
                                    width: parent.width

                                    Text {
                                        text: "Audio sources"
                                        font.family: cfg ? cfg.fontFamily : "Rubik"
                                        font.pixelSize: 13
                                        font.weight: 700
                                        color: col.onSurface
                                    }

                                    Item { Layout.fillWidth: true }

                                    Text {
                                        text: "" + audioTab.routedSourceCount
                                        font.family: cfg ? cfg.fontFamily : "Rubik"
                                        font.pixelSize: 11
                                        color: col.onSurfaceVariant
                                        opacity: 0.8
                                    }
                                }

                                Item {
                                    width: parent.width
                                    height: Math.min(180, streamsColumn.implicitHeight)

                                    Flickable {
                                        anchors.fill: parent
                                        clip: true
                                        contentHeight: streamsColumn.implicitHeight
                                        flickableDirection: Flickable.VerticalFlick
                                        boundsBehavior: Flickable.StopAtBounds

                                        Column {
                                            id: streamsColumn
                                            width: parent.width
                                            spacing: 8

                                            Repeater {
                                                id: streamsRepeater
                                                model: Pipewire.linkGroups

                                                Rectangle {
                                                    required property var modelData
                                                     property var linkGroup: modelData
                                                     property var srcNode: linkGroup ? linkGroup.source : null
                                                     property bool routedToDefaultSink: linkGroup && Pipewire.defaultAudioSink
                                                         ? (linkGroup.target === Pipewire.defaultAudioSink)
                                                         : false

                                                    width: streamsColumn.width
                                                    height: visible ? streamRow.implicitHeight + 10 : 0
                                                    radius: 12
                                                    color: col.surfaceContainerHigh
                                                    visible: routedToDefaultSink && srcNode && srcNode.audio

                                                    PwObjectTracker { objects: [srcNode] }

                                                    ColumnLayout {
                                                        id: streamRow
                                                        anchors.fill: parent
                                                        anchors.leftMargin: 10
                                                        anchors.rightMargin: 10
                                                        anchors.topMargin: 6
                                                        anchors.bottomMargin: 6
                                                        spacing: 4

                                                        // ── Row 1: icon + name ──
                                                        RowLayout {
                                                            Layout.fillWidth: true
                                                            spacing: 6

                                                            Item {
                                                                width: 16
                                                                height: 16
                                                                Layout.alignment: Qt.AlignVCenter

                                                                Image {
                                                                    anchors.fill: parent
                                                                    fillMode: Image.PreserveAspectFit
                                                                    smooth: true
                                                                    source: {
                                                                        if (!srcNode || !srcNode.properties) return ""
                                                                        const iconName = srcNode.properties["application.icon-name"]
                                                                        return iconName ? ("image://icon/" + iconName) : ""
                                                                    }
                                                                }
                                                            }

                                                            Text {
                                                                text: {
                                                                    if (!srcNode) return "Unknown"
                                                                    const app = srcNode.properties["application.name"]
                                                                        ?? (srcNode.description !== "" ? srcNode.description : srcNode.name)
                                                                    const media = srcNode.properties["media.name"]
                                                                    return media !== undefined ? app + " – " + media : app
                                                                }
                                                                font.family: cfg ? cfg.fontFamily : "Rubik"
                                                                font.pixelSize: 12
                                                                font.weight: 500
                                                                color: col.onSurface
                                                                elide: Text.ElideRight
                                                                Layout.fillWidth: true
                                                            }
                                                        }

                                                        // ── Row 2: slider + mute ──
                                                        RowLayout {
                                                            Layout.fillWidth: true
                                                            spacing: 8

                                                            StyledSlider {
                                                                id: streamSlider
                                                                Layout.fillWidth: true
                                                                Layout.alignment: Qt.AlignVCenter
                                                                sliderWidth: streamSlider.width
                                                                sliderHeight: 28
                                                                radius: 14
                                                                value: srcNode && srcNode.audio ? srcNode.audio.volume : 0
                                                                from: 0.0; to: 1.0; stepSize: 0.01
                                                                onMoved: function(v) {
                                                                    if (srcNode && srcNode.audio)
                                                                        srcNode.audio.volume = v
                                                                }
                                                            }

                                                            Rectangle {
                                                                width: 28
                                                                height: 28
                                                                radius: 14
                                                                Layout.alignment: Qt.AlignVCenter
                                                                color: srcNode && srcNode.audio && srcNode.audio.muted
                                                                    ? col.errorContainer : col.secondaryContainer

                                                                MaterialSymbol {
                                                                    anchors.centerIn: parent
                                                                    iconSize: 15
                                                                    fill: 1
                                                                    icon: srcNode && srcNode.audio && srcNode.audio.muted ? "volume_off" : "volume_up"
                                                                    color: srcNode && srcNode.audio && srcNode.audio.muted
                                                                        ? col.onErrorContainer : col.onSecondaryContainer
                                                                }

                                                                MouseArea {
                                                                    anchors.fill: parent
                                                                    cursorShape: Qt.PointingHandCursor
                                                                    onClicked: {
                                                                        if (srcNode && srcNode.audio)
                                                                            srcNode.audio.muted = !srcNode.audio.muted
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            Item {
                                                width: parent.width
                                                height: 54
                                                visible: audioTab.routedSourceCount === 0

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "No active sources"
                                                    font.family: cfg ? cfg.fontFamily : "Rubik"
                                                    font.pixelSize: 12
                                                    color: col.onSurfaceVariant
                                                    opacity: 0.7
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            // end mainFlickable (page 0)

            // ── PAGE 1: WiFi networks ──────────────────────────────────
            Item {
                id: wifiPage
                x: panel.width
                width: panel.width
                height: panel.height

                // password state
                property string pendingSsid: ""
                property bool showPasswordField: false
                property string passwordInput: ""

                // ── Header ────────────────────────────────────────────
                Rectangle {
                    id: wifiPageHeader
                    width: parent.width
                    height: 56
                    color: "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 16
                        spacing: 4

                        // Back button
                        Rectangle {
                            width: 36; height: 36; radius: 18
                            color: backBtnMa.containsMouse ? col.surfaceContainerHigh : "transparent"
                            Behavior on color { ColorAnimation { duration: 150 } }

                            MaterialSymbol {
                                anchors.centerIn: parent
                                icon: "arrow_back"
                                iconSize: 20
                                color: col.onSurface
                            }

                            MouseArea {
                                id: backBtnMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    panel.currentPage = 0
                                    wifiPage.showPasswordField = false
                                    wifiPage.passwordInput = ""
                                    wifiPasswordField.text = ""
                                    NetworkManager.connectError = ""
                                }
                            }
                        }

                        Text {
                            text: "Wi-Fi Networks"
                            font.family: cfg ? cfg.fontFamily : "Rubik"
                            font.pixelSize: 16
                            font.weight: 600
                            color: col.onSurface
                            Layout.fillWidth: true
                        }

                        // Refresh button
                        Rectangle {
                            width: 36; height: 36; radius: 18
                            color: wifiRefreshMa.containsMouse ? col.surfaceContainerHigh : "transparent"
                            Behavior on color { ColorAnimation { duration: 150 } }

                            MaterialSymbol {
                                anchors.centerIn: parent
                                icon: "refresh"
                                iconSize: 20
                                color: col.onSurfaceVariant

                                RotationAnimator on rotation {
                                    from: 0; to: 360
                                    duration: 900
                                    loops: Animation.Infinite
                                    running: NetworkManager.scanning
                                }
                            }

                            MouseArea {
                                id: wifiRefreshMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: NetworkManager.rescanWifi()
                            }
                        }
                    }
                }

                // ── Scrollable network list ────────────────────────────
                Flickable {
                    anchors.top: wifiPageHeader.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 12
                    anchors.topMargin: 0
                    contentHeight: wifiPageCol.implicitHeight
                    clip: true
                    flickableDirection: Flickable.VerticalFlick
                    boundsBehavior: Flickable.StopAtBounds

                    Column {
                        id: wifiPageCol
                        width: parent.width
                        spacing: 6

                        // ── Error banner ─────────────────────────────
                        Rectangle {
                            width: parent.width
                            height: NetworkManager.connectError !== "" ? wifiErrRow.implicitHeight + 16 : 0
                            clip: true
                            radius: 12
                            color: col.errorContainer
                            visible: height > 0
                            Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                            RowLayout {
                                id: wifiErrRow
                                anchors.left: parent.left; anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 12; anchors.rightMargin: 8
                                spacing: 8

                                MaterialSymbol { icon: "error"; iconSize: 15; color: col.onErrorContainer }

                                Text {
                                    Layout.fillWidth: true
                                    text: NetworkManager.connectError
                                    font.family: cfg ? cfg.fontFamily : "Rubik"
                                    font.pixelSize: 11
                                    color: col.onErrorContainer
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                MaterialSymbol {
                                    icon: "close"; iconSize: 14; color: col.onErrorContainer
                                    MouseArea {
                                        anchors.fill: parent; anchors.margins: -4
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: NetworkManager.connectError = ""
                                    }
                                }
                            }
                        }

                        // ── Password field ────────────────────────────
                        Rectangle {
                            width: parent.width
                            height: wifiPage.showPasswordField ? 48 : 0
                            clip: true
                            radius: 14
                            color: col.surfaceContainerHigh
                            visible: height > 0
                            Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14; anchors.rightMargin: 8
                                spacing: 8

                                MaterialSymbol { icon: "lock"; iconSize: 16; color: col.onSurfaceVariant }

                                Item {
                                    Layout.fillWidth: true
                                    height: 24

                                    Text {
                                        anchors.fill: parent
                                        verticalAlignment: Text.AlignVCenter
                                        text: "Password for " + wifiPage.pendingSsid
                                        font.family: cfg ? cfg.fontFamily : "Rubik"
                                        font.pixelSize: 13
                                        color: col.onSurfaceVariant
                                        opacity: 0.5
                                        visible: wifiPasswordField.text.length === 0
                                    }

                                    TextInput {
                                        id: wifiPasswordField
                                        anchors.fill: parent
                                        verticalAlignment: TextInput.AlignVCenter
                                        echoMode: TextInput.Password
                                        font.family: cfg ? cfg.fontFamily : "Rubik"
                                        font.pixelSize: 13
                                        color: col.onSurface
                                        onTextChanged: wifiPage.passwordInput = text
                                        onAccepted: {
                                            if (wifiPage.passwordInput.length >= 8) {
                                                NetworkManager.connectWithPassword(wifiPage.pendingSsid, wifiPage.passwordInput)
                                                wifiPage.showPasswordField = false
                                                wifiPage.passwordInput = ""
                                                wifiPasswordField.text = ""
                                            }
                                        }
                                    }
                                }

                                // Confirm
                                Rectangle {
                                    width: 30; height: 30; radius: 15
                                    color: col.primary
                                    opacity: wifiPage.passwordInput.length >= 8 ? 1.0 : 0.35
                                    Behavior on opacity { NumberAnimation { duration: 150 } }

                                    MaterialSymbol { anchors.centerIn: parent; icon: "arrow_forward"; iconSize: 16; color: col.onPrimary }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        enabled: wifiPage.passwordInput.length >= 8
                                        onClicked: {
                                            NetworkManager.connectWithPassword(wifiPage.pendingSsid, wifiPage.passwordInput)
                                            wifiPage.showPasswordField = false
                                            wifiPage.passwordInput = ""
                                            wifiPasswordField.text = ""
                                        }
                                    }
                                }

                                // Cancel
                                Rectangle {
                                    width: 30; height: 30; radius: 15
                                    color: cancelPwMa.containsMouse ? col.surfaceContainerHighest : "transparent"
                                    Behavior on color { ColorAnimation { duration: 150 } }

                                    MaterialSymbol { anchors.centerIn: parent; icon: "close"; iconSize: 15; color: col.onSurfaceVariant }

                                    MouseArea {
                                        id: cancelPwMa
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            wifiPage.showPasswordField = false
                                            wifiPage.passwordInput = ""
                                            wifiPasswordField.text = ""
                                        }
                                    }
                                }
                            }
                        }

                        // ── Empty / scanning ──────────────────────────
                        Item {
                            width: parent.width
                            height: 80
                            visible: NetworkManager.wifiNetworks.length === 0

                            Column {
                                anchors.centerIn: parent
                                spacing: 8

                                MaterialSymbol {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    icon: NetworkManager.scanning ? "wifi_find" : "wifi_off"
                                    iconSize: 28
                                    color: col.onSurfaceVariant
                                    opacity: 0.5
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: NetworkManager.scanning ? "Scanning…" : "No networks found"
                                    font.family: cfg ? cfg.fontFamily : "Rubik"
                                    font.pixelSize: 13
                                    color: col.onSurfaceVariant
                                    opacity: 0.5
                                }
                            }
                        }

                        // ── Network rows ──────────────────────────────
                        Repeater {
                            model: NetworkManager.wifiNetworks

                            Rectangle {
                                id: wifiNetRow
                                required property var modelData

                                readonly property string net_ssid:      modelData ? (modelData.ssid     || "") : ""
                                readonly property int    net_signal:    modelData ? (modelData.signal   || 0)  : 0
                                readonly property string net_security:  modelData ? (modelData.security || "") : ""
                                readonly property bool   net_connected: modelData ? !!modelData.connected      : false
                                readonly property bool   net_secured:   net_security !== "" && net_security !== "--"

                                width: parent.width
                                height: 52
                                radius: 16
                                color: {
                                    if (net_connected) return col.primaryContainer
                                    if (wifiRowHover.containsMouse) return col.surfaceContainerHigh
                                    return col.surfaceContainer
                                }
                                Behavior on color { ColorAnimation { duration: 150 } }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: net_connected ? 50 : 12
                                    spacing: 12

                                    MaterialSymbol {
                                        iconSize: 22
                                        fill: 1
                                        color: wifiNetRow.net_connected ? col.onPrimaryContainer : col.onSurface
                                        icon: {
                                            const s = wifiNetRow.net_signal
                                            if (s > 75) return "network_wifi"
                                            if (s > 50) return "network_wifi_3_bar"
                                            if (s > 25) return "network_wifi_2_bar"
                                            return "network_wifi_1_bar"
                                        }
                                        Behavior on color { ColorAnimation { duration: 150 } }
                                    }

                                    Column {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            width: parent.width
                                            text: wifiNetRow.net_ssid
                                            font.family: cfg ? cfg.fontFamily : "Rubik"
                                            font.pixelSize: 13
                                            font.weight: wifiNetRow.net_connected ? 600 : 400
                                            color: wifiNetRow.net_connected ? col.onPrimaryContainer : col.onSurface
                                            elide: Text.ElideRight
                                            Behavior on color { ColorAnimation { duration: 150 } }
                                        }

                                        Text {
                                            visible: NetworkManager.connectingTo === wifiNetRow.net_ssid || wifiNetRow.net_connected
                                            text: NetworkManager.connectingTo === wifiNetRow.net_ssid ? "Connecting…" : "Connected"
                                            font.family: cfg ? cfg.fontFamily : "Rubik"
                                            font.pixelSize: 11
                                            color: wifiNetRow.net_connected ? col.onPrimaryContainer : col.onSurfaceVariant
                                            opacity: 0.8
                                            Behavior on color { ColorAnimation { duration: 150 } }
                                        }
                                    }

                                    MaterialSymbol {
                                        iconSize: 14; icon: "lock"
                                        color: wifiNetRow.net_connected ? col.onPrimaryContainer : col.onSurfaceVariant
                                        opacity: wifiNetRow.net_secured ? 0.7 : 0
                                    }

                                    Text {
                                        text: wifiNetRow.net_signal + "%"
                                        font.family: cfg ? cfg.fontFamily : "Rubik"
                                        font.pixelSize: 11
                                        color: wifiNetRow.net_connected ? col.onPrimaryContainer : col.onSurfaceVariant
                                        opacity: 0.6
                                        Behavior on color { ColorAnimation { duration: 150 } }
                                    }
                                }

                                // Disconnect button
                                Rectangle {
                                    visible: wifiNetRow.net_connected
                                    width: 34; height: 34; radius: 17
                                    z: 2
                                    anchors.right: parent.right
                                    anchors.rightMargin: 9
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: wifiDisconnectMa.containsMouse ? col.error : "transparent"
                                    Behavior on color { ColorAnimation { duration: 150 } }

                                    MaterialSymbol {
                                        anchors.centerIn: parent
                                        icon: "logout"; iconSize: 17
                                        color: wifiDisconnectMa.containsMouse ? col.onError : col.onPrimaryContainer
                                        Behavior on color { ColorAnimation { duration: 150 } }
                                    }

                                    MouseArea {
                                        id: wifiDisconnectMa
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: NetworkManager.disconnectWifi()
                                    }
                                }

                                MouseArea {
                                    id: wifiRowHover
                                    anchors.fill: parent
                                    anchors.rightMargin: wifiNetRow.net_connected ? 50 : 0
                                    hoverEnabled: true
                                    cursorShape: wifiNetRow.net_connected ? Qt.ArrowCursor : Qt.PointingHandCursor
                                    onClicked: {
                                        if (wifiNetRow.net_connected || NetworkManager.connectingTo !== "") return
                                        const hasSaved = NetworkManager.savedProfiles.indexOf(wifiNetRow.net_ssid) !== -1
                                        if (wifiNetRow.net_secured && !hasSaved) {
                                            wifiPage.pendingSsid = wifiNetRow.net_ssid
                                            wifiPage.showPasswordField = true
                                            wifiPage.passwordInput = ""
                                            wifiPasswordField.text = ""
                                            wifiPasswordField.forceActiveFocus()
                                        } else {
                                            NetworkManager.connectTo(wifiNetRow.net_ssid)
                                        }
                                    }
                                }
                            }
                        }

                        Item { width: 1; height: 8 }
                    }
                }
            }
            // end pageContainer
            }
        }
    }
}

