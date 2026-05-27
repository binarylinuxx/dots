hl.layer_rule({
    name = "quickshell_popup_no_compositor_animation",
    match = { namespace = "^quickshell:popup:" },
    animation = "none",
})

hl.layer_rule({
    name = "background_slide",
    match = { namespace = "^quickshell:background$" },
    animation = "slide left",
})
