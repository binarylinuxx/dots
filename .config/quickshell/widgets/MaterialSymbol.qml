import QtQuick 

Text {
    property string icon: ""
    property int iconSize: 24  // Default size, can be overridden
    property bool rounded: true  // Use Rounded variant by default
    font.pixelSize: iconSize
    font.family: rounded ? "Material Symbols Rounded" : "Material Symbols Outlined"
    text: icon
}

