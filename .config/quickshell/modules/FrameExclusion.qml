import Quickshell
import Quickshell.Wayland

// Layout reservation only. This surface has no visual or input region.
PanelWindow {
    id: root

    required property string edge
    required property int thickness
    property bool enabled: true
    readonly property bool vertical: edge === "left" || edge === "right"

    visible: enabled
    exclusiveZone: visible ? thickness : 0
    color: "transparent"
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.namespace: "blxshell-frame-exclusion"
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    mask: Region {}
    anchors.top: vertical || edge === "top"
    anchors.bottom: vertical || edge === "bottom"
    anchors.left: !vertical || edge === "left"
    anchors.right: !vertical || edge === "right"
    implicitWidth: vertical ? thickness : 1
    implicitHeight: vertical ? 1 : thickness
}
