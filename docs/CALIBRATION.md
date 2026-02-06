# Calibration Guide

## Overview

The StaticTeststandController supports calibration for:
- Load cell (thrust measurement)
- Pressure transducer
- Igniter continuity sensing
- Breakwire monitoring

## Entering Calibration Mode

1. Ensure system is in IDLE state with switch in SAFE position
2. Use button press combinations to enter calibration states
3. Real-time sensor values are displayed on REMOTE OLED

## Load Cell Calibration

### Procedure
1. Enter CALIBRATE_LOADCELL state
2. Remove all loads - record zero reading
3. Apply known weight (e.g., 10 kg)
4. Record loaded reading
5. Calculate calibration value: `cal = known_weight / (loaded_reading - zero_reading)`
6. Update `ADC_CAL_VALUE_LOADCELL` in settings.txt

### Notes
- Use certified calibration weights
- Perform calibration at operating temperature
- Verify linearity with multiple known weights

## Pressure Transducer Calibration

### Procedure
1. Enter CALIBRATE_PRESSURE state
2. Vent to atmosphere - record zero reading
3. Apply known pressure (e.g., 10 bar)
4. Record pressurized reading
5. Calculate calibration value: `cal = known_pressure / (pressure_reading - zero_reading)`
6. Update `ADC_CAL_VALUE_PRESSURE_TRANSDUCER` in settings.txt

### Notes
- Use a calibrated pressure source
- Verify at multiple pressure points
- Check for pressure leaks before calibration

## Igniter Continuity Check

### Procedure
1. Enter CHECK_IGNITER state
2. Connect igniter to terminals
3. System measures resistance via voltage divider
4. Display shows continuity status (open/closed/short)

### Expected Values
- Open circuit: ~0V (igniter not connected)
- Good igniter (0.5-2 ohm): Voltage proportional to resistance
- Short circuit: Maximum voltage (fault condition)

## Breakwire Check

### Procedure
1. Enter CHECK_BREAKWIRES state
2. System reads all 4 breakwire channels
3. Display shows status of each breakwire (intact/broken)

### Expected Values
- Intact wire: ~0V (closed circuit to ground)
- Broken wire: Pull-up voltage (~3.3V)

## Settings File Reference

All calibration values are stored in `settings.txt` on the SD card:

```
ADC_CAL_VALUE_LOADCELL 0.001234
ADC_CAL_VALUE_PRESSURE_TRANSDUCER 0.004567
ADC_CAL_VALUE_IGNITER 0.000806
ADC_CAL_VALUE_BREAKWIRE1 1.0
ADC_CAL_VALUE_BREAKWIRE2 1.0
ADC_CAL_VALUE_BREAKWIRE3 1.0
ADC_CAL_VALUE_BREAKWIRE4 1.0
```
