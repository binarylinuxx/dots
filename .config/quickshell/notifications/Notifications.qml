import QtQuick
import Quickshell
import Quickshell.Widgets
import qs.services

Item {
    id: root

    anchors.fill: parent
    z: 250

    property int maximumVisible: 3
    property int defaultTimeout: 7000
    readonly property int outerMargin: 10
    readonly property int frameGap: 2
    readonly property string notificationEdge: Gstate.barEdge === "right" ? "left" : "right"
    readonly property real stackWidth: Math.min(390, Math.max(0, width - Gstate.leftInset - Gstate.rightInset - outerMargin * 2))
    readonly property real maximumStackHeight: Math.max(0, height - Gstate.topInset - Gstate.bottomInset - outerMargin * 2)
    readonly property var entries: NotificationService.trackedNotifications.values.slice(0, maximumVisible)
    readonly property bool shouldShow: !NotificationService.dndEnabled && entries.length > 0
    property Item surface: notificationStack

    visible: shouldShow

    Component.onCompleted: NotificationService.markAllAsKnown()
    onVisibleChanged: if (visible) NotificationService.markAllAsKnown()

    Item {
        id: notificationStack

        x: root.notificationEdge === "left" ? Gstate.leftInset + root.frameGap : root.width - Gstate.rightInset - width - root.frameGap
        y: Gstate.barEdge === "bottom"
            ? root.height - Gstate.bottomInset - height - 2
            : Gstate.topInset + 2
        width: root.stackWidth
        height: root.shouldShow ? Math.min(stack.contentHeight + root.outerMargin * 2, root.maximumStackHeight) : 0
        visible: height > 0
        property real radius: 25

        Behavior on height { NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic } }

        ListView {
            id: stack

            anchors.fill: parent
            anchors.margins: root.outerMargin
            anchors.rightMargin: 0
            clip: true
            interactive: false
            spacing: 8
            model: root.entries

            add: Transition {
                NumberAnimation { properties: "x,y"; duration: Gstate.animDuration; easing.type: Easing.OutCubic }
            }
            move: Transition {
                NumberAnimation { properties: "x,y"; duration: Gstate.animDuration; easing.type: Easing.OutCubic }
            }
            displaced: Transition {
                NumberAnimation { properties: "x,y"; duration: Gstate.animDuration; easing.type: Easing.OutCubic }
            }

            delegate: Item {
                id: card

                required property var modelData
                width: stack.width
                property real reveal: 0
                property real swipeOffset: 0
                property real entryOffset: (root.notificationEdge === "right" ? 1 : -1) * stack.width * (1 - reveal)
                property real removalHeight: 1
                property bool swiped: false
                property bool expanded: false
                property bool dismissed: false
                property int elapsedTime: 0
                property string notificationId: modelData && modelData.id ? modelData.id.toString() : ""
                readonly property int timeout: modelData && modelData.expireTimeout > 0 ? modelData.expireTimeout : root.defaultTimeout
                readonly property bool paused: hover.hovered || gesture.pressed

                implicitHeight: Math.max(76, content.implicitHeight + 24) * removalHeight
                height: implicitHeight
                x: entryOffset + swipeOffset
                opacity: reveal
                clip: true

                Component.onCompleted: {
                    const trackingValue = NotificationService.knownNotifications[notificationId]
                    if (trackingValue && trackingValue.toString().startsWith("NEW_"))
                        NotificationService.knownNotifications[notificationId] = Date.now()
                    reveal = 1
                }

                Behavior on entryOffset { NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic } }
                Behavior on swipeOffset {
                    enabled: !gesture.pressed
                    NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
                }
                Behavior on opacity { NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic } }
                Behavior on implicitHeight { NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic } }
                Behavior on y { NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic } }

                ListView.onRemove: {
                    ListView.delayRemove = true
                    card.swiped = true
                    card.swipeOffset = card.swipeOffset < 0 ? -stack.width : stack.width
                    card.removalHeight = 0
                    removeAfterSlide.restart()
                }

                Rectangle {
                    anchors.fill: parent
                    radius: notificationStack.radius
                    color: card.modelData.urgency > 1 ? col.errorContainer : col.surfaceContainer
                }

                HoverHandler { id: hover }

                Item {
                    id: content

                    z: 2
                    anchors.fill: parent
                    anchors.margins: 12
                    implicitHeight: Math.max(iconWell.height, textColumn.implicitHeight)

                    Item {
                        id: iconWell

                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: 54
                        height: width

                        Rectangle {
                            anchors.fill: parent
                            radius: width / 2
                            color: card.modelData.urgency > 1 ? col.error : col.primaryContainer
                        }

                        IconImage {
                            id: appIcon

                            anchors.centerIn: parent
                            implicitSize: 38
                            visible: (card.modelData.image || card.modelData.appIcon || "").length > 0
                            source: card.modelData.image || Quickshell.iconPath(card.modelData.appIcon, "application-x-executable")
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !appIcon.visible || appIcon.status === Image.Error
                            text: "notifications"
                            color: card.modelData.urgency > 1 ? col.onError : col.onPrimaryContainer
                            font.family: "Material Symbols Rounded"
                            font.pixelSize: 34
                        }
                    }

                    Column {
                        id: textColumn

                        anchors.left: iconWell.right
                        anchors.right: expandButton.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 2

                        Text {
                            width: parent.width
                            text: card.modelData.appName || "System"
                            color: col.onSurfaceVariant
                            elide: Text.ElideRight
                            font.family: cfg.fontFamily
                            font.pixelSize: Math.max(9, cfg.fontSize - 5)
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.7
                        }

                        Text {
                            width: parent.width
                            text: card.modelData.summary || "Notification"
                            color: col.onSurface
                            elide: Text.ElideRight
                            font.family: cfg.fontFamily
                            font.pixelSize: Math.max(13, cfg.fontSize)
                            font.weight: Font.Black
                        }

                        Text {
                            id: bodyText

                            width: parent.width
                            visible: text.length > 0
                            text: card.modelData.body || ""
                            color: col.onSurfaceVariant
                            elide: card.expanded ? Text.ElideNone : Text.ElideRight
                            maximumLineCount: card.expanded ? 0 : 2
                            wrapMode: Text.WordWrap
                            font.family: cfg.fontFamily
                            font.pixelSize: Math.max(10, cfg.fontSize - 3)
                        }

                        Flow {
                            width: parent.width
                            visible: actionRepeater.count > 1
                            spacing: 6

                            Repeater {
                                id: actionRepeater
                                model: card.modelData.actions || []

                                delegate: Rectangle {
                                    required property var modelData
                                    width: Math.min(parent.width, Math.max(72, actionLabel.implicitWidth + 18))
                                    height: 24
                                    radius: height / 2
                                    color: actionHover.containsMouse ? col.secondaryContainer : col.surfaceContainerHigh

                                    Text {
                                        id: actionLabel

                                        anchors.centerIn: parent
                                        width: parent.width - 18
                                        text: modelData.text || modelData.identifier || "Action"
                                        color: actionHover.containsMouse ? col.onSecondaryContainer : col.onSurface
                                        elide: Text.ElideRight
                                        font.family: cfg.fontFamily
                                        font.pixelSize: Math.max(9, cfg.fontSize - 4)
                                        font.weight: Font.DemiBold
                                    }

                                    MouseArea {
                                        id: actionHover

                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onPressed: mouse => mouse.accepted = true
                                        onClicked: modelData.invoke()
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        id: expandButton

                        z: 2
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        width: 24
                        height: width
                        visible: bodyText.truncated || card.expanded

                        Rectangle {
                            anchors.centerIn: parent
                            width: 20
                            height: width
                            radius: width / 2
                            color: expandHover.containsMouse ? col.surfaceContainerHighest : col.primaryContainer

                            Behavior on color { ColorAnimation { duration: Gstate.animDuration } }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: card.expanded ? "expand_less" : "expand_more"
                            color: col.onPrimaryContainer
                            font.family: "Material Symbols Rounded"
                            font.pixelSize: 17
                        }
                        MouseArea {
                            id: expandHover

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onPressed: mouse => mouse.accepted = true
                            onClicked: card.expanded = !card.expanded
                        }
                    }
                }

                MouseArea {
                    id: gesture

                    anchors.fill: parent
                    z: 1
                    acceptedButtons: Qt.LeftButton
                    property real pressX: 0
                    property bool wasDragged: false

                    onPressed: mouse => {
                        pressX = mouse.x
                        wasDragged = false
                    }
                    onPositionChanged: mouse => {
                        if (!pressed)
                            return
                        card.swipeOffset = mouse.x - pressX
                        wasDragged = Math.abs(card.swipeOffset) > 6
                    }
                    onReleased: {
                        if (Math.abs(card.swipeOffset) > card.width * 0.32) {
                            card.swiped = true
                            card.swipeOffset = card.swipeOffset < 0 ? -stack.width : stack.width
                            dismissAfterSwipe.restart()
                        } else {
                            card.swipeOffset = 0
                        }
                    }
                    onClicked: {
                        if (wasDragged || card.swiped)
                            return
                        const actions = card.modelData.actions || []
                        if (actions.length === 1)
                            actions[0].invoke()
                    }
                }

                Timer {
                    id: expiry

                    interval: 50
                    running: !card.modelData.resident && !card.dismissed && !card.paused
                    repeat: true
                    onTriggered: {
                        card.elapsedTime += interval
                        if (card.elapsedTime < card.timeout)
                            return
                        card.dismissed = true
                        card.swiped = true
                        card.swipeOffset = stack.width
                        dismissAfterSwipe.restart()
                    }
                }

                Timer {
                    id: dismissAfterSwipe

                    interval: 190
                    onTriggered: {
                        NotificationService.cleanupNotificationTracking(card.notificationId)
                        card.modelData.dismiss()
                    }
                }

                Timer {
                    id: removeAfterSlide

                    interval: Gstate.animDuration
                    onTriggered: card.ListView.delayRemove = false
                }

                Connections {
                    target: card.modelData
                    function onClosed() {
                        expiry.stop()
                    }
                }
            }
        }
    }
}
