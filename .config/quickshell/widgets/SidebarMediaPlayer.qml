import QtCore
import qs.services
import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import Quickshell
import Quickshell.Io
import Quickshell.Widgets
import qs.widgets

Item {
    id: root

    implicitHeight: 160

    property var cavaBars: []

    Process {
        id: cavaProcess
        command: ["cava", "-p", StandardPaths.writableLocation(StandardPaths.GenericConfigLocation).toString() + "/blxshell/cava_media.conf"]
        running: true
        stdout: SplitParser {
            onRead: data => {
                const parts = data.trim().split(";")
                const vals = []
                for (let i = 0; i < parts.length; i++) {
                    const v = parseInt(parts[i])
                    if (!isNaN(v)) vals.push(v)
                }
                if (vals.length > 0) {
                    root.cavaBars = vals
                    cavaCanvas.requestPaint()
                }
            }
        }
    }

    property bool seeking: false

    readonly property var activePlayer: MprisService.activePlayer
    readonly property real effectiveLength: MprisService.effectiveLength
    readonly property real effectivePosition: MprisService.effectivePosition
    readonly property bool hasProgressTrack: MprisService.hasProgressTrack
    readonly property string realArtUrl: MprisService.realArtUrl
    readonly property string artUrl: MprisService.artUrl
    readonly property bool hasArt: MprisService.hasArt
    readonly property var activeTrack: MprisService.activeTrack

    function formatTime(seconds) {
        const s = Math.max(0, Math.floor(seconds || 0))
        const h = Math.floor(s / 3600)
        const m = Math.floor((s % 3600) / 60)
        const r = s % 60
        if (h > 0)
            return h + ":" + (m < 10 ? "0" : "") + m + ":" + (r < 10 ? "0" : "") + r
        return m + ":" + (r < 10 ? "0" : "") + r
    }

    // ── Dominant color extraction ──
    ColorQuantizer {
        id: quantizer
        source: root.realArtUrl
        depth: 2      // 4 colors — fast, enough for dominant
        rescaleSize: 48
        onColorsChanged: {
            if (colors && colors.length > 0)
                dominantColor.targetColor = colors[0]
        }
    }

    QtObject {
        id: dominantColor
        property color targetColor: col.primaryContainer
        property color color: col.primaryContainer
        Behavior on color { ColorAnimation { duration: 600; easing.type: Easing.OutCubic } }
        onTargetColorChanged: color = targetColor
    }

    // Auto-contrast text colors derived from dominant
    readonly property color textColor: {
        const c = dominantColor.color
        const lum = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b
        return lum > 0.45 ? Qt.rgba(0, 0, 0, 0.92) : Qt.rgba(1, 1, 1, 0.95)
    }
    readonly property color subTextColor: {
        const c = dominantColor.color
        const lum = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b
        return lum > 0.45 ? Qt.rgba(0, 0, 0, 0.60) : Qt.rgba(1, 1, 1, 0.62)
    }

    // ── Card ──
    ClippingRectangle {
        anchors.fill: parent
        radius: 20
        color: root.hasArt ? dominantColor.color : col.surfaceContainer
        //clip: true
        Behavior on color { ColorAnimation { duration: 600; easing.type: Easing.OutCubic } }

        // Full-bleed blurred art background
        Image {
            id: artBg
            anchors.fill: parent
            source: root.realArtUrl
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: false
            visible: false
            opacity: 0

            onStatusChanged: if (status === Image.Ready) artFadeIn.start()
            onSourceChanged: opacity = 0

            NumberAnimation {
                id: artFadeIn
                target: artBg
                property: "opacity"
                to: 0.38
                duration: 700
                easing.type: Easing.OutCubic
            }
        }

        MultiEffect {
            anchors.fill: parent
            source: artBg
            visible: root.hasArt
            opacity: artBg.opacity
            blurEnabled: true
            blur: 0.72
            brightness: -0.08
            saturation: 1.18
            autoPaddingEnabled: false
        }

        // Gradient scrim for readability
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Qt.rgba(
                    dominantColor.color.r,
                    dominantColor.color.g,
                    dominantColor.color.b, 0.78) }
                GradientStop { position: 1.0; color: Qt.rgba(
                    dominantColor.color.r,
                    dominantColor.color.g,
                    dominantColor.color.b, 0.32) }
            }
        }

        // Cava visualizer overlay at bottom
        Canvas {
            id: cavaCanvas
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 28
            opacity: 0.18

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                const bars = root.cavaBars
                if (!bars || bars.length === 0) return
                const n = bars.length
                const c = root.hasArt ? root.textColor : col.onSurface
                ctx.fillStyle = Qt.rgba(c.r, c.g, c.b, 1)

                // Build points: evenly spaced x, y = height - bar amplitude
                const pts = []
                for (let i = 0; i < n; i++) {
                    pts.push({
                        x: (i / (n - 1)) * width,
                        y: height - (bars[i] / 100) * height
                    })
                }

                // Draw smooth filled wave using cubic bezier curves
                ctx.beginPath()
                ctx.moveTo(pts[0].x, pts[0].y)
                for (let i = 0; i < pts.length - 1; i++) {
                    const cx = (pts[i].x + pts[i + 1].x) / 2
                    ctx.bezierCurveTo(cx, pts[i].y, cx, pts[i + 1].y, pts[i + 1].x, pts[i + 1].y)
                }
                ctx.lineTo(width, height)
                ctx.lineTo(0, height)
                ctx.closePath()
                ctx.fill()
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            // ── Album art thumbnail ──
            Rectangle {
                Layout.preferredWidth: 72
                Layout.preferredHeight: 72
                radius: 12
                color: Qt.rgba(0, 0, 0, 0.25)
                clip: true

                Image {
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    source: root.artUrl
                    visible: root.artUrl !== ""
                    asynchronous: true
                    cache: false
                }

                MaterialSymbol {
                    anchors.centerIn: parent
                    icon: "music_note"
                    iconSize: 24
                    color: col.onSurfaceVariant
                    visible: root.artUrl === ""
                }
            }

            // ── Right column ──
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4

                // ── Marquee title ──
                Item {
                    id: marqueeItem
                    Layout.fillWidth: true
                    height: titleMain.implicitHeight
                    clip: true

                    readonly property string fullText: root.activePlayer
                        ? (root.activeTrack.title || "Unknown title")
                        : "No media player"
                    readonly property bool overflows: titleMain.implicitWidth > marqueeItem.width

                    // Reset + restart when text or play state changes
                    onFullTextChanged: {
                        titleMain.x = 0
                        marqueeAnim.restart()
                    }

                    Text {
                        id: titleMain
                        text: marqueeItem.fullText
                        font.family: cfg ? cfg.fontFamily : "Rubik"
                        font.pixelSize: 13
                        font.weight: 700
                        color: root.hasArt ? root.textColor : col.onSurface
                        Behavior on color { ColorAnimation { duration: 400 } }
                    }

                    // Seamless second copy for looping
                    Text {
                        id: titleCopy
                        text: "   •   " + marqueeItem.fullText
                        font.family: cfg ? cfg.fontFamily : "Rubik"
                        font.pixelSize: 13
                        font.weight: 700
                        color: root.hasArt ? root.textColor : col.onSurface
                        visible: marqueeItem.overflows
                        x: titleMain.x + titleMain.implicitWidth
                        Behavior on color { ColorAnimation { duration: 400 } }
                    }

                    SequentialAnimation {
                        id: marqueeAnim
                        running: marqueeItem.overflows && MprisService.isPlaying
                        loops: Animation.Infinite

                        PauseAnimation { duration: 1400 }
                        NumberAnimation {
                            target: titleMain
                            property: "x"
                            to: -(titleMain.implicitWidth + 24)
                            duration: titleMain.implicitWidth * 24
                            easing.type: Easing.Linear
                        }
                        ScriptAction { script: titleMain.x = 0 }
                    }
                }

                // Artist / source
                Text {
                    Layout.fillWidth: true
                    text: root.activePlayer
                        ? (root.activeTrack.artist || root.activePlayer.identity || "Unknown")
                        : "Start playback in any MPRIS app"
                    font.family: cfg ? cfg.fontFamily : "Rubik"
                    font.pixelSize: 12
                    color: root.hasArt ? root.subTextColor : col.onSurfaceVariant
                    elide: Text.ElideRight
                    Behavior on color { ColorAnimation { duration: 400 } }
                }

                Item { Layout.fillHeight: true }

                // ── Wave progress slider ──
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    visible: root.hasProgressTrack

                    Item {
                        id: waveSlider
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        property real trackInset: 10
                        readonly property real usableWidth: Math.max(1, width - trackInset * 2)
                        property real internalProgress: {
                            if (!root.hasProgressTrack || !root.activePlayer || root.effectiveLength <= 0) return 0
                            return Math.max(0, Math.min(1, root.effectivePosition / root.effectiveLength))
                        }
                        property real progress: seekArea.pressed ? seekArea.dragProgress : internalProgress
                        property real phase: 0

                        function setFromX(xPos) {
                            const localX = Math.max(0, Math.min(usableWidth, xPos - trackInset))
                            const p = Math.max(0, Math.min(1, localX / usableWidth))
                            seekArea.dragProgress = p
                        }

                        Timer {
                            interval: 33
                            repeat: true
                            running: root.activePlayer && root.activePlayer.isPlaying
                            onTriggered: { waveSlider.phase += 0.22; activeWave.requestPaint() }
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: height / 2
                            color: Qt.rgba(0, 0, 0, 0.20)

                            // Flat unplayed section
                            Rectangle {
                                x: waveSlider.trackInset + waveSlider.usableWidth * waveSlider.progress
                                width: Math.max(0, waveSlider.usableWidth * (1 - waveSlider.progress))
                                anchors.verticalCenter: parent.verticalCenter
                                height: 2; radius: 1
                                color: root.hasArt ? root.textColor : col.onSurfaceVariant
                                opacity: 0.4
                            }

                            // Sine wave played section
                            Item {
                                x: waveSlider.trackInset
                                width: Math.max(0, waveSlider.usableWidth * waveSlider.progress)
                                height: parent.height
                                clip: true

                                Canvas {
                                    id: activeWave
                                    anchors.fill: parent
                                    onPaint: {
                                        const ctx = getContext("2d")
                                        ctx.clearRect(0, 0, width, height)
                                        ctx.lineWidth = 2.2
                                        ctx.strokeStyle = root.hasArt
                                            ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.9)
                                            : col.primary
                                        ctx.beginPath()
                                        for (let x = 0; x <= width; x += 2) {
                                            const window = Math.sin(Math.PI * x / width)
                                            const y = height / 2 + Math.sin((x / 3.5) + waveSlider.phase) * (height * 0.12) * window
                                            if (x === 0) ctx.moveTo(x, y)
                                            else ctx.lineTo(x, y)
                                        }
                                        ctx.stroke()
                                    }
                                }
                            }

                            // Thumb
                            Rectangle {
                                width: 11; height: 11; radius: 5.5
                                x: Math.max(
                                    waveSlider.trackInset - width / 2,
                                    Math.min(
                                        waveSlider.trackInset + waveSlider.usableWidth - width / 2,
                                        waveSlider.trackInset + waveSlider.usableWidth * waveSlider.progress - width / 2))
                                y: parent.height / 2 - height / 2
                                color: root.hasArt ? root.textColor : col.primary
                                border.width: 2
                                border.color: root.hasArt
                                    ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.4)
                                    : col.onPrimary
                            }
                        }

                        MouseArea {
                            id: seekArea
                            anchors.fill: parent
                            enabled: root.hasProgressTrack
                            hoverEnabled: true
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            property real dragProgress: 0
                            onPressed: mouse => {
                                root.seeking = true
                                waveSlider.setFromX(mouse.x)
                            }
                            onPositionChanged: mouse => {
                                if (pressed)
                                    waveSlider.setFromX(mouse.x)
                            }
                            onReleased: {
                                MprisService.seekToProgress(dragProgress)
                                root.seeking = false
                            }
                            onCanceled: root.seeking = false
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: root.formatTime(root.effectivePosition)
                            font.family: cfg ? cfg.fontFamily : "Rubik"
                            font.pixelSize: 10
                            color: root.hasArt ? root.subTextColor : col.onSurfaceVariant
                            opacity: 0.85
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: root.formatTime(root.effectiveLength)
                            font.family: cfg ? cfg.fontFamily : "Rubik"
                            font.pixelSize: 10
                            color: root.hasArt ? root.subTextColor : col.onSurfaceVariant
                            opacity: 0.85
                        }
                    }
                }

                // ── Playback controls ──
                Rectangle {
                    Layout.alignment: Qt.AlignLeft
                    Layout.preferredWidth: controlsRow.implicitWidth + 12
                    Layout.preferredHeight: 44
                    radius: 22
                    color: root.hasArt
                        ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.10)
                        : col.surfaceContainerHigh
                    border.width: 1
                    border.color: root.hasArt
                        ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.14)
                        : col.outlineVariant

                    RowLayout {
                        id: controlsRow
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 8

                        Rectangle {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            radius: 17
                            color: prevHover.containsMouse
                                ? (root.hasArt ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.24) : col.secondaryContainer)
                                : (root.hasArt ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.14) : col.surfaceContainerHighest)
                            visible: MprisService.canGoPrevious
                            Behavior on color { ColorAnimation { duration: Gstate.animDuration } }
                            MaterialSymbol {
                                anchors.centerIn: parent
                                icon: "skip_previous"
                                iconSize: 19
                                color: root.hasArt ? root.textColor : col.onSecondaryContainer
                                Behavior on color { ColorAnimation { duration: Gstate.animDuration } }
                            }
                            MouseArea {
                                id: prevHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: MprisService.previous()
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 38
                            Layout.preferredHeight: 38
                            radius: 19
                            color: playHover.containsMouse
                                ? (root.hasArt ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.92) : col.primary)
                                : (root.hasArt ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.82) : col.primaryContainer)
                            visible: MprisService.canTogglePlaying
                            scale: playHover.containsMouse ? 1.04 : 1.0
                            Behavior on color { ColorAnimation { duration: Gstate.animDuration } }
                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                            MaterialSymbol {
                                anchors.centerIn: parent
                                icon: root.activePlayer && root.activePlayer.isPlaying ? "pause" : "play_arrow"
                                iconSize: 22
                                color: root.hasArt ? dominantColor.color : col.onPrimaryContainer
                                Behavior on color { ColorAnimation { duration: Gstate.animDuration } }
                            }
                            MouseArea {
                                id: playHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: MprisService.togglePlaying()
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            radius: 17
                            color: nextHover.containsMouse
                                ? (root.hasArt ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.24) : col.secondaryContainer)
                                : (root.hasArt ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.14) : col.surfaceContainerHighest)
                            visible: MprisService.canGoNext
                            Behavior on color { ColorAnimation { duration: Gstate.animDuration } }
                            MaterialSymbol {
                                anchors.centerIn: parent
                                icon: "skip_next"
                                iconSize: 19
                                color: root.hasArt ? root.textColor : col.onSecondaryContainer
                                Behavior on color { ColorAnimation { duration: Gstate.animDuration } }
                            }
                            MouseArea {
                                id: nextHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: MprisService.next()
                            }
                        }
                    }
                }
            }
        }
    }
}
