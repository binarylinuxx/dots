import Quickshell
import Quickshell.Wayland
import qs.services

// Layout reservation only. This surface never draws or receives input.
PanelWindow {
    id: root
    required property string edge
    property var bar: null
    property bool enabled: true
    readonly property bool vertical: edge === "left" || edge === "right"
    // FrameExclusion already reserves the outer frame. Add only the remaining bar depth.
    readonly property int thickness: Math.max(0, Math.round(bar ? (vertical ? bar.width : bar.height) : Gstate.barThickness) - Gstate.frameThickness)
    visible: enabled && !Gstate.barFloating
    exclusiveZone: visible ? thickness : 0
    color: "transparent"
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.namespace: "blxshell-bar-exclusion"
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    mask: Region { }
    anchors.top: vertical || edge === "top"
    anchors.bottom: vertical || edge === "bottom"
    anchors.left: !vertical || edge === "left"
    anchors.right: !vertical || edge === "right"
    implicitWidth: vertical ? thickness : 1
    implicitHeight: vertical ? 1 : thickness
}
