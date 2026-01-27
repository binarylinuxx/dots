pragma Singleton
import QtQml
import Quickshell
import Quickshell.Io

QtObject {
	property string name: ""
	property string id: ""
	property string prettyName: ""
	property string homeUrl: ""
	property string documentationUrl: ""
	property string logo: ""
	property string ansiColor: ""
	property bool loaded: false

	property var _process: Process {
		command: ["cat", "/etc/os-release"]
		stdout: SplitParser {
			onRead: data => {
				var line = data.trim()
				if (!line || line.startsWith("#")) return
				
				var eqIndex = line.indexOf("=")
				if (eqIndex < 0) return
				
				var key = line.substring(0, eqIndex).trim()
				var value = line.substring(eqIndex + 1).trim()
				
				// Remove quotes if present
				if ((value.startsWith('"') && value.endsWith('"')) ||
					(value.startsWith("'") && value.endsWith("'"))) {
					value = value.substring(1, value.length - 1)
				}
				
				switch (key) {
					case "NAME": name = value; break
					case "ID": id = value; break
					case "PRETTY_NAME": prettyName = value; break
					case "HOME_URL": homeUrl = value; break
					case "DOCUMENTATION_URL": documentationUrl = value; break
					case "LOGO": logo = value; break
					case "ANSI_COLOR": ansiColor = value; break
				}
			}
		}
		onExited: {
			loaded = true
		}
	}

	Component.onCompleted: {
		_process.running = true
	}
}
