# User Manual — Multi-Channel Temperature Controller

## Table of Contents
1. [Getting Started](#1-getting-started)
2. [Hardware Setup](#2-hardware-setup)
3. [Web Dashboard](#3-web-dashboard)
4. [PID Configuration](#4-pid-configuration)
5. [Auto-Tuning](#5-auto-tuning)
6. [Modbus Interface](#6-modbus-interface)
7. [Safety & Alarms](#7-safety--alarms)
8. [Troubleshooting](#8-troubleshooting)

---

## 1. Getting Started

### Power-On Sequence
1. Apply 24V DC power to the controller
2. The system boots in **INIT** state for ~500ms while initializing SPI, sensors, and outputs
3. After initialization, the controller enters **IDLE** state
4. Connect the Ethernet cable — the controller acquires an IP via DHCP (fallback: `192.168.1.100`)
5. Open a web browser to `http://<device-ip>` to access the dashboard

### Default Configuration
| Parameter | Default Value |
|---|---|
| PID Setpoints (all channels) | 25.0°C |
| PID Mode | Manual (output = 0%) |
| Output Mode | PWM |
| Sensor Read Interval | 250 ms |
| PID Sample Time | 100 ms |

---

## 2. Hardware Setup

### Sensor Wiring — MAX31855 (Thermocouple)

```
MAX31855 Pin    →   STM32F407 Pin
─────────────       ──────────────
VCC (Pin 6)    →   3.3V
GND (Pin 2)    →   GND
CS  (Pin 5)    →   GPIOB.0–GPIOB.3, GPIOC.0–GPIOC.3 (one per sensor)
SO  (Pin 1)    →   PA6 (SPI1 MISO) — shared bus
SCK (Pin 4)    →   PA5 (SPI1 SCK)  — shared bus
T+  (Pin 7)    →   Thermocouple positive lead (yellow)
T-  (Pin 8)    →   Thermocouple negative lead (red)

NOTE: Each MAX31855 requires its own CS line. All sensors share MISO/SCK.
```

### Sensor Wiring — MAX31865 (PT100 RTD)

```
MAX31865 Pin   →   STM32F407 Pin
────────────       ──────────────
VCC (Pin 13)   →   3.3V
GND (Pin 9)    →   GND
CS  (Pin 7)    →   CS pin (as above, one per sensor)
SDO (Pin 12)   →   PA6 (SPI1 MISO)
SDI (Pin 11)   →   PA7 (SPI1 MOSI)
SCK (Pin 10)   →   PA5 (SPI1 SCK)

For 2-wire PT100: jumper FORCE+ to RTDIN+, FORCE2 to RTDIN-
For 3-wire PT100: FORCE+ to RTDIN+, FORCE2 to RTDIN-, RTDIN+ from FORCE+ via RJ1 jumper
For 4-wire PT100: Do not jumper.
```

### Output Wiring — PWM (SSR/Relay)

```
TIM2 Channel → STM32 Pin  → Connect to
────────────   ──────────    ──────────
CH1 (PA0)   → PA0        → SSR Input (+)
                    GND        → SSR Input (-)
CH2 (PA1)   → PA1        → SSR Input (+)
CH3 (PA2)   → PA2        → SSR Input (+)
CH4 (PA3)   → PA3        → SSR Input (+)

PWM: 1 kHz, 0-100% duty cycle, 3.3V logic level
```

### Output Wiring — Analog (0-10V)

```
DAC Channel → STM32 Pin  → Voltage Divider → Terminal
───────────   ──────────    ───────────────   ────────
DAC1/CH1    → PA4        → 3:1 divider      → AO1 (0-10V)
DAC1/CH2    → PA5        → 3:1 divider      → AO2 (0-10V)

Use external op-amp buffer for driving loads >1mA.
```

---

## 3. Web Dashboard

The dashboard is accessible at `http://<ip-address>/` and updates every 1 second.

### Sensor Panel
- Shows all 8 sensor channels with current temperature, cold-junction temperature, and fault status
- Green indicator = OK, Red indicator = Fault
- Fault code displayed next to each sensor (see [Troubleshooting](#8-troubleshooting))

### PID Control Panel
- **Setpoint input**: Type a value and press Enter to change the target temperature
- **Output bar**: Green bar showing 0–100% output
- **Mode button**: Toggles between AUTO and MANUAL
- **Tuning info**: Displays current Kp, Ki, Kd values
- **Auto-Tune buttons**: Z-N (Ziegler-Nichols) and C-C (Cohen-Coon)

### Output Panel
- Shows duty cycle for each output channel with orange bar
- Indicates ENABLED/DISABLED status

### Trend Chart
- Real-time canvas chart plotting setpoints for all 4 PID channels
- X-axis: last 200 data points (~200 seconds)
- Y-axis: 0°C to 450°C
- Color legend: Blue (CH1), Green (CH2), Orange (CH3), Red (CH4)

### Status Bar
- **State**: INIT → IDLE → RUNNING → FAULT → EMERGENCY
- **Alarms**: Count of active thermal/sensor alarms
- **Uptime**: Seconds since boot

---

## 4. PID Configuration

### Manual Tuning
Tune PID parameters via Modbus registers or serial command:

1. Set the channel to **MANUAL** mode
2. Set Kp to a small value (e.g., 0.5)
3. Increase Kp until the system oscillates slightly
4. Set Ki to Kp / oscillation_period
5. Set Kd to Kp * oscillation_period / 8

### PID Algorithm Details

```
Output = Kp * error + Ki * integral(error) * dt + Kd * d(PV)/dt

Anti-windup: Integral clamped to [-50, 50]; back-calculation on output saturation
Derivative: On process variable (not error) to avoid derivative kick on setpoint change
Sample time: 100 ms (configurable)
```

### Manual vs. Automatic Mode
- **Manual**: Output percentage is held at last value; user can set directly
- **Automatic**: PID loop computes output based on SP and PV

---

## 5. Auto-Tuning

### When to Use
- First-time setup with unknown thermal system characteristics
- After significant hardware changes (new heater, insulation, sensor relocation)
- Before starting production runs

### Ziegler-Nichols (Closed-Loop) Method
1. Click **Auto-Tune Z-N** on the desired PID channel
2. The controller switches to P-only mode and begins incrementing gain
3. The system will oscillate — do NOT interrupt the process
4. Once sustained oscillation is detected, tuning completes automatically
5. The new Kp, Ki, Kd are applied and the PID returns to AUTO mode

**Typical duration**: 30 seconds to 3 minutes

### Cohen-Coon (Open-Loop) Method
1. Ensure the system is at a stable temperature
2. Click **Auto-Tune C-C** on the desired PID channel
3. A 20% output step is applied and the process reaction is measured
4. Dead time and time constant are extracted from the curve
5. PID parameters are computed and applied

**Typical duration**: 1 to 5 minutes

### Aborting Auto-Tune
- Send abort command (cmd=0) via Modbus or the web API
- The original setpoint and manual mode are restored
- The auto-tune state will show `FAILED`

---

## 6. Modbus Interface

### Connection Parameters

| Protocol | Port/Baud | Format |
|---|---|---|
| Modbus TCP | Port 502 | MBAP header + standard PDU |
| Modbus RTU | 115200 baud (configurable) | USART3, 8N1 |
| Device ID | 1 | Configurable |

### Function Codes Supported
- **0x03** — Read Holding Registers
- **0x06** — Write Single Register
- **0x10** — Write Multiple Registers
- **0x04** — Read Input Registers

### Reading Temperature (Modbus Poll / Python Example)

```
# Read PV for sensor 1 (address 0x0000)
Register: 0x0000, Count: 1, FC: 0x03
Response: uint16 value ÷ 10 = °C (e.g., 253 → 25.3°C)
```

### Writing Setpoint

```
# Set PID Channel 1 setpoint to 150.0°C
Register: 0x0008, Value: 1500 (150.0 × 10), FC: 0x06
```

### Starting Auto-Tune via Modbus

```
# Start Z-N tuning on PID Channel 2
Register: 0x0041, Value: 1, FC: 0x06
```

---

## 7. Safety & Alarms

### Alarm Types

| Alarm | Bit | Description | Action |
|---|---|---|---|
| PID1 Thermal | 0x01 | PV deviates >10°C from SP for >5s | Emergency stop |
| PID2 Thermal | 0x02 | " | Emergency stop |
| PID3 Thermal | 0x04 | " | Emergency stop |
| PID4 Thermal | 0x08 | " | Emergency stop |
| Sensor Fault | 0x10 | ≥1 sensor has persistent fault | PID set to manual |
| Emergency | 0x20 | Over-temperature or thermal runaway | All outputs 0% |

### Emergency Stop Behavior
When triggered:
1. All 4 outputs forced to 0%
2. PID mode set to MANUAL for all channels
3. Controller state changes to EMERGENCY
4. Dashboard shows red state indicator
5. Alarm flag blinks on web UI

### Clearing Alarms
- Verify the root cause is resolved (faulty sensor replaced, temperature stabilized)
- Write `0` to thermal alarm register (0x0039) via Modbus
- Or cycle power to the controller

### Sensor Fault Codes (bitmask)

| Code | Fault Type | Likely Cause |
|---|---|---|
| 0x01 | Open Circuit | Broken wire, disconnected thermocouple |
| 0x02 | Short to GND | Thermocouple touching ground/shield |
| 0x04 | Short to VCC | Thermocouple shorted to power |
| 0x08 | General Fault | Internal MAX31855 error |
| 0x10 | Under-voltage | RTD resistance too low (MAX31865) |
| 0x20 | Over-voltage | RTD resistance too high (MAX31865) |
| 0x40 | Comm Error | SPI communication failure |

---

## 8. Troubleshooting

### Sensor reads 0°C or -999°C
- Check thermocouple wiring polarity (T+ yellow, T- red)
- Verify CS pin is correct for the sensor channel
- Use a multimeter to confirm thermocouple continuity
- Check cold-junction temperature — if >100°C or <-50°C, the MAX31855 may be damaged

### PID output stuck at 0% or 100%
- Verify PID mode is AUTO (not MANUAL)
- Check sensor validity — if sensor faulted, PID enters MANUAL automatically
- Check emergency stop status — if active, all outputs are 0%
- Verify Ki is not zero (required to eliminate steady-state error)

### Auto-tune fails repeatedly
- Reduce the step output percentage (ATUNE_STEP_OUTPUT_PCT in system_config.h)
- Increase the tune timeout (ATUNE_TUNE_TIMEOUT_MS)
- Ensure the thermal load is connected — tuning on an unloaded heater gives poor results
- Try the other method (Z-N if C-C failed, or vice versa)

### Web dashboard not loading
- Verify Ethernet cable is connected
- Check IP address via serial console or network scanner
- Ensure port 80 is not blocked by firewall
- Try `http://192.168.1.100` (default static IP fallback)

### Modbus communication timeout
- TCP: Verify port 502 is open (try `telnet <ip> 502`)
- RTU: Check baud rate matches (115200, 8N1)
- RTU: Verify USART3 TX/RX pins (PC10, PC11)

---

## Appendices

### A. Building from Source
See [README.md](../README.md#build) for build instructions.

### B. Flash Memory Map
| Region | Start | Size | Content |
|---|---|---|---|
| Bootloader | 0x08000000 | 32K | (optional) |
| Application | 0x08008000 | 960K | Firmware |
| Config EEPROM | 0x080F0000 | 64K | (optional NVRAM emulation) |

### C. Timing Budget
| Task | Period | Duration (approx) |
|---|---|---|
| Sensor read (1 channel) | 250 ms | ~1 ms (SPI at 4.5 MHz) |
| PID compute (all 4 channels) | 100 ms | <0.5 ms |
| Safety check | 1000 ms | <0.1 ms |
| Modbus RTU poll | continuous | <5 ms |
| HTTP response (JSON) | on-demand | ~10 ms |
| Web UI asset serve | on-demand | ~20 ms |
