#!/usr/bin/env bash

# ============================================================================
# blxshell Installation Script
# ============================================================================
# Usage:
#   Local:  ./install.sh
#   Remote: curl -fsSL https://raw.githubusercontent.com/USER/REPO/main/install.sh | bash
#           bash <(curl -fsSL https://raw.githubusercontent.com/USER/REPO/main/install.sh)
# ============================================================================

set -eo pipefail

# Colors
readonly RED='\033[0;31m'
readonly GREEN='\033[0;32m'
readonly BLUE='\033[0;34m'
readonly YELLOW='\033[0;33m'
readonly CYAN='\033[0;36m'
readonly BOLD='\033[1m'
readonly NC='\033[0m'

# Configuration
readonly REPO_URL="https://github.com/binarylinuxx/dots.git"  
readonly REPO_BRANCH="main"
readonly BACKUP_DIR="$HOME/.cache/.config_backup"
readonly LOG_FILE="/tmp/blxshell_install_$(date +%Y%m%d_%H%M%S).log"
readonly PACKAGES=("blxshell-shell" "blxshell-audio" "blxshell-hyprland" "blxshell-font-googlesans" "blxshell-font-bitcount")

# State (will be set by detect_install_mode)
SCRIPT_DIR=""
REMOTE_INSTALL=false
INSTALL_DOTFILES=false
BACKUP_DONE=false
CLEANUP_DIR=""

# ============================================================================
# Utility Functions
# ============================================================================

log() { echo "[$(date '+%H:%M:%S')] $*" >> "$LOG_FILE"; }

info()    { echo -e "${BLUE}::${NC} $1"; log "INFO: $1"; }
success() { echo -e "${GREEN}[+]${NC} $1"; log "SUCCESS: $1"; }
warning() { echo -e "${YELLOW}[!]${NC} $1"; log "WARNING: $1"; }
error()   { echo -e "${RED}[-]${NC} $1"; log "ERROR: $1"; }

die() { error "$1"; exit 1; }

confirm() {
    local prompt="$1" default="${2:-n}" response

    # If not interactive (piped), use default
    if [[ ! -t 0 ]]; then
        [[ "$default" =~ ^[Yy]$ ]]
        return $?
    fi

    [[ "$default" =~ ^[Yy] ]] && prompt="$prompt [Y/n]: " || prompt="$prompt [y/N]: "
    echo -en "${YELLOW}${prompt}${NC}"
    read -r response </dev/tty
    [[ "${response:-$default}" =~ ^[Yy]$ ]]
}

command_exists() { command -v "$1" &>/dev/null; }

# ============================================================================
# Installation Mode Detection
# ============================================================================

detect_install_mode() {
    # Method 1: Check if BASH_SOURCE points to a real file in a valid repo
    if [[ -n "${BASH_SOURCE[0]}" ]] && [[ -f "${BASH_SOURCE[0]}" ]]; then
        local script_dir
        script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd 2>/dev/null)"

        if [[ -n "$script_dir" ]] && [[ -d "$script_dir/arch-deps" ]]; then
            SCRIPT_DIR="$script_dir"
            REMOTE_INSTALL=false
            info "Detected: Running from local repository"
            return 0
        fi
    fi

    # Method 2: Check if we're in a git repo with the expected structure
    if git rev-parse --git-dir &>/dev/null; then
        local git_root
        git_root="$(git rev-parse --show-toplevel 2>/dev/null)"
        if [[ -n "$git_root" ]] && [[ -d "$git_root/arch-deps" ]]; then
            SCRIPT_DIR="$git_root"
            REMOTE_INSTALL=false
            info "Detected: Running from git repository"
            return 0
        fi
    fi

    # Method 3: Running from curl/pipe - need to clone
    REMOTE_INSTALL=true
    SCRIPT_DIR="/tmp/blxshell_install_$$"
    CLEANUP_DIR="$SCRIPT_DIR"
    info "Detected: Remote installation (will clone repository)"
}

clone_repository() {
    info "Cloning repository to $SCRIPT_DIR..."

    if ! command_exists git; then
        info "Installing git..."
        sudo pacman -S --needed --noconfirm git || die "Failed to install git"
    fi

    git clone --depth 1 --branch "$REPO_BRANCH" "$REPO_URL" "$SCRIPT_DIR" 2>&1 | tee -a "$LOG_FILE" || {
        die "Failed to clone repository from $REPO_URL"
    }

    success "Repository cloned successfully"
}

# ============================================================================
# Pre-flight Checks
# ============================================================================

