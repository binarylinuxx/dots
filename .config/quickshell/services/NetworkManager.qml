pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io

Singleton {
    id: root

    property string primaryConnectionType: "unknown" // "ethernet", "wifi", "none", "unknown"
    property int wifiSignalStrength: 0
    property string wifiSsid: ""
    property string connectionStatus: "disconnected" // "connected", "connecting", "disconnected"

    Process {
        id: connectionTypeChecker
        command: ["sh", "-c", "nmcli -t -f TYPE,STATE connection show --active | grep -E '^(ethernet|802-3-ethernet|wifi|802-11-wireless):activated' | head -1"]
        stdout: StdioCollector {
            onStreamFinished: {
                const output = text.trim()
                
                if (output.includes("ethernet") || output.includes("802-3-ethernet")) {
                    root.primaryConnectionType = "ethernet"
                    root.wifiSignalStrength = 0
                    root.wifiSsid = ""
                    root.connectionStatus = "connected"
                } else if (output.includes("wifi") || output.includes("802-11-wireless")) {
                    root.primaryConnectionType = "wifi"
                    root.connectionStatus = "connected"
                    wifiInfoChecker.running = true
                } else {
                    root.primaryConnectionType = "none"
                    root.wifiSignalStrength = 0
                    root.wifiSsid = ""
                    root.connectionStatus = "disconnected"
                }
            }
        }
    }

    Process {
        id: wifiInfoChecker
        command: ["sh", "-c", "nmcli -t -f name,device,SIGNAL connection show --active | grep -E '^.*:.*:[0-9]+$'"]
        stdout: StdioCollector {
            onStreamFinished: {
                const output = text.trim()
                if (output) {
                    const parts = output.split(':')
                    if (parts.length >= 3) {
                        root.wifiSsid = parts[0]
                        root.wifiSignalStrength = parseInt(parts[2])
                    }
                }
            }
        }
    }

    Timer {
        interval: 5000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            connectionTypeChecker.running = true
        }
    }

    Component.onCompleted: {
        console.log("NetworkManager service initialized")
        connectionTypeChecker.running = true
    }
}
