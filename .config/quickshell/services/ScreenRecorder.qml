pragma Singleton
import Quickshell
import Quickshell.Io
import QtQuick

QtObject {
    id: root

    // ── State ──────────────────────────────────────────────────────────────
    property bool recording: false
    property int  elapsedSeconds: 0
    property string lastSavedPath: ""
    property string errorMessage: ""

    // Formatted elapsed time  e.g. "02:34"
    readonly property string elapsedFormatted: {
        var m = Math.floor(elapsedSeconds / 60)
        var s = elapsedSeconds % 60
        return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
    }

    // ── Config — bound from shell.qml via Binding{} (cfg not accessible in singletons) ──
    property string monitor:      "HDMI-A-1"
    property int    fps:          60
    property string quality:      "very_high"   // very_high | high | medium | low
    property string codec:        "av1"          // h264 | hevc | av1
    property string audioDevice:  "default_output"
    property string outputDir:    ""             // empty = ~/Videos

    function resolvedOutputDir() {
        var home = Quickshell.env("HOME") || "/root"
        return (outputDir !== "") ? outputDir : (home + "/Videos")
    }

    // ── GSR process ────────────────────────────────────────────────────────
    property var _gsrProcess: Process {
        id: gsrProcess

        stderr: StdioCollector {
            onStreamFinished: {
                // GSR writes status lines to stderr; check for save confirmation
                var lines = text.split("\n")
                for (var i = 0; i < lines.length; i++) {
                    var line = lines[i]
                    // "gsr info: saved recording to /path/file.mp4"
                    if (line.indexOf("saved") !== -1 && line.indexOf("/") !== -1) {
                        var parts = line.split(" ")
                        root.lastSavedPath = parts[parts.length - 1].trim()
                    }
                    if (line.indexOf("error") !== -1 || line.indexOf("Error") !== -1) {
                        root.errorMessage = line
                    }
                }
            }
        }

        onRunningChanged: {
            if (!running && root.recording) {
                // Process died unexpectedly
                root.recording = false
                elapsedTimer.stop()
                root.elapsedSeconds = 0
            }
        }
    }

    // ── Stop signal process (sends SIGINT to GSR to finalize file) ─────────
    property var _stopProcess: Process {
        id: stopProcess
    }

    // ── Elapsed timer ──────────────────────────────────────────────────────
    property var _elapsedTimer: Timer {
        id: elapsedTimer
        interval: 1000
        repeat: true
        running: false
        onTriggered: root.elapsedSeconds++
    }

    // ── Public API ─────────────────────────────────────────────────────────
    function startRecording() {
        if (recording) return

        errorMessage = ""
        lastSavedPath = ""

        var mon    = monitor
        var f      = fps
        var q      = quality
        var k      = codec
        var audio  = audioDevice
        var outDir = resolvedOutputDir()

        // Build filename: Recording_YYYY-MM-DD_HH-MM-SS.mp4
        var now = new Date()
        var ts  = now.getFullYear() + "-" +
                  String(now.getMonth()+1).padStart(2,"0") + "-" +
                  String(now.getDate()).padStart(2,"0") + "_" +
                  String(now.getHours()).padStart(2,"0") + "-" +
                  String(now.getMinutes()).padStart(2,"0") + "-" +
                  String(now.getSeconds()).padStart(2,"0")
        var outFile = outDir + "/Recording_" + ts + ".mp4"
        lastSavedPath = outFile

        gsrProcess.command = [
            "gpu-screen-recorder",
            "-w", mon,
            "-f", String(f),
            "-q", q,
            "-k", k,
            "-a", audio,
            "-o", outFile
        ]
        gsrProcess.running = true
        recording = true
        elapsedSeconds = 0
        elapsedTimer.start()
    }

    function stopRecording() {
        if (!recording) return

        // Send SIGINT so GSR finalizes and writes the file cleanly
        stopProcess.command = ["sh", "-c", "pkill -INT -f gpu-screen-recorder 2>/dev/null; true"]
        stopProcess.running = true

        recording = false
        elapsedTimer.stop()
    }

    function toggleRecording() {
        if (recording) stopRecording()
        else startRecording()
    }
}
