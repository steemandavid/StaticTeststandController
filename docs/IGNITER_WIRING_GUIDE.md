# Igniter Firing Circuit - Wiring Guide

This guide explains step-by-step how to connect each component of the igniter firing circuit.

---

## Components Needed

| Component | Quantity | Part Number Example |
|-----------|----------|---------------------|
| N-Channel MOSFET (Q1) | 1 | IRLZ44N or IRLZ34N |
| P-Channel MOSFET (Q2) | 1 | IRF9540 or IRLML6401 |
| Resistor (R1) | 1 | 10kΩ (1/4 watt) |
| Key Switch | 1 | SPST key-lock switch (maintained) |
| Igniter Connector | 1 | 2-pin connector matching your igniter |
| Wire | Various | 18-22 AWG for power, 24-26 AWG for signals |

---

## MOSFET Pin Identification

### IRLZ44N (N-Channel) - Q1
```
     Front (facing you with text)
    ┌─────────────────────┐
    │  ┌───┐   ┌───┐      │
    │  │ G │   │ D │      │   G = Gate (connect to GPIO 41)
    │  └───┘   └───┘      │   D = Drain (connect to igniter negative)
    │                     │   S = Source (connect to GND)
    │       ┌───┐         │   Tab = Drain (also connect to igniter negative)
    │       │ S │         │
    │       └───┘         │
    └─────────────────────┘
```

### IRF9540 (P-Channel) - Q2
```
     Front (facing you with text)
    ┌─────────────────────┐
    │  ┌───┐   ┌───┐      │
    │  │ G │   │ D │      │   G = Gate (connect to GPIO 10)
    │  └───┘   └───┘      │   D = Drain (connect to resistor and ADC)
    │                     │   S = Source (connect to igniter negative)
    │       ┌───┐         │   Tab = Drain (also connect to resistor and ADC)
    │       │ S │         │
    │       └───┘         │
    └─────────────────────┘
```

**Important: Always verify pinout with your specific MOSFET datasheet!**

---

## Step-by-Step Wiring Instructions

### STEP 1: Connect the Key Switch

The key switch goes in the **positive** battery lead.

```
BATTERY POSITIVE (+) ───────┐
                              │
                          ┌───┴───┐
                          │ KEY  │
                          │ SWITCH│
                          └───┬───┘
                              │
                              │    This wire becomes "IGNITER+" for the rest of circuit
                              │
                              ▼
                        (to igniter connector positive)
```

**What to do:**
1. Cut the positive wire from your battery
2. Connect one end to one terminal of the key switch
3. The other terminal becomes your switched positive supply (call it "IGNITER+")

---

### STEP 2: Connect Q1 (N-Channel MOSFET - Main Firing FET)

This FET switches the high current to fire the igniter.

```
ESP32-S3 GPIO 41 ───────┐
                        │
                    ┌───┴─────┐
                    │  Gate   │
                    │   (G)   │
                    └─────────┘
                        │
                    ┌───┴─────────────────────────┐
                    │                             │
                ┌───┴────┐                    ┌───▼───┐
     (from igniter │   D   │                    │ Source│
      connector -) │   (D) │                    │  (S)  │
                └───┬────┘                    └───┬───┘
                    │                             │
                    │                    ┌────────┴────────┐
                    │                    │                 │
                    │                 GND              GND
                    │              (ESP32 GND)      (Battery GND)
```

