import Quickshell
import Quickshell.Wayland
import Quickshell.Io
import QtQuick
import QtQuick.Layouts
import qs.widgets

PanelWindow {
    id: root
    WlrLayershell.layer: editMode ? WlrLayer.Overlay : WlrLayer.Background
    WlrLayershell.keyboardFocus: editMode ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.namespace: "quickshell:backgroundGrid"

    width: screen.width
    height: screen.height
    color: "transparent"

    // Export edit mode state for external control
    property bool editMode: false

    // Grid configuration (read from config)
    property int gridColumns: cfg ? cfg.gridColumns : 16
    property int gridRows: cfg ? cfg.gridRows : 9
    property color dotColor: col?.primary || "#adc6ff"
    property string fontFamily: cfg ? cfg.fontFamily : "Rubik"

    // Bar exclusive zone — offset the usable grid area
    property bool barOnTop: cfg ? cfg.barOnTop : true
    property int barHeight: cfg ? cfg.barHeight : 35
    property int barGap: cfg && cfg.barFloating ? cfg.barGap : 0
    property int barExclusion: barHeight + barGap

    // Usable area for the grid (full screen minus bar)
    property real gridAreaX: 0
    property real gridAreaY: barOnTop ? barExclusion : 0
    property real gridAreaWidth: root.width
    property real gridAreaHeight: root.height - barExclusion

    // Cell dimensions based on usable area
    property real cellWidth: gridAreaWidth / gridColumns
    property real cellHeight: gridAreaHeight / gridRows

    // Widget storage
    property var widgets: []

    // ── Clock state ──
    property string clockTime: Qt.formatTime(new Date(), "hh:mm")
    property string clockSeconds: Qt.formatTime(new Date(), "ss")
    property string clockDate: Qt.formatDate(new Date(), "dddd, MMMM d")
    property string clockAmPm: Qt.formatTime(new Date(), "AP")

    Timer {
        id: clockTimer
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            var now = new Date()
            clockTime = Qt.formatTime(now, "hh:mm")
            clockSeconds = Qt.formatTime(now, "ss")
            clockDate = Qt.formatDate(now, "dddd, MMMM d")
            clockAmPm = Qt.formatTime(now, "AP")
        }
    }

    // ── Weather state ──
    property string weatherTemp: "--"
    property string weatherCondition: ""
    property string weatherIcon: "cloud"
    property string weatherCity: ""
    property string weatherHumidity: ""
    property string weatherWind: ""
    property bool weatherLoaded: false

    // Fetch weather via wttr.in (auto-detects location by IP)
    Process {
        id: weatherProcess
        command: ["curl", "-sf", "wttr.in/?format=%t|%C|%h|%w|%l"]
        stdout: StdioCollector {
            onStreamFinished: {
                var output = text.trim()
                if (output && output.indexOf("|") !== -1) {
                    var parts = output.split("|")
                    weatherTemp = parts[0] || "--"
                    weatherCondition = parts[1] || ""
                    weatherHumidity = parts[2] || ""
                    weatherWind = parts[3] || ""
                    weatherCity = parts[4] ? parts[4].split(",")[0] : ""
                    weatherIcon = mapWeatherIcon(weatherCondition)
                    weatherLoaded = true
                }
            }
        }
    }

    function mapWeatherIcon(condition) {
        var c = condition.toLowerCase()
        if (c.indexOf("sunny") !== -1 || c.indexOf("clear") !== -1) return "clear_day"
        if (c.indexOf("partly") !== -1) return "partly_cloudy_day"
        if (c.indexOf("cloud") !== -1 || c.indexOf("overcast") !== -1) return "cloud"
        if (c.indexOf("rain") !== -1 || c.indexOf("drizzle") !== -1) return "rainy"
        if (c.indexOf("thunder") !== -1 || c.indexOf("storm") !== -1) return "thunderstorm"
        if (c.indexOf("snow") !== -1 || c.indexOf("blizzard") !== -1) return "weather_snowy"
        if (c.indexOf("fog") !== -1 || c.indexOf("mist") !== -1) return "foggy"
        if (c.indexOf("haze") !== -1) return "blur_on"
        return "cloud"
    }

    Timer {
        id: weatherTimer
        interval: 600000  // 10 minutes
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: weatherProcess.running = true
    }

    // ── Widget persistence via widgets.json ──
    property string widgetFilePath: Qt.resolvedUrl("../widgets.json").toString().replace("file://", "")

    // Default widgets used when no file exists
    property var defaultWidgets: [
        { id: "w1", type: "clock", gridX: 2, gridY: 2, gridWidth: 4, gridHeight: 3, title: "Clock" },
        { id: "w2", type: "weather", gridX: 10, gridY: 2, gridWidth: 4, gridHeight: 3, title: "Weather" }
    ]

    Process {
        id: readProcess
        command: ["cat", widgetFilePath]
        stdout: StdioCollector {
            onStreamFinished: {
                var output = text.trim()
                if (output && output.length > 2) {
                    try {
                        var parsed = JSON.parse(output)
                        if (Array.isArray(parsed) && parsed.length > 0) {
                            widgets = parsed
                            widgetRepeater.model = widgets
                            return
                        }
                    } catch (e) {
                        console.log("widgets.json parse error, using defaults")
                    }
                }
                // File empty, missing, or invalid — use defaults and write them
                widgets = defaultWidgets.map(function(w) { return Object.assign({}, w) })
                widgetRepeater.model = widgets
                saveWidgets()
            }
        }
        stderr: StdioCollector {
            onStreamFinished: {
                if (text.trim() !== "") {
                    // File doesn't exist — use defaults
                    widgets = defaultWidgets.map(function(w) { return Object.assign({}, w) })
                    widgetRepeater.model = widgets
                    saveWidgets()
                }
            }
        }
    }

    Process {
        id: writeProcess
    }

    // Watch widgets.json for external changes
    FileView {
        id: widgetsFileWatcher
        path: Qt.resolvedUrl("../widgets.json")
        watchChanges: true
        onFileChanged: {
            if (!editMode) {
                loadWidgets()
            }
        }
    }

    Component.onCompleted: {
        readProcess.running = true
    }

    // IPC handler for reloading widgets from external sources
    IpcHandler {
        target: "widgets"
        function reload() {
            loadWidgets()
        }
    }

    function toggleEditMode() {
        editMode = !editMode
        if (!editMode) {
            saveWidgets()
        }
    }

    function setEditMode(enabled) {
        editMode = enabled
        if (!enabled) {
            saveWidgets()
        }
    }

    function loadWidgets() {
        readProcess.running = true
    }

    function saveWidgets() {
        // Serialize to single-line JSON to avoid shell quoting issues
        var data = JSON.stringify(widgets)
        // Use printf to safely write (handles special chars better than echo)
        writeProcess.command = ["sh", "-c", "printf '%s' '" + data.replace(/'/g, "'\\''") + "' > " + widgetFilePath]
        writeProcess.running = true
    }

    function constrainGridX(x, w) {
        return Math.max(0, Math.min(x, gridColumns - w))
    }

    function constrainGridY(y, h) {
        return Math.max(0, Math.min(y, gridRows - h))
    }

    // Grid - only visible in edit mode, respects bar exclusive zone
    Item {
        id: gridContainer
        x: gridAreaX
        y: gridAreaY
        width: gridAreaWidth
        height: gridAreaHeight
        visible: editMode
        z: 1
        opacity: editMode ? 1 : 0

        Repeater {
            model: gridColumns + 1
            Rectangle {
                width: 1
                height: parent.height
                color: dotColor
                opacity: 0.3
                x: index * cellWidth
            }
        }

        Repeater {
            model: gridRows + 1
            Rectangle {
                width: parent.width
                height: 1
                color: dotColor
                opacity: 0.3
                y: index * cellHeight
            }
        }
    }

    // Drag/resize overlay — lives at root level so coordinates never shift
    // Activated when a widget drag or resize begins, captures all mouse movement
    MouseArea {
        id: dragOverlay
        anchors.fill: parent
        z: 200
        visible: false
        enabled: visible

        property var targetWidget: null   // the delegate Rectangle being manipulated
        property int targetIndex: -1
        property bool isResizing: false

        property int startGridX: 0
        property int startGridY: 0
        property int startGridW: 0
        property int startGridH: 0
        property real startMouseX: 0
        property real startMouseY: 0

        function beginDrag(w, idx, mx, my) {
            targetWidget = w
            targetIndex = idx
            isResizing = false
            startGridX = w.gridX
            startGridY = w.gridY
            startMouseX = mx
            startMouseY = my
            visible = true
        }

        function beginResize(w, idx, mx, my) {
            targetWidget = w
            targetIndex = idx
            isResizing = true
            startGridW = w.gridWidth
            startGridH = w.gridHeight
            startMouseX = mx
            startMouseY = my
            visible = true
        }

        onPositionChanged: (mouse) => {
            if (!targetWidget) return
            var dx = Math.round((mouse.x - startMouseX) / cellWidth)
            var dy = Math.round((mouse.y - startMouseY) / cellHeight)

            if (isResizing) {
                var newW = Math.max(2, startGridW + dx)
                var newH = Math.max(1, startGridH + dy)
                targetWidget.gridWidth = Math.min(newW, gridColumns - targetWidget.gridX)
                targetWidget.gridHeight = Math.min(newH, gridRows - targetWidget.gridY)
            } else {
                targetWidget.gridX = constrainGridX(startGridX + dx, targetWidget.gridWidth)
                targetWidget.gridY = constrainGridY(startGridY + dy, targetWidget.gridHeight)
            }
        }

        onReleased: {
            if (targetWidget && targetIndex >= 0 && targetIndex < widgets.length) {
                widgets[targetIndex].gridX = targetWidget.gridX
                widgets[targetIndex].gridY = targetWidget.gridY
                widgets[targetIndex].gridWidth = targetWidget.gridWidth
                widgets[targetIndex].gridHeight = targetWidget.gridHeight
            }
            targetWidget = null
            targetIndex = -1
            visible = false
        }
    }

    // Widget container
    Item {
        id: widgetContainer
        anchors.fill: parent
        z: 100
        visible: cfg ? cfg.desktopWidgets : true

        Repeater {
            id: widgetRepeater
            model: widgets

            delegate: Rectangle {
                id: widget
                property string widgetId: modelData.id
                property string widgetTitle: modelData.title
                property int gridX: modelData.gridX
                property int gridY: modelData.gridY
                property int gridWidth: modelData.gridWidth
                property int gridHeight: modelData.gridHeight

                x: gridAreaX + gridX * cellWidth
                y: gridAreaY + gridY * cellHeight
                width: gridWidth * cellWidth
                height: gridHeight * cellHeight
                color: cfg && cfg.widgetBackgroundColor !== "" ? cfg.widgetBackgroundColor : (col?.background || "#111318")
                opacity: cfg ? cfg.widgetOpacity : 0.85
                radius: cfg ? cfg.widgetRadius : 12
                clip: false
                z: 50

                // Border
                Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                    border.width: editMode ? 2 : (cfg ? cfg.widgetBorderWidth : 1)
                    border.color: editMode ? (col?.primary || "#adc6ff") : (cfg && cfg.widgetBorderColor !== "" ? cfg.widgetBorderColor : (col?.outline || "#8e9099"))
                    radius: parent.radius
                }

                // Blue tint in edit mode
                Rectangle {
                    anchors.fill: parent
                    color: editMode ? Qt.rgba(0.1, 0.5, 0.8, 0.25) : "transparent"
                    radius: parent.radius
                }

                // ── Adaptive scaling based on actual pixel dimensions ──
                // Independent width/height units for proper aspect-ratio handling
                property real wU: widget.width / 10   // width unit
                property real hU: widget.height / 10  // height unit
                // Primary text size: scale with height but cap relative to width
                property real primarySize: Math.min(hU * 3.5, wU * 2.2, 80)
                // Secondary text: proportional to primary
                property real secondarySize: Math.max(primarySize * 0.35, 10)
                property real tertiarySize: Math.max(primarySize * 0.28, 9)
                // Layout mode based on actual cell counts
                property int cellsW: widget.gridWidth
                property int cellsH: widget.gridHeight
                property real aspect: widget.width / Math.max(widget.height, 1)
                // Tiers: tiny (2x1), small (3x2), medium (4x3), large (6x4+)
                property bool isTiny: cellsW <= 2 || cellsH <= 1
                property bool isWide: aspect > 2.0
                property bool isTall: aspect < 0.8
                // Padding scales with size
                property real pad: Math.min(wU * 0.8, hU * 0.8, 20)

                // ── Clock content ──
                Item {
                    anchors.fill: parent
                    anchors.margins: pad
                    visible: modelData.type === "clock"
                    clip: true

                    // Tiny: just time, no extras
                    Text {
                        anchors.centerIn: parent
                        visible: isTiny
                        text: clockTime
                        color: col?.onSurface || "#e2e2e9"
                        font.pixelSize: Math.min(hU * 6, wU * 3.5)
                        font.weight: Font.Light
                        font.family: fontFamily
                    }

                    // Wide: horizontal — time | am/pm+sec | divider | date
                    Row {
                        anchors.centerIn: parent
                        spacing: wU * 0.4
                        visible: !isTiny && isWide

                        Text {
                            text: clockTime
                            color: col?.onSurface || "#e2e2e9"
                            font.pixelSize: primarySize
                            font.weight: Font.Light
                            font.family: fontFamily
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 1
                            Text {
                                text: clockAmPm
                                color: col?.primary || "#adc6ff"
                                font.pixelSize: secondarySize
                                font.weight: Font.Medium
                                font.family: fontFamily
                            }
                            Text {
                                text: clockSeconds
                                color: col?.onSurfaceVariant || "#c5c6d0"
                                font.pixelSize: secondarySize
                                font.family: fontFamily
                                opacity: 0.6
                            }
                        }
                        Rectangle {
                            width: 1; height: primarySize * 0.7
                            color: col?.outlineVariant || "#46464f"
                            anchors.verticalCenter: parent.verticalCenter
                            opacity: 0.4
                        }
                        Text {
                            text: clockDate
                            color: col?.onSurfaceVariant || "#c5c6d0"
                            font.pixelSize: secondarySize
                            font.family: fontFamily
                            anchors.verticalCenter: parent.verticalCenter
                            opacity: 0.8
                        }
                    }

                    // Normal/tall: vertical stacked
                    Column {
                        anchors.centerIn: parent
                        spacing: hU * 0.3
                        visible: !isTiny && !isWide

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: wU * 0.3

                            Text {
                                text: clockTime
                                color: col?.onSurface || "#e2e2e9"
                                font.pixelSize: primarySize
                                font.weight: Font.Light
                                font.family: fontFamily
                            }
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 1
                                Text {
                                    text: clockAmPm
                                    color: col?.primary || "#adc6ff"
                                    font.pixelSize: secondarySize
                                    font.weight: Font.Medium
                                    font.family: fontFamily
                                }
                                Text {
                                    text: clockSeconds
                                    color: col?.onSurfaceVariant || "#c5c6d0"
                                    font.pixelSize: secondarySize
                                    font.family: fontFamily
                                    opacity: 0.6
                                }
                            }
                        }

                        Text {
                            text: clockDate
                            color: col?.onSurfaceVariant || "#c5c6d0"
                            font.pixelSize: secondarySize
                            font.family: fontFamily
                            anchors.horizontalCenter: parent.horizontalCenter
                            opacity: 0.8
                            visible: cellsH >= 2
                        }
                    }
                }

                // ── Weather content ──
                Item {
                    anchors.fill: parent
                    anchors.margins: pad
                    visible: modelData.type === "weather"
                    clip: true

                    // Tiny: just temp
                    Text {
                        anchors.centerIn: parent
                        visible: isTiny && weatherLoaded
                        text: weatherTemp
                        color: col?.onSurface || "#e2e2e9"
                        font.pixelSize: Math.min(hU * 5, wU * 3)
                        font.weight: Font.Light
                        font.family: fontFamily
                    }

                    // Wide: icon + temp on left, details right
                    Row {
                        anchors.centerIn: parent
                        spacing: wU * 0.8
                        visible: !isTiny && isWide && weatherLoaded

                        Row {
                            spacing: wU * 0.3
                            anchors.verticalCenter: parent.verticalCenter
                            MaterialSymbol {
                                icon: weatherIcon
                                iconSize: primarySize * 0.85
                                color: col?.primary || "#adc6ff"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: weatherTemp
                                color: col?.onSurface || "#e2e2e9"
                                font.pixelSize: primarySize
                                font.weight: Font.Light
                                font.family: fontFamily
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text {
                                text: weatherCondition
                                color: col?.onSurfaceVariant || "#c5c6d0"
                                font.pixelSize: secondarySize
                                font.family: fontFamily
                            }
                            Row {
                                spacing: wU * 0.5
                                Row {
                                    spacing: 3
                                    MaterialSymbol { icon: "water_drop"; iconSize: tertiarySize; color: col?.onSurfaceVariant || "#c5c6d0"; anchors.verticalCenter: parent.verticalCenter }
                                    Text { text: weatherHumidity; color: col?.onSurfaceVariant || "#c5c6d0"; font.pixelSize: tertiarySize; font.family: fontFamily; anchors.verticalCenter: parent.verticalCenter }
                                }
                                Row {
                                    spacing: 3
                                    MaterialSymbol { icon: "air"; iconSize: tertiarySize; color: col?.onSurfaceVariant || "#c5c6d0"; anchors.verticalCenter: parent.verticalCenter }
                                    Text { text: weatherWind; color: col?.onSurfaceVariant || "#c5c6d0"; font.pixelSize: tertiarySize; font.family: fontFamily; anchors.verticalCenter: parent.verticalCenter }
                                }
                            }
                            Text {
                                text: weatherCity
                                color: col?.outline || "#8e9099"
                                font.pixelSize: tertiarySize
                                font.family: fontFamily
                                opacity: 0.7
                                visible: cellsW >= 6
                            }
                        }
                    }

                    // Normal/tall: vertical stacked
                    Column {
                        anchors.centerIn: parent
                        spacing: hU * 0.35
                        visible: !isTiny && !isWide && weatherLoaded

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: wU * 0.4

                            MaterialSymbol {
                                icon: weatherIcon
                                iconSize: primarySize * 0.85
                                color: col?.primary || "#adc6ff"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: weatherTemp
                                color: col?.onSurface || "#e2e2e9"
                                font.pixelSize: primarySize
                                font.weight: Font.Light
                                font.family: fontFamily
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        Text {
                            text: weatherCondition
                            color: col?.onSurfaceVariant || "#c5c6d0"
                            font.pixelSize: secondarySize
                            font.family: fontFamily
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: cellsH >= 2
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: wU * 0.6
                            visible: cellsH >= 3
                            Row {
                                spacing: 3
                                MaterialSymbol { icon: "water_drop"; iconSize: tertiarySize; color: col?.onSurfaceVariant || "#c5c6d0"; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: weatherHumidity; color: col?.onSurfaceVariant || "#c5c6d0"; font.pixelSize: tertiarySize; font.family: fontFamily; anchors.verticalCenter: parent.verticalCenter }
                            }
                            Row {
                                spacing: 3
                                MaterialSymbol { icon: "air"; iconSize: tertiarySize; color: col?.onSurfaceVariant || "#c5c6d0"; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: weatherWind; color: col?.onSurfaceVariant || "#c5c6d0"; font.pixelSize: tertiarySize; font.family: fontFamily; anchors.verticalCenter: parent.verticalCenter }
                            }
                        }

                        Text {
                            text: weatherCity
                            color: col?.outline || "#8e9099"
                            font.pixelSize: tertiarySize
                            font.family: fontFamily
                            anchors.horizontalCenter: parent.horizontalCenter
                            opacity: 0.7
                            visible: cellsH >= 4
                        }
                    }

                    // Loading state
                    Column {
                        anchors.centerIn: parent
                        spacing: hU * 0.5
                        visible: !weatherLoaded

                        MaterialSymbol {
                            icon: "cloud_sync"
                            iconSize: primarySize * 0.7
                            color: col?.onSurfaceVariant || "#c5c6d0"
                            anchors.horizontalCenter: parent.horizontalCenter
                            opacity: 0.5
                        }
                        Text {
                            text: isTiny ? "..." : "Loading weather..."
                            color: col?.onSurfaceVariant || "#c5c6d0"
                            font.pixelSize: secondarySize
                            font.family: fontFamily
                            anchors.horizontalCenter: parent.horizontalCenter
                            opacity: 0.5
                        }
                    }
                }

                // ── Generic / custom widget content ──
                Column {
                    anchors.centerIn: parent
                    spacing: hU * 0.4
                    visible: modelData.type !== "clock" && modelData.type !== "weather"

                    MaterialSymbol {
                        icon: "widgets"
                        iconSize: primarySize * 0.7
                        color: col?.onSurface || "#e2e2e9"
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: widgetTitle
                        color: col?.onSurface || "#e2e2e9"
                        font.pixelSize: secondarySize
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: !isTiny
                    }
                }

                // Drag MouseArea — on press, hands off to the root-level overlay
                MouseArea {
                    id: widgetMouse
                    anchors.fill: parent
                    enabled: editMode
                    hoverEnabled: true
                    cursorShape: editMode ? Qt.SizeAllCursor : Qt.ArrowCursor

                    onPressed: (mouse) => {
                        // Convert press position to root coords manually
                        var rootX = widget.x + mouse.x
                        var rootY = widget.y + mouse.y
                        dragOverlay.beginDrag(widget, index, rootX, rootY)
                    }
                }

                // Resize handle (bottom-right corner)
                Rectangle {
                    id: resizeHandle
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: -8
                    width: 24
                    height: 24
                    radius: 12
                    color: resizeMouse.containsMouse ? col?.primaryContainer || "#2b4678" : (col?.surfaceContainerHighest || "#333")
                    border.width: 2
                    border.color: col?.primary || "#adc6ff"
                    visible: editMode
                    z: 20

                    MaterialSymbol {
                        anchors.centerIn: parent
                        icon: "open_in_full"
                        iconSize: 14
                        color: col?.onPrimary || "#102f60"
                    }

                    MouseArea {
                        id: resizeMouse
                        anchors.fill: parent
                        enabled: editMode
                        hoverEnabled: true
                        cursorShape: Qt.SizeFDiagCursor

                        onPressed: (mouse) => {
                            var rootX = widget.x + resizeHandle.x + mouse.x
                            var rootY = widget.y + resizeHandle.y + mouse.y
                            dragOverlay.beginResize(widget, index, rootX, rootY)
                        }
                    }
                }

                // Delete button
                Rectangle {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: -10
                    width: 24
                    height: 24
                    radius: 12
                    color: deleteMouse.containsMouse ? col?.error || "#ffb4ab" : col?.errorContainer || "#93000a"
                    visible: editMode
                    z: 20

                    MaterialSymbol {
                        icon: "close"
                        iconSize: 12
                        color: col?.onErrorContainer || "#ffdad6"
                        anchors.centerIn: parent
                    }

                    MouseArea {
                        id: deleteMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var newWidgets = widgets.filter(w => w.id !== widgetId)
                            widgets = newWidgets
                            widgetRepeater.model = widgets
                        }
                    }
                }
            }
        }
    }

    // Edit mode indicator
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 30
        width: 120
        height: 36
        radius: 18
        color: col?.errorContainer || "#93000a"
        opacity: editMode ? 1 : 0
        z: 100

        Row {
            anchors.centerIn: parent
            spacing: 8
            MaterialSymbol { icon: "edit"; iconSize: 18; color: col?.onErrorContainer; anchors.verticalCenter: parent.verticalCenter }
            Text { text: "Edit Mode"; color: col?.onErrorContainer; font.pixelSize: 13; font.weight: Font.Bold; anchors.verticalCenter: parent.verticalCenter }
        }
    }

    // Exit button
    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 30
        width: 36
        height: 36
        radius: 18
        color: exitMouse.containsMouse ? col?.error || "#ffb4ab" : col?.surfaceContainer || "#1e1f25"
        opacity: editMode ? 1 : 0
        z: 100

        MaterialSymbol {
            icon: "close"
            iconSize: 18
            color: exitMouse.containsMouse ? col?.onError || "#690005" : col?.onSurface || "#e2e2e9"
            anchors.centerIn: parent
        }

        MouseArea {
            id: exitMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: toggleEditMode()
        }
    }

    // Add Widget button
    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 30
        anchors.rightMargin: 80
        width: addText.width + 36
        height: 36
        radius: 18
        color: addMouse.containsMouse ? col?.primaryContainer || "#2b4678" : col?.surfaceContainer || "#1e1f25"
        opacity: editMode ? 1 : 0
        z: 100

        Row {
            id: addRow
            anchors.centerIn: parent
            spacing: 6
            MaterialSymbol { icon: "add"; iconSize: 18; color: col?.onSurface; anchors.verticalCenter: parent.verticalCenter }
            Text { id: addText; text: "Add Widget"; color: col?.onSurface; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
        }

        MouseArea {
            id: addMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                var newW = { id: "w" + Date.now(), type: "custom", gridX: 6, gridY: 3, gridWidth: 4, gridHeight: 3, title: "New Widget" }
                widgets.push(newW)
                widgetRepeater.model = widgets
            }
        }
    }

    // Tooltip
    Rectangle {
        id: tooltip
        width: coordText.width + 16
        height: 26
        radius: 13
        color: col?.surfaceContainer || "#1e1f25"
        border.width: 1
        border.color: col?.outline || "#8e9099"
        opacity: editMode && coordText.text !== "" ? 0.9 : 0
        z: 100

        Text {
            id: coordText
            anchors.centerIn: parent
            text: ""
            color: col?.onSurface || "#e2e2e9"
            font.pixelSize: 11
            font.family: fontFamily
        }
    }

    // Global mouse tracking - DISABLED to not interfere with widget MouseAreas
    // We'll show tooltip via individual widget hovers instead
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        enabled: false  // ALWAYS disabled - blocks widget events otherwise
        visible: false  // Also invisible
    }
}
