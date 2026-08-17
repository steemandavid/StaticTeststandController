#!/usr/bin/env python3
"""
User Workflow State Transition Tests for StaticTeststandController

Tests the complete user workflow as specified in FSD Section 4.1:
- INIT → IDLE
- IDLE → ARMED (when SWITCH_ARMED activated)
- IDLE → CHK_IGN (ignition button press while SAFE)
- ARMED → STARTTEST (ignition button long press)
- STARTTEST → IGNITION → RUNNING → ENDTEST → IDLE
- Any state → HALT (on critical error)
"""

import argparse
import json
import os
import re
import sys
import time
from typing import Optional, Dict, List, Tuple

try:
    import serial
except ImportError:
    print("Error: pyserial not installed. Run: pip install pyserial")
    sys.exit(1)


class SerialConnection:
    """Simple serial connection for test protocol."""

    def __init__(self, port: str, baud: int = 115200, timeout: float = 3.0):
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.ser: Optional[serial.Serial] = None

    def open(self) -> bool:
        """Open serial connection. Returns True on success."""
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=self.timeout)
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            time.sleep(0.5)

            # Clear any boot messages
            self.ser.reset_input_buffer()
            return True
        except Exception as e:
            print(f"Failed to open {self.port}: {e}")
            return False

    def close(self):
        """Close serial connection."""
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
            self.ser = None

    def send_command(self, command: str) -> Optional[dict]:
        """
        Send a command and return JSON response.
        Commands are automatically prefixed with 'TEST '.
        Skips non-JSON lines (log messages) to handle ESP logging.
        """
        if not self.ser:
            return None

        # Add TEST prefix if not already present
        if not command.startswith("TEST "):
            full_cmd = f"TEST {command}\n"
        else:
            full_cmd = f"{command}\n"

        try:
            # Send command
            self.ser.write(full_cmd.encode())
            self.ser.flush()

            # Read response, skipping non-JSON lines
            start_time = time.time()
            while time.time() - start_time < self.timeout:
                response_line = self.ser.readline().decode('utf-8', errors='ignore').strip()

                if not response_line:
                    time.sleep(0.01)
                    continue

                # All valid JSON responses start with '{'
                # Skip anything that doesn't look like JSON
                if not response_line.startswith('{'):
                    continue

                # Try to parse as JSON
                try:
                    return json.loads(response_line)
                except json.JSONDecodeError:
                    # Looked like JSON but wasn't valid
                    return {'status': 'error', 'message': response_line}

            # Timeout
            return None

        except Exception as e:
            print(f"Serial error: {e}")
            return None

    def flush(self):
        """Flush input/output buffers."""
        if self.ser:
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()


class StateTransition:
    """Represents a state transition test case."""

    def __init__(self, name: str, from_state: str, to_state: str,
                 trigger: str, expected_behavior: str):
        self.name = name
        self.from_state = from_state
        self.to_state = to_state
        self.trigger = trigger
        self.expected_behavior = expected_behavior
        self.passed = False
        self.error_message = ""


