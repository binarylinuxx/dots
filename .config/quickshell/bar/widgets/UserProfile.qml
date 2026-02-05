import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Widgets
import qs.services

Item {
    anchors.fill: parent

    property int moduleRadius: cfg ? Math.max(8, Math.round(cfg.barRadius * 0.7)) : 14
    property string fontFamily: cfg ? cfg.fontFamily : "Rubik"

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: 1 
        anchors.verticalCenter: parent.verticalCenter
        width: userProfile.width + 3
        height: 33
        radius: moduleRadius
        color: col.background

        Row {
            id: userProfile
            spacing: 3
            leftPadding: 3
            rightPadding: 0
            anchors.verticalCenter: parent.verticalCenter

            ClippingRectangle {
                width: 28
                height: 28
                radius: moduleRadius
                Image {
                    fillMode: Image.PreserveAspectCrop
                    source: "cat.png"
                    anchors.fill: parent
                }
            }

            ClippingRectangle {
                width: user.width + 16
                height: 28
                radius: moduleRadius
                color: col.surfaceContainer

                Text {
                    id: user
                    text: Quickshell.env("USER")
                    font.pixelSize: 16
                    font.family: fontFamily
                    font.weight: 700
                    color: col.primary
                    anchors.centerIn: parent
                }
            }
        }
    }
}
