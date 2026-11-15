#!/usr/bin/env bash
# blxshell installer, after runix-installer: one centred column under the
# wordmark, one question per screen, a review, then the install with its
# phases, a progress bar and the live log. Questions are gum; the install
# screen is drawn here.
#
#   ./install.sh              install on Arch (or an Arch-based distro)
#   ./install.sh --demo       walk through it without changing anything
#   ./install.sh --demo-fail  the same, ending in a failed install
#
# NixOS uses the flake instead; the installer says how and stops.
set -uo pipefail

readonly DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly LOG="${XDG_CACHE_HOME:-$HOME/.cache}/blxshell-install.log"
readonly THEME_DIR=/var/lib/blxshell-greeter
readonly WIKI=https://bshell.zapzig.org

DEMO=0
DEMO_FAIL=0
for argument in "$@"; do
  case "$argument" in
    --demo) DEMO=1 ;;
    --demo-fail) DEMO=1 DEMO_FAIL=1 ;;
    -h | --help) sed -n '2,11p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "install.sh: unknown argument: $argument" >&2; exit 2 ;;
  esac
done

# Named colours, so the terminal's palette decides the shades.
readonly ACCENT=5 LIGHT=15 MUTED=8 WARN=3 ERROR=1
readonly COLUMN=72

# ── Where we are ─────────────────────────────────────────────────────────────

os_field() { (. /etc/os-release 2>/dev/null && eval "printf '%s' \"\${$1:-}\""); }

distro() {
  if [[ -e /etc/NIXOS ]] || [[ "$(os_field ID)" == nixos ]]; then
    echo nixos
  elif command -v pacman >/dev/null 2>&1 || [[ " $(os_field ID) $(os_field ID_LIKE) " == *" arch "* ]]; then
    echo arch
  else
    echo other
  fi
}

nixos_notice() {
  cat <<EOF
blxshell on NixOS comes from the flake, not this installer:

  # flake.nix
  inputs.blxshell.url = "github:binarylinuxx/dots";

  # configuration.nix
  environment.systemPackages = [ inputs.blxshell.packages.\${pkgs.system}.default ];
  fonts.packages = [ inputs.blxshell.packages.\${pkgs.system}.blxshell-custom-fonts ];

  # optional: the login screen
  imports = [ inputs.blxshell.nixosModules.default ];
  programs.blxshell.greeter.enable = true;

More: $WIKI/getting-started/installation/
EOF
}

case "$(distro)" in
  nixos) [[ $DEMO == 1 ]] || { nixos_notice; exit 0; } ;;
  arch) ;;
  other)
    if [[ $DEMO == 0 ]]; then
      echo "This installer supports Arch and Arch-based distributions."
      echo "Elsewhere, build blxshell from source: $WIKI/getting-started/installation/"
      exit 1
    fi
    ;;
esac

if [[ $DEMO == 0 && $EUID == 0 ]]; then
  echo "Run the installer as your own user; it asks for sudo when it needs it." >&2
  exit 1
fi

if ! command -v gum >/dev/null 2>&1; then
  if [[ $DEMO == 1 ]]; then
    echo "The demo needs gum (try: nix shell nixpkgs#gum)." >&2
    exit 1
  fi
  echo ":: The installer needs gum; installing it with pacman."
  sudo pacman -S --needed --noconfirm gum || { echo "Could not install gum." >&2; exit 1; }
fi

# ── Drawing ──────────────────────────────────────────────────────────────────

# Asks the terminal itself: stdin and stdout are often pipes while drawing.
tty_size() { stty size </dev/tty 2>/dev/null || echo "24 80"; }
cols() { local size; size=$(tty_size); echo "${size#* }"; }
rows() { local size; size=$(tty_size); echo "${size% *}"; }
width() { local c; c=$(cols); (( c - 4 < COLUMN )) && echo $((c - 4)) || echo $COLUMN; }
left() { local c w; c=$(cols); w=$(width); echo $(((c - w) / 2)); }

fg() { printf '\033[%sm' "$(( $1 < 8 ? 30 + $1 : 90 + $1 - 8 ))"; }
readonly BOLD=$'\033[1m' RESET=$'\033[0m' ESC=$'\033'

