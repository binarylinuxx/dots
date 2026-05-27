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
			id: cfg
			property int widgetSize:    260
			property int refreshMs:     2000
			property bool showLabels:   true
			property bool showHostname: true
			property int cookieSides:   8
		}
	}

	ColumnLayout {
		width: root.availableWidth - 10
		spacing: 20

		Text {
			text: plugin ? plugin.name : "System Stats"
			font.pixelSize: 26; font.weight: 700; font.family: "Rubik"
			color: col.onSurface
		}

		Text {
			visible: plugin && plugin.description
			text: plugin ? plugin.description : ""
			font.pixelSize: 13; font.family: "Rubik"
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
				anchors.left: parent.left; anchors.right: parent.right
				anchors.top: parent.top; anchors.margins: 15
				spacing: 15

				// Widget size
				RowLayout {
					Layout.fillWidth: true; spacing: 15
					ColumnLayout {
						spacing: 2
						Text { text: "Widget size"; font.pixelSize: 14; font.weight: 700; font.family: "Rubik"; color: col.onSurface }
						Text { text: "Diameter in pixels"; font.pixelSize: 11; font.family: "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
					}
					RowLayout {
						spacing: 0
						Rectangle {
							width: 44; height: 44; topLeftRadius: 22; bottomLeftRadius: 22
							color: wsdecM.containsMouse ? col.primary : col.surfaceContainerHigh
							opacity: cfg.widgetSize <= 160 ? 0.5 : 1.0
							Behavior on color { ColorAnimation { duration: 150 } }
							MaterialSymbol { icon: "remove"; iconSize: 22; anchors.centerIn: parent; color: wsdecM.containsMouse ? col.onPrimary : col.onSurfaceVariant }
							MouseArea { id: wsdecM; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; enabled: cfg.widgetSize > 160; onClicked: cfg.widgetSize = Math.max(160, cfg.widgetSize - 20) }
						}
						Rectangle {
							width: 60; height: 44; color: col.surfaceContainerHighest
							Text { text: cfg.widgetSize; anchors.centerIn: parent; font.pixelSize: 16; font.weight: 700; font.family: "Rubik"; color: col.onSurface }
						}
						Rectangle {
							width: 44; height: 44; topRightRadius: 22; bottomRightRadius: 22
							color: wsincM.containsMouse ? col.primary : col.surfaceContainerHigh
							opacity: cfg.widgetSize >= 500 ? 0.5 : 1.0
							Behavior on color { ColorAnimation { duration: 150 } }
							MaterialSymbol { icon: "add"; iconSize: 22; anchors.centerIn: parent; color: wsincM.containsMouse ? col.onPrimary : col.onSurfaceVariant }
							MouseArea { id: wsincM; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; enabled: cfg.widgetSize < 500; onClicked: cfg.widgetSize = Math.min(500, cfg.widgetSize + 20) }
						}
					}
				}

				Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

				// Cookie sides
				RowLayout {
					Layout.fillWidth: true; spacing: 15
					ColumnLayout {
						spacing: 2
						Text { text: "Cookie sides"; font.pixelSize: 14; font.weight: 700; font.family: "Rubik"; color: col.onSurface }
						Text { text: "Shape of the background"; font.pixelSize: 11; font.family: "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
					}
					RowLayout {
						spacing: 0
						Rectangle {
							width: 44; height: 44; topLeftRadius: 22; bottomLeftRadius: 22
							color: csdecM.containsMouse ? col.primary : col.surfaceContainerHigh
							opacity: cfg.cookieSides <= 4 ? 0.5 : 1.0
							Behavior on color { ColorAnimation { duration: 150 } }
							MaterialSymbol { icon: "remove"; iconSize: 22; anchors.centerIn: parent; color: csdecM.containsMouse ? col.onPrimary : col.onSurfaceVariant }
							MouseArea { id: csdecM; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; enabled: cfg.cookieSides > 4; onClicked: cfg.cookieSides = Math.max(4, cfg.cookieSides - 1) }
						}
						Rectangle {
							width: 50; height: 44; color: col.surfaceContainerHighest
							Text { text: cfg.cookieSides; anchors.centerIn: parent; font.pixelSize: 18; font.weight: 700; font.family: "Rubik"; color: col.onSurface }
						}
						Rectangle {
							width: 44; height: 44; topRightRadius: 22; bottomRightRadius: 22
							color: csincM.containsMouse ? col.primary : col.surfaceContainerHigh
							opacity: cfg.cookieSides >= 16 ? 0.5 : 1.0
							Behavior on color { ColorAnimation { duration: 150 } }
							MaterialSymbol { icon: "add"; iconSize: 22; anchors.centerIn: parent; color: csincM.containsMouse ? col.onPrimary : col.onSurfaceVariant }
							MouseArea { id: csincM; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; enabled: cfg.cookieSides < 16; onClicked: cfg.cookieSides = Math.min(16, cfg.cookieSides + 1) }
						}
					}
				}

				Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

				// Refresh interval
				RowLayout {
					Layout.fillWidth: true; spacing: 15
					ColumnLayout {
						spacing: 2
						Text { text: "Refresh interval"; font.pixelSize: 14; font.weight: 700; font.family: "Rubik"; color: col.onSurface }
						Text { text: cfg.refreshMs + " ms"; font.pixelSize: 11; font.family: "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
					}
					StyledSlider {
						sliderWidth: 140; sliderHeight: 28
						from: 500; to: 10000; stepSize: 500
						value: cfg.refreshMs
						onMoved: (v) => cfg.refreshMs = v
					}
				}

				Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

				// Show labels
				RowLayout {
					Layout.fillWidth: true; spacing: 15
					Text { text: "Show ring labels"; font.pixelSize: 14; font.weight: 700; font.family: "Rubik"; color: col.onSurface; Layout.fillWidth: true }
					ToggleSwitch { checked: cfg.showLabels; onToggled: (s) => cfg.showLabels = s }
				}

				Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

				// Show hostname
				RowLayout {
					Layout.fillWidth: true; spacing: 15
					Text { text: "Show hostname"; font.pixelSize: 14; font.weight: 700; font.family: "Rubik"; color: col.onSurface; Layout.fillWidth: true }
					ToggleSwitch { checked: cfg.showHostname; onToggled: (s) => cfg.showHostname = s }
				}
			}
		}

		Text {
			text: plugin ? "Path: " + plugin.path : ""
			font.pixelSize: 10; color: col.onSurfaceVariant; opacity: 0.5
			wrapMode: Text.WrapAnywhere; Layout.fillWidth: true
		}

		Item { Layout.fillHeight: true }
	}
}
