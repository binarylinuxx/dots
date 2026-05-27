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
			property int sides: 7
			property real innerRatio: 0.84
			property real outerRounding: 0.45
			property real innerRounding: 0.45
			property bool morphWithTime: true
			property bool showSeconds: true
			property bool showTicks: true
			property int clockSize: 280
		}
	}

	ColumnLayout {
		width: root.availableWidth - 10
		spacing: 20

		Text {
			text: plugin ? plugin.name : "Analog Clock"
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

				// Sides counter
				RowLayout {
					Layout.fillWidth: true
					spacing: 15

					ColumnLayout {
						spacing: 2
						Text { text: "Sides"; font.pixelSize: 14; font.weight: 700; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurface }
						Text { text: "Cookie corner count"; font.pixelSize: 11; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
					}

					RowLayout {
						spacing: 0
						Rectangle {
							width: 44; height: 44; topLeftRadius: 22; bottomLeftRadius: 22
							color: sdecM.containsMouse ? col.primary : col.surfaceContainerHigh
							opacity: cfgAdapter.sides <= 4 ? 0.5 : 1.0
							Behavior on color { ColorAnimation { duration: 150 } }
							MaterialSymbol { icon: "remove"; iconSize: 22; anchors.centerIn: parent; color: sdecM.containsMouse ? col.onPrimary : col.onSurfaceVariant }
							MouseArea { id: sdecM; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; enabled: cfgAdapter.sides > 4; onClicked: cfgAdapter.sides = Math.max(4, cfgAdapter.sides - 1) }
						}
						Rectangle {
							width: 50; height: 44; color: col.surfaceContainerHighest
							Text { text: cfgAdapter.sides; anchors.centerIn: parent; font.pixelSize: 18; font.weight: 700; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurface }
						}
						Rectangle {
							width: 44; height: 44; topRightRadius: 22; bottomRightRadius: 22
							color: sincM.containsMouse ? col.primary : col.surfaceContainerHigh
							opacity: cfgAdapter.sides >= 12 ? 0.5 : 1.0
							Behavior on color { ColorAnimation { duration: 150 } }
							MaterialSymbol { icon: "add"; iconSize: 22; anchors.centerIn: parent; color: sincM.containsMouse ? col.onPrimary : col.onSurfaceVariant }
							MouseArea { id: sincM; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; enabled: cfgAdapter.sides < 12; onClicked: cfgAdapter.sides = Math.min(12, cfgAdapter.sides + 1) }
						}
					}
				}

				Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

				// Clock size counter
				RowLayout {
					Layout.fillWidth: true
					spacing: 15

					ColumnLayout {
						spacing: 2
						Text { text: "Clock size"; font.pixelSize: 14; font.weight: 700; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurface }
						Text { text: "Diameter in pixels"; font.pixelSize: 11; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
					}

					RowLayout {
						spacing: 0
						Rectangle {
							width: 44; height: 44; topLeftRadius: 22; bottomLeftRadius: 22
							color: csdecM.containsMouse ? col.primary : col.surfaceContainerHigh
							opacity: cfgAdapter.clockSize <= 120 ? 0.5 : 1.0
							Behavior on color { ColorAnimation { duration: 150 } }
							MaterialSymbol { icon: "remove"; iconSize: 22; anchors.centerIn: parent; color: csdecM.containsMouse ? col.onPrimary : col.onSurfaceVariant }
							MouseArea { id: csdecM; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; enabled: cfgAdapter.clockSize > 120; onClicked: cfgAdapter.clockSize = Math.max(120, cfgAdapter.clockSize - 20) }
						}
						Rectangle {
							width: 60; height: 44; color: col.surfaceContainerHighest
							Text { text: cfgAdapter.clockSize; anchors.centerIn: parent; font.pixelSize: 16; font.weight: 700; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurface }
						}
						Rectangle {
							width: 44; height: 44; topRightRadius: 22; bottomRightRadius: 22
							color: csincM.containsMouse ? col.primary : col.surfaceContainerHigh
							opacity: cfgAdapter.clockSize >= 600 ? 0.5 : 1.0
							Behavior on color { ColorAnimation { duration: 150 } }
							MaterialSymbol { icon: "add"; iconSize: 22; anchors.centerIn: parent; color: csincM.containsMouse ? col.onPrimary : col.onSurfaceVariant }
							MouseArea { id: csincM; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; enabled: cfgAdapter.clockSize < 600; onClicked: cfgAdapter.clockSize = Math.min(600, cfgAdapter.clockSize + 20) }
						}
					}
				}

				Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

				// Inner ratio slider
				RowLayout {
					Layout.fillWidth: true; spacing: 15
					ColumnLayout {
						spacing: 2
						Text { text: "Inner ratio"; font.pixelSize: 14; font.weight: 700; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurface }
						Text { text: "Depth of the indents"; font.pixelSize: 11; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
					}
					StyledSlider {
						sliderWidth: 140; sliderHeight: 28
						from: 0.5; to: 0.98; stepSize: 0.01
						value: cfgAdapter.innerRatio
						onMoved: (v) => cfgAdapter.innerRatio = v
					}
				}

				Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

				// Outer rounding slider
				RowLayout {
					Layout.fillWidth: true; spacing: 15
					ColumnLayout {
						spacing: 2
						Text { text: "Outer rounding"; font.pixelSize: 14; font.weight: 700; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurface }
						Text { text: "Roundness of outer corners"; font.pixelSize: 11; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
					}
					StyledSlider {
						sliderWidth: 140; sliderHeight: 28
						from: 0.0; to: 1.0; stepSize: 0.01
						value: cfgAdapter.outerRounding
						onMoved: (v) => cfgAdapter.outerRounding = v
					}
				}

				Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

				// Inner rounding slider
				RowLayout {
					Layout.fillWidth: true; spacing: 15
					ColumnLayout {
						spacing: 2
						Text { text: "Inner rounding"; font.pixelSize: 14; font.weight: 700; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurface }
						Text { text: "Roundness of inner corners"; font.pixelSize: 11; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
					}
					StyledSlider {
						sliderWidth: 140; sliderHeight: 28
						from: 0.0; to: 1.0; stepSize: 0.01
						value: cfgAdapter.innerRounding
						onMoved: (v) => cfgAdapter.innerRounding = v
					}
				}

				Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

				// Morph with time
				RowLayout {
					Layout.fillWidth: true; spacing: 15
					ColumnLayout {
						spacing: 2
						Text { text: "Morph with time"; font.pixelSize: 14; font.weight: 700; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurface }
						Text { text: "Shape changes every 15 min"; font.pixelSize: 11; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurfaceVariant; opacity: 0.8 }
					}
					ToggleSwitch { checked: cfgAdapter.morphWithTime; onToggled: (s) => cfgAdapter.morphWithTime = s }
				}

				Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

				// Show ticks
				RowLayout {
					Layout.fillWidth: true; spacing: 15
					Text { text: "Show tick marks"; font.pixelSize: 14; font.weight: 700; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurface; Layout.fillWidth: true }
					ToggleSwitch { checked: cfgAdapter.showTicks; onToggled: (s) => cfgAdapter.showTicks = s }
				}

				Rectangle { Layout.fillWidth: true; height: 1; color: col.outlineVariant; opacity: 0.5 }

				// Show seconds
				RowLayout {
					Layout.fillWidth: true; spacing: 15
					Text { text: "Show second hand"; font.pixelSize: 14; font.weight: 700; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurface; Layout.fillWidth: true }
					ToggleSwitch { checked: cfgAdapter.showSeconds; onToggled: (s) => cfgAdapter.showSeconds = s }
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