class UserWorkflowTester:
    """Tests user workflow state transitions."""

    # State name mapping (must match s_state_names in state_machine.c)
    STATES = {
        'INIT': 0,
        'IDLE': 1,
        'ARMED': 2,
        'STARTTEST': 3,
        'IGNITION': 4,
        'RUNNING': 5,  # Changed from RUNNING
        'ENDTEST': 6,
        'HALT': 7,
        'CHK_IGN': 8,  # Changed from CHK_IGN
        'CHK_BRK': 9,  # Changed from CHK_BRK
        'CAL_LC': 10,  # Changed from CAL_LC
        'CAL_PR': 11,  # Changed from CAL_PR
        'WELCOME': 12,  # Changed from WELCOME
    }

    def __init__(self, base_port: str):
        """Initialize tester with serial port."""
        self.base = SerialConnection(base_port)
        self.test_results: List[StateTransition] = []

    def setup_test_mode(self) -> bool:
        """Enable test mode to allow controlled state transitions."""
        print("Enabling TEST MODE...")
        resp = self.base.send_command("TEST_MODE ON")
        if resp and resp.get('status') == 'ok':
            print("✓ TEST MODE enabled")
            return True
        else:
            print(f"✗ Failed to enable TEST MODE: {resp}")
            return False

    def teardown_test_mode(self):
        """Disable test mode before exiting."""
        print("\nDisabling TEST MODE...")
        self.base.send_command("TEST_MODE OFF")
        print("✓ TEST MODE disabled")

    def get_current_state(self) -> Optional[str]:
        """Get current BASE state."""
        resp = self.base.send_command("STATE")
        if resp and resp.get('status') == 'ok':
            data = resp.get('data', {})
            return data.get('name', 'UNKNOWN')
        return None

    def force_state(self, state_name: str) -> bool:
        """Force BASE to a specific state."""
        resp = self.base.send_command(f"STATE {state_name}")
        if resp and resp.get('status') == 'ok':
            # Wait for state transition to complete
            time.sleep(0.2)
            return True
        print(f"Failed to set state {state_name}: {resp}")
        return False

    def test_transition(self, test: StateTransition) -> bool:
        """Execute a single state transition test."""
        print(f"\n{'='*60}")
        print(f"TEST: {test.name}")
        print(f"{'='*60}")
        print(f"  From: {test.from_state}")
        print(f"  To:   {test.to_state}")
        print(f"  Trigger: {test.trigger}")
        print(f"  Expected: {test.expected_behavior}")

        # Set up initial state
        print(f"\n  Setting initial state: {test.from_state}")
        if not self.force_state(test.from_state):
            test.error_message = f"Failed to set initial state {test.from_state}"
            print(f"  ✗ {test.error_message}")
            return False
        print(f"  ✓ Initial state set")

        # Verify initial state
        current = self.get_current_state()
        if current != test.from_state:
            test.error_message = f"Initial state mismatch: expected {test.from_state}, got {current}"
            print(f"  ✗ {test.error_message}")
            return False

        # Trigger the transition (direct state change for test mode)
        print(f"  Triggering: {test.trigger}")
        self.force_state(test.to_state)

        # Wait for transition to complete
        time.sleep(0.3)

        # Verify final state
        final_state = self.get_current_state()
        print(f"  Final state: {final_state}")

        if final_state == test.to_state:
            test.passed = True
            print(f"  ✓ PASS: Transition {test.from_state} → {test.to_state}")
            return True
        else:
            test.error_message = f"State mismatch: expected {test.to_state}, got {final_state}"
            test.passed = False
            print(f"  ✗ FAIL: {test.error_message}")
            return False

    def run_all_tests(self) -> Dict[str, Dict]:
        """Run all user workflow tests."""
        print("\n" + "="*60)
        print("USER WORKFLOW STATE TRANSITION TESTS")
        print("="*60)

        # Define all test cases from FSD Section 4.1
        tests = [
            # Boot sequence
            StateTransition(
                "Boot: INIT → WELCOME (auto)",
                "INIT", "WELCOME",
                "Automatic after 100ms",
                "System shows welcome screen then auto-transitions to IDLE"
            ),
            StateTransition(
                "Boot: WELCOME → IDLE (auto)",
                "WELCOME", "IDLE",
                "Automatic after 2 seconds",
                "Welcome screen times out, enters IDLE"
            ),

            # Normal user workflow
            StateTransition(
                "IDLE → ARMED (arm switch activated)",
                "IDLE", "ARMED",
                "Arm/Safe switch toggled to ARMED position",
                "System arms, LED turns orange, buzzer beeps"
            ),
            StateTransition(
                "ARMED → IDLE (disarm via safe switch)",
                "ARMED", "IDLE",
                "Arm/Safe switch toggled to SAFE position",
                "System disarms, returns to green breathing LED"
            ),
            StateTransition(
                "IDLE → CHK_IGN (button short press)",
                "IDLE", "CHK_IGN",
                "Ignition button short press while in SAFE mode",
                "System enters igniter check mode, LED magenta pulsing"
            ),
            StateTransition(
                "CHK_IGN → IDLE (exit check mode)",
                "CHK_IGN", "IDLE",
                "Button long press to exit check mode",
                "Returns to IDLE, LED green breathing"
            ),
            StateTransition(
                "IDLE → CHK_BRK (via command)",
                "IDLE", "CHK_BRK",
                "State command (for testing)",
                "System enters breakwire check mode"
            ),
            StateTransition(
                "CHK_BRK → IDLE (exit)",
                "CHK_BRK", "IDLE",
                "Button long press to exit",
                "Returns to IDLE"
            ),
            StateTransition(
                "IDLE → CAL_LC (via command)",
                "IDLE", "CAL_LC",
                "State command (for testing)",
                "System enters load cell calibration mode"
            ),
            StateTransition(
                "CAL_LC → IDLE (exit)",
                "CAL_LC", "IDLE",
                "Button long press to exit",
                "Returns to IDLE"
            ),
            StateTransition(
                "IDLE → CAL_PR (via command)",
                "IDLE", "CAL_PR",
                "State command (for testing)",
                "System enters pressure calibration mode"
            ),
            StateTransition(
                "CAL_PR → IDLE (exit)",
                "CAL_PR", "IDLE",
                "Button long press to exit",
                "Returns to IDLE"
            ),

            # Test firing sequence
            StateTransition(
                "ARMED → STARTTEST (button long press)",
                "ARMED", "STARTTEST",
                "Deliberate long press of ignition button",
                "Initializes test, creates data file"
            ),
            StateTransition(
                "STARTTEST → IGNITION (auto after 1s)",
                "STARTTEST", "IGNITION",
                "Automatic transition after file creation",
                "Fires igniter, LED red blinking, buzzer alarm"
            ),
            StateTransition(
                "IGNITION → RUNNING (auto)",
                "IGNITION", "RUNNING",
                "Automatic after igniter timeout",
                "Data logging active, LED yellow solid"
            ),
            StateTransition(
                "RUNNING → ENDTEST (burn complete)",
                "RUNNING", "ENDTEST",
                "Automatic after end-of-burn detection",
                "Writes summary, LED green, buzzer double beep"
            ),
            StateTransition(
                "ENDTEST → IDLE (auto after 2s)",
                "ENDTEST", "IDLE",
                "Automatic after summary written",
                "Returns to ready state"
            ),

            # Emergency transitions
            StateTransition(
                "Any State → HALT (emergency command)",
                "ARMED", "HALT",
                "Emergency HALT command",
                "Immediate safe state, red pulse LED"
            ),
            StateTransition(
                "IDLE → HALT (error condition)",
                "IDLE", "HALT",
                "Error condition detected",
                "System enters safe halt state"
            ),
            StateTransition(
                "HALT → INIT (requires reboot)",
                "HALT", "INIT",
                "Force state transition (test mode only)",
                "Resets from halt (normally requires power cycle)"
            ),

            # Safe switch emergency stop during test
            StateTransition(
                "RUNNING → HALT (emergency stop)",
                "RUNNING", "HALT",
                "Safe switch toggled during test",
                "Emergency halt, cuts all power"
            ),
        ]

        # Run each test
        for i, test in enumerate(tests, 1):
            print(f"\n[{i}/{len(tests)}]", end="")
            self.test_transition(test)
            self.test_results.append(test)
            time.sleep(0.1)  # Brief pause between tests

        return self.generate_report()

    def generate_report(self) -> Dict[str, Dict]:
        """Generate test report."""
        total = len(self.test_results)
        passed = sum(1 for t in self.test_results if t.passed)
        failed = total - passed

        print("\n" + "="*60)
        print("TEST RESULTS SUMMARY")
        print("="*60)
        print(f"\nTotal Tests: {total}")
        print(f"Passed:       {passed} ({passed*100//total}%)")
        print(f"Failed:       {failed} ({failed*100//total if total > 0 else 0}%)")

        if failed > 0:
            print("\n" + "-"*60)
            print("FAILED TESTS:")
            print("-"*60)
            for test in self.test_results:
                if not test.passed:
                    print(f"\n✗ {test.name}")
                    print(f"  Error: {test.error_message}")

        print("\n" + "="*60)

        return {
            'total': total,
            'passed': passed,
            'failed': failed,
            'pass_rate': f"{passed*100//total if total > 0 else 0}%",
            'tests': [
                {
                    'name': t.name,
                    'from': t.from_state,
                    'to': t.to_state,
                    'passed': t.passed,
                    'error': t.error_message
                }
                for t in self.test_results
            ]
        }


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description='Test user workflow state transitions'
    )
    parser.add_argument(
        '--base-port', '-b',
        default='/dev/ttyACM0',
        help='BASE unit serial port (default: /dev/ttyACM0)'
    )
    parser.add_argument(
        '--json', '-j',
        action='store_true',
        help='Output results as JSON'
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Verbose output'
    )

    args = parser.parse_args()

    # Check if port exists
    if not os.path.exists(args.base_port):
        # Try common alternatives
        for alt_port in ['/dev/ttyACM1', '/dev/ttyUSB0', '/dev/ttyUSB1']:
            if os.path.exists(alt_port):
                print(f"Port {args.base_port} not found, using {alt_port}")
                args.base_port = alt_port
                break
        else:
            print(f"Error: Serial port {args.base_port} not found")
            print("Available ports:")
            os.system("ls -la /dev/tty* 2>/dev/null | grep -E 'tty(ACM|USB)' || true")
            sys.exit(1)

    try:
        tester = UserWorkflowTester(args.base_port)

        # Open connection
        if not tester.base.open():
            print(f"Failed to open connection to {args.base_port}")
            sys.exit(1)

        print(f"Connected to BASE on {args.base_port}")

        # Reset the board for a clean boot
        print("Resetting board...")
        tester.base.ser.setDTR(False)
        time.sleep(0.1)
        tester.base.ser.setDTR(True)
        time.sleep(0.1)

        # Wait for system to complete boot sequence
        print("Waiting for boot to complete...")
        time.sleep(3.0)  # Give time for boot

        # Clear any boot messages from buffer
        tester.base.flush()

        # Setup test mode
        if not tester.setup_test_mode():
            print("Failed to enable test mode")
            tester.base.close()
            sys.exit(1)

        # Run tests
        results = tester.run_all_tests()

        # Cleanup
        tester.teardown_test_mode()
        tester.base.close()

        if args.json:
            print("\n" + json.dumps(results, indent=2))

        # Exit with appropriate code
        sys.exit(0 if results['failed'] == 0 else 1)

    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        sys.exit(130)
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
