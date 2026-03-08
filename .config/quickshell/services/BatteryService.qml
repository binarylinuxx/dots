pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io

Singleton {
    id: root

    property int    percentage: 0       // 0–100
    property bool   charging:   false
    property bool   present:    false

    Process {
        id: batteryProc
        command: ["sh", "-c",
            "upower -i $(upower -e | grep BAT | head -1) 2>/dev/null | grep -E 'percentage|state|present'"]
        stdout: StdioCollector {
            onStreamFinished: {
                const lines = text.trim().split('\n')
                for (let i = 0; i < lines.length; i++) {
                    const line = lines[i].trim()
                    if (line.startsWith("percentage:")) {
                        const val = parseInt(line.replace("percentage:", "").trim())
                        if (!isNaN(val)) root.percentage = val
                    } else if (line.startsWith("state:")) {
                        const state = line.replace("state:", "").trim()
                        root.charging = (state === "charging")
                    } else if (line.startsWith("present:")) {
                        root.present = line.includes("yes")
                    }
                }
            }
        }
    }

    Timer {
        interval: 30000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: batteryProc.running = true
    }
}
