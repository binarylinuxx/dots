import Quickshell
import Quickshell.Wayland
import Quickshell.Io
import QtQuick
import qs.services

Scope {
    id: nightLightScope

    property real temperature: cfg ? cfg.nightLightTemperature : 0.5
    property real strength:    cfg ? cfg.nightLightStrength    : 0.8

    // Convert 0.0-1.0 slider to Kelvin: 0.0 = 6500K (neutral), 1.0 = 1800K (deep amber)
    property real kelvin: 6500.0 - temperature * 4700.0

    readonly property string shaderPath: Qt.resolvedUrl("../../.config/hypr/shaders/nightlight.glsl")
        .toString().replace("file://", "")
    readonly property string shaderTemplate: "/home/blx/.config/hypr/shaders/nightlight.glsl"
    readonly property string shaderRuntime: "/tmp/qs-nightlight.glsl"

    Process {
        id: applyProc
    }

    Process {
        id: removeProc
        command: ["hyprctl", "keyword", "decoration:screen_shader", "[[EMPTY]]"]
    }

    function apply() {
        // Sed the TEMPERATURE and STRENGTH defines into a runtime copy of the shader
        applyProc.command = [
            "sh", "-c",
            "sed 's/#define TEMPERATURE.*/#define TEMPERATURE " + Math.round(nightLightScope.kelvin) + ".0/' " +
            shaderTemplate + " | sed 's/#define STRENGTH.*/#define STRENGTH " + nightLightScope.strength.toFixed(2) + "/' " +
            "> " + shaderRuntime +
            " && hyprctl keyword decoration:screen_shader " + shaderRuntime
        ]
        applyProc.running = true
    }

    function remove() {
        removeProc.running = true
    }

    // React to toggle
    Connections {
        target: Gstate
        function onNightLightEnabledChanged() {
            if (Gstate.nightLightEnabled) nightLightScope.apply()
            else nightLightScope.remove()
        }
    }

    // React to slider changes while active
    Connections {
        target: cfg ? cfg : null
        function onNightLightTemperatureChanged() {
            if (Gstate.nightLightEnabled) nightLightScope.apply()
        }
        function onNightLightStrengthChanged() {
            if (Gstate.nightLightEnabled) nightLightScope.apply()
        }
    }

    // Apply on startup if enabled
    Component.onCompleted: {
        if (Gstate.nightLightEnabled) nightLightScope.apply()
    }
}
