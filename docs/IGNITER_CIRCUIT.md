# Igniter Fire and Sense Circuit Design

## Overview

This document describes the safety-critical igniter firing and continuity sensing circuit for the Static Test Stand Controller. The circuit uses **multiple layers of protection** to prevent accidental firing.

## Safety Requirements

1. **No current flow unless explicitly commanded** - Ground must be disconnected by default
2. **Key switch override** - Physical switch enables/disables firing capability
3. **Continuity sensing without firing risk** - Must detect igniter presence using <1mA
4. **Software must detect key switch position** - Firmware should know if key switch is ON or OFF
5. **Configurable continuity threshold** - ADC threshold stored in `settings.txt`

## Circuit Schematic

```
                          ┌─────────────────────────────────────────┐
                          │               ESP32-S3                  │
                          │                                         │
BATTERY+ ────────┬────────│───3.3V (for ADC reference)             │
                │        │                                         │
        ┌───────┴───────┐│───GPIO 41 ────┬─────┬───── CONTINUITY_SENSE (ADC1)
        │  KEY SWITCH   │││              │     │     (GPIO 4 / ADC1_CH3)
        │   "ARMED"     │││           ┌─┴─┐   │
        │               │││           │R1 │   │    Continuity Check
        │      ON ──────┼┼┼───┐       │10k│   │    Path (low current)
        │      OFF ─────┼┼┼───┘       └───┘   │
        └───────┬───────┘││              │     │
                │        ││              │     │
                │        ││         GPIO 10    │
                │        ││      (SAFETY_FET)  │
                │        ││              │     │
                │        ││           ┌──┴──┐  │
                │        ││           │ Q2  ├──┘
                │        ││           │ P-  │
                │        ││           │ MOS │
                │        ││           │ FET │
                │        ││           └─────┘
                │        ││            │
                │        ││        GND (during check)
                │        ││
                │        ││         GPIO 41
                │        │││      (IGNITER_FET)
                │        │││           │
                │        │││        ┌──┴───┐
                │        │││        │  Q1  │
                │        │││        │ N-   │───────┐
                │        │││        │ MOS  │       │
                │        │││        │ FET  │       │
                │        │││        └──────┘       │
                │        │││         │             │
                │        │││      ┌──┴───────┐     │
                │        │└┴──────┤ IGNITER  │     │
                │        └────────┤   +      │     │
                │                 │          │     │
                └─────────────────┤   -      ┬─────┘
                                  └──────────┘
                                           │
                                          GND
```

## Circuit Components

| Component | Value/Part | Purpose |
|-----------|------------|---------|
| **Q1** - Control FET | N-Channel MOSFET (e.g., IRLZ44N) | Main igniter current switching |
| **Q2** - Safety FET | P-Channel MOSFET (e.g., IRF9540) | Low-side disconnect for continuity check |
| **R1** - Pull-up | 10kΩ ±1% | Pull-up for continuity sensing |
| **Key Switch** | SPST key-lock switch | Manual enable/disable of ignition circuit |
| **ADC Input** | GPIO 4 (ADC1_CH3) | Continuity voltage measurement |

## GPIO Pin Assignments

| GPIO | Function | Direction | Description |
|------|----------|-----------|-------------|
| GPIO 4 | ADC1_CH3 | Input | Continuity sense voltage |
| GPIO 10 | SAFETY_FET | Output | Controls Q2 (low-side FET) |
| GPIO 41 | IGNITER_FET | Output | Controls Q1 (main firing FET) |

## Operating Modes

### Mode 1: Normal (Safe) Operation
- **Key Switch:** OFF
- **GPIO 10 (SAFETY_FET):** HIGH (Q2 OFF)
- **GPIO 41 (IGNITER_FET):** LOW (Q1 OFF)
- **Result:** No current can flow through igniter, regardless of software state

### Mode 2: Armed (Ready to Fire)
- **Key Switch:** ON (battery+ connected to igniter+)
- **GPIO 10 (SAFETY_FET):** HIGH (Q2 OFF - safety disconnect)
- **GPIO 41 (IGNITER_FET):** LOW (Q1 OFF)
- **Result:** Igniter powered but no current path to ground - safe to handle

### Mode 3: Continuity Check
- **Key Switch:** Any position
- **GPIO 10 (SAFETY_FET):** LOW (Q2 ON - connects R1 to igniter-)
- **GPIO 41 (IGNITER_FET):** LOW (Q1 OFF)
- **Result:** Small test current flows: Battery+ → Key → Igniter → Q2 → R1 → 3.3V
  - **Good igniter (low resistance):** ADC reads ~3.3V (near VCC)
  - **Open/broken igniter:** ADC reads ~0V (no connection)
  - **Test current:** ≈ 0.33mA maximum (safe)

### Mode 4: Firing
- **Key Switch:** MUST BE ON
- **GPIO 10 (SAFETY_FET):** HIGH (Q2 OFF - disconnect R1)
- **GPIO 41 (IGNITER_FET):** HIGH (Q1 ON - connects igniter- to ground)
- **Result:** Full battery current flows through igniter
  - Path: Battery+ → Key → Igniter → Q1 → GND
  - Q2 is OFF so R1 is not in the high-current path

