import QtQuick
import QtQuick.Layouts
import Quickshell.Io
import qs.services
import qs.widgets
import qs.RoundedPolygon

Item {
	id: root
	property var plugin: null
	property string pluginPath: plugin ? plugin.path : ""

	// ── Config ───────────────────────────────────────────────────────────────
	FileView {
		path: pluginPath ? "file://" + pluginPath + "/config.json" : ""
		watchChanges: true
		onFileChanged: reload()
		adapter: JsonAdapter {
			id: cfg
			property int widgetSize:    260
			property int refreshMs:     2000
			property bool showLabels:   true
			property bool showHostname: true
			property int cookieSides:   8
		}
	}

	readonly property int sz: cfg.widgetSize

	// ── Stats state ──────────────────────────────────────────────────────────
	property string hostname: ""
	property real cpuSmooth:  0
	property real ramSmooth:  0
	property real diskSmooth: 0
	Behavior on cpuSmooth  { NumberAnimation { duration: 600; easing.type: Easing.OutCubic } }
	Behavior on ramSmooth  { NumberAnimation { duration: 600; easing.type: Easing.OutCubic } }
	Behavior on diskSmooth { NumberAnimation { duration: 600; easing.type: Easing.OutCubic } }

	// ── Processes ────────────────────────────────────────────────────────────
	Process {
		id: cpuProc
		command: ["python3", "-c",
			"import time; " +
			"f=open('/proc/stat'); l1=f.readline().split(); f.close(); " +
			"time.sleep(0.4); " +
			"f=open('/proc/stat'); l2=f.readline().split(); f.close(); " +
			"idle1=int(l1[4])+int(l1[5]); idle2=int(l2[4])+int(l2[5]); " +
			"tot1=sum(int(x) for x in l1[1:8]); tot2=sum(int(x) for x in l2[1:8]); " +
			"print(round(100*(1-(idle2-idle1)/(tot2-tot1))))"
		]
		stdout: StdioCollector {
			onStreamFinished: {
				var v = parseInt(text.trim())
				if (!isNaN(v)) root.cpuSmooth = v
			}
		}
	}

	Process {
		id: ramProc
		command: ["bash", "-c",
			"awk '/MemTotal/{t=$2} /MemAvailable/{a=$2} END{printf \"%d\", (t-a)*100/t}' /proc/meminfo"
		]
		stdout: StdioCollector {
			onStreamFinished: {
				var v = parseInt(text.trim())
				if (!isNaN(v)) root.ramSmooth = v
			}
		}
	}

	Process {
		id: diskProc
		command: ["bash", "-c", "df / --output=pcent | tail -1 | tr -d '% '"]
		stdout: StdioCollector {
			onStreamFinished: {
				var v = parseInt(text.trim())
				if (!isNaN(v)) root.diskSmooth = v
			}
		}
	}

	Process {
		command: ["hostname"]
		running: true
		stdout: StdioCollector {
			onStreamFinished: root.hostname = text.trim()
		}
	}

	Timer {
		interval: cfg.refreshMs
		running: true
		repeat: true
		triggeredOnStart: true
		onTriggered: { cpuProc.running = true; ramProc.running = true; diskProc.running = true }
	}

	// ── Visuals ──────────────────────────────────────────────────────────────
	Item {
		anchors.centerIn: parent
		width: sz; height: sz

		// Cookie face
		ParametricCookie {
			anchors.centerIn: parent
			size: sz
			sides:         cfg.cookieSides
			innerRatio:    0.82
			outerRounding: 0.40
			innerRounding: 0.40
			color:         col.primaryContainer
			morphDuration: 800
			strokeWidth:   0
		}

		// ── Track rings (background) ──────────────────────────────────────────
		Canvas {
			anchors.centerIn: parent; width: sz; height: sz
			Component.onCompleted: requestPaint()
			onPaint: {
				var ctx = getContext("2d")
				ctx.clearRect(0, 0, width, height)
				var cx = width/2, cy = height/2
				var rings = [{ r: sz*0.400, w: 14 }, { r: sz*0.310, w: 12 }, { r: sz*0.225, w: 10 }]
				var c = col.onPrimaryContainer
				ctx.strokeStyle = Qt.rgba(c.r, c.g, c.b, 0.12)
				ctx.lineCap = "round"
				for (var i = 0; i < rings.length; i++) {
					ctx.beginPath()
					ctx.arc(cx, cy, rings[i].r, 0, Math.PI * 2)
					ctx.lineWidth = rings[i].w
					ctx.stroke()
				}
			}
		}

		// ── CPU arc (outer, primary) ──────────────────────────────────────────
		Canvas {
			anchors.centerIn: parent; width: sz; height: sz
			property real pct: root.cpuSmooth / 100
			onPctChanged: requestPaint()
			onPaint: {
				var ctx = getContext("2d")
				ctx.clearRect(0, 0, width, height)
				if (pct <= 0) return
				ctx.beginPath()
				ctx.arc(width/2, height/2, sz*0.400, -Math.PI/2, -Math.PI/2 + pct*Math.PI*2)
				ctx.strokeStyle = col.primary
				ctx.lineWidth = 14; ctx.lineCap = "round"; ctx.stroke()
			}
		}

		// ── RAM arc (middle, tertiary) ────────────────────────────────────────
		Canvas {
			anchors.centerIn: parent; width: sz; height: sz
			property real pct: root.ramSmooth / 100
			onPctChanged: requestPaint()
			onPaint: {
				var ctx = getContext("2d")
				ctx.clearRect(0, 0, width, height)
				if (pct <= 0) return
				ctx.beginPath()
				ctx.arc(width/2, height/2, sz*0.310, -Math.PI/2, -Math.PI/2 + pct*Math.PI*2)
				ctx.strokeStyle = col.tertiary
				ctx.lineWidth = 12; ctx.lineCap = "round"; ctx.stroke()
			}
		}

		// ── Disk arc (inner, secondary) ───────────────────────────────────────
		Canvas {
			anchors.centerIn: parent; width: sz; height: sz
			property real pct: root.diskSmooth / 100
			onPctChanged: requestPaint()
			onPaint: {
				var ctx = getContext("2d")
				ctx.clearRect(0, 0, width, height)
				if (pct <= 0) return
				ctx.beginPath()
				ctx.arc(width/2, height/2, sz*0.225, -Math.PI/2, -Math.PI/2 + pct*Math.PI*2)
				ctx.strokeStyle = col.secondary
				ctx.lineWidth = 10; ctx.lineCap = "round"; ctx.stroke()
			}
		}

		// ── Center labels ─────────────────────────────────────────────────────
		Column {
			anchors.centerIn: parent
			spacing: 5
			width: sz * 0.30

			Text {
				anchors.horizontalCenter: parent.horizontalCenter
				visible: cfg.showHostname
				text: root.hostname.toUpperCase()
				color: col.onPrimaryContainer
				opacity: 0.35
				font.pixelSize: 7
				font.family: "Rubik"
				font.weight: Font.Medium
				font.letterSpacing: 1.0
				width: parent.width
				wrapMode: Text.WrapAnywhere
				horizontalAlignment: Text.AlignHCenter
			}

			Rectangle {
				visible: cfg.showHostname && cfg.showLabels
				anchors.horizontalCenter: parent.horizontalCenter
				width: parent.width * 0.7; height: 1
				color: col.onPrimaryContainer; opacity: 0.15
			}

			Row {
				visible: cfg.showLabels
				anchors.horizontalCenter: parent.horizontalCenter
				spacing: 4
				Rectangle { width: 6; height: 6; radius: 3; color: col.primary; anchors.verticalCenter: parent.verticalCenter }
				Text { text: "CPU"; color: col.onPrimaryContainer; opacity: 0.6; font.pixelSize: 10; font.family: "Rubik" }
				Text { text: Math.round(root.cpuSmooth) + "%"; color: col.primary; font.pixelSize: 12; font.family: "Rubik"; font.weight: Font.Bold }
			}
			Row {
				visible: cfg.showLabels
				anchors.horizontalCenter: parent.horizontalCenter
				spacing: 4
				Rectangle { width: 6; height: 6; radius: 3; color: col.tertiary; anchors.verticalCenter: parent.verticalCenter }
				Text { text: "RAM"; color: col.onPrimaryContainer; opacity: 0.6; font.pixelSize: 10; font.family: "Rubik" }
				Text { text: Math.round(root.ramSmooth) + "%"; color: col.tertiary; font.pixelSize: 12; font.family: "Rubik"; font.weight: Font.Bold }
			}
			Row {
				visible: cfg.showLabels
				anchors.horizontalCenter: parent.horizontalCenter
				spacing: 4
				Rectangle { width: 6; height: 6; radius: 3; color: col.secondary; anchors.verticalCenter: parent.verticalCenter }
				Text { text: "DSK"; color: col.onPrimaryContainer; opacity: 0.6; font.pixelSize: 10; font.family: "Rubik" }
				Text { text: Math.round(root.diskSmooth) + "%"; color: col.secondary; font.pixelSize: 12; font.family: "Rubik"; font.weight: Font.Bold }
			}
		}
	}
}
