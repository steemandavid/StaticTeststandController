# Operation Guide

## System States

| State | Description |
|-------|-------------|
| INIT | System initialization (automatic) |
| IDLE | Ready, waiting for commands |
| ARMED | System armed, ready to fire |
| STARTTEST | Initializing test, creating files |
| IGNITION | Firing igniter |
| TESTRUNNING | Monitoring burn, logging data |
| ENDTEST | Finalizing, writing summary |
| HALT | Safe shutdown (requires power cycle) |

## Normal Test Procedure

1. **Power on** both BASE and REMOTE units
2. **Verify connection** - REMOTE displays BASE state and RSSI
3. **Pre-flight checks** (optional):
   - Press button while SAFE to check igniter continuity
   - Verify breakwire status
4. **Arm system** - Flip toggle switch to ARMED position
   - RGB LED changes from green breathing to orange solid
5. **Fire** - Long press (>=2s) the ignition button
   - System transitions: STARTTEST -> IGNITION -> TESTRUNNING
6. **Monitor** - Data logging at 1000 Hz
7. **End detection** - Automatic when thrust/pressure return to baseline
8. **Review** - System enters HALT, remove SD card to review CSV data

## Safety Features

- Cannot arm unless SWITCH_ARMED is active
- Cannot fire unless in ARMED state
- Long press required for ignition (prevents accidental fire)
- Auto-disarm on communication timeout
- Igniter auto-cutoff at 2x configured on-time
- Watchdog monitors state machine heartbeat
- Any critical error triggers HALT (safe shutdown)

## LED Status Indicators

| Color | Pattern | Meaning |
|-------|---------|---------|
| Green | Breathing | Safe/IDLE |
| Orange | Solid | Armed |
| Red | Blink 2Hz | Testing |
| Blue | Solid | Test complete |
| Red | Slow pulse | Error/HALT |
| Red | Rapid 5Hz | Switch error |

## Troubleshooting

- **No display on REMOTE**: Check I2C connections (SDA:8, SCL:9)
- **No communication**: Verify MAC addresses in config.h match hardware
- **HALT on boot**: Check SD card is inserted with valid settings.txt
- **Switch error**: Verify both switch positions read correctly (not both HIGH/LOW)
