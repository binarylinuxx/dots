pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io

Singleton {
    id: root

    // List of reminder objects: { id, title, date (YYYY-MM-DD), time (HH:MM), fired }
    property var reminders: []

    readonly property string filePath: "/home/blx/.config/quickshell/reminders.json"

    // ── Load ──
    FileView {
        id: fileView
        path: root.filePath
        watchChanges: false
        onLoaded: {
            try {
                const parsed = JSON.parse(fileView.text())
                if (Array.isArray(parsed)) root.reminders = parsed
            } catch (e) {
                root.reminders = []
            }
        }
    }

    // ── Save process ──
    Process {
        id: saveProcess
        command: []
        running: false
    }

    // ── Notify process ──
    Process {
        id: notifyProcess
        command: []
        running: false
    }

    // ── Check timer — every 30 seconds ──
    Timer {
        interval: 30000
        repeat: true
        running: true
        triggeredOnStart: true
        onTriggered: root.checkReminders()
    }

    function checkReminders() {
        const now = new Date()
        const todayStr = _dateStr(now)
        const timeStr = _timeStr(now)
        let changed = false

        const updated = root.reminders.map(r => {
            if (r.fired) return r
            if (r.date === todayStr && r.time <= timeStr) {
                _notify(r.title, r.date, r.time)
                changed = true
                return Object.assign({}, r, { fired: true })
            }
            return r
        })

        if (changed) {
            root.reminders = updated
            _save()
        }
    }

    function addReminder(title, date, time) {
        const r = {
            id: Date.now().toString(),
            title: title,
            date: date,   // YYYY-MM-DD
            time: time,   // HH:MM
            fired: false
        }
        root.reminders = root.reminders.concat([r])
        _save()
    }

    function removeReminder(id) {
        root.reminders = root.reminders.filter(r => r.id !== id)
        _save()
    }

    // Returns list of reminders for a given YYYY-MM-DD date string
    function remindersForDate(dateStr) {
        return root.reminders.filter(r => r.date === dateStr && !r.fired)
    }

    // ── Internals ──
    function _dateStr(d) {
        const y = d.getFullYear()
        const m = String(d.getMonth() + 1).padStart(2, "0")
        const day = String(d.getDate()).padStart(2, "0")
        return y + "-" + m + "-" + day
    }

    function _timeStr(d) {
        return String(d.getHours()).padStart(2, "0") + ":" + String(d.getMinutes()).padStart(2, "0")
    }

    function _notify(title, date, time) {
        notifyProcess.command = [
            "gdbus", "call",
            "--session",
            "--dest", "org.freedesktop.Notifications",
            "--object-path", "/org/freedesktop/Notifications",
            "--method", "org.freedesktop.Notifications.Notify",
            "Reminders", "0", "appointment-soon",
            "Reminder: " + title,
            date + " at " + time,
            "[]", "{}", "8000"
        ]
        notifyProcess.running = true
    }

    function _save() {
        const json = JSON.stringify(root.reminders, null, 2)
        saveProcess.command = ["sh", "-c", "printf '%s' '" + json.replace(/'/g, "'\\''") + "' > " + root.filePath]
        saveProcess.running = true
    }

    Component.onCompleted: {
        fileView.reload()
    }
}
