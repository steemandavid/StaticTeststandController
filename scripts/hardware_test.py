#!/usr/bin/env python3
"""
Automated hardware test runner for StaticTeststandController.

Sends TEST protocol commands over serial to BASE and REMOTE ESP32-S3
units and validates JSON responses.

Usage:
    sudo python3 scripts/hardware_test.py
    sudo python3 scripts/hardware_test.py --base-port /dev/ttyS0 --remote-port /dev/ttyS1
"""

import argparse
import json
import sys
import time

import serial

# ── Configuration ──────────────────────────────────────────────────────────

DEFAULT_BASE_PORT = "/dev/ttyS0"
DEFAULT_REMOTE_PORT = "/dev/ttyS1"
BAUD_RATE = 115200
SERIAL_TIMEOUT = 3  # seconds per read attempt
MAX_READ_LINES = 20  # max lines to scan for a JSON response
SETTLE_TIME = 0.5    # seconds to wait after opening port


# ── Helpers ────────────────────────────────────────────────────────────────

class TestResult:
    def __init__(self, name, target):
        self.name = name
        self.target = target
        self.passed = False
        self.detail = ""
        self.response = None

    def __repr__(self):
        status = "PASS" if self.passed else "FAIL"
        return f"[{status}] {self.target}: {self.name} -- {self.detail}"


def open_serial(port):
    """Open serial port, flush buffers, send warmup command to sync."""
    ser = serial.Serial(port, BAUD_RATE, timeout=SERIAL_TIMEOUT)
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    time.sleep(SETTLE_TIME)
    ser.reset_input_buffer()

    # Warmup: send a dummy PING to flush any remaining boot output,
    # then drain everything before real tests begin.
    ser.write(b"TEST PING\n")
    ser.flush()
    time.sleep(0.5)
    ser.reset_input_buffer()
    return ser


def send_command(ser, command):
    """Send a TEST command and return the parsed JSON response, or None."""
    ser.reset_input_buffer()
    ser.write(f"TEST {command}\n".encode())
    ser.flush()

    # Accumulate data — some responses (TASKS) may span multiple reads
    buf = b""
    deadline = time.monotonic() + SERIAL_TIMEOUT
    while time.monotonic() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf += chunk
        # Check if we have a complete JSON line
        for raw_line in buf.split(b"\n"):
            try:
                text = raw_line.decode("utf-8", errors="replace").strip()
            except Exception:
                continue
            if text.startswith("{") and text.endswith("}"):
                try:
                    return json.loads(text)
                except json.JSONDecodeError:
                    continue
    return None


# ── Test Definitions ───────────────────────────────────────────────────────

def test_ping(ser, target):
    r = TestResult("PING", target)
    resp = send_command(ser, "PING")
    r.response = resp
    if resp is None:
        r.detail = "No response received"
        return r
    if resp.get("status") == "ok" and resp.get("data", {}).get("pong") is True:
        r.passed = True
        r.detail = "pong=true"
    else:
        r.detail = f"Unexpected response: {resp}"
    return r


def test_info(ser, target):
    r = TestResult("INFO", target)
    resp = send_command(ser, "INFO")
    r.response = resp
    if resp is None:
        r.detail = "No response received"
        return r
    data = resp.get("data", {})
    reported_target = data.get("target", "")
    chip = data.get("chip", "")
    idf_ver = data.get("idf_version", "")
    uptime = data.get("uptime_s", -1)

    issues = []
    if resp.get("status") != "ok":
        issues.append(f"status={resp.get('status')}")
    if reported_target != target:
        issues.append(f"target mismatch: expected {target}, got {reported_target}")
    if chip != "ESP32-S3":
        issues.append(f"chip={chip}")
    if not idf_ver:
        issues.append("missing idf_version")
    if uptime < 0:
        issues.append("missing uptime")

    if not issues:
        r.passed = True
        r.detail = f"target={reported_target}, chip={chip}, idf={idf_ver}, uptime={uptime}s"
    else:
        r.detail = "; ".join(issues)
    return r


