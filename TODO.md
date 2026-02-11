# StaticTeststandController - Development TODO

## Completed: Phase 1 MVP ✅

**Status:** COMPLETE (v1.1.105)
**Date Completed:** 2026-02-10

### Priority 1 - Completed Features

These 5 features were implemented first, in order:

1. **Project Build Configuration** ✅ COMPLETE
   - Created `sdkconfig.defaults` for ESP32-S3-N16R8
   - Verified CMakeLists.txt conditional compilation (BASE vs REMOTE)
   - Both targets build successfully
   - Automated build and flash script (`build_and_flash_all.sh`)

2. **FreeRTOS Queue and Semaphore Setup** ✅ COMPLETE
   - Defined shared queues (espnow_rx_queue, adc_sample_queue, log_queue, etc.)
   - Defined semaphores (spi_mutex, sd_mutex, i2c_mutex - for Phase 2)
   - Created initialization function in `shared_queues.c`
   - Both BASE and REMOTE initialize on boot

3. **ESP-NOW Communication Protocol** ✅ COMPLETE
   - Implemented 25-byte packed packet structure
   - Send functions: critical (5 retries) and normal (single attempt)
   - Receive callback with queue posting
   - Peer MAC registration working
   - Ping/response protocol operational

4. **BASE State Machine Framework** ✅ COMPLETE
   - Implemented state enum (13 states)
   - State transition function with validation
   - State machine task (FreeRTOS)
   - State handler dispatch table
   - Test mode support for automated testing
   - All states accessible via serial command

5. **REMOTE Input Handler** ✅ COMPLETE
   - Button detection: short press (<2s), long press (>=2s), double press (<500ms)
   - Switch detection: SAFE, ARMED, ERROR states
   - 50ms debouncing
   - FreeRTOS task with event queue

### Phase 1 Continued - Completed Features

6. **REMOTE OLED Display Driver** ✅ COMPLETE - SSD1306 I2C driver, status line + 5-line log display
7. **ESP-NOW RX/TX Tasks** ✅ COMPLETE - FreeRTOS tasks for send/receive with queue integration
8. **BASE State Handlers - All States** ✅ COMPLETE - All 13 state handlers implemented
9. **RGB LED Controller** ✅ COMPLETE - WS2812 driver with breathing, blink, pulse patterns
10. **Switch Safety Logic** ✅ COMPLETE - Interlock logic preventing unsafe state transitions
11. **Safety Watchdog** ✅ COMPLETE - Highest priority task monitors system health
12. **Buzzer Controller** ✅ COMPLETE - LEDC PWM at 4000Hz for audio feedback
13. **Button LED Control** ✅ COMPLETE - GPIO 17 control with proper initialization
14. **Test Protocol** ✅ COMPLETE - Serial command interface for automated testing
15. **Firmware Verification** ✅ COMPLETE - Auto-detect on connection

---

## Phase Overview

| Phase | Features | Status |
|-------|----------|--------|
| Infrastructure | 4 | ✅ Complete |
| Phase 1: MVP | 15 | ✅ Complete |
| Phase 2: ADC + Logging | 11 | 🚧 Partial (scaffolded) |
| Phase 3: Time Sync | 4 | ⏳ Not Started |
| Phase 4: Calibration | 7 | ⏳ Not Started |
| Phase 5: Future | 3 | ⏳ Not Started |
| Testing | 3 | ✅ Complete |

---

## Current Sprint: Phase 2 - ADC & SD Card Logging

### Priority 1 - Start Phase 2

These features should be implemented next, in order:

1. **AS1256 ADC Driver Implementation** (Phase 2)
   - [ ] SPI communication with AS1256 24-bit ADC
   - [ ] Channel reading (8 channels)
   - [ ] Data ready interrupt handling (GPIO 40)
   - [ ] ADC initialization and calibration
   - [ ] Integration with state machine
   - _Location: main/base/adc_as1256.c_

2. **SD Card Logger Implementation** (Phase 2)
   - [ ] SD card FAT32 mounting
   - [ ] CSV file generation with headers
   - [ ] Timestamped file naming (YYYY-MM-DD-HH-MM-SS.csv)
   - [ ] Write operations at 1000 Hz
   - [ ] Test summary generation
   - [ ] Run log (runlog.txt) implementation
   - _Location: main/base/sd_logger.c_

