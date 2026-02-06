#!/usr/bin/env bash
#
# flash.sh - Build and flash firmware to ESP32-S3 targets
#
# Builds firmware for BASE and/or REMOTE targets and flashes them
# to their configured serial ports.
#
# Usage:
#   ./scripts/flash.sh              # Build and flash all targets
#   ./scripts/flash.sh BASE         # Build and flash BASE only
#   ./scripts/flash.sh REMOTE       # Build and flash REMOTE only
#   ./scripts/flash.sh --build-only # Build without flashing
#
# Reads serial port mapping from hardware_test_config.json
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CONFIG_FILE="$PROJECT_DIR/hardware_test_config.json"
BUILD_DIR="$PROJECT_DIR/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info()  { echo -e "${BLUE}[INFO]${NC}  $*"; }
log_ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }

# Parse config file for serial port mapping
get_port() {
    local target="$1"
    if command -v python3 &>/dev/null; then
        python3 -c "
import json, sys
with open('$CONFIG_FILE') as f:
    cfg = json.load(f)
print(cfg['targets']['$target']['serial_port'])
"
    elif command -v jq &>/dev/null; then
        jq -r ".targets.${target}.serial_port" "$CONFIG_FILE"
    else
        log_error "Requires python3 or jq to parse config"
        exit 1
    fi
}

get_flash_baud() {
    if command -v python3 &>/dev/null; then
        python3 -c "
import json
with open('$CONFIG_FILE') as f:
    cfg = json.load(f)
print(cfg['flash']['baud_rate'])
"
    elif command -v jq &>/dev/null; then
        jq -r '.flash.baud_rate' "$CONFIG_FILE"
    else
        echo "921600"
    fi
}

# Check prerequisites
check_prerequisites() {
    if ! command -v idf.py &>/dev/null; then
        log_error "idf.py not found. Please source ESP-IDF export.sh first:"
        log_error "  . \$IDF_PATH/export.sh"
        exit 1
    fi

    if [ ! -f "$CONFIG_FILE" ]; then
        log_error "Config file not found: $CONFIG_FILE"
        exit 1
    fi
}

# Build firmware for a target
build_target() {
    local target="$1"
    local target_build_dir="$BUILD_DIR/$target"

    log_info "Building firmware for $target..."

    cd "$PROJECT_DIR"

    # Use separate build directories per target
    idf.py -B "$target_build_dir" -D BUILD_TARGET="$target" build

    if [ $? -eq 0 ]; then
        log_ok "Build successful for $target"
        return 0
    else
        log_error "Build failed for $target"
        return 1
    fi
}

# Flash firmware to a target
flash_target() {
    local target="$1"
    local port
    local baud
    local target_build_dir="$BUILD_DIR/$target"

    port=$(get_port "$target")
    baud=$(get_flash_baud)

    if [ ! -e "$port" ]; then
        log_error "Serial port $port not found for $target"
        return 1
    fi

    log_info "Flashing $target to $port at ${baud} baud..."

    cd "$PROJECT_DIR"
    idf.py -B "$target_build_dir" -p "$port" -b "$baud" flash

    if [ $? -eq 0 ]; then
        log_ok "Flash successful for $target on $port"
        return 0
    else
        log_error "Flash failed for $target on $port"
        return 1
    fi
}

# Main
main() {
    local targets=()
    local build_only=false

    # Parse arguments
    while [ $# -gt 0 ]; do
        case "$1" in
            BASE|REMOTE)
                targets+=("$1")
                ;;
            --build-only)
                build_only=true
                ;;
            -h|--help)
                echo "Usage: $0 [BASE|REMOTE] [--build-only]"
                echo ""
                echo "Build and flash firmware to ESP32-S3 targets."
                echo ""
                echo "Arguments:"
                echo "  BASE         Build/flash BASE unit only"
                echo "  REMOTE       Build/flash REMOTE unit only"
                echo "  --build-only Build without flashing"
                echo ""
                echo "With no target arguments, builds and flashes both units."
                exit 0
                ;;
            *)
                log_error "Unknown argument: $1"
                exit 1
                ;;
        esac
        shift
    done

    # Default: both targets
    if [ ${#targets[@]} -eq 0 ]; then
        targets=(BASE REMOTE)
    fi

    check_prerequisites

    log_info "=== Firmware Build & Flash ==="
    log_info "Targets: ${targets[*]}"
    log_info "Project: $PROJECT_DIR"
    echo ""

    local build_failed=0
    local flash_failed=0

    # Build phase
    for target in "${targets[@]}"; do
        if ! build_target "$target"; then
            build_failed=$((build_failed + 1))
        fi
        echo ""
    done

    if [ $build_failed -gt 0 ]; then
        log_error "$build_failed target(s) failed to build"
        exit 1
    fi

    if [ "$build_only" = true ]; then
        log_ok "All builds completed (flash skipped)"
        exit 0
    fi

    # Flash phase
    for target in "${targets[@]}"; do
        if ! flash_target "$target"; then
            flash_failed=$((flash_failed + 1))
        fi
        echo ""
    done

    if [ $flash_failed -gt 0 ]; then
        log_error "$flash_failed target(s) failed to flash"
        exit 1
    fi

    log_ok "=== All targets built and flashed successfully ==="
}

main "$@"
