// Quick sanity test. Run with:
//   QML_IMPORT_PATH=$PWD/build/qml qs -c test.qml
// (or: qs --path ./ -c plugin_manager/test.qml from repo root)
import QtQuick
import Quickshell
import qs.PluginManager

ShellRoot {
    Component.onCompleted: {
        console.log("[test] pluginsDir:", PluginRegistry.pluginsDir)
        console.log("[test] count:", PluginRegistry.count)
        for (var i = 0; i < PluginRegistry.count; i++) {
            var idx = PluginRegistry.index(i, 0)
            console.log("  -", PluginRegistry.data(idx, 257 /* IdRole */),
                        "valid=", PluginRegistry.data(idx, 267),
                        "error=", PluginRegistry.data(idx, 268))
        }
        var bars = PluginRegistry.byKind("bar")
        console.log("[test] bar plugins:", JSON.stringify(bars, null, 2))
    }
}
