import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Widgets
import Quickshell.Services.Pipewire

Scope {
  id: root

  PwObjectTracker {
    objects: [Pipewire.defaultAudioSink ]
  }

  Connections {
    target: Pipewire.defaultAudioSink?.audio 

    function onVolumeChanged() {
      root.shouldShowOsd = true;
      hideTimer.restart();
    }
  }

  property bool shouldShowOsd: false
  property int integerVolume: Pipewire.defaultAudioSink?.audio.volume * 100

  Timer {
    id: hideTimer
    interval: 1000
    onTriggered: root.shouldShowOsd = false
  }

  LazyLoader {
    active: root.shouldShowOsd

    PanelWindow {
      anchors.right: true
      margins.right: 5
      exclusiveZone: 0
      width: 50
      height: screen.height / 3
      color: "transparent"
      Rectangle {
        id: mainBox
        anchors.fill: parent
        radius: 50
        color: col.background
        ColumnLayout {
          anchors {
            fill: parent
            leftMargin: 5
            bottomMargin: 5
            topMargin: 5
          }
          Rectangle {
            width: 40
            height: 40
            anchors.bottom: parent.bottom
            radius: 50
            color: col.surfaceContainer
            Text {
              text: integerVolume ?? 0
              anchors.centerIn: parent
              font.pixelSize: 20
              font.weight: 650
              font.family: cfg ? cfg.fontFamily : "Rubik"
              color: col.primary
            }
          }
          ClippingRectangle {
            height: mainBox.height - 55
            width: 40
            anchors.top: parent.top
            radius: 40
            color: col.surfaceContainer
            Rectangle {
              anchors.bottom: parent.bottom
              width: 40
              height: parent.height * (Pipewire.defaultAudioSink?.audio.volume ?? 0)
              color: col.primary
              radius: 40
              Behavior on height {
                NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
              }
            }
            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              y: parent.height * (1 - (Pipewire.defaultAudioSink?.audio.volume ?? 0)) - 10
              visible: (Pipewire.defaultAudioSink?.audio.volume ?? 0) > 0.05
              font.pixelSize: 40
              font.family: "FiraCode Nerd Font"
              color: /*Pipewire.defaultAudioSink?.audio.muted ? col.error : col.surfaceContainer*/ "transparent"

              Behavior on y {
                NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
              }

              text: {
                if (Pipewire.defaultAudioSink?.audio.muted) {
                  return "󰝟"
                } else if (integerVolume > 66) {
                  return "󰕾"
                } else if (integerVolume > 33) {
                  return "󰖀"
                } else {
                  return "󰕿"
                }
              }
            }
          }
        }
      }
    }
  }
}
