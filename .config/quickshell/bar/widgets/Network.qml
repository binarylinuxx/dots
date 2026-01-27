import QtQuick
import Quickshell
import qs.services
import qs.widgets

Item {
    property bool taskbarOpen: false
    width: networkContainer.width

    // Load the nerd font needed for network icons
    FontLoader {
        id: nerdFont
        source: Qt.resolvedUrl("../../fonts/FiraCodeNerdFont-Regular.ttf")
    }

    Rectangle {
        id: networkContainer
        width: networkIcon.width + (taskbarOpen ? 20 : 10)
        height: 28
        anchors.centerIn: parent
        radius: 14
        color: /*taskbarOpen ? col.primary :*/ "transparent"
        
        Behavior on color {
            ColorAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
        
        Behavior on width {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        MaterialSymbol {
            id: networkIcon
            anchors.centerIn: parent
            color: /*taskbarOpen ? col.onPrimary :*/ col.primary
            iconSize: 20
            
            Behavior on color {
                ColorAnimation { duration: 200; easing.type: Easing.OutCubic }
            }
        
            icon: {
                if (NetworkManager.primaryConnectionType === "ethernet") {
                    return "Lan"                  // or "settings_ethernet"
                } else if (NetworkManager.primaryConnectionType === "wifi") {
                    const strength = NetworkManager.wifiSignalStrength
                    if (strength > 75) {
                        return "signal_wifi_4_bar"
                    } else if (strength > 50) {
                        return "signal_wifi_3_bar"
                    } else if (strength > 25) {
                        return "signal_wifi_2_bar"
                    } else {
                        return "signal_wifi_1_bar"
                    }
                } else {
                    return "ethernet"
                }
            }
        }
    }
}