# repeat TEXT COUNT
repeat() { local i; for ((i = 0; i < $2; i++)); do printf '%s' "$1"; done; }

# Prints a line at the column's left edge.
say() { printf '%*s%s%s\n' "$(left)" '' "$1" "$RESET"; }
muted() { say "$(fg $MUTED)$1"; }

# "blx" in the accent, "shell" in the light colour.
readonly WORDMARK=(
  "████▄  █      █   █  ▄███▄  █   █  █████  █      █    "
  "█   █  █      ▀▄ ▄▀  █      █   █  █      █      █    "
  "████▄  █        █    ▀███▄  █████  ████   █      █    "
  "█   █  █      ▄▀ ▀▄      █  █   █  █      █      █    "
  "████▀  █████  █   █  ▀███▀  █   █  █████  █████  █████"
)
readonly WORDMARK_WIDTH=54 SPLIT=21

header() {
  printf '\033[2J\033[H'
  if [[ $DEMO == 1 ]]; then
    printf '%*s%s\n' "$(( $(cols) - 33 ))" '' "$(fg 0)${ESC}[43m DEMO · nothing will be changed $RESET"
  else
    echo
  fi
  echo
  if (( $(rows) >= 30 && $(width) >= WORDMARK_WIDTH )); then
    local line
    for line in "${WORDMARK[@]}"; do
      say "$BOLD$(fg $ACCENT)${line:0:SPLIT}$(fg $LIGHT)${line:SPLIT}"
    done
  else
    say "$BOLD$(fg $ACCENT)blx$(fg $LIGHT)shell$RESET$(fg $MUTED)  installer"
  fi
  echo
}

# ■ for finished steps, a bold ■ for this one, · for the rest; then n/N.
progress() {
  local current=$1 total=$2 line="" index
  for ((index = 0; index < total; index++)); do
    if ((index < current)); then line+="$(fg $ACCENT)■ "
    elif ((index == current)); then line+="$BOLD$(fg $LIGHT)■$RESET "
    else line+="$(fg $MUTED)· "
    fi
  done
  (( current < total )) && line+="$(fg $MUTED) $((current + 1))/$total"
  say "$line"
}

