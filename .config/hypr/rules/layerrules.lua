hl.layer_rule({
    name = "blxshell_no_compositor_animation",
    match = { namespace = "^blxshell$" },
    animation = "none",
})

hl.layer_rule({
    name = "background_slide",
    match = { namespace = "^blxshell-background$" },
    animation = "slide left",
})
