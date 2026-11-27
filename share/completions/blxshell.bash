# blxshell completions for bash.
_blxshell() {
    local cur=${COMP_WORDS[COMP_CWORD]} first=${COMP_WORDS[1]} second=${COMP_WORDS[2]}
    local ids
    _blxshell_ids() { blxshell "$1" list 2>/dev/null | awk '{print $1}'; }
    if (( COMP_CWORD == 1 )); then
        COMPREPLY=($(compgen -W "settings launcher powermenu sidebar wallpapers lock reload media ping popout dashboard osd pack inspect install search update repo remove theme plugin" -- "$cur"))
        return
    fi
    case "$first:$COMP_CWORD" in
        settings:2) ids="theme colours shape type apps bar sidebar panels power lock login desktop plugins about" ;;
        launcher:2|powermenu:2|dashboard:2|popout:3) ids="toggle open close" ;;
        sidebar:2) ids="both top bottom corner" ;;
        media:2) ids="play-pause pause next previous" ;;
        popout:2) ids="wifi bluetooth audio battery" ;;
        repo:2) ids="list add remove" ;;
        theme:2) ids="list apply reset" ;;
        plugin:2) ids="list status enable disable call restart" ;;
        theme:3) [[ $second == apply ]] && ids="$(_blxshell_ids theme) --force"; COMPREPLY=($(compgen -f -- "$cur")) ;;
        plugin:3) [[ $second != list && $second != status ]] && ids=$(_blxshell_ids plugin) ;;
        install:*) ids="$(blxshell search 2>/dev/null | awk '/^[^ ]/{print $1}') --force"; COMPREPLY=($(compgen -f -- "$cur")) ;;
        inspect:*|update:*|remove:*) ids="$(_blxshell_ids theme) $(_blxshell_ids plugin)" ;;
        pack:*) COMPREPLY=($(compgen -d -- "$cur")); return ;;
    esac
    COMPREPLY+=($(compgen -W "$ids" -- "$cur"))
}
complete -F _blxshell blxshell
