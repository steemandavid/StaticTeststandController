# Manual Testing Plan
## StaticTeststandController v1.1.0

**Purpose:** Comprehensive manual verification of Phase 1 MVP features
**Duration:** Approximately 45-60 minutes
**Prerequisites:** Both units powered and connected via USB

---

## Test Preparation

### Equipment Needed
- [ ] BASE unit (ESP32-S3)
- [ ] REMOTE unit (ESP32-S3)
- [ ] USB cables for both units
- [ ] Computer with Python 3.7+
- [ ] OLED display visible on REMOTE
- [ ] Access to RGB LEDs on both units
- [ ] Audible buzzer on both units
- [ ] Arm/Safe switch accessible
- [ ] Ignition button accessible

### Pre-Test Checklist
1. [ ] Flash both units with correct firmware (use `scripts/build_and_flash_all.sh`)
2. [ ] Verify firmware with `scripts/diagnostic.py`
3. [ ] Power cycle both units
4. [ ] Confirm 3 beeps + LED flash on boot (both units)
5. [ ] Confirm green breathing LED on both units after boot

---

## Test Sections

### Section 1: Automated Infrastructure Tests ✅ ALREADY PASSED

**Status:** These tests run automatically and have all passed (12/12)

| Test | Result | Notes |
|------|--------|-------|
| PING BASE | ✅ PASS | 14ms response |
| INFO BASE | ✅ PASS | v1.1.3, BASE target |
| HEAP BASE | ✅ PASS | 8.5MB free |
| TASKS BASE | ✅ PASS | 17 tasks running |
| QUEUE STATUS BASE | ✅ PASS | All healthy |
| ESPNOW STATUS BASE | ✅ PASS | Initialized |
| PING REMOTE | ✅ PASS | 7ms response |
| INFO REMOTE | ✅ PASS | v1.1.3, REMOTE target |
| HEAP REMOTE | ✅ PASS | 8.5MB free |
| TASKS REMOTE | ✅ PASS | 19 tasks running |
| QUEUE STATUS REMOTE | ✅ PASS | All healthy |
| ESPNOW STATUS REMOTE | ✅ PASS | Initialized |

**No manual testing required - these are fully automated.**

---

### Section 2: Buzzer Tests (Manual Verification)

**Purpose:** Verify buzzer PWM tone generation on both units

#### Test 2.1: BASE Buzzer
1. Run automated test: `python3 scripts/interactive_test.py --base-port /dev/ttyACM0 --remote-port /dev/ttyACM1`
2. When prompted "Can you hear the BASE buzzer beeping?", verify:
   - [ ] Buzzer is making a continuous tone (not just clicking)
   - [ ] Tone is clearly audible (4000Hz PWM)
   - [ ] Respond 'y' if heard, 'n' if not
3. Verify buzzer turns OFF after responding:
   - [ ] Buzzer completely silent after test

**Expected Result:** Continuous beeping at 4000Hz, then complete silence

#### Test 2.2: REMOTE Buzzer
1. Same procedure as BASE
2. Verify:
   - [ ] Continuous tone audible
   - [ ] Turns off completely after test

**Expected Result:** Same as BASE

---

### Section 3: RGB LED Tests (Visual Verification)

**Purpose:** Verify WS2812 RGB LED functionality and state-based colors

#### Test 3.1: BASE RGB LED Colors
1. Test will cycle through colors: RED → GREEN → BLUE → STATE
2. The test will ask you to verify each color
3. Verify each color:
   - [ ] **RED:** Bright, pure red (no green/blue tint)
   - [ ] **GREEN:** Bright, pure green (known issue: green channel may be dim)
   - [ ] **BLUE:** Bright, pure blue
   - [ ] **STATE:** Returns to current state color (red pulse for HALT)

**Expected Result:** All colors work correctly, LED returns to state color

#### Test 3.2: REMOTE RGB LED Colors
1. Same test sequence as BASE
2. Verify same color sequence
   - [ ] RED, GREEN, BLUE all working
   - [ ] Returns to green breathing (IDLE state)

#### Test 3.3: REMOTE Button LED Test

**What to expect:**
- The test will FIRST turn the button LED OFF
- Wait 1 second (LED will be dark)
- THEN turn the button LED ON
- Ask if you saw the OFF → ON transition

**Why this works:**
- The button LED is normally always ON
- By turning it OFF first, the ON transition becomes clearly visible
- Watch for the LED to go dark, then light up again

**Procedure:**
1. Watch the button LED on the REMOTE unit
2. You will see it turn OFF (go dark)
3. A moment later, it will turn ON (light up)
4. Confirm you saw this transition

**Verify:**
   - [ ] LED turns OFF first (visible darkness)
   - [ ] LED turns ON again (clear OFF→ON transition)
   - [ ] Test asks: "Did the button LED turn ON just now?"

