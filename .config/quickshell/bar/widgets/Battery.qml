import QtQuick
import Quickshell
import Quickshell.Services.UPower

Item {
	width: batteryText.width + 10
	visible: UPower.displayDevice && UPower.displayDevice.isLaptopBattery

	property real batteryPercentage: UPower.displayDevice?.percentage ?? 0
	property int batteryState: UPower.displayDevice?.state ?? 0
	property bool isCharging: batteryState === 1 // UPowerDeviceState.Charging

	Text {
		id: batteryText
		anchors.centerIn: parent
		color: col.foreground
		font.family: "FiraCode Nerd Font"
		font.pixelSize: 15
		font.weight: 800
		text: {
			if (isCharging) {
				return "󰂄"
			} else if (batteryPercentage > 90) {
				return "󰁹"
			} else if (batteryPercentage > 80) {
				return "󰂂"
			} else if (batteryPercentage > 70) {
				return "󰂁"
			} else if (batteryPercentage > 60) {
				return "󰂀"
			} else if (batteryPercentage > 50) {
				return "󰁿"
			} else if (batteryPercentage > 40) {
				return "󰁾"
			} else if (batteryPercentage > 30) {
				return "󰁽"
			} else if (batteryPercentage > 20) {
				return "󰁼"
			} else if (batteryPercentage > 10) {
				return "󰁻"
			} else {
				return "󰁺"
			}
		}
	}
}
