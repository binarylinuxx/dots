import QtQuick
import QtQuick.Layouts
import qs.services

Item {
    id: root
    
    implicitWidth: workspaceRow.implicitWidth
    implicitHeight: workspaceRow.implicitHeight
    
    Component.onCompleted: {
        console.log("SwayWorkspaces loaded")
        console.log("Workspaces count:", Sway.allWorkspaces.length)
        console.log("Workspaces:", JSON.stringify(Sway.allWorkspaces))
    }
    
    // Properties for customization
    property int buttonSize: 32
    property int buttonSpacing: 6
    property int buttonRadius: 8
    property int fontSize: 13
    
    // Color properties (defaults to Catppuccin Mocha)
    property color focusedBg: "#89b4fa"
    property color focusedFg: "#1e1e2e"
    property color focusedBorder: "#89dceb"
    
    property color visibleBg: "#45475a"
    property color visibleFg: "#cdd6f4"
    property color visibleBorder: "#6c7086"
    
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
        id: workspaceRow
        anchors.centerIn: parent
        spacing: buttonSpacing
        
        Repeater {
            model: Sway.allWorkspaces
            
            Rectangle {
                id: button
                
                property var workspace: modelData
                property bool isFocused: workspace.focused
                property bool isVisible: workspace.visible
                property bool isUrgent: workspace.urgent
                property bool hasWindows: {
                    if (!Sway.windows) return false
                    var wins = Object.values(Sway.windows)
                    for (var i = 0; i < wins.length; i++) {
                        if (wins[i].workspace === workspace.num) return true
                    }
                    return false
                }
                
                Layout.preferredWidth: buttonSize
                Layout.preferredHeight: buttonSize
                
                radius: buttonRadius
                
                // Background color based on state
                color: {
                    if (isUrgent) return urgentBg
                    if (isFocused) return focusedBg
                    if (isVisible) return visibleBg
                    if (hasWindows) return occupiedBg
                    return emptyBg
                }
                
                // Border
                border.width: 2
                border.color: {
                    if (isUrgent) return urgentBorder
                    if (isFocused) return focusedBorder
                    if (isVisible) return visibleBorder
                    if (hasWindows) return occupiedBorder
                    return emptyBorder
                }
                
                // Workspace number text
                Text {
                    anchors.centerIn: parent
                    text: workspace.num
                    color: {
                        if (isUrgent) return urgentFg
                        if (isFocused) return focusedFg
                        if (isVisible) return visibleFg
                        if (hasWindows) return occupiedFg
                        return emptyFg
                    }
                    font.pixelSize: fontSize
                    font.weight: isFocused ? Font.Bold : Font.Medium
                }
                
                // Indicator dot for occupied workspaces
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 4
                    anchors.horizontalCenter: parent.horizontalCenter
                    
                    width: 4
                    height: 4
                    radius: 2
                    
                    visible: hasWindows && !isFocused
                    color: occupiedFg
                    
                    Behavior on opacity {
                        NumberAnimation { duration: 150 }
                    }
                }
                
                // Click handler
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    
                    onClicked: {
                        console.log("Switching to workspace:", workspace.num)
                        Sway.switchToWorkspace(workspace.num)
                    }
                    
                    onEntered: {
                        button.scale = 1.05
                    }
                    
                    onExited: {
                        button.scale = 1.0
                    }
                }
                
                // Smooth transitions
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
