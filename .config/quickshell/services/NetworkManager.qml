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

    // ── WiFi scan results ──────────────────────────────────────────────────
    // Each element: { ssid, signal, security, connected }
    property var wifiNetworks: []
    property bool scanning: false
    property string connectingTo: ""
    property string connectError: ""
    property var savedProfiles: []  // list of SSIDs with saved nmcli profiles

    // ── Helpers ────────────────────────────────────────────────────────────
    function parseNetworks(raw) {
        // nmcli terse format: SSID:SIGNAL:SECURITY:IN-USE
        // IN-USE is "*" when active, empty otherwise. Fields may be empty.
        const lines = raw.trim().split('\n')
        const seen = {}
        const nets = []
        for (let i = 0; i < lines.length; i++) {
            const line = lines[i]
            if (!line.trim()) continue
            const parts = line.split(':')
            if (parts.length < 4) continue
            // Last field = IN-USE ("*" or ""), second-to-last = SECURITY, third-to-last = SIGNAL
            const inUse    = parts[parts.length - 1].trim()
            const security = parts[parts.length - 2].trim()
            const signal   = parseInt(parts[parts.length - 3])
            const ssid     = parts.slice(0, parts.length - 3).join(':').trim()
            if (!ssid) continue           // skip hidden networks
            if (seen[ssid]) {
                // keep the entry with highest signal
                if (!isNaN(signal) && signal > seen[ssid].signal)
                    seen[ssid].signal = signal
                continue
            }
            const net = {
                ssid:      ssid,
                signal:    isNaN(signal) ? 0 : signal,
                security:  security,
                connected: inUse === "*"
            }
            seen[ssid] = net
            nets.push(net)
        }
        nets.sort((a, b) => {
            if (a.connected !== b.connected) return a.connected ? -1 : 1
            return b.signal - a.signal
        })
        return nets
    }

    // ── Primary connection polling ─────────────────────────────────────────
    Process {
        id: connectionTypeChecker
        command: ["sh", "-c",
            "nmcli -t -f TYPE,STATE connection show --active | grep -E '^(ethernet|802-3-ethernet|wifi|802-11-wireless):activated' | head -1"]
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
        // Get SSID and signal for the currently connected AP (IN-USE == "*")
        command: ["sh", "-c",
            "nmcli --terse --fields IN-USE,SSID,SIGNAL dev wifi list 2>/dev/null | grep '^\\*'"]
        stdout: StdioCollector {
            onStreamFinished: {
                const output = text.trim()
                if (output) {
                    // format: *:SSID:SIGNAL
                    const parts = output.split(':')
                    if (parts.length >= 3) {
                        // SSID is everything between first and last field
                        const signal = parseInt(parts[parts.length - 1])
                        const ssid   = parts.slice(1, parts.length - 1).join(':').trim()
                        root.wifiSsid = ssid
                        root.wifiSignalStrength = isNaN(signal) ? 0 : signal
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
        onTriggered: connectionTypeChecker.running = true
    }

    // ── Saved profiles ────────────────────────────────────────────────────
    Process {
        id: savedProfilesProc
        command: ["sh", "-c",
            "nmcli -t -f NAME,TYPE connection show | grep '802-11-wireless' | cut -d: -f1"]
        stdout: StdioCollector {
            onStreamFinished: {
                root.savedProfiles = text.trim().split('\n').filter(s => s.length > 0)
            }
        }
    }

    // ── WiFi scan (cached — fast) ──────────────────────────────────────────
    Process {
        id: wifiScanner
        command: ["sh", "-c",
            "nmcli --terse --fields SSID,SIGNAL,SECURITY,IN-USE dev wifi list 2>/dev/null"]
        stdout: StdioCollector {
            onStreamFinished: {
                root.scanning = false
                root.wifiNetworks = root.parseNetworks(text)
            }
        }
    }

    // ── WiFi rescan (forces hardware scan, slower) ─────────────────────────
    Process {
        id: wifiRescanProc
        command: ["sh", "-c",
            "nmcli dev wifi rescan 2>/dev/null; sleep 2; nmcli --terse --fields SSID,SIGNAL,SECURITY,IN-USE dev wifi list 2>/dev/null"]
        stdout: StdioCollector {
            onStreamFinished: {
                root.scanning = false
                root.wifiNetworks = root.parseNetworks(text)
            }
        }
    }

    function scanWifi() {
        if (wifiScanner.running || wifiRescanProc.running) return
        root.scanning = true
        wifiScanner.running = true
    }

    function rescanWifi() {
        if (wifiRescanProc.running) return
        root.scanning = true
        wifiRescanProc.running = true
    }

    // ── Connect / disconnect ───────────────────────────────────────────────
    Process {
        id: wifiConnectProc
        stdout: StdioCollector {
            onStreamFinished: {
                const out = text.trim()
                const ok = out.toLowerCase().includes("success") ||
                           out.toLowerCase().includes("activated")
                if (!ok && out.length > 0) {
                    root.connectError = out.split('\n')[0]
                }
                root.connectingTo = ""
                connectionTypeChecker.running = true
                savedProfilesProc.running = true
                root.scanWifi()
            }
        }
        stderr: StdioCollector {
            onStreamFinished: {
                const err = text.trim()
                if (err && root.connectError === "") {
                    root.connectError = err.split('\n')[0]
                }
            }
        }
    }

    // For open (unsecured) networks or networks with a saved profile.
    // Tries saved profile by SSID name first; falls back to fresh connect (open only).
    function connectTo(ssid) {
        root.connectingTo = ssid
        root.connectError = ""
        wifiConnectProc.command = ["sh", "-c",
            "nmcli connection up id '" + ssid.replace(/'/g, "'\\''") + "' 2>&1 || " +
            "nmcli device wifi connect '" + ssid.replace(/'/g, "'\\''") + "' 2>&1 || " +
            "echo 'No saved profile found. Please enter the password.'"]
        wifiConnectProc.running = true
    }

    function connectWithPassword(ssid, password) {
        root.connectingTo = ssid
        root.connectError = ""
        // nmcli infers key-mgmt automatically from the AP's capabilities;
        // just provide ssid + password and let nmcli handle the rest.
        wifiConnectProc.command = ["sh", "-c",
            "nmcli device wifi connect '" + ssid.replace(/'/g, "'\\''") + "'" +
            " password '" + password.replace(/'/g, "'\\''") + "' 2>&1"]
        wifiConnectProc.running = true
    }

    function disconnectWifi() {
        root.connectError = ""
        wifiConnectProc.command = ["sh", "-c",
            "nmcli device disconnect " +
            "$(nmcli -t -f DEVICE,TYPE device status | grep ':wifi' | cut -d: -f1 | head -1) 2>&1"]
        wifiConnectProc.running = true
    }

    Component.onCompleted: {
        console.log("NetworkManager service initialized")
        connectionTypeChecker.running = true
        savedProfilesProc.running = true
    }
}
