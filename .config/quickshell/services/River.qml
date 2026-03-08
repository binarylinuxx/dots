pragma Singleton
pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Quickshell.Io

Singleton {
    id: root

    property var outputs: ({})
    property var allOutputs: []
    property var seats: ({})
    property var allSeats: []
    property string currentOutput: ""
    property string focusedViewTitle: ""
    property string currentMode: "normal"
    property int focusedTags: 0
    property int urgentTags: 0
    property var viewTags: ({})
    property string layout: ""
    property var windows: ({})

    readonly property bool available: Quickshell.env("WAYLAND_DISPLAY") !== ""
    readonly property string riverFetchPath: "/home/blx/river-fetch/river-fetch"

    Component.onCompleted: {
        console.log("River service initialized")
        
        // Start polling for state updates
        pollTimer.running = true
    }

    Timer {
        id: pollTimer
        interval: 1
        repeat: true
        onTriggered: {
            refreshState()
        }
    }

    Process {
        id: stateProcess
        command: [root.riverFetchPath, "-j"]
        running: false

        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    handleStateUpdate(text)
                } catch (e) {
                    console.warn("River: Failed to parse state:", e)
                }
            }
        }

        onExited: (exitCode, exitStatus) => {
            if (exitCode !== 0) {
                // River not running, ignore
            }
        }
    }

    // Command execution process
    Process {
        id: commandProcess
        running: false

        stdout: SplitParser {
            onRead: line => {
                // riverctl doesn't output JSON, just success/failure
            }
        }

        onExited: (exitCode, exitStatus) => {
            if (exitCode !== 0) {
                console.warn("River command failed with code:", exitCode)
            }
        }
    }

    function refreshState() {
        stateProcess.running = true
    }

    function handleStateUpdate(text) {
        try {
            const state = JSON.parse(text)
            
            // Update outputs
            const newOutputs = {}
            if (state.outputs) {
                for (const out of state.outputs) {
                    newOutputs[out.name] = {
                        name: out.name,
                        focusedTags: out.focused_tags,
                        urgentTags: out.urgent_tags,
                        viewTags: out.view_tags || [],
                        layout: out.layout || ""
                    }
                    
                    // Track focused tags from first output (usually the main one)
                    if (root.focusedTags === 0 || out.focused_tags !== 0) {
                        root.focusedTags = out.focused_tags
                        root.urgentTags = out.urgent_tags
                        root.layout = out.layout || ""
                        
                        // Build view tags map
                        const tags = {}
                        for (const tag of (out.view_tags || [])) {
                            tags[tag] = true
                        }
                        root.viewTags = tags
                    }
                }
            }
            root.outputs = newOutputs
            root.allOutputs = Object.values(newOutputs)
            
            // Update seats
            const newSeats = {}
            if (state.seats) {
                for (const seat of state.seats) {
                    newSeats[seat.name] = {
                        name: seat.name,
                        focusedViewTitle: seat.focused_view_title || "",
                        mode: seat.mode || "normal"
                    }
                    
                    // Track first seat's state
                    root.focusedViewTitle = seat.focused_view_title || ""
                    root.currentMode = seat.mode || "normal"
                }
            }
            root.seats = newSeats
            root.allSeats = Object.values(newSeats)
            
        } catch (e) {
            console.warn("River: JSON parse error:", e, "text:", text)
        }
    }

    // Action methods using riverctl

    function runCommand(command) {
        commandProcess.command = ["riverctl"].concat(command.split(" "))
        commandProcess.running = true
    }

    function close() {
        runCommand("close")
    }

    function exit() {
        runCommand("exit")
    }

    function switchToTag(tag) {
        runCommand("set-focused-tags " + tag)
    }

    function toggleTag(tag) {
        runCommand("toggle-focused-tags " + tag)
    }

    function focusTag(tag) {
        switchToTag(tag)
    }

    function moveToTag(tag) {
        console.log("move to tag not fully implemented")
    }

    function focusOutputNext() {
        runCommand("focus-output next")
    }

    function focusOutputPrevious() {
        runCommand("focus-output previous")
    }

    function focusOutput(outputName) {
        runCommand("focus-output " + outputName)
    }

    function sendToOutput(direction) {
        runCommand("send-to-output " + direction)
    }

    function closeWindow() {
        close()
    }

    function toggleFloating() {
        runCommand("toggle-float")
    }

    function toggleFullscreen() {
        runCommand("toggle-fullscreen")
    }

    function focusViewNext() {
        runCommand("focus-view next")
    }

    function focusViewPrevious() {
        runCommand("focus-view previous")
    }

    function swapViewNext() {
        runCommand("swap next")
    }

    function swapViewPrevious() {
        runCommand("swap previous")
    }

    function moveView(direction, delta) {
        if (delta === undefined) delta = 10
        runCommand("move " + direction + " " + delta)
    }

    function resizeView(direction, delta) {
        if (delta === undefined) delta = 10
        runCommand("resize " + direction + " " + delta)
    }

    function snapView(direction) {
        runCommand("snap " + direction)
    }

    function zoom() {
        runCommand("zoom")
    }

    function spawn(command) {
        runCommand("spawn " + command)
    }

    function setDefaultLayout(namespace) {
        runCommand("default-layout " + namespace)
    }

    function setOutputLayout(namespace) {
        runCommand("output-layout " + namespace)
    }

    function sendLayoutCmd(namespace, command) {
        runCommand("send-layout-cmd " + namespace + " " + command)
    }

    function enterMode(mode) {
        runCommand("enter-mode " + mode)
    }

    function declareMode(name) {
        runCommand("declare-mode " + name)
    }

    // Tag management helpers
    
    function tagBitToNumber(bit) {
        return 1 << bit
    }

    function numberToTagBits(num) {
        const tags = []
        for (let i = 0; i < 32; i++) {
            if (num & (1 << i)) {
                tags.push(i + 1)
            }
        }
        return tags
    }
    
    function isTagFocused(tag) {
        return (root.focusedTags & tagBitToNumber(tag - 1)) !== 0
    }
    
    function isTagOccupied(tag) {
        return root.viewTags[tagBitToNumber(tag - 1)] === true
    }
    
    function isTagUrgent(tag) {
        return (root.urgentTags & tagBitToNumber(tag - 1)) !== 0
    }
}
