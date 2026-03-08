import QtQuick
import QtQuick.Layouts
import qs.services

Item {
    id: root
    
    implicitWidth: tagsRow.implicitWidth
    implicitHeight: tagsRow.implicitHeight
    
    // Properties for customization
    property int buttonSize: 32
    property int buttonSpacing: 6
    property int buttonRadius: 8
    property int fontSize: 13
    property int numTags: 9
    
    // Color properties (defaults to Catppuccin Mocha)
    property color focusedBg: "#89b4fa"
    property color focusedFg: "#1e1e2e"
    property color focusedBorder: "#89dceb"
    
    property color occupiedBg: "#313244"
    property color occupiedFg: "#cdd6f4"
    property color occupiedBorder: "#45475a"
    
    property color emptyBg: "#1e1e2e"
    property color emptyFg: "#6c7086"
    property color emptyBorder: "#313244"
    
    property color urgentBg: "#f38ba8"
    property color urgentFg: "#1e1e2e"
    property color urgentBorder: "#eba0ac"
    
    RowLayout {
        id: tagsRow
        anchors.centerIn: parent
        spacing: buttonSpacing
        
        Repeater {
            model: numTags
            
            Rectangle {
                property int tagNum: index + 1
                property int tagBit: 1 << index

                property bool isFocused: (River.focusedTags & tagBit) !== 0
                property bool isOccupied: River.viewTags[tagBit] === true
                property bool isUrgent: (River.urgentTags & tagBit) !== 0
                
                Layout.preferredWidth: buttonSize
                Layout.preferredHeight: buttonSize
                
                radius: buttonRadius
                
                color: {
                    if (isUrgent) return urgentBg
                    if (isFocused) return focusedBg
                    if (isOccupied) return occupiedBg
                    return emptyBg
                }
                
                border.width: 2
                border.color: {
                    if (isUrgent) return urgentBorder
                    if (isFocused) return focusedBorder
                    if (isOccupied) return occupiedBorder
                    return emptyBorder
                }
                
                Text {
                    anchors.centerIn: parent
                    text: tagNum
                    color: {
                        if (isUrgent) return urgentFg
                        if (isFocused) return focusedFg
                        if (isOccupied) return occupiedFg
                        return emptyFg
                    }
                    font.pixelSize: fontSize
                    font.weight: isFocused ? Font.Bold : Font.Medium
                }
                
                // Occupied indicator dot
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 4
                    anchors.horizontalCenter: parent.horizontalCenter
                    
                    width: 4
                    height: 4
                    radius: 2
                    
                    visible: isOccupied && !isFocused
                    color: occupiedFg
                }
                
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    
                    onClicked: {
                        River.switchToTag(tagBit)
                    }
                    
                    onEntered: {
                        parent.scale = 1.05
                    }
                    
                    onExited: {
                        parent.scale = 1.0
                    }
                }
                
                Behavior on color {
                    ColorAnimation { duration: 200; easing.type: Easing.OutCubic }
                }
                
                Behavior on border.color {
                    ColorAnimation { duration: 200; easing.type: Easing.OutCubic }
                }
                
                Behavior on scale {
                    NumberAnimation { duration: 100; easing.type: Easing.OutQuad }
                }
            }
        }
    }
}
