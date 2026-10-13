import qs.services
import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Shapes
import Quickshell
import Quickshell.Widgets

Item {
    id: root
    anchors.fill: parent
    anchors.bottomMargin: 0

    // Get workspace count from config, default to 10
    property int wsCount: Hyprland.isZwwm ? Hyprland.workspaceIds.length : (cfg ? cfg.workspaceCount : 10)
    function workspaceId(index) { return Hyprland.isZwwm ? Hyprland.workspaceIds[index] : index + 1 }
    function workspaceName(index) {
        if (index < 0 || index >= wsCount) return ""
        return Hyprland.isZwwm ? Hyprland.workspaces[index]?.name || String(workspaceId(index)) : String(index + 1)
    }
    function isActive(index) { return Hyprland.activeWorkspaceIds.indexOf(workspaceId(index)) >= 0 }
    readonly property int activeIndex: Hyprland.isZwwm ? Hyprland.workspaceIds.indexOf(Hyprland.activeWorkspaceId) : Hyprland.activeWorkspaceId - 1
    property bool dynamicWs: cfg ? cfg.dynamicWorkspaces : false
    property string wsStyle: cfg ? cfg.workspaceStyle : "dots"
    property int moduleRadius: cfg ? Math.max(8, Math.round(cfg.barRadius * 0.7)) : 14
    property string fontFamily: cfg ? cfg.fontFamily : "Rubik"
    property bool vertical: false   // set true for left/right bar

    readonly property var workspaceApps: {
        // Track desktop-entry updates as well as window/tag changes.
        const entries = DesktopEntries.applications.values
        const apps = {}
        const seen = {}
        const clients = Hyprland.clients.slice().sort((a, b) => Number(b.activated) - Number(a.activated))
        for (const client of clients) {
            const id = client.workspace?.id
            if (!id) continue
            const appId = client.className || ""
            const entry = appId ? DesktopEntries.heuristicLookup(appId) : null
            const key = String(entry?.id || appId || "unknown").toLowerCase()
            if (!apps[id]) { apps[id] = []; seen[id] = new Set() }
            if (seen[id].has(key) || apps[id].length >= 2) continue
            seen[id].add(key)
            apps[id].push({ appId: appId, icon: entry?.icon || appId || "application-x-executable" })
        }
        return apps
    }
    function appsForWorkspace(id) { return workspaceApps[id] || [] }
    onWorkspaceAppsChanged: metaballCanvas.requestPaint()

    // Helper function to check if workspace has windows
    function workspaceHasWindows(wsId) {
        return Hyprland.workspaceHasWindows(wsId)
    }

    // Store occupied state for each workspace
    property var occupiedStates: {
        let states = []
        for (let i = 0; i < wsCount; i++) {
            states.push(workspaceHasWindows(workspaceId(i)))
        }
        return states
    }

    // Refresh states when workspaces change
    Connections {
        target: Hyprland
        function onStateChanged() { root.occupiedStates = root.computeStates() }
    }

    function computeStates() {
        let states = []
        for (let i = 0; i < wsCount; i++) {
            states.push(workspaceHasWindows(workspaceId(i)))
        }
        return states
    }

    Rectangle {
        id: container
        width:  root.vertical ? 29 : wsRow.width
        height: root.vertical ? wsRow.height : 29
        anchors.centerIn: parent
        radius: moduleRadius
        color: col.surfaceContainer
        Behavior on width  { NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic } }
        Behavior on height { NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic } }

        // Metaball canvas for dots style
        Canvas {
            id: metaballCanvas
            anchors.fill: parent
            z: 1
            visible: wsStyle === "dots"

            property var states: root.occupiedStates

            onStatesChanged: requestPaint()

            Connections {
                target: Hyprland
                function onStateChanged() { metaballCanvas.requestPaint() }
            }

            Connections {
                target: col
                function onSecondaryContainerChanged() { metaballCanvas.requestPaint() }
                function onSurfaceContainerChanged() { metaballCanvas.requestPaint() }
                function onOutlineChanged() { metaballCanvas.requestPaint() }
            }

            Connections {
                target: cfg
                function onWorkspaceCountChanged() { metaballCanvas.requestPaint() }
                function onWorkspaceStyleChanged() { metaballCanvas.requestPaint() }
            }

            onPaint: {
                let ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                let cellSize = 29
                let centerX = width / 2
                let centerY = height / 2
                let dotRadius = 4
                let bigRadius = 12

                // Find groups of adjacent occupied workspaces
                let groups = []
                let currentGroup = null

                for (let i = 0; i < root.wsCount; i++) {
                    let occupied = root.workspaceHasWindows(root.workspaceId(i))

                    if (occupied) {
                        if (currentGroup === null) {
                            currentGroup = { start: i, end: i }
                        } else {
                            currentGroup.end = i
                        }
                    } else {
                        if (currentGroup !== null) {
                            groups.push(currentGroup)
                            currentGroup = null
                        }
                    }
                }
                if (currentGroup !== null) {
                    groups.push(currentGroup)
                }

                // Draw metaball shapes for groups
                ctx.fillStyle = col.secondaryContainer

                for (let g = 0; g < groups.length; g++) {
                    let group = groups[g]
                    let startPos = group.start * cellSize + cellSize / 2
                    let endPos = group.end * cellSize + cellSize / 2

                    if (group.start === group.end) {
                        ctx.beginPath()
                        ctx.arc(root.vertical ? centerX : startPos, root.vertical ? startPos : centerY, bigRadius, 0, Math.PI * 2)
                        ctx.fill()
                    } else {
                        let r = bigRadius
                        ctx.beginPath()
                        if (root.vertical) {
                            ctx.arc(centerX, startPos, r, Math.PI, 0)
                            ctx.lineTo(centerX + r, endPos)
                            ctx.arc(centerX, endPos, r, 0, Math.PI)
                            ctx.lineTo(centerX - r, startPos)
                        } else {
                            ctx.arc(startPos, centerY, r, Math.PI / 2, -Math.PI / 2)
                            ctx.lineTo(endPos, centerY - r)
                            ctx.arc(endPos, centerY, r, -Math.PI / 2, Math.PI / 2)
                            ctx.lineTo(startPos, centerY + r)
                        }
                        ctx.closePath()
                        ctx.fill()
                    }
                }

                // Draw small dots for all workspaces
                for (let i = 0; i < root.wsCount; i++) {
                    let occupied = root.workspaceHasWindows(root.workspaceId(i))
                    let pos = i * cellSize + cellSize / 2
                    if (root.appsForWorkspace(root.workspaceId(i)).length > 0) continue
                    
                    if (occupied) {
                        ctx.fillStyle = col.surfaceContainer
                    } else {
                        ctx.fillStyle = col.outline
                    }
                    
                    ctx.beginPath()
                    ctx.arc(root.vertical ? centerX : pos, root.vertical ? pos : centerY, dotRadius, 0, Math.PI * 2)
                    ctx.fill()
                }
            }
        }

        // Numbers/Bars style overlay
        Grid {
            id: numbersRow
            anchors.centerIn: parent
            visible: wsStyle !== "dots"
            z: 1
            columns: root.vertical ? 1 : root.wsCount
            rows: root.vertical ? root.wsCount : 1

            Repeater {
                model: root.wsCount
                
                Rectangle {
                    width: wsStyle === "bars" ? 8 : 29
                    height: wsStyle === "bars" ? 16 : 29
                    radius: wsStyle === "bars" ? 4 : moduleRadius
                    color: {
                        let isActive = root.isActive(index)
                        let occupied = root.workspaceHasWindows(root.workspaceId(index))
                        
                        if (isActive) return "transparent"
                        if (occupied) return col.secondaryContainer
                        return "transparent"
                    }

                    Text {
                        anchors.centerIn: parent
                        text: root.workspaceName(index)
                        visible: wsStyle === "numbers" && root.appsForWorkspace(root.workspaceId(index)).length === 0
                        font.pixelSize: 12
                        font.family: root.fontFamily
                        font.weight: 700
                        color: {
                            let isActive = root.isActive(index)
                            let occupied = root.workspaceHasWindows(root.workspaceId(index))
                            
                            if (isActive) return "transparent"
                            if (occupied) return col.onSecondaryContainer
                            return col.outline
                        }
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: 4
                        height: parent.height - 4
                        radius: 2
                        visible: wsStyle === "bars" && root.appsForWorkspace(root.workspaceId(index)).length === 0
                        color: {
                            let isActive = root.isActive(index)
                            let occupied = root.workspaceHasWindows(root.workspaceId(index))
                            
                            if (isActive) return "transparent"
                            if (occupied) return col.onSecondaryContainer
                            return col.outline
                        }
                    }
                }
            }
        }

        // Interaction layer
        Grid {
            id: wsRow
            z: 2
            columns: root.vertical ? 1 : root.wsCount
            rows:    root.vertical ? root.wsCount : 1
            Repeater {
                model: root.wsCount
                Rectangle {
                    width: 29
                    height: 29
                    radius: moduleRadius
                    color: "transparent"

                    Rectangle {
                        id: hoverIndicator
                        anchors.fill: parent
                        radius: moduleRadius
                        color: col.surfaceContainerHighest
                        opacity: 0
                        Behavior on opacity {
                            NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: hoverIndicator.opacity = 0.3
                        onExited: hoverIndicator.opacity = 0
                        onClicked: {
                            Hyprland.dispatch("workspace " + root.workspaceId(index))
                        }
                        onWheel: wheel => {
                            if (wheel.angleDelta.y > 0) {
                                if (root.activeIndex > 0) {
                                    Hyprland.dispatch("workspace -1")
                                }
                            } else if (wheel.angleDelta.y < 0) {
                                if (root.activeIndex < root.wsCount - 1) {
                                    Hyprland.dispatch("workspace +1")
                                }
                            }
                        }
                    }
                }
            }
        }

        // Active workspace indicator with trail effect
        Item {
            id: activeIndicatorContainer
            visible: root.activeIndex >= 0
            anchors.fill: parent
            z: 10

            property real targetPos: root.activeIndex * 29 + 3
            // keep legacy alias for horizontal
            property real targetX: !root.vertical ? targetPos : 3
            property real targetY:  root.vertical ? targetPos : 3

            // Stretching trail capsule
            Rectangle {
                id: trail
                width:  root.vertical ? 23 : Math.abs(activeIndicator.x - tailPos.x) + 45
                height: root.vertical ? Math.abs(activeIndicator.y - tailPos.y) + 45 : 23
                radius: 11.5
                color: col.primary
                anchors.verticalCenter:   root.vertical ? undefined : parent.verticalCenter
                anchors.horizontalCenter: root.vertical ? parent.horizontalCenter : undefined
                visible: trailAnimation.running

			x: root.vertical ? 3 : Math.min(activeIndicator.x, tailPos.x)
			y: root.vertical ? Math.min(activeIndicator.y, tailPos.y) : 3

                Item {
                    id: tailPos
                    x: root.vertical ? 0 : activeIndicatorContainer.targetPos
                    y: root.vertical ? activeIndicatorContainer.targetPos : 0

                    Behavior on x {
                        NumberAnimation {
                            id: trailAnimation
                            duration: Gstate.animDuration
                            easing.type: Easing.OutCubic
                        }
                    }
                    Behavior on y {
                        NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
                    }
                }
            }

            // Main active indicator
            Rectangle {
                id: activeIndicator
                width: 23
                height: 23
                radius: 40
                color: col.primary
                anchors.verticalCenter:   root.vertical ? undefined : parent.verticalCenter
                anchors.horizontalCenter: root.vertical ? parent.horizontalCenter : undefined
                x: root.vertical ? 3 : activeIndicatorContainer.targetPos
                y: root.vertical ? activeIndicatorContainer.targetPos : 3

                Behavior on x {
                    NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
                }
                Behavior on y {
                    NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
                }

                // Content based on style
                Rectangle {
                    anchors.centerIn: parent
                    width: 8
                    height: 8
                    radius: 4
                    color: col.onPrimary
                    visible: wsStyle === "dots" && root.appsForWorkspace(Hyprland.activeWorkspaceId).length === 0
                }

                Text {
                    anchors.centerIn: parent
                    text: root.workspaceName(root.activeIndex)
                    visible: wsStyle === "numbers" && root.appsForWorkspace(Hyprland.activeWorkspaceId).length === 0
                    font.pixelSize: 12
                    font.family: root.fontFamily
                    font.weight: 700
                    color: col.onPrimary
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 4
                    height: 14
                    radius: 2
                    color: col.onPrimary
                    visible: wsStyle === "bars" && root.appsForWorkspace(Hyprland.activeWorkspaceId).length === 0
                }
            }
        }

        // Keep the icon column above the active capsule while leaving input
        // handling on the existing tag cells below it.
        Grid {
            anchors.centerIn: parent
            z: 11
            columns: root.vertical ? 1 : root.wsCount
            rows: root.vertical ? root.wsCount : 1

            Repeater {
                model: root.wsCount
                Item {
                    id: iconCell
                    width: 29
                    height: 29
                    readonly property var apps: root.appsForWorkspace(root.workspaceId(index))
                    readonly property bool active: root.isActive(index)

                    Column {
                        anchors.centerIn: parent
                        spacing: 1
                        Repeater {
                            model: iconCell.apps
                            IconImage {
                                required property var modelData
                                implicitSize: iconCell.apps.length > 1 ? 11 : 18
                                width: implicitSize
                                height: implicitSize
                                source: Quickshell.iconPath(modelData.icon, true)
                                    || Quickshell.iconPath("application-x-executable", true)
                                    || Qt.resolvedUrl("workspace-app-fallback.svg")
                                layer.enabled: true
                                layer.effect: MultiEffect {
                                    colorization: 1
                                    colorizationColor: iconCell.active ? col.onPrimary : col.onSecondaryContainer
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
