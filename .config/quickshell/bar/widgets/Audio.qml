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
        width: audioIcon.width + (taskbarOpen ? 20 : 10)
        height: 28
        anchors.centerIn: parent
        radius: 14
        color: /*taskbarOpen ? col.primary :*/ "transparent"
        
        Behavior on color {
            ColorAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
        
        Behavior on width {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        MaterialSymbol {
            id: audioIcon
            anchors.centerIn: parent
            color: /*taskbarOpen ? col.onPrimary :*/ col.primary
            iconSize: 20

            Behavior on color {
                ColorAnimation { duration: 200; easing.type: Easing.OutCubic }
            }

            icon: {
                if (isMuted || volume === 0) {
                    return "volume_off"  // Muted or 0%
                } else if (volume > 0.66) {
                    return "volume_up"   // High (>66%)
                } else if (volume > 0.33) {
                    return "volume_down" // Medium (33-66%)
                } else {
                    return "volume_mute" // Low (1-33%)
                }
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
