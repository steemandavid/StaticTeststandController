# StaticTeststandController

Dual-unit embedded system for rocket motor static testing (ESP32-S3 + ESP-IDF).

## Overview

StaticTeststandController provides a safe, reliable system for static testing rocket motors with:
- Real-time data acquisition at 1000 Hz (AS1256 24-bit ADC)
- Remote wireless control via ESP-NOW
- Comprehensive data logging to SD card (CSV format)
- Safety interlocks, watchdog, and emergency halt
- Calibration modes for all sensors

## Architecture

**Dual-unit design:**
- **BASE** - Data acquisition, logging, igniter control, state machine
- **REMOTE** - Wireless controller with OLED display, buttons, switches

**Hardware:** ESP32-S3-N16R8 (16MB Flash, 8MB PSRAM)

## Project Structure

```
StaticTeststandController/
├── CMakeLists.txt          # Top-level project config
├── main/
│   ├── CMakeLists.txt      # Component config (BASE/REMOTE conditional)
│   ├── main.c              # Entry point dispatcher
│   ├── config.h            # Pin definitions & constants
│   ├── base/               # BASE unit firmware
│   │   ├── base_main.c     # BASE initialization
│   │   ├── state_machine.c/h
│   │   ├── adc_as1256.c/h  # 24-bit ADC driver
│   │   ├── sd_logger.c/h   # SD card logging
│   │   ├── rtc_ds1307.c/h  # Real-time clock
│   │   ├── settings.c/h    # Settings file parser
│   │   └── igniter.c/h     # Igniter FET control
│   ├── remote/             # REMOTE unit firmware
│   │   ├── remote_main.c   # REMOTE initialization
│   │   ├── display_ssd1306.c/h  # OLED display
│   │   ├── input_handler.c/h    # Buttons & switches
│   │   └── battery_monitor.c/h  # LiPo monitoring
│   └── common/             # Shared between units
│       ├── esp_now_protocol.c/h  # Communication
│       ├── rgb_led.c/h     # WS2812 LED control
│       └── safety.c/h      # Safety interlocks
└── docs/
    ├── SETUP.md
    ├── OPERATION.md
    └── CALIBRATION.md
```

## Building

Requires ESP-IDF v5.0+ installed and configured.

```bash
# Build for BASE unit
idf.py -D BUILD_TARGET=BASE build

# Build for REMOTE unit
idf.py -D BUILD_TARGET=REMOTE build

# Flash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Development Phases

1. **Phase 1: MVP** - Communication, state machine, basic I/O
2. **Phase 2: ADC + Logging** - Data acquisition, SD card, settings
3. **Phase 3: Time Sync** - WiFi, NTP, RTC integration
4. **Phase 4: Calibration** - Sensor calibration modes

## License

MIT
