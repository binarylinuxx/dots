import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import QtQuick
import QtQuick.Layouts
import qs.services
import qs.widgets

PanelWindow {
    id: root

    visible: Gstate.screenshotOpen
    implicitWidth: screen.width
    implicitHeight: screen.height
    color: "transparent"
    focusable: true
    exclusiveZone: 0
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.namespace: "blxshell:screenshot-overlay"
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive

    anchors {
        top: true
        bottom: true
        left: true
        right: true
    }

    margins {
        top: root.barPosition === "top" ? -root.reservedBarHeight : 0
        bottom: root.barPosition === "bottom" ? -root.reservedBarHeight : 0
        left: root.barPosition === "left" ? -root.reservedBarWidth : 0
        right: root.barPosition === "right" ? -root.reservedBarWidth : 0
    }

    property string selectedMode: "region"
    property real frameX: 0
    property real frameY: 0
    property real frameW: 0
    property real frameH: 0
    readonly property real minFrameW: 220
    readonly property real minFrameH: 130
    readonly property string barPosition: cfg ? (cfg.barPosition || "bottom") : "bottom"
    readonly property real reservedBarWidth: cfg ? (cfg.barWidth || 46) : 46
    readonly property real reservedBarHeight: cfg ? (cfg.barHeight || 35) : 35

    onVisibleChanged: {
        if (visible) {
            resetFrame()
            keyScope.forceActiveFocus()
        }
    }

    function resetFrame() {
        if (screen.width <= 0 || screen.height <= 0)
            return

        frameW = Math.min(screen.width * 0.56, 760)
        frameH = Math.min(screen.height * 0.30, 320)
        frameX = (screen.width - frameW) / 2
        frameY = (screen.height - frameH) / 2 - 48
        clampFrame()
    }

    function close() {
        Gstate.screenshotOpen = false
    }

    function clampFrame() {
        frameW = Math.max(minFrameW, Math.min(frameW, screen.width))
        frameH = Math.max(minFrameH, Math.min(frameH, screen.height))
        frameX = Math.max(0, Math.min(frameX, screen.width - frameW))
        frameY = Math.max(0, Math.min(frameY, screen.height - frameH))
    }

    function regionGeometry() {
        return Math.round(frameX) + "," + Math.round(frameY) + " " + Math.round(frameW) + "x" + Math.round(frameH)
    }

    function runSelected() {
        runScreenshot(selectedMode)
    }

    function selectMode(mode) {
        selectedMode = mode
        if (mode === "fullscreen") {
            frameX = 0
            frameY = 0
            frameW = screen.width
            frameH = screen.height
        } else if (mode === "window") {
            fetchActiveWindowGeometry()
        } else {
            resetFrame()
        }
        keyScope.forceActiveFocus()
    }

    function fetchActiveWindowGeometry() {
        if (windowGeometryProcess.running)
            return

        windowGeometryProcess.command = ["sh", "-c", "hyprctl activewindow -j | jq -r '\"\\(.at[0]),\\(.at[1]) \\(.size[0])x\\(.size[1])\"'"]
        windowGeometryProcess.running = true
    }

    function fetchNearestWindowGeometry() {
        if (nearestWindowProcess.running)
            return

        const cx = Math.round(frameX + frameW / 2)
        const cy = Math.round(frameY + frameH / 2)
        nearestWindowProcess.command = ["sh", "-c",
            "hyprctl clients -j | jq -r --argjson cx " + cx + " --argjson cy " + cy + " '"
            + "[.[] | select(.mapped == true and .hidden == false) | "
            + "{x: .at[0], y: .at[1], w: .size[0], h: .size[1], "
            + "score: (((.at[0] + (.size[0] / 2)) - $cx) * ((.at[0] + (.size[0] / 2)) - $cx) + ((.at[1] + (.size[1] / 2)) - $cy) * ((.at[1] + (.size[1] / 2)) - $cy))}] "
            + "| sort_by(.score) | .[0] | select(. != null) | \"\\(.x),\\(.y) \\(.w)x\\(.h)\"'"
        ]
        nearestWindowProcess.running = true
    }

    function applyGeometryString(geometry) {
        const match = geometry.match(/(-?\d+),(-?\d+)\s+(\d+)x(\d+)/)
        if (!match)
            return

        frameX = parseInt(match[1])
        frameY = parseInt(match[2])
        frameW = parseInt(match[3])
        frameH = parseInt(match[4])
        clampFrame()
    }

    function runScreenshot(mode) {
        selectedMode = mode
        if (mode === "fullscreen") {
            frameX = 0
            frameY = 0
            frameW = screen.width
            frameH = screen.height
        }
        close()

        let command = ""
        if (mode === "fullscreen") {
            command = "if command -v grimblast >/dev/null 2>&1; then grimblast --notify copy screen; "
                + "elif command -v grim >/dev/null 2>&1 && command -v wl-copy >/dev/null 2>&1; then grim - | wl-copy; "
                + "else notify-send 'Screenshot' 'No screenshot tool found'; fi"
        } else if (mode === "window") {
            const geo = regionGeometry()
            command = "if command -v grim >/dev/null 2>&1 && command -v wl-copy >/dev/null 2>&1; then grim -g '" + geo + "' - | wl-copy; "
                + "elif command -v grimblast >/dev/null 2>&1; then grimblast --notify copy active; "
                + "else notify-send 'Screenshot' 'No screenshot tool found'; fi"
        } else {
            const geo = regionGeometry()
            command = "if command -v grim >/dev/null 2>&1 && command -v wl-copy >/dev/null 2>&1; then grim -g '" + geo + "' - | wl-copy; "
                + "elif command -v grimblast >/dev/null 2>&1; then grimblast --notify copy area; "
                + "elif command -v flameshot >/dev/null 2>&1; then flameshot gui; "
                + "else notify-send 'Screenshot' 'No screenshot tool found'; fi"
        }

        screenshotProcess.command = ["sh", "-c", command]
        screenshotProcess.running = true
    }

    Process {
        id: screenshotProcess
        running: false
    }

    Process {
        id: windowGeometryProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: root.applyGeometryString(text.trim())
        }
    }

    Process {
        id: nearestWindowProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: root.applyGeometryString(text.trim())
        }
    }

    Item {
        id: rootContent
        anchors.fill: parent

        onWidthChanged: if (root.visible) root.resetFrame()
        onHeightChanged: if (root.visible) root.resetFrame()

        Rectangle {
            anchors.fill: parent
            color: col ? col.background : "#101010"
            opacity: 0.72

            MouseArea {
                anchors.fill: parent
                onClicked: root.close()
            }
        }

        Rectangle {
            id: selectionFrame
            x: root.frameX
            y: root.frameY
            width: root.frameW
            height: root.frameH
            radius: 18
            color: col ? Qt.rgba(col.surfaceContainerHighest.r, col.surfaceContainerHighest.g, col.surfaceContainerHighest.b, 0.16) : Qt.rgba(1, 1, 1, 0.08)
            border.width: 0

            Rectangle {
                anchors.fill: parent
                anchors.margins: 10
                radius: 12
                color: col ? Qt.rgba(col.surface.r, col.surface.g, col.surface.b, 0.10) : Qt.rgba(0, 0, 0, 0.16)
                border.width: 0
            }

            Text {
                anchors.centerIn: parent
                width: implicitWidth + 16
                height: 24

                Rectangle {
                    anchors.fill: parent
                    radius: 12
                    color: col ? col.surfaceContainerHigh : "#2a2a2a"
                    opacity: 0.92
                }

                Text {
                    anchors.centerIn: parent
                    text: Math.round(root.frameW) + " x " + Math.round(root.frameH)
                    color: col ? col.onSurfaceVariant : "#d7d7d7"
                    font.family: cfg ? cfg.fontFamily : "Rubik"
                    font.pixelSize: 12
                    font.weight: 500
                }
            }

            Repeater {
                model: [
                    { edge: "top", x: 0.5, y: 0 },
                    { edge: "bottom", x: 0.5, y: 1 },
                    { edge: "left", x: 0, y: 0.5 },
                    { edge: "right", x: 1, y: 0.5 }
                ]

                Rectangle {
                    required property var modelData
                    readonly property bool horizontal: modelData.edge === "top" || modelData.edge === "bottom"
                    width: horizontal ? 82 : 8
                    height: horizontal ? 8 : 82
                    visible: root.selectedMode !== "region"
                    radius: 4
                    color: col ? col.primaryContainer : "#5a2d00"
                    opacity: 0.95
                    x: modelData.x === 0 ? -width / 2 : (modelData.x === 1 ? selectionFrame.width - width / 2 : (selectionFrame.width - width) / 2)
                    y: modelData.y === 0 ? -height / 2 : (modelData.y === 1 ? selectionFrame.height - height / 2 : (selectionFrame.height - height) / 2)
                }
            }

            /*
            Text {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: 16
                anchors.topMargin: -28
                text: Math.round(root.frameW) + " x " + Math.round(root.frameH)
                color: Qt.rgba(1, 1, 1, 0.72)
                font.family: cfg ? cfg.fontFamily : "Rubik"
                font.pixelSize: 12
            }
            */

            MouseArea {
                id: moveArea
                anchors.fill: parent
                anchors.margins: 16
                enabled: root.selectedMode === "region" || root.selectedMode === "window"
                cursorShape: Qt.SizeAllCursor
                property real pressGlobalX: 0
                property real pressGlobalY: 0
                property real startX: 0
                property real startY: 0
                onPressed: mouse => {
                    const pos = mapToItem(rootContent, mouse.x, mouse.y)
                    pressGlobalX = pos.x
                    pressGlobalY = pos.y
                    startX = root.frameX
                    startY = root.frameY
                    keyScope.forceActiveFocus()
                }
                onPositionChanged: mouse => {
                    if (!pressed)
                        return

                    const pos = mapToItem(rootContent, mouse.x, mouse.y)
                    root.frameX = startX + pos.x - pressGlobalX
                    root.frameY = startY + pos.y - pressGlobalY
                    root.clampFrame()
                }
                onReleased: {
                    if (root.selectedMode === "window")
                        root.fetchNearestWindowGeometry()
                }
            }

            Repeater {
                model: ["tl", "tr", "bl", "br"]

                Rectangle {
                    id: handle
                    required property string modelData
                    readonly property bool isLeftHandle: modelData.indexOf("l") !== -1
                    readonly property bool topSide: modelData.indexOf("t") !== -1

                    width: 26
                    height: 26
                    visible: root.selectedMode === "region"
                    radius: 13
                    color: col ? col.primary : "#ff7a1a"
                    border.width: 0
                    scale: mouse.containsMouse ? 1.12 : 1.0
                    x: isLeftHandle ? -width / 2 : selectionFrame.width - width / 2
                    y: topSide ? -height / 2 : selectionFrame.height - height / 2

                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

                    MouseArea {
                        id: mouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: handle.isLeftHandle === handle.topSide ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                        property real pressGlobalX: 0
                        property real pressGlobalY: 0
                        property real startX: 0
                        property real startY: 0
                        property real startW: 0
                        property real startH: 0

                        onPressed: mouse => {
                            const pos = mapToItem(rootContent, mouse.x, mouse.y)
                            pressGlobalX = pos.x
                            pressGlobalY = pos.y
                            startX = root.frameX
                            startY = root.frameY
                            startW = root.frameW
                            startH = root.frameH
                            keyScope.forceActiveFocus()
                        }

                        onPositionChanged: mouse => {
                            if (!pressed)
                                return

                            const pos = mapToItem(rootContent, mouse.x, mouse.y)
                            const dx = pos.x - pressGlobalX
                            const dy = pos.y - pressGlobalY

                            if (handle.isLeftHandle) {
                                root.frameX = startX + dx
                                root.frameW = startW - dx
                            } else {
                                root.frameW = startW + dx
                            }

                            if (handle.topSide) {
                                root.frameY = startY + dy
                                root.frameH = startH - dy
                            } else {
                                root.frameH = startH + dy
                            }

                            root.clampFrame()
                        }
                    }
                }
            }
        }

        Rectangle {
            id: modePill
            x: (rootContent.width - width) / 2
            y: rootContent.height - height - 68
            width: modeRow.implicitWidth + 18
            height: 48
            radius: 24
            color: col ? col.surfaceContainer : "#242424"
            border.width: 0

            RowLayout {
                id: modeRow
                anchors.centerIn: parent
                spacing: 4

                ModeButton {
                    label: "FULLSCREEN"
                    mode: "fullscreen"
                }

                ModeButton {
                    label: "WINDOW"
                    mode: "window"
                }

                ModeButton {
                    label: "REGION"
                    mode: "region"
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 24
                    color: Qt.rgba(1, 1, 1, 0.12)
                }

                CaptureButton {}
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: modePill.top
            anchors.bottomMargin: 12
            text: "Choose mode, drag region frame if needed. Enter captures, Esc cancels."
            color: Qt.rgba(1, 1, 1, 0.62)
            font.family: cfg ? cfg.fontFamily : "Rubik"
            font.pixelSize: 12
        }

        Item {
            id: keyScope
            anchors.fill: parent
            focus: root.visible
            Keys.onEscapePressed: root.close()
            Keys.onReturnPressed: root.runSelected()
            Keys.onEnterPressed: root.runSelected()
        }
    }

    component ModeButton: Rectangle {
        id: button
        property string label: ""
        property string mode: "region"
        readonly property bool active: root.selectedMode === mode

        Layout.preferredWidth: label === "FULLSCREEN" ? 104 : 78
        Layout.preferredHeight: 34
        radius: 17
        color: active
            ? (col ? col.primaryContainer : "#3a2b00")
            : (hover.containsMouse ? (col ? col.surfaceContainerHighest : "#333333") : "transparent")

        Text {
            anchors.centerIn: parent
            text: button.label
            color: button.active
                ? (col ? col.onPrimaryContainer : "#ffe0b2")
                : (col ? col.onSurfaceVariant : "#ececec")
            font.family: cfg ? cfg.fontFamily : "Rubik"
            font.pixelSize: 11
            font.weight: 600
        }

        MouseArea {
            id: hover
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                root.selectMode(button.mode)
            }
        }
    }

    component CaptureButton: Rectangle {
        Layout.preferredWidth: 86
        Layout.preferredHeight: 34
        radius: 17
        color: hover.containsMouse ? (col ? col.primaryContainer : "#ff8a2a") : (col ? col.primary : "#ff7a1a")

        RowLayout {
            anchors.centerIn: parent
            spacing: 5

            MaterialSymbol {
                icon: "photo_camera"
                iconSize: 16
                color: hover.containsMouse ? (col ? col.onPrimaryContainer : "#101010") : (col ? col.onPrimary : "#101010")
            }

            Text {
                text: "CAPTURE"
                color: hover.containsMouse ? (col ? col.onPrimaryContainer : "#101010") : (col ? col.onPrimary : "#101010")
                font.family: cfg ? cfg.fontFamily : "Rubik"
                font.pixelSize: 11
                font.weight: 700
            }
        }

        MouseArea {
            id: hover
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.runSelected()
        }
    }
}
