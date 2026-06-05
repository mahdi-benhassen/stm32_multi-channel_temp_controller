# Multi-Channel Temperature Controller

Industrial-grade STM32F4 firmware for an 8-sensor, 4-PID-channel temperature controller with real-time auto-tuning, Modbus TCP/RTU, and an embedded web dashboard.

## Features

- **8× Thermocouple channels** via MAX31855 (or PT100 via MAX31865) over SPI
- **4× Independent PID controllers** with anti-windup protection (clamping + back-calculation)
- **Auto-Tuning Engine** — Ziegler-Nichols closed-loop and Cohen-Coon open-loop methods
- **Dual Output Modes** — Analog 0-10V (DAC) or PWM for SSR/relay drive
- **Modbus TCP/RTU** — Full register map for process variables, setpoints, PID gains, and status
- **Embedded Web Dashboard** — Real-time temperature trend chart, sensor cards, PID controls via lwIP HTTP server
- **Safety Monitor** — Thermal runaway detection, sensor fault handling, emergency stop

## Hardware Support

| Component | Supported IC | Interface |
|---|---|---|
| Thermocouple | MAX31855 (K/J/T/E/N/S/R type) | SPI (shared bus, individual CS) |
| PT100 RTD | MAX31865 | SPI (shared bus, individual CS) |
| MCU | STM32F407VG (Cortex-M4, 168 MHz) | — |
| Network | On-chip Ethernet MAC (RMII) + lwIP stack | RJ45 |
| Output | DAC (0-10V) or PWM (TIM2, 4 channels, 1 kHz) | GPIO |

## Project Structure

```
├── CMakeLists.txt          # CMake build for ARM GCC
├── Inc/                    # Header files
│   ├── system_config.h     # Pin mapping, constants, build flags
│   ├── data_structures.h   # Global state, structs, Modbus register map
│   ├── spi_driver.h        # SPI bus abstraction
│   ├── sensor_driver.h     # MAX31855 / MAX31865 driver
│   ├── output_driver.h     # DAC & PWM output driver
│   ├── pid_controller.h    # PID with anti-windup
│   ├── auto_tune.h         # Z-N & Cohen-Coon auto-tuning
│   ├── modbus_server.h     # Modbus TCP & RTU server
│   ├── http_server.h       # lwIP HTTP + JSON API
│   └── safety_monitor.h    # Thermal runaway & emergency stop
├── Src/                    # Source files
│   ├── main.c              # Super-loop, HAL init, system clock
│   ├── spi_driver.c
│   ├── sensor_driver.c
│   ├── output_driver.c
│   ├── pid_controller.c
│   ├── auto_tune.c
│   ├── modbus_server.c
│   ├── http_server.c
│   ├── webui_data.c        # Embedded HTML/CSS/JS assets
│   └── safety_monitor.c
├── WebUI/                  # Frontend source (also embedded in flash)
│   ├── index.html
│   ├── style.css
│   └── app.js
└── docs/
    └── user_manual.md
```

## Build

### Prerequisites
- **ARM GNU Toolchain** (`arm-none-eabi-gcc`)
- **CMake** ≥ 3.16
- STM32CubeF4 HAL driver package (place under `Drivers/`)

### Steps

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm-none-eabi.cmake
cmake --build .
```

## Modbus Register Map

| Address | Description | Scale |
|---|---|---|
| 0x0000–0x0007 | Process Variable (PV), sensors 1–8 | °C × 10 |
| 0x0008–0x000B | Setpoint (SP), PID channels 1–4 | °C × 10 |
| 0x000C–0x000F | Proportional Gain (Kp), channels 1–4 | × 1000 |
| 0x0010–0x0013 | Integral Gain (Ki), channels 1–4 | × 1000 |
| 0x0014–0x0017 | Derivative Gain (Kd), channels 1–4 | × 1000 |
| 0x0018–0x001B | Output percentage, channels 1–4 | % × 10 |
| 0x001C–0x001F | Auto-tune status (0=Idle,1=Running,2=Complete,3=Failed) | raw |
| 0x0020–0x0027 | Sensor fault flags, sensors 1–8 | bitmask |
| 0x0028–0x002B | Output mode (0=AO,1=PWM,2=Disabled) | raw |
| 0x0034–0x0037 | PID mode (0=Manual,1=Auto) | raw |
| 0x0038 | Controller state | raw |
| 0x0039 | Thermal alarm flags | bitmask |
| 0x0040–0x0043 | Auto-tune command (write 1 to start) | raw |

## Web API Endpoints

| Endpoint | Method | Description |
|---|---|---|
| `/api/data` | GET | Full system state JSON (sensors, PID, outputs, alarms, uptime) |
| `/api/control?ch=N&action=setpoint&value=V` | POST | Set PID setpoint for channel N |
| `/api/control?ch=N&action=toggle_mode` | POST | Toggle PID auto/manual mode |
| `/api/autotune?ch=N&cmd=M` | GET | Start auto-tune: cmd=1 (Z-N), cmd=2 (Cohen-Coon), cmd=0 (abort) |
| `/api/config` | GET | Hardware configuration info |
| `/` | GET | Web dashboard |
| `/style.css` | GET | Dashboard stylesheet |
| `/app.js` | GET | Dashboard JavaScript |

## Auto-Tuning

### Ziegler-Nichols Closed-Loop
1. PID set to P-only mode with low initial gain
2. Gain incremented until sustained oscillation detected
3. Ultimate gain Ku and period Pu recorded
4. PID computed: Kp = 0.6*Ku, Ki = 2*Kp/Pu, Kd = Kp*Pu/8

### Cohen-Coon Open-Loop
1. Step output bump applied, PID in manual
2. Dead time and time constant measured from process reaction curve
3. PID computed from τ and θ using Cohen-Coon formula

## Safety Features

- **Thermal Runaway**: Trip if PV deviates >10°C from SP for >5 seconds
- **Absolute Limits**: Max 450°C / Min -50°C cutoff
- **Sensor Fault Detection**: Open circuit, short to GND, short to VCC (MAX31855)
- **Emergency Stop**: All outputs forced to 0%, controller enters EMERGENCY state
- **Watchdog**: Sensor invalidation after 3 consecutive read failures

## License

MIT
