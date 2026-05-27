// Bar plugin: shows now playing track info with play/pause control.
import QtQuick
import Quickshell.Io
import Quickshell.Services.Mpris
import qs.services
import qs.widgets

Item {
	id: root
	property var plugin: null
	property string pluginPath: plugin ? plugin.path : ""
	property string configPath: pluginPath + "/config.json"

	implicitWidth: row.implicitWidth + 16
	implicitHeight: 28

	FileView {
		id: configFile
		path: pluginPath ? "file://" + pluginPath + "/config.json" : ""
		watchChanges: true
		onFileChanged: reload()
		onAdapterUpdated: writeAdapter()

		adapter: JsonAdapter {
			property bool showArtist: false
			property int maxLength: 20
		}
	}

	readonly property var activePlayer: {
		const players = Mpris.players && Mpris.players.values ? Mpris.players.values : []
		if (!players || players.length === 0) return null
		for (let i = 0; i < players.length; ++i)
			if (players[i] && players[i].isPlaying) return players[i]
		return players[0]
	}

	readonly property int maxLen: (configFile.adapter && configFile.adapter.maxLength) ? configFile.adapter.maxLength : 20

	function truncate(str, len) {
		if (!str) return ""
		return str.length > len ? str.substring(0, len - 1) + "…" : str
	}

	Rectangle {
		anchors.fill: parent
		radius: cfg ? Math.max(8, Math.round(cfg.barRadius * 0.7)) : 14
		color: mouse.containsMouse ? col.primaryContainer : (activePlayer ? col.surfaceContainer : col.surfaceContainerHighest)
		Behavior on color { ColorAnimation { duration: Gstate.animDuration } }

		Row {
			id: row
			anchors.centerIn: parent
			spacing: 6

			MaterialSymbol {
				icon: activePlayer && activePlayer.isPlaying ? "pause" : "play_arrow"
				iconSize: 16
				color: activePlayer ? col.primary : col.onSurfaceVariant
				anchors.verticalCenter: parent.verticalCenter
			}

			Text {
				text: {
					if (!activePlayer) return "No media player"
					const p = activePlayer
					const artist = p.trackArtist || p.identity || ""
					const title = p.trackTitle || "Unknown"
					const display = artist && artist !== title
						? truncate(artist, Math.floor(maxLen / 2)) + " - " + truncate(title, Math.ceil(maxLen / 2))
						: truncate(title, maxLen)
					return display
				}
				font.pixelSize: 12
				font.family: cfg ? cfg.fontFamily : "Rubik"
				font.weight: 600
				color: activePlayer ? col.onSurface : col.onSurfaceVariant
				anchors.verticalCenter: parent.verticalCenter
			}
		}

		MouseArea {
			id: mouse
			anchors.fill: parent
			hoverEnabled: true
			cursorShape: Qt.PointingHandCursor
			onClicked: {
				if (activePlayer && activePlayer.canTogglePlaying)
					activePlayer.togglePlaying()
			}
		}
	}
}