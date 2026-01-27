pragma Singleton
pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Quickshell.Io

Singleton {
    id: root

    property var workspaces: ({})
    property var windows: ({})
    property var allWorkspaces: []
    property int focusedWorkspaceIndex: 0
    property int focusedWorkspaceNum: 1
    property var currentOutputWorkspaces: []
    property string currentOutput: ""
    property string activeTitle: ""
    property var outputs: ({})

    readonly property string socketPath: Quickshell.env("SWAYSOCK")

    Component.onCompleted: {
        console.log("Sway service initialized")
        console.log("Socket path:", root.socketPath)
        
        // Start event subscriber
        eventSubscriber.running = true
        
        // Initial state fetch
        workspacesProcess.running = true
        outputsProcess.running = true
        treeProcess.running = true
    }

    // Event subscription process
    Process {
        id: eventSubscriber
        command: ["swaymsg", "-t", "subscribe", "-m", '["workspace","window","output"]']
        running: false

        stdout: SplitParser {
            onRead: line => {
                try {
                    const event = JSON.parse(line)
                    handleSwayEvent(event)
                } catch (e) {
                    // Ignore parse errors for partial lines
                }
            }
        }

        onExited: (exitCode, exitStatus) => {
            console.warn("Sway event subscriber exited:", exitCode)
            reconnectTimer.start()
        }
    }

    Timer {
        id: reconnectTimer
        interval: 2000
        onTriggered: eventSubscriber.running = true
    }

    // Workspaces fetch process
    Process {
        id: workspacesProcess
        command: ["swaymsg", "-t", "get_workspaces", "-r"]
        running: false

        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    const workspaceList = JSON.parse(text)
                    handleWorkspacesUpdate(workspaceList)
                } catch (e) {
                    console.warn("Sway: Failed to parse workspaces:", e)
                }
            }
        }
    }

    // Outputs fetch process
    Process {
        id: outputsProcess
        command: ["swaymsg", "-t", "get_outputs", "-r"]
        running: false

        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    const outputList = JSON.parse(text)
                    handleOutputsUpdate(outputList)
                } catch (e) {
                    console.warn("Sway: Failed to parse outputs:", e)
                }
            }
        }
    }

    // Tree fetch process
    Process {
        id: treeProcess
        command: ["swaymsg", "-t", "get_tree", "-r"]
        running: false

        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    const tree = JSON.parse(text)
                    handleTreeUpdate(tree)
                } catch (e) {
                    console.warn("Sway: Failed to parse tree:", e)
                }
            }
        }
    }

    // Command execution process
    Process {
        id: commandProcess
        running: false

        stdout: SplitParser {
            onRead: line => {
                try {
                    const response = JSON.parse(line)
                    if (response[0] && !response[0].success) {
                        console.warn("Sway command failed:", response)
                    }
                } catch (e) {
                    // Ignore parse errors
                }
            }
        }
    }

    function handleSwayEvent(event) {
        if (!event || !event.change) {
            return
        }

        const eventType = event.change

        switch (eventType) {
            case "init":
            case "empty":
            case "focus":
            case "move":
            case "rename":
            case "urgent":
            case "reload":
                workspacesProcess.running = true
                break
            case "new":
            case "close":
            case "title":
            case "fullscreen_mode":
            case "floating":
                treeProcess.running = true
                break
        }

        // Handle window focus changes
        if (event.container && event.container.focused) {
            root.activeTitle = event.container.name || ""
        }
    }

    function handleWorkspacesUpdate(workspaceList) {
        const newWorkspaces = {}
        let focusedWs = null

        for (const ws of workspaceList) {
            newWorkspaces[ws.num] = {
                num: ws.num,
                name: ws.name,
                visible: ws.visible,
                focused: ws.focused,
                urgent: ws.urgent,
                output: ws.output,
                representation: ws.representation || null
            }

            if (ws.focused) {
                focusedWs = newWorkspaces[ws.num]
            }
        }

        root.workspaces = newWorkspaces
        root.allWorkspaces = Object.values(newWorkspaces).sort((a, b) => a.num - b.num)

        if (focusedWs) {
            root.focusedWorkspaceNum = focusedWs.num
            root.focusedWorkspaceIndex = root.allWorkspaces.findIndex(w => w.num === focusedWs.num)
            root.currentOutput = focusedWs.output || ""
        } else {
            root.focusedWorkspaceIndex = 0
            root.focusedWorkspaceNum = 1
        }

        updateCurrentOutputWorkspaces()
    }

    function handleOutputsUpdate(outputList) {
        const newOutputs = {}

        for (const output of outputList) {
            if (output.active) {
                newOutputs[output.name] = {
                    name: output.name,
                    make: output.make,
                    model: output.model,
                    serial: output.serial,
                    active: output.active,
                    primary: output.primary || false,
                    scale: output.scale,
                    transform: output.transform,
                    current_workspace: output.current_workspace,
                    rect: output.rect,
                    focused: output.focused
                }

                if (output.focused) {
                    root.currentOutput = output.name
                }
            }
        }

        root.outputs = newOutputs
    }

    function handleTreeUpdate(tree) {
        const newWindows = {}
        let focusedWindow = null

        function traverseNode(node) {
            if (node.type === "con" && node.pid) {
                newWindows[node.id] = {
                    id: node.id,
                    name: node.name || "",
                    app_id: node.app_id || (node.window_properties ? node.window_properties.class : "") || "",
                    pid: node.pid,
                    focused: node.focused,
                    visible: node.visible,
                    urgent: node.urgent,
                    workspace: node.workspace || null,
                    rect: node.rect,
                    window_rect: node.window_rect,
                    floating: node.type === "floating_con",
                    fullscreen_mode: node.fullscreen_mode
                }

                if (node.focused) {
                    focusedWindow = newWindows[node.id]
                }
            }

            if (node.nodes && node.nodes.length > 0) {
                for (const child of node.nodes) {
                    traverseNode(child)
                }
            }

            if (node.floating_nodes && node.floating_nodes.length > 0) {
                for (const child of node.floating_nodes) {
                    traverseNode(child)
                }
            }
        }

        traverseNode(tree)

        root.windows = newWindows

        if (focusedWindow) {
            root.activeTitle = focusedWindow.name
        } else {
            root.activeTitle = ""
        }
    }

    function updateCurrentOutputWorkspaces() {
        if (!currentOutput) {
            currentOutputWorkspaces = allWorkspaces
            return
        }

        const outputWs = allWorkspaces.filter(w => w.output === currentOutput)
        currentOutputWorkspaces = outputWs
    }

    // Action methods

    function runCommand(command) {
        commandProcess.command = ["swaymsg", command]
        commandProcess.running = true
    }

    function switchToWorkspace(workspaceNum) {
        runCommand("workspace number " + workspaceNum)
    }

    function focusWorkspaceByNum(workspaceNum) {
        switchToWorkspace(workspaceNum)
    }

    function moveToWorkspace(workspaceNum) {
        runCommand("move container to workspace number " + workspaceNum)
    }

    function focusOutput(outputName) {
        runCommand("focus output " + outputName)
    }

    function moveWorkspaceToOutput(outputName) {
        runCommand("move workspace to output " + outputName)
    }

    function closeWindow() {
        runCommand("kill")
    }

    function toggleFloating() {
        runCommand("floating toggle")
    }

    function toggleFullscreen() {
        runCommand("fullscreen toggle")
    }

    function focusWindow(windowId) {
        runCommand('[con_id="' + windowId + '"] focus')
    }

    function moveWindow(direction) {
        runCommand("move " + direction)
    }

    function resizeWindow(direction, amount) {
        if (amount === undefined) amount = 10
        runCommand("resize " + direction + " " + amount + " px or " + amount + " ppt")
    }

    function splitHorizontal() {
        runCommand("split h")
    }

    function splitVertical() {
        runCommand("split v")
    }

    function setLayout(layout) {
        runCommand("layout " + layout)
    }

    function reload() {
        runCommand("reload")
    }
}
