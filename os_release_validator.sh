#!/usr/bin/env bash
# ============================================================================
# OS Release Validator for blxshell
# ============================================================================
# Sources /etc/os-release and determines distro compatibility.
# Sets IS_ARCH_LIKE=true/false and SKIP_PACKAGES=true/false
# for install.sh to use.
#
# Usage: source os_release_validator.sh
# ============================================================================

IS_ARCH_LIKE=false
SKIP_PACKAGES=false

validate_os() {
    if [[ ! -f /etc/os-release ]]; then
        warning "/etc/os-release not found"
        warning "Cannot determine your distribution"
        IS_ARCH_LIKE=false
        SKIP_PACKAGES=true
        _show_non_arch_notice
        return
    fi

    # shellcheck source=/dev/null
    source /etc/os-release

    local id="${ID:-unknown}"
    local id_like="${ID_LIKE:-}"
    local pretty="${PRETTY_NAME:-$id}"

    info "Detected: ${BOLD}$pretty${NC}"
    log "OS: ID=$id ID_LIKE=$id_like PRETTY_NAME=$pretty"

    # Pure Arch
    if [[ "$id" == "arch" ]]; then
        success "Arch Linux detected"
        IS_ARCH_LIKE=true
        SKIP_PACKAGES=false
        return
    fi

    # Arch-based distros (ID_LIKE contains "arch")
    if [[ "$id_like" == *"arch"* ]]; then

        # Manjaro warning
        if [[ "$id" == "manjaro" ]]; then
            echo ""
            echo -e "${YELLOW}${BOLD}  ┌─────────────────────────────────────────────────────┐${NC}"
            echo -e "${YELLOW}${BOLD}  │              Manjaro Detected                       │${NC}"
            echo -e "${YELLOW}${BOLD}  ├─────────────────────────────────────────────────────┤${NC}"
            echo -e "${YELLOW}         │  Manjaro uses its own delayed repositories which    │${NC}"
            echo -e "${YELLOW}  	   │  may cause version mismatches and broken packages.  │${NC}"
            echo -e "${YELLOW}         │  AUR packages expect up-to-date Arch repos.         │${NC}"
            echo -e "${YELLOW}         │                                                     │${NC}"
            echo -e "${YELLOW}         │  Proceed with caution — things may break.           │${NC}"
            echo -e "${YELLOW}${BOLD}  └─────────────────────────────────────────────────────┘${NC}"
            echo ""
            warning "Manjaro's repositories are known to cause issues with AUR"
            confirm "Continue anyway?" || exit 1
        else
            success "Arch-based distro detected ($pretty)"
        fi

        IS_ARCH_LIKE=true
        SKIP_PACKAGES=false
        return
    fi

    # Not Arch-based at all
    IS_ARCH_LIKE=false
    SKIP_PACKAGES=true
    _show_non_arch_notice
}

_show_non_arch_notice() {
    echo ""
    echo -e "${YELLOW}${BOLD}  ┌─────────────────────────────────────────────────────┐${NC}"
    echo -e "${YELLOW}${BOLD}  │              Non-Arch System Detected               │${NC}"
    echo -e "${YELLOW}${BOLD}  ├─────────────────────────────────────────────────────┤${NC}"
    echo -e "${YELLOW}         │  blxshell currently supports Arch-based distros     │${NC}"
    echo -e "${YELLOW}         │  only for automatic package installation.           │${NC}"
    echo -e "${YELLOW}         │                                                     │${NC}"
    echo -e "${YELLOW}         │  Only .config dotfiles will be installed.           │${NC}"
    echo -e "${YELLOW}         │  You will need to manually install dependencies     │${NC}"
    echo -e "${YELLOW}         │  listed in: arch-deps/blxshell-*/PKGBUILD           │${NC}"
    echo -e "${YELLOW}${BOLD}  └─────────────────────────────────────────────────────┘${NC}"
    echo ""
    warning "Package installation will be skipped"
    confirm "Continue with dotfiles only?" || exit 1
}
