import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Widgets

Item {
    id: styledSlider

    // === PUBLIC PROPERTIES ===
    property real value: 0.5
    property real from: 0.0
    property real to: 1.0
    property real stepSize: 0.01

    // Size
    property int sliderWidth: 180
    property int sliderHeight: 36

    // Styling
    property color trackColor: col.surfaceContainerHighest
    property color fillColor: col.primary
    property color handleColor: col.primary
    property int radius: sliderHeight / 2
    property bool showHandle: true

    // Interaction
    property bool enabled: true

    // Signals
    signal moved(real newValue)
    signal userInteractionStarted()
    signal userInteractionEnded()

    // === IMPLICIT SIZE ===
    implicitWidth: sliderWidth
    implicitHeight: sliderHeight

    // === NORMALIZED VALUE (internal 0.0-1.0) ===
    readonly property real _normalizedValue: {
        if (to === from) return 0
        return Math.max(0, Math.min(1, (value - from) / (to - from)))
    }

    // === UPDATE VALUE FROM NORMALIZED ===
    function _setFromNormalized(norm) {
        const clamped = Math.max(0, Math.min(1, norm))
        let newValue = from + clamped * (to - from)

        if (stepSize > 0) {
            newValue = Math.round(newValue / stepSize) * stepSize
        }

        newValue = Math.max(from, Math.min(to, newValue))

        if (Math.abs(value - newValue) > 0.0001) {
            value = newValue
            moved(newValue)
        }
    }

    // === UI ===
    Item {
        id: sliderBody
        anchors.fill: parent
        opacity: styledSlider.enabled ? 1.0 : 0.45

        // Outer track pill
        Rectangle {
            id: track
            anchors.fill: parent
            radius: styledSlider.radius
            color: styledSlider.trackColor
        }

        // Filled portion — inset 2px vertically, left corners rounded, right edge flat
        Rectangle {
            id: fill
            readonly property int inset: 2
            readonly property real fillRadius: Math.max(0, styledSlider.radius - inset)

            anchors.left: track.left
            anchors.leftMargin: inset
            anchors.top: track.top
            anchors.topMargin: inset
            anchors.bottom: track.bottom
            anchors.bottomMargin: inset

            // Right edge flush with handle's left edge
            width: Math.max(fillRadius * 2, handle.x - inset - 4)
            topLeftRadius: fillRadius
            bottomLeftRadius: fillRadius
            topRightRadius: 2
            bottomRightRadius: 2
            color: styledSlider.fillColor

            Behavior on width {
                NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
            }
        }

        // Handle — pill, 1px inset vertically (taller than fill), left edge touches fill
        Rectangle {
            id: handle
            visible: styledSlider.showHandle

            readonly property int vPadding: 4
            readonly property real travelWidth: track.width - fill.inset * 2 - width

            width: Math.round(track.height * 3 / 16)  // ~proportional to SVG w=3/h=16
            height: track.height + 5 // full track height — QML renders slightly smaller than spec
            radius: width / 2
            color: styledSlider.handleColor

            anchors.verticalCenter: track.verticalCenter
            x: fill.inset + _normalizedValue * travelWidth

            Behavior on x {
                NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
            }

            // Pressed state: slight opacity drop
            opacity: mouseArea.pressed ? 0.6 : 1.0
            Behavior on opacity {
                NumberAnimation { duration: 80 }
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            enabled: styledSlider.enabled
            hoverEnabled: true
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

            onPressed: {
                userInteractionStarted()
                updateFromPosition(mouse.x)
            }
            onPositionChanged: {
                if (pressed) updateFromPosition(mouse.x)
            }
            onReleased: {
                userInteractionEnded()
            }

            onWheel: function(wheel) {
                if (!styledSlider.enabled) return
                const range = to - from
                const step = stepSize > 0 ? stepSize : range * 0.01
                const delta = wheel.angleDelta.y > 0 ? step : -step
                const newVal = Math.max(from, Math.min(to, value + delta))
                if (Math.abs(value - newVal) > 0.0001) {
                    value = newVal
                    moved(newVal)
                }
                wheel.accepted = true
            }

            function updateFromPosition(xPos) {
                const inset = fill.inset
                const travel = handle.travelWidth
                const pos = Math.max(0, Math.min(travel, xPos - inset - handle.width / 2))
                const norm = pos / travel
                _setFromNormalized(norm)
            }
        }
    }
}
