import Quickshell
import Quickshell.Wayland
import QtQuick

PanelWindow {
    WlrLayershell.layer: WlrLayer.Background
    exclusionMode: ExclusionMode.Ignore

    width: screen.width
    height: screen.height
    color: "transparent"

    // Main container – centers the whole clock block
    Item {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 50

        Column {
            spacing: 8               // space between time and date
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top

            // Time – big and prominent
            Text {
                id: timeText

                property var currentTime: new Date()

                Timer {
                    interval: 1000
                    repeat: true
                    running: true
                    onTriggered: timeText.currentTime = new Date()
                }

                text: Qt.formatDateTime(currentTime, "hh:mmAP")   // or "HH:mm" for 24h
                font.family: cfg.fontFamily
                font.pixelSize: 100                        // ← adjust as you like
                color: col.primary
                anchors.horizontalCenter: parent.horizontalCenter
            }

            // Date – smaller, numeric, perfectly centered under time
            Text {
                text: Qt.formatDateTime(new Date(), "dd/MM/yyyy")
                font.family: cfg.fontFamily
                font.pixelSize: 100                        // ← smaller than time
                color: /*Qt.lighter(col.primary, 1.4)*/ col.primary       // slightly faded, or use "#aaa"
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }
}