**Expected Result:** You clearly see the OFF → ON blink sequence

#### Test 3.4: State Color Mapping (Advanced Test)

This test requires manually changing states and verifying LED colors:

**BASE State LED Colors:**
| State | Color | Pattern | Verify |
|-------|-------|---------|--------|
| IDLE | Green | Breathing | [ ] |
| ARMED | Orange | Solid | [ ] |
| HALT | Red | Pulse 0.5Hz | [ ] |
| WELCOME_SCREEN | Blue | Solid | [ ] |

**How to test:**
1. Send state commands via test script:
   ```
   python3 -c "from scripts.interactive_test import *; conn = SerialConnection('/dev/ttyACM0'); conn.send_command('STATE IDLE')"
   ```
2. Verify LED color matches expected pattern
3. Repeat for each state

---

### Section 4: Display Tests (Visual Verification)

**Purpose:** Verify SSD1306 OLED display functionality

#### Test 4.1: Display Text Display
1. Test will display 5 lines of text
2. Verify on REMOTE OLED:
   - [ ] Status line shows BASE state (top line)
   - [ ] 5 log lines visible and readable
   - [ ] Text scrolls properly (newest at top)
   - [ ] Characters are clear, not garbled
   - [ ] No pixel artifacts or dead rows

**Expected Result:** Clear 128×64 display with 6 lines (1 status + 5 log)

#### Test 4.2: Display State Updates (Advanced)

**How to test:**
1. Put BASE in different states (see Test 3.3)
2. Verify REMOTE display updates:
   - [ ] State name changes in status line
   - [ ] Log lines scroll when new events occur
   - [ ] RSSI indicator visible (if implemented)

---

### Section 5: Input Tests (Interactive)

**Purpose:** Verify button and switch detection on REMOTE

#### Test 5.1: Button Short Press

**What to expect:**
- The test will display clear instructions explaining what to do
- Press Enter to START the monitoring phase
- Then press and release the button quickly (< 1 second)
- The test automatically detects the button press

**Procedure:**
1. Read the instructions displayed on screen
2. Press Enter when ready to begin monitoring
3. When you see "MONITORING ACTIVE - Press the button NOW!"
   - Press and QUICKLY release the ignition button
4. The test will confirm: "BUTTON PRESS DETECTED!"

**Verify:**
   - [ ] Test displays clear instructions
   - [ ] Press Enter starts monitoring phase
   - [ ] Button press is automatically detected
   - [ ] Test shows "BUTTON PRESS DETECTED!" message

**Expected Result:** PASS with "Button press registered (queue activity detected)"

#### Test 5.2: Button Long Press

**What to expect:**
- The test will display clear instructions for long press
- Press Enter to START the monitoring phase
- Then press and HOLD the button for 3+ seconds
- Watch for LED or display feedback
- The test detects the button press and waits for feedback

**Procedure:**
1. Read the instructions displayed on screen
2. Press Enter when ready to begin monitoring
3. When you see "MONITORING ACTIVE - Hold the button NOW!"
   - Press and HOLD the ignition button
   - Hold for 3+ seconds
   - Watch for LED or display changes
   - Release when you see feedback
4. Answer the question: "Did you see a state change or LED/display feedback?"

**Verify:**
   - [ ] Test displays clear instructions
   - [ ] Press Enter starts monitoring phase
   - [ ] Button press is automatically detected
   - [ ] Long press triggers visible feedback (LED/display)

**Expected Result:** Long press triggers state change or command (feedback visible)

#### Test 5.3: Arm/Safe Switch

**What to expect:**
- The test displays current state
- Press Enter to START monitoring
- Toggle the switch
- Test detects the change

**Procedure:**
1. Note current state displayed on screen
2. Press Enter to begin monitoring
3. Toggle the Arm/Safe switch
4. The test will verify the state change

**Verify:**
   - [ ] Switch position detected by REMOTE
   - [ ] State changes from SAFE to ARMED or vice versa
   - [ ] LED color changes appropriately

**Expected Result:** Switch position correctly detected

---

### Section 6: Communication Tests

**Purpose:** Verify ESP-NOW bidirectional communication

#### Test 6.1: ESP-NOW Link
**Status:** ✅ PASS (automated)

#### Test 6.2: Ping Response
**Status:** ✅ PASS (automated)

#### Test 6.3: State Broadcast (Manual Verification)
1. Put BASE in different states (use STATE command)
2. Verify on REMOTE:
   - [ ] OLED display shows BASE state correctly
   - [ ] RGB LED color matches BASE state
   - [ ] Delay is minimal (< 1 second)

**Expected Result:** REMOTE shows BASE state in real-time

#### Test 6.4: Communication Range (Optional Advanced Test)

**Purpose:** Test wireless range and reliability

