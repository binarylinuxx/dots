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
    property color trackColor: col.surfaceContainerHighest
    property color fillColor: col.primary
    property color handleColor: col.onPrimary
    property int radius: 14
    property bool showHandle: true
    property int trackThickness: 6
    property int thumbSize: 16

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
    Item {
        id: sliderBody
        anchors.centerIn: parent
        width: styledSlider.sliderWidth
        height: styledSlider.sliderHeight
        opacity: styledSlider.enabled ? 1.0 : 0.45

        Rectangle {
            id: inactiveTrack
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: styledSlider.trackThickness
            radius: height / 2
            color: styledSlider.trackColor
        }

        Rectangle {
            id: activeTrack
            anchors.left: inactiveTrack.left
            anchors.verticalCenter: inactiveTrack.verticalCenter
            width: Math.max(0, inactiveTrack.width * _normalizedValue)
            height: inactiveTrack.height
            radius: inactiveTrack.radius
            color: styledSlider.fillColor

            Behavior on width {
                NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
            }
        }

        Item {
            id: thumb
            visible: styledSlider.showHandle
            width: styledSlider.thumbSize + (mouseArea.pressed ? 4 : 0)
            height: width
            x: inactiveTrack.x + (inactiveTrack.width * _normalizedValue) - width / 2
            y: inactiveTrack.y + inactiveTrack.height / 2 - height / 2

            Behavior on x {
                NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width + (mouseArea.containsMouse || mouseArea.pressed ? 10 : 2)
                height: width
                radius: width / 2
                color: styledSlider.fillColor
                opacity: mouseArea.pressed ? 0.22 : (mouseArea.containsMouse ? 0.12 : 0)

                Behavior on opacity {
                    NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                }
                Behavior on width {
                    NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: width
                radius: width / 2
                color: styledSlider.fillColor
                border.width: 2
                border.color: styledSlider.handleColor

                Behavior on width {
                    NumberAnimation { duration: 100; easing.type: Easing.OutCubic }
                }
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
                const pos = Math.max(0, Math.min(inactiveTrack.width, xPos - inactiveTrack.x))
                const norm = pos / inactiveTrack.width
                _setFromNormalized(norm)
            }
        }
    }
}
