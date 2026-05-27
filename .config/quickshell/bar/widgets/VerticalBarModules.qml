import QtQuick
import QtQuick.Layouts
import qs.services
import qs.bar.widgets.vertical

Item {
    id: root

    anchors.fill: parent

    property int moduleRadius: cfg ? Math.max(10, Math.round(cfg.barRadius * 0.55)) : 16
    property int itemSize: 34

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 7

        VerticalProfile {
            itemSize: root.itemSize
            moduleRadius: root.moduleRadius
        }

        VerticalActions {
            itemSize: root.itemSize
            moduleRadius: root.moduleRadius
        }

        VerticalWorkspaces {}

        VerticalSystemActions {
            itemSize: root.itemSize
            moduleRadius: root.moduleRadius
        }

        VerticalClock {
            itemSize: root.itemSize
            moduleRadius: root.moduleRadius
        }
    }
}
