import QtQuick
import QtQuick.Layouts
import qs.services
import qs.bar.widgets.vertical

ColumnLayout {
    id: root

    property int itemSize: 34
    property int moduleRadius: cfg ? Math.max(10, Math.round(cfg.barRadius * 0.55)) : 16

    spacing: 7

    VerticalButton {
        itemSize: root.itemSize
        moduleRadius: root.moduleRadius
        icon: "apps"
        active: Gstate.appsOpen
        onClicked: Gstate.appsOpen = !Gstate.appsOpen
    }

    VerticalButton {
        itemSize: root.itemSize
        moduleRadius: root.moduleRadius
        icon: "crop_free"
        active: Gstate.screenshotOpen
        onClicked: Gstate.screenshotOpen = !Gstate.screenshotOpen
    }

    VerticalButton {
        itemSize: root.itemSize
        moduleRadius: root.moduleRadius
        icon: "settings"
        active: Gstate.settingsOpen
        onClicked: Gstate.settingsOpen = !Gstate.settingsOpen
    }
}
