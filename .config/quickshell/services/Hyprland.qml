pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io

Singleton {
    id: root

    readonly property string runtimeDir: Quickshell.env("XDG_RUNTIME_DIR") || "/run/user/" + (Quickshell.env("UID") || "1000")
    readonly property string instanceSignature: Quickshell.env("HYPRLAND_INSTANCE_SIGNATURE") || ""
    readonly property string socketDir: instanceSignature ? runtimeDir + "/hypr/" + instanceSignature : ""
    readonly property string eventSocketPath: socketDir ? socketDir + "/.socket2.sock" : ""
    readonly property string commandSocketPath: socketDir ? socketDir + "/.socket.sock" : ""

    property bool ready: false
    property int activeWorkspaceId: 1
    property string activeWindowAddress: ""
    property var workspaces: []
    property var clients: []
    property var monitors: []
    property var focusedMonitor: ({ activeWorkspace: { id: activeWorkspaceId } })

    property var _requestQueue: []
    property string _activeRequest: ""
    property string _activeKind: ""
    property string _responseBuffer: ""
    property bool _requestSent: false

    signal stateChanged()

    Component.onCompleted: {
        if (!eventSocketPath || !commandSocketPath) {
            console.warn("HyprlandService: missing Hyprland IPC environment")
            return
        }

        eventSocket.connected = true
        refresh()
    }

    onActiveWorkspaceIdChanged: focusedMonitor = ({ activeWorkspace: { id: activeWorkspaceId } })
    onActiveWindowAddressChanged: updateClientActivation()

    function refresh() {
        refreshTimer.restart()
    }

    function dispatch(command) {
        const ipcCommand = mapLegacyDispatch(command)
        if (ipcCommand)
            request("dispatch", ipcCommand)
    }

    function quoteLuaString(value) {
        return "\"" + String(value).replace(/\\/g, "\\\\").replace(/\"/g, "\\\"") + "\""
    }

    function mapLegacyDispatch(command) {
        const text = String(command).trim()

        if (text.indexOf("workspace ") === 0) {
            const target = text.slice(10).trim()
            const parsed = parseInt(target)
            const value = !isNaN(parsed) && String(parsed) === target ? parsed : quoteLuaString(target)
            return "dispatch hl.dsp.focus({ workspace = " + value + " })"
        }

        if (text.indexOf("focuswindow ") === 0) {
            const target = text.slice(12).trim()
            return "dispatch hl.dsp.focus({ window = " + quoteLuaString(target) + " })"
        }

        console.warn("HyprlandService: unsupported dispatch", text)
        return ""
    }

    function request(kind, command) {
        _requestQueue.push({ kind: kind, command: command })
        startNextRequest()
    }

    function startNextRequest() {
        if (_activeRequest || commandSocket.connected || _requestQueue.length === 0)
            return

        const next = _requestQueue.shift()
        _activeKind = next.kind
        _activeRequest = next.command
        _responseBuffer = ""
        _requestSent = false
        commandSocket.connected = true
    }

    function finishRequest() {
        if (!_activeRequest)
            return

        const kind = _activeKind
        const text = _responseBuffer.trim()
        _activeKind = ""
        _activeRequest = ""
        _responseBuffer = ""
        _requestSent = false

        if (kind === "state")
            applyState(text)
        else if (kind === "monitors")
            applyMonitors(text)
        else if (kind === "workspaces")
            applyWorkspaces(text)
        else if (kind === "clients")
            applyClients(text)

        startNextRequest()
    }

    function applyState(text) {
        const sections = text.split(/\n\s*\n/).filter(section => section.trim().length > 0)
        if (sections.length < 3) {
            console.warn("HyprlandService: incomplete batch response")
            return
        }

        applyMonitors(sections[0])
        applyWorkspaces(sections[1])
        applyClients(sections.slice(2).join("\n\n"))
    }

    function applyMonitors(text) {
        try {
            const data = JSON.parse(text)
            monitors = data
            for (let i = 0; i < data.length; ++i) {
                if (data[i].focused && data[i].activeWorkspace) {
                    activeWorkspaceId = data[i].activeWorkspace.id || activeWorkspaceId
                    break
                }
            }
            ready = true
            stateChanged()
        } catch (e) {
            console.warn("HyprlandService: failed to parse monitors", e)
        }
    }

    function applyWorkspaces(text) {
        try {
            const data = JSON.parse(text)
            data.sort((a, b) => a.id - b.id)
            workspaces = data
            ready = true
            stateChanged()
        } catch (e) {
            console.warn("HyprlandService: failed to parse workspaces", e)
        }
    }

    function applyClients(text) {
        try {
            const data = JSON.parse(text)
            clients = data.map(client => ({
                address: client.address || "",
                title: client.title || "Untitled",
                className: client.class || "",
                workspace: client.workspace || { id: 0, name: "" },
                activated: activeWindowAddress !== "" && client.address === activeWindowAddress
            }))
            ready = true
            stateChanged()
        } catch (e) {
            console.warn("HyprlandService: failed to parse clients", e)
        }
    }

    function updateClientActivation() {
        clients = clients.map(client => ({
            address: client.address,
            title: client.title,
            className: client.className,
            workspace: client.workspace,
            activated: activeWindowAddress !== "" && client.address === activeWindowAddress
        }))
        stateChanged()
    }

    function workspaceHasWindows(workspaceId) {
        for (let i = 0; i < workspaces.length; ++i) {
            const ws = workspaces[i]
            if (ws.id === workspaceId)
                return (ws.windows || 0) > 0
        }
        return false
    }

    function activateClient(address) {
        if (address)
            dispatch("focuswindow address:" + address)
    }

    function handleEvent(line) {
        if (!line)
            return

        const parts = line.split(">>")
        const event = parts[0]
        const payload = parts.length > 1 ? parts.slice(1).join(">>") : ""

        switch (event) {
            case "workspace":
            case "workspacev2": {
                const ws = event === "workspacev2" ? payload.split(",")[0] : payload
                const parsed = parseInt(ws)
                if (!isNaN(parsed))
                    activeWorkspaceId = parsed
                break
            }
            case "focusedmon": {
                const fields = payload.split(",")
                const parsed = parseInt(fields[1])
                if (!isNaN(parsed))
                    activeWorkspaceId = parsed
                break
            }
            case "activewindowv2":
                activeWindowAddress = payload
                break
            case "activewindow":
            case "openwindow":
            case "closewindow":
            case "movewindow":
            case "movewindowv2":
            case "changefloatingmode":
            case "fullscreen":
            case "windowtitle":
            case "windowtitlev2":
                refreshTimer.restart()
                break
        }

        stateChanged()
    }

    Socket {
        id: eventSocket
        path: root.eventSocketPath

        parser: SplitParser {
            onRead: line => root.handleEvent(line.trim())
        }

        onError: error => {
            console.warn("HyprlandService: event socket error", error)
            reconnectTimer.restart()
        }

        onConnectedChanged: {
            if (!connected && root.eventSocketPath)
                reconnectTimer.restart()
        }
    }

    Socket {
        id: commandSocket
        path: root.commandSocketPath

        parser: SplitParser {
            splitMarker: ""
            onRead: chunk => root._responseBuffer += chunk
        }

        onConnectedChanged: {
            if (connected && root._activeRequest) {
                write(root._activeRequest)
                flush()
                root._requestSent = true
            } else if (!connected) {
                root.finishRequest()
            }
        }

        onError: error => {
            if (root._requestSent) {
                root.finishRequest()
                return
            }

            console.warn("HyprlandService: command socket error", error)
            root._activeKind = ""
            root._activeRequest = ""
            root._responseBuffer = ""
            root._requestSent = false
            root.startNextRequest()
        }
    }

    Timer {
        id: refreshTimer
        interval: 120
        repeat: false
        onTriggered: root.request("state", "[[BATCH]]j/monitors ; j/workspaces ; j/clients")
    }

    Timer {
        id: reconnectTimer
        interval: 1000
        repeat: false
        onTriggered: {
            if (root.eventSocketPath)
                eventSocket.connected = true
        }
    }
}
