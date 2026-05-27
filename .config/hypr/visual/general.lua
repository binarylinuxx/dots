local colors = require("colors")

hl.config({
    general = {
        gaps_in = 3,
        gaps_out = 5,
        border_size = 2,
        col = {
            inactive_border = colors.on_primary,
            active_border = {
                colors = {
                    colors.primary,
                    colors.surface_container,
                    colors.primary,
                    colors.surface_container,
                    colors.primary,
                    colors.surface_container,
                    colors.primary,
                    colors.surface_container,
                    colors.primary,
                    colors.surface_container,
                },
                angle = 45,
            },
        },
        resize_on_border = false,
        allow_tearing = false,
        layout = "dwindle",
    },
})
