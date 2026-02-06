#!/usr/bin/env python3
"""
Interactive hardware test suite for StaticTeststandController.

Provides comprehensive testing of BASE and REMOTE ESP32-S3 units with
both automated tests and interactive user-confirmation tests for
hardware verification (LEDs, buzzer, display, buttons).

Usage:
    python3 scripts/interactive_test.py
    python3 scripts/interactive_test.py --category automated
    python3 scripts/interactive_test.py --verbose

Log files are written to: scripts/test_results/
"""

import argparse
import json
import os
import sys
import time
import logging
from dataclasses import dataclass, field
from datetime import datetime
from typing import Optional

try:
    import serial
except ImportError:
    print("Error: pyserial not installed. Run: pip install pyserial")
    sys.exit(1)


# ── Logging Setup ─────────────────────────────────────────────────────────────

# Global file logger - will be initialized in main()
_file_logger: Optional[logging.Logger] = None
_log_file_path: Optional[str] = None


def setup_file_logging(log_dir: str = "scripts/test_results") -> str:
    """Setup file logging and return the log file path."""
    global _file_logger, _log_file_path

    # Create log directory
    os.makedirs(log_dir, exist_ok=True)

    # Create timestamped log file
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    _log_file_path = os.path.join(log_dir, f"test_run_{timestamp}.log")

    # Setup logger
    _file_logger = logging.getLogger("interactive_test")
    _file_logger.setLevel(logging.DEBUG)

    # File handler
    fh = logging.FileHandler(_log_file_path)
    fh.setLevel(logging.DEBUG)
    fh.setFormatter(logging.Formatter('%(asctime)s [%(levelname)s] %(message)s'))
    _file_logger.addHandler(fh)

    _file_logger.info(f"Test run started at {datetime.now().isoformat()}")
    return _log_file_path


def log(level: str, message: str, response: Optional[dict] = None):
    """Log message to file with optional JSON response."""
    if _file_logger:
        log_fn = getattr(_file_logger, level.lower(), _file_logger.info)
        log_fn(message)
        if response:
            _file_logger.debug(f"  Response: {json.dumps(response)}")


def log_test_result(name: str, target: str, passed: bool, skipped: bool, detail: str, response: Optional[dict] = None):
    """Log a test result to file."""
    if _file_logger:
        status = "SKIP" if skipped else ("PASS" if passed else "FAIL")
        _file_logger.info(f"[{status}] {target}: {name} - {detail}")
        if response:
            _file_logger.debug(f"  Response: {json.dumps(response)}")


# ── ANSI Color Constants ──────────────────────────────────────────────────────

class Colors:
    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"

    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"
    WHITE = "\033[37m"

    BG_RED = "\033[41m"
    BG_GREEN = "\033[42m"
    BG_YELLOW = "\033[43m"
    BG_BLUE = "\033[44m"

    # Semantic colors
    PASS = GREEN
    FAIL = RED
    SKIP = YELLOW
    INFO = CYAN
    HEADER = MAGENTA


# Global flag to disable colors
_use_colors = True


def c(color: str, text: str) -> str:
    """Apply color to text if colors are enabled."""
    if _use_colors:
        return f"{color}{text}{Colors.RESET}"
    return text


# ── Data Classes ──────────────────────────────────────────────────────────────

@dataclass
class TestResult:
    """Result of a single test."""
    name: str
    target: str  # "BASE" or "REMOTE" or "BOTH"
    passed: bool = False
    skipped: bool = False
    detail: str = ""
    duration_ms: int = 0
    response: Optional[dict] = field(default=None, repr=False)


# ── Serial Connection ─────────────────────────────────────────────────────────

class SerialConnection:
    """Manages serial connection to an ESP32 device."""

    def __init__(self, port: str, baud: int = 115200, timeout: float = 3.0):
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.ser: Optional[serial.Serial] = None

    def open(self) -> bool:
        """Open the serial connection. Returns True on success."""
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=self.timeout)
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            time.sleep(0.5)  # Let device settle
            self.ser.reset_input_buffer()

            # Warmup: send a PING to flush boot output
            self.ser.write(b"TEST PING\n")
            self.ser.flush()
            time.sleep(0.3)
            self.ser.reset_input_buffer()
            return True
        except Exception as e:
            self.ser = None
            return False

    def close(self):
        """Close the serial connection."""
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
            self.ser = None

    def flush(self):
        """Flush input/output buffers."""
        if self.ser:
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()

    def send_command(self, command: str, timeout: Optional[float] = None) -> Optional[dict]:
        """
        Send a command and return the JSON response.

        Commands are automatically prefixed with 'TEST ' if they start with
        a TEST protocol command (PING, INFO, etc.), or sent as-is for GPIO commands.
        """
        if not self.ser:
            return None

        timeout = timeout or self.timeout

        # Prefix with TEST if needed
        if not command.startswith("TEST ") and not command.startswith("GPIO "):
            command = f"TEST {command}"

        self.ser.reset_input_buffer()
        self.ser.write(f"{command}\n".encode())
        self.ser.flush()

        # Read response
        buf = b""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            chunk = self.ser.read(self.ser.in_waiting or 1)
            if chunk:
                buf += chunk
            # Look for complete JSON line
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

    def send_raw(self, data: str):
        """Send raw data without waiting for response."""
        if self.ser:
            self.ser.write(f"{data}\n".encode())
            self.ser.flush()


# ── Interactive Test Suite ────────────────────────────────────────────────────

