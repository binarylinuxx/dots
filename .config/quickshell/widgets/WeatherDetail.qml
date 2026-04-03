import Quickshell
import Quickshell.Wayland
import Quickshell.Hyprland
import QtQuick
import QtQuick.Layouts
import qs.services

PanelWindow {
    id: weatherDetailWindow

    anchors.left: true
    anchors.top: true
    exclusiveZone: 0
    color: "transparent"

    implicitWidth:  Gstate.weatherDetailOpen ? panel.width  : 1
    implicitHeight: Gstate.weatherDetailOpen ? panel.height : 1

    // Slide-in from left
    property real slideX: Gstate.weatherDetailOpen ? 0 : -(panel.width + 24)
    Behavior on slideX {
        NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
    }

    property real panelOpacity: Gstate.weatherDetailOpen ? 1.0 : 0.0
    Behavior on panelOpacity {
        NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
    }

    HyprlandFocusGrab {
        windows: [weatherDetailWindow]
        active: Gstate.weatherDetailOpen
        onCleared: Gstate.weatherDetailOpen = false
    }

    Rectangle {
        id: panel
        x: weatherDetailWindow.slideX
        y: 0
        // width driven by content column; height driven by children
        width: 520
        height: content.implicitHeight + 40  // 40 = top+bottom margins
        opacity: weatherDetailWindow.panelOpacity
        color: col?.surfaceContainer || "#1f2019"
        radius: 20

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.width: 1
            border.color: col?.outlineVariant || "#393b30"
            radius: parent.radius
        }

        ColumnLayout {
            id: content
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                margins: 20
            }
            spacing: 0

            // ── Header ───────────────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                // Icon + temp + condition
                RowLayout {
                    spacing: 12

                    MaterialSymbol {
                        icon: backgroundGrid.weatherIcon
                        iconSize: 64
                        color: col?.primary || "#97ab62"
                        Layout.alignment: Qt.AlignVCenter
                    }

                    ColumnLayout {
                        spacing: 2
                        Layout.alignment: Qt.AlignVCenter

                        Text {
                            text: backgroundGrid.weatherTemp
                            color: col?.onSurface || "#cbcbc0"
                            font.pixelSize: 48
                            font.weight: Font.Light
                            font.family: cfg?.fontFamily || "Rubik"
                        }
                        Text {
                            text: backgroundGrid.weatherCondition
                            color: col?.onSurfaceVariant || "#a2a495"
                            font.pixelSize: 14
                            font.family: cfg?.fontFamily || "Rubik"
                            font.capitalization: Font.Capitalize
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // City + pills
                ColumnLayout {
                    spacing: 6
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight

                    Text {
                        Layout.alignment: Qt.AlignRight
                        text: backgroundGrid.weatherCity
                        color: col?.outline || "#909284"
                        font.pixelSize: 13
                        font.family: cfg?.fontFamily || "Rubik"
                    }

                    RowLayout {
                        spacing: 8
                        Layout.alignment: Qt.AlignRight

                        // Humidity pill
                        Rectangle {
                            implicitWidth: humRow.implicitWidth + 16
                            implicitHeight: 28
                            radius: 14
                            color: col?.surfaceContainerHigh || "#292b23"
                            RowLayout {
                                id: humRow
                                anchors.centerIn: parent
                                spacing: 4
                                MaterialSymbol {
                                    icon: "water_drop"
                                    iconSize: 14
                                    color: col?.tertiary || "#7eaca3"
                                }
                                Text {
                                    text: backgroundGrid.weatherHumidity
                                    color: col?.onSurfaceVariant || "#a2a495"
                                    font.pixelSize: 12
                                    font.family: cfg?.fontFamily || "Rubik"
                                }
                            }
                        }

                        // Wind pill
                        Rectangle {
                            implicitWidth: windRow.implicitWidth + 16
                            implicitHeight: 28
                            radius: 14
                            color: col?.surfaceContainerHigh || "#292b23"
                            RowLayout {
                                id: windRow
                                anchors.centerIn: parent
                                spacing: 4
                                MaterialSymbol {
                                    icon: "air"
                                    iconSize: 14
                                    color: col?.tertiary || "#7eaca3"
                                }
                                Text {
                                    text: backgroundGrid.weatherWind
                                    color: col?.onSurfaceVariant || "#a2a495"
                                    font.pixelSize: 12
                                    font.family: cfg?.fontFamily || "Rubik"
                                }
                            }
                        }
                    }
                }
            }

            // ── Divider ───────────────────────────────────────────────────────
            Item { Layout.preferredHeight: 14 }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: col?.outlineVariant || "#393b30"
                opacity: 0.6
            }

            Item { Layout.preferredHeight: 14 }

            // ── Section label ─────────────────────────────────────────────────
            Text {
                text: "7-Day Forecast"
                color: col?.outline || "#909284"
                font.pixelSize: 11
                font.weight: Font.Medium
                font.family: cfg?.fontFamily || "Rubik"
                font.letterSpacing: 0.8
                opacity: 0.9
            }

            Item { Layout.preferredHeight: 8 }

            // ── Loading state ─────────────────────────────────────────────────
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10
                visible: !backgroundGrid.forecastLoaded

                Item { Layout.preferredHeight: 16 }

                MaterialSymbol {
                    Layout.alignment: Qt.AlignHCenter
                    icon: "cloud_sync"
                    iconSize: 32
                    color: col?.onSurfaceVariant || "#a2a495"
                    opacity: 0.4
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Loading forecast..."
                    color: col?.onSurfaceVariant || "#a2a495"
                    font.pixelSize: 13
                    font.family: cfg?.fontFamily || "Rubik"
                    opacity: 0.5
                }

                Item { Layout.preferredHeight: 16 }
            }

            // ── Forecast rows ─────────────────────────────────────────────────
            Repeater {
                model: backgroundGrid.forecastLoaded ? backgroundGrid.forecastDays : []

                delegate: Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 52
                    color: rowMouse.containsMouse
                        ? (col?.surfaceContainerHigh || "#292b23")
                        : "transparent"
                    radius: 12

                    Behavior on color { ColorAnimation { duration: 120 } }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 0

                        // Day name
                        Text {
                            Layout.preferredWidth: 60
                            text: modelData.dayName
                            color: modelData.dayName === "Today"
                                ? (col?.primary || "#97ab62")
                                : (col?.onSurface || "#cbcbc0")
                            font.pixelSize: 13
                            font.weight: modelData.dayName === "Today" ? Font.Medium : Font.Normal
                            font.family: cfg?.fontFamily || "Rubik"
                            verticalAlignment: Text.AlignVCenter
                        }

                        // Icon
                        MaterialSymbol {
                            Layout.preferredWidth: 32
                            icon: modelData.icon
                            iconSize: 22
                            color: col?.primary || "#97ab62"
                        }

                        // Condition
                        Text {
                            Layout.fillWidth: true
                            text: modelData.condition
                            color: col?.onSurfaceVariant || "#a2a495"
                            font.pixelSize: 12
                            font.family: cfg?.fontFamily || "Rubik"
                            elide: Text.ElideRight
                            font.capitalization: Font.Capitalize
                            verticalAlignment: Text.AlignVCenter
                        }

                        // Humidity
                        RowLayout {
                            Layout.preferredWidth: 54
                            spacing: 3
                            MaterialSymbol {
                                icon: "water_drop"
                                iconSize: 12
                                color: col?.tertiary || "#7eaca3"
                                opacity: 0.8
                            }
                            Text {
                                text: modelData.humidity
                                color: col?.onSurfaceVariant || "#a2a495"
                                font.pixelSize: 11
                                font.family: cfg?.fontFamily || "Rubik"
                                opacity: 0.8
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        // Low temp
                        Text {
                            Layout.preferredWidth: 36
                            text: modelData.tempLow
                            color: col?.onSurfaceVariant || "#a2a495"
                            font.pixelSize: 13
                            font.family: cfg?.fontFamily || "Rubik"
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: Text.AlignVCenter
                            opacity: 0.7
                        }

                        // Temperature range bar
                        Item {
                            Layout.preferredWidth: 60
                            Layout.leftMargin: 4
                            Layout.rightMargin: 4
                            implicitHeight: 4

                            property real allMin: {
                                if (!backgroundGrid.forecastDays || backgroundGrid.forecastDays.length === 0) return 0
                                var m = 999
                                for (var i = 0; i < backgroundGrid.forecastDays.length; i++) {
                                    var v = parseFloat(backgroundGrid.forecastDays[i].tempLow)
                                    if (!isNaN(v) && v < m) m = v
                                }
                                return m
                            }
                            property real allMax: {
                                if (!backgroundGrid.forecastDays || backgroundGrid.forecastDays.length === 0) return 1
                                var mx = -999
                                for (var i = 0; i < backgroundGrid.forecastDays.length; i++) {
                                    var v2 = parseFloat(backgroundGrid.forecastDays[i].tempHigh)
                                    if (!isNaN(v2) && v2 > mx) mx = v2
                                }
                                return mx
                            }
                            property real rangeSpan: Math.max(allMax - allMin, 1)
                            property real barStart: (parseFloat(modelData.tempLow)  - allMin) / rangeSpan
                            property real barEnd:   (parseFloat(modelData.tempHigh) - allMin) / rangeSpan

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width
                                height: 4
                                radius: 2
                                color: col?.outlineVariant || "#393b30"

                                Rectangle {
                                    x: parent.parent.barStart * parent.width
                                    width: Math.max((parent.parent.barEnd - parent.parent.barStart) * parent.width, 6)
                                    height: parent.height
                                    radius: parent.radius
                                    color: col?.primary || "#97ab62"
                                    opacity: 0.85
                                }
                            }
                        }

                        // High temp
                        Text {
                            Layout.preferredWidth: 36
                            text: modelData.tempHigh
                            color: col?.onSurface || "#cbcbc0"
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            font.family: cfg?.fontFamily || "Rubik"
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            // ── Footer ────────────────────────────────────────────────────────
            Item { Layout.preferredHeight: 10 }

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: closeRow.implicitWidth + 24
                implicitHeight: 32
                radius: 16
                color: closeMouse.containsMouse
                    ? (col?.surfaceContainerHigh || "#292b23")
                    : "transparent"
                border.width: 1
                border.color: closeMouse.containsMouse
                    ? (col?.outlineVariant || "#393b30")
                    : "transparent"

                Behavior on color { ColorAnimation { duration: 120 } }

                RowLayout {
                    id: closeRow
                    anchors.centerIn: parent
                    spacing: 6
                    MaterialSymbol {
                        icon: "close"
                        iconSize: 14
                        color: col?.onSurfaceVariant || "#a2a495"
                    }
                    Text {
                        text: "Close"
                        color: col?.onSurfaceVariant || "#a2a495"
                        font.pixelSize: 12
                        font.family: cfg?.fontFamily || "Rubik"
                    }
                }

                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Gstate.weatherDetailOpen = false
                }
            }

            Item { Layout.preferredHeight: 4 }
        }
    }
}
