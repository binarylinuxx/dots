import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Widgets
import Quickshell.Services.Mpris
import qs.widgets

Item {
    id: root

    implicitHeight: 160

    property bool seeking: false
    readonly property bool canSeekTrack: activePlayer
        && activePlayer.canSeek
        && activePlayer.positionSupported
        && activePlayer.lengthSupported
        && activePlayer.length > 0

    readonly property var activePlayer: {
        const players = Mpris.players && Mpris.players.values ? Mpris.players.values : []
        if (!players || players.length === 0) return null
        for (let i = 0; i < players.length; ++i)
            if (players[i] && players[i].isPlaying) return players[i]
        return players[0]
    }

    readonly property string artUrl: activePlayer ? (activePlayer.trackArtUrl || "") : ""
    readonly property bool hasArt: artUrl !== ""

    function formatTime(seconds) {
        const s = Math.max(0, Math.floor(seconds || 0))
        const m = Math.floor(s / 60)
        const r = s % 60
        return m + ":" + (r < 10 ? "0" : "") + r
    }

    // ── Position refresh timer ──
    Timer {
        interval: 1000
        repeat: true
        running: root.activePlayer && root.activePlayer.isPlaying && root.canSeekTrack && !root.seeking
        onTriggered: {
            if (root.activePlayer && root.activePlayer.positionChanged)
                root.activePlayer.positionChanged()
        }
    }

    // ── Dominant color extraction ──
    ColorQuantizer {
        id: quantizer
        source: root.artUrl
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
        clip: true
        Behavior on color { ColorAnimation { duration: 600; easing.type: Easing.OutCubic } }

        // Full-bleed blurred art background
        Image {
            id: artBg
            anchors.fill: parent
            source: root.artUrl
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            visible: root.hasArt
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
                    visible: root.hasArt
                    asynchronous: true
                }

                MaterialSymbol {
                    anchors.centerIn: parent
                    icon: "music_note"
                    iconSize: 24
                    color: col.onSurfaceVariant
                    visible: !root.hasArt
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
                        ? (root.activePlayer.trackTitle || "Unknown title")
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
                        running: marqueeItem.overflows && root.activePlayer && root.activePlayer.isPlaying
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
                        ? (root.activePlayer.trackArtist || root.activePlayer.identity || "Unknown")
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
                    visible: root.canSeekTrack

                    Item {
                        id: waveSlider
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        property real trackInset: 10
                        readonly property real usableWidth: Math.max(1, width - trackInset * 2)
                        property real internalProgress: {
                            if (!root.canSeekTrack || !root.activePlayer || root.activePlayer.length <= 0) return 0
                            return Math.max(0, Math.min(1, root.activePlayer.position / root.activePlayer.length))
                        }
                        property real progress: seekArea.pressed ? seekArea.dragProgress : internalProgress
                        property real phase: 0

                        function setFromX(xPos) {
                            const localX = Math.max(0, Math.min(usableWidth, xPos - trackInset))
                            const p = Math.max(0, Math.min(1, localX / usableWidth))
                            seekArea.dragProgress = p
                            if (root.activePlayer && root.canSeekTrack)
                                root.activePlayer.position = Math.floor((root.activePlayer.length || 0) * p)
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
                                            const y = height / 2 + Math.sin((x / 10) + waveSlider.phase) * (height * 0.2)
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
                            enabled: root.canSeekTrack
                            hoverEnabled: true
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            property real dragProgress: 0
                            onPressed: { root.seeking = true; waveSlider.setFromX(mouse.x) }
                            onPositionChanged: if (pressed) waveSlider.setFromX(mouse.x)
                            onReleased: root.seeking = false
                            onCanceled: root.seeking = false
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: root.formatTime(root.activePlayer ? root.activePlayer.position : 0)
                            font.family: cfg ? cfg.fontFamily : "Rubik"
                            font.pixelSize: 10
                            color: root.hasArt ? root.subTextColor : col.onSurfaceVariant
                            opacity: 0.85
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: root.formatTime(root.activePlayer ? root.activePlayer.length : 0)
                            font.family: cfg ? cfg.fontFamily : "Rubik"
                            font.pixelSize: 10
                            color: root.hasArt ? root.subTextColor : col.onSurfaceVariant
                            opacity: 0.85
                        }
                    }
                }

                // ── Playback controls ──
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        Layout.preferredWidth: 30; Layout.preferredHeight: 30; radius: 15
                        color: prevHover.containsMouse ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.18) : "transparent"
                        visible: root.activePlayer && root.activePlayer.canGoPrevious
                        MaterialSymbol {
                            anchors.centerIn: parent; icon: "skip_previous"; iconSize: 18
                            color: root.hasArt ? root.textColor : col.onSurfaceVariant
                            Behavior on color { ColorAnimation { duration: 300 } }
                        }
                        MouseArea {
                            id: prevHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: if (root.activePlayer && root.activePlayer.canGoPrevious) root.activePlayer.previous()
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 36; Layout.preferredHeight: 36; radius: 18
                        color: playHover.containsMouse
                            ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.28)
                            : Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.15)
                        visible: root.activePlayer && root.activePlayer.canTogglePlaying
                        MaterialSymbol {
                            anchors.centerIn: parent
                            icon: root.activePlayer && root.activePlayer.isPlaying ? "pause" : "play_arrow"
                            iconSize: 20
                            color: root.hasArt ? root.textColor : col.onSurface
                            Behavior on color { ColorAnimation { duration: 300 } }
                        }
                        MouseArea {
                            id: playHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: if (root.activePlayer && root.activePlayer.canTogglePlaying) root.activePlayer.togglePlaying()
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 30; Layout.preferredHeight: 30; radius: 15
                        color: nextHover.containsMouse ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.18) : "transparent"
                        visible: root.activePlayer && root.activePlayer.canGoNext
                        MaterialSymbol {
                            anchors.centerIn: parent; icon: "skip_next"; iconSize: 18
                            color: root.hasArt ? root.textColor : col.onSurfaceVariant
                            Behavior on color { ColorAnimation { duration: 300 } }
                        }
                        MouseArea {
                            id: nextHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: if (root.activePlayer && root.activePlayer.canGoNext) root.activePlayer.next()
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }
    }
}
