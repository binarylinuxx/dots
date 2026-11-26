# blxshell completions for nushell.
def "nu-complete blxshell ids" [kind: string] { ^blxshell $kind list | lines | each { split row ' ' | first } }
def "nu-complete blxshell installed" [] { (nu-complete blxshell ids theme) ++ (nu-complete blxshell ids plugin) }
def "nu-complete blxshell available" [] { ^blxshell search | lines | where { $in !~ '^\s' } | each { split row ' ' | first } }
def "nu-complete blxshell pages" [] { [theme colours shape type apps bar sidebar panels power lock desktop plugins about] }
def "nu-complete blxshell actions" [] { [toggle open close] }

export extern "blxshell settings" [page?: string@"nu-complete blxshell pages"]
export extern "blxshell launcher" [action?: string@"nu-complete blxshell actions"]
export extern "blxshell powermenu" [action?: string@"nu-complete blxshell actions"]
export extern "blxshell dashboard" [action?: string@"nu-complete blxshell actions"]
export extern "blxshell sidebar" [part?: string@"nu-complete blxshell sidebar"]
def "nu-complete blxshell sidebar" [] { [both top bottom corner] }
export extern "blxshell media" [action: string@"nu-complete blxshell media"]
def "nu-complete blxshell media" [] { [play-pause pause next previous] }
export extern "blxshell popout" [name: string@"nu-complete blxshell popouts", action?: string@"nu-complete blxshell actions"]
def "nu-complete blxshell popouts" [] { [wifi bluetooth audio battery] }
export extern "blxshell install" [package: string@"nu-complete blxshell available", --force]
export extern "blxshell inspect" [package: string@"nu-complete blxshell installed"]
export extern "blxshell update" [...packages: string@"nu-complete blxshell installed"]
export extern "blxshell remove" [package: string@"nu-complete blxshell installed"]
export extern "blxshell theme apply" [theme: string@"nu-complete blxshell theme ids", --force]
def "nu-complete blxshell theme ids" [] { nu-complete blxshell ids theme }
export extern "blxshell plugin enable" [plugin: string@"nu-complete blxshell plugin ids"]
export extern "blxshell plugin disable" [plugin: string@"nu-complete blxshell plugin ids"]
export extern "blxshell plugin restart" [plugin: string@"nu-complete blxshell plugin ids"]
export extern "blxshell plugin call" [plugin: string@"nu-complete blxshell plugin ids", command: string, ...args: string]
def "nu-complete blxshell plugin ids" [] { nu-complete blxshell ids plugin }
