import QtQuick
import QtQuick.Layouts
import qs.services
import qs.widgets

Rectangle {
    id: root

    signal clicked()

    property string icon: "circle"
    property bool active: false
    property int itemSize: 34
    property int moduleRadius: cfg ? Math.max(10, Math.round(cfg.barRadius * 0.55)) : 16

    Layout.preferredWidth: itemSize
    Layout.preferredHeight: itemSize
    radius: moduleRadius
    color: active ? col.primaryContainer : (hover.containsMouse ? col.surfaceContainerHighest : col.surfaceContainer)

    Behavior on color { ColorAnimation { duration: Gstate.animDuration } }

    MaterialSymbol {
        anchors.centerIn: parent
        icon: root.icon
        iconSize: 19
        color: root.active ? col.onPrimaryContainer : col.primary
        Behavior on color { ColorAnimation { duration: Gstate.animDuration } }
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
