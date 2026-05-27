hl.config({
    animations = {
        enabled = true,
    },
})

hl.curve("o2_shot", { type = "bezier", points = { { 0, 0.72 }, { 0.4, 1.15 } } })
hl.curve("o2_smooth", { type = "bezier", points = { { 0.4, 0 }, { 0.2, 1 } } })
hl.curve("wind_up", { type = "bezier", points = { { 0.1, 0.8 }, { 0.1, 1.1 } } })
hl.curve("bounce", { type = "bezier", points = { { 1.1, 1.2 }, { 0.1, 1.05 } } })
hl.curve("fluffy", { type = "bezier", points = { { 0.1, 1.3 }, { 0.1, 1.0 } } })
hl.curve("slingshot", { type = "bezier", points = { { 1, -0.15 }, { 0.75, 1.25 } } })
hl.curve("drop_in", { type = "bezier", points = { { 0.2, 1.5 }, { 0.2, 0.95 } } })
hl.curve("rise_up", { type = "bezier", points = { { 0.75, -0.25 }, { 0.25, 1.2 } } })

hl.animation({ leaf = "global", enabled = true, speed = 3, bezier = "o2_shot" })
hl.animation({ leaf = "windows", enabled = true, speed = 2, bezier = "drop_in", style = "slidevert" })
hl.animation({ leaf = "windowsOut", enabled = true, speed = 3, bezier = "rise_up", style = "slidevert" })
hl.animation({ leaf = "windowsMove", enabled = true, speed = 4, bezier = "wind_up", style = "slidevert" })
hl.animation({ leaf = "border", enabled = true, speed = 2, bezier = "o2_smooth" })
hl.animation({ leaf = "borderangle", enabled = true, speed = 100, bezier = "o2_shot", style = "loop" })
hl.animation({ leaf = "fade", enabled = true, speed = 4, bezier = "o2_smooth" })
hl.animation({ leaf = "workspaces", enabled = true, speed = 5, bezier = "slingshot", style = "slidevert" })
hl.animation({ leaf = "specialWorkspace", enabled = true, speed = 3, bezier = "bounce", style = "slidevert" })
hl.animation({ leaf = "fadeSwitch", enabled = true, speed = 5, bezier = "fluffy" })
hl.animation({ leaf = "fadeDim", enabled = true, speed = 3, bezier = "o2_smooth" })
