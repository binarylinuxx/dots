import QtQuick
import QtQuick.Layouts
import Quickshell

// Settings row slider: label + optional subtext on left, slider on right
// Used in cards that already have their own RowLayout context
RowLayout {
    id: root

    // Label
    property string label: ""
    property string subtext: ""

    // Slider
    property real value: 0
    property real from: 0
    property real to: 1
    property real stepSize: 0.01
    property bool enabled: true

    // Value display — set to "" to hide, or provide a custom string
    property string valueText: ""

    // Slider width
    property int sliderWidth: 180

    signal moved(real newValue)
    signal changed(real newValue)

    spacing: 15

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        Text {
            text: root.label
            font.pixelSize: 14
            font.family: cfg ? cfg.fontFamily : "Rubik"
            font.weight: 500
            color: col.onSurface
            visible: root.label !== ""
        }

        Text {
            text: root.subtext
            font.pixelSize: 11
            font.family: cfg ? cfg.fontFamily : "Rubik"
            color: col.onSurfaceVariant
            opacity: 0.8
            visible: root.subtext !== ""
        }
    }

    StyledSlider {
        id: slider
        sliderWidth: root.sliderWidth
        sliderHeight: 28
        radius: 8
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
        Layout.preferredWidth: 42
        horizontalAlignment: Text.AlignRight
        visible: root.valueText !== ""
    }
}
