# blxshell completions for fish.
set -l commands settings launcher powermenu sidebar wallpapers lock reload media ping popout dashboard osd \
    pack inspect install search update repo remove theme plugin
set -l pages theme colours shape type apps bar sidebar panels power lock login desktop plugins about

function __blxshell_ids
    blxshell $argv list 2>/dev/null | string match -r '^\S+' 
end
function __blxshell_available
    blxshell search 2>/dev/null | string match -r '^\S+'
end
function __blxshell_installed
    __blxshell_ids theme; __blxshell_ids plugin
end

complete -c blxshell -f
complete -c blxshell -n "not __fish_seen_subcommand_from $commands" -a "$commands"
complete -c blxshell -n "__fish_seen_subcommand_from settings" -a "$pages"
complete -c blxshell -n "__fish_seen_subcommand_from launcher powermenu dashboard" -a "toggle open close"
complete -c blxshell -n "__fish_seen_subcommand_from sidebar" -a "both top bottom corner"
complete -c blxshell -n "__fish_seen_subcommand_from media" -a "play-pause pause next previous"
complete -c blxshell -n "__fish_seen_subcommand_from popout; and test (count (commandline -opc)) -eq 2" -a "wifi bluetooth audio battery"
complete -c blxshell -n "__fish_seen_subcommand_from popout; and test (count (commandline -opc)) -eq 3" -a "toggle open close"
complete -c blxshell -n "__fish_seen_subcommand_from pack inspect" -F
complete -c blxshell -n "__fish_seen_subcommand_from inspect" -a "(__blxshell_installed)"
complete -c blxshell -n "__fish_seen_subcommand_from install" -F -a "(__blxshell_available)"
complete -c blxshell -n "__fish_seen_subcommand_from install theme" -l force
complete -c blxshell -n "__fish_seen_subcommand_from update remove" -a "(__blxshell_installed)"
complete -c blxshell -n "__fish_seen_subcommand_from repo; and test (count (commandline -opc)) -eq 2" -a "list add remove"
complete -c blxshell -n "__fish_seen_subcommand_from theme; and test (count (commandline -opc)) -eq 2" -a "list apply reset"
complete -c blxshell -n "__fish_seen_subcommand_from theme; and __fish_seen_subcommand_from apply" -F -a "(__blxshell_ids theme)"
complete -c blxshell -n "__fish_seen_subcommand_from plugin; and test (count (commandline -opc)) -eq 2" -a "list status enable disable call restart"
complete -c blxshell -n "__fish_seen_subcommand_from plugin; and __fish_seen_subcommand_from enable disable call restart; and test (count (commandline -opc)) -eq 3" -a "(__blxshell_ids plugin)"
