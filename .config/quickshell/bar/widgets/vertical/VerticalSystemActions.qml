import QtQuick
import QtQuick.Layouts
import qs.services
import qs.bar.widgets.vertical

ColumnLayout {
    id: root

    property int itemSize: 34
    property int moduleRadius: cfg ? Math.max(10, Math.round(cfg.barRadius * 0.55)) : 16

    spacing: 7

    Layout.alignment: Qt.AlignHCenter

    VerticalButton {
        itemSize: root.itemSize
        moduleRadius: root.moduleRadius
        icon: Gstate.sidebarOpen ? "right_panel_close" : "right_panel_open"
        active: Gstate.sidebarOpen
        onClicked: Gstate.sidebarOpen = !Gstate.sidebarOpen
    }

    VerticalButton {
        itemSize: root.itemSize
        moduleRadius: root.moduleRadius
        icon: "power_settings_new"
        active: Gstate.powerMenuOpen
        onClicked: Gstate.powerMenuOpen = !Gstate.powerMenuOpen
    }
}
