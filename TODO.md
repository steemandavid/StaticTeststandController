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
| Phase 2: ADC + Logging | 11 | ✅ Complete (v1.2.0) |
| Phase 3: Time Sync | 4 | ⏳ Not Started |
| Phase 4: Calibration | 7 | ⏳ Not Started |
| Phase 5: Future | 3 | ⏳ Not Started |
| Testing | 3 | ✅ Complete |

---

## Current Sprint: Phase 3 - Time Synchronization

### Priority 1 - Start Phase 3

1. **DS1307 RTC Driver Implementation** (Phase 3)
   - [ ] I2C communication with DS1307 RTC
   - [ ] Time read/write operations
   - [ ] Battery backup detection
   - [ ] Square wave output (1 Hz) for timestamping
   - _Location: main/base/rtc_ds1307.c_

2. **NTP Time Sync** (Phase 3)
   - [ ] WiFi connection (station mode)
   - [ ] NTP client implementation
   - [ ] RTC synchronization on boot
   - [ ] Timezone handling
   - _Location: main/base/ntp_sync.c_

3. **Timestamp Integration** (Phase 3)
   - [ ] ADC sample timestamping
   - [ ] CSV file timestamps
   - [ ] Run log timestamps
   - _Location: main/base/adc_as1256.c_

4. **Time Drift Compensation** (Phase 3)
   - [ ] Periodic NTP resync
   - [ ] RTC drift measurement
   - [ ] Software compensation
   - _Location: main/base/ntp_sync.c_

---

## Completed: Phase 2 - ADC & SD Card Logging ✅

**Status:** COMPLETE (v1.2.0)
**Date Completed:** 2026-02-11

### Priority 1 - Completed Features

1. **AS1256 ADC Driver Implementation** ✅ COMPLETE
   - SPI communication with AS1256 24-bit ADC
   - Channel reading (8 channels)
   - Data ready interrupt handling (GPIO 40)
   - ADC initialization and calibration
   - Integration with state machine
   - _Location: main/base/adc_as1256.c_

2. **SD Card Logger Implementation** ✅ COMPLETE
   - SD card FAT32 mounting
   - CSV file generation with headers
   - Timestamped file naming (TEST_YYYYMMDD_HHMMSS.csv)
   - Write operations at 1000 Hz
   - Test summary generation
   - _Location: main/base/sd_logger.c_

