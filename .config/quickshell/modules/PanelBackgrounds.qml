import Quickshell
import Quickshell.Wayland
import QtQuick
import QtQuick.Shapes 1.0
import QtQuick.Effects
import qs.services

// Fullscreen overlay that draws a colored border frame around the screen.
PanelWindow {
    id: root

    anchors.top: true
    anchors.left: true
    anchors.right: true
    exclusionMode: ExclusionMode.Ignore
    implicitHeight: screen ? screen.height : 1080
    focusable: false
    color: "transparent"
    WlrLayershell.layer: WlrLayer.Top

    mask: Region {}

    readonly property int thickness: 15

    // ── Border frame: fill screen with col.background, mask out inner rect ──
    Rectangle {
        anchors.fill: parent
        color: col.background

        layer.enabled: true
        layer.effect: MultiEffect {
            maskSource: frameMask
            maskEnabled: true
            maskInverted: true
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1
        }
    }

    Item {
        id: frameMask
        anchors.fill: parent
        layer.enabled: true
        visible: false

        Rectangle {
            x: root.thickness
            y: root.thickness
            width: parent.width - root.thickness * 2
            height: parent.height - root.thickness * 2
        }
    }
}
