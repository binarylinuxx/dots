#!/usr/bin/env bash

# ============================================================================
# AUR Publishing Script for blxshell Metapackages
# ============================================================================
# This script automates publishing/updating all blxshell packages to the AUR
# ============================================================================

set -eo pipefail

# Colors
readonly RED='\033[0;31m'
readonly GREEN='\033[0;32m'
readonly BLUE='\033[0;34m'
readonly YELLOW='\033[0;33m'
readonly NC='\033[0m'

# Configuration
readonly PACKAGES=(
    "blxshell-shell"
    "blxshell-hyprland"
    "blxshell-audio"
    "blxshell-font-bitcount"
    "blxshell-font-googlesans"
    "blxshell-fonts"
    "blxshell-complete"
)

readonly AUR_DIR="/tmp/aur_publish_$$"
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ============================================================================
# Utility Functions
# ============================================================================

info()    { echo -e "${BLUE}::${NC} $1"; }
success() { echo -e "${GREEN}[+]${NC} $1"; }
warning() { echo -e "${YELLOW}[!]${NC} $1"; }
error()   { echo -e "${RED}[-]${NC} $1"; }
die()     { error "$1"; exit 1; }

confirm() {
    local prompt="$1" default="${2:-n}" response
    [[ "$default" =~ ^[Yy] ]] && prompt="$prompt [Y/n]: " || prompt="$prompt [y/N]: "
    echo -en "${YELLOW}${prompt}${NC}"
    read -r response
    [[ "${response:-$default}" =~ ^[Yy]$ ]]
}

command_exists() { command -v "$1" &>/dev/null; }

# ============================================================================
# Pre-flight Checks
# ============================================================================

preflight_checks() {
    info "Running pre-flight checks..."
    
    # Check if we have git
    command_exists git || die "git is required"
    
    # Check if we have makepkg
    command_exists makepkg || die "makepkg is required"
    
    # Check if SSH key is set up for AUR
    if ! ssh -T aur@aur.archlinux.org 2>&1 | grep -q "successfully authenticated"; then
        warning "AUR SSH authentication not configured"
        echo "Set up your SSH key: https://wiki.archlinux.org/title/AUR_submission_guidelines#Authentication"
        confirm "Continue anyway?" || exit 1
    fi
    
    # Check if in correct directory
    [[ -d "$SCRIPT_DIR/blxshell-shell" ]] || die "Run this script from arch-deps/ directory"
    
    success "Pre-flight checks passed"
}

# ============================================================================
# Package Publishing
# ============================================================================

publish_package() {
    local pkg_name="$1"
    local pkg_dir="$SCRIPT_DIR/$pkg_name"
    local aur_repo="$AUR_DIR/$pkg_name"
    
    echo -e "\n${BLUE}━━━ Publishing $pkg_name ━━━${NC}"
    
    # Verify PKGBUILD exists
    [[ -f "$pkg_dir/PKGBUILD" ]] || { error "No PKGBUILD found for $pkg_name"; return 1; }
    
    # Clone or update AUR repo
    if [[ -d "$aur_repo" ]]; then
        info "Updating existing AUR clone..."
        (cd "$aur_repo" && git pull) || return 1
    else
        info "Cloning AUR repository..."
        git clone "ssh://aur@aur.archlinux.org/$pkg_name.git" "$aur_repo" 2>/dev/null || {
            warning "Package doesn't exist on AUR yet (first upload)"
            mkdir -p "$aur_repo"
            (cd "$aur_repo" && git init && git remote add origin "ssh://aur@aur.archlinux.org/$pkg_name.git")
        }
    fi
    
    # Copy PKGBUILD
    info "Copying PKGBUILD..."
    cp "$pkg_dir/PKGBUILD" "$aur_repo/"
    
    # Generate .SRCINFO
    info "Generating .SRCINFO..."
    (cd "$aur_repo" && makepkg --printsrcinfo > .SRCINFO) || {
        error "Failed to generate .SRCINFO"
        return 1
    }
    
    # Show diff
    info "Changes to be committed:"
    (cd "$aur_repo" && git diff PKGBUILD .SRCINFO 2>/dev/null || echo "New package")
    
    # Commit
    if confirm "Commit and push $pkg_name?" "y"; then
        local commit_msg
        if [[ -f "$aur_repo/.git/HEAD" ]]; then
            # Existing package - increment version
            commit_msg="Update to version $(grep -Po '(?<=^pkgver=).*' "$pkg_dir/PKGBUILD")-$(grep -Po '(?<=^pkgrel=).*' "$pkg_dir/PKGBUILD")"
        else
            # New package
            commit_msg="Initial commit"
        fi
        
        (cd "$aur_repo" && \
            git add PKGBUILD .SRCINFO && \
            git commit -m "$commit_msg" && \
            git push -u origin master) || {
            error "Failed to push $pkg_name"
            return 1
        }
        
        success "$pkg_name published successfully!"
    else
        warning "Skipped $pkg_name"
    fi
}