**What to do:**
1. **Gate (G)**: Connect to ESP32 GPIO 41 using 24-26 AWG wire
2. **Drain (D)**: Connect to igniter connector negative terminal (use 18-20 AWG for high current)
3. **Source (S)**: Connect to ground (both ESP32 GND and battery negative)
4. **Metal tab**: Also connect to ground (it's connected to Drain internally)

---

### STEP 3: Connect Q2 (P-Channel MOSFET - Safety/Continuity FET)

This FET is used for continuity checking only.

```
ESP32-S3 GPIO 10 ───────┐
                        │
                    ┌───┴─────┐
                    │  Gate   │
                    │   (G)   │
                    └─────────┘
                        │
                    ┌───┴────────────────────────────────┐
                    │                                    │
                ┌───▼───┐                            ┌───┴────┐
                │ Source│                            │  Drain │
                │  (S)  │                            │   (D)  │
                └───┬───┘                            └───┬────┘
                    │                                    │
                    │                                    │
      (from igniter                   ┌─────────┐    ┌───▼─────┐
       connector -)                   │  R1     │    │ GPIO 4  │
                    └──────────────────│ 10kΩ    │────│ (ADC)   │
                                       └─────────┘    └─────────┘
```

**What to do:**
1. **Gate (G)**: Connect to ESP32 GPIO 10 using 24-26 AWG wire
2. **Source (S)**: Connect to the same point as Q1's Drain (igniter connector negative)
3. **Drain (D)**: Connect to one end of the 10kΩ resistor
4. **Metal tab**: Also connect to the resistor (same as Drain)

---

### STEP 4: Connect the Resistor (R1 - 10kΩ)

This resistor limits the continuity check current to a safe level.

```
               From Q2 Drain (D)
                    │
                    │
              ┌─────┴─────┐
              │   R1      │
              │   10kΩ    │
              └─────┬─────┘
                    │
         ┌──────────┴──────────┐
         │                     │
    ┌────▼────┐          ┌─────▼─────┐
    │ GPIO 4  │          │ 3.3V      │
    │ (ADC)   │          │ (from ESP32)│
    └─────────┘          └───────────┘
```

**What to do:**
1. **One end**: Connect to Q2's Drain (already done in step 3)
2. **Other end**: Connect to ESP32 3.3V output pin
3. **Middle point**: The connection between Q2 Drain and the resistor goes to ESP32 GPIO 4 (ADC input)

---

### STEP 5: Connect the Igniter Connector

This is where you plug in your actual igniter.

```
         Positive Side              Negative Side
              │                          │
              ▼                          ▼
     ┌────────────────┐         ┌────────────────┐
     │   Connector    │         │   Connector    │
     │     Pin 1      │         │     Pin 2      │
     │   (IGNITER+)   │         │   (IGNITER-)   │
     └────────┬───────┘         └────────┬───────┘
              │                          │
              │                          │
     ┌────────▼───────┐         ┌────────▼──────────────────────┐
     │ From Key Switch│         │ From Q1 Drain (D) AND Q2 Source (S)│
     └────────────────┘         └─────────────────────────────────┘
```

**What to do:**
1. **Positive terminal**: Connect to the output side of the key switch
2. **Negative terminal**: Connect to BOTH Q1's Drain AND Q2's Source
   - Use a wire nut or terminal block to join these three wires together:
     - Igniter connector negative
     - Q1 Drain
     - Q2 Source

---

### STEP 6: Ground Connections

All ground connections must be tied together.

```
                    ┌─────────────────────────┐
                    │                         │
         ┌──────────▼──────────┐   ┌─────────▼──────────┐
         │   ESP32-S3 GND      │   │   Battery Negative │
         │   (any GND pin)     │   │   (-) terminal     │
         └──────────┬──────────┘   └─────────┬──────────┘
                    │                         │
                    │    ┌─────────────┐      │
                    └────┤  Main GND   ├──────┘
                         │  Junction  │
                         │  (star     │
                         │   point)  │
                         └─────┬──────┘
                               │
              ┌────────────────┴────────────────┐
              │                                │
         ┌────▼─────┐                   ┌──────▼──────┐
         │ Q1 Source│                   │ Q2 Gate     │
         │  (S)     │                   │ (via 10kΩ   │
         └──────────┘                   │  pulldown)  │
                                        └─────────────┘
```

**What to do:**
1. Create a common ground point (use a terminal block or wire nut)
2. Connect to this point:
   - ESP32-S3 GND pin (use the GND pins closest to the MOSFETs)
   - Battery negative terminal
   - Q1 Source pin
   - Q2 Gate (via a 10kΩ pulldown resistor - see note below)

**IMPORTANT:** Add a 10kΩ pulldown resistor from Q2 Gate to ground. This keeps Q2 OFF when GPIO 10 is not actively driven.

---

## Complete Wiring Summary

### ESP32-S3 Connections
| ESP32 Pin | Connect To | Wire Gauge |
|-----------|------------|------------|
| GPIO 41 | Q1 Gate (N-FET) | 24-26 AWG |
| GPIO 10 | Q2 Gate (P-FET) | 24-26 AWG |
| GPIO 4 | Connection between Q2 Drain and R1 | 24-26 AWG |
| 3.3V | Other end of R1 (10kΩ resistor) | 24-26 AWG |
| GND | Q1 Source, Battery negative, Q2 Gate pulldown | 18-22 AWG |

### Q1 (N-Channel MOSFET) Connections
| Pin | Connect To |
|-----|------------|
| Gate | GPIO 41 |
| Drain | Igniter connector negative (also Q2 Source) |
| Source | Ground |

### Q2 (P-Channel MOSFET) Connections
| Pin | Connect To |
|-----|------------|
| Gate | GPIO 10 (via 10kΩ resistor to ground for pulldown) |
| Source | Igniter connector negative (also Q1 Drain) |
| Drain | One end of 10kΩ resistor (R1) and GPIO 4 (ADC) |

### Resistor R1 (10kΩ) Connections
| End | Connect To |
|-----|------------|
| End 1 | Q2 Drain and GPIO 4 (ADC) |
| End 2 | ESP32 3.3V |

### Key Switch Connections
| Terminal | Connect To |
|----------|------------|
| Input | Battery positive |
| Output | Igniter connector positive |

### Igniter Connector Connections
| Pin | Connect To |
|-----|------------|
| Positive | Key switch output |
| Negative | Q1 Drain AND Q2 Source (join together) |

---

## Additional Components Needed

### Gate Pulldown for Q2 (P-Channel MOSFET)

P-channel MOSFETs need a pulldown resistor to keep them OFF when not actively driven.

```
GPIO 10 ────────┐
               │
           ┌───┴────┐
           │  10kΩ  │  (pulldown resistor)
           └───┬────┘
               │
              GND
```

**What to do:**
- Add a 10kΩ resistor between GPIO 10 and ground
- This ensures Q2 stays OFF when the ESP32 starts up (GPIOs are floating before initialization)

### Gate Resistor for Q1 (Optional but Recommended)

Adding a small resistor in series with the gate prevents oscillation.

```
GPIO 41 ───────┬──── 100Ω ────┬──── Q1 Gate
               │              │
            10kΩ          (gate stopper)
               │
              GND      (pulldown)
```

**What to do:**
- Add 100Ω resistor in series with GPIO 41 and Q1 Gate
- Add 10kΩ resistor from Q1 Gate to ground

---

## Wiring Diagram (Text-Based)

```
                            ┌─────────────────┐
                            │    Battery      │
                            │                 │
                            │    (+)  (-)     │
                            │      │   │      │
                            └──────┬───┴──────┘
                                   │
                          (positive wire)
                                   │
                            ┌──────▼──────┐
                            │  KEY SWITCH │
                            └──────┬──────┘
                                   │
                          (switched positive)
                                   │
                    ┌──────────────▼────────────────┐
                    │                                │
              ┌─────▼─────┐                   ┌─────▼────────────────────┐
              │ IGNITER+  │                   │                          │
              │ (connector)│              ┌────▼────┐              ┌─────▼─────┐
              └─────┬─────┘              │ IGNITER-│              │           │
                    │                    │(connector)             │    Q1     │
                    │                    └────┬────┘             │  (N-FET)   │
                    │                         │                 │           │
                    │         ┌───────────────┼─────────────────┤── SOURCE   │
                    │         │               │                 │           │
                    │         │               │          ┌──────┤-- DRAIN    │
                    │         │               │          │      │           │
                    │         │               │          │      └─────┬─────┘
                    │         │               │          │            │
                    │    ┌────▼─────┐   ┌────▼──────┐   │       ┌────▼────┐
                    │    │    Q2    │   │   Q1      │   │       │  GATE   │
                    │    │ (P-FET)  │   │ (N-FET)   │   │       └────┬────┘
                    │    │          │   │          │   │            │
                    │    │  SOURCE  │   │  DRAIN    │   │       ┌────▼────┐
                    │    └────┬─────┘   └────┬─────┘   │       │ GPIO 41 │
                    │         │              │       │       └─────────┘
                    │    ┌────▼─────┐   ┌────▼─────┐  │
                    │    │  GATE    │   │  SOURCE  │  │
                    │    └────┬─────┘   └────┬─────┘  │
                    │         │              │       │
                    │    ┌────▼─────┐        │       │
                    │    │ GPIO 10  │        │       │
                    │    └────┬─────┘        │       │
                    │         │              │       │
                    │    ┌────▼─────┐        │       │
                    │    │ 10kΩ     │        │       │
                    │    │pulldown  │        │       │
                    │    └────┬─────┘        │       │
                    │         │              │       │
                    │         └──────┬───────┘       │
                    │                │               │
                    │            ┌───▼───────┐       │
                    │            │    GND    │◄──────┘
                    │            └───────────┘
                    │
                    │    ┌─────┴───────────────────┐
                    │    │                         │
                    │    ▼                         ▼
              ┌─────▼─────┐                 ┌─────────────┐
              │    R1     │                 │    GND      │
              │   10kΩ    │                 │ (ESP32 and  │
              └─────┬─────┘                 │  Battery)   │
                    │                       └─────────────┘
                    │
              ┌─────┴─────────┐
              │               │
         ┌────▼────┐     ┌────▼────┐
         │ GPIO 4  │     │ 3.3V    │
         │ (ADC)   │     │ (ESP32)  │
         └─────────┘     └──────────┘
```

---

## Testing Checklist

Before connecting a real igniter:

- [ ] Key switch in OFF position
- [ ] Battery disconnected
- [ ] Verify all connections with multimeter (continuity mode)
- [ ] Check Q1: Gate to GPIO 41, Drain to igniter-, Source to GND
- [ ] Check Q2: Gate to GPIO 10, Source to igniter-, Drain to resistor
- [ ] Check resistor: One end to Q2 Drain, other to 3.3V
- [ ] Check ADC connection: GPIO 4 connected to Q2 Drain/resistor junction
- [ ] Check key switch: Battery+ → switch → igniter+
- [ ] Check all grounds connected together
- [ ] Test with dummy load (1-2Ω, 50W resistor) first

---

## Safety Reminders

1. **NEVER test with real igniter on workbench** - Use dummy load only
2. **Always disconnect battery when wiring**
3. **Double-check connections before applying power**
4. **Use appropriate wire gauge** for high-current paths (18-20 AWG)
5. **Keep MOSFETs away from heat sources** - consider heatsinks for high-current firing
6. **Add inline fuse** (5-10A) on battery positive for extra protection
