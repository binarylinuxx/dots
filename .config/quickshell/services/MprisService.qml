pragma Singleton
pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Services.Mpris

Singleton {
    id: root

    property MprisPlayer trackedPlayer: null
    readonly property list<MprisPlayer> players: filterDuplicatePlayers(Mpris.players.values.filter(player => isRealPlayer(player)))
    readonly property MprisPlayer activePlayer: trackedPlayer && players.indexOf(trackedPlayer) !== -1 ? trackedPlayer : (players[0] || null)

    property real playerctlFallbackPosition: 0
    property real pendingSeekPosition: 0
    property string playerctlUrl: ""
    property string playerctlTitle: ""
    property string playerctlArtist: ""
    property string playerctlArtUrl: ""
    property real playerctlFallbackLength: 0
    property string playerctlStatus: ""
    property string artDownloadTargetUrl: ""
    property bool artDownloaded: false
    property string displayedArtSource: ""
    property int artWidth: 0
    property int artHeight: 0
    property bool serviceReady: false
    property var activeTrack: ({
        uniqueId: 0,
        artUrl: "",
        title: "",
        artist: "",
        album: ""
    })

    readonly property bool hasActivePlasmaIntegration: Mpris.players.values.some(p => p.dbusName && p.dbusName.startsWith("org.mpris.MediaPlayer2.plasma-browser-integration"))

    readonly property real effectiveLength: activePlayer && activePlayer.lengthSupported && activePlayer.length > 0
        ? activePlayer.length
        : playerctlFallbackLength
    readonly property real effectivePosition: playerctlFallbackPosition > 0
        ? playerctlFallbackPosition
        : (activePlayer && activePlayer.positionSupported && activePlayer.position > 0 ? activePlayer.position : 0)
    readonly property bool hasProgressTrack: activePlayer && effectiveLength > 0
    readonly property bool canSeekTrack: hasProgressTrack && activePlayer.canSeek

    readonly property string trackArtUrl: activePlayer ? (activePlayer.trackArtUrl || "") : ""
    readonly property string mprisUrl: activePlayer && activePlayer.metadata ? (activePlayer.metadata["xesam:url"] || "") : ""
    readonly property string mediaUrl: playerctlUrl || mprisUrl
    readonly property string rawArtUrl: playerctlArtUrl || trackArtUrl
    readonly property bool rawArtIsRemote: rawArtUrl.indexOf("http://") === 0 || rawArtUrl.indexOf("https://") === 0
    readonly property string artFilePath: rawArtUrl ? "/tmp/blxshell-mpris-art-" + Qt.md5(rawArtUrl) + ".jpg" : ""
    readonly property string downloadedArtSource: artDownloaded && artDownloadTargetUrl === rawArtUrl && artFilePath ? normalizeImageSource(artFilePath) : ""
    readonly property string playerIconSource: activePlayer && activePlayer.desktopEntry ? ("image://icon/" + activePlayer.desktopEntry) : ""
    readonly property string realArtUrl: displayedArtSource
    readonly property string artUrl: realArtUrl || playerIconSource
    readonly property bool hasArt: realArtUrl !== ""
    readonly property string playerctlName: activePlayer && activePlayer.dbusName
        ? activePlayer.dbusName.replace("org.mpris.MediaPlayer2.", "")
        : ""
    readonly property string preferredPlayerctlName: Mpris.players.values.some(p => p.dbusName && p.dbusName.startsWith("org.mpris.MediaPlayer2.plasma-browser-integration"))
        ? "plasma-browser-integration"
        : playerctlName

    readonly property bool isPlaying: playerctlStatus ? playerctlStatus === "Playing" : (activePlayer && activePlayer.isPlaying)
    readonly property bool canTogglePlaying: activePlayer && activePlayer.canTogglePlaying
    readonly property bool canGoPrevious: activePlayer && activePlayer.canGoPrevious
    readonly property bool canGoNext: activePlayer && activePlayer.canGoNext

    signal trackChanged(bool reverse)

    property bool reverseTrackChange: false

    function isRealPlayer(player) {
        if (!player)
            return false

        return !(hasActivePlasmaIntegration && player.dbusName && player.dbusName.startsWith("org.mpris.MediaPlayer2.firefox"))
            && !(hasActivePlasmaIntegration && player.dbusName && player.dbusName.startsWith("org.mpris.MediaPlayer2.chromium"))
            && !(player.dbusName && player.dbusName.startsWith("org.mpris.MediaPlayer2.playerctld"))
            && !(player.dbusName && player.dbusName.endsWith(".mpd") && !player.dbusName.endsWith("MediaPlayer2.mpd"))
    }

    function choosePlayerGroup(players, group) {
        for (let i = 0; i < group.length; ++i) {
            const player = players[group[i]]
            if (player && player.trackArtUrl && player.trackArtUrl.length > 0)
                return player
        }
        for (let i = 0; i < group.length; ++i) {
            const player = players[group[i]]
            if (player && player.isPlaying)
                return player
        }
        return players[group[0]]
    }

    function filterDuplicatePlayers(players) {
        const filtered = []
        const used = new Set()

        for (let i = 0; i < players.length; ++i) {
            if (used.has(i))
                continue

            const first = players[i]
            const group = [i]
            for (let j = i + 1; j < players.length; ++j) {
                const other = players[j]
                const similarTitle = first.trackTitle && other.trackTitle
                    && (first.trackTitle.indexOf(other.trackTitle) !== -1 || other.trackTitle.indexOf(first.trackTitle) !== -1)
                const similarTiming = first.length > 0 && other.length > 0
                    && Math.abs(first.position - other.position) <= 2
                    && Math.abs(first.length - other.length) <= 2

                if (similarTitle || similarTiming)
                    group.push(j)
            }

            filtered.push(choosePlayerGroup(players, group))
            group.forEach(idx => used.add(idx))
        }

        return filtered
    }

    function normalizeImageSource(source) {
        if (!source)
            return ""
        if (source.charAt(0) === "/")
            return "file://" + source
        return source
    }

    function shellQuote(value) {
        return "'" + String(value).replace(/'/g, "'\\''") + "'"
    }

    function downloadArtwork() {
        if (!rawArtUrl) {
            artDownloaded = false
            artDownloadTargetUrl = ""
            displayedArtSource = ""
            artWidth = 0
            artHeight = 0
            updateTrack()
            return
        }

        if (artDownloaded && artDownloadTargetUrl === rawArtUrl)
            return
        if (artDownloadProcess.running)
            return

        artDownloaded = false
        artWidth = 0
        artHeight = 0
        artDownloadTargetUrl = rawArtUrl

        const cachePath = shellQuote(artFilePath)
        if (!rawArtIsRemote) {
            const src = shellQuote(rawArtUrl.replace(/^file:\/\//, ""))
            artDownloadProcess.command = ["bash", "-c", "mkdir -p $(dirname " + cachePath + "); cp " + src + " " + cachePath + "; file " + cachePath]
        } else {
            artDownloadProcess.command = ["bash", "-c", "mkdir -p $(dirname " + cachePath + "); [ -f " + cachePath + " ] || curl -4 -sSL " + shellQuote(rawArtUrl) + " -o " + cachePath + "; file " + cachePath]
        }
        artDownloadProcess.running = true
    }

    function updateTrack() {
        activeTrack = {
            uniqueId: activePlayer ? activePlayer.uniqueId : 0,
            artUrl: realArtUrl,
            title: playerctlTitle || (activePlayer ? (activePlayer.trackTitle || "Unknown Title") : ""),
            artist: playerctlArtist || (activePlayer ? (activePlayer.trackArtist || "Unknown Artist") : ""),
            album: activePlayer ? (activePlayer.trackAlbum || "Unknown Album") : ""
        }
    }

    function parseMprisTime(value) {
        const parsed = parseFloat(value)
        if (isNaN(parsed) || parsed <= 0)
            return 0
        return parsed > 100000 ? parsed / 1000000 : parsed
    }

    function fetchPlayerctlMetadata() {
        if (!serviceReady || playerctlMetadataProcess.running)
            return

        playerctlMetadataProcess.command = preferredPlayerctlName
            ? ["playerctl", "-p", preferredPlayerctlName, "metadata", "--format", "{{title}}\u001f{{artist}}\u001f{{xesam:url}}\u001f{{mpris:artUrl}}\u001f{{position}}\u001f{{mpris:length}}\u001f{{status}}"]
            : ["playerctl", "metadata", "--format", "{{title}}\u001f{{artist}}\u001f{{xesam:url}}\u001f{{mpris:artUrl}}\u001f{{position}}\u001f{{mpris:length}}\u001f{{status}}"]
        playerctlMetadataProcess.running = true
    }

    function applyPlayerctlMetadata(text) {
        const parts = text.trim().split("\u001f")
        if (parts.length < 7)
            return

        const oldUrl = playerctlUrl
        const oldArtUrl = playerctlArtUrl
        playerctlTitle = parts[0]
        playerctlArtist = parts[1]
        playerctlUrl = parts[2]
        playerctlArtUrl = parts[3]

        playerctlFallbackPosition = parseMprisTime(parts[4])

        playerctlFallbackLength = parseMprisTime(parts[5])
        playerctlStatus = parts[6]
        if (playerctlUrl !== oldUrl || playerctlArtUrl !== oldArtUrl)
            downloadArtwork()
        updateTrack()
    }

    function fetchPlayerctlPosition() {
        fetchPlayerctlMetadata()
    }

    function seekToProgress(progress) {
        if (!activePlayer || effectiveLength <= 0)
            return

        pendingSeekPosition = Math.floor(effectiveLength * Math.max(0, Math.min(1, progress)))
        if (canSeekTrack) {
            activePlayer.position = pendingSeekPosition
            return
        }
        if (playerctlSeekProcess.running)
            return

        playerctlSeekProcess.command = preferredPlayerctlName
            ? ["playerctl", "-p", preferredPlayerctlName, "position", pendingSeekPosition.toString()]
            : ["playerctl", "position", pendingSeekPosition.toString()]
        playerctlSeekProcess.running = true
        playerctlFallbackPosition = pendingSeekPosition
    }

    function togglePlaying() {
        if (canTogglePlaying)
            activePlayer.togglePlaying()
    }

    function previous() {
        if (canGoPrevious) {
            reverseTrackChange = true
            activePlayer.previous()
        }
    }

    function next() {
        if (canGoNext) {
            reverseTrackChange = false
            activePlayer.next()
        }
    }

    function setActivePlayer(player) {
        trackedPlayer = player || players[0] || null
    }

    onTrackArtUrlChanged: {
        const reverse = reverseTrackChange
        downloadArtwork()
        updateTrack()
        reverseTrackChange = reverse
    }
    onRawArtUrlChanged: downloadArtwork()

    onActivePlayerChanged: {
        fetchPlayerctlMetadata()
        downloadArtwork()
        updateTrack()
        trackChanged(reverseTrackChange)
        reverseTrackChange = false
    }

    Component.onCompleted: {
        serviceReady = true
        fetchPlayerctlMetadata()
        downloadArtwork()
        updateTrack()
    }

    Instantiator {
        model: Mpris.players

        Connections {
            required property MprisPlayer modelData
            target: modelData

            Component.onCompleted: {
                if (!root.trackedPlayer || modelData.isPlaying)
                    root.trackedPlayer = modelData
            }

            Component.onDestruction: {
                if (!root.trackedPlayer || !root.trackedPlayer.isPlaying)
                    root.trackedPlayer = root.players.find(player => player && player.isPlaying) || root.players[0] || null
            }

            function onPlaybackStateChanged() {
                if (modelData.dbusName && modelData.dbusName.startsWith("org.mpris.MediaPlayer2.plasma-browser-integration")) {
                    root.trackedPlayer = modelData
                    return
                }
                if (modelData.isPlaying && root.trackedPlayer !== modelData)
                    root.trackedPlayer = modelData
            }

            function onPostTrackChanged() {
                if (root.trackedPlayer === modelData) {
                    root.fetchPlayerctlMetadata()
                    root.downloadArtwork()
                    root.updateTrack()
                    root.trackChanged(root.reverseTrackChange)
                }
            }

            function onTrackArtUrlChanged() {
                if (root.trackedPlayer === modelData) {
                    const reverse = root.reverseTrackChange
                    root.downloadArtwork()
                    root.updateTrack()
                    root.reverseTrackChange = reverse
                }
            }

            function onTrackTitleChanged() {
                if (root.trackedPlayer === modelData) {
                    root.fetchPlayerctlMetadata()
                    root.downloadArtwork()
                    root.updateTrack()
                }
            }

            function onTrackArtistChanged() {
                if (root.trackedPlayer === modelData) {
                    root.fetchPlayerctlMetadata()
                    root.downloadArtwork()
                    root.updateTrack()
                }
            }
        }
    }

    Process {
        id: artDownloadProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                const output = text.trim()
                const match = output.match(/(\d+)\s*x\s*(\d+)/)
                root.artDownloaded = !!match && root.artDownloadTargetUrl === root.rawArtUrl
                root.artWidth = match ? Number(match[1]) : 0
                root.artHeight = match ? Number(match[2]) : 0
                if (root.artDownloaded)
                    root.displayedArtSource = root.downloadedArtSource
                else if (root.artDownloadTargetUrl === root.rawArtUrl)
                    root.displayedArtSource = ""
                root.updateTrack()
                if (root.artDownloadTargetUrl !== root.rawArtUrl)
                    root.downloadArtwork()
            }
        }
    }

    Process {
        id: playerctlMetadataProcess
        running: false
        stdout: StdioCollector {
            onStreamFinished: root.applyPlayerctlMetadata(text)
        }
    }

    Process {
        id: playerctlSeekProcess
        running: false
    }

    Timer {
        interval: 1000
        repeat: true
        running: root.activePlayer || root.playerctlUrl !== ""
        triggeredOnStart: true
        onTriggered: root.fetchPlayerctlMetadata()
    }

    Timer {
        interval: 1000
        repeat: true
        running: root.activePlayer && root.activePlayer.isPlaying && root.hasProgressTrack
        onTriggered: {
            if (root.activePlayer && root.activePlayer.positionChanged)
                root.activePlayer.positionChanged()
        }
    }

    IpcHandler {
        target: "mpris"

        function pauseAll(): void {
            for (const player of root.players) {
                if (player.canPause)
                    player.pause()
            }
        }

        function playPause(): void { root.togglePlaying() }
        function previous(): void { root.previous() }
        function next(): void { root.next() }
    }
}
