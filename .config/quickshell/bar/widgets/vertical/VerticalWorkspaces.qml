import QtQuick
import QtQuick.Layouts
import qs.bar.widgets
import qs.services

Item {
    id: root

    Layout.fillHeight: true
    Layout.fillWidth: true

    Workspaces {
        anchors.centerIn: parent
        width: 36
        height: Math.min(parent.height, (cfg ? cfg.workspaceCount : 10) * 29 + 8)
        vertical: true
    }
}
