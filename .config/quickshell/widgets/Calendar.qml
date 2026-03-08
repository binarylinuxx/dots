import QtQuick
import QtQuick.Layouts
import qs.widgets
import qs.services

Item {
    id: calendarRoot

    property var displayDate: new Date()

    readonly property int displayYear: displayDate.getFullYear()
    readonly property int displayMonth: displayDate.getMonth()  // 0-11
    readonly property int todayDay: new Date().getDate()
    readonly property int todayMonth: new Date().getMonth()
    readonly property int todayYear: new Date().getFullYear()

    // First weekday of the month (0=Sun..6=Sat), shifted to Mon-start (0=Mon..6=Sun)
    readonly property int firstWeekday: {
        const d = new Date(displayYear, displayMonth, 1).getDay()
        return (d + 6) % 7
    }
    readonly property int daysInMonth: new Date(displayYear, displayMonth + 1, 0).getDate()

    implicitHeight: calCol.implicitHeight

    function prevMonth() {
        const d = new Date(displayYear, displayMonth - 1, 1)
        displayDate = d
    }

    function nextMonth() {
        const d = new Date(displayYear, displayMonth + 1, 1)
        displayDate = d
    }

    ColumnLayout {
        id: calCol
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 0

        // ── Month header ──
        Item {
            Layout.fillWidth: true
            height: 40

            Text {
                anchors.centerIn: parent
                text: Qt.formatDate(calendarRoot.displayDate, "MMMM yyyy")
                font.family: cfg ? cfg.fontFamily : "Rubik"
                font.pixelSize: 14
                font.weight: 600
                color: col.onSurface
            }

            // Prev
            Item {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 32; height: 32

                MaterialSymbol {
                    anchors.centerIn: parent
                    icon: "chevron_left"
                    iconSize: 20
                    color: col.onSurfaceVariant
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: calendarRoot.prevMonth()
                }
            }

            // Next
            Item {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 32; height: 32

                MaterialSymbol {
                    anchors.centerIn: parent
                    icon: "chevron_right"
                    iconSize: 20
                    color: col.onSurfaceVariant
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: calendarRoot.nextMonth()
                }
            }
        }

        // ── Day-of-week headers ──
        Row {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8

            Repeater {
                model: ["Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"]

                Text {
                    width: (calCol.width - 16) / 7
                    height: 28
                    text: modelData
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.family: cfg ? cfg.fontFamily : "Rubik"
                    font.pixelSize: 11
                    font.weight: 600
                    color: col.onSurfaceVariant
                    opacity: 0.6
                }
            }
        }

        // ── Day grid ──
        // Total cells = firstWeekday offset + daysInMonth, rounded up to full weeks
        Grid {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            columns: 7

            Repeater {
                model: Math.ceil((calendarRoot.firstWeekday + calendarRoot.daysInMonth) / 7) * 7

                Item {
                    readonly property int dayNum: index - calendarRoot.firstWeekday + 1
                    readonly property bool isValid: dayNum >= 1 && dayNum <= calendarRoot.daysInMonth
                    readonly property bool isToday: isValid
                        && dayNum === calendarRoot.todayDay
                        && calendarRoot.displayMonth === calendarRoot.todayMonth
                        && calendarRoot.displayYear === calendarRoot.todayYear

                    width: (calCol.width - 16) / 7
                    height: width  // square cells

                    Rectangle {
                        anchors.centerIn: parent
                        width: Math.min(parent.width, parent.height) - 4
                        height: width
                        radius: width / 2.25
                        color: isToday ? col.primary : "transparent"
                        visible: isValid
                    }

                    Text {
                        anchors.centerIn: parent
                        text: isValid ? dayNum : ""
                        font.family: cfg ? cfg.fontFamily : "Rubik"
                        font.pixelSize: 13
                        font.weight: isToday ? 700 : 400
                        color: isToday ? col.onPrimary : col.onSurface
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }

        Item { width: 1; height: 8 }
    }
}
