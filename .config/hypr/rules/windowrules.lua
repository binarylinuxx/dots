hl.window_rule({
    name = "quickshell-settings",
    match = { class = "^org\\.quickshell$", title = "^Settings$" },
    float = true,
    center = true,
})

hl.window_rule({
    name = "pip",
    match = { title = "^Picture-in-Picture$" },
    float = true,
    pin = true,
    size = "480 270",
    move = "100%-490 100%-280",
})

hl.window_rule({
    name = "file-dialogs-basic",
    match = { title = "^(Open File|Save File|Open Folder)$" },
    float = true,
    center = true,
})

hl.window_rule({
    name = "file-dialogs-select",
    match = { title = "^Select" },
    float = true,
    center = true,
})

hl.window_rule({
    name = "pavucontrol",
    match = { class = "^pavucontrol$" },
    float = true,
    size = "800 500",
    center = true,
})

hl.window_rule({
    name = "nm-connection-editor",
    match = { class = "^nm-connection-editor$" },
    float = true,
    center = true,
})

hl.window_rule({
    name = "blueman-manager",
    match = { class = "^blueman-manager$" },
    float = true,
    center = true,
})

hl.window_rule({
    name = "calculators",
    match = { class = "^(org\\.gnome\\.Calculator|qalculate-gtk)$" },
    float = true,
})
