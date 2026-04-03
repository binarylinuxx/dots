pragma Singleton
import Quickshell
import Quickshell.Io
import QtQuick

QtObject {
    id: root

    // ── Signals — handled in shell.qml where lock/Gstate are in scope ─────
    signal requestLock()
    signal requestDpmsOff()
    signal requestDpmsOn()
    signal requestSuspend()
    signal requestCleanup()   // fired on real unlock so shell.qml can restore DPMS

    // ── Config — bound from shell.qml via Binding{} ────────────────────────
    property bool enabled:              true
    property int  timeout:              300
    property bool inhibitWhenRecording: true
    property bool dpmsEnabled:          true
    property int  dpmsDelay:            300
    property bool suspendEnabled:       true
    property int  suspendDelay:         600

    // ── Internal state ─────────────────────────────────────────────────────
    property bool _idle:      false
    property bool _dpmsOff:   false
    property bool _suspended: false
    property bool _locked:    false   // set directly by shell.qml onLockedChanged, no Binding lag

    // ── Timers ─────────────────────────────────────────────────────────────
    property var _dpmsTimer: Timer {
        id: dpmsTimer
        interval: root.dpmsDelay * 1000
        repeat: false
        running: false
        onTriggered: {
            if (!root._idle) return
            if (!root.dpmsEnabled) {
                console.log("[IdleService] DPMS skipped — disabled in settings")
                return
            }
            if (root._locked) {
                console.log("[IdleService] DPMS skipped — screen is locked, unsafe to blank")
                return
            }
            console.log("[IdleService] DPMS off — blanking display")
            root._dpmsOff = true
            requestDpmsOff()
        }
    }

    property var _suspendTimer: Timer {
        id: suspendTimer
        interval: root.suspendDelay * 1000
        repeat: false
        running: false
        onTriggered: {
            if (!root._idle) return
            if (!root.suspendEnabled) {
                console.log("[IdleService] Suspend skipped — disabled in settings")
                return
            }
            console.log("[IdleService] Suspending system")
            root._suspended = true
            requestSuspend()
        }
    }

    // ── Public API ─────────────────────────────────────────────────────────
    function onIdle() {
        if (!enabled) {
            console.log("[IdleService] Idle detected — disabled in settings, skipping")
            return
        }
        if (inhibitWhenRecording && ScreenRecorder.recording) {
            console.log("[IdleService] Idle detected — recording active, inhibited")
            return
        }

        var seq = "lock"
        if (dpmsEnabled)    seq += " → dpms off in " + dpmsDelay + "s"
        if (suspendEnabled) seq += " → suspend in " + suspendDelay + "s"
        console.log("[IdleService] Idle detected (timeout=" + timeout + "s) — sequence: " + seq)

        _idle = true
        _dpmsOff = false
        _suspended = false

        console.log("[IdleService] Emitting requestLock")
        requestLock()

        if (dpmsEnabled)    dpmsTimer.restart()
        if (suspendEnabled) suspendTimer.restart()
    }

    // Called when IdleMonitor.isIdle goes false — but we IGNORE this if we
    // locked the screen, because the compositor resets idle on new surfaces.
    // Real cleanup happens in onUnlocked() called from shell.qml.
    function onResume() {
        if (!_idle) return
        // Always ignore — let onUnlocked handle cleanup
        console.log("[IdleService] isIdle reset (compositor timer reset by lock surface) — ignoring")
    }

    // Called from shell.qml Connections{ target: lock } when lock.locked → false
    function onUnlocked() {
        if (!_idle) return
        console.log("[IdleService] Screen unlocked — cleaning up idle state, dpmsOff:", _dpmsOff)

        _idle = false
        dpmsTimer.stop()
        suspendTimer.stop()

        if (_dpmsOff) {
            console.log("[IdleService] Requesting DPMS restore")
            requestDpmsOn()
            _dpmsOff = false
        }

        _suspended = false
    }
}