# The shared frame: header, step dots, title and subtitle.
frame() {
  local title=$1 subtitle=$2 colour=${3:-$LIGHT}
  header
  # Count only the steps that apply, as the dots do.
  local index position=0 total=0
  for index in "${!WIZARD[@]}"; do
    applicable "${WIZARD[index]}" || continue
    ((index < STEP)) && position=$((position + 1))
    total=$((total + 1))
  done
  ((STEP >= ${#WIZARD[@]})) && position=$total
  progress "$position" "$total"
  say "$BOLD$(fg "$colour")$title"
  muted "$subtitle"
  echo
}

# gum, lined up with the column and in the installer's colours.
choose() {
  gum choose --padding "0 $(left)" --header "" --cursor "▌ " \
    --cursor.foreground $ACCENT --selected.foreground $ACCENT --item.foreground $LIGHT \
    --selected-prefix "■ " --unselected-prefix "· " --cursor-prefix "· " \
    --label-delimiter "|" "$@"
}

# ── The wizard ───────────────────────────────────────────────────────────────

WIZARD=(welcome packages dotfiles login theme review)
STEP=0

EXTRAS=(shell audio)
DOTFILES=copy
LOGIN=keep
THEME=sync
CURRENT_DM=""

if [[ -L /etc/systemd/system/display-manager.service ]]; then
  CURRENT_DM=$(basename "$(readlink -f /etc/systemd/system/display-manager.service)" .service)
fi
[[ $DEMO == 1 && -z $CURRENT_DM ]] && CURRENT_DM=sddm

applicable() {
  case "$1" in
    theme) [[ $LOGIN == greeter ]] ;;
    *) true ;;
  esac
}

go() { # go +1 / go -1: the next applicable screen
  local step=$((STEP + $1))
  while ((step >= 0 && step < ${#WIZARD[@]})) && ! applicable "${WIZARD[step]}"; do
    step=$((step + $1))
  done
  ((step < 0)) && step=0
  STEP=$step
}

quit() {
  echo
  if gum confirm --padding "0 $(left)" --prompt.foreground $LIGHT --selected.background $ACCENT \
    "Exit the installer? Nothing has been changed." --affirmative "Exit" --negative "Stay"; then
    clear
    exit 0
  fi
}

screen_welcome() {
  header
  say "$(fg $MUTED)Vellum · a Material You desktop shell, in C++ on vw"
  echo
  say "This installs blxshell, the packages around it and, if you"
  say "want, its login screen. Nothing is changed until you confirm"
  say "on the review screen."
  echo
  local answer
  answer=$(choose "Begin|begin" "Exit|exit") || { quit; return; }
  [[ $answer == begin ]] && go 1 || quit
}

screen_packages() {
  frame "Packages" "blxshell, vw and the fonts always come. Space picks the extras."
  local selected=() picked line
  for line in "${EXTRAS[@]}"; do
    case $line in
      shell) selected+=("Shell & terminal   fish · ghostty · starship · matugen") ;;
      audio) selected+=("Audio              PipeWire · pavucontrol · cava") ;;
      hyprland) selected+=("Hyprland           compositor and portals") ;;
    esac
  done
  local IFS=,
  picked=$(choose --no-limit --selected "${selected[*]}" \
    "Shell & terminal   fish · ghostty · starship · matugen|shell" \
    "Audio              PipeWire · pavucontrol · cava|audio" \
    "Hyprland           compositor and portals|hyprland") || { go -1; return; }
  unset IFS
  mapfile -t EXTRAS <<<"$picked"
  [[ -z ${EXTRAS[0]:-} ]] && EXTRAS=()
  go 1
}

screen_dotfiles() {
  frame "Dotfiles" "Hyprland, zwwm, fish, ghostty, GTK, fastfetch and starship configs."
  local answer
  answer=$(choose \
    "Copy them, keeping a backup   your current ones go to ~/.config-blxshell-backup|copy" \
    "Skip                          keep my configs as they are|skip") || { go -1; return; }
  DOTFILES=$answer
  go 1
}

screen_login() {
  local now=${CURRENT_DM:-no login screen}
  frame "Login screen" "Optional. blxshell-greeter is a greetd login screen in the lock screen's style."
  local answer
  answer=$(choose \
    "Keep $now|keep" \
    "Replace it with blxshell-greeter   greetd and cage; from the next boot|greeter") || { go -1; return; }
  LOGIN=$answer
  go 1
}

screen_theme() {
  frame "Theme sync" "Optional. The login screen can't read your home, so it can wear a copy of your theme."
  local answer
  answer=$(choose \
    "Keep it in sync   copied now and whenever your theme changes|sync" \
    "Copy it once      run blxshell-greeter --export again after changes|once" \
    "Don't copy it     the login screen uses blxshell's colours|none") || { go -1; return; }
  THEME=$answer
  go 1
}

extras_summary() {
  local names=() extra
  for extra in "${EXTRAS[@]}"; do
    case $extra in
      shell) names+=("shell & terminal") ;;
      audio) names+=("audio") ;;
      hyprland) names+=("Hyprland") ;;
    esac
  done
  ((${#names[@]})) && (IFS=,; echo "${names[*]}" | sed 's/,/, /g') || echo "none"
}

row() { say "$(fg $MUTED)  $(printf '%-14s' "$1")$(fg $LIGHT)$2"; }

screen_review() {
  frame "Review" "Last chance to go back. Nothing has changed yet."
  row "Shell" "blxshell-git and vw-git, built here"
  row "Fonts" "blxshell-fonts"
  row "Extras" "$(extras_summary)"
  row "Dotfiles" "$([[ $DOTFILES == copy ]] && echo "copy, with a backup" || echo "keep mine")"
  if [[ $LOGIN == greeter ]]; then
    row "Login screen" "blxshell-greeter${CURRENT_DM:+, replacing $CURRENT_DM}"
    case $THEME in
      sync) row "Theme" "kept in sync" ;;
      once) row "Theme" "copied once" ;;
      none) row "Theme" "blxshell's colours" ;;
    esac
  else
    row "Login screen" "${CURRENT_DM:-none}, unchanged"
  fi
  row "Log" "$LOG"
  echo
  [[ $DEMO == 0 ]] && muted "  sudo asks for your password before the install starts."
  [[ $LOGIN == greeter ]] && say "$(fg $WARN)  The login screen changes at the next boot, not now."
  echo
  local answer
  answer=$(choose "Install|install" "Back|back") || { go -1; return; }
  [[ $answer == install ]] && INSTALL=1 || go -1
}

