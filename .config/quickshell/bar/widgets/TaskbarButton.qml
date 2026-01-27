import QtQuick
import qs.widgets

Item {
    id: root
    property bool isOpen: false
    signal clicked()
    
    width: buttonRect.width
    height: 33
    
    Rectangle {
        id: buttonRect
        width: taskbarIcon.width + 10
        height: 28
        anchors.centerIn: parent
        radius: 14
        color: root.isOpen ? col.primary : "transparent"
        
        Behavior on color {
            ColorAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
        
        MaterialSymbol {
            id: taskbarIcon
            anchors.centerIn: parent
            icon: "grid_view"
            color: root.isOpen ? col.onPrimary : col.primary
            iconSize: 20
            
            Behavior on color {
                ColorAnimation { duration: 200; easing.type: Easing.OutCubic }
            }
        }
        
        MouseArea {
            anchors.fill: parent
            onClicked: root.clicked()
        }
    }
}
