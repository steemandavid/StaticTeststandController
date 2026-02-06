#!/usr/bin/env bash
#
# test_runner.sh - Automated hardware test runner for ESP32-S3 targets
#
# Sends TEST commands over serial and parses JSON responses.
# Produces structured pass/fail reports.
#
# Usage:
#   ./scripts/test_runner.sh              # Run all tests on all targets
#   ./scripts/test_runner.sh BASE         # Test BASE only
#   ./scripts/test_runner.sh REMOTE       # Test REMOTE only
#   ./scripts/test_runner.sh BASE PING    # Run single test on BASE
#
# Reads serial port mapping from hardware_test_config.json
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CONFIG_FILE="$PROJECT_DIR/hardware_test_config.json"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

log_info()  { echo -e "${BLUE}[INFO]${NC}  $*"; }
log_ok()    { echo -e "${GREEN}[PASS]${NC}  $*"; }
log_fail()  { echo -e "${RED}[FAIL]${NC}  $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_test()  { echo -e "${CYAN}[TEST]${NC}  $*"; }

# Counters
TOTAL=0
PASSED=0
FAILED=0
SKIPPED=0

###############################################################################
# Config parsing
###############################################################################

get_config_value() {
    local jq_path="$1"
    if command -v python3 &>/dev/null; then
        python3 -c "
import json
with open('$CONFIG_FILE') as f:
    cfg = json.load(f)
keys = '''$jq_path'''.strip('.').split('.')
val = cfg
for k in keys:
    val = val[k]
print(val)
"
    elif command -v jq &>/dev/null; then
        jq -r "$jq_path" "$CONFIG_FILE"
    else
        echo ""
    fi
}

get_port() {
    get_config_value ".targets.${1}.serial_port"
}

get_baud() {
    get_config_value ".targets.${1}.baud_rate"
}

get_timeout() {
    get_config_value ".test_runner.timeout_s"
}

get_boot_wait() {
    get_config_value ".test_runner.boot_wait_s"
}

get_retry_count() {
    get_config_value ".test_runner.retry_count"
}

get_retry_delay() {
    get_config_value ".test_runner.retry_delay_s"
}

###############################################################################
# Serial communication
###############################################################################

# Send a TEST command and capture the JSON response
# Args: $1=port $2=baud $3=command $4=timeout_s
send_test_command() {
    local port="$1"
    local baud="$2"
    local command="$3"
    local timeout="${4:-$(get_timeout)}"

    # Configure serial port
    stty -F "$port" "$baud" raw -echo -echoe -echok -echoctl -echoke cs8 \
        -parenb -cstopb -crtscts 2>/dev/null || true

    # Flush any pending data
    timeout 0.5 cat "$port" >/dev/null 2>&1 || true

    # Send command
    echo "TEST $command" > "$port"

    # Read response with timeout - look for JSON line
    local response=""
    local deadline=$((SECONDS + timeout))

    while [ $SECONDS -lt $deadline ]; do
        local line
        if line=$(timeout 1 head -n 1 "$port" 2>/dev/null); then
            # Check if line looks like our JSON response
            if echo "$line" | grep -q '"status"'; then
                response="$line"
                break
            fi
        fi
    done

    echo "$response"
}

# Parse JSON response status field
# Returns 0 if status is "ok", 1 otherwise
check_response_status() {
    local response="$1"
    if [ -z "$response" ]; then
        return 1
    fi

    if command -v python3 &>/dev/null; then
        python3 -c "
import json, sys
try:
    r = json.loads('''$response''')
    sys.exit(0 if r.get('status') == 'ok' else 1)
except:
    sys.exit(1)
"
    else
        echo "$response" | grep -q '"status":"ok"'
    fi
}

# Extract a field from JSON response
get_response_field() {
    local response="$1"
    local field="$2"

    if command -v python3 &>/dev/null; then
        python3 -c "
import json
try:
    r = json.loads('''$response''')
    data = r.get('data', {})
    val = data.get('$field', r.get('$field', 'N/A'))
    print(val)
except:
    print('N/A')
"
    else
        echo "N/A"
    fi
}

###############################################################################
# Test cases
###############################################################################

# Run a single test with retry logic
# Args: $1=target $2=port $3=baud $4=test_name $5=command $6=validation_func
run_single_test() {
    local target="$1"
    local port="$2"
    local baud="$3"
    local test_name="$4"
    local command="$5"
    local validate="${6:-check_response_status}"

    TOTAL=$((TOTAL + 1))
    log_test "$target::$test_name"

    local retries
    retries=$(get_retry_count)
    local retry_delay
    retry_delay=$(get_retry_delay)

    local attempt=0
    local response=""
    local success=false

    while [ $attempt -lt "$retries" ]; do
        attempt=$((attempt + 1))

        response=$(send_test_command "$port" "$baud" "$command")

        if [ -n "$response" ] && $validate "$response"; then
            success=true
            break
        fi

        if [ $attempt -lt "$retries" ]; then
            log_warn "  Attempt $attempt failed, retrying in ${retry_delay}s..."
            sleep "$retry_delay"
        fi
    done

    if [ "$success" = true ]; then
        PASSED=$((PASSED + 1))
        log_ok "$target::$test_name"
        if [ -n "$response" ]; then
            echo "       Response: $response"
        fi
        return 0
    else
        FAILED=$((FAILED + 1))
        log_fail "$target::$test_name (after $attempt attempts)"
        if [ -n "$response" ]; then
            echo "       Response: $response"
        else
            echo "       No response received (timeout)"
        fi
        return 1
    fi
}

# Validate heap response - check free heap > 0
validate_heap() {
    local response="$1"
    check_response_status "$response" || return 1
    local free_heap
    free_heap=$(get_response_field "$response" "free_heap")
    [ "$free_heap" != "N/A" ] && [ "$free_heap" -gt 0 ] 2>/dev/null
}

# Validate info response - check target matches
validate_info_base() {
    local response="$1"
    check_response_status "$response" || return 1
    local target
    target=$(get_response_field "$response" "target")
    [ "$target" = "BASE" ]
}

validate_info_remote() {
    local response="$1"
    check_response_status "$response" || return 1
    local target
    target=$(get_response_field "$response" "target")
    [ "$target" = "REMOTE" ]
}

###############################################################################
# Test suites
###############################################################################

run_common_tests() {
    local target="$1"
    local port="$2"
    local baud="$3"

    log_info "--- Common tests for $target ---"

    run_single_test "$target" "$port" "$baud" "ping" "PING" || true
    run_single_test "$target" "$port" "$baud" "info" "INFO" || true
    run_single_test "$target" "$port" "$baud" "heap" "HEAP" "validate_heap" || true
    run_single_test "$target" "$port" "$baud" "tasks" "TASKS" || true
    run_single_test "$target" "$port" "$baud" "queue_status" "QUEUE STATUS" || true
    run_single_test "$target" "$port" "$baud" "espnow_status" "ESPNOW STATUS" || true
}

run_base_tests() {
    local port="$1"
    local baud="$2"

    log_info "--- BASE-specific tests ---"

    run_single_test "BASE" "$port" "$baud" "info_target_check" "INFO" "validate_info_base" || true
    run_single_test "BASE" "$port" "$baud" "state" "STATE" || true
}

run_remote_tests() {
    local port="$1"
    local baud="$2"

    log_info "--- REMOTE-specific tests ---"

    run_single_test "REMOTE" "$port" "$baud" "info_target_check" "INFO" "validate_info_remote" || true
}

run_single_named_test() {
    local target="$1"
    local port="$2"
    local baud="$3"
    local test_name="$4"

    run_single_test "$target" "$port" "$baud" "$test_name" "$test_name" || true
}

###############################################################################
# Target test execution
###############################################################################

test_target() {
    local target="$1"
    local specific_test="${2:-}"
    local port
    local baud

    port=$(get_port "$target")
    baud=$(get_baud "$target")

    if [ ! -e "$port" ]; then
        log_fail "Serial port $port not found for $target"
        TOTAL=$((TOTAL + 1))
        FAILED=$((FAILED + 1))
        return 1
    fi

    log_info "=== Testing $target on $port at ${baud} baud ==="
    echo ""

    # Wait for device to be ready after possible reset/flash
    local boot_wait
    boot_wait=$(get_boot_wait)
    if [ "$boot_wait" -gt 0 ] 2>/dev/null; then
        log_info "Waiting ${boot_wait}s for device boot..."
        sleep "$boot_wait"
    fi

    if [ -n "$specific_test" ]; then
        run_single_named_test "$target" "$port" "$baud" "$specific_test"
    else
        run_common_tests "$target" "$port" "$baud"

        if [ "$target" = "BASE" ]; then
            run_base_tests "$port" "$baud"
        elif [ "$target" = "REMOTE" ]; then
            run_remote_tests "$port" "$baud"
        fi
    fi

    echo ""
}

###############################################################################
# Report
###############################################################################

print_report() {
    echo ""
    echo "========================================"
    echo "         HARDWARE TEST REPORT"
    echo "========================================"
    echo "  Total:   $TOTAL"
    echo -e "  Passed:  ${GREEN}$PASSED${NC}"
    echo -e "  Failed:  ${RED}$FAILED${NC}"
    echo -e "  Skipped: ${YELLOW}$SKIPPED${NC}"
    echo "========================================"

    if [ $FAILED -eq 0 ] && [ $TOTAL -gt 0 ]; then
        echo -e "  Result:  ${GREEN}ALL TESTS PASSED${NC}"
    elif [ $TOTAL -eq 0 ]; then
        echo -e "  Result:  ${YELLOW}NO TESTS RUN${NC}"
    else
        echo -e "  Result:  ${RED}$FAILED TEST(S) FAILED${NC}"
    fi
    echo "========================================"
    echo ""

    # Also output machine-readable JSON report
    echo "{\"total\":$TOTAL,\"passed\":$PASSED,\"failed\":$FAILED,\"skipped\":$SKIPPED,\"result\":\"$([ $FAILED -eq 0 ] && [ $TOTAL -gt 0 ] && echo 'pass' || echo 'fail')\"}"
}

###############################################################################
# Main
###############################################################################

main() {
    local targets=()
    local specific_test=""

    # Parse arguments
    while [ $# -gt 0 ]; do
        case "$1" in
            BASE|REMOTE)
                targets+=("$1")
                ;;
            -h|--help)
                echo "Usage: $0 [BASE|REMOTE] [TEST_NAME]"
                echo ""
                echo "Run automated hardware tests on ESP32-S3 targets."
                echo ""
                echo "Arguments:"
                echo "  BASE         Test BASE unit only"
                echo "  REMOTE       Test REMOTE unit only"
                echo "  TEST_NAME    Run a specific test (e.g., PING, INFO, HEAP)"
                echo ""
                echo "Test Commands:"
                echo "  PING           Check device is responsive"
                echo "  INFO           Get device info"
                echo "  STATE          Get state machine state (BASE only)"
                echo "  GPIO READ N    Read GPIO pin N"
                echo "  HEAP           Get heap memory stats"
                echo "  TASKS          List FreeRTOS tasks"
                echo "  QUEUE STATUS   Get queue fill levels"
                echo "  ESPNOW STATUS  Get ESP-NOW link status"
                echo ""
                echo "With no arguments, tests both targets with all tests."
                exit 0
                ;;
            *)
                specific_test="$1"
                ;;
        esac
        shift
    done

    # Default: both targets
    if [ ${#targets[@]} -eq 0 ]; then
        targets=(BASE REMOTE)
    fi

    # Check prerequisites
    if [ ! -f "$CONFIG_FILE" ]; then
        log_fail "Config file not found: $CONFIG_FILE"
        exit 1
    fi

    if ! command -v stty &>/dev/null; then
        log_fail "stty command not found (required for serial communication)"
        exit 1
    fi

    log_info "=== Hardware Test Runner ==="
    log_info "Targets: ${targets[*]}"
    if [ -n "$specific_test" ]; then
        log_info "Test: $specific_test"
    fi
    echo ""

    for target in "${targets[@]}"; do
        test_target "$target" "$specific_test"
    done

    print_report

    # Exit with failure if any tests failed
    [ $FAILED -eq 0 ] && [ $TOTAL -gt 0 ]
}

main "$@"
