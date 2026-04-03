pragma Singleton
import QtQml

QtObject {
    property bool appsOpen: false
    property bool settingsOpen: false
    property bool sidebarOpen: false
    property bool dndEnabled: false
    property bool nightLightEnabled: false
    property bool weatherDetailOpen: false
    property bool idle: false

    // Global animation duration multiplier — 0 = disabled, driven by cfg.animationSpeed
    property int animDuration: 250
}
