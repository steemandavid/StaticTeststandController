# Manual Testing Plan
## StaticTeststandController v1.2.0

**Purpose:** Comprehensive manual verification of Phase 1 MVP + Phase 2 ADC & SD Card Logging features
**Duration:** Approximately 60-90 minutes
**Prerequisites:** Both units powered and connected via USB, SD card inserted in BASE

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
- [ ] **SD card (FAT32 formatted) inserted in BASE**
- [ ] **Optional:** ADS1256 ADC module connected to BASE
- [ ] **Optional:** Load cell and/or pressure transducer connected
- [ ] **Optional:** Test motor/igniter for actual firing test

### Pre-Test Checklist
1. [ ] Flash both units with correct firmware (use `scripts/build_and_flash_all.sh`)
2. [ ] Verify firmware with `scripts/diagnostic.py`
3. [ ] **Format SD card as FAT32 (if not already)**
4. [ ] **Copy `settings_example.txt` to SD card as `settings.txt`**
5. [ ] **Edit `settings.txt` with your WiFi credentials and desired sample rate**
6. [ ] Insert SD card into BASE unit
7. [ ] Power cycle both units
8. [ ] Confirm 3 beeps + LED flash on boot (both units)
9. [ ] Confirm green breathing LED on both units after boot
10. [ ] **Check BASE serial output for "SD card mounted successfully" message**

---

## Test Sections

### Section 1: Automated Infrastructure Tests ✅ ALREADY PASSED

**Status:** These tests run automatically and have all passed (12/12)

**Current Firmware:** v1.2.0 (Phase 2 COMPLETE)

| Test | Result | Notes |
|------|--------|-------|
| PING BASE | ✅ PASS | ~15ms response |
| INFO BASE | ✅ PASS | v1.2.0, BASE target, ESP-IDF v5.5.2 |
| HEAP BASE | ✅ PASS | 8.5MB free, PSRAM active |
| TASKS BASE | ✅ PASS | 19 tasks running (Phase 2 adds 2) |
| QUEUE STATUS BASE | ✅ PASS | All healthy (7 queues including ADC/log) |
| ESPNOW STATUS BASE | ✅ PASS | Initialized |
| PING REMOTE | ✅ PASS | ~12ms response |
| INFO REMOTE | ✅ PASS | v1.2.0, REMOTE target, ESP-IDF v5.5.2 |
| HEAP REMOTE | ✅ PASS | 8.5MB free, PSRAM active |
| TASKS REMOTE | ✅ PASS | 19 tasks running |
| QUEUE STATUS REMOTE | ✅ PASS | All healthy (5 queues) |
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

### Section 11: Phase 2 - SD Card Logging Tests

**Purpose:** Verify SD card FAT32 logging functionality (Phase 2 feature)

#### Test 11.1: SD Card Mount Detection
1. Power on BASE with SD card inserted
2. Check serial output for:
   - [ ] "SD card mounted successfully" message
   - [ ] Card size displayed correctly
   - [ ] Settings loaded from settings.txt
3. Remove SD card and power cycle:
   - [ ] Warning message logged: "SD card not available, continuing without logging"
   - [ ] System continues operating normally (graceful degradation)

**Expected Result:** SD card mount detected, graceful degradation when absent

#### Test 11.2: Settings File Parsing
1. Edit `settings.txt` on SD card with custom values:
   ```
   ADC_SAMPLE_RATE 100
   IGNITER_ON_TIME 2.5
   WIFI_SSID YourNetwork
   ```
2. Reboot BASE with SD card inserted
3. Check serial output:
   - [ ] Settings displayed correctly on boot
   - [ ] "ADC sample rate: 100 Hz" message visible
4. Verify ADC task uses correct rate:
   - [ ] Watch ADC sampling LEDs (if implemented)
   - [ ] Check sample queue depth via TASKS command

**Expected Result:** Settings parsed and applied correctly

#### Test 11.3: CSV File Creation Test
1. Put BASE in TESTRUNNING state:
   ```
   TEST TEST_MODE ON
   TEST STATE TESTRUNNING
   ```
2. Wait 10 seconds, then:
   ```
   TEST STATE IDLE
   TEST TEST_MODE OFF
   ```
3. Remove SD card and check on computer:
   - [ ] File created: `TEST_YYYYMMDD_HHMMSS.csv`
   - [ ] File has correct header row
   - [ ] Data rows present (~100 rows at 10 Hz)
   - [ ] Summary section at end

**Expected Result:** CSV file created with headers, data, and summary

#### Test 11.4: Data Logging Verification (Without Motor)
1. Create a test setup with:
   - Load cell connected (or simulate with voltage source)
   - Pressure transducer connected (or simulate)
2. Put BASE in TESTRUNNING state for 30 seconds
3. Apply varying input during test
4. Check CSV file:
   - [ ] Timestamp column present
   - [ ] Load cell data varies with input
   - [ ] Pressure data varies with input
   - [ ] No missing samples (check row count)
   - [ ] Summary statistics correct

