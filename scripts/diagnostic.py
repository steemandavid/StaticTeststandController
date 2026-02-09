#!/usr/bin/env python3
"""
Diagnostic script to check ESP32-S3 unit health
"""
import sys
import serial
import time

def test_port(port, name):
    print(f"\n{'='*60}")
    print(f"Testing {name} on {port}")
    print('='*60)

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
        ser.write(b"TEST PING\n")
        ser.flush()
        start = time.time()
        response = b""
        while time.time() - start < 2:
            if ser.in_waiting:
                response += ser.read(ser.in_waiting)
                if b"\n" in response:
                    break
        if response and b"pong" in response:
            print(f"  ✓ PING successful")
        else:
            print(f"  ✗ PING failed - unit may be crashed")
            print(f"  Recommendation: Power cycle the unit")
            ser.close()
            return False

        # Test 2: INFO
        print(f"\n[TEST 2] Sending INFO...")
        ser.write(b"TEST INFO\n")
        ser.flush()
        response = b""
        start = time.time()
        while time.time() - start < 2:
            if ser.in_waiting:
                response += ser.read(ser.in_waiting)
                if b"}" in response:
                    break
        if response and b"target" in response:
            print(f"  ✓ INFO successful")
            if b'"target":"BASE"' in response:
                print(f"    Firmware: BASE")
            elif b'"target":"REMOTE"' in response:
                print(f"    Firmware: REMOTE")
        else:
            print(f"  ✗ INFO failed")

        # Test 3: TASKS
        print(f"\n[TEST 3] Sending TASKS...")
        ser.write(b"TEST TASKS\n")
        ser.flush()
        response = b""
        start = time.time()
        while time.time() - start < 2:
            if ser.in_waiting:
                response += ser.read(ser.in_waiting)
                if b"]}}" in response:
                    break
        if response and b"count" in response:
            print(f"  ✓ TASKS successful")
            try:
                import json
                data = json.loads(response.decode('utf-8', errors='replace').split('data:')[1].rstrip('}'))
                print(f"    Running tasks: {data.get('count', '?')}")
            except:
                pass
        else:
            print(f"  ✗ TASKS failed")

        ser.close()
        print(f"\n✓ {name} appears to be functioning normally")
        return True

    except serial.SerialException as e:
        print(f"  ✗ Serial error: {e}")
        print(f"  Recommendation: Check USB connection and port permissions")
        return False
    except Exception as e:
        print(f"  ✗ Error: {e}")
        return False

if __name__ == "__main__":
    import json

    print("="*60)
    print(" ESP32-S3 Diagnostic Tool")
    print("="*60)

    ports_to_test = []

    # Check which ports exist
    import os
    for port in ["/dev/ttyACM0", "/dev/ttyACM1"]:
        if os.path.exists(port):
            name = "BASE" if "ACM0" in port else "REMOTE"
            ports_to_test.append((port, name))

    if not ports_to_test:
        print("✗ No serial ports found (ttyACM0, ttyACM1)")
        print("  Check USB connections and permissions")
        sys.exit(1)

    results = {}
    for port, name in ports_to_test:
        results[name] = test_port(port, name)

    print(f"\n{'='*60}")
    print(" SUMMARY")
    print('='*60)

    for name, result in results.items():
        status = "✓ OK" if result else "✗ FAILED"
        print(f"  {name}: {status}")

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
