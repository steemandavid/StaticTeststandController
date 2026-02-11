# Changelog

All notable changes to the Static Test Stand Controller firmware will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - 2026-02-11

### Added - Phase 2: ADC + SD Card Logging
- **AS1256 ADC Driver** - Complete 24-bit ADC implementation (539 lines)
  - SPI communication with ADS1256/AS1256 via SPI2_HOST (GPIO 35-40)
  - 8-channel single-ended input reading at up to 1000 Hz
  - DRDY interrupt handling for efficient data ready signaling
  - Configurable data rate (5-30000 SPS)
  - Self-calibration on startup
  - Per-channel calibration value support
  - Configurable port assignments
  - High-speed sampling FreeRTOS task (1000 Hz default)

- **SD Card Logger** - Complete FAT32 SD card logging (318 lines)
  - SD card mounting via SPI (GPIO 10-13)
  - CSV file generation with headers
  - Timestamped file naming (TEST_YYYYMMDD_HHMMSS.csv)
  - 1000 Hz write capability with buffering
  - Test summary generation (duration, max thrust, impulse, max pressure)
  - Sample count tracking
  - Periodic flushing every 100 samples for data safety
  - Mutex-protected access for thread safety

- **Settings Parser** - Complete settings.txt file parser (326 lines)
  - Parse settings from SD card root directory
  - Key-value pair format: `KEY VALUE # comment`
  - Comment support (# and // styles)
  - Comprehensive validation of all settings
  - Default values for missing settings
  - Graceful handling of missing/corrupt files (continue with defaults)

- **BASE Main Integration** - Phase 2 components integrated
  - SD card initialization with graceful degradation
  - Settings loading with automatic application to ADC
  - ADC initialization with DRDY interrupt
  - ADC sampling task creation (Priority 7)
  - SD logging task creation (Priority 6)

### Changed
- Version bumped to 1.2.0 for Phase 2 feature release
- Documentation updated (TODO.md, IMPLEMENTATION_STATUS.md)
- GPIO map updated to show Phase 2 pins as implemented

### Technical Notes
- ADC and SD card share SPI2_HOST but use different chip select pins
- DRDY semaphore used for efficient interrupt-driven sampling
- Settings provide default values if SD card not present
- All Phase 2 features gracefully degrade if hardware not available

## [Unreleased]

### Fixed
- **Button LED GPIO Configuration** - Fixed button LED (GPIO 17) not being configured as output
  - Added gpio_config() call to button_led_init()
  - Button LED now properly toggles on/off during tests
  - Fixes issue where button LED test always showed as ON

- **Test Script GPIO READ Bug** - Fixed test script not sending TEST prefix for GPIO commands
  - Removed incorrect condition that skipped "TEST " prefix for GPIO commands
  - All GPIO READ/WRITE commands now properly prefixed with "TEST "
  - Fixes issue where GPIO diagnostic showed -1 (ERROR) for all pins

- **Interactive Test Script Improvements** - Enhanced test clarity and user experience
  - Button press tests now show clear two-step process: "Press Enter to START" → "Press button NOW"
  - Button LED test uses OFF→ON blink sequence for clear visibility
  - Added GPIO diagnostic section showing real-time pin states
  - Added polling debug output during button tests
  - Fixed ask_user() invalid parameter bug

### Added
- **JSON Test Results Output** - Both test scripts now output detailed JSON results
  - `interactive_test.py`: Creates `test_results/test_run_YYYYMMDD_HHMMSS.json`
  - `diagnostic.py`: Creates `test_results/diagnostic_YYYYMMDD_HHMMSS.json`
  - Each test result includes timestamp, duration, full device response
  - Final summary includes totals, category breakdown, success rate
  - Enables automated analysis and learning from test runs

### Changed
- Test results are now saved to both `.log` and `.json` files
- Test script displays file paths at completion for easy access

## [1.0.54] - 2026-02-10

### Fixed
- **State Transition Testing** - Fixed state transitions failing during automated tests
  - Added test mode flag to prevent automatic HALT transitions during testing
  - State machine now continuously drains events when in test mode
  - Added TEST_MODE ON/OFF commands to test protocol
  - All 7/7 state transitions now passing in automated tests

### Added
- **Automated State Broadcast Verification** - State broadcast test now fully automated
  - Added BASE_STATE command to REMOTE to query received BASE state
  - Test automatically compares BASE state with REMOTE's received state
  - Eliminates need for visual verification during automated testing

### Changed
- State broadcast test no longer requires manual visual verification
- WELCOME state skipped in transition tests (auto-transitions to IDLE by design)

## [1.0.53] - 2026-02-08

### Fixed
- **BASE Unit Crash Fix** - Fixed critical crash caused by stack overflow and task starvation in test protocol task
  - Increased test_protocol_task stack size from 4KB to 16KB
  - Increased task priority from 1 (lowest) to 3 (same as buzzer/display tasks)
  - Fixes issue where BASE would stop responding after LED/buzzer tests

- **Buzzer PWM Tone Generation** - Implemented proper PWM-based buzzer using LEDC driver
  - Changed from GPIO toggling to 4000Hz PWM with 50% duty cycle
  - Fixed buzzer not turning off after tests (added `ledc_stop()`)
  - Buzzer now works with tone() function like Arduino reference

- **Safe State GPIO Test** - Fixed incorrect GPIO pin numbers
  - Changed from GPIO 40 & 41 to correct pins: GPIO 4 (power) & GPIO 41 (control)
  - Test now verifies igniter safety properly

- **Interactive Test Improvements**
  - Fixed logging - log files now flush immediately after each write
  - Added firmware verification on connection (detects wrong firmware)
  - Increased button/switch test timeouts from 10-30s to 50s
  - Fixed LED test to restore state color after testing
  - Improved error messages with possible causes

- **Test Protocol Enhancements**
  - Added LED STATE command to restore LED to current state color
  - Fixed ESP-IDF v5.5 compatibility (clk_src → clk_cfg)

### Added
- **Firmware Verification** - Automatic firmware type verification on connection
  - Detects if BASE/REMOTE firmware is on wrong unit
  - Shows clear warning message if firmware mismatch detected

- **Diagnostic Tool** - Created `scripts/diagnostic.py`
  - Quick health check for both units
  - Tests PING, INFO, TASKS commands
  - Helps identify crashed units

- **Parallel Build/Flash Script** - Created `scripts/build_and_flash_all.sh`
  - Builds BASE and REMOTE in parallel
  - Flashes both units simultaneously
  - Returns error if either fails

### Changed
- Test protocol task now runs at priority 3 (was priority 1)
- Test protocol task stack size increased to 16384 bytes (was 4096)

## [1.0.0 - 1.0.52] - 2026-02-01 to 2026-02-07

### Added
- Initial implementation of BASE and REMOTE firmware
- ESP-NOW communication protocol
- State machine with 13 states (INIT, IDLE, ARMED, HALT, etc.)
- RGB LED controller with WS2812 Neopixel support
- OLED display driver (SSD1306) for REMOTE unit
- Buzzer controller with PWM tone generation
- Input handler for button and switch inputs
- Safety watchdog with automatic safe state entry
- Serial test protocol for automated/interactive testing
- SD card logging for BASE unit
- ADC support for AS1256 ADC
- RTC support (DS1307)
- Battery monitoring for REMOTE unit

### Architecture
- **BASE Unit**: Data acquisition, state management, safety control
  - Reads pressure from AS1256 ADC
  - Logs data to SD card
  - Manages igniter control
  - State machine for test sequences

- **REMOTE Unit**: User interface and control
  - OLED display for status
  - Button for ignition control
  - Arm/Safe switch
  - Sends commands to BASE via ESP-NOW

- **Communication**: ESP-NOW protocol between BASE and REMOTE
  - Bidirectional command passing
  - Heartbeat monitoring
  - Automatic watchdog on communication loss

---

## Version Format

Version numbers follow Semantic Versioning: MAJOR.MINOR.PATCH-BUILD

- **MAJOR**: Breaking changes, architecture changes
- **MINOR**: New features, backward-compatible changes
- **PATCH**: Bug fixes, small improvements
- **BUILD**: Incremented on each build (0-65535)

## Firmware Targets

- **BASE**: Data acquisition and control unit
- **REMOTE**: User interface and control unit
