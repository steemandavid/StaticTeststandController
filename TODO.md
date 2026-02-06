# StaticTeststandController - Development TODO

## Current Sprint: Phase 1 MVP Foundation

### Priority 1 - Start Here

These 5 features should be implemented first, in order:

1. **Project Build Configuration** (`feature-1770190880132-roivcvnmd`)
   - Create `sdkconfig.defaults` for ESP32-S3-N16R8
   - Verify CMakeLists.txt conditional compilation (BASE vs REMOTE)
   - Ensure both targets build successfully
   - _No dependencies_

2. **FreeRTOS Queue and Semaphore Setup** (`feature-1770190880201-7n4zu7247`)
   - Define shared queues (espnow_rx_queue, adc_sample_queue, log_queue)
   - Define semaphores (spi_mutex, sd_mutex, i2c_mutex)
   - Create initialization function called from base_main/remote_main
   - _Depends on: #1_

3. **ESP-NOW Communication Protocol** (`feature-1770190880402-sykojp323`)
   - Implement 25-byte packed packet structure
   - Send functions: critical (5 retries) and normal (single attempt)
   - Receive callback with queue posting
   - Peer MAC registration
   - _Depends on: #1_

4. **BASE State Machine Framework** (`feature-1770190880605-bug75r7wj`)
   - Implement state enum (13 states)
   - State transition function with validation
   - State machine task (FreeRTOS)
   - State handler dispatch table
   - _Depends on: #1, #2_

5. **REMOTE Input Handler** (`feature-1770190880808-hw7c8w7cx`)
   - Button detection: short press (<2s), long press (>=2s), double press (<500ms)
   - Switch detection: SAFE, ARMED, ERROR states
   - 50ms debouncing
   - FreeRTOS task with event queue
   - _Depends on: #1_

---

## Next Up: Phase 1 MVP Continued

After the first 5, continue with these in parallel where possible:

6. **REMOTE OLED Display Driver** - SSD1306 I2C driver, status line + 5-line log display
7. **ESP-NOW RX/TX Tasks** - FreeRTOS tasks for send/receive with queue integration
8. **BASE State Handlers - INIT and IDLE** - Initialization sequence and idle state logic
9. **RGB LED Controller** - WS2812 driver with breathing, blink, pulse patterns
10. **Switch Safety Logic** - Interlock logic preventing unsafe state transitions

---

## Phase Overview

| Phase | Features | Status |
|-------|----------|--------|
| Infrastructure | 4 | Backlog |
| Phase 1: MVP | 15 | Backlog |
| Phase 2: ADC + Logging | 11 | Backlog |
| Phase 3: Time Sync | 4 | Backlog |
| Phase 4: Calibration | 7 | Backlog |
| Phase 5: Future | 3 | Backlog |
| Testing | 3 | Backlog |

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
