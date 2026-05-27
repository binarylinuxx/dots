local home = os.getenv("HOME")
local xdg_config_home = os.getenv("XDG_CONFIG_HOME") or (home .. "/.config")
local hypr_dir = xdg_config_home .. "/hypr"

package.path = table.concat({
    hypr_dir .. "/?.lua",
    hypr_dir .. "/?/init.lua",
    package.path,
}, ";")

require("variables")
require("monitor")
require("autostart")
require("visual.general")
require("visual.decoration")
require("visual.layout")
require("visual.o2-vertical")
require("keyboard.kblayout")
require("keyboard.gestures")
require("keyboard.binds")
require("rules.layerrules")
require("rules.windowrules")

hl.config({
    misc = {
        disable_hyprland_guiutils_check = true,
        disable_hyprland_logo = true,
        disable_splash_rendering = true,
        force_default_wallpaper = 0,
    },
})
