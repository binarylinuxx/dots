import QtQuick
import QtQuick.Layouts
import Quickshell
import qs.services
import qs.widgets

Item {
    id: root
    visible: BatteryService.present
    width: batteryRow.width + 10

    property int  pct:      BatteryService.percentage
    property bool charging: BatteryService.charging

    // ── Icon + optional percent label ──────────────────────────────
    RowLayout {
        id: batteryRow
        anchors.centerIn: parent
        spacing: 4

        MaterialSymbol {
            id: batteryIcon
            iconSize: 20
            fill: 1
            color: {
                if (root.charging)    return col.primary
                if (root.pct <= 15)   return col.error
                if (root.pct <= 30)   return "#f0b429"  // amber warning
                return col.primary
            }
            icon: {
                if (root.charging)  return "battery_charging_full"
                if (root.pct > 90)  return "battery_full"
                if (root.pct > 70)  return "battery_6_bar"
                if (root.pct > 55)  return "battery_5_bar"
                if (root.pct > 40)  return "battery_4_bar"
                if (root.pct > 25)  return "battery_3_bar"
                if (root.pct > 15)  return "battery_2_bar"
                if (root.pct > 5)   return "battery_1_bar"
                return "battery_0_bar"
            }

            Behavior on color { ColorAnimation { duration: Gstate.animDuration } }
        }

        // Percent label — only visible on hover
        Text {
            id: pctLabel
            text: root.pct + "%"
            font.family: cfg ? cfg.fontFamily : "Rubik"
            font.pixelSize: 12
            font.weight: 500
            color: col.primary
            opacity: hoverArea.containsMouse ? 1 : 0
            visible: opacity > 0

            Behavior on opacity {
                NumberAnimation { duration: Gstate.animDuration }
            }
        }
    }

    // ── Hover area ─────────────────────────────────────────────────
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.ArrowCursor
        // expand hit area a bit
        anchors.margins: -4
    }
}
