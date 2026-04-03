#!/usr/bin/env bash
# ============================================================================
# blxshell OBS Publisher
# ============================================================================
# Publishes all opensuse-deps/ spec files to OBS home:binarylinuxx:blxshell
#
# Usage:
#   ./publish-to-obs.sh              # publish all packages
#   ./publish-to-obs.sh blxshell-shell blxshell-audio   # publish specific
# ============================================================================

set -eo pipefail

readonly OBS_PROJECT="home:binarylinuxx:blxshell"
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly WORK_DIR="/tmp/obs_publish_$$"
AUTO_YES=false

# Colors
readonly RED='\033[0;31m'
readonly GREEN='\033[0;32m'
readonly BLUE='\033[0;34m'
readonly YELLOW='\033[0;33m'
readonly CYAN='\033[0;36m'
readonly BOLD='\033[1m'
readonly NC='\033[0m'

info()    { echo -e "${BLUE}::${NC} $1"; }
success() { echo -e "${GREEN}[+]${NC} $1"; }
warning() { echo -e "${YELLOW}[!]${NC} $1"; }
error()   { echo -e "${RED}[-]${NC} $1"; }
die()     { error "$1"; exit 1; }

confirm() {
    local prompt="$1"
    if [[ "$AUTO_YES" == true ]]; then
        echo -e "${YELLOW}${prompt} [y/N]:${NC} y (auto)"
        return 0
    fi
    echo -en "${YELLOW}${prompt} [y/N]: ${NC}"
    read -r response </dev/tty
    [[ "${response}" =~ ^[Yy]$ ]]
}

cleanup() {
    [[ -d "$WORK_DIR" ]] && rm -rf "$WORK_DIR"
}
trap cleanup EXIT

# ============================================================================
# All packages in opensuse-deps/
# ============================================================================

ALL_PACKAGES=(
    blxshell-shell
    blxshell-audio
    blxshell-hyprland
    blxshell-quickshell-git
    blxshell-fonts
    blxshell-font-bitcount
    blxshell-font-googlesans
    blxshell-font-material-symbols
    blxshell-font-readex-pro
    blxshell-font-rubik
    blxshell-font-space-grotesk
)

# ============================================================================

show_banner() {
    echo -e "${CYAN}"
    cat << 'EOF'
   ___  ____  ____    ____        _     _ _     _
  / _ \| __ )/ ___|  |  _ \ _   _| |__ | (_)___| |__   ___ _ __
 | | | |  _ \\___ \  | |_) | | | | '_ \| | / __| '_ \ / _ \ '__|
 | |_| | |_) |___) | |  __/| |_| | |_) | | \__ \ | | |  __/ |
  \___/|____/|____/  |_|    \__,_|_.__/|_|_|___/_| |_|\___|_|

EOF
    echo -e "${NC}${BOLD}Publish to OBS${NC} → ${CYAN}${OBS_PROJECT}${NC}\n"
}

check_osc() {
    command -v osc &>/dev/null || die "osc not found — install it with: doas zypper install osc"

    info "Verifying OBS authentication..."
    osc whois &>/dev/null || die "Not logged in to OBS. Run: osc whois"
    local user
    user=$(osc whois 2>/dev/null | head -1)
    success "Logged in as: $user"
}

publish_package() {
    local pkg_name="$1"
    local pkg_dir="$SCRIPT_DIR/$pkg_name"
    local spec_file="$pkg_dir/$pkg_name.spec"

    echo -e "\n${CYAN}━━━ $pkg_name ━━━${NC}"

    [[ -d "$pkg_dir" ]]   || { error "Directory not found: $pkg_dir"; return 1; }
    [[ -f "$spec_file" ]] || { error "Spec not found: $spec_file"; return 1; }

    local obs_pkg_dir="$WORK_DIR/$OBS_PROJECT/$pkg_name"

    # Check if package already exists on OBS
    if osc ls "$OBS_PROJECT" 2>/dev/null | grep -qx "$pkg_name"; then
        info "Package exists on OBS — checking out..."
    else
        info "Creating new package on OBS..."
        # Create package via API directly (no editor)
        local xml="<package name=\"$pkg_name\" project=\"$OBS_PROJECT\"><title>$pkg_name</title><description>blxshell dependency metapackage for openSUSE Tumbleweed</description></package>"
        osc api -X PUT \
            -d "$xml" \
            "/source/$OBS_PROJECT/$pkg_name/_meta" 2>&1 || {
            error "Failed to create package $pkg_name on OBS"
            return 1
        }
    fi

    # Checkout into WORK_DIR (osc creates $WORK_DIR/$OBS_PROJECT/$pkg_name)
    (cd "$WORK_DIR" && osc checkout "$OBS_PROJECT/$pkg_name") 2>&1 || {
        error "Failed to checkout $pkg_name"
        return 1
    }

    # Copy spec file into the osc working copy
    cp "$spec_file" "$obs_pkg_dir/$pkg_name.spec"

    # Copy any extra files from the package dir (hooks, patches, etc.)
    for f in "$pkg_dir"/*; do
        local fname
        fname="$(basename "$f")"
        [[ "$fname" == "$pkg_name.spec" ]] && continue
        info "Including extra file: $fname"
        cp "$f" "$obs_pkg_dir/$fname"
    done

    confirm "Commit $pkg_name to OBS?" || { warning "Skipping $pkg_name"; return 0; }

    (
        cd "$obs_pkg_dir"
        osc addremove 2>/dev/null || true
        osc commit -m "blxshell 2.5.1: initial openSUSE Tumbleweed package" 2>&1
    ) || {
        error "Failed to commit $pkg_name"
        return 1
    }

    success "$pkg_name published to OBS"
}

# ============================================================================
# Main
# ============================================================================

show_banner

# Parse flags
POSITIONAL=()
for arg in "$@"; do
    case "$arg" in
        -y|--yes) AUTO_YES=true ;;
        *) POSITIONAL+=("$arg") ;;
    esac
done
set -- "${POSITIONAL[@]}"

check_osc

mkdir -p "$WORK_DIR/$OBS_PROJECT"

# Determine which packages to publish
if [[ $# -gt 0 ]]; then
    PACKAGES=("$@")
else
    PACKAGES=("${ALL_PACKAGES[@]}")
fi

info "Packages to publish to ${BOLD}$OBS_PROJECT${NC}:"
for pkg in "${PACKAGES[@]}"; do
    if [[ -f "$SCRIPT_DIR/$pkg/$pkg.spec" ]]; then
        echo -e "  ${GREEN}✓${NC} $pkg"
    else
        echo -e "  ${RED}✗${NC} $pkg ${YELLOW}(spec missing)${NC}"
    fi
done

echo ""
confirm "Proceed with publishing?" || exit 0

failed=()
for pkg in "${PACKAGES[@]}"; do
    publish_package "$pkg" || failed+=("$pkg")
done

echo ""
if [[ ${#failed[@]} -gt 0 ]]; then
    error "Failed packages: ${failed[*]}"
    exit 1
fi

echo -e "${BOLD}${GREEN}══════ All packages published! ══════${NC}"
info "View at: https://build.opensuse.org/project/show/$OBS_PROJECT"