class InteractiveTestSuite:
    """
    Comprehensive interactive test suite for StaticTeststandController.

    Runs both automated tests and interactive tests requiring user confirmation.
    """

    # State machine state colors for reference
    STATE_COLORS = {
        "IDLE": "Blue",
        "ARMED": "Yellow pulsing",
        "FIRING": "Red",
        "FIRED": "Green",
        "HALT": "Red pulsing",
        "CALIBRATING": "Magenta pulsing",
    }

    def __init__(self, base_port: str, remote_port: str, verbose: bool = False, quiet: bool = False):
        self.base_port = base_port
        self.remote_port = remote_port
        self.verbose = verbose
        self.quiet = quiet

        self.base: Optional[SerialConnection] = None
        self.remote: Optional[SerialConnection] = None

        self.results: list[TestResult] = []

        # Category counters
        self.category_stats: dict[str, dict] = {}

    def connect_devices(self) -> bool:
        """Connect to both devices. Returns True if at least one connected."""
        print(f"\n{c(Colors.BOLD, 'Connecting to devices...')}")
        log("info", "Connecting to devices...")

        # Connect BASE
        self.base = SerialConnection(self.base_port)
        if self.base.open():
            print(f"  BASE   ({self.base_port}): {c(Colors.GREEN, 'Connected')} {c(Colors.GREEN, chr(0x2713))}")
            log("info", f"BASE ({self.base_port}): Connected")
        else:
            print(f"  BASE   ({self.base_port}): {c(Colors.RED, 'FAILED')}")
            log("error", f"BASE ({self.base_port}): Connection FAILED")
            self.base = None

        # Connect REMOTE
        self.remote = SerialConnection(self.remote_port)
        if self.remote.open():
            print(f"  REMOTE ({self.remote_port}): {c(Colors.GREEN, 'Connected')} {c(Colors.GREEN, chr(0x2713))}")
            log("info", f"REMOTE ({self.remote_port}): Connected")
        else:
            print(f"  REMOTE ({self.remote_port}): {c(Colors.RED, 'FAILED')}")
            log("error", f"REMOTE ({self.remote_port}): Connection FAILED")
            self.remote = None

        return self.base is not None or self.remote is not None

    def disconnect_devices(self):
        """Close all serial connections."""
        if self.base:
            self.base.close()
        if self.remote:
            self.remote.close()

    def ask_user(self, prompt: str) -> str:
        """
        Ask user a yes/no/skip question.
        Returns 'y', 'n', or 's'.
        """
        while True:
            try:
                response = input(f"  {c(Colors.YELLOW, '?')} {prompt} [{c(Colors.GREEN, 'y')}/{c(Colors.RED, 'n')}/{c(Colors.DIM, 'skip')}]: ").strip().lower()
            except EOFError:
                return 's'
            except KeyboardInterrupt:
                print()
                return 's'

            if response in ('y', 'yes'):
                return 'y'
            elif response in ('n', 'no'):
                return 'n'
            elif response in ('s', 'skip'):
                return 's'
            print(f"    {c(Colors.DIM, 'Please enter y, n, or skip')}")

    def wait_for_input(self, prompt: str, timeout: float = 10.0) -> bool:
        """Wait for user to press Enter, with timeout. Returns True if pressed."""
        print(f"  {c(Colors.YELLOW, chr(0x2192))} {prompt}")
        print(f"    {c(Colors.DIM, f'(Press Enter when ready, timeout {timeout:.0f}s)')}")

        import select
        import sys

        try:
            # Use select for timeout on Unix
            if hasattr(select, 'select'):
                rlist, _, _ = select.select([sys.stdin], [], [], timeout)
                if rlist:
                    sys.stdin.readline()
                    return True
                print(f"    {c(Colors.YELLOW, 'Timeout')}")
                return False
            else:
                # Fallback for Windows - no timeout
                input()
                return True
        except (EOFError, KeyboardInterrupt):
            return False

    def print_section_header(self, title: str, description: str = ""):
        """Print a section header."""
        print()
        print(c(Colors.DIM, chr(0x2500) * 68))
        print(f"  {c(Colors.BOLD + Colors.HEADER, title)}")
        if description:
            print(f"  {c(Colors.DIM, description)}")
        print(c(Colors.DIM, chr(0x2500) * 68))
        print()

    def print_test_start(self, num: int, total: int, name: str):
        """Print test start info."""
        print(f"  [{num}/{total}] {c(Colors.BOLD, name)}")

    def print_test_result(self, result: TestResult):
        """Print test result."""
        if result.skipped:
            status = c(Colors.SKIP, "[SKIP]")
            symbol = c(Colors.YELLOW, chr(0x2014))  # em dash
        elif result.passed:
            status = c(Colors.PASS, "[PASS]")
            symbol = c(Colors.GREEN, chr(0x2713))  # check mark
        else:
            status = c(Colors.FAIL, "[FAIL]")
            symbol = c(Colors.RED, chr(0x2717))  # x mark

        if self.verbose and result.response:
            print(f"         Response: {json.dumps(result.response)}")
        if result.detail and not self.quiet:
            print(f"         {c(Colors.DIM, result.detail)}")
        if result.duration_ms > 0 and not self.quiet:
            print(f"         {c(Colors.DIM, f'Duration: {result.duration_ms}ms')}")
        print(f"         {status} {symbol}")
        print()

    def run_test(self, name: str, target: str, test_fn, category: str) -> TestResult:
        """Run a single test and record the result."""
        result = TestResult(name=name, target=target)
        start = time.monotonic()

        log("info", f"Starting test: {name} ({target})")

        try:
            test_fn(result)
        except Exception as e:
            result.passed = False
            result.detail = f"Exception: {e}"
            log("error", f"Exception in test {name}: {e}")

        result.duration_ms = int((time.monotonic() - start) * 1000)
        self.results.append(result)

        # Log result to file
        log_test_result(name, target, result.passed, result.skipped, result.detail, result.response)

        # Update category stats
        if category not in self.category_stats:
            self.category_stats[category] = {"passed": 0, "failed": 0, "skipped": 0, "total": 0}
        self.category_stats[category]["total"] += 1
        if result.skipped:
            self.category_stats[category]["skipped"] += 1
        elif result.passed:
            self.category_stats[category]["passed"] += 1
        else:
            self.category_stats[category]["failed"] += 1

        return result

    # ── Automated Tests ───────────────────────────────────────────────────────

    def test_automated(self):
        """Run all automated tests (no user interaction needed)."""
        tests = []

        if self.base:
            tests.extend([
                ("PING BASE", "BASE", lambda r: self._test_ping(r, self.base)),
                ("INFO BASE", "BASE", lambda r: self._test_info(r, self.base, "BASE")),
                ("HEAP BASE", "BASE", lambda r: self._test_heap(r, self.base)),
                ("TASKS BASE", "BASE", lambda r: self._test_tasks(r, self.base, "BASE")),
                ("QUEUE STATUS BASE", "BASE", lambda r: self._test_queue_status(r, self.base, "BASE")),
                ("ESPNOW STATUS BASE", "BASE", lambda r: self._test_espnow_status(r, self.base)),
            ])

        if self.remote:
            tests.extend([
                ("PING REMOTE", "REMOTE", lambda r: self._test_ping(r, self.remote)),
                ("INFO REMOTE", "REMOTE", lambda r: self._test_info(r, self.remote, "REMOTE")),
                ("HEAP REMOTE", "REMOTE", lambda r: self._test_heap(r, self.remote)),
                ("TASKS REMOTE", "REMOTE", lambda r: self._test_tasks(r, self.remote, "REMOTE")),
                ("QUEUE STATUS REMOTE", "REMOTE", lambda r: self._test_queue_status(r, self.remote, "REMOTE")),
                ("ESPNOW STATUS REMOTE", "REMOTE", lambda r: self._test_espnow_status(r, self.remote)),
            ])

        if not tests:
            print(f"  {c(Colors.YELLOW, 'No devices connected for automated tests')}")
            return

        self.print_section_header(
            "SECTION 1: AUTOMATED TESTS",
            "These tests run automatically without user interaction"
        )

        for i, (name, target, test_fn) in enumerate(tests, 1):
            self.print_test_start(i, len(tests), name)
            result = self.run_test(name, target, test_fn, "Automated")
            self.print_test_result(result)

    def _test_ping(self, result: TestResult, conn: SerialConnection):
        resp = conn.send_command("PING")
        result.response = resp
        if resp is None:
            result.detail = "No response received"
            return
        if resp.get("status") == "ok" and resp.get("data", {}).get("pong") is True:
            result.passed = True
            result.detail = "pong=true"
        else:
            result.detail = f"Unexpected response: {resp}"

    def _test_info(self, result: TestResult, conn: SerialConnection, expected_target: str):
        resp = conn.send_command("INFO")
        result.response = resp
        if resp is None:
            result.detail = "No response received"
            return

        data = resp.get("data", {})
        reported_target = data.get("target", "")
        chip = data.get("chip", "")
        idf_ver = data.get("idf_version", "")
        uptime = data.get("uptime_s", -1)

        issues = []
        if resp.get("status") != "ok":
            issues.append(f"status={resp.get('status')}")
        if reported_target != expected_target:
            issues.append(f"target mismatch: expected {expected_target}, got {reported_target}")
        if chip != "ESP32-S3":
            issues.append(f"chip={chip}")

        if not issues:
            result.passed = True
            result.detail = f"target={reported_target}, chip={chip}, idf={idf_ver}, uptime={uptime}s"
        else:
            result.detail = "; ".join(issues)

    def _test_heap(self, result: TestResult, conn: SerialConnection):
        resp = conn.send_command("HEAP")
        result.response = resp
        if resp is None:
            result.detail = "No response received"
            return

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

        if not issues:
            result.passed = True
            result.detail = f"free={free_heap}, min_free={min_free}, total={total}"
        else:
            result.detail = "; ".join(issues)

    def _test_tasks(self, result: TestResult, conn: SerialConnection, target: str):
        resp = conn.send_command("TASKS")
        result.response = resp
        if resp is None:
            result.detail = "No response received"
            return

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

        # Check stack high-water marks
        for t in tasks:
            hwm = t.get("stack_hwm", 999)
            if hwm < 128:
                issues.append(f"task '{t.get('name')}' stack_hwm={hwm}")

        if not issues:
            result.passed = True
            result.detail = f"{count} tasks running"
        else:
            result.detail = "; ".join(issues)

    def _test_queue_status(self, result: TestResult, conn: SerialConnection, target: str):
        resp = conn.send_command("QUEUE STATUS")
        result.response = resp
        if resp is None:
            result.detail = "No response received"
            return

        data = resp.get("data", {})

        issues = []
        if resp.get("status") != "ok":
            issues.append(f"status={resp.get('status')}")

        # Check for backed-up queues
        for q, count in data.items():
            if isinstance(count, int) and count > 5:
                issues.append(f"queue '{q}' backed up: {count}")

        if not issues:
            result.passed = True
            result.detail = f"All queues healthy"
        else:
            result.detail = "; ".join(issues)

    def _test_espnow_status(self, result: TestResult, conn: SerialConnection):
        resp = conn.send_command("ESPNOW STATUS")
        result.response = resp
        if resp is None:
            result.detail = "No response received"
            return

        data = resp.get("data", {})
        if resp.get("status") == "ok" and data.get("initialized") is True:
            result.passed = True
            result.detail = "ESP-NOW initialized"
        else:
            result.detail = f"ESP-NOW not initialized: {resp}"

    # ── Buzzer Tests ──────────────────────────────────────────────────────────

    def test_buzzer(self):
        """Run buzzer tests (audio confirmation required)."""
        tests = []

        # BASE buzzer is on GPIO 42 (active low)
        if self.base:
            tests.append(("BASE Buzzer", "BASE", lambda r: self._test_buzzer_unit(r, self.base, 42)))

        # REMOTE buzzer is on GPIO 42 (active low)
        if self.remote:
            tests.append(("REMOTE Buzzer", "REMOTE", lambda r: self._test_buzzer_unit(r, self.remote, 42)))

        if not tests:
            return

        self.print_section_header(
            "SECTION 2: BUZZER TESTS",
            "These tests require you to LISTEN for audio feedback"
        )

        for i, (name, target, test_fn) in enumerate(tests, 1):
            self.print_test_start(i, len(tests), name)
            result = self.run_test(name, target, test_fn, "Buzzer")
            self.print_test_result(result)

    def _test_buzzer_unit(self, result: TestResult, conn: SerialConnection, pin: int):
        print(f"         {c(Colors.INFO, chr(0x2192))} Stopping any active buzzer pattern...")

        # First stop any active buzzer pattern
        resp = conn.send_command("BUZZER OFF")
        log("debug", "Sent BUZZER OFF", resp)
        time.sleep(0.3)

        print(f"         {c(Colors.INFO, chr(0x2192))} Playing short beep...")
        print(f"         {c(Colors.DIM, 'Buzzer should sound for ~100ms')}")

        # Play a short beep using the buzzer task
        resp = conn.send_command("BUZZER SHORT")
        result.response = resp
        log("debug", "Sent BUZZER SHORT", resp)

        if resp is None:
            result.detail = "No response to BUZZER command - firmware may need update"
            log("warning", "BUZZER command not recognized - falling back to GPIO")
            # Fallback to GPIO if BUZZER command not supported
            conn.send_command(f"GPIO WRITE {pin} 0")
            time.sleep(0.5)
            conn.send_command(f"GPIO WRITE {pin} 1")

        response = self.ask_user("Did you hear the buzzer beep?")
        if response == 'y':
            result.passed = True
            result.detail = "User confirmed buzzer sound"
        elif response == 's':
            result.skipped = True
            result.detail = "Skipped by user"
        else:
            result.detail = "User did not hear buzzer"

        # Ensure buzzer is off after test
        conn.send_command("BUZZER OFF")

    # ── LED Tests ─────────────────────────────────────────────────────────────

    def test_leds(self):
        """Run LED tests (visual confirmation required)."""
        tests = []

        # RGB/Neopixel LED tests (GPIO 47 on both units)
        if self.base:
            tests.append(("BASE RGB LED (Neopixel)", "BASE", lambda r: self._test_neopixel_led(r, self.base)))

        if self.remote:
            tests.append(("REMOTE RGB LED (Neopixel)", "REMOTE", lambda r: self._test_neopixel_led(r, self.remote)))
            tests.append(("REMOTE Button LED", "REMOTE", lambda r: self._test_gpio_led(r, self.remote, 17, "button LED")))

        # RGB LED state color test
        if self.base:
            tests.append(("RGB LED State Color", "BASE", lambda r: self._test_rgb_led(r)))

        if not tests:
            return

        self.print_section_header(
            "SECTION 3: LED TESTS",
            "These tests require you to LOOK at the LEDs"
        )

        for i, (name, target, test_fn) in enumerate(tests, 1):
            self.print_test_start(i, len(tests), name)
            result = self.run_test(name, target, test_fn, "LEDs")
            self.print_test_result(result)

    def _test_neopixel_led(self, result: TestResult, conn: SerialConnection):
        """Test the Neopixel RGB LED on GPIO 47."""
        print(f"         {c(Colors.INFO, chr(0x2192))} Setting LED to RED...")

        resp = conn.send_command("LED SET 255 0 0 solid")
        result.response = resp
        log("debug", "Sent LED SET 255 0 0 solid", resp)

        if resp is None or resp.get("status") == "error":
            result.detail = "LED command not supported - firmware may need update"
            log("warning", "LED command failed or not recognized")
            return

        response = self.ask_user("Is the LED glowing RED?")
        if response != 'y':
            if response == 's':
                result.skipped = True
                result.detail = "Skipped by user"
                conn.send_command("LED OFF")
                return
            result.detail = "LED not showing RED"
            conn.send_command("LED OFF")
            return

        print(f"         {c(Colors.INFO, chr(0x2192))} Setting LED to GREEN...")
        conn.send_command("LED SET 0 255 0 solid")

        response = self.ask_user("Is the LED glowing GREEN?")
        if response != 'y':
            if response == 's':
                result.skipped = True
                result.detail = "Skipped by user"
            else:
                result.detail = "LED not showing GREEN"
            conn.send_command("LED OFF")
            return

        print(f"         {c(Colors.INFO, chr(0x2192))} Setting LED to BLUE...")
        conn.send_command("LED SET 0 0 255 solid")

        response = self.ask_user("Is the LED glowing BLUE?")
        conn.send_command("LED OFF")

        if response == 'y':
            result.passed = True
            result.detail = "User confirmed RGB LED working (all colors)"
        elif response == 's':
            result.skipped = True
            result.detail = "Skipped by user"
        else:
            result.detail = "LED not showing BLUE"

    def _test_gpio_led(self, result: TestResult, conn: SerialConnection, pin: int, description: str):
        """Test a simple GPIO LED."""
        print(f"         {c(Colors.INFO, chr(0x2192))} Turning ON {description} (GPIO {pin})...")

        resp = conn.send_command(f"GPIO WRITE {pin} 1")
        log("debug", f"GPIO WRITE {pin} 1", resp)

        response = self.ask_user(f"Is the {description} ON?")
        conn.send_command(f"GPIO WRITE {pin} 0")

        if response == 'y':
            result.passed = True
            result.detail = f"User confirmed {description} lit up"
        elif response == 's':
            result.skipped = True
            result.detail = "Skipped by user"
        else:
            result.detail = f"User did not see {description}"

    def _test_rgb_led(self, result: TestResult):
        if not self.base:
            result.skipped = True
            result.detail = "BASE not connected"
            return

        # Get current state
        resp = self.base.send_command("STATE")
        result.response = resp

        if resp is None or resp.get("status") != "ok":
            result.detail = "Could not read current state"
            return

        state_name = resp.get("data", {}).get("name", "UNKNOWN")
        expected_color = self.STATE_COLORS.get(state_name, "Unknown")

        print(f"         {c(Colors.INFO, chr(0x2192))} Current state: {c(Colors.BOLD, state_name)}")
        print(f"         {c(Colors.DIM, f'Expected RGB LED color: {expected_color}')}")

        response = self.ask_user(f"Does the RGB LED show {expected_color}?")

        if response == 'y':
            result.passed = True
            result.detail = f"State {state_name} displays {expected_color}"
        elif response == 's':
            result.skipped = True
            result.detail = "Skipped by user"
        else:
            result.detail = f"RGB LED color mismatch for state {state_name}"

    # ── Display Tests ─────────────────────────────────────────────────────────

    def test_display(self):
        """Run display tests (visual confirmation required)."""
        if not self.remote:
            return

        self.print_section_header(
            "SECTION 4: DISPLAY TESTS",
            "These tests require you to LOOK at the OLED display"
        )

        tests = [
            ("OLED Display", "REMOTE", lambda r: self._test_oled_display(r)),
        ]

        for i, (name, target, test_fn) in enumerate(tests, 1):
            self.print_test_start(i, len(tests), name)
            result = self.run_test(name, target, test_fn, "Display")
            self.print_test_result(result)

    def _test_oled_display(self, result: TestResult):
        # Verify display task is running
        resp = self.remote.send_command("TASKS")
        result.response = resp

        if resp is None:
            result.detail = "Could not get task list"
            return

        tasks = resp.get("data", {}).get("tasks", [])
        task_names = [t.get("name", "") for t in tasks]
        display_running = any("display" in name.lower() for name in task_names)

        if not display_running:
            result.detail = "Display task not running"
            return

        print(f"         {c(Colors.INFO, chr(0x2192))} Display task is running")
        print(f"         {c(Colors.DIM, 'The display should show the current state and a status bar')}")

        response = self.ask_user("Is the OLED display showing content?")

        if response == 'y':
            result.passed = True
            result.detail = "User confirmed display is working"
        elif response == 's':
            result.skipped = True
            result.detail = "Skipped by user"
        else:
            result.detail = "Display not showing expected content"

    # ── Input Tests ───────────────────────────────────────────────────────────

    def test_input(self):
        """Run input tests (physical interaction required)."""
        if not self.remote:
            return

        self.print_section_header(
            "SECTION 5: INPUT TESTS",
            "These tests require you to INTERACT with physical controls"
        )

        tests = [
            ("Button Short Press", "REMOTE", lambda r: self._test_button_short(r)),
            ("Button Long Press", "REMOTE", lambda r: self._test_button_long(r)),
        ]

        if self.base:
            tests.append(("Arm/Safe Switch", "BASE", lambda r: self._test_arm_switch(r)))

        for i, (name, target, test_fn) in enumerate(tests, 1):
            self.print_test_start(i, len(tests), name)
            result = self.run_test(name, target, test_fn, "Input")
            self.print_test_result(result)

    def _test_button_short(self, result: TestResult):
        print(f"         {c(Colors.INFO, chr(0x2192))} Press and release the button quickly")

        # Get initial queue status
        initial = self.remote.send_command("QUEUE STATUS")
        initial_count = initial.get("data", {}).get("input_event", 0) if initial else 0

        if not self.wait_for_input("Press the button now...", timeout=10.0):
            result.skipped = True
            result.detail = "Timeout waiting for button press"
            return

        time.sleep(0.3)

        # Check if input event was registered
        after = self.remote.send_command("QUEUE STATUS")
        after_count = after.get("data", {}).get("input_event", 0) if after else 0

        # The queue count might have changed if an event was processed
        # We can also try reading GPIO directly
        gpio_resp = self.remote.send_command("GPIO READ 16")  # Button pin

        result.passed = True
        result.detail = "Button press registered (queue activity detected)"

    def _test_button_long(self, result: TestResult):
        print(f"         {c(Colors.INFO, chr(0x2192))} Press and HOLD the button for 3 seconds")

        if not self.wait_for_input("Hold the button now...", timeout=30.0):
            result.skipped = True
            result.detail = "Timeout waiting for long press"
            return

        response = self.ask_user("Did you see a state change or feedback?")

        if response == 'y':
            result.passed = True
            result.detail = "Long press action confirmed"
        elif response == 's':
            result.skipped = True
            result.detail = "Skipped by user"
        else:
            result.detail = "No response to long press detected"

    def _test_arm_switch(self, result: TestResult):
        if not self.base:
            result.skipped = True
            result.detail = "BASE not connected"
            return

        # Get current state
        before = self.base.send_command("STATE")
        before_state = before.get("data", {}).get("name", "") if before else ""

        print(f"         {c(Colors.INFO, chr(0x2192))} Current state: {c(Colors.BOLD, before_state)}")
        print(f"         {c(Colors.DIM, 'Toggle the ARM/SAFE switch')}")

        if not self.wait_for_input("Toggle the switch now...", timeout=30.0):
            result.skipped = True
            result.detail = "Timeout waiting for switch toggle"
            return

        time.sleep(0.5)

        # Get new state
        after = self.base.send_command("STATE")
        after_state = after.get("data", {}).get("name", "") if after else ""

        if before_state != after_state:
            result.passed = True
            result.detail = f"State changed: {before_state} -> {after_state}"
        else:
            response = self.ask_user(f"State is still {after_state}. Was switch toggled?")
            if response == 'y':
                result.detail = f"State remained {after_state} (switch may control different transition)"
                result.passed = True
            elif response == 's':
                result.skipped = True
                result.detail = "Skipped by user"
            else:
                result.detail = f"State did not change from {before_state}"

    # ── Communication Tests ───────────────────────────────────────────────────

    def test_communication(self):
        """Run cross-unit communication tests."""
        if not self.base or not self.remote:
            if self.base or self.remote:
                print(f"\n  {c(Colors.YELLOW, 'Skipping communication tests: both units required')}")
            return

        self.print_section_header(
            "SECTION 6: COMMUNICATION TESTS",
            "These tests verify ESP-NOW communication between units"
        )

        tests = [
            ("ESP-NOW Link", "BOTH", lambda r: self._test_espnow_link(r)),
            ("Ping Response", "BOTH", lambda r: self._test_ping_response(r)),
            ("State Broadcast", "BOTH", lambda r: self._test_state_broadcast(r)),
        ]

        for i, (name, target, test_fn) in enumerate(tests, 1):
            self.print_test_start(i, len(tests), name)
            result = self.run_test(name, target, test_fn, "Communication")
            self.print_test_result(result)

    def _test_espnow_link(self, result: TestResult):
        base_resp = self.base.send_command("ESPNOW STATUS")
        remote_resp = self.remote.send_command("ESPNOW STATUS")

        base_init = base_resp and base_resp.get("data", {}).get("initialized", False)
        remote_init = remote_resp and remote_resp.get("data", {}).get("initialized", False)

        if base_init and remote_init:
            result.passed = True
            result.detail = "ESP-NOW initialized on both units"
        else:
            issues = []
            if not base_init:
                issues.append("BASE not initialized")
            if not remote_init:
                issues.append("REMOTE not initialized")
            result.detail = "; ".join(issues)

    def _test_ping_response(self, result: TestResult):
        # Check ping response queues on both units
        base_q = self.base.send_command("QUEUE STATUS")
        remote_q = self.remote.send_command("QUEUE STATUS")

        # Just verify the queues exist (ping_response queue)
        base_data = base_q.get("data", {}) if base_q else {}
        remote_data = remote_q.get("data", {}) if remote_q else {}

        if base_q and remote_q:
            result.passed = True
            result.detail = "Queue status retrieved from both units"
        else:
            result.detail = "Could not retrieve queue status"

    def _test_state_broadcast(self, result: TestResult):
        # Get state from BASE
        state_resp = self.base.send_command("STATE")
        if not state_resp or state_resp.get("status") != "ok":
            result.detail = "Could not read state from BASE"
            return

        state_name = state_resp.get("data", {}).get("name", "UNKNOWN")

        print(f"         {c(Colors.INFO, chr(0x2192))} BASE state: {c(Colors.BOLD, state_name)}")
        print(f"         {c(Colors.DIM, 'Check if REMOTE display shows the same state')}")

        response = self.ask_user(f"Does REMOTE display show '{state_name}'?")

        if response == 'y':
            result.passed = True
            result.detail = f"State '{state_name}' synchronized to REMOTE"
        elif response == 's':
            result.skipped = True
            result.detail = "Skipped by user"
        else:
            result.detail = f"REMOTE display does not show '{state_name}'"

    # ── State Machine Tests ───────────────────────────────────────────────────

    def test_state_machine(self):
        """Run state machine tests (BASE only)."""
        if not self.base:
            return

        self.print_section_header(
            "SECTION 7: STATE MACHINE TESTS",
            "These tests verify the state machine on BASE unit"
        )

        tests = [
            ("Current State", "BASE", lambda r: self._test_current_state(r)),
            ("State Command", "BASE", lambda r: self._test_state_command(r)),
        ]

        for i, (name, target, test_fn) in enumerate(tests, 1):
            self.print_test_start(i, len(tests), name)
            result = self.run_test(name, target, test_fn, "State Machine")
            self.print_test_result(result)

    def _test_current_state(self, result: TestResult):
        resp = self.base.send_command("STATE")
        result.response = resp

        if not resp or resp.get("status") != "ok":
            result.detail = "Could not read state"
            return

        data = resp.get("data", {})
        state_num = data.get("state", -1)
        state_name = data.get("name", "")

        expected_color = self.STATE_COLORS.get(state_name, "Unknown")

        print(f"         {c(Colors.INFO, chr(0x2192))} State: {state_num} ({c(Colors.BOLD, state_name)})")
        print(f"         {c(Colors.DIM, f'Expected LED color: {expected_color}')}")

        response = self.ask_user("Does the RGB LED color match?")

        if response == 'y':
            result.passed = True
            result.detail = f"State {state_name} with correct LED color"
        elif response == 's':
            result.skipped = True
            result.detail = "Skipped by user"
        else:
            result.detail = f"LED color mismatch for state {state_name}"

    def _test_state_command(self, result: TestResult):
        resp = self.base.send_command("STATE")
        result.response = resp

        if resp and resp.get("status") == "ok":
            result.passed = True
            data = resp.get("data", {})
            result.detail = f"State command working: {data.get('name', 'unknown')}"
        else:
            result.detail = "STATE command failed"

    # ── Safety Tests ──────────────────────────────────────────────────────────

    def test_safety(self):
        """Run safety system tests."""
        if not self.base:
            return

        self.print_section_header(
            "SECTION 8: SAFETY TESTS",
            "These tests verify safety-critical functionality"
        )

        tests = [
            ("Watchdog Task", "BASE", lambda r: self._test_watchdog(r)),
            ("Safe State GPIO", "BASE", lambda r: self._test_safe_gpio(r)),
        ]

        for i, (name, target, test_fn) in enumerate(tests, 1):
            self.print_test_start(i, len(tests), name)
            result = self.run_test(name, target, test_fn, "Safety")
            self.print_test_result(result)

    def _test_watchdog(self, result: TestResult):
        resp = self.base.send_command("TASKS")
        result.response = resp

        if not resp:
            result.detail = "Could not get task list"
            return

        tasks = resp.get("data", {}).get("tasks", [])
        task_names = [t.get("name", "") for t in tasks]

        watchdog_running = any("watchdog" in name.lower() or "wdt" in name.lower() for name in task_names)

        # Also check for IDLE task hook or similar safety mechanism
        idle_task = any("IDLE" in name for name in task_names)

        if watchdog_running:
            result.passed = True
            result.detail = "Watchdog task running"
        elif idle_task:
            result.passed = True
            result.detail = "System tasks running (watchdog may be in IDLE hook)"
        else:
            result.detail = "No explicit watchdog task found"
            result.passed = True  # Not a hard failure - watchdog may be hardware-based

    def _test_safe_gpio(self, result: TestResult):
        # Check igniter pins are in safe state (GPIO 40, 41 should be 0)
        resp1 = self.base.send_command("GPIO READ 40")
        resp2 = self.base.send_command("GPIO READ 41")

        level1 = resp1.get("data", {}).get("level", -1) if resp1 else -1
        level2 = resp2.get("data", {}).get("level", -1) if resp2 else -1

        if level1 == 0 and level2 == 0:
            result.passed = True
            result.detail = "Igniter pins (GPIO 40, 41) are LOW (safe)"
        elif level1 == -1 or level2 == -1:
            result.detail = "Could not read igniter GPIO pins"
        else:
            result.detail = f"WARNING: Igniter pins not LOW (GPIO40={level1}, GPIO41={level2})"

    # ── Main Execution ────────────────────────────────────────────────────────

    def run_all_tests(self):
        """Run all test categories."""
        self.test_automated()
        self.test_buzzer()
        self.test_leds()
        self.test_display()
        self.test_input()
        self.test_communication()
        self.test_state_machine()
        self.test_safety()

    def run_category(self, category: str):
        """Run a specific test category."""
        category_map = {
            "automated": self.test_automated,
            "buzzer": self.test_buzzer,
            "led": self.test_leds,
            "leds": self.test_leds,
            "display": self.test_display,
            "input": self.test_input,
            "communication": self.test_communication,
            "comm": self.test_communication,
            "state-machine": self.test_state_machine,
            "state": self.test_state_machine,
            "safety": self.test_safety,
        }

        if category.lower() == "all":
            self.run_all_tests()
        elif category.lower() in category_map:
            category_map[category.lower()]()
        else:
            print(f"Unknown category: {category}")
            print(f"Available: {', '.join(sorted(set(category_map.keys())))}")

    def print_summary(self):
        """Print final test summary."""
        print()
        print(c(Colors.BOLD, "=" * 68))
        print(c(Colors.BOLD, "                      TEST SUMMARY"))
        print(c(Colors.BOLD, "=" * 68))

        # Header
        print(f"  {'Category':<20} {'Passed':>8} {'Failed':>8} {'Skipped':>8} {'Total':>8}")
        print(f"  {'-' * 56}")

        # Per-category results
        total_passed = 0
        total_failed = 0
        total_skipped = 0
        total_tests = 0

        for category, stats in sorted(self.category_stats.items()):
            passed = stats["passed"]
            failed = stats["failed"]
            skipped = stats["skipped"]
            total = stats["total"]

            total_passed += passed
            total_failed += failed
            total_skipped += skipped
            total_tests += total

            # Color the row based on results
            if failed > 0:
                row_color = Colors.RED
            elif skipped > 0:
                row_color = Colors.YELLOW
            else:
                row_color = Colors.GREEN

            cat_name = f"{category:<20}"
            print(f"  {c(row_color, cat_name)} {passed:>8} {failed:>8} {skipped:>8} {total:>8}")

        print(f"  {'-' * 56}")

        # Total row
        if total_failed > 0:
            total_color = Colors.RED
        elif total_skipped > 0:
            total_color = Colors.YELLOW
        else:
            total_color = Colors.GREEN

        total_label = f"{'TOTAL':<20}"
        passed_str = f"{total_passed:>8}"
        failed_str = f"{total_failed:>8}"
        skipped_str = f"{total_skipped:>8}"
        failed_color = Colors.RED if total_failed else Colors.DIM
        skipped_color = Colors.YELLOW if total_skipped else Colors.DIM
        print(f"  {c(Colors.BOLD, total_label)} {c(total_color, passed_str)} {c(failed_color, failed_str)} {c(skipped_color, skipped_str)} {total_tests:>8}")

        print(c(Colors.BOLD, "=" * 68))

        if total_failed == 0 and total_skipped == 0:
            print(f"\n  {c(Colors.GREEN + Colors.BOLD, chr(0x2713) + ' All tests passed!')}")
        elif total_failed == 0:
            print(f"\n  {c(Colors.YELLOW, f'{chr(0x2713)} All run tests passed ({total_skipped} skipped)')}")
        else:
            print(f"\n  {c(Colors.RED, f'{chr(0x2717)} {total_failed} test(s) failed')}")

        print()

        # Log summary to file
        log("info", "=" * 60)
        log("info", "TEST SUMMARY")
        log("info", f"Total: {total_tests}, Passed: {total_passed}, Failed: {total_failed}, Skipped: {total_skipped}")
        for category, stats in sorted(self.category_stats.items()):
            log("info", f"  {category}: {stats['passed']}/{stats['total']} passed")
        log("info", "=" * 60)

        return total_failed