def test_state(ser, target):
    r = TestResult("STATE", target)
    resp = send_command(ser, "STATE")
    r.response = resp
    if resp is None:
        r.detail = "No response received"
        return r

    if target == "REMOTE":
        # STATE is BASE-only; REMOTE should return an error
        if resp.get("status") == "error":
            r.passed = True
            r.detail = "Correctly rejected on REMOTE"
        else:
            r.detail = f"Expected error on REMOTE, got: {resp}"
        return r

    # BASE should return a valid state
    data = resp.get("data", {})
    state_num = data.get("state", -1)
    state_name = data.get("name", "")
    if resp.get("status") == "ok" and state_name:
        r.passed = True
        r.detail = f"state={state_num} ({state_name})"
    else:
        r.detail = f"Unexpected: {resp}"
    return r


def test_heap(ser, target):
    r = TestResult("HEAP", target)
    resp = send_command(ser, "HEAP")
    r.response = resp
    if resp is None:
        r.detail = "No response received"
        return r
    data = resp.get("data", {})
    free_heap = data.get("free_heap", 0)
    min_free = data.get("min_free_heap", 0)
    total = data.get("total_heap", 0)

    issues = []
    if resp.get("status") != "ok":
        issues.append(f"status={resp.get('status')}")
    if free_heap < 50000:
        issues.append(f"low free_heap={free_heap}")
    if min_free < 30000:
        issues.append(f"low min_free_heap={min_free}")
    if total == 0:
        issues.append("total_heap=0")

    if not issues:
        r.passed = True
        r.detail = f"free={free_heap}, min_free={min_free}, total={total}"
    else:
        r.detail = "; ".join(issues) + f" (free={free_heap}, min={min_free}, total={total})"
    return r


def test_tasks(ser, target):
    r = TestResult("TASKS", target)
    resp = send_command(ser, "TASKS")
    r.response = resp
    if resp is None:
        r.detail = "No response received"
        return r
    data = resp.get("data", {})
    count = data.get("count", 0)
    tasks = data.get("tasks", [])
    task_names = [t.get("name", "") for t in tasks]

    issues = []
    if resp.get("status") != "ok":
        issues.append(f"status={resp.get('status')}")
    if count < 5:
        issues.append(f"low task count={count}")

    # Check for expected tasks
    expected = ["espnow_rx", "espnow_tx", "rgb_led", "test_proto"]
    if target == "BASE":
        expected.append("state_mach")
    else:
        expected.extend(["input", "display"])

    for exp in expected:
        found = any(exp in name for name in task_names)
        if not found:
            issues.append(f"missing task '{exp}'")

    # Check stack high-water marks (warn if < 128 bytes remaining)
    for t in tasks:
        hwm = t.get("stack_hwm", 999)
        if hwm < 128:
            issues.append(f"task '{t.get('name')}' stack_hwm={hwm} (dangerously low)")

    if not issues:
        r.passed = True
        r.detail = f"{count} tasks: {', '.join(task_names)}"
    else:
        r.detail = "; ".join(issues) + f" (found: {', '.join(task_names)})"
    return r


def test_queue_status(ser, target):
    r = TestResult("QUEUE STATUS", target)
    resp = send_command(ser, "QUEUE STATUS")
    r.response = resp
    if resp is None:
        r.detail = "No response received"
        return r
    data = resp.get("data", {})

    issues = []
    if resp.get("status") != "ok":
        issues.append(f"status={resp.get('status')}")

    expected_queues = ["espnow_rx", "espnow_tx", "input_event", "display_cmd", "state_event"]
    for q in expected_queues:
        if q not in data:
            issues.append(f"missing queue '{q}'")

    if target == "BASE":
        for q in ["adc_sample", "log"]:
            if q not in data:
                issues.append(f"missing BASE queue '{q}'")

    # Check for backed-up queues (> 5 messages is suspicious in idle)
    for q, count in data.items():
        if isinstance(count, int) and count > 5:
            issues.append(f"queue '{q}' backed up: {count} messages")

    if not issues:
        r.passed = True
        r.detail = f"queues: {data}"
    else:
        r.detail = "; ".join(issues) + f" (data: {data})"
    return r


