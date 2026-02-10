# Functional Specification Document
## StaticTeststandController v2.0

**Version:** 2.0  
**Date:** February 3, 2026  
**Author:** David  
**Status:** ✅ READY FOR IMPLEMENTATION

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Hardware Architecture](#2-hardware-architecture)
3. [Software Architecture](#3-software-architecture)
4. [BASE Unit Specifications](#4-base-unit-specifications)
5. [REMOTE Unit Specifications](#5-remote-unit-specifications)
6. [Communication Protocol](#6-communication-protocol)
7. [Development Phases](#7-development-phases)
8. [Testing Strategy](#8-testing-strategy)
9. [Appendices](#9-appendices)

---

## 1. System Overview

### 1.1 Purpose
StaticTeststandController provides a safe, reliable system for static testing rocket motors with:
- Real-time data acquisition at 1000 Hz
- Remote wireless control
- Comprehensive data logging to SD card
- Safety interlocks

### 1.2 Key Features
- **Dual-unit architecture:** BASE (data logger) + REMOTE (controller)
- **High-speed ADC:** AS1256 24-bit, up to 1000 Hz sampling
- **Wireless communication:** ESP-NOW protocol
- **Data logging:** CSV format with test summaries
- **Time synchronization:** NTP + RTC (DS1307)
- **Safety features:** Interlocks, watchdog, emergency halt
- **Calibration modes:** For all sensors
- **Status indication:** RGB LEDs, OLED display, buzzer

### 1.3 Technology Stack
- **Platform:** ESP32-S3-N16R8 (16MB Flash, 8MB PSRAM)
- **Framework:** ESP-IDF v5.0+
- **RTOS:** FreeRTOS
- **ADC:** AS1256 24-bit SPI
- **Storage:** SD card (FAT32)
- **Display:** SSD1306 OLED (128×64)
- **Communication:** ESP-NOW
- **Repository:** GitHub

---

## 2. Hardware Architecture

### 2.1 BASE Unit

**Main Components:**
- ESP32-S3-N16R8 development board
- AS1256 24-bit ADC (SPI interface)
- SD card module (SPI)
- DS1307 RTC with CR2032 backup
- N-channel FET for igniter (19V, 2A max)
- WS2812 RGB LED
- Buzzer

**GPIO Assignments:**

| GPIO | Function | Notes |
|------|----------|-------|
| 35 | AS1256 DIN (MOSI) | SPI |
| 36 | AS1256 SCLK | SPI clock |
| 37 | AS1256 DOUT (MISO) | SPI |
| 38 | AS1256 RST | Reset |
| 39 | AS1256 CS | Chip select |
| 40 | AS1256 DRDY | Data ready interrupt |
| 10 | SD_CS | SD card chip select |
| 9 | SD_MOSI | SD card SPI |
| 12 | SD_CLK | SD card clock |
| 13 | SD_MISO | SD card SPI |
| 8 | RTC_SDA | I2C data |
| 9 | RTC_SCL | I2C clock |
| 41 | IGNITION | Igniter FET control |
| 40 | LOW_SIDE_POWER | Igniter power |
| 42 | BUZZER | Audio feedback |
| 47 | RGB_LED | WS2812 status |
| 2 | LED_BUILTIN | Status LED |

**AS1256 ADC Channel Map:**

| Port | Sensor | Purpose |
|------|--------|---------|
| 0 | Load cell | Thrust (kg) |
| 1 | Pressure transducer | Chamber pressure (bar) |
| 2 | Igniter sense | Continuity/current (V) |
| 3-6 | Break wires 1-4 | Safety interlocks (V) |
| 7 | Reserved | Future use |

**Igniter Circuit:**
- Supply: 19V DC
- Max current: 2A
- Load resistance: 0.5-2Ω (typical igniter)
- Current sensing: 3.3kΩ resistor to AS1256 port 2

### 2.2 REMOTE Unit

**Main Components:**
- ESP32-S3 development board  
- SSD1306 OLED display (128×64, I2C)
- Illuminated pushbutton
- Arm/Safe toggle switch
- 1S LiPo battery with voltage monitoring
- WS2812 RGB LED
- Buzzer

**GPIO Assignments:**

| GPIO | Function | Notes |
|------|----------|-------|
| 8 | I2C_SDA | OLED display |
| 9 | I2C_SCL | OLED display |
| 16 | BUTTON_BUTTON | Ignition button input |
| 17 | LED_BUTTON | Button backlight |
| 4 | SWITCH_ARMED | Toggle position (active low) |
| 5 | SWITCH_SAFE | Toggle position (active low) |
| 1 | VOLT_BAT | Battery voltage ADC |
| 25 | BUZZER | Audio feedback |
| 47 | RGB_LED | WS2812 status |
| 32 | LED_BUILTIN | Status LED |

**Battery Monitoring:**
- Battery: 1S LiPo (3.0-4.2V)
- Voltage divider: R1=5.6kΩ, R2=10kΩ
- ADC range: 1.92-2.69V (maps to 3.0-4.2V battery)
- Warning threshold: 30% (3.36V)
- Critical threshold: 10% (3.12V)

---

## 3. Software Architecture

### 3.1 Project Structure

```
StaticTeststandController/
├── CMakeLists.txt
├── sdkconfig
├── README.md
├── TODO.md
├── main/
│   ├── main.c
│   ├── config.h
│   ├── base/
│   │   ├── base_main.c
│   │   ├── state_machine.c/h
│   │   ├── adc_as1256.c/h
│   │   ├── sd_logger.c/h
│   │   ├── rtc_ds1307.c/h
│   │   ├── settings.c/h
│   │   └── igniter.c/h
│   ├── remote/
│   │   ├── remote_main.c
│   │   ├── display_ssd1306.c/h
│   │   ├── input_handler.c/h
│   │   └── battery_monitor.c/h
│   └── common/
│       ├── esp_now_protocol.c/h
│       ├── rgb_led.c/h
│       └── safety.c/h
└── docs/
    ├── SETUP.md
    ├── OPERATION.md
    └── CALIBRATION.md
```

### 3.2 FreeRTOS Tasks

**BASE Tasks:**
- `state_machine_task` (P5): Main state machine execution
- `adc_sampling_task` (P7): 1000 Hz sampling
- `sd_logging_task` (P6): Write to SD card
- `espnow_rx_task` (P5): Receive commands
- `espnow_tx_task` (P5): Send status
- `watchdog_task` (P8): Safety monitoring

**REMOTE Tasks:**
- `input_handler_task` (P5): Button/switch processing
- `display_update_task` (P3): OLED refresh
- `espnow_rx_task` (P5): Receive status
- `espnow_tx_task` (P5): Send commands + ping
- `battery_monitor_task` (P2): Voltage monitoring
- `rgb_led_task` (P4): LED animations

---

## 4. BASE Unit Specifications

### 4.1 State Machine

**States:**
- `INIT`: System initialization
- `IDLE`: Ready, waiting for commands
- `ARMED`: System armed, ready to fire
- `STARTTEST`: Initialize test, create files
- `IGNITION`: Fire igniter
- `TESTRUNNING`: Monitor burn, log data
- `ENDTEST`: Finalize, write summary
- `HALT`: Safe shutdown (requires power cycle)
- `CHECK_IGNITER`: Pre-flight igniter check
- `CHECK_BREAKWIRES`: Pre-flight breakwire check
- `CALIBRATE_LOADCELL`: Calibrate thrust sensor
- `CALIBRATE_PRESSURE`: Calibrate pressure sensor
- `WELCOME_SCREEN`: Startup splash (optional)

**State Transitions:**

```
INIT → IDLE
IDLE → ARMED (when SWITCH_ARMED activated)
IDLE → CHECK_IGNITER (ignition button press while SAFE)
ARMED → STARTTEST (ignition button long press)
STARTTEST → IGNITION
IGNITION → TESTRUNNING
TESTRUNNING → ENDTEST (burn complete)
ENDTEST → HALT
Any state → HALT (on critical error)
```

### 4.2 Settings File (settings.txt)

**Format:** `KEY VALUE // optional comment`

**Example:**
```
IGNITER_ON_TIME 0.5
ADC_PORT_LOADCELL 0
ADC_PORT_PRESSURE_TRANSDUCER 1
ADC_PORT_IGNITER_SENSE 2
ADC_PORT_BREAKWIRE1 3
ADC_PORT_BREAKWIRE2 4
ADC_PORT_BREAKWIRE3 5
ADC_PORT_BREAKWIRE4 6
WIFI_NETWORK_SSID your-network-name
WIFI_NETWORK_PASSWORD your-password
ADC_SAMPLE_RATE 1000
ADC_CAL_VALUE_LOADCELL 0.001234
ADC_CAL_VALUE_PRESSURE_TRANSDUCER 0.004567
ADC_CAL_VALUE_IGNITER 0.000806
ADC_CAL_VALUE_BREAKWIRE1 1.0
ADC_CAL_VALUE_BREAKWIRE2 1.0
ADC_CAL_VALUE_BREAKWIRE3 1.0
ADC_CAL_VALUE_BREAKWIRE4 1.0
COMMS_WARNING 2
COMMS_ERROR 5
END_TEST_DELAY 5
```

**Notes:**
- File must be present on SD card root
- No default values - missing file causes HALT
- Comments start with `//`
- Calibration values determined through calibration states

### 4.3 Data Logging

**CSV File Format:**

Filename: `YYYY-MM-DD-HH-MM-SS.csv` (e.g., `2026-02-03-14-30-15.csv`)

**Header:**
```csv
Static test started
State: 4 (StartTest)
Date: 2026-02-03 14:30:15
Settings:
  IGNITER_ON_TIME: 0.5
  ADC_SAMPLE_RATE: 1000
  ...
  
Timestamp_us,Thrust_kg,Pressure_bar,Igniter_V,BW1_V,BW2_V,BW3_V,BW4_V
```

**Data Rows:**
```csv
0.000000,0.00,0.00,12.45,0.00,0.00,0.00,0.00
0.001000,0.12,0.50,12.40,0.00,0.00,0.00,0.00
...
```

**Test Summary (appended):**
```csv

--- Test Summary ---
End State: 6 (EndTest)
Test Duration: 6.543 seconds
Maximum Thrust: 45.23 kg
Total Impulse: 89.34 kg·s
Maximum Pressure: 18.76 bar
```

**Run Log (runlog.txt):**
```
2026-02-03 14:25:10 - System initialized
2026-02-03 14:26:45 - State: ARMED -> STARTTEST
2026-02-03 14:26:52 - Burn end detected
...
```

### 4.4 End-of-Burn Detection

**Algorithm:**
1. Record baseline (average first 0.5 seconds)
2. Detect when thrust AND pressure return to within 5% of baseline
3. Must remain at baseline for END_TEST_DELAY seconds
4. Continue logging for additional END_TEST_DELAY seconds after detection
5. Transition to ENDTEST

**Implementation:**
```c
// Check if sensors within 5% of baseline
bool thrust_at_baseline = fabs(thrust - baseline_thrust) < (baseline_thrust * 0.05);
bool pressure_at_baseline = fabs(pressure - baseline_pressure) < (baseline_pressure * 0.05);

if (thrust_at_baseline && pressure_at_baseline) {
    end_counter++;
    if (end_counter >= SAMPLE_RATE * END_TEST_DELAY) {
        // Continue logging for END_TEST_DELAY more
        post_burn_counter++;
        if (post_burn_counter >= SAMPLE_RATE * END_TEST_DELAY) {
            goto ENDTEST;
        }
    }
}
```

---

## 5. REMOTE Unit Specifications

### 5.1 Display Layout (SSD1306 OLED)

**Screen: 128×64 pixels**

```
┌────────────────────────────┐
│ [STATE]           [RSSI]   │ ← 16px font, status line
├────────────────────────────┤ ← Double line separator
│ Most recent log            │ ← 8px font
│ Previous log               │
│ Older log                  │
│ Even older                 │
│ Oldest visible             │
└────────────────────────────┘
```

- Status line shows BASE state (left) and RSSI/fail counter (right)
- Log lines scroll: new appears on line 1, oldest falls off line 5
- Max 21 characters per log line

### 5.2 Input Detection

**Button Events:**
- Short press: < 2 seconds
- Long press: ≥ 2 seconds  
- Double press: Two presses within 500ms

**Switch States:**
- SAFE: SWITCH_SAFE=LOW, SWITCH_ARMED=HIGH
- ARMED: SWITCH_SAFE=HIGH, SWITCH_ARMED=LOW
- ERROR: Both LOW or both HIGH

**Debouncing:** 50ms for all inputs

### 5.3 RGB LED Status Colors

| State | Color | Pattern |
|-------|-------|---------|
| Safe (IDLE) | Green | Breathing |
| Armed | Orange | Solid |
| Testing | Red | Blinking 2 Hz |
| Test Complete | Blue | Solid |
| Error/HALT | Red | Slow pulse 0.5 Hz |
| Switch Error | Red | Rapid blink 5 Hz |

---

## 6. Communication Protocol (ESP-NOW)

### 6.1 Packet Structure

```c
typedef struct {
    uint8_t base_state;           // Current BASE state (0-12)
    uint8_t command;              // Command ID
    int16_t data;                 // Parameter (duration, count, RSSI, etc.)
    char message[21];             // Text payload (20 chars + null)
} __attribute__((packed)) espnow_packet_t;
```

**Total size:** 25 bytes

### 6.2 Command IDs

**Communication (0x00-0x0F):**
- `0x00`: PING (REMOTE → BASE, every 1 second)
- `0x01`: PING_RESPONSE (BASE → REMOTE, includes RSSI)
- `0x02`: COMMS_WARNING
- `0x03`: COMMS_ERROR
- `0x04`: HALT (emergency stop)

**Input Commands (0x10-0x3F, REMOTE → BASE):**
- `0x10-0x13`: Safe/Arm switch short/long press
- `0x14-0x15`: Switch position changes
- `0x20-0x22`: Ignition button short/long/double press
- `0x30-0x31`: Battery warning/critical

**Output Commands (0x40-0x4F, BASE → REMOTE):**
- `0x40-0x48`: LED/buzzer control

**Display Commands (0x50-0x5F, BASE → REMOTE):**
- `0x50`: Clear display
- `0x51`: Add log line (message field)
- `0x52`: Display sensor value

### 6.3 Retry Logic

**Critical commands** (state changes, errors): 5 retries, 100ms apart
- If 2+ retries needed: Log warning
- If all 5 fail: Trigger COMMS_ERROR

**Normal commands** (ping, routine updates): Single attempt

### 6.4 RSSI Monitoring

**REMOTE behavior:**
- Send ping every 1 second
- Track weighted average of last 5 RSSI values
- Display RSSI magnitude on OLED
- Warning if RSSI < -70 dBm for COMMS_WARNING seconds
- Error if RSSI < -85 dBm for COMMS_ERROR seconds

**Thresholds (configurable in config.h):**
```c
#define RSSI_WARNING_THRESHOLD -70  // dBm
#define RSSI_ERROR_THRESHOLD -85    // dBm
```

---

## 7. Development Phases

### Phase 1: MVP (Minimum Viable Product)

**Goal:** Basic communication and state machine

**Deliverables:**
- BASE firmware with complete state machine
- REMOTE firmware with display, inputs, outputs
- ESP-NOW bidirectional communication
- Retry logic
- RGB LED status indication
- Switch safety logic

**Test:** Manual walkthrough of all states via buttons

**Hardware:** ESP32-S3 boards, OLED, buttons, switches (no ADC/SD yet)

---

### Phase 2: ADC & SD Card Logging

**Goal:** Sensor acquisition and storage

**Deliverables:**
- AS1256 SPI driver
- SD card FAT32 mounting
- Settings file parser
- High-speed data logging (1000 Hz)
- CSV generation with summaries
- Run log

**Test:** Verify sample rate with oscilloscope, validate CSV format

**Hardware:** Add AS1256 ADC, SD card, potentiometers (simulate sensors)

---

### Phase 3: Time Synchronization

**Goal:** Accurate timestamping

**Deliverables:**
- WiFi connection manager
- NTP/SNTP client
- DS1307 RTC I2C driver
- File naming with timestamps

**Test:** Verify NTP sync, RTC persistence across power cycles

**Hardware:** Add DS1307 RTC with CR2032 battery

---

### Phase 4: Calibration

**Goal:** Sensor calibration and pre-flight checks

**Deliverables:**
- Calibration state logic
- Real-time sensor display to REMOTE
- Calibration procedures documented

**Test:** Calibrate with known weights/pressures, verify data accuracy

**Hardware:** Add real load cell and pressure transducer

---

### Phase 5: Future Features (TBD)

- Auto power-off circuit
- Web interface for data viewing
- Multi-motor support

---

## 8. Testing Strategy

### 8.1 Unit Tests

- Settings parser (valid, missing, corrupt files)
- State machine (all transitions, error conditions)
- AS1256 driver (SPI communication, timeouts)
- SD card (mount, write, read, removal)
- ESP-NOW (send, receive, RSSI, retry)
- Button handler (debounce, short/long/double press)
- Display (clear, write, scroll)
- Battery monitor (voltage calc, thresholds)

### 8.2 Integration Tests

1. **Communication range:** Test at 1m, 5m, 10m, 20m+
2. **State walkthrough:** Boot → Calibrate → Arm → Fire → Complete
3. **Data accuracy:** Known inputs → verify CSV output
4. **Calibration:** Apply known weight/pressure → verify calculations
5. **Safety:** Trigger all error conditions → verify safe state

### 8.3 System Test

**Full test sequence:**
1. Power on, verify init
2. Check igniter continuity
3. Check breakwires
4. Arm system
5. Fire motor (or dummy load)
6. Verify data logging
7. Verify end detection
8. Review CSV file

**Success criteria:**
- 10 consecutive tests without crashes
- Communication range > 20m
- Sample rate accuracy ±1%
- All safety interlocks functional

---

## 9. Appendices

### Appendix A: Complete GPIO Tables

**BASE Unit:**

| GPIO | Function | Dir | Notes |
|------|----------|-----|-------|
| 0 | BOOT_BUTTON | IN | Boot mode |
| 2 | LED_BUILTIN | OUT | Status |
| 8 | RTC_SDA | I/O | I2C |
| 9 | RTC_SCL | OUT | I2C |
| 10 | SD_CS | OUT | SPI |
| 12 | SD_CLK | OUT | SPI |
| 13 | SD_MISO | IN | SPI |
| 35 | ADS_DN | OUT | SPI MOSI |
| 36 | ADS_SCLK | OUT | SPI CLK |
| 37 | ADS_DOUT | IN | SPI MISO |
| 38 | ADS_RST | OUT | Reset |
| 39 | ADS_CS | OUT | SPI CS |
| 40 | ADS_DRDY | IN | Interrupt |
| 41 | IGNITION | OUT | FET control |
| 42 | BUZZER | OUT | Audio |
| 47 | RGB_LED | OUT | WS2812 |

**REMOTE Unit:**

| GPIO | Function | Dir | Notes |
|------|----------|-----|-------|
| 0 | BOOT_BUTTON | IN | Boot mode |
| 1 | VOLT_BAT | IN | ADC |
| 4 | SWITCH_ARMED | IN | Pull-up |
| 5 | SWITCH_SAFE | IN | Pull-up |
| 8 | I2C_SDA | I/O | OLED |
| 9 | I2C_SCL | OUT | OLED |
| 16 | BUTTON_BUTTON | IN | Pull-up |
| 17 | LED_BUTTON | OUT | Backlight |
| 25 | BUZZER | OUT | Audio |
| 32 | LED_BUILTIN | OUT | Status |
| 47 | RGB_LED | OUT | WS2812 |

### Appendix B: Data Structures

```c
// State machine
typedef enum {
    STATE_INIT = 0,
    STATE_IDLE,
    STATE_ARMED,
    STATE_STARTTEST,
    STATE_IGNITION,
    STATE_TESTRUNNING,
    STATE_ENDTEST,
    STATE_HALT,
    STATE_CHECK_IGNITER,
    STATE_CHECK_BREAKWIRES,
    STATE_CALIBRATE_LOADCELL,
    STATE_CALIBRATE_PRESSURE,
    STATE_WELCOME_SCREEN,
    STATE_MAX
} base_state_t;

// Settings
typedef struct {
    float igniter_on_time;
    uint8_t adc_port_loadcell;
    uint8_t adc_port_pressure;
    uint8_t adc_port_igniter_sense;
    uint8_t adc_port_breakwire[4];
    char wifi_ssid[64];
    char wifi_password[64];
    uint16_t adc_sample_rate;
    float adc_cal_loadcell;
    float adc_cal_pressure;
    float adc_cal_igniter;
    float adc_cal_breakwire[4];
    uint8_t comms_warning_timeout;
    uint8_t comms_error_timeout;
    uint8_t end_test_delay;
} settings_t;

// ADC sample
typedef struct {
    uint64_t timestamp_us;
    int32_t raw_adc[8];
    float loadcell_kg;
    float pressure_bar;
    float igniter_v;
    float breakwire_v[4];
} adc_sample_t;

// Display params
typedef struct {
    char base_state[10];
    uint16_t tx_rx_fails;
    char log_lines[5][22];
} display_params_t;
```

### Appendix C: Safety Features

**Interlocks:**
- Cannot arm unless SWITCH_ARMED active
- Cannot fire unless in IGNITION state
- Auto-disarm on comm timeout
- Igniter auto-cutoff at 2× IGNITER_ON_TIME

**Watchdog monitors:**
- State machine heartbeat (1s timeout)
- Communication link (COMMS_ERROR seconds)
- Igniter runaway

**Safe state actions:**
- Cut all power outputs (IGNITION, LOW_SIDE_POWER, BUZZER)
- Suspend dangerous tasks
- Close all files
- Notify REMOTE
- Set RGB to red

**Error conditions trigger HALT:**
- SD mount/write failure
- Settings missing/corrupt
- AS1256 not responding
- Communication timeout
- Switch error (except during test)
- Igniter overcurrent (>3A)
- State machine hang

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-02-03 | Initial draft with questions |
| 2.0 | 2026-02-03 | All questions resolved, ready for implementation |

---

## For Claude Code / Automaker

This FSD is ready for feature breakdown. Please:

1. Create GitHub repository: `StaticTeststandController`
2. Break into features for Kanban board
3. Organize by development phases
4. Begin with Phase 1 (MVP)

**Suggested initial features:**
- ESP-NOW communication protocol
- BASE state machine framework
- REMOTE input handling
- REMOTE OLED display driver
- RGB LED control
- Settings file parser
- AS1256 SPI driver
- SD card logging
- DS1307 RTC driver
- Safety interlocks
- Battery monitoring
- Calibration modes

---

**END OF FUNCTIONAL SPECIFICATION DOCUMENT v2.0**

---

*Document Status:* ✅ APPROVED FOR IMPLEMENTATION  
*Next Step:* Feature breakdown and Kanban board creation
