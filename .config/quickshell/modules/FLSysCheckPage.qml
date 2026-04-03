import QtQuick
import QtQuick.Layouts
import Quickshell.Io
import qs.widgets
import qs.services

Item {
	id: sysCheckPage

	property var checks: [
		{ name: "quickshell (qs)", cmd: "qs",      status: "pending", detail: "" },
		{ name: "ghostty",         cmd: "ghostty", status: "pending", detail: "" },
		{ name: "pactl / audio",   cmd: "",        status: "pending", detail: "" },
		{ name: "curl",            cmd: "curl",    status: "pending", detail: "" },
		{ name: "nmcli",           cmd: "nmcli",   status: "pending", detail: "" },
		{ name: "jq",              cmd: "jq",      status: "pending", detail: "" }
	]
	property int checkIndex: 0

	Component.onCompleted: Qt.callLater(runNextCheck)

	function runNextCheck() {
		if (checkIndex >= checks.length) return
		if (checkIndex === 2) {
			pactlProc.running = true
		} else {
			sysCheckProc.command = ["sh", "-c", "command -v " + checks[checkIndex].cmd + " 2>/dev/null && echo ok || echo missing"]
			sysCheckProc.running = true
		}
	}

	function markCurrent(ok, detail) {
		var arr = checks.slice()
		arr[checkIndex] = { name: arr[checkIndex].name, cmd: arr[checkIndex].cmd, status: ok ? "ok" : "missing", detail: detail || "" }
		checks = arr
		checkIndex++
		Qt.callLater(runNextCheck)
	}

	Process {
		id: sysCheckProc
		stdout: StdioCollector {
			onStreamFinished: {
				var out = text.trim()
				var ok = out.length > 0 && !out.startsWith("missing")
				sysCheckPage.markCurrent(ok, ok ? out.split("\n")[0] : "")
			}
		}
	}

	Process {
		id: pactlProc
		command: ["sh", "-c", "pactl info 2>/dev/null | head -2"]
		stdout: StdioCollector {
			onStreamFinished: {
				var out = text.trim()
				sysCheckPage.markCurrent(out.length > 0, out.split("\n")[0] || "")
			}
		}
	}

	ColumnLayout {
		anchors.fill: parent
		spacing: 10

		Text {
			text: checkIndex < checks.length ? "Checking required programs..." : "Check complete"
			font.pixelSize: 14
			font.family: cfg ? cfg.fontFamily : "Rubik"
			font.weight: 600
			color: col.onSurfaceVariant
		}

		Repeater {
			model: checks
			Rectangle {
				Layout.fillWidth: true
				height: 54
				radius: 12
				color: modelData.status === "ok" ? Qt.rgba(0.1, 0.8, 0.3, 0.08)
					: (modelData.status === "missing" ? Qt.rgba(0.9, 0.2, 0.2, 0.08) : col.surfaceContainer)
				border.width: 1
				border.color: modelData.status === "ok" ? col.primary : (modelData.status === "missing" ? col.error : col.outlineVariant)

				RowLayout {
					anchors { fill: parent; margins: 14 }
					spacing: 12
					MaterialSymbol {
						icon: modelData.status === "ok" ? "check_circle" : (modelData.status === "missing" ? "cancel" : "hourglass_empty")
						iconSize: 22
						color: modelData.status === "ok" ? col.primary : (modelData.status === "missing" ? col.error : col.onSurfaceVariant)
					}
					ColumnLayout {
						Layout.fillWidth: true
						spacing: 1
						Text { text: modelData.name; font.pixelSize: 14; font.weight: 600; font.family: cfg ? cfg.fontFamily : "Rubik"; color: col.onSurface }
						Text {
							text: modelData.status === "ok" ? (modelData.detail.length > 0 ? modelData.detail : "Found")
								: (modelData.status === "missing" ? "Not found" : "Checking...")
							font.pixelSize: 11
							font.family: "JetBrains Mono"
							color: modelData.status === "ok" ? col.primary : (modelData.status === "missing" ? col.error : col.onSurfaceVariant)
							elide: Text.ElideRight
							Layout.fillWidth: true
						}
					}
				}
			}
		}

		Item { Layout.fillHeight: true }

		Rectangle {
			Layout.fillWidth: true
			height: summaryRow.implicitHeight + 20
			radius: 12
			color: col.surfaceContainerHigh
			visible: checkIndex >= checks.length

			RowLayout {
				id: summaryRow
				anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 14 }
				spacing: 10
				MaterialSymbol {
					icon: checks.filter(function(c) { return c.status === "missing" }).length === 0 ? "verified" : "warning"
					iconSize: 22
					color: checks.filter(function(c) { return c.status === "missing" }).length === 0 ? col.primary : col.error
				}
				Text {
					Layout.fillWidth: true
					text: {
						var missing = checks.filter(function(c) { return c.status === "missing" })
						if (missing.length === 0) return "All systems ready. Click Finish to start using your shell."
						return missing.map(function(c) { return c.name }).join(", ") + " not found - you can still continue."
					}
					font.pixelSize: 13
					font.family: cfg ? cfg.fontFamily : "Rubik"
					font.weight: 500
					color: col.onSurface
					wrapMode: Text.WordWrap
				}
			}
		}
	}
}
