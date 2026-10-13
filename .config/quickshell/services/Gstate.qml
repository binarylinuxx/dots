pragma Singleton
import QtQml

QtObject {
    property bool appsOpen: false
    property bool settingsOpen: false
    property bool sidebarOpen: false
    property bool screenshotOpen: false
    property bool powerMenuOpen: false
    property bool dndEnabled: false
    property bool nightLightEnabled: false
    property bool weatherDetailOpen: false
    property bool idle: false

    // Global animation duration multiplier — 0 = disabled, driven by cfg.animationSpeed
    property int animDuration: 250

    // Canonical shell geometry. shell.qml binds the raw configuration and root
    // frame thickness here; consumers use the resolved edge insets below.
    property string barPosition: "bottom"
    property string barPreset: "horizontal"
    property int barHeight: 35
    property int barWidth: 46
    property int barGap: 5
    property bool barFloating: false
    property int frameThickness: 15
    readonly property bool verticalBar: barPreset.indexOf("vertical") === 0 || barPosition === "left" || barPosition === "right"
    readonly property string barEdge: verticalBar ? (barPosition === "right" ? "right" : "left") : (barPosition === "top" ? "top" : "bottom")
    readonly property int barThickness: verticalBar ? barWidth : barHeight
    readonly property int barOccupiedInset: barFloating ? barThickness + barGap : barThickness
    readonly property int topInset: Math.max(frameThickness, barEdge === "top" ? barOccupiedInset : 0)
    readonly property int bottomInset: Math.max(frameThickness, barEdge === "bottom" ? barOccupiedInset : 0)
    readonly property int leftInset: Math.max(frameThickness, barEdge === "left" ? barOccupiedInset : 0)
    readonly property int rightInset: Math.max(frameThickness, barEdge === "right" ? barOccupiedInset : 0)
}