**Expected Result:** Accurate data capture, no dropped samples

---

### Section 12: Phase 2 - ADC Hardware Tests

**Purpose:** Verify ADS1256 ADC functionality (Phase 2 feature)

**Note:** These tests require ADS1256 hardware connected (GPIO 35-40)

#### Test 12.1: ADC Initialization
1. Power on BASE with ADS1256 connected
2. Check serial output:
   - [ ] "ADS1256 ADC initialized successfully" message
   - [ ] Self-calibration completes
   - [ ] DRDY interrupt configured

**Expected Result:** ADC initializes without errors

#### Test 12.2: ADC Channel Reading
1. Apply known voltages to ADC inputs:
   - Channel 0 (Loadcell): 0V, 1.65V, 3.3V
   - Channel 1 (Pressure): 0V, 1.65V, 3.3V
2. Monitor CSV file output:
   - [ ] Values change with input voltage
   - [ ] No saturation at mid-range (1.65V)
   - [ ] 0V reads near minimum, 3.3V reads near maximum

**Expected Result:** ADC readings proportional to input voltage

#### Test 12.3: Sample Rate Verification
1. Set `ADC_SAMPLE_RATE 10` in settings.txt
2. Run 30-second test
3. Verify CSV file:
   - [ ] ~300 samples (10 Hz × 30s)
   - [ ] Consistent sample interval (check timestamps)
4. Repeat with `ADC_SAMPLE_RATE 100`:
   - [ ] ~3000 samples
   - [ ] No sample drops

**Expected Result:** Correct sample rate, consistent timing

#### Test 12.4: End-of-Burn Detection
1. Apply simulated thrust signal to load cell input
2. Put BASE in TESTRUNNING state
3. Wait for baseline (0.5s), then apply signal
4. Remove signal and wait:
   - [ ] State transitions to ENDTEST after ~5 seconds
   - [ ] Post-burn data captured (END_TEST_DELAY = 10s default)
   - [ ] Test summary includes max thrust, impulse, etc.

**Expected Result:** Automatic burn detection and post-burn logging

---

### Section 13: Phase 2 - Integration Tests

**Purpose:** Verify Phase 2 features integrate with Phase 1 functionality

#### Test 13.1: State Machine + Logging Integration
1. Perform full state transition test (Section 7)
2. Verify each state:
   - [ ] Data logging starts/stops correctly in TESTRUNNING
   - [ ] No data logged in IDLE/ARMED states
   - [ ] CSV file created only when test runs

**Expected Result:** Logging controlled by state machine correctly

#### Test 13.2: ESP-NOW + Logging Concurrent Operation
1. Run BASE and REMOTE simultaneously
2. Start test on BASE (TESTRUNNING state)
3. Monitor REMOTE during logging:
   - [ ] Display updates continue (no lag)
   - [ ] ESP-NOW pings still successful
   - [ ] State broadcasts working
4. Verify BASE serial output:
   - [ ] No watchdog warnings
   - [ ] ADC task running (P7 priority)
   - [ ] SD logging task running (P6 priority)

**Expected Result:** All systems run concurrently without interference

#### Test 13.3: SD Card Failure Recovery
1. Start test with SD card inserted
2. During test, carefully remove SD card (hot-swap):
   - [ ] Test continues without crash
   - [ ] Warning logged: "SD card write failed"
   - [ ] Data continues to queue (until full)
3. Reinsert SD card (before test ends):
   - [ ] SD card remounts
   - [ ] Remaining data written
4. Finish test and verify CSV:
   - [ ] File has gap where SD removed
   - [ ] Data before and after gap intact

**Expected Result:** Graceful handling of SD card failure

---

### Section 14: Live Fire Test (Optional - Requires Motor)

**Purpose:** Full system test with actual motor firing

**WARNING:** Only perform this test in a safe, designated testing area with proper safety equipment.

#### Test 14.1: Complete Static Fire Test
1. Set up test stand with motor
2. Connect all sensors (load cell, pressure, breakwires, igniter)
3. Configure settings.txt for your motor
4. Perform pre-flight checks (CHK_IGN, CHK_BRK states)
5. Execute full test sequence:
   - [ ] IDLE → ARMED (switch)
   - [ ] ARMED → STARTTEST (button)
   - [ ] STARTTEST → IGNITION → TESTRUNNING
   - [ ] Motor fires (verify video/audio if recording)
   - [ ] TESTRUNNING → ENDTEST (auto-detect)
   - [ ] ENDTEST → IDLE (auto)
6. Review CSV file:
   - [ ] Pre-ignition baseline captured
   - [ ] Burn curve visible (thrust rise, plateau, decay)
   - [ ] Peak thrust value reasonable
   - [ ] Pressure data captured
   - [ ] Breakwire triggers recorded (if used)
   - [ ] Summary statistics accurate

**Expected Result:** Complete test data captured, motor fired successfully

---

## Test Results Template

