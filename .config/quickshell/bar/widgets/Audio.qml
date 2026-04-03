import qs.services
import QtQuick
import Quickshell
import Quickshell.Services.Pipewire
import qs.widgets

Item {
    property bool taskbarOpen: false
    width: audioContainer.width

    PwObjectTracker {
        objects: [Pipewire.defaultAudioSink]
    }

    property real volume: 0
    property bool isMuted: false

    Connections {
        target: Pipewire.defaultAudioSink?.audio

        function onVolumeChanged() {
            volume = Pipewire.defaultAudioSink?.audio?.volume ?? 0
        }

        function onMutedChanged() {
            isMuted = Pipewire.defaultAudioSink?.audio?.muted ?? false
        }
    }

    Component.onCompleted: {
        volume = Pipewire.defaultAudioSink?.audio?.volume ?? 0
        isMuted = Pipewire.defaultAudioSink?.audio?.muted ?? false
    }

    Rectangle {
        id: audioContainer
        width: audioIcon.width + 10
        height: 28
        anchors.centerIn: parent
        radius: 14
        color: "transparent"

        Behavior on color {
            ColorAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
        }

        Behavior on width {
            NumberAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
        }

        MaterialSymbol {
            id: audioIcon
            anchors.centerIn: parent
            color: col.primary
            iconSize: 20

            Behavior on color {
                ColorAnimation { duration: Gstate.animDuration; easing.type: Easing.OutCubic }
            }

            icon: {
                if (isMuted || volume === 0) return "volume_off"
                if (volume > 0.66)           return "volume_up"
                if (volume > 0.33)           return "volume_down"
                return "volume_mute"
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (Pipewire.defaultAudioSink?.audio) {
                    Pipewire.defaultAudioSink.audio.muted = !Pipewire.defaultAudioSink.audio.muted
                }
            }
        }
    }
}