# ============================================================================
# Main
# ============================================================================

show_banner() {
    echo -e "${BLUE}"
    cat << 'EOF'
   ___  __  ______     ___       __    ___     __        
  / _ |/ / / / __ \   / _ \__ __/ /   / (_)__ / /  ___   
 / __ / /_/ / /_/ /  / ___/ // / _ \ / / (_-</ _ \/ -_)  
/_/ |_\____/\____/  /_/   \_,_/_.__//_/_/___/_//_/\__/   

EOF
    echo -e "${NC}${BLUE}Automated AUR Publishing Script${NC}\n"
}

main() {
    show_banner
    preflight_checks
    
    mkdir -p "$AUR_DIR"
    trap "rm -rf '$AUR_DIR'" EXIT
    
    echo -e "\n${YELLOW}Packages to publish:${NC}"
    for pkg in "${PACKAGES[@]}"; do
        if [[ -d "$SCRIPT_DIR/$pkg" ]]; then
            echo -e "  ${GREEN}✓${NC} $pkg"
        else
            echo -e "  ${RED}✗${NC} $pkg ${YELLOW}(missing)${NC}"
        fi
    done
    echo
    
    confirm "Proceed with AUR publishing?" "y" || exit 0
    
    local failed=()
    for pkg in "${PACKAGES[@]}"; do
        [[ -d "$SCRIPT_DIR/$pkg" ]] || { warning "Skipping $pkg (not found)"; continue; }
        publish_package "$pkg" || failed+=("$pkg")
    done
    
    echo -e "\n${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    if [[ ${#failed[@]} -eq 0 ]]; then
        echo -e "${GREEN}✓ All packages published successfully!${NC}\n"
        echo "View your packages at:"
        for pkg in "${PACKAGES[@]}"; do
            echo "  https://aur.archlinux.org/packages/$pkg"
        done
    else
        echo -e "${RED}✗ Failed packages: ${failed[*]}${NC}"
        exit 1
    fi
}

# Show help
if [[ "${1:-}" == "-h" ]] || [[ "${1:-}" == "--help" ]]; then
    cat << EOF
Usage: $0

Publishes all blxshell metapackages to the AUR.

Prerequisites:
  - SSH key configured for AUR (https://wiki.archlinux.org/title/AUR_submission_guidelines)
  - git and makepkg installed
  - Run from arch-deps/ directory

What it does:
  1. Clones/updates AUR repositories for each package
  2. Copies PKGBUILD from local to AUR repo
  3. Generates .SRCINFO
  4. Shows diff and asks for confirmation
  5. Commits and pushes to AUR

Packages published:
$(printf "  - %s\n" "${PACKAGES[@]}")

Options:
  -h, --help    Show this help message

Examples:
  cd arch-deps/
  ./publish-to-aur.sh

Notes:
  - You'll be prompted before each package is pushed
  - Failed packages will be reported at the end
  - Temporary files are auto-cleaned
EOF
    exit 0
fi

main "$@"
