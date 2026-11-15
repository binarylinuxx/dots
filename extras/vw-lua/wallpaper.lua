local wallpaper = require("vw.wallpaper")
local base = require("vw.base")
local hyprland = require("vw.services.hyprland")

local width, height = 1920, 1080
if hyprland.available() then
  local ok, monitors = pcall(hyprland.monitors)
  if ok and monitors[1] then
    local monitor = monitors[1]
    local scale = monitor.scale or 1
    --width = math.floor((monitor.width or width) / scale)
    --height = math.floor((monitor.height or height) / scale)
  end
end

local background = base.rectangle({
  width = width,
  height = height,
  color = { 0.025, 0.04, 0.065, 1.0 },
})

local image = wallpaper.set({
  path = "/home/blx/.local/wallpapers/vulcan.jpg",
  width = width,
  height = height,
})

base.stack({
  width = width,
  height = height,
  anchor = "top_left",
  layer = "bottom",
  children = { background, image },
})
