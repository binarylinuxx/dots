import Quickshell
import Quickshell.Widgets
import Quickshell.Services.Notifications
import QtQuick
import QtQuick.Layouts

PanelWindow {
	id: notificationsWindow
	property int xwidth: 400
	property int ywidth: 430
	property int defaultTimeout: 5000
	property var knownNotifications: ({})
	property int lastNotificationCount: 0
	property var notificationOrder: []
	property int currentExpiringIndex: -1
	exclusiveZone: 0

	visible: notificationServer.trackedNotifications.values.length > 0

	anchors {
		top: true
		right: true
	}
	margins {
		right: 0
		top: 0
	}

	width: xwidth
	height: ywidth
	color: "transparent"

	Component.onCompleted: {
		console.log("Notifications.qml loaded successfully")
		lastNotificationCount = notificationServer.trackedNotifications.values.length
		markAllAsKnown()
	}

	onVisibleChanged: {
		if (visible) {
			markAllAsKnown()
		}
	}

	function markAllAsKnown() {
		let currentTime = Date.now()
		notificationOrder = []
		for (let i = 0; i < notificationServer.trackedNotifications.values.length; i++) {
			let notification = notificationServer.trackedNotifications.values[i]
			if (notification && notification.id) {
				let id = notification.id.toString()
				knownNotifications[id] = currentTime
				notificationOrder.push(id)
			}
		}
		lastNotificationCount = notificationServer.trackedNotifications.values.length
	}

	function isNotificationNew(notificationId) {
		if (!notificationId) return false
		let id = notificationId.toString()
		let isNew = !(id in knownNotifications)
		if (isNew) {
			knownNotifications[id] = Date.now()
		}
		return isNew
	}

	function cleanupNotificationTracking(notificationId) {
		if (notificationId) {
			let id = notificationId.toString()
			delete knownNotifications[id]
			let index = notificationOrder.indexOf(id)
			if (index > -1) {
				notificationOrder.splice(index, 1)
			}
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
		if (notificationId) {
			currentExpiringIndex = -1
		}
	}

	NotificationServer {
		id: notificationServer
		keepOnReload: true
		bodySupported: true
		actionsSupported: true
		inlineReplySupported: false
		imageSupported: true
		actionIconsSupported: true
		persistenceSupported: false

		onNotification: function(notification) {
			try {
				notification.tracked = true
				if (notification.id) {
					let id = notification.id.toString()
					knownNotifications[id] = "NEW_" + Date.now()
					notificationOrder.unshift(id)
				}
			} catch (error) {
				console.error("Error in onNotification handler: " + error)
			}
		}
	}

	ClippingRectangle {
		width: xwidth
		height: ywidth
		radius: 25
		color: "transparent"

		ColumnLayout {
			anchors.fill: parent
			anchors.margins: 5

			ListView {
				id: notifsBG
				width: xwidth - 10
				height: ywidth - 10
				clip: true
				model: notificationServer.trackedNotifications.values
				spacing: 5

				add: Transition {
					NumberAnimation {
						properties: "x"
						from: notifsBG.width
						to: 0
						duration: 400
						easing.type: Easing.OutCubic
					}
				}

				remove: Transition {
					NumberAnimation {
						properties: "x"
						to: notifsBG.width
						duration: 300
						easing.type: Easing.InCubic
					}
				}

				displaced: Transition {
					NumberAnimation {
						properties: "y"
						duration: 250
						easing.type: Easing.OutQuart
					}
				}

				delegate: Rectangle {
					id: notificationRect
					width: notifsBG.width
					height: 100
					radius: 20
					color: col.background
					border.width: 0
					opacity: 1.0

					property var parentWindow: notificationsWindow
					property bool shouldAnimate: false
					property string notificationId: (modelData && modelData.id) ? modelData.id.toString() : ""
					property bool isExpiring: false
					property real timeProgress: 0.0
					property int totalDuration: modelData && modelData.expireTimeout > 0 ? 
						(modelData.expireTimeout * 1000) : notificationsWindow.defaultTimeout
					property int elapsedTime: 0

					states: [
						State {
							name: "hovered"
							when: mouseArea.containsMouse && !isExpiring
							PropertyChanges {
								target: notificationRect
								x: -5
							}
						},
						State {
							name: "expiring"
							when: isExpiring
						}
					]

					transitions: Transition {
						NumberAnimation {
							properties: "x"
							duration: 200
							easing.type: Easing.OutQuart
						}
					}

					SequentialAnimation {
						id: slideInAnimation
						running: false

						PropertyAction {
							target: notificationRect
							property: "x"
							value: notifsBG.width
						}

						NumberAnimation {
							target: notificationRect
							property: "x"
							to: 0
							duration: 400
							easing.type: Easing.OutCubic
						}

						ScriptAction {
							script: {
								if (!progressTimer.running) progressTimer.start()
								if (!autoExpireTimer.running) autoExpireTimer.start()
							}
						}
					}

					SequentialAnimation {
						id: expireAnimation
						running: false

						ScriptAction {
							script: {
								notificationRect.isExpiring = true
							}
						}

						NumberAnimation {
							target: notificationRect
							property: "x"
							to: notifsBG.width
							duration: 400
							easing.type: Easing.InCubic
						}

						ScriptAction {
							script: {
								try {
									if (modelData) {
										notificationRect.parentWindow.cleanupNotificationTracking(notificationRect.notificationId)
										notificationRect.parentWindow.notificationExpirationComplete(notificationRect.notificationId)
										modelData.expire()
									}
								} catch (error) {
									console.error("Error expiring notification:", error)
									notificationRect.parentWindow.notificationExpirationComplete(notificationRect.notificationId)
								}
							}
						}
					}

					Component.onCompleted: {
						if (notificationId) {
							let trackingValue = parentWindow.knownNotifications[notificationId]
							let isNew = trackingValue && trackingValue.toString().startsWith("NEW_")
							shouldAnimate = isNew

							if (shouldAnimate) {
								parentWindow.knownNotifications[notificationId] = Date.now()
								slideInAnimation.start()
							} else {
								startTimersDelayed.start()
							}
						} else {
							startTimersDelayed.start()
						}
					}

					Timer {
						id: startTimersDelayed
						interval: 50
						repeat: false
						onTriggered: {
							if (!progressTimer.running) progressTimer.start()
							if (!autoExpireTimer.running) autoExpireTimer.start()
						}
					}

					Timer {
						id: progressTimer
						interval: 100
						running: false
						repeat: true
						triggeredOnStart: false
						onTriggered: {
							if (notificationRect.isExpiring) {
								stop()
								return
							}
							notificationRect.elapsedTime += interval
							notificationRect.timeProgress = notificationRect.elapsedTime / notificationRect.totalDuration
						}
					}

					Timer {
						id: autoExpireTimer
						interval: notificationRect.totalDuration
						running: false
						repeat: false
						triggeredOnStart: false
						onTriggered: {
							if (notificationRect.isExpiring) return

							if (parentWindow.canNotificationExpire(notificationRect.notificationId)) {
								progressTimer.stop()
								expireAnimation.start()
							} else {
								autoExpireTimer.interval = 500
								autoExpireTimer.start()
							}
						}
					}

					ColumnLayout {
						id: contentLayout
						anchors.fill: parent
						anchors.margins: 10
						anchors.bottomMargin: 13
						spacing: 8

						RowLayout {
							Layout.fillWidth: true
							spacing: 8

							ClippingRectangle {
								id: iconContainer
								width: 32
								height: 32
								radius: 80
								color: col.surfaceContainerHigh
								visible: modelData && (modelData.image || modelData.appIcon)

								Image {
									id: notificationIcon
									anchors.centerIn: parent
									width: 32
									height: 32
									fillMode: Image.PreserveAspectFit
									smooth: true

									source: {
										if (modelData) {
											if (modelData.image && modelData.image !== "")
												return modelData.image
											else if (modelData.appIcon && modelData.appIcon !== "")
												return modelData.appIcon
										}
										return ""
									}

									Rectangle {
										anchors.fill: parent
										radius: 4
										color: col.primary
										visible: parent.status === Image.Error || parent.status === Image.Null

										Text {
											anchors.centerIn: parent
											text: {
												let appName = (modelData && modelData.appName) || "?"
												return appName.charAt(0).toUpperCase()
											}
											font.pixelSize: 14
											font.weight: Font.Bold
											font.family: cfg ? cfg.fontFamily : "Rubik"
											color: col.onPrimary
										}
									}
								}
							}

							Rectangle {
								id: appNameBadge
								width: appNameText.width + 16
								height: 24
								radius: 50
								color: col.primary

								Text {
									id: appNameText
									text: (modelData && modelData.appName) || "Unknown"
									font.pixelSize: 12
									font.weight: Font.Bold
									font.family: cfg ? cfg.fontFamily : "Rubik"
									anchors.centerIn: parent
									color: col.onPrimary
								}
							}

							Item { Layout.fillWidth: true }

							Rectangle {
								id: timestampBadge
								width: 55
								height: 24
								color: col.surfaceContainer
								radius: 12

								Text {
									text: Qt.formatTime(new Date(), "hh:mm")
									font.pixelSize: 14
									font.weight: 700
									font.family: cfg ? cfg.fontFamily : "Rubik"
									anchors.centerIn: parent
									color: col.onSurfaceVariant
								}
							}
						}

						Text {
							id: summaryText
							Layout.fillWidth: true
							text: (modelData && modelData.summary) || "No Summary"
							font.pixelSize: 16
							font.weight: Font.Bold
							font.family: cfg ? cfg.fontFamily : "Rubik"
							color: col.onSurface
							elide: Text.ElideRight
							maximumLineCount: 1
						}

						Text {
							id: bodyText
							Layout.fillWidth: true
							Layout.fillHeight: true
							text: (modelData && modelData.body) || "No content"
							font.pixelSize: 14
							font.family: cfg ? cfg.fontFamily : "Rubik"
							color: col.onSurfaceVariant
							wrapMode: Text.Wrap
							maximumLineCount: 3
							elide: Text.ElideRight
						}
					}

					MouseArea {
						id: mouseArea
						anchors.fill: parent
						hoverEnabled: true
						acceptedButtons: Qt.LeftButton | Qt.RightButton

						drag.target: notificationRect
						drag.axis: Drag.XAxis
						drag.minimumX: 0
						drag.maximumX: notificationRect.width * 1.5

						property bool wasDragged: false

						onEntered: {
							if (!notificationRect.isExpiring) {
								autoExpireTimer.stop()
								progressTimer.stop()
							}
						}

						onExited: {
							if (!notificationRect.isExpiring && !drag.active) {
								autoExpireTimer.interval = notificationRect.totalDuration - notificationRect.elapsedTime
								if (autoExpireTimer.interval > 0) {
									autoExpireTimer.start()
									progressTimer.start()
								}
							}
						}

						onPressed: wasDragged = false

						onPositionChanged: {
							if (drag.active) wasDragged = true
						}

						onReleased: {
							if (wasDragged) {
								if (notificationRect.x > notificationRect.width * 0.4) {
									autoExpireTimer.stop()
									progressTimer.stop()
									expireAnimation.start()
								} else {
									snapBackAnimation.start()
								}
							}
						}

						onClicked: function(mouse) {
							if (wasDragged || notificationRect.isExpiring) return

							if (mouse.button === Qt.LeftButton) {
								try {
									autoExpireTimer.stop()
									progressTimer.stop()
									parentWindow.currentExpiringIndex = -1
									dismissAnimation.start()
								} catch (error) {
									console.error("Error dismissing notification: " + error)
								}
							} else if (mouse.button === Qt.RightButton) {
								try {
									parentWindow.knownNotifications = {}
									parentWindow.notificationOrder = []
									parentWindow.currentExpiringIndex = -1
									for (let i = 0; i < notificationServer.trackedNotifications.values.length; ++i) {
										let n = notificationServer.trackedNotifications.values[i]
										if (n) n.dismiss()
									}
								} catch (error) {
									console.error("Error dismissing all notifications: " + error)
								}
							}
						}

						NumberAnimation {
							id: snapBackAnimation
							target: notificationRect
							property: "x"
							to: 0
							duration: 200
							easing.type: Easing.OutBounce
						}
					}

					SequentialAnimation {
						id: dismissAnimation

						NumberAnimation {
							target: notificationRect
							property: "x"
							to: -notifsBG.width
							duration: 300
							easing.type: Easing.InCubic
						}

						ScriptAction {
							script: {
								try {
									notificationRect.parentWindow.cleanupNotificationTracking(notificationRect.notificationId)
									modelData.dismiss()
								} catch (error) {
									console.error("Error dismissing notification: " + error)
								}
							}
						}
					}

					Connections {
						target: modelData
						function onClosed(reason) {
							try {
								autoExpireTimer.stop()
								progressTimer.stop()
							} catch (error) {
								console.error("Error in onClosed handler: " + error)
							}
						}
					}
				}
			}
		}
	}
}