## Continuity Detection Logic

The ADC reading on GPIO 4 determines igniter status:

| ADC Reading | Voltage | Igniter Status |
|-------------|---------|----------------|
| > ~4000 (12-bit) | > ~3.3V | **Continuity OK** (low resistance) |
| < ~1000 | < ~0.8V | **Open** (no igniter or broken) |
| 1000-4000 | 0.8-3.3V | **Poor contact** (high resistance) |

**Configurable Threshold (`settings.txt`):**
```
igniter.continuity_threshold_adc=4000
```

## Key Switch Detection

The firmware detects key switch position during continuity check:

1. **Key OFF:** Even with GPIO 10 LOW, ADC reads 0V (no battery+ connection)
2. **Key ON:** With GPIO 10 LOW, ADC reads high if igniter present

Detection logic:
```
if (continuity_check_succeeded) {
    // Key must be ON AND igniter connected
    key_switch_state = ON;
} else {
    // Either key is OFF or igniter not connected
    // To distinguish, check if ADC reads exactly 0V
    if (adc_reading < 100) {
        key_switch_state = UNKNOWN;  // Could be OFF or missing igniter
    } else {
        key_switch_state = OFF;      // Key OFF detected
    }
}
```

## MOSFET Selection

### Q1 - Main Control FET (N-Channel)
Requirements:
- **Vds:** > Battery voltage (e.g., 20V+ for 12V battery)
- **Id:** > Igniter current (e.g., >30A for typical igniters)
- **Rds(on):** < 10mΩ (minimize voltage drop)
- **Vgs:** Logic-level (fully ON at 3.3V)

**Recommended:** IRLZ44N, IRLZ34N, IRLB8743

### Q2 - Safety FET (P-Channel)
Requirements:
- **Vds:** > Battery voltage
- **Id:** > 10mA (only handles continuity check current)
- **Vgs:** Logic-level (fully ON at 3.3V gate drive)
- **Rds(on):** < 100mΩ (not critical for low current)

**Recommended:** IRF9540, IRLML6401, Si7461DP

## Safety Features

1. **Two-FET Safety:**
   - Q2 must be OFF (GPIO 10 HIGH) before Q1 can turn ON for firing
   - Continuity check uses separate path through R1

2. **Key Switch Physical Disconnect:**
   - Completely removes battery+ from igniter when OFF
   - Software can detect key state

3. **Low-Current Continuity Check:**
   - Maximum 0.33mA through igniter
   - Cannot fire igniter even if defective

4. **Software Safety:**
   - FET control requires specific state sequence (IDLE → ARMED → STARTTEST → IGNITION)
   - State machine prevents firing from wrong states
   - Watchdog forces safe state on timeout

## Firmware Implementation

### GPIO Configuration

```c
// In config.h or pins.h
#define GPIO_IGNITER_CONTROL    41   // Main firing FET (N-channel)
#define GPIO_IGNITER_SAFETY     10   // Continuity check FET (P-channel)
#define GPIO_IGNITER_SENSE      4    // ADC1 channel 3

// ADC configuration
#define ADC_UNIT                ADC_UNIT_1
#define ADC_CHANNEL             ADC_CHANNEL_3  // GPIO 4
#define ADC_ATTENUATION         ADC_ATTEN_DB_11  // 0-3.3V range
```

### Continuity Check Function

```c
typedef enum {
    IGNITER_CONTINUITY_OK,
    IGNITER_OPEN,
    IGNITER_SHORT,
    IGNITER_KEY_OFF,
    IGNITER_UNKNOWN
} igniter_status_t;

igniter_status_t igniter_check_continuity(void)
{
    // Enable continuity check path
    gpio_set_level(GPIO_IGNITER_SAFETY, 0);  // Turn ON Q2
    vTaskDelay(pdMS_TO_TICKS(10));  // Settle time

    // Read ADC
    int adc_raw = adc1_get_raw(ADC_CHANNEL);

    // Disable continuity check path
    gpio_set_level(GPIO_IGNITER_SAFETY, 1);  // Turn OFF Q2

    // Check against threshold (from settings.txt)
    int threshold = settings_get_int("igniter.continuity_threshold_adc", 4000);

    if (adc_raw > threshold) {
        return IGNITER_CONTINUITY_OK;
    } else if (adc_raw < 100) {
        // Very low reading - either key is OFF or completely open
        return IGNITER_KEY_OFF;
    } else {
        return IGNITER_OPEN;
    }
}
```

### Fire Igniter Function