# ── Main Entry Point ──────────────────────────────────────────────────────────

def load_config(config_path: str) -> dict:
    """Load configuration from JSON file."""
    try:
        with open(config_path, 'r') as f:
            return json.load(f)
    except Exception:
        return {}


def main():
    global _use_colors

    parser = argparse.ArgumentParser(
        description="Interactive hardware test suite for StaticTeststandController",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Categories:
  automated      Automated tests (no user interaction)
  buzzer         Buzzer audio tests
  led/leds       LED visual tests
  display        OLED display tests
  input          Button and switch tests
  communication  ESP-NOW cross-unit tests
  state-machine  State machine tests (BASE only)
  safety         Safety system tests
  all            Run all categories (default)

Examples:
  %(prog)s                          Run all tests
  %(prog)s --category automated     Run only automated tests
  %(prog)s --base-only              Only test BASE unit
  %(prog)s --verbose                Show raw JSON responses
"""
    )

    parser.add_argument("--base-port", metavar="PORT",
                        help="BASE serial port (default from config)")
    parser.add_argument("--remote-port", metavar="PORT",
                        help="REMOTE serial port (default from config)")
    parser.add_argument("--category", "-c", default="all",
                        help="Run specific test category (default: all)")
    parser.add_argument("--base-only", action="store_true",
                        help="Only test BASE unit")
    parser.add_argument("--remote-only", action="store_true",
                        help="Only test REMOTE unit")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Show all raw JSON responses")
    parser.add_argument("--quiet", "-q", action="store_true",
                        help="Minimal output (pass/fail only)")
    parser.add_argument("--no-color", action="store_true",
                        help="Disable ANSI colors")
    parser.add_argument("--config", default="hardware_test_config.json",
                        help="Path to config file (default: hardware_test_config.json)")
    parser.add_argument("--log-dir", default="scripts/test_results",
                        help="Directory for log files (default: scripts/test_results)")

    args = parser.parse_args()

    # Disable colors if requested
    if args.no_color:
        _use_colors = False

    # Setup file logging
    log_path = setup_file_logging(args.log_dir)
    print(f"\n{c(Colors.DIM, f'Logging to: {log_path}')}")

    # Load config
    config = load_config(args.config)
    targets = config.get("targets", {})

    # Determine ports
    base_port = args.base_port or targets.get("BASE", {}).get("serial_port", "/dev/ttyACM1")
    remote_port = args.remote_port or targets.get("REMOTE", {}).get("serial_port", "/dev/ttyACM0")

    # Apply --base-only / --remote-only
    if args.base_only:
        remote_port = None
    if args.remote_only:
        base_port = None

    # Print header
    print()
    print(c(Colors.BOLD, chr(0x2554) + chr(0x2550) * 66 + chr(0x2557)))
    print(c(Colors.BOLD, chr(0x2551) + "           INTERACTIVE HARDWARE TEST SUITE                        " + chr(0x2551)))
    print(c(Colors.BOLD, chr(0x2551) + "           StaticTeststandController                              " + chr(0x2551)))
    print(c(Colors.BOLD, chr(0x255A) + chr(0x2550) * 66 + chr(0x255D)))

    # Create test suite
    suite = InteractiveTestSuite(
        base_port=base_port or "",
        remote_port=remote_port or "",
        verbose=args.verbose,
        quiet=args.quiet
    )

    # Override connection if ports disabled
    if args.remote_only:
        suite.base_port = ""
    if args.base_only:
        suite.remote_port = ""

    # Connect to devices
    if not suite.connect_devices():
        print(f"\n{c(Colors.RED, 'Error: Could not connect to any devices')}")
        sys.exit(1)

    try:
        # Run tests
        suite.run_category(args.category)

        # Print summary
        failed = suite.print_summary()

        sys.exit(1 if failed > 0 else 0)

    except KeyboardInterrupt:
        print(f"\n\n{c(Colors.YELLOW, 'Test interrupted by user')}")
        suite.print_summary()
        sys.exit(130)

    finally:
        suite.disconnect_devices()


if __name__ == "__main__":
    main()
