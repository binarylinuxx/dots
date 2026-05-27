local home = os.getenv("HOME")

terminal = "ghostty --gtk-single-instance=true"
fileManager = "dolphin"
menu = "rofi -show drun -theme " .. home .. "/.config/rofi/custom_theme.rasi"
mainMod = "SUPER"
turnMod = "RETURN"

hl.env("XCURSOR_SIZE", "24")
hl.env("HYPRCURSOR_SIZE", "24")
hl.env("ELECTRON_OZONE_PLATFORM_HINT", "auto")
hl.env("BLXSHELL_PATH", home .. "/.local/blxshell")
