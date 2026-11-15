local base = require("vw.base")
local toplevel = require("vw.toplevel")

toplevel.set({ title = "vw — Liquid Glass", app_id = "vw-glass", width = 640, height = 420 })
local size = toplevel.size()
base.rectangle({ width = size.width, height = size.height,
                 color = { 0.025, 0.05, 0.09, 1 } })
for index = 0, 11 do
  base.rectangle({ x = index * 56, y = 48 + (index % 3) * 36,
                   width = 40, height = 280, radius = 18,
                   color = { 0.1 + index * 0.04, 0.65, 0.8, 1 },
                   color2 = { 0.7, 0.16, 0.25, 1 }, gradient = "vertical" })
end
base.rectangle({ x = 70, y = 100, width = math.max(120, size.width - 140), height = 210,
                 radius = 34, border_width = 1,
                 border_color = { 1, 1, 1, 0.12 }, color = { 0.1, 0.15, 0.2, 0.04 },
                 shader = "build/liquid_glass.comp.spv", shader_params = { 0.12, 0.3, 2, 0.98 } })
base.text({ text = "Liquid glass", font = "sans-serif", size = 26,
            x = 100, y = 150, color = { 1, 1, 1, 1 } })
base.text({ text = "Rounded lens • refraction • chromatic shift", font = "sans-serif", size = 14,
            x = 100, y = 210, color = { 0.88, 0.94, 1, 1 } })
