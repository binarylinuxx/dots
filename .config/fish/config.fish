starship init fish | source
set -x PATH /opt/zig $PATH

# opperator
fish_add_path /home/blx/.opperator/bin

# bun
set --export BUN_INSTALL "$HOME/.bun"
set --export PATH $BUN_INSTALL/bin $PATH

# ZVM
set -gx PATH "$HOME/.zvm/bin" "$HOME/.zvm/self" $PATH
set -gx PATH $HOME/.zap/bin $PATH