preflight_checks() {
    info "Running pre-flight checks..."

    # Don't run as root
    [[ $EUID -eq 0 ]] && die "Do not run this script as root!"

    # Check if Arch-based
    if [[ ! -f /etc/arch-release ]] && ! command_exists pacman; then
        die "This script is designed for Arch Linux!"
    fi

    # Check internet
    if ! ping -c 1 -W 3 archlinux.org &>/dev/null; then
        warning "No internet connection detected"
        confirm "Continue anyway?" || exit 1
    fi

    # For remote install, we need internet
    if [[ "$REMOTE_INSTALL" == true ]]; then
        ping -c 1 -W 3 github.com &>/dev/null || die "Internet required for remote installation!"
    fi

    # Check disk space (minimum 1GB)
    local space
    space=$(df -BG "$HOME" | awk 'NR==2 {gsub(/G/,""); print $4}')
    if [[ "$space" -lt 1 ]]; then
        warning "Low disk space: ${space}GB available"
        confirm "Continue anyway?" || exit 1
    fi

    success "Pre-flight checks passed"
}

# ============================================================================
# Yay Installation
# ============================================================================

install_yay() {
    if command_exists yay; then
        success "yay already installed"
        return 0
    fi

    info "Installing yay..."
    sudo pacman -S --needed --noconfirm git base-devel || die "Failed to install base dependencies"

    local tmp_dir="/tmp/yay_install_$$"
    trap "rm -rf '$tmp_dir'" RETURN

    git clone https://aur.archlinux.org/yay-bin.git "$tmp_dir" || die "Failed to clone yay"
    (cd "$tmp_dir" && makepkg -si --noconfirm) || die "Failed to install yay"

    command_exists yay && success "yay installed" || die "yay installation failed"
}

# ============================================================================
# Package Installation
# ============================================================================

get_depends() {
    local pkgbuild="$1"
    bash -c "source '$pkgbuild' 2>/dev/null && echo \"\${depends[*]}\""
}

install_package() {
    local pkg_name="$1"
    local pkg_dir="$SCRIPT_DIR/arch-deps/$pkg_name"

    echo -e "\n${CYAN}━━━ $pkg_name ━━━${NC}"

    [[ -f "$pkg_dir/PKGBUILD" ]] || { error "PKGBUILD not found for $pkg_name"; return 1; }

    local depends
    depends=$(get_depends "$pkg_dir/PKGBUILD")

    if [[ -n "$depends" ]]; then
        info "Installing dependencies: $depends"
        # shellcheck disable=SC2086
        if ! yay -S --sudoloop --needed --noconfirm --asdeps --overwrite '*' $depends 2>&1 | tee -a "$LOG_FILE"; then
            error "Failed to install dependencies"
            return 1
        fi
    fi

    info "Building $pkg_name..."
    (cd "$pkg_dir" && makepkg -Afc --noconfirm) 2>&1 | tee -a "$LOG_FILE" || {
        error "Failed to build $pkg_name"
        return 1
    }

    local pkg_file
    pkg_file=$(find "$pkg_dir" -maxdepth 1 -name "*.pkg.tar.zst" -newer "$pkg_dir/PKGBUILD" | head -1)

    [[ -n "$pkg_file" ]] || { error "No package file found"; return 1; }

    info "Installing $pkg_name..."
    sudo pacman -U --noconfirm --overwrite '*' "$pkg_file" 2>&1 | tee -a "$LOG_FILE" || {
        error "Failed to install $pkg_name"
        return 1
    }

    success "$pkg_name installed"
}

