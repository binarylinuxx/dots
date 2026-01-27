import QtQuick
import Quickshell
import Quickshell.Hyprland
import qs.widgets

Scope {
    id: taskbarRoot
    property bool isOpen: false
    signal close()
    
    // Sliding sidebar from right
    LazyLoader {
        active: taskbarRoot.isOpen
        
        PopupWindow {
            id: sidebar
            visible: false
            
            color: "transparent"
            
            Rectangle {
                width: 350
                height: sidebar.screen.height
                color: col.background
            
            Column {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 10
                
                Text {
                    text: "Open Windows"
                    font.pixelSize: 18
                    font.weight: 700
                    color: col.primary
                    font.family: cfg ? cfg.fontFamily : "Rubik"
                }
                
                Rectangle {
                    width: parent.width
                    height: 2
                    color: col.outline
                    radius: 1
                }
                
                Flickable {
                    width: parent.width
                    height: parent.height - 40
                    contentHeight: windowsList.height
                    clip: true
                    
                    Column {
                        id: windowsList
                        width: parent.width
                        spacing: 5
                        
                        Repeater {
                            model: Hyprland.workspaces
                            
                            Repeater {
                                model: modelData.toplevels
                                
                                Rectangle {
                                    width: windowsList.width
                                    height: 50
                                    radius: 12
                                    color: modelData.activated ? col.primaryContainer : col.surfaceContainer
                                    
                                    Rectangle {
                                        id: windowHover
                                        anchors.fill: parent
                                        radius: 12
                                        color: col.surfaceContainerHighest
                                        opacity: 0
                                        Behavior on opacity {
                                            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                                        }
                                    }
                                    
                                    Row {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        spacing: 10
                                        
                                        Rectangle {
                                            width: 4
                                            height: parent.height - 16
                                            color: modelData.activated ? col.primary : "transparent"
                                            radius: 2
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        
                                        Column {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: parent.width - 60
                                            spacing: 2
                                            
                                            Text {
                                                text: modelData.title || "Untitled"
                                                color: modelData.activated ? col.onPrimaryContainer : col.foreground
                                                font.pixelSize: 14
                                                font.weight: 500
                                                font.family: cfg ? cfg.fontFamily : "Rubik"
                                                elide: Text.ElideRight
                                                width: parent.width
                                            }
                                            
                                            Text {
                                                text: "Workspace " + (modelData.workspace?.id || "?")
                                                color: col.onSurfaceVariant
                                                font.pixelSize: 11
                                                font.family: cfg ? cfg.fontFamily : "Rubik"
                                            }
                                        }
                                    }
                                    
                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onEntered: windowHover.opacity = 0.3
                                        onExited: windowHover.opacity = 0
                                        onClicked: {
                                            if (modelData.workspace) {
                                                modelData.workspace.activate()
                                            }
                                            taskbarRoot.close()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
            
            HyprlandFocusGrab {
                windows: [sidebar]
                active: sidebar.visible
                
                onCleared: {
                    taskbarRoot.close()
                }
            }
        }
    }
}
