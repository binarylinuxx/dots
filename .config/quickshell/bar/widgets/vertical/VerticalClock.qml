import QtQuick
import QtQuick.Layouts
import qs.services

Rectangle {
    id: root

    property int itemSize: 34
    property int moduleRadius: cfg ? Math.max(10, Math.round(cfg.barRadius * 0.55)) : 16
    property date now: new Date()
    readonly property string configuredFormat: cfg ? (cfg.clockFormat || "hh:mm AP") : "hh:mm AP"
    readonly property bool use24Hour: configuredFormat.indexOf("H") !== -1 || configuredFormat.indexOf("k") !== -1
    readonly property bool showPeriod: configuredFormat.toLowerCase().indexOf("ap") !== -1 || configuredFormat.toLowerCase().indexOf("a") !== -1
    readonly property int hourValue: use24Hour ? now.getHours() : ((now.getHours() % 12) || 12)
    readonly property string hourText: hourValue < 10 ? ("0" + hourValue) : hourValue.toString()
    readonly property string minuteText: Qt.formatTime(now, "mm")
    readonly property string periodText: showPeriod ? (now.getHours() >= 12 ? "PM" : "AM") : ""

    Layout.alignment: Qt.AlignHCenter
    Layout.preferredWidth: itemSize
    Layout.preferredHeight: showPeriod ? 70 : 58
    radius: moduleRadius
    color: col.surfaceContainer

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: root.now = new Date()
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: -1

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: root.hourText
            font.family: cfg ? cfg.fontFamily : "Rubik"
            font.pixelSize: 13
            font.weight: 800
            color: col.primary
        }

        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 24
            Layout.preferredHeight: 9

            Rectangle {
                width: 22
                height: 2
                radius: 1
                color: col.primary
                opacity: 0.75
                rotation: -28
                anchors.centerIn: parent
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: root.minuteText
            font.family: cfg ? cfg.fontFamily : "Rubik"
            font.pixelSize: 13
            font.weight: 800
            color: col.primary
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            visible: root.showPeriod
            text: root.periodText
            font.family: cfg ? cfg.fontFamily : "Rubik"
            font.pixelSize: 8
            font.weight: 700
            color: col.onSurfaceVariant
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: Gstate.sidebarOpen = !Gstate.sidebarOpen
    }
}
