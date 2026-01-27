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
    property int sliderHeight: 28

    // Styling
    property color trackColor: col.surfaceContainerHigh
    property color fillColor: col.primary
    property color handleColor: col.onPrimary
    property int radius: 14
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
        
        // Apply step size
        if (stepSize > 0) {
            newValue = Math.round(newValue / stepSize) * stepSize
        }
        
        // Clamp to range
        newValue = Math.max(from, Math.min(to, newValue))
        
        if (Math.abs(value - newValue) > 0.0001) {
            value = newValue
            moved(newValue)
        }
    }

    // === UI ===
    ClippingRectangle {
        id: track
        anchors.centerIn: parent
        width: styledSlider.sliderWidth
        height: styledSlider.sliderHeight
        color: styledSlider.trackColor
        radius: styledSlider.radius

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

            onWheel: {
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
                const pos = Math.max(0, Math.min(track.width, xPos))
                const norm = pos / track.width
                _setFromNormalized(norm)
            }
        }

        // Fill bar
        Rectangle {
            id: fill
            height: track.height
            width: Math.max(0, track.width * _normalizedValue)
            color: styledSlider.fillColor
            radius: styledSlider.radius

            Behavior on width {
                NumberAnimation { duration: 50; easing.type: Easing.OutQuad }
            }
        }

    }
}