Copy this template to record your test results:

```
Manual Test Results - StaticTeststandController v1.2.0
Date: _______________
Tester: _______________

=== Phase 1 Tests (Sections 2-10) ===

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

=== Phase 2 Tests (Sections 11-14) ===

Section 11: SD Card Logging
  [ ] SD Card Mount Detection - PASS / FAIL
  [ ] Settings File Parsing - PASS / FAIL
  [ ] CSV File Creation - PASS / FAIL
  [ ] Data Logging Verification - PASS / FAIL / SKIPPED

Section 12: ADC Hardware
  [ ] ADC Initialization - PASS / FAIL / N/A (no hardware)
  [ ] ADC Channel Reading - PASS / FAIL / N/A (no hardware)
  [ ] Sample Rate Verification - PASS / FAIL / N/A (no hardware)
  [ ] End-of-Burn Detection - PASS / FAIL / N/A (no hardware)

Section 13: Phase 2 Integration
  [ ] State Machine + Logging - PASS / FAIL / SKIPPED
  [ ] ESP-NOW + Logging Concurrent - PASS / FAIL / SKIPPED
  [ ] SD Card Failure Recovery - PASS / FAIL / SKIPPED

Section 14: Live Fire Test
  [ ] Complete Static Fire Test - PASS / FAIL / SKIPPED / N/A

Overall Result:
  Phase 1: _____ PASSED / FAILED tests of _____ TOTAL
  Phase 2: _____ PASSED / FAILED tests of _____ TOTAL
  Combined: _____ PASSED / FAILED tests of _____ TOTAL

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

### Phase 2 Troubleshooting

**Issue: SD Card Not Detected**
- Verify SD card formatted as FAT32
- Check SD card SPI connections (GPIO 10-13)
- Try different SD card (some brands not compatible)
- Verify card size ≤ 32GB (SDHC)
- Check serial output for "SD card not available" warning

**Issue: Settings Not Loaded**
- Verify `settings.txt` exists in SD card root
- Check file format: `KEY VALUE` (one per line)
- Verify no BOM or special characters
- Check serial output for parsing errors
- Compare with `settings_example.txt`

**Issue: CSV File Not Created**
- Verify BASE entered TESTRUNNING state
- Check SD card has free space
- Verify ADC sampling task running (TASKS command)
- Check log_queue has samples (QUEUE STATUS command)
- Try manual test with TEST TEST_MODE ON

**Issue: ADC Not Reading**
- Verify ADS1256 connected to GPIO 35-40
- Check SPI connections (MOSI, MISO, SCLK, CS, RST, DRDY)
- Verify 3.3V power supply to ADC
- Check serial output for "ADC initialization failed"
- Verify DRDY interrupt GPIO (40) connected

**Issue: Sample Rate Wrong**
- Verify `ADC_SAMPLE_RATE` in settings.txt
- Check serial output for applied sample rate
- Reboot BASE after changing settings.txt
- Lower rate if CPU overloaded (try 10 Hz)

**Issue: Dropped Samples**
- Reduce ADC_SAMPLE_RATE (try 10 Hz)
- Check available heap (HEAP command)
- Verify SD card write speed (class 10 recommended)
- Check for watchdog warnings in serial output

---

## Success Criteria

### Phase 1 MVP (COMPLETE ✅)
- [x] All automated tests pass (12/12)
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

### Phase 2: ADC & SD Logging (COMPLETE ✅)
- [ ] SD card mounts successfully on boot
- [ ] Settings.txt parsed and applied
- [ ] CSV file created on test completion
- [ ] ADC initializes and samples at configured rate
- [ ] Data logging controlled by state machine
- [ ] End-of-burn detection works
- [ ] SD card failure handled gracefully
- [ ] No sample drops at configured rate
- [ ] Test summary statistics accurate

**Current Status:**
- Phase 1 Automated Tests: ✅ 12/12 PASS
- Phase 1 Manual Tests: ⏳ Pending execution
- Phase 2 Implementation: ✅ COMPLETE (v1.2.0)
- Phase 2 Manual Tests: ⏳ Pending execution
- Overall: Both phases ready for manual verification

---

**Next Steps After Manual Testing:**
1. Document any failures in GitHub Issues
2. Fix critical bugs found during testing
3. Begin Phase 3 planning (WiFi + NTP time sync)
4. Create calibration procedures (Phase 4)

---

**Test Plan Version:** 2.0
**Last Updated:** 2026-02-11
**Firmware Version:** v1.2.0

**Changes in v2.0:**
- Updated for firmware v1.2.0 (Phase 2 COMPLETE)
- Added Section 11: SD Card Logging Tests
- Added Section 12: ADC Hardware Tests
- Added Section 13: Phase 2 Integration Tests
- Added Section 14: Live Fire Test
- Updated automated test results (19 tasks on BASE)
- Added Phase 2 troubleshooting section
- Expanded success criteria for Phase 2
- Updated equipment needed and prerequisites