3. **Settings Parser Implementation** ✅ COMPLETE
   - Parse settings.txt from SD card
   - Handle missing/corrupt files (warning, continue with defaults)
   - Comment handling (# and //)
   - Key-value pair parsing
   - Validation of all settings
   - _Location: main/base/settings.c_

4. **High-Speed ADC Sampling Task** ✅ COMPLETE
   - Sample all 8 ADC channels
   - 1000 Hz sampling rate
   - Queue samples to logging task
   - Calibration applied
   - _Location: main/base/adc_as1256.c (adc_sampling_task)_

5. **End-of-Burn Detection Algorithm** ✅ COMPLETE
   - Baseline thrust measurement (0.5s)
   - 5% threshold detection
   - 5-second confirmation window
   - Post-burn logging period
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
- **Flash Backup Logging** (`feature-flashbackup`) - Flash memory backup logging for SD card failure recovery

---

## Flash Backup Logging Feature - Resource Analysis

### Feature Description
Add flash memory as a backup storage medium for test data in case SD card fails or is removed. This provides redundancy for critical test data.

### ESP32-S3-N16R8 Resource Analysis

**Hardware Specifications:**
- Flash: 16MB (128 Mbit)
- PSRAM: 8MB
- SRAM: 512KB
- CPU: 240MHz dual-core (Xtensa LX7)
- SPI: Two SPI buses (SPI2 for ADC, SPI3 for SD card via SDMMC)

**Data Volume at Different Sample Rates:**
| Sample Rate | Data Size (60s test) | Notes |
|-------------|---------------------|-------|
| 10 Hz | ~9.6 KB | Current default |
| 100 Hz | ~96 KB | Configurable |
| 1000 Hz | ~960 KB (~1 MB) | Max design spec |

**Data per sample:**
- 8 channels × 2 bytes (int16) = 16 bytes
- Timestamp (int64) = 8 bytes
- Total = 24 bytes per sample
- Plus CSV overhead (commas, newline) ≈ 32 bytes/sample

### Implementation Options

#### Option 1: Concurrent Flash + SD Writing (Redundancy Mode)
Write to both flash and SD card simultaneously during test.

**Pros:**
- Maximum redundancy - automatic failover
- No post-test copy delay
- True parallel storage

**Cons:**
- SPI bus contention (SDMMC uses SPI3, flash uses instruction cache)
- Higher CPU overhead during critical test period
- Increased flash wear (10K-100K erase cycles)
- More complex error handling

**Feasibility Assessment:**
- ✅ **RAM:** PSRAM (8MB) can easily buffer writes
- ⚠️ **CPU:** At 10 Hz - easily feasible; At 1000 Hz - may impact ADC sampling
- ⚠️ **Flash Wear:** Daily tests = ~3650/year - exceeds 10K cycle rating in 3 years
- ⚠️ **SPI Bus:** SDMMC (SD card) and instruction cache (flash) share internal bus

**Recommendation:** Not recommended for frequent use. Flash endurance is the limiting factor.

#### Option 2: Flash-Primary, Copy-to-SD After Test (Recommended)
Write to flash during test, then copy to SD card in ENDTEST state.

**Pros:**
- Minimal SPI contention during test (no SD card writes)
- Lower CPU overhead during critical sampling
- Reduced flash wear (only erase when copying)
- Simpler error recovery
- Better for high sample rates (100-1000 Hz)

**Cons:**
- Data at risk if system crashes before copy completes
- Post-test delay for file copy (~1-10 seconds depending on size)
- Requires sufficient flash partition space

**Feasibility Assessment:**
- ✅ **Flash Space:** 16MB easily holds hundreds of tests even at 1000 Hz
- ✅ **CPU:** Single write path - minimal overhead
- ✅ **Flash Wear:** Erase once per test (vs. continuous wear in concurrent mode)
- ✅ **Copy Speed:** 100-500 KB/s flash read → 1-10 MB/s SD write = 1-10s for 1MB

**Recommendation:** ✅ **RECOMMENDED** - Best balance of reliability, performance, and flash longevity.

#### Option 3: Flash Fallback Only
Write to SD card during test; automatically switch to flash if SD card fails.

**Pros:**
- Normal operation uses SD card (higher endurance)
- Flash only used on SD card failure
- No performance impact when SD card works

**Cons:**
- Most complex logic (runtime switching)
- Need to detect SD card failure mid-test
- Requires maintaining two write paths in hot code

**Feasibility Assessment:**
- ⚠️ **Complexity:** High - requires fault detection and graceful failover
- ✅ **Performance:** No impact when SD card working
- ⚠️ **Reliability:** Failure detection may not catch all SD card errors

**Recommendation:** Good for reliability, but higher implementation complexity.

### Partition Requirements

**Current partitions (from sdkconfig):**
- `app` (factory): ~1MB
- `otadata`: 8KB
- `phy_init`: 4KB

**Recommended new partition:**
```
# Add to partitions.csv
test_data,data,ota,0x300000,0xD00000,
```

This allocates 13MB for test data storage at offset 3MB, allowing:
- ~13,000 tests at 10 Hz
- ~1,300 tests at 100 Hz
- ~130 tests at 1000 Hz

### API Design (Draft)

```c
// Flash backup API
esp_err_t flash_backup_init(void);
esp_err_t flash_backup_start_test(uint32_t test_id);
esp_err_t flash_backup_write_sample(const adc_sample_t *sample);
esp_err_t flash_backup_finish_test(void);
esp_err_t flash_backup_copy_to_sd(void);  // Copy to SD card
esp_err_t flash_backup_erase_old(void);   // Erase old data
```

### Settings File Additions

```
# Flash Backup Settings (Phase 5)
FLASH_BACKUP_MODE          normal    # normal, fallback, concurrent
FLASH_BACKUP_AUTO_COPY     true      # Auto-copy to SD after test
FLASH_BACKUP_KEEP_DAYS     7         # Keep flash backups for N days
FLASH_BACKUP_MIN_SPACE_MB  5         # Minimum free space before warning
```

### Implementation Priority

**Phase 5.1 (Recommended First Step):**
1. Implement Option 2 (Flash-Primary, Copy-to-SD)
2. Add custom partition for test data
3. Implement basic API
4. Add settings configuration
5. Integration with ENDTEST state

**Phase 5.2 (Enhancement):**
1. Implement Option 3 (Flash Fallback)
2. Add SD card health monitoring
3. Runtime switching logic

**Phase 5.3 (Advanced - Not Recommended):**
1. Implement Option 1 (Concurrent Writing)
2. Only if specific use-case requires
3. Add flash wear monitoring
4. Consider industrial-grade flash with higher endurance

### Conclusion

**Recommended Approach:** Option 2 (Write to flash during test, copy to SD after)

**Justification:**
- ESP32-S3 has sufficient flash (16MB) and PSRAM (8MB) for buffering
- Minimal impact on critical ADC sampling task (P7 priority)
- Flash wear is manageable with erase-on-copy strategy
- 1-10 second post-test copy delay is acceptable
- Simpler implementation, higher reliability

**Not Recommended:** Concurrent flash + SD writing due to flash endurance limitations and SPI bus contention.

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