```c
bool igniter_fire(uint32_t duration_ms)
{
    // Safety checks
    if (state_machine_get_state() != STATE_IGNITION) {
        ESP_LOGE(TAG, "Cannot fire: not in IGNITION state");
        return false;
    }

    // Check continuity first
    igniter_status_t status = igniter_check_continuity();
    if (status != IGNITER_CONTINUITY_OK) {
        ESP_LOGE(TAG, "Cannot fire: continuity check failed");
        return false;
    }

    // Ensure safety FET is OFF (disconnect R1 from circuit)
    gpio_set_level(GPIO_IGNITER_SAFETY, 1);

    // Fire sequence
    ESP_LOGW(TAG, "Firing igniter for %lu ms", duration_ms);
    gpio_set_level(GPIO_IGNITER_CONTROL, 1);  // Turn ON Q1

    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    gpio_set_level(GPIO_IGNITER_CONTROL, 0);  // Turn OFF Q1
    ESP_LOGI(TAG, "Igniter fired");

    return true;
}
```

## Wiring Diagram (Physical Connection)

```
ESP32-S3 Board                   Power Distribution
┌─────────────────┐              ┌──────────────────┐
│                 │              │                  │
│ GPIO 41 ────────┼──────────┐   │   BATTERY+ ──────┼────┐
│                 │          │   │                  │    │
│ GPIO 10 ────────┼──────┐   │   │                  │    │
│                 │      │   │   │                  │    │
│ GPIO 4 (ADC) ───┼──┐   │   │   │                  │    │
│                 │  │   │   │   │                  │    │
│ 3.3V ───────────┼──┼───┼───┼───┼─┐                │    │
│                 │  │   │   │   │ │                │    │
│ GND ────────────┼──┼───┼───┼───┼─┼────────┐       │    │
└─────────────────┘  │   │   │   │ │        │       │    │
                     │   │   │   │ │        │       │    │
                 ┌───┴──┐ │   │   │ │    ┌───┴───┐   │    │
                 │ Q1  │ │   │   │ │    │ KEY   │   │    │
                 │ Nch │ │   │   │ │    │ SWITCH│   │    │
                 │     │ │   │   │ │    └───┬───┘   │    │
                 └──┬───┘ │   │   │ │        │       │    │
                    │    │   │   │ │        └───┬─────┘    │
                 ┌───┴───┐│   │   │ │            │         │
                 │ Q2   ││   │   │ │        ┌───┴─────┐   │
                 │ Pch  ││   │   │ │        │ IGNITER │   │
                 └───┬──┘│   │   │ │        │ CONNECTOR│  │
                     │  │   │   │ │        └────┬────┘   │
                  ┌──┴──┐ │   │   │ │             │        │
                  │ R1  │ │   │   │ │             └────────┘
                  │ 10k │ │   │   │ │
                  └──┬──┘ │   │   │ │
                     │   │   │   │ │
                     └───┼───┼───┼─┼──────────────┘
                         │   │   │ │
                         └───┴───┴─┘
                         GND plane
```

## Testing Procedure

### 1. Continuity Test (No Igniter Connected)
- Key switch: OFF
- Run continuity check
- Expected: ADC reads 0V (KEY_OFF status)

### 2. Continuity Test (Igniter Connected, Key OFF)
- Connect dummy load (e.g., 1Ω resistor) to igniter connector
- Key switch: OFF
- Run continuity check
- Expected: ADC reads 0V (KEY_OFF status)

### 3. Continuity Test (Igniter Connected, Key ON)
- Dummy load connected
- Key switch: ON
- Run continuity check
- Expected: ADC reads > 3.0V (CONTINUITY_OK)

### 4. Firing Test (Dummy Load Only!)
- ⚠️ **NEVER test with real igniter on workbench!**
- Use high-power resistor (e.g., 1Ω, 50W) as dummy load
- Key switch: ON
- Verify continuity OK
- Execute fire command with short duration (e.g., 100ms)
- Expected: GPIO 41 goes HIGH for specified duration, then LOW

## Bill of Materials

| Qty | Component | Part Number | Notes |
|-----|-----------|-------------|-------|
| 1 | N-Channel MOSFET | IRLZ44N | Logic level, >30A |
| 1 | P-Channel MOSFET | IRF9540 | Logic level, >20V |
| 1 | Resistor | 10kΩ ±1% | 1/4W |
| 1 | Key Switch | SPST key-lock | Maintained contact |
| 1 | Igniter Connector | Compatible with igniter | High-current capable |
| 1 | PCB or perfboard | - | For mounting components |

## Safety Warnings

⚠️ **CRITICAL SAFETY NOTES:**

1. **Never test with real igniters on workbench** - Use dummy load only
2. **Always disconnect battery when working on circuit**
3. **Verify key switch operation before first use**
4. **Test continuity check with known resistive loads first**
5. **Keep flammable materials away from test area**
6. **Have fire extinguisher nearby during live tests**
7. **Never bypass safety features** (key switch, continuity check)
8. **Double-check all connections before applying power**

## Future Enhancements

1. **Current monitoring** - Add ACS712 or similar to measure actual firing current
2. **Voltage monitoring** - Monitor igniter voltage during firing
3. **Firing status feedback** - LED indicator when continuity OK
4. **Audible continuity indication** - Buzzer tone when igniter detected
5. **Data logging** - Log firing events to SD card with timestamp
