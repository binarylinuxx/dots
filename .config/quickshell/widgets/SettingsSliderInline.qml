import QtQuick
import QtQuick.Layouts
import Quickshell

// Inline settings slider: short label on left, fill-width slider, value text on right
// Used in the launcher page and compact rows
RowLayout {
    id: root

    property string label: ""
    property int labelWidth: 100

    property real value: 0
    property real from: 0
    property real to: 1
    property real stepSize: 1
    property bool enabled: true

    property string valueText: ""
    property int valueTextWidth: 45

    signal moved(real newValue)
    signal changed(real newValue)

    spacing: 15

    Text {
        text: root.label
        font.pixelSize: 13
        font.family: cfg ? cfg.fontFamily : "Rubik"
        color: col.onSurfaceVariant
        Layout.preferredWidth: root.labelWidth
    }

    StyledSlider {
        id: slider
        Layout.fillWidth: true
        sliderHeight: 26
        radius: 7
        from: root.from
        to: root.to
        stepSize: root.stepSize
        value: root.value
        enabled: root.enabled

        onMoved: function(v) {
            root.moved(v)
            root.changed(v)
        }
    }

    Text {
        text: root.valueText
        font.pixelSize: 12
        font.family: cfg ? cfg.fontFamily : "Rubik"
        color: col.onSurfaceVariant
        Layout.preferredWidth: root.valueTextWidth
        horizontalAlignment: Text.AlignRight
        visible: root.valueText !== ""
    }
}