install_metapackages() {
    echo -e "\n${BOLD}${BLUE}══════ Installing Metapackages ══════${NC}\n"

    install_yay
    sudo pacman -S --needed --noconfirm base-devel &>/dev/null

    info "Packages to install:"
    for pkg in "${PACKAGES[@]}"; do
        if [[ -d "$SCRIPT_DIR/arch-deps/$pkg" ]]; then
            echo -e "  ${GREEN}✓${NC} $pkg"
        else
            echo -e "  ${RED}✗${NC} $pkg ${YELLOW}(missing)${NC}"
        fi
    done

    local failed=()
    for pkg in "${PACKAGES[@]}"; do
        [[ -d "$SCRIPT_DIR/arch-deps/$pkg" ]] || { warning "Skipping $pkg"; continue; }
        install_package "$pkg" || failed+=("$pkg")
    done

    if [[ ${#failed[@]} -gt 0 ]]; then
        error "Failed packages: ${failed[*]}"
        return 1
    fi

    success "All metapackages installed!"
}

# ============================================================================
# Backup & Dotfiles
# ============================================================================

backup_config() {
    local timestamp backup_path
    timestamp=$(date +%Y%m%d_%H%M%S)
    backup_path="$BACKUP_DIR/config_$timestamp"

    info "Creating backup at $backup_path..."
    mkdir -p "$BACKUP_DIR"

    if cp -r "$HOME/.config" "$backup_path"; then
        ln -sfn "$backup_path" "$BACKUP_DIR/latest"
        success "Backup completed"
        BACKUP_DONE=true
    else
        error "Backup failed!"
        return 1
    fi
}

install_dotfiles() {
    [[ -d "$SCRIPT_DIR/.config" ]] || { error "No .config in $SCRIPT_DIR"; return 1; }

    info "Installing dotfiles..."

    if [[ "$BACKUP_DONE" == false && -d "$HOME/.config" ]]; then
        warning "No backup exists - creating one now..."
        backup_config || return 1
    fi

    rm -rf "$HOME/.config"
    cp -r "$SCRIPT_DIR/.config" "$HOME/"    
    
    success "Dotfiles installed to $HOME/.config"
}

# ============================================================================
# Main
# ============================================================================

show_banner() {
    echo -e "${CYAN}"
    cat << 'EOF'
  _     _          _          _ _
 | |__ | |_  _____| |__   ___| | |
 | '_ \| \ \/ / __| '_ \ / _ \ | |
 | |_) | |>  <\__ \ | | |  __/ | |
 |_.__/|_/_/\_\___/_| |_|\___|_|_|

EOF
    echo -e "${NC}${BOLD}Installation Script${NC} | Log: ${BLUE}$LOG_FILE${NC}\n"
}

greet_user() {
    echo -e "Hello ${BOLD}$USER${NC}!\n"

    if [[ "$REMOTE_INSTALL" == true ]]; then
        info "Remote install mode - dotfiles will be installed by default"
        INSTALL_DOTFILES=true
        if [[ -d "$HOME/.config" ]]; then
            backup_config || { confirm "Continue without backup?" || exit 1; }
        fi
        return
    fi

    if confirm "Install dotfiles (will replace ~/.config)?" "n"; then
        INSTALL_DOTFILES=true
        if [[ -d "$HOME/.config" ]]; then
            if confirm "Backup existing ~/.config first?" "y"; then
                backup_config || { confirm "Continue without backup?" || exit 1; }
            else
                warning "Config will be overwritten without backup!"
                confirm "Are you absolutely sure?" || exit 1
            fi
        fi
    fi
}

cleanup() {
    local exit_code=$?

    # Clean up cloned repo if remote install
    if [[ -n "$CLEANUP_DIR" ]] && [[ -d "$CLEANUP_DIR" ]]; then
        info "Cleaning up temporary files..."
        rm -rf "$CLEANUP_DIR"
    fi

    if [[ $exit_code -ne 0 ]]; then
        echo -e "\n${RED}━━━ Installation Failed ━━━${NC}"
        error "Check log: $LOG_FILE"
        [[ "$BACKUP_DONE" == true ]] && info "Backup at: $BACKUP_DIR/latest"
    fi
}

trap cleanup EXIT

main() {
    show_banner

    # Detect if running locally or from curl
    detect_install_mode

    # Clone if remote install
    if [[ "$REMOTE_INSTALL" == true ]]; then
        clone_repository
    fi

    cd "$SCRIPT_DIR"

    # Verify directory structure
    [[ -d "$SCRIPT_DIR/arch-deps" ]] || die "'arch-deps' directory not found!"

    preflight_checks
    greet_user

    echo -e "\n${BOLD}${YELLOW}══════ Starting Installation ══════${NC}"

    install_metapackages || exit 1

    if [[ "$INSTALL_DOTFILES" == true ]]; then
        echo -e "\n${BOLD}${BLUE}══════ Installing Dotfiles ══════${NC}\n"
        install_dotfiles || exit 1
    fi

    echo -e "\n${BOLD}${GREEN}══════ Installation Complete! ══════${NC}\n"

    info "Restart your session or run:"
    echo -e "  ${CYAN}source ~/.bashrc${NC}  or  ${CYAN}source ~/.zshrc${NC}"
    command_exists fish && echo -e "  ${CYAN}exec fish${NC}  (switch to fish shell)"

    echo -e "\n${GREEN}Please reboot and login into Hyprland (NOT UWSM)${NC}\n"
}

main "$@"
