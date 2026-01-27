import Quickshell
import Quickshell.Wayland
import QtQuick
import QtQuick.Layouts
import qs.services
import qs.widgets

ShellRoot {
    id: root

    // Simple bar component for each screen
    PanelWindow {
        id: bar
        
        anchors {
            top: true
            left: true
            right: true
        }
        
        height: 40
        color: "#1e1e2e"
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 12
                
                // Workspace buttons
                SwayWorkspaces {
                    Layout.alignment: Qt.AlignVCenter
                }
                
                // Spacer
                Item {
                    Layout.fillWidth: true
                }
                
                // Active window title
                Rectangle {
                    Layout.preferredHeight: 30
                    Layout.preferredWidth: titleText.implicitWidth + 24
                    Layout.maximumWidth: 400
                    
                    visible: Sway.activeTitle !== ""
                    
                    radius: 6
                    color: "#313244"
                    
                    Text {
                        id: titleText
                        anchors.centerIn: parent
                        anchors.margins: 12
                        
                        text: Sway.activeTitle
                        color: "#cdd6f4"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        
                        width: Math.min(implicitWidth, 376)
                    }
                }
                
                // Spacer
                Item {
                    Layout.fillWidth: true
                }
                
                // Workspace info
                Rectangle {
                    Layout.preferredHeight: 30
                    Layout.preferredWidth: wsInfoText.implicitWidth + 24
                    
                    radius: 6
                    color: "#313244"
                    
                    Text {
                        id: wsInfoText
                        anchors.centerIn: parent
                        
                        text: "WS " + Sway.focusedWorkspaceNum + " | " + Sway.currentOutput
                        color: "#89b4fa"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }
                }
                
                // Window count
                Rectangle {
                    Layout.preferredHeight: 30
                    Layout.preferredWidth: windowCountText.implicitWidth + 24
                    
                    radius: 6
                    color: "#313244"
                    
                    Text {
                        id: windowCountText
                        anchors.centerIn: parent
                        
                        property int windowCount: Sway.windows ? Object.keys(Sway.windows).length : 0
                        text: windowCount + " windows"
                        color: "#a6e3a1"
                        font.pixelSize: 12
                    }
                }
            }
        }
    
    // Debug output
    Component.onCompleted: {
        console.log("Shell initialized with Sway service")
        console.log("Focused workspace:", Sway.focusedWorkspaceNum)
        console.log("Current output:", Sway.currentOutput)
        console.log("Total workspaces:", Sway.allWorkspaces.length)
        console.log("Total windows:", Sway.windows ? Object.keys(Sway.windows).length : 0)
    }
}
