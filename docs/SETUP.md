# Setup Guide

## Prerequisites

- ESP-IDF v5.0+ installed and configured
- Two ESP32-S3-N16R8 development boards
- USB cables for flashing

## Hardware Connections

### BASE Unit

| Component | GPIO | Notes |
|-----------|------|-------|
| ADS1256 ADC (SPI) | MOSI:35, SCLK:36, MISO:37, CS:39, RST:38, DRDY:40 | 24-bit ADC |
| SD Card (SPI) | MOSI:11, CLK:12, MISO:13, CS:10 | FAT32 formatted |
| DS1307 RTC (I2C) | SDA:8, SCL:9 | DS:3 | CR2032 backup battery |
| Igniter FET | GPIO 41 (control), GPIO 4 (power) | 19V, 2A max |
| WS2812 RGB LED | GPIO 48 | Status indicator |
| Buzzer | GPIO 42 | Audio feedback |

### REMOTE Unit

| Component | GPIO | Notes |
|-----------|------|-------|
| SSD1306 OLED (I2C) | SDA:8, SCL:9 | 128x64 pixels |
| Ignition Button | GPIO 16 (input), GPIO 17 (LED) | Illuminated pushbutton |
| Arm/Safe Switch | GPIO 4 (armed), GPIO 5 (safe) | Toggle, active low |
| Battery Monitor | GPIO 6 | ADC, voltage divider |
| WS2812 RGB LED | GPIO 48 | Status indicator |
| Buzzer | GPIO 42 | Audio feedback |

## Building Firmware

```bash
# Clone repository
git clone https://github.com/steemandavid/StaticTeststandController.git
cd StaticTeststandController

# Build for BASE unit
idf.py -D BUILD_TARGET=BASE build

# Build for REMOTE unit
idf.py -D BUILD_TARGET=REMOTE build

# Flash BASE unit
idf.py -p /dev/ttyUSB0 flash monitor

# Flash REMOTE unit
idf.py -p /dev/ttyUSB1 flash monitor
```

## SD Card Preparation

1. Format SD card as FAT32
2. Create `settings.txt` in root directory (see FSD Section 4.2 for format)
3. Insert into BASE unit SD card slot

## First Boot

1. Power on BASE unit - should enter INIT then IDLE state
2. Power on REMOTE unit - should connect via ESP-NOW
3. Verify communication: REMOTE display shows BASE state
4. Verify RSSI reading on REMOTE OLED
