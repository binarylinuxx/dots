import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Quickshell.Io
import qs.services
import qs.widgets

ScrollView {
	id: root
	property var plugin: null
	clip: true

	readonly property string configPath: plugin ? "file://" + plugin.path + "/config.json" : ""

	FileView {
		id: configFile
		path: configPath
		watchChanges: true
		onFileChanged: reload()
		onAdapterUpdated: writeAdapter()

		adapter: JsonAdapter {
			id: cfgAdapter
			property string greeting: "Hello"
			property bool showIcon: true
			property int refreshSec: 60
			property int clickCount: 0
		}
	}

	ColumnLayout {
		width: root.availableWidth - 10
		spacing: 20

		Text {
			text: plugin ? plugin.name : "Plugin"
			font.pixelSize: 26
			font.family: cfg ? cfg.fontFamily : "Rubik"
			font.weight: 700
			color: col.onSurface
		}

		Text {
			visible: plugin && plugin.description
			text: plugin ? plugin.description : ""
			font.pixelSize: 13
			font.family: cfg ? cfg.fontFamily : "Rubik"
			color: col.onSurfaceVariant
			wrapMode: Text.WordWrap
			Layout.fillWidth: true
		}

		Rectangle {
			Layout.fillWidth: true
			Layout.preferredHeight: form.implicitHeight + 30
			radius: 16
			color: col.surfaceContainer

			ColumnLayout {
				id: form
				anchors.left: parent.left
				anchors.right: parent.right
				anchors.top: parent.top
				anchors.margins: 15
				spacing: 15

				// Greeting text field (styled like Settings)
				RowLayout {
					Layout.fillWidth: true
					spacing: 15

					Text {
						text: "Greeting"
						font.pixelSize: 14
						font.weight: 700
						font.family: cfg ? cfg.fontFamily : "Rubik"
						color: col.onSurface
						Layout.fillWidth: true
					}

					Rectangle {
						Layout.preferredWidth: 180
						Layout.preferredHeight: 36
						radius: 18
						color: col.surfaceContainerHigh

						TextField {
							anchors.fill: parent
							anchors.margins: 4
							text: cfgAdapter.greeting
							color: col.onSurface
							font.pixelSize: 14
							font.family: cfg ? cfg.fontFamily : "Rubik"
							background: null
							verticalAlignment: Text.AlignVCenter
							onEditingFinished: cfgAdapter.greeting = text
						}
					}
				}

				Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

				// Show icon toggle (uses ToggleSwitch widget)
				RowLayout {
					Layout.fillWidth: true
					spacing: 15

					Text {
						text: "Show icon"
						font.pixelSize: 14
						font.weight: 700
						font.family: cfg ? cfg.fontFamily : "Rubik"
						color: col.onSurface
						Layout.fillWidth: true
					}

					ToggleSwitch {
						checked: cfgAdapter.showIcon
						onToggled: (s) => cfgAdapter.showIcon = s
					}
				}

				Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

				// Refresh interval counter (styled like workspace count in Settings)
				RowLayout {
					Layout.fillWidth: true
					spacing: 15

					ColumnLayout {
						spacing: 2
						Text {
							text: "Refresh interval"
							font.pixelSize: 14
							font.weight: 700
							font.family: cfg ? cfg.fontFamily : "Rubik"
							color: col.onSurface
						}
						Text {
							text: "Seconds between updates"
							font.pixelSize: 11
							font.family: cfg ? cfg.fontFamily : "Rubik"
							color: col.onSurfaceVariant
							opacity: 0.8
						}
					}

					RowLayout {
						spacing: 0

						// Decrement button
						Rectangle {
							width: 44
							height: 44
							color: refreshDecMouse.containsMouse ? col.primary : col.surfaceContainerHigh
							topLeftRadius: 22
							bottomLeftRadius: 22
							opacity: cfgAdapter.refreshSec <= 5 ? 0.5 : 1.0
							Behavior on color { ColorAnimation { duration: 150 } }

							MaterialSymbol {
								icon: "remove"
								iconSize: 22
								anchors.centerIn: parent
								color: refreshDecMouse.containsMouse ? col.onPrimary : col.onSurfaceVariant
							}

							MouseArea {
								id: refreshDecMouse
								anchors.fill: parent
								cursorShape: Qt.PointingHandCursor
								hoverEnabled: true
								enabled: cfgAdapter.refreshSec > 5
								onClicked: cfgAdapter.refreshSec = Math.max(5, cfgAdapter.refreshSec - 5)
							}
						}

						// Value display
						Rectangle {
							width: 50
							height: 44
							color: col.surfaceContainerHighest

							Text {
								text: cfgAdapter.refreshSec
								anchors.centerIn: parent
								font.pixelSize: 18
								font.family: cfg ? cfg.fontFamily : "Rubik"
								font.weight: 700
								color: col.onSurface
							}
						}

						// Increment button
						Rectangle {
							width: 44
							height: 44
							color: refreshIncMouse.containsMouse ? col.primary : col.surfaceContainerHigh
							topRightRadius: 22
							bottomRightRadius: 22
							opacity: cfgAdapter.refreshSec >= 3600 ? 0.5 : 1.0
							Behavior on color { ColorAnimation { duration: 150 } }

							MaterialSymbol {
								icon: "add"
								iconSize: 22
								anchors.centerIn: parent
								color: refreshIncMouse.containsMouse ? col.onPrimary : col.onSurfaceVariant
							}

							MouseArea {
								id: refreshIncMouse
								anchors.fill: parent
								cursorShape: Qt.PointingHandCursor
								hoverEnabled: true
								enabled: cfgAdapter.refreshSec < 3600
								onClicked: cfgAdapter.refreshSec = Math.min(3600, cfgAdapter.refreshSec + 5)
							}
						}
					}
				}

				Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

				// Click count display (read-only)
				RowLayout {
					Layout.fillWidth: true
					spacing: 15

					Text {
						text: "Click count"
						font.pixelSize: 14
						font.weight: 700
						font.family: cfg ? cfg.fontFamily : "Rubik"
						color: col.onSurface
						Layout.fillWidth: true
					}

					Text {
						text: String(cfgAdapter.clickCount)
						font.pixelSize: 14
						font.family: cfg ? cfg.fontFamily : "Rubik"
						font.weight: 700
						color: col.onSurfaceVariant
					}
				}
			}
		}

		Text {
			text: plugin ? "Path: " + plugin.path : ""
			font.pixelSize: 10
			color: col.onSurfaceVariant
			opacity: 0.5
			wrapMode: Text.WrapAnywhere
			Layout.fillWidth: true
		}

		Item { Layout.fillHeight: true }
	}
}