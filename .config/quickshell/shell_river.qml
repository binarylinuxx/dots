import Quickshell
import Quickshell.Wayland
import QtQuick
import QtQuick.Layouts
import qs.services
import qs.widgets

ShellRoot {
    id: root

    // Dwm bar: tags | layout | title | status
    PanelWindow {
        id: bar
        
        anchors {
            top: true
            left: true
            right: true
        }
        
        implicitHeight: 12
        color: "#222222"
            
        // Use RowLayout for proper spacing
        RowLayout {
            anchors.fill: parent
            spacing: 0
            
            // 1. TAGS (left)
            Row {
                Layout.alignment: Qt.AlignVCenter
                
                Repeater {
                    model: 9
                    
                    Item {
                        width: 18
                        height: 12
                        
                        property int tagNum: index + 1
                        property int tagBit: 1 << index
                        property bool isFocused: (River.focusedTags & tagBit) !== 0
                        property bool isOccupied: River.viewTags[tagBit] === true
                        
                        Text {
                            x: 2
                            y: 1
                            color: isFocused ? "#eeeeee" : "#bbbbbb"
                            font.family: "monospace"
                            font.pixelSize: 10
                            text: tagNum + ""
                        }
                        
                        Rectangle {
                            x: 2
                            y: 1
                            width: 3
                            height: 3
                            visible: isOccupied
                            color: isFocused ? "#eeeeee" : "transparent"
                            border.width: 1
                            border.color: "#bbbbbb"
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: River.switchToTag(tagBit)
                        }
                    }
                }
            }
            
            // 2. LAYOUT (after tags)
            Text {
                Layout.alignment: Qt.AlignVCenter
                text: River.layout || "[]="
                color: "#bbbbbb"
                font.family: "monospace"
                font.pixelSize: 10
            }
            
            // 3. TITLE (fills middle)
            Text {
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
                text: River.focusedViewTitle || ""
                color: "#eeeeee"
                font.family: "monospace"
                font.pixelSize: 10
                elide: Text.ElideMiddle
                maximumLineCount: 1
            }
            
            // 4. STATUS (right)
            Text {
                Layout.alignment: Qt.AlignVCenter
                text: Object.keys(River.viewTags).length + ""
                color: "#bbbbbb"
                font.family: "monospace"
                font.pixelSize: 10
            }
        }
    }
}
