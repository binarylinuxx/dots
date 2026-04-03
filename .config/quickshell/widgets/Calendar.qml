import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
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

    readonly property int firstWeekday: {
        const d = new Date(displayYear, displayMonth, 1).getDay()
        return (d + 6) % 7
    }
    readonly property int daysInMonth: new Date(displayYear, displayMonth + 1, 0).getDate()
    readonly property int daysInPrevMonth: new Date(displayYear, displayMonth, 0).getDate()

    implicitHeight: calCol.implicitHeight

    function prevMonth() {
        displayDate = new Date(displayYear, displayMonth - 1, 1)
    }

    function nextMonth() {
        displayDate = new Date(displayYear, displayMonth + 1, 1)
    }

    function _pad(n) { return String(n).padStart(2, "0") }
    function _dateKey(y, m, d) { return y + "-" + _pad(m + 1) + "-" + _pad(d) }

    // ── Popover state ──
    property string popDate: ""
    property int popDayNum: 0
    property bool popOpen: false
    property real popY: 0             // y position below clicked cell, in calendarRoot coords

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

                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: col.onSurface
                    opacity: prevHover.containsMouse ? 0.10 : 0
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }

                MaterialSymbol {
                    anchors.centerIn: parent
                    icon: "chevron_left"
                    iconSize: 20
                    color: col.onSurfaceVariant
                }

                MouseArea {
                    id: prevHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { calendarRoot.popOpen = false; calendarRoot.prevMonth() }
                }
            }

            // Next
            Item {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 32; height: 32

                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: col.onSurface
                    opacity: nextHover.containsMouse ? 0.10 : 0
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }

                MaterialSymbol {
                    anchors.centerIn: parent
                    icon: "chevron_right"
                    iconSize: 20
                    color: col.onSurfaceVariant
                }

                MouseArea {
                    id: nextHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { calendarRoot.popOpen = false; calendarRoot.nextMonth() }
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
        Grid {
            id: dayGrid
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            columns: 7

            Repeater {
                model: 42

                Item {
                    id: dayCell
                    readonly property int dayNum: index - calendarRoot.firstWeekday + 1
                    readonly property bool isCurrent: dayNum >= 1 && dayNum <= calendarRoot.daysInMonth
                    readonly property bool isPrev: dayNum < 1
                    readonly property bool isNext: dayNum > calendarRoot.daysInMonth
                    readonly property int displayDay: isPrev
                        ? calendarRoot.daysInPrevMonth + dayNum
                        : isNext ? dayNum - calendarRoot.daysInMonth : dayNum
                    readonly property bool isToday: isCurrent
                        && dayNum === calendarRoot.todayDay
                        && calendarRoot.displayMonth === calendarRoot.todayMonth
                        && calendarRoot.displayYear === calendarRoot.todayYear
                    readonly property bool isWeekend: (index % 7) === 4 || (index % 7) === 5
                    readonly property string dateKey: isCurrent
                        ? calendarRoot._dateKey(calendarRoot.displayYear, calendarRoot.displayMonth, dayNum)
                        : ""
                    readonly property var dayReminders: isCurrent
                        ? ReminderService.remindersForDate(dateKey)
                        : []
                    readonly property bool hasReminders: dayReminders.length > 0
                    readonly property bool isSelected: calendarRoot.popOpen && calendarRoot.popDate === dateKey

                    width: (calCol.width - 16) / 7
                    height: width

                    // Hover/selected background
                    Rectangle {
                        anchors.centerIn: parent
                        width: Math.min(parent.width, parent.height) - 4
                        height: width
                        radius: width / 2.25
                        color: isToday ? col.primary
                             : isSelected ? Qt.rgba(col.primary.r, col.primary.g, col.primary.b, 0.18)
                             : dayHover.containsMouse && isCurrent ? Qt.rgba(col.onSurface.r, col.onSurface.g, col.onSurface.b, 0.08)
                             : "transparent"
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    // Day number
                    Text {
                        id: dayText
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: hasReminders ? -4 : 0
                        text: displayDay
                        font.family: cfg ? cfg.fontFamily : "Rubik"
                        font.pixelSize: 13
                        font.weight: isToday ? 700 : 400
                        color: isToday ? col.onPrimary : (isWeekend && isCurrent ? col.primary : col.onSurface)
                        opacity: !isCurrent ? 0.25 : (isWeekend ? 0.75 : 1.0)
                        horizontalAlignment: Text.AlignHCenter

                        Behavior on anchors.verticalCenterOffset { NumberAnimation { duration: 150 } }
                    }

                    // Reminder dot + first event name
                    Item {
                        visible: hasReminders
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 3
                        width: parent.width
                        height: 10

                        Row {
                            anchors.centerIn: parent
                            spacing: 3

                            // Dot
                            Rectangle {
                                width: 4; height: 4
                                radius: 2
                                anchors.verticalCenter: parent.verticalCenter
                                color: isToday ? col.onPrimary : col.primary
                            }

                            // First reminder title truncated
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: dayCell.dayReminders.length > 0 ? dayCell.dayReminders[0].title : ""
                                font.family: cfg ? cfg.fontFamily : "Rubik"
                                font.pixelSize: 7
                                color: isToday ? col.onPrimary : col.primary
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                width: Math.min(implicitWidth, dayCell.width - 10)
                            }
                        }
                    }

                    MouseArea {
                        id: dayHover
                        anchors.fill: parent
                        z: 1
                        hoverEnabled: true
                        cursorShape: isCurrent ? Qt.PointingHandCursor : Qt.ArrowCursor
                        enabled: isCurrent
                        onClicked: {
                            if (calendarRoot.popOpen && calendarRoot.popDate === dateKey) {
                                calendarRoot.popOpen = false
                            } else {
                                calendarRoot.popDate = dateKey
                                calendarRoot.popDayNum = dayNum
                                const cellBottom = dayCell.mapToItem(calendarRoot, 0, dayCell.height)
                                calendarRoot.popY = cellBottom.y + 4
                                calendarRoot.popOpen = true
                            }
                        }
                    }
                }
            }
        }

        Item { width: 1; height: 8 }
    }

    // ── Floating reminder popover ──
    Rectangle {
        id: popContent
        x: 8
        y: calendarRoot.popY
        width: calendarRoot.width - 16
        height: popCol.implicitHeight + 20
        z: 10
        radius: 12
        color: col.surfaceContainerHigh
        visible: calendarRoot.popOpen
        opacity: calendarRoot.popOpen ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 150 } }
        Behavior on y { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

        ColumnLayout {
            id: popCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            spacing: 6

            // Header
            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: calendarRoot.popDate
                    font.family: cfg ? cfg.fontFamily : "Rubik"
                    font.pixelSize: 11
                    font.weight: 600
                    color: col.primary
                }

                Item { Layout.fillWidth: true }

                MaterialSymbol {
                    icon: "close"
                    iconSize: 14
                    color: col.onSurfaceVariant

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: calendarRoot.popOpen = false
                    }
                }
            }

            // Existing reminders list
            Repeater {
                model: calendarRoot.popOpen
                    ? ReminderService.remindersForDate(calendarRoot.popDate)
                    : []

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Rectangle {
                        width: 4; height: 4; radius: 2
                        color: col.primary
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Text {
                        text: modelData.time + "  " + modelData.title
                        font.family: cfg ? cfg.fontFamily : "Rubik"
                        font.pixelSize: 11
                        color: col.onSurface
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    MaterialSymbol {
                        icon: "delete"
                        iconSize: 13
                        color: col.onSurfaceVariant
                        opacity: 0.6

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: ReminderService.removeReminder(modelData.id)
                        }
                    }
                }
            }

            // ── Add reminder row ──
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                property int selHour: 0
                property int selMin: 0
                function timeStr() {
                    return String(selHour).padStart(2,"0") + ":" + String(selMin).padStart(2,"0")
                }

                // ── Hour/Minute spinner ──
                Rectangle {
                    width: 72; height: 58
                    radius: 8
                    color: col.surfaceContainerHighest

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 2

                        // Hour spinner
                        ColumnLayout {
                            spacing: 1

                            MaterialSymbol {
                                Layout.alignment: Qt.AlignHCenter
                                icon: "keyboard_arrow_up"; iconSize: 16
                                color: col.onSurfaceVariant
                                MouseArea {
                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: parent.parent.parent.parent.parent.selHour = (parent.parent.parent.parent.parent.selHour + 1) % 24
                                }
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: String(parent.parent.parent.parent.selHour).padStart(2, "0")
                                font.family: cfg ? cfg.fontFamily : "Rubik"
                                font.pixelSize: 13
                                font.weight: 600
                                color: col.onSurface
                            }

                            MaterialSymbol {
                                Layout.alignment: Qt.AlignHCenter
                                icon: "keyboard_arrow_down"; iconSize: 16
                                color: col.onSurfaceVariant
                                MouseArea {
                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: parent.parent.parent.parent.parent.selHour = (parent.parent.parent.parent.parent.selHour + 23) % 24
                                }
                            }
                        }

                        Text {
                            text: ":"
                            font.family: cfg ? cfg.fontFamily : "Rubik"
                            font.pixelSize: 13; font.weight: 600
                            color: col.onSurfaceVariant
                        }

                        // Minute spinner
                        ColumnLayout {
                            spacing: 1

                            MaterialSymbol {
                                Layout.alignment: Qt.AlignHCenter
                                icon: "keyboard_arrow_up"; iconSize: 16
                                color: col.onSurfaceVariant
                                MouseArea {
                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: parent.parent.parent.parent.parent.selMin = (parent.parent.parent.parent.parent.selMin + 1) % 60
                                }
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: String(parent.parent.parent.parent.selMin).padStart(2, "0")
                                font.family: cfg ? cfg.fontFamily : "Rubik"
                                font.pixelSize: 13
                                font.weight: 600
                                color: col.onSurface
                            }

                            MaterialSymbol {
                                Layout.alignment: Qt.AlignHCenter
                                icon: "keyboard_arrow_down"; iconSize: 16
                                color: col.onSurfaceVariant
                                MouseArea {
                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: parent.parent.parent.parent.parent.selMin = (parent.parent.parent.parent.parent.selMin + 59) % 60
                                }
                            }
                        }
                    }
                }

                // Title field
                Rectangle {
                    Layout.fillWidth: true
                    height: 58
                    radius: 8
                    color: col.surfaceContainerHighest

                    TextInput {
                        id: titleInput
                        anchors.fill: parent
                        anchors.margins: 8
                        font.family: cfg ? cfg.fontFamily : "Rubik"
                        font.pixelSize: 11
                        color: col.onSurface
                        verticalAlignment: TextInput.AlignVCenter
                        selectByMouse: true
                        wrapMode: TextInput.Wrap

                        Text {
                            anchors.fill: parent
                            text: "Reminder…"
                            font: titleInput.font
                            color: col.onSurfaceVariant
                            opacity: 0.5
                            visible: !titleInput.text && !titleInput.activeFocus
                            verticalAlignment: Text.AlignVCenter
                        }

                        Keys.onReturnPressed: addBtn.addReminder()
                    }
                }

                // Add button
                Rectangle {
                    id: addBtn
                    width: 32; height: 58
                    radius: 8
                    color: col.primary

                    function addReminder() {
                        const t = titleInput.text.trim()
                        if (!t) return
                        const timeRow = addBtn.parent
                        ReminderService.addReminder(t, calendarRoot.popDate, timeRow.timeStr())
                        titleInput.text = ""
                        timeRow.selHour = 0
                        timeRow.selMin = 0
                    }

                    MaterialSymbol {
                        anchors.centerIn: parent
                        icon: "add"
                        iconSize: 16
                        color: col.onPrimary
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: addBtn.addReminder()
                    }
                }
            }
        }
    }
}
