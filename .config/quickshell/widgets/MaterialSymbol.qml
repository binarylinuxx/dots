import QtQuick 

Text {
    property string icon: ""
    property int iconSize: 24
    property bool rounded: true
    property int weight: 400          // 100–700 обычно
    property real fill: 0             // 0..1
    property int grade: 0             // -50..200
    property int opticalSize: iconSize

    font.pixelSize: iconSize
    font.family: rounded ? "Material Symbols Rounded" : "Material Symbols Outlined"
    font.weight: weight
    font.variableAxes: [
        { tag: "FILL", value: fill },
        { tag: "wght", value: weight },
        { tag: "GRAD", value: grade },
        { tag: "opsz", value: opticalSize }
    ]
    text: icon
}