3. **Settings Parser Implementation** (Phase 2)
   - [ ] Parse settings.txt from SD card
   - [ ] Handle missing/corrupt files (HALT)
   - [ ] Comment handling (//)
   - [ ] Calibration value storage
   - [ ] WiFi credentials parsing
   - _Location: main/base/settings.c_

4. **High-Speed ADC Sampling Task** (Phase 2)
   - [ ] FreeRTOS task at 1000 Hz
   - [ ] Sample all 8 ADC channels
   - [ ] Post to adc_sample_queue
   - [ ] Raw-to-engineered unit conversion
   - _Priority: P7_

5. **End-of-Burn Detection Algorithm** (Phase 2)
   - [ ] Baseline recording (first 0.5s)
   - [ ] 5% threshold detection
   - [ ] END_TEST_DELAY verification
   - [ ] Post-burn logging period
   - _Location: main/base/state_machine.c (TESTRUNNING state)_

---

## Dependency Graph (Simplified)

```
Build Config
  |
  +-- FreeRTOS Queues
  |     |
  |     +-- BASE Main Init
  |     +-- REMOTE Main Init
  |     +-- State Machine Framework
  |     |     |
  |     |     +-- State Handlers (INIT/IDLE)
  |     |     +-- State Handlers (ARMED/HALT)
  |     |     +-- Safety Module & Watchdog
  |     |     +-- Test Sequence States (Phase 2)
  |     |     +-- Welcome Screen
  |     |     +-- Calibration States (Phase 4)
  |     |
  |     +-- ESP-NOW RX/TX Tasks
  |
  +-- ESP-NOW Protocol
  |     |
  |     +-- ESP-NOW RX/TX Tasks
  |     +-- Ping/RSSI Monitoring
  |     +-- Switch Safety Logic
  |     +-- Display Command Handler
  |
  +-- REMOTE Input Handler
  |     |
  |     +-- Switch Safety Logic
  |
  +-- REMOTE OLED Display
  |     |
  |     +-- Display Command Handler
  |     +-- Welcome Screen
  |
  +-- RGB LED Controller
  +-- Buzzer Controller
  +-- Button LED Control
  |
  +-- AS1256 ADC Init (Phase 2)
  |     |
  |     +-- AS1256 Channel Reading
  |           |
  |           +-- CSV File Generator
  |           +-- Test Sequence States
  |           +-- End-of-Burn Detection
  |           +-- Calibration States (Phase 4)
  |           +-- Real-Time Sensor Display
  |
  +-- SD Card Mount (Phase 2)
  |     |
  |     +-- Settings Parser
  |     +-- CSV File Generator
  |     +-- Run Log
  |
  +-- Igniter Control (Phase 2)
  |     |
  |     +-- Test Sequence States
  |     +-- Igniter Check (Phase 4)
  |
  +-- WiFi Manager (Phase 3)
  |     |
  |     +-- NTP Client
  |           |
  |           +-- Timestamp Formatter
  |
  +-- DS1307 RTC (Phase 3)
  |     |
  |     +-- Timestamp Formatter
  |
  +-- Battery Monitor (REMOTE, Phase 4)
  |     |
  |     +-- Auto Power-Off Circuit (Phase 5)
  |
  +-- Unit Test Framework (Testing)
  |     |
  |     +-- Integration Test Suite (Testing)
  |
  +-- WiFi Manager + SD Card + Settings + CSV (Phase 2+)
        |
        +-- Web Interface for Data Viewing (Phase 5)
        +-- Multi-Motor Support (Phase 5)
```

---

## Phase 5: Future Features

These features are planned for future development (from FSD Section 7, Phase 5):

- **Auto Power-Off Circuit** (`feature-1770206713333-autopwr04`) - ESP32-S3 deep sleep for REMOTE battery preservation
- **Web Interface for Data Viewing** (`feature-1770206713334-webview05`) - HTTP server on BASE for data access and live monitoring
- **Multi-Motor Support** (`feature-1770206713335-multimt06`) - Sequential/simultaneous ignition of multiple motors

---

## Testing Features

- **Unit Test Framework Setup** (`feature-1770206713330-utfsetup1`) - Unity test framework with ESP-IDF
- **Integration Test Suite** (`feature-1770206713331-inttest02`) - End-to-end system tests
- **Automated Firmware Flashing and Hardware Test Runner** (`feature-1770194733915-0470oc3jj`) - CI-style build/flash/test pipeline

---

## Phase 4: Additional

- **Calibration Procedures Documentation** (`feature-1770206713332-caldocs03`) - Detailed step-by-step calibration docs

---

## Notes

- All source files are scaffolded with headers and stub implementations
- Build system supports conditional compilation: `idf.py -D BUILD_TARGET=BASE build`
- Pin definitions are complete in `main/config.h`
- ESP-IDF v5.0+ required