def test_espnow_status(ser, target):
    r = TestResult("ESPNOW STATUS", target)
    resp = send_command(ser, "ESPNOW STATUS")
    r.response = resp
    if resp is None:
        r.detail = "No response received"
        return r
    data = resp.get("data", {})
    if resp.get("status") == "ok" and data.get("initialized") is True:
        r.passed = True
        r.detail = "ESP-NOW initialized"
    else:
        r.detail = f"Unexpected: {resp}"
    return r


def test_gpio_read(ser, target):
    """Read a known safe GPIO pin (GPIO 0 - boot button, typically pulled high)."""
    r = TestResult("GPIO READ", target)
    resp = send_command(ser, "GPIO READ 0")
    r.response = resp
    if resp is None:
        r.detail = "No response received"
        return r
    data = resp.get("data", {})
    if resp.get("status") == "ok" and "level" in data and "pin" in data:
        r.passed = True
        r.detail = f"pin={data['pin']}, level={data['level']}"
    else:
        r.detail = f"Unexpected: {resp}"
    return r


# ── Runner ─────────────────────────────────────────────────────────────────

ALL_TESTS = [
    test_ping,
    test_info,
    test_state,
    test_heap,
    test_tasks,
    test_queue_status,
    test_espnow_status,
    test_gpio_read,
]


def run_tests_on_device(port, target):
    """Run all tests on a single device. Returns list of TestResult."""
    results = []
    try:
        ser = open_serial(port)
    except Exception as e:
        # Return all tests as failed if we can't open the port
        for test_fn in ALL_TESTS:
            r = TestResult(test_fn.__name__.replace("test_", "").upper(), target)
            r.detail = f"Could not open {port}: {e}"
            results.append(r)
        return results

    try:
        for test_fn in ALL_TESTS:
            try:
                result = test_fn(ser, target)
                results.append(result)
            except Exception as e:
                r = TestResult(test_fn.__name__.replace("test_", "").upper(), target)
                r.detail = f"Exception: {e}"
                results.append(r)
    finally:
        ser.close()

    return results


def print_report(all_results):
    """Print a summary report."""
    passed = sum(1 for r in all_results if r.passed)
    failed = sum(1 for r in all_results if not r.passed)
    total = len(all_results)

    print("\n" + "=" * 72)
    print("  HARDWARE TEST REPORT")
    print("=" * 72)

    for target in ["BASE", "REMOTE"]:
        target_results = [r for r in all_results if r.target == target]
        if not target_results:
            continue
        print(f"\n── {target} ──")
        for r in target_results:
            marker = "PASS" if r.passed else "FAIL"
            print(f"  [{marker}] {r.name:<20s} {r.detail}")

    print("\n" + "-" * 72)
    print(f"  TOTAL: {total}  |  PASSED: {passed}  |  FAILED: {failed}")
    if failed == 0:
        print("  All tests passed.")
    print("-" * 72 + "\n")

    return failed


def main():
    parser = argparse.ArgumentParser(description="Hardware test runner for StaticTeststandController")
    parser.add_argument("--base-port", default=DEFAULT_BASE_PORT, help=f"BASE serial port (default: {DEFAULT_BASE_PORT})")
    parser.add_argument("--remote-port", default=DEFAULT_REMOTE_PORT, help=f"REMOTE serial port (default: {DEFAULT_REMOTE_PORT})")
    parser.add_argument("--base-only", action="store_true", help="Only test BASE unit")
    parser.add_argument("--remote-only", action="store_true", help="Only test REMOTE unit")
    args = parser.parse_args()

    all_results = []

    if not args.remote_only:
        print(f"Testing BASE on {args.base_port}...")
        all_results.extend(run_tests_on_device(args.base_port, "BASE"))

    if not args.base_only:
        print(f"Testing REMOTE on {args.remote_port}...")
        all_results.extend(run_tests_on_device(args.remote_port, "REMOTE"))

    failed = print_report(all_results)
    sys.exit(1 if failed > 0 else 0)


if __name__ == "__main__":
    main()
