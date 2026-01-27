import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import Quickshell
import Quickshell.Hyprland

Item {
    id: root
    anchors.fill: parent
    anchors.bottomMargin: 0

    // Get workspace count from config, default to 10
    property int wsCount: cfg ? cfg.workspaceCount : 10
    property bool dynamicWs: cfg ? cfg.dynamicWorkspaces : false
    property string wsStyle: cfg ? cfg.workspaceStyle : "dots"
    property int moduleRadius: cfg ? Math.max(8, Math.round(cfg.barRadius * 0.7)) : 14
    property string fontFamily: cfg ? cfg.fontFamily : "Rubik"

    // Helper function to check if workspace has windows
    function workspaceHasWindows(wsId) {
        for (let i = 0; i < Hyprland.workspaces.values.length; i++) {
            let ws = Hyprland.workspaces.values[i]
            if (ws.id === wsId && ws.toplevels && ws.toplevels.values.length > 0) {
                return true
            }
        }
        return false
    }

    // Store occupied state for each workspace
    property var occupiedStates: {
        let states = []
        for (let i = 1; i <= wsCount; i++) {
            states.push(workspaceHasWindows(i))
        }
        return states
    }

    // Refresh states when workspaces change
    Connections {
        target: Hyprland.workspaces
        function onObjectInserted() { root.occupiedStates = Qt.binding(() => root.computeStates()) }
        function onObjectRemoved() { root.occupiedStates = Qt.binding(() => root.computeStates()) }
    }

    function computeStates() {
        let states = []
        for (let i = 1; i <= wsCount; i++) {
            states.push(workspaceHasWindows(i))
        }
        return states
    }

    Rectangle {
        id: container
        width: wsRow.width
        height: 29
        anchors.centerIn: parent
        radius: moduleRadius
        color: col.surfaceContainer
        Behavior on width {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        // Metaball canvas for dots style
        Canvas {
            id: metaballCanvas
            anchors.fill: parent
            z: 1
            visible: wsStyle === "dots"

            property var states: root.occupiedStates

            onStatesChanged: requestPaint()

            Connections {
                target: Hyprland.workspaces
                function onObjectInserted() { metaballCanvas.requestPaint() }
                function onObjectRemoved() { metaballCanvas.requestPaint() }
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

                let cellWidth = 29
                let centerY = height / 2
                let dotRadius = 4
                let bigRadius = 12

                // Find groups of adjacent occupied workspaces
                let groups = []
                let currentGroup = null

                for (let i = 0; i < root.wsCount; i++) {
                    let occupied = root.workspaceHasWindows(i + 1)

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
                    let startX = group.start * cellWidth + cellWidth / 2
                    let endX = group.end * cellWidth + cellWidth / 2

                    if (group.start === group.end) {
                        ctx.beginPath()
                        ctx.arc(startX, centerY, bigRadius, 0, Math.PI * 2)
                        ctx.fill()
                    } else {
                        let r = bigRadius
                        ctx.beginPath()
                        ctx.arc(startX, centerY, r, Math.PI / 2, -Math.PI / 2)
                        ctx.lineTo(endX, centerY - r)
                        ctx.arc(endX, centerY, r, -Math.PI / 2, Math.PI / 2)
                        ctx.lineTo(startX, centerY + r)
                        ctx.closePath()
                        ctx.fill()
                    }
                }

                // Draw small dots for all workspaces
                for (let i = 0; i < root.wsCount; i++) {
                    let occupied = root.workspaceHasWindows(i + 1)
                    let x = i * cellWidth + cellWidth / 2
                    
                    if (occupied) {
                        ctx.fillStyle = col.surfaceContainer
                    } else {
                        ctx.fillStyle = col.outline
                    }
                    
                    ctx.beginPath()
                    ctx.arc(x, centerY, dotRadius, 0, Math.PI * 2)
                    ctx.fill()
                }
            }
        }

        // Numbers/Bars style overlay
        Row {
            id: numbersRow
            anchors.centerIn: parent
            visible: wsStyle !== "dots"
            z: 1

            Repeater {
                model: root.wsCount
                
                Rectangle {
                    width: wsStyle === "bars" ? 8 : 29
                    height: wsStyle === "bars" ? 16 : 29
                    radius: wsStyle === "bars" ? 4 : moduleRadius
                    color: {
                        let isActive = Hyprland.focusedMonitor && 
                                       Hyprland.focusedMonitor.activeWorkspace &&
                                       Hyprland.focusedMonitor.activeWorkspace.id === (index + 1)
                        let occupied = root.workspaceHasWindows(index + 1)
                        
                        if (isActive) return "transparent"
                        if (occupied) return col.secondaryContainer
                        return "transparent"
                    }

                    Text {
                        anchors.centerIn: parent
                        text: index + 1
                        visible: wsStyle === "numbers"
                        font.pixelSize: 12
                        font.family: root.fontFamily
                        font.weight: 700
                        color: {
                            let isActive = Hyprland.focusedMonitor && 
                                           Hyprland.focusedMonitor.activeWorkspace &&
                                           Hyprland.focusedMonitor.activeWorkspace.id === (index + 1)
                            let occupied = root.workspaceHasWindows(index + 1)
                            
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
                        visible: wsStyle === "bars"
                        color: {
                            let isActive = Hyprland.focusedMonitor && 
                                           Hyprland.focusedMonitor.activeWorkspace &&
                                           Hyprland.focusedMonitor.activeWorkspace.id === (index + 1)
                            let occupied = root.workspaceHasWindows(index + 1)
                            
                            if (isActive) return "transparent"
                            if (occupied) return col.onSecondaryContainer
                            return col.outline
                        }
                    }
                }
            }
        }

        // Interaction layer
        Row {
            id: wsRow
            z: 2
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
                            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: hoverIndicator.opacity = 0.3
                        onExited: hoverIndicator.opacity = 0
                        onClicked: {
                            Hyprland.dispatch("workspace " + (index + 1))
                        }
                        onWheel: {
                            if (wheel.angleDelta.y > 0) {
                                if (Hyprland.focusedMonitor.activeWorkspace.id > 1) {
                                    Hyprland.dispatch("workspace -1")
                                }
                            } else if (wheel.angleDelta.y < 0) {
                                if (Hyprland.focusedMonitor.activeWorkspace.id < root.wsCount) {
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
            anchors.fill: parent
            z: 10

            property real targetX: Hyprland.focusedMonitor && Hyprland.focusedMonitor.activeWorkspace
                ? (Hyprland.focusedMonitor.activeWorkspace.id - 1) * 29 + 3
                : 3

            // Stretching trail capsule
            Rectangle {
                id: trail
                height: 23
                radius: 11.5
                color: col.primary
                anchors.verticalCenter: parent.verticalCenter
                visible: trailAnimation.running
                
                x: Math.min(activeIndicator.x, tailX.x)
                width: Math.abs(activeIndicator.x - tailX.x) + 45
                
                Item {
                    id: tailX
                    x: activeIndicatorContainer.targetX
                    
                    Behavior on x {
                        NumberAnimation {
                            id: trailAnimation
                            duration: 250
                            easing.type: Easing.OutCubic
                        }
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
                anchors.verticalCenter: parent.verticalCenter
                x: activeIndicatorContainer.targetX
                
                Behavior on x {
                    NumberAnimation {
                        duration: 150
                        easing.type: Easing.OutCubic
                    }
                }

                // Content based on style
                Rectangle {
                    anchors.centerIn: parent
                    width: 8
                    height: 8
                    radius: 4
                    color: col.onPrimary
                    visible: wsStyle === "dots"
                }

                Text {
                    anchors.centerIn: parent
                    text: Hyprland.focusedMonitor && Hyprland.focusedMonitor.activeWorkspace 
                        ? Hyprland.focusedMonitor.activeWorkspace.id : "1"
                    visible: wsStyle === "numbers"
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
                    visible: wsStyle === "bars"
                }
            }
        }
    }
}
