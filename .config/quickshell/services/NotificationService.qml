pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Services.Notifications

Singleton {
    id: root

    // Do Not Disturb: sourced from Gstate (which is kept in sync with cfg in shell.qml)
    readonly property bool dndEnabled: Gstate.dndEnabled

    property alias trackedNotifications: server.trackedNotifications
    property alias notificationServer: server
    // Live tracked notifications for the popup
    property var notifications: server.trackedNotifications.values

    // Persistent sidebar history — plain JS snapshots, survives expiry
    property var sidebarHistory: []

    // Shared state for popup tracking
    property var knownNotifications: ({})
    property var notificationOrder: []
    property int currentExpiringIndex: -1
    property int defaultTimeout: 5000

    NotificationServer {
        id: server
        keepOnReload: true
        bodySupported: true
        actionsSupported: true
        imageSupported: true
        persistenceSupported: false
        inlineReplySupported: false
        actionIconsSupported: true

        onNotification: function(notification) {
            notification.tracked = !root.dndEnabled

            // Snapshot into sidebar history
            let snap = {
                id: notification.id,
                appName: notification.appName || "",
                summary: notification.summary || "",
                body: notification.body || "",
                appIcon: notification.appIcon || "",
                image: notification.image || "",
                time: new Date()
            }
            // Prepend (newest first), cap at 50
            let newHistory = [snap].concat(root.sidebarHistory)
            if (newHistory.length > 50) newHistory = newHistory.slice(0, 50)
            root.sidebarHistory = newHistory

            if (notification.id) {
                let id = notification.id.toString()
                root.knownNotifications[id] = "NEW_" + Date.now()
                root.notificationOrder.unshift(id)
            }
        }
    }

    onDndEnabledChanged: {
        if (!dndEnabled) return

        // Immediately clear live popup notifications while preserving sidebar history.
        for (let i = 0; i < server.trackedNotifications.values.length; ++i) {
            let n = server.trackedNotifications.values[i]
            if (n) {
                cleanupNotificationTracking(n.id)
                n.dismiss()
            }
        }
        currentExpiringIndex = -1
    }

    function toggleDnd() {
        Gstate.dndEnabled = !Gstate.dndEnabled
    }

    function removeSidebarItem(snapId) {
        root.sidebarHistory = root.sidebarHistory.filter(s => s.id !== snapId)
    }

    function clearSidebarHistory() {
        root.sidebarHistory = []
        // Also dismiss any still-live tracked notifications
        for (let i = 0; i < server.trackedNotifications.values.length; ++i) {
            let n = server.trackedNotifications.values[i]
            if (n) n.dismiss()
        }
    }

    function markAllAsKnown() {
        let currentTime = Date.now()
        notificationOrder = []
        for (let i = 0; i < server.trackedNotifications.values.length; i++) {
            let notification = server.trackedNotifications.values[i]
            if (notification && notification.id) {
                let id = notification.id.toString()
                knownNotifications[id] = currentTime
                notificationOrder.push(id)
            }
        }
    }

    function cleanupNotificationTracking(notificationId) {
        if (notificationId) {
            let id = notificationId.toString()
            delete knownNotifications[id]
            let index = notificationOrder.indexOf(id)
            if (index > -1) notificationOrder.splice(index, 1)
        }
    }

    function canNotificationExpire(notificationId) {
        if (!notificationId) return false
        let id = notificationId.toString()
        let newestId = notificationOrder.length > 0 ? notificationOrder[0] : null
        if (currentExpiringIndex === -1) {
            if (id === newestId) {
                currentExpiringIndex = notificationOrder.indexOf(id)
                return true
            }
            return false
        }
        return false
    }

    function notificationExpirationComplete(notificationId) {
        if (notificationId) currentExpiringIndex = -1
    }

    function dismissAll() {
        knownNotifications = {}
        notificationOrder = []
        currentExpiringIndex = -1
        for (let i = 0; i < server.trackedNotifications.values.length; ++i) {
            let n = server.trackedNotifications.values[i]
            if (n) n.dismiss()
        }
    }
}