# ── The install ──────────────────────────────────────────────────────────────

PHASES=()
COMMANDS=()
plan() {
  PHASES=("Prepare" "Fonts" "vw" "blxshell")
  COMMANDS=(phase_prepare phase_fonts phase_vw phase_blxshell)
  if ((${#EXTRAS[@]})); then PHASES+=("Extras"); COMMANDS+=(phase_extras); fi
  if [[ $DOTFILES == copy ]]; then PHASES+=("Dotfiles"); COMMANDS+=(phase_dotfiles); fi
  if [[ $LOGIN == greeter ]]; then PHASES+=("Login screen"); COMMANDS+=(phase_login); fi
  PHASES+=("Finish")
  COMMANDS+=(phase_finish)
}

WORK=""

helper() {
  if command -v yay >/dev/null; then echo yay
  elif command -v paru >/dev/null; then echo paru
  fi
}

aur_install() { # repository or AUR packages, whichever they are
  ((${#@})) || return 0
  case $(helper) in
    yay) yay -S --needed --noconfirm --answerdiff None --answerclean None "$@" ;;
    paru) paru -S --needed --noconfirm --skipreview "$@" ;;
  esac
}

# Builds one of arch-deps/ with makepkg, its dependencies first.
build_local() {
  local name=$1 dependencies missing
  echo "==> $name"
  rm -rf "${WORK:?}/$name"
  cp -r "$DIR/arch-deps/$name" "$WORK/$name"
  dependencies=$(cd "$WORK/$name" && bash -c 'source PKGBUILD; printf "%s\n" "${depends[@]}" "${makedepends[@]}"' |
    sed 's/[<>=].*//' | sort -u)
  missing=$(pacman -T $dependencies 2>/dev/null || true)
  # shellcheck disable=SC2086
  aur_install $missing || return 1
  (cd "$WORK/$name" && makepkg -si --noconfirm --needed) || return 1
}

phase_prepare() {
  sudo pacman -S --needed --noconfirm base-devel git || return 1
  if [[ -z $(helper) ]]; then
    echo "No AUR helper found; building yay-bin."
    rm -rf "$WORK/yay-bin"
    git clone --depth 1 https://aur.archlinux.org/yay-bin.git "$WORK/yay-bin" || return 1
    (cd "$WORK/yay-bin" && makepkg -si --noconfirm) || return 1
  fi
  echo "AUR helper: $(helper)"
}

phase_fonts() {
  build_local blxshell-font-googlesans &&
    build_local blxshell-font-bitcount &&
    build_local blxshell-fonts
}

phase_vw() { build_local vw-git; }
phase_blxshell() { build_local blxshell-git; }

phase_extras() {
  local extra
  for extra in "${EXTRAS[@]}"; do build_local "blxshell-$extra" || return 1; done
}

phase_dotfiles() {
  local backup="$HOME/.config-blxshell-backup-$(date +%Y%m%d-%H%M%S)" source name
  for source in "$DIR"/.config/*; do
    name=$(basename "$source")
    if [[ -e "$HOME/.config/$name" ]]; then
      mkdir -p "$backup"
      echo "backup: ~/.config/$name -> $backup/$name"
      cp -a "$HOME/.config/$name" "$backup/" || return 1
    fi
    echo "copy: $name"
    mkdir -p "$HOME/.config"
    cp -a "$source" "$HOME/.config/" || return 1
  done
}

phase_login() {
  aur_install greetd cage || return 1

  # Starts the greeter in cage, with the cursor the theme export found.
  sudo tee /etc/greetd/blxshell-greeter.sh >/dev/null <<'EOF' || return 1
#!/bin/sh
# Written by the blxshell installer.
if [ -r /var/lib/blxshell-greeter/cursor ]; then
  . /var/lib/blxshell-greeter/cursor
  export XCURSOR_PATH XCURSOR_THEME XCURSOR_SIZE
fi
exec cage -s -- blxshell-greeter
EOF
  sudo chmod 755 /etc/greetd/blxshell-greeter.sh || return 1
  if [[ -f /etc/greetd/config.toml ]]; then
    sudo cp -n /etc/greetd/config.toml /etc/greetd/config.toml.before-blxshell || return 1
  fi
  sudo tee /etc/greetd/config.toml >/dev/null <<'EOF' || return 1
# Written by the blxshell installer; the old one is config.toml.before-blxshell.
[terminal]
vt = 1

[default_session]
command = "/etc/greetd/blxshell-greeter.sh"
user = "greeter"
EOF

  case $THEME in
    once) sudo blxshell-greeter --export "$THEME_DIR" --from "$HOME" --user "$USER" || return 1 ;;
    sync)
      sudo tee /etc/systemd/system/blxshell-greeter-sync.service >/dev/null <<EOF || return 1
[Unit]
Description=Copy $USER's blxshell theme to the login screen

[Service]
Type=oneshot
ExecStart=/usr/bin/blxshell-greeter --export $THEME_DIR --from $HOME --user $USER
EOF
      sudo tee /etc/systemd/system/blxshell-greeter-sync.path >/dev/null <<EOF || return 1
[Unit]
Description=Watch $USER's blxshell theme for the login screen

[Path]
PathChanged=$HOME/.config/blxshell/config.json
PathChanged=$HOME/.local/state/blxshell/matugen-colours.json
PathChanged=$HOME/.local/state/blxshell/wallpaper
PathChanged=$HOME/.face
# A burst of saves (a new wallpaper and its scheme) is one export.
TriggerLimitIntervalSec=10s
TriggerLimitBurst=5

[Install]
WantedBy=multi-user.target
EOF
      sudo systemctl daemon-reload &&
        sudo systemctl start blxshell-greeter-sync.service &&
        sudo systemctl enable --now blxshell-greeter-sync.path || return 1
      ;;
  esac

  # Swap the display manager for the next boot; the running one stays.
  if [[ -n $CURRENT_DM && $CURRENT_DM != greetd ]]; then
    sudo systemctl disable "$CURRENT_DM.service" || return 1
  fi
  sudo systemctl enable greetd.service
}

phase_finish() {
  rm -rf "${WORK:?}"
  echo "Done."
}

demo_phase() {
  local step
  for step in $(seq 1 12); do
    echo "demo: ${PHASES[$1]} step $step of 12"
    sleep 0.12
  done
  [[ $DEMO_FAIL == 1 && $1 == 2 ]] && { echo "demo: simulated failure"; return 1; }
  return 0
}

STARTED=0
FINISHED=0
PHASE=0
FAILED=""

elapsed() {
  local seconds=$(( ${FINISHED:-0} > 0 ? FINISHED - STARTED : $(date +%s) - STARTED ))
  printf '%02d:%02d' $((seconds / 60)) $((seconds % 60))
}

readonly SPINNER='|/-\'
draw_install() {
  local tick=$1 title=$2 subtitle=$3 colour=${4:-$LIGHT} index name
  STEP=${#WIZARD[@]}
  frame "$title" "$subtitle" "$colour"
  for index in "${!PHASES[@]}"; do
    name=${PHASES[index]}
    if ((index < PHASE)); then say "$(fg $ACCENT)■ $name"
    elif ((index == PHASE)) && [[ -n $FAILED ]]; then say "$BOLD$(fg $ERROR)x $name"
    elif ((index == PHASE)); then say "$BOLD$(fg $LIGHT)${SPINNER:tick % 4:1} $name"
    else say "$(fg $MUTED)· $name"
    fi
  done
  echo
  local total=${#PHASES[@]} label cells filled
  label=$(printf ' %3d%%  %s' $((PHASE * 100 / total)) "$(elapsed)")
  cells=$(( $(width) - ${#label} ))
  filled=$((cells * PHASE / total))
  say "$(fg $ACCENT)$(repeat █ "$filled")$(fg $MUTED)$(repeat ░ $((cells - filled)))$(fg $LIGHT)$label"
  echo
  local room=$(( $(rows) - ${#PHASES[@]} - ($(rows) >= 30 ? 18 : 12) ))
  ((room < 1)) && room=1
  local line
  while IFS= read -r line; do
    muted "${line:0:$(width)}"
  done < <(tail -n "$room" "$LOG" 2>/dev/null | tr -d '\r' | sed 's/\x1b\[[0-9;]*[A-Za-z]//g')
}

run_install() {
  plan
  mkdir -p "$(dirname "$LOG")"
  : >"$LOG"
  if [[ $DEMO == 0 ]]; then
    clear
    echo ":: sudo needs your password once for the install."
    sudo -v || return 1
    # Keep sudo's timestamp fresh while packages build.
    (while kill -0 $$ 2>/dev/null; do sudo -n true; sleep 50; done) &
    WORK=$(mktemp -d "${TMPDIR:-/tmp}/blxshell-install.XXXXXX")
  fi
  STARTED=$(date +%s) FINISHED=0 PHASE=0 FAILED=""
  local tick=0 job status
  for PHASE in "${!PHASES[@]}"; do
    echo "==> ${PHASES[PHASE]}" >>"$LOG"
    if [[ $DEMO == 1 ]]; then
      demo_phase "$PHASE" >>"$LOG" 2>&1 &
    else
      "${COMMANDS[PHASE]}" >>"$LOG" 2>&1 </dev/null &
    fi
    job=$!
    while kill -0 "$job" 2>/dev/null; do
      draw_install $((tick++)) "Install" "This takes a while; vw and blxshell build here."
      sleep 0.2
    done
    wait "$job"
    status=$?
    if ((status != 0)); then
      FAILED="${PHASES[PHASE]} failed (exit $status)"
      echo "error: $FAILED" >>"$LOG"
      FINISHED=$(date +%s)
      return 1
    fi
  done
  PHASE=${#PHASES[@]}
  FINISHED=$(date +%s)
}

screen_failed() {
  draw_install 0 "Install failed" "Nothing past the failed step was run." $ERROR
  say "$BOLD$(fg $ERROR)$FAILED"
  muted "Full log: $LOG"
  echo
  local answer
  answer=$(choose "Retry|retry" "Change settings|back" "Exit|exit") || answer="exit"
  case $answer in
    retry) ;;
    back)
      INSTALL=0
      STEP=1
      ;;
    exit) clear; exit 1 ;;
  esac
}

screen_done() {
  STEP=${#WIZARD[@]}
  frame "Done" "Finished in $(elapsed)."
  say "$BOLD$(fg $ACCENT)blxshell is installed."
  echo
  say "Start it with $BOLD$(fg $LIGHT)blxshell$RESET from your compositor's autostart."
  say "$(fg $MUTED)blxshell help lists the commands binds can send it."
  if [[ $LOGIN == greeter ]]; then
    say "$(fg $MUTED)The login screen takes over at the next boot."
  fi
  echo
  local actions=("Exit|exit")
  [[ $LOGIN == greeter ]] && actions=("Reboot now|reboot" "${actions[@]}")
  local answer
  answer=$(choose "${actions[@]}") || answer="exit"
  clear
  if [[ $answer == reboot ]]; then
    [[ $DEMO == 1 ]] && { echo "demo: would reboot"; exit 0; }
    exec systemctl reboot
  fi
  exit 0
}

# ── Main ─────────────────────────────────────────────────────────────────────

trap 'printf "\033[?25h"' EXIT
INSTALL=0
while true; do
  if ((INSTALL)); then
    if run_install; then
      screen_done
    else
      screen_failed
    fi
    continue
  fi
  "screen_${WIZARD[STEP]}"
done
