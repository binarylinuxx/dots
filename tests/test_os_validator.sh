#!/usr/bin/env bash
# ============================================================================
# Test runner for os_release_validator.sh
# Overrides /etc/os-release with fake files from tests/os-release/
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_DIR="$SCRIPT_DIR/tests/os-release"

# Minimal stubs so validator doesn't crash
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'
LOG_FILE="/dev/null"

log()     { :; }
info()    { echo -e "${BLUE}::${NC} $1"; }
success() { echo -e "${GREEN}[+]${NC} $1"; }
warning() { echo -e "${YELLOW}[!]${NC} $1"; }
error()   { echo -e "${RED}[-]${NC} $1"; }
die()     { error "$1"; }
confirm() { return 0; }  # auto-accept everything in tests

# Override validate_os to use fake os-release
run_test() {
    local name="$1"
    local file="$TEST_DIR/$name"

    echo ""
    echo -e "${CYAN}${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}${BOLD}  TEST: $name${NC}"
    echo -e "${CYAN}${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    if [[ ! -f "$file" ]]; then
        error "Test file not found: $file"
        return 1
    fi

    # Reset state
    IS_ARCH_LIKE=false
    SKIP_PACKAGES=false

    # Override: source fake os-release instead of /etc/os-release
    # We redefine validate_os inline to use our fake file
    (
        # Subshell to avoid leaking variables between tests
        source "$SCRIPT_DIR/os_release_validator.sh"

        # Monkey-patch: replace validate_os with version that uses fake file
        validate_os_fake() {
            source "$file"

            local id="${ID:-unknown}"
            local id_like="${ID_LIKE:-}"
            local pretty="${PRETTY_NAME:-$id}"

            info "Detected: ${BOLD}$pretty${NC}"

            if [[ "$id" == "arch" ]]; then
                success "Arch Linux detected"
                IS_ARCH_LIKE=true
                SKIP_PACKAGES=false
                _print_result
                return
            fi

            if [[ "$id_like" == *"arch"* ]]; then
                if [[ "$id" == "manjaro" ]]; then
                    echo ""
                    echo -e "${YELLOW}${BOLD}  ┌─────────────────────────────────────────────────────┐${NC}"
                    echo -e "${YELLOW}${BOLD}  │              Manjaro Detected                        │${NC}"
                    echo -e "${YELLOW}${BOLD}  ├─────────────────────────────────────────────────────┤${NC}"
                    echo -e "${YELLOW}  │  Manjaro uses its own delayed repositories which    │${NC}"
                    echo -e "${YELLOW}  │  may cause version mismatches and broken packages.  │${NC}"
                    echo -e "${YELLOW}  │  AUR packages expect up-to-date Arch repos.         │${NC}"
                    echo -e "${YELLOW}  │                                                     │${NC}"
                    echo -e "${YELLOW}  │  Proceed with caution — things may break.           │${NC}"
                    echo -e "${YELLOW}${BOLD}  └─────────────────────────────────────────────────────┘${NC}"
                    echo ""
                    warning "Manjaro's repositories are known to cause issues with AUR"
                else
                    success "Arch-based distro detected ($pretty)"
                fi
                IS_ARCH_LIKE=true
                SKIP_PACKAGES=false
                _print_result
                return
            fi

            IS_ARCH_LIKE=false
            SKIP_PACKAGES=true
            _show_non_arch_notice
            _print_result
        }

        _print_result() {
            echo ""
            echo -e "  Result: IS_ARCH_LIKE=${BOLD}$IS_ARCH_LIKE${NC}  SKIP_PACKAGES=${BOLD}$SKIP_PACKAGES${NC}"
        }

        validate_os_fake
    )
}

# ============================================================================
# Run all tests
# ============================================================================

echo -e "${BOLD}${CYAN}"
cat << 'EOF'
  ┌─────────────────────────────────────────┐
  │   os_release_validator.sh  Test Suite   │
  └─────────────────────────────────────────┘
EOF
echo -e "${NC}"

for test_file in "$TEST_DIR"/*; do
    run_test "$(basename "$test_file")"
done

echo ""
echo -e "${GREEN}${BOLD}All tests completed.${NC}"
