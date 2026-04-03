import QtQuick
import QtQuick.Layouts
import qs.services

// StyledDropdown
// The popup is reparented to the nearest non-clipping ancestor via
// the `popupParent` property — pass the sidebar panel Rectangle so
// it renders above all scroll/clip containers.
//
// Usage:
//   StyledDropdown {
//       model: ["Option A", "Option B"]
//       popupParent: sidebarPanelRect   // pass panel root
//       currentIndex: 0
//       onActivated: index => console.log(model[index])
//   }

Item {
    id: root

    property var model: []
    property int currentIndex: 0
    property string placeholder: "Select..."
    property Item popupParent: parent   // override with sidebar panel
    signal activated(int index)

    readonly property string currentText: model.length > 0 && currentIndex >= 0
        ? model[currentIndex] : placeholder

    implicitWidth: 220
    implicitHeight: 36

    // ── Trigger ──
    Rectangle {
        id: trigger
        anchors.fill: parent
        radius: 12
        color: dropdownMouse.containsMouse
            ? (col.surfaceContainerHigh || "#282a2f")
            : (col.surfaceContainer || "#1e1f25")
        border.color: popupRect.visible
            ? (col.primary || "#adc6ff")
            : (col.outlineVariant || "#44474f")
        border.width: popupRect.visible ? 2 : 1
        Behavior on color { ColorAnimation { duration: Gstate.animDuration } }
        Behavior on border.color { ColorAnimation { duration: Gstate.animDuration } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 8
            spacing: 6

            Text {
                Layout.fillWidth: true
                text: root.currentText
                font.family: cfg ? cfg.fontFamily : "Rubik"
                font.pixelSize: 13
                color: col.onSurface || "#e2e2e9"
                elide: Text.ElideRight
            }

            Text {
                text: "expand_more"
                font.family: "Material Symbols Rounded"
                font.pixelSize: 16
                color: col.onSurfaceVariant || "#8d9199"
                rotation: popupRect.visible ? 180 : 0
                Behavior on rotation {
                    NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
                }
            }
        }

        MouseArea {
            id: dropdownMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (popupRect.visible) {
                    popupRect.visible = false
                } else {
                    // Map trigger bottom-left into popupParent coordinates
                    var pos = root.mapToItem(root.popupParent, 0, root.height + 4)
                    popupRect.x = pos.x
                    popupRect.y = pos.y
                    popupRect.visible = true
                }
            }
        }
    }

    // ── Popup rectangle — lives in popupParent, above all clipping ──
    Rectangle {
        id: popupRect
        parent: root.popupParent
        visible: false
        z: 9999
        width: root.implicitWidth
        height: Math.min(listContent.implicitHeight + 12, 220)
        radius: 14
        color: col.surfaceContainerHigh || "#282a2f"
        border.color: col.outlineVariant || "#44474f"
        border.width: 1
        clip: true

        opacity: visible ? 1.0 : 0.0
        scale: visible ? 1.0 : 0.95
        transformOrigin: Item.Top
        Behavior on opacity { NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic } }
        Behavior on scale   { NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic } }

        Flickable {
            anchors.fill: parent
            anchors.margins: 6
            contentHeight: listContent.implicitHeight
            clip: true

            Column {
                id: listContent
                width: parent.width
                spacing: 2

                Repeater {
                    model: root.model

                    Rectangle {
                        width: listContent.width
                        height: 38
                        radius: 10
                        color: itemMouse.containsMouse
                            ? (col.secondaryContainer || "#2d3142")
                            : (index === root.currentIndex
                                ? Qt.rgba(Qt.color(col.primary || "#adc6ff").r,
                                          Qt.color(col.primary || "#adc6ff").g,
                                          Qt.color(col.primary || "#adc6ff").b, 0.12)
                                : "transparent")
                        Behavior on color { ColorAnimation { duration: 100 } }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 10
                            spacing: 8

                            Text {
                                Layout.fillWidth: true
                                text: modelData
                                font.family: cfg ? cfg.fontFamily : "Rubik"
                                font.pixelSize: 13
                                color: index === root.currentIndex
                                    ? (col.primary || "#adc6ff")
                                    : (col.onSurface || "#e2e2e9")
                                elide: Text.ElideRight
                            }

                            Text {
                                visible: index === root.currentIndex
                                text: "check"
                                font.family: "Material Symbols Rounded"
                                font.pixelSize: 15
                                color: col.primary || "#adc6ff"
                            }
                        }

                        MouseArea {
                            id: itemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.currentIndex = index
                                root.activated(index)
                                popupRect.visible = false
                            }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: Gstate
        function onSidebarOpenChanged() {
            if (!Gstate.sidebarOpen) popupRect.visible = false
        }
    }
}