**How to test:**
1. Place both units 1 meter apart
2. Run diagnostic: `python3 scripts/diagnostic.py --base /dev/ttyACM0 --remote /dev/ttyACM1`
3. Verify PING success
4. Move units apart incrementally: 5m, 10m, 15m, 20m
5. At each distance:
   - [ ] Run diagnostic
   - [ ] Check RSSI value (should be > -85 dBm)
   - [ ] Verify all PINGs succeed

**Expected Result:** Reliable communication up to 20m+

---

### Section 7: State Machine Tests

**Purpose:** Verify BASE state machine functionality

#### Test 7.1: Current State
**Status:** ✅ PASS (automated)

#### Test 7.2: State Command
**Status:** ✅ PASS (automated)

#### Test 7.3: State Transitions (Manual Walkthrough)

**Purpose:** Walk through all 13 states manually

**How to test:**
1. Use serial console to send state commands:
   ```bash
   # Connect to BASE
   screen /dev/ttyACM0 115200

   # Send commands
   STATE IDLE
   STATE ARMED
   STATE HALT
   # etc.
   ```

2. For each state, verify:
   - [ ] RGB LED color matches expected (see Test 3.3)
   - [ ] State is accessible via INFO command
   - [ ] State broadcasts to REMOTE
   - [ ] OLED display updates

3. Test state transitions (FSD §4.1):
   ```
   INIT → IDLE ✅ Automatic on boot
   IDLE → ARMED ✅ Via switch or command
   ARMED → HALT ✅ Via emergency command
   Any → HALT ✅ Via HALT command
   ```

**Expected Result:** All states accessible, correct LED/display feedback

---

### Section 8: Safety Tests

**Purpose:** Verify safety-critical systems

#### Test 8.1: Watchdog Task
**Status:** ✅ PASS (automated)

**What it tests:**
- Watchdog task is running at highest priority
- Monitors system health
- Can trigger safe state if needed

#### Test 8.2: Safe State GPIO (Known Issue)

**Status:** ⚠️ Test query fails (infrastructure issue)

**Manual Verification:**
1. Power off BASE
2. Use multimeter to check GPIO pins:
   - [ ] GPIO 4 (LOW_SIDE_POWER) reads LOW (0V)
   - [ ] GPIO 41 (IGNITION) reads LOW (0V)
3. Power on BASE
4. Check again:
   - [ ] Both pins still LOW (safe state)

**Expected Result:** Igniter pins LOW when system idle/safe

#### Test 8.3: Emergency Halt Test

**Purpose:** Verify emergency halt functionality

**How to test:**
1. Put BASE in ARMED state
2. Send HALT command:
   ```
   HALT
   ```
3. Verify:
   - [ ] State immediately changes to HALT
   - [ ] RGB LED turns red pulse
   - [ ] OLED displays HALT state
   - [ ] Cannot exit HALT without power cycle

**Expected Result:** Immediate transition to HALT, requires power cycle

---

### Section 9: Boot Sequence Tests

**Purpose:** Verify proper initialization

#### Test 9.1: Normal Boot Sequence
1. Power off both units
2. Power on BASE
3. Verify:
   - [ ] 3 beeps heard (150ms ON, 100ms OFF × 3)
   - [ ] LED flashes green → yellow → blue (200ms each)
   - [ ] LED settles to green breathing
   - [ ] Unit responsive to PING within 5 seconds

4. Power on REMOTE
5. Verify same sequence

**Expected Result:** Consistent boot notification on both units

#### Test 9.2: Boot Without Remote

**Purpose:** Test BASE standalone operation

1. Power on BASE only (REMOTE off)
2. Verify:
   - [ ] BASE boots normally
   - [ ] Enters IDLE state
   - [ ] Complains about missing REMOTE (log warning)
   - [ ] Continues operating

**Expected Result:** BASE works independently

#### Test 9.3: Boot Without Base

**Purpose:** Test REMOTE standalone operation

1. Power on REMOTE only (BASE off)
2. Verify:
   - [ ] REMOTE boots normally
   - [ ] Shows "disconnected" or error state
   - [ ] Continues operating

**Expected Result:** REMOTE works independently

---

### Section 10: Stress Tests (Optional)

**Purpose:** Test system stability under load

#### Test 10.1: Rapid State Changes
1. Send rapid state commands:
   ```
   STATE IDLE
   STATE ARMED
   STATE HALT
   STATE IDLE
   STATE ARMED
   # Repeat 20 times
   ```
2. Verify:
   - [ ] No crashes or reboots
   - [ ] All commands executed
   - [ ] LED updates correctly each time

**Expected Result:** Stable under rapid changes

