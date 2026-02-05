import Quickshell
import Quickshell.Wayland
import QtQuick
import qs.widgets

PanelWindow {
    id: root
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "quickshell:hotcorner"
    exclusionMode: ExclusionMode.Ignore

    anchors.top: true
    anchors.left: true

    width: hitSize
    height: hitSize
    color: "transparent"

    // Configuration
    property int hitSize: 48
    property int visualSize: 28
    property int triggerDelay: 400
    property color accentColor: col?.primary || "#adc6ff"
    property color dimColor: col?.surfaceContainerHighest || "#36343b"

    // State
    property bool isHovering: false
    property bool isActive: false
    property real progress: 0

    signal triggered()

    // Smooth progress arc drawn with Canvas
    Canvas {
        id: arcCanvas
        width: visualSize
        height: visualSize
        anchors.top: parent.top
        anchors.left: parent.left

        property real prog: root.progress
        onProgChanged: requestPaint()
        Component.onCompleted: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()

            var cx = width
            var cy = height
            var r = width - 3

            // Background arc (quarter circle, top-left corner)
            // Always visible at low opacity so you know it's there
            ctx.beginPath()
            ctx.arc(cx, cy, r, Math.PI, 1.5 * Math.PI)
            ctx.lineWidth = 2.5
            ctx.strokeStyle = isHovering || isActive ? dimColor : Qt.rgba(1, 1, 1, 0.15)
            ctx.stroke()

            // Idle dot — small hint at the corner
            if (!isHovering && !isActive) {
                ctx.beginPath()
                ctx.arc(cx, cy, 2.5, 0, 2 * Math.PI)
                ctx.fillStyle = Qt.rgba(1, 1, 1, 0.3)
                ctx.fill()
            }

            // Progress arc
            if (prog > 0) {
                var startAngle = Math.PI
                var endAngle = Math.PI + (0.5 * Math.PI * prog)
                ctx.beginPath()
                ctx.arc(cx, cy, r, startAngle, endAngle)
                ctx.lineWidth = 2.5
                ctx.strokeStyle = accentColor
                ctx.lineCap = "round"
                ctx.stroke()
            }

            // Dot at corner when active
            if (isActive) {
                ctx.beginPath()
                ctx.arc(cx, cy, 3, 0, 2 * Math.PI)
                ctx.fillStyle = accentColor
                ctx.fill()
            }
        }
    }

    // Small edit icon that fades in on hover
    MaterialSymbol {
        icon: isActive ? "close" : "edit"
        iconSize: 12
        color: accentColor
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 2
        anchors.leftMargin: 2
        opacity: isHovering || isActive ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
        }
    }

    // Progress animation
    NumberAnimation {
        id: progressAnim
        target: root
        property: "progress"
        from: 0
        to: 1
        duration: triggerDelay
        easing.type: Easing.InOutCubic
        onFinished: {
            if (hoverArea.containsMouse) {
                triggerAction()
            }
        }
    }

    // Fade out animation for progress on exit
    NumberAnimation {
        id: fadeOutAnim
        target: root
        property: "progress"
        to: 0
        duration: 200
        easing.type: Easing.OutCubic
    }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true

        onEntered: {
            isHovering = true
            fadeOutAnim.stop()
            progress = 0
            progressAnim.start()
        }

        onExited: {
            isHovering = false
            progressAnim.stop()
            if (!isActive) {
                fadeOutAnim.start()
            } else {
                progress = 0
            }
            arcCanvas.requestPaint()
        }

        onClicked: {
            triggerAction()
        }
    }

    function triggerAction() {
        progressAnim.stop()
        fadeOutAnim.stop()
        progress = 0
        isActive = !isActive
        triggered()
        arcCanvas.requestPaint()
    }
}
