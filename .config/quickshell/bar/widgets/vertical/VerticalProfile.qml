import Quickshell
import Quickshell.Widgets
import QtQuick
import QtQuick.Layouts
import qs.services

ClippingRectangle {
    id: root

    property int itemSize: 34
    property int moduleRadius: cfg ? Math.max(10, Math.round(cfg.barRadius * 0.55)) : 16

    Layout.alignment: Qt.AlignHCenter
    Layout.preferredWidth: itemSize
    Layout.preferredHeight: itemSize
    radius: moduleRadius

    Image {
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        source: "../cat.png"
    }
}