#### Test 10.2: Communication Stress
1. Leave units running for 1 hour
2. Monitor with diagnostic every 10 minutes
3. Verify:
   - [ ] No memory leaks (heap stable)
   - [ ] No communication drops
   - [ ] RSSI stable

**Expected Result:** Stable long-term operation

#### Test 10.3: Button Spam Test
1. Press ignition button 50 times rapidly
2. Verify:
   - [ ] No crashes
   - [ ] All presses registered or debounced correctly

**Expected Result:** Handles rapid input

---

## Test Results Template

Copy this template to record your test results:

```
Manual Test Results - StaticTeststandController v1.1.0
Date: _______________
Tester: _______________

Section 2: Buzzer Tests
  [ ] BASE Buzzer - PASS / FAIL
  [ ] REMOTE Buzzer - PASS / FAIL

Section 3: RGB LED Tests
  [ ] BASE RGB LED Colors - PASS / FAIL
  [ ] REMOTE RGB LED Colors - PASS / FAIL
  [ ] REMOTE Button LED (OFF->ON blink) - PASS / FAIL
  [ ] State Color Mapping - PASS / FAIL / SKIPPED

Section 4: Display Tests
  [ ] Display Text Display - PASS / FAIL
  [ ] Display State Updates - PASS / FAIL / SKIPPED

Section 5: Input Tests
  [ ] Button Short Press (auto-detect) - PASS / FAIL
  [ ] Button Long Press (auto-detect) - PASS / FAIL / SKIPPED
  [ ] Arm/Safe Switch - PASS / FAIL / SKIPPED

Section 6: Communication Tests
  [ ] State Broadcast - PASS / FAIL
  [ ] Communication Range - PASS / FAIL / SKIPPED

Section 7: State Machine Tests
  [ ] State Transitions - PASS / FAIL / SKIPPED

Section 8: Safety Tests
  [ ] Safe State GPIO (Manual) - PASS / FAIL
  [ ] Emergency Halt - PASS / FAIL

Section 9: Boot Sequence Tests
  [ ] Normal Boot Sequence - PASS / FAIL
  [ ] Boot Without Remote - PASS / FAIL / SKIPPED
  [ ] Boot Without Base - PASS / FAIL / SKIPPED

Section 10: Stress Tests
  [ ] Rapid State Changes - PASS / FAIL / SKIPPED
  [ ] Communication Stress - PASS / FAIL / SKIPPED
  [ ] Button Spam Test - PASS / FAIL / SKIPPED

Overall Result: _____ PASSED / FAILED tests of _____ TOTAL

Notes/Observations:
_______________________________________________________________________________
_______________________________________________________________________________
_______________________________________________________________________________
```

---

## Troubleshooting Guide

### Issue: Buzzer Not Working
- Check GPIO connection (pin 42 on both units)
- Verify active-low wiring (0V = ON, 3.3V = OFF)
- Run diagnostic to confirm firmware type

### Issue: LED Not Displaying Correct Color
- Check GPIO 47 connection
- Verify WS2812 data line orientation
- Green channel known issue (may be hardware)

### Issue: Display Garbled/Blank
- Check I2C connection (GPIO 8/9)
- Verify SSD1306 address (0x3C default)
- Check display power supply

### Issue: Button Not Responding
- Check GPIO 16 connection
- Verify pull-up resistor
- Try longer press (2+ seconds)

### Issue: Communication Failure
- Verify both units powered on
- Check MAC addresses in config.h match
- Move units closer together
- Verify no WiFi interference

---

## Success Criteria

**Phase 1 MVP is considered COMPLETE when:**
- [ ] All automated tests pass (12/12)
- [ ] Buzzer works on both units (continuous tone)
- [ ] RGB LED shows correct state colors
- [ ] OLED display shows state and log lines
- [ ] Button short press detected reliably
- [ ] Switch position detected correctly
- [ ] State broadcast working (REMOTE shows BASE state)
- [ ] All 13 states accessible via command
- [ ] Emergency halt triggers immediately
- [ ] Boot sequence consistent (3 beeps + LED flash)
- [ ] Safe state GPIO verified (igniter pins LOW)

**Current Status:**
- Automated Tests: ✅ 12/12 PASS
- Manual Tests: ⏳ Pending execution
- Overall: Ready for manual verification

---

**Next Steps After Manual Testing:**
1. Document any failures in GitHub Issues
2. Fix critical bugs
3. Begin Phase 2 planning (ADC & SD logging)
4. Create calibration procedures (Phase 4)

---

**Test Plan Version:** 1.1
**Last Updated:** 2026-02-10
**Firmware Version:** v1.1.3

**Changes in v1.1:**
- Improved button press test instructions with clear "Press Enter to START" workflow
- Button LED test now uses OFF→ON blink sequence for clear visibility
- Fixed `ask_user()` bug with invalid `default` parameter
- Enhanced test feedback with automatic button press detection
