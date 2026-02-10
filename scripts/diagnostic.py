#!/usr/bin/env python3
"""
Diagnostic script to check ESP32-S3 unit health

Outputs both console results and a JSON results file for analysis.
"""
import sys
import serial
import time
import json
import os
from datetime import datetime

def test_port(port, name, results_data):
    """Test a single port and return success status."""
    timestamp_start = datetime.now().isoformat()
    print(f"\n{'='*60}")
    print(f"Testing {name} on {port}")
    print('='*60)

    port_result = {
        "name": name,
        "port": port,
        "start_time": timestamp_start,
        "tests": []
    }

    try:
        ser = serial.Serial(port, 115200, timeout=3)
        print(f"✓ Serial port opened")

        # Clear any pending data
        time.sleep(0.5)
        if ser.in_waiting:
            data = ser.read(ser.in_waiting)
            print(f"  Cleared {len(data)} bytes from buffer")

        # Test 1: PING
        print(f"\n[TEST 1] Sending PING...")
        test_start = time.time()
        ser.write(b"TEST PING\n")
        ser.flush()
        response = b""
        while time.time() - test_start < 2:
            if ser.in_waiting:
                response += ser.read(ser.in_waiting)
                if b"\n" in response:
                    break
        duration_ms = int((time.time() - test_start) * 1000)

        ping_passed = response and b"pong" in response
        ping_result = {
            "test": "PING",
            "timestamp": datetime.now().isoformat(),
            "duration_ms": duration_ms,
            "passed": ping_passed,
            "response": response.decode('utf-8', errors='replace').strip() if response else None
        }
        port_result["tests"].append(ping_result)

        if ping_passed:
            print(f"  ✓ PING successful")
        else:
            print(f"  ✗ PING failed - unit may be crashed")
            print(f"  Recommendation: Power cycle the unit")
            port_result["success"] = False
            port_result["end_time"] = datetime.now().isoformat()
            ser.close()
            results_data["ports"].append(port_result)
            return False

        # Test 2: INFO
        print(f"\n[TEST 2] Sending INFO...")
        test_start = time.time()
        ser.write(b"TEST INFO\n")
        ser.flush()
        response = b""
        while time.time() - test_start < 2:
            if ser.in_waiting:
                response += ser.read(ser.in_waiting)
                if b"}" in response:
                    break
        duration_ms = int((time.time() - test_start) * 1000)

        info_passed = response and b"target" in response
        info_result = {
            "test": "INFO",
            "timestamp": datetime.now().isoformat(),
            "duration_ms": duration_ms,
            "passed": info_passed,
            "response": response.decode('utf-8', errors='replace').strip() if response else None
        }

        if info_passed and b'"target":"BASE"' in response:
            print(f"  ✓ INFO successful")
            print(f"    Firmware: BASE")
            info_result["firmware"] = "BASE"
        elif info_passed and b'"target":"REMOTE"' in response:
            print(f"  ✓ INFO successful")
            print(f"    Firmware: REMOTE")
            info_result["firmware"] = "REMOTE"
        else:
            print(f"  ✗ INFO failed")

        port_result["tests"].append(info_result)

        # Test 3: TASKS
        print(f"\n[TEST 3] Sending TASKS...")
        test_start = time.time()
        ser.write(b"TEST TASKS\n")
        ser.flush()
        response = b""
        while time.time() - test_start < 2:
            if ser.in_waiting:
                response += ser.read(ser.in_waiting)
                if b"]}}" in response:
                    break
        duration_ms = int((time.time() - test_start) * 1000)

        tasks_passed = response and b"count" in response
        tasks_result = {
            "test": "TASKS",
            "timestamp": datetime.now().isoformat(),
            "duration_ms": duration_ms,
            "passed": tasks_passed,
            "response": response.decode('utf-8', errors='replace').strip() if response else None
        }

        if tasks_passed:
            print(f"  ✓ TASKS successful")
            try:
                data = json.loads(response.decode('utf-8', errors='replace').split('data:')[1].rstrip('}'))
                task_count = data.get('count', '?')
                print(f"    Running tasks: {task_count}")
                tasks_result["task_count"] = task_count
            except:
                pass
        else:
            print(f"  ✗ TASKS failed")

        port_result["tests"].append(tasks_result)

        ser.close()
        port_result["success"] = True
        port_result["end_time"] = datetime.now().isoformat()
        results_data["ports"].append(port_result)

        print(f"\n✓ {name} appears to be functioning normally")
        return True

    except serial.SerialException as e:
        print(f"  ✗ Serial error: {e}")
        print(f"  Recommendation: Check USB connection and port permissions")
        port_result["success"] = False
        port_result["error"] = str(e)
        port_result["end_time"] = datetime.now().isoformat()
        results_data["ports"].append(port_result)
        return False
    except Exception as e:
        print(f"  ✗ Error: {e}")
        port_result["success"] = False
        port_result["error"] = str(e)
        port_result["end_time"] = datetime.now().isoformat()
        results_data["ports"].append(port_result)
        return False

def main():
    start_time = datetime.now().isoformat()
    print("="*60)
    print(" ESP32-S3 Diagnostic Tool")
    print("="*60)

    # Create results directory
    results_dir = "scripts/test_results"
    os.makedirs(results_dir, exist_ok=True)

    # Initialize results structure
    results_data = {
        "type": "diagnostic_results",
        "start_time": start_time,
        "ports": []
    }

    ports_to_test = []

    # Check which ports exist
    for port in ["/dev/ttyACM0", "/dev/ttyACM1"]:
        if os.path.exists(port):
            name = "BASE" if "ACM0" in port else "REMOTE"
            ports_to_test.append((port, name))

    if not ports_to_test:
        print("✗ No serial ports found (ttyACM0, ttyACM1)")
        print("  Check USB connections and permissions")

        # Save results even if no ports found
        results_data["end_time"] = datetime.now().isoformat()
        results_data["error"] = "No serial ports found"

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        json_path = os.path.join(results_dir, f"diagnostic_{timestamp}.json")
        with open(json_path, 'w') as f:
            json.dump(results_data, f, indent=2)
        print(f"\nResults saved to: {json_path}")

        sys.exit(1)

    # Run tests
    results = {}
    for port, name in ports_to_test:
        results[name] = test_port(port, name, results_data)

    # Finalize results
    end_time = datetime.now().isoformat()
    results_data["end_time"] = end_time
    results_data["summary"] = {
        "total_ports": len(ports_to_test),
        "passed": sum(1 for r in results.values() if r),
        "failed": sum(1 for r in results.values() if not r)
    }

    # Save JSON results
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    json_path = os.path.join(results_dir, f"diagnostic_{timestamp}.json")
    with open(json_path, 'w') as f:
        json.dump(results_data, f, indent=2)

    # Print summary
    print(f"\n{'='*60}")
    print(" SUMMARY")
    print('='*60)

    for name, result in results.items():
        status = "✓ OK" if result else "✗ FAILED"
        print(f"  {name}: {status}")

    print(f"\nResults saved to: {json_path}")

    if all(results.values()):
        print(f"\n✓ All units passed diagnostic tests")
        sys.exit(0)
    else:
        print(f"\n✗ Some units failed diagnostic tests")
        print(f"\nTroubleshooting steps:")
        print(f"  1. Power cycle failed units (unplug USB, wait 5s, replug)")
        print(f"  2. Check if units are crashed (no response to PING)")
        print(f"  3. Re-flash firmware using: ./build_and_flash_all.sh")
        print(f"  4. If units still crash, check serial monitor for crash logs")
        sys.exit(1)

if __name__ == "__main__":
    main()
