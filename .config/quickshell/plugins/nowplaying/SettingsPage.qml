// Settings page for nowplaying plugin.
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Quickshell.Io
import qs.services
import qs.widgets

ScrollView {
	id: root
	property var plugin: null

	readonly property string configPath: plugin ? "file://" + plugin.path + "/config.json" : ""

	FileView {
		id: configFile
		path: configPath
		watchChanges: true
		onFileChanged: reload()
		onAdapterUpdated: writeAdapter()

		adapter: JsonAdapter {
			property bool showArtist: false
			property int maxLength: 20
		}
	}

	ColumnLayout {
		width: root.availableWidth - 10
		spacing: 20

		Text {
			text: plugin ? plugin.name : "Now Playing"
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

				// Max length (counter style)
				RowLayout {
					Layout.fillWidth: true
					spacing: 15

					ColumnLayout {
						spacing: 2
						Text {
							text: "Max display length"
							font.pixelSize: 14
							font.weight: 700
							font.family: cfg ? cfg.fontFamily : "Rubik"
							color: col.onSurface
						}
						Text {
							text: "Characters in bar widget"
							font.pixelSize: 11
							font.family: cfg ? cfg.fontFamily : "Rubik"
							color: col.onSurfaceVariant
							opacity: 0.8
						}
					}

					RowLayout {
						spacing: 0

						Rectangle {
							width: 44
							height: 44
							color: lenDecMouse.containsMouse ? col.primary : col.surfaceContainerHigh
							topLeftRadius: 22
							bottomLeftRadius: 22
							opacity: configFile.adapter.maxLength <= 10 ? 0.5 : 1.0
							Behavior on color { ColorAnimation { duration: 150 } }

							MaterialSymbol {
								icon: "remove"
								iconSize: 22
								anchors.centerIn: parent
								color: lenDecMouse.containsMouse ? col.onPrimary : col.onSurfaceVariant
							}

							MouseArea {
								id: lenDecMouse
								anchors.fill: parent
								cursorShape: Qt.PointingHandCursor
								hoverEnabled: true
								enabled: configFile.adapter.maxLength > 10
								onClicked: configFile.adapter.maxLength = Math.max(10, configFile.adapter.maxLength - 2)
							}
						}

						Rectangle {
							width: 50
							height: 44
							color: col.surfaceContainerHighest

							Text {
								text: configFile.adapter.maxLength
								anchors.centerIn: parent
								font.pixelSize: 18
								font.family: cfg ? cfg.fontFamily : "Rubik"
								font.weight: 700
								color: col.onSurface
							}
						}

						Rectangle {
							width: 44
							height: 44
							color: lenIncMouse.containsMouse ? col.primary : col.surfaceContainerHigh
							topRightRadius: 22
							bottomRightRadius: 22
							opacity: configFile.adapter.maxLength >= 100 ? 0.5 : 1.0
							Behavior on color { ColorAnimation { duration: 150 } }

							MaterialSymbol {
								icon: "add"
								iconSize: 22
								anchors.centerIn: parent
								color: lenIncMouse.containsMouse ? col.onPrimary : col.onSurfaceVariant
							}

							MouseArea {
								id: lenIncMouse
								anchors.fill: parent
								cursorShape: Qt.PointingHandCursor
								hoverEnabled: true
								enabled: configFile.adapter.maxLength < 100
								onClicked: configFile.adapter.maxLength = Math.min(100, configFile.adapter.maxLength + 2)
							}
						}
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
