# Smart Garden

A self-hosted garden monitoring system that measures soil moisture and temperature, transmits data over WiFi via MQTT, and displays it on a web dashboard.

## Architecture

```
STM32 ──UART 115200──► ESP32 ──MQTT──► Mosquitto ──► Spring Boot ──► PostgreSQL
                                                                          │
                                                            React frontend ◄─ HTTP /api
```

## Components

| Directory | Stack | Role |
|---|---|---|
| `STM/` | STM32F103 (Blue Pill), STM32CubeIDE | Reads sensors, sends JSON over UART |
| `ESP/` | ESP32 DOIT DevKit V1, PlatformIO/Arduino | UART↔WiFi bridge, publishes to MQTT |
| `garden-backend/` | Spring Boot 3.5, Java 21, PostgreSQL | MQTT listener, REST API |
| `garden-frontend/` | React 18, Vite, Tailwind CSS | Dashboard, charts, admin |

## Data Flow

1. ESP32 wakes from deep sleep (timer every 60 min, or manual button press).
2. ESP32 sends `MEASURE\n` over UART to STM32.
3. STM32 reads sensors and replies with one JSON line over UART.
4. ESP32 publishes the JSON to MQTT topic `smartgarden/{deviceId}/data`.
5. Spring Boot receives the message and writes one `Measurement` row per field to PostgreSQL.
6. React frontend polls `GET /api/sensors/{id}/history` to display current state and charts.

## MQTT Payload

Topic: `smartgarden/{deviceId}/data`

```json
{"deviceId":1,"temperature":24.50,"soil":[78,45,23,90],"battery":3812}
```

- `soil` values are percentages 0–100 (calibrated on the STM32 side).
- `temperature` is `null` if the DS18B20 read fails.
- `battery` is an integer in millivolts. Backend and frontend ignore unknown fields — forward-compatible.

## REST API

All endpoints are keyed by `deviceId` (hardware ID, not internal DB id).

| Method | Path | Description |
|---|---|---|
| GET | `/api/sensors` | List all sensors |
| GET | `/api/sensors/{deviceId}/history` | All measurements, newest first |
| GET | `/api/sensors/{deviceId}/latest-temperature` | Last 5 temperature rows |
| PUT | `/api/sensors/{deviceId}` | Update `name` / `location` |
| DELETE | `/api/sensors/{deviceId}` | Delete sensor and its measurements |

## Frontend Pages

| Page | URL | Description |
|---|---|---|
| Dashboard | `/` | Sensor cards with current values |
| Sensors | `/sensors` | Sensor list with online/offline status |
| Sensor Detail | `/sensors/:id` | Temperature and moisture charts, CSV export |
| Measurements | `/measurements` | All measurements with filters, CSV export |
| Admin | `/admin` | Edit and delete sensors |

## Local Development

### Full stack

```bash
docker compose up -d   # backend, frontend, postgres, mosquitto
```

Requires an external Docker network named `mimozalab-network`. Create it first or remove the `networks:` block from `docker-compose.yml` for a self-contained run.

### Backend only

```bash
cd garden-backend
./mvnw spring-boot:run   # port 8083, expects postgres on localhost:5432
```

Environment overrides: `SPRING_DATASOURCE_URL`, `SPRING_DATASOURCE_USERNAME`, `SPRING_DATASOURCE_PASSWORD`.

### Frontend only

```bash
cd garden-frontend
cp .env.example .env    # sets VITE_API_URL=http://localhost:8083
npm install
npm run dev             # http://localhost:5173
```

### ESP32 firmware

```bash
cd ESP
pio run                 # build
pio run -t upload       # flash via COM3
pio device monitor      # serial monitor at 115200
```

WiFi credentials, MQTT broker IP, and device ID are hardcoded at the top of `ESP/src/main.cpp`.

### STM32 firmware

Primary IDE: STM32CubeIDE. For headless builds:

```bash
cd STM
make                    # output in STM/build/
```

## CI/CD

Push to `main` → GitHub Actions → Docker image pushed to GHCR → deployed to Hetzner via SSH.

| Workflow | Trigger paths | Action |
|---|---|---|
| `backend-deploy.yml` | `garden-backend/**`, `docker-compose.yml` | Build + deploy backend |
| `frontend-deploy.yml` | `garden-frontend/**` | Build + deploy frontend |
| `esp32-build.yml` | `ESP/**` | Compile, upload `firmware.bin` as artifact |
| `stm32-build.yml` | `STM/**` | Compile, upload `.bin`/`.hex` as artifacts |

Required GitHub secrets: `HETZNER_HOST`, `HETZNER_USER`, `HETZNER_SSH_KEY`, `GHCR_TOKEN`.

Server files live at `/srv/projects/garden/backend/`.

## Hardware Wiring

| Signal | ESP32 pin | Other side |
|---|---|---|
| UART RX (from STM) | `GPIO16` (RXD2) | STM32 `PA9` (USART1 TX) |
| UART TX (to STM) | `GPIO17` (TXD2) | STM32 `PA10` (USART1 RX) |
| Manual wake button | `GPIO33` | Button contact → GND |
| Diagnostic LED | `GPIO26` | LED anode → resistor → cathode → GND |
| Common ground | `GND` | STM32 `GND` |

STM32 diagnostic LED: `PB12` → 330 Ω → LED anode → cathode → GND (active HIGH, 20 mA capable). Mirrors the built-in PC13 LED (active LOW, limited to ~3 mA — do not drive external LEDs from PC13).

Battery voltage divider: `TP4056 OUT+` → R1 100 kΩ → `PA3` → R2 100 kΩ → GND. Tap `OUT+`, not `B+` — `B+` bypasses the DW01A protection circuit.

## Sleep Cycle

- STM32 stays in **STOP mode** between measurements.
- ESP32 stays in **deep sleep** between cycles (60-minute timer).
- On wake: ESP32 sends `MEASURE\n` over UART → STM32 wakes via UART RX interrupt, measures, replies with JSON → ESP32 publishes to MQTT → both sleep again.
- Manual wake: button on `GPIO33` (ESP32) triggers immediate measurement cycle.

## Diagnostic LEDs

Pulse pattern: 120 ms ON / 200 ms OFF, 400 ms gap between groups (so `1+2` reads distinctly from `3`).

**STM32 — PC13 (built-in, active LOW) + PB12 (external, active HIGH), driven in sync:**

| Event | Blinks |
|---|---|
| Woke, starting measurement | 1 |
| UART TX success (`HAL_OK`) | 1 |
| UART TX failed / timeout | 2 |

**ESP32 — GPIO26 (active HIGH):**

| Event | Blinks |
|---|---|
| `setup()` after wake from deep sleep | 1 |
| `client.publish()` returned `true` | 1 |
| `client.publish()` returned `false` | 2 |
| `readMeasurement()` timed out (no JSON from STM) | 3 |

**Typical readable sequences (STM group + ESP group per cycle):**

| Situation | Pattern |
|---|---|
| All OK | `1+1` / `1+1` |
| STM sent OK, MQTT failed | `1+1` / `1+2` |
| STM sent OK, ESP missed it (UART noise) | `1+1` / `1+3` |
| STM UART TX broken | `1+2` / `1+3` |
| ESP woke, STM never woke | `—` / `1+3` |

## Battery Monitoring (18650 Li-Ion)

- Voltage measured on `PA3` (ADC1 IN3) via 100 kΩ / 100 kΩ divider.
- Formula: `mV = raw × 3300 × 2 / 4095`, averaged over 8 samples.
- Sampling time overridden to 71.5 cycles (CubeMX default 7.5 cycles is too short for the 50 kΩ Thévenin source impedance).
- Thresholds in `main.c`: `BATTERY_LOW_MV = 3400` (informational), `BATTERY_CRITICAL_MV = 3100` (triggers STANDBY after 3 consecutive readings).
- Floor at 2400 mV: anything below means PA3 is floating (divider not wired). Treated as noise, not critical battery.
- A freshly charged 18650 reads 4026–4029 mV, stable within ±3 mV.

Power path: solar/USB → TP4056 (with DW01A protection) → LDO (MCP1700-3302, ~1.6 µA quiescent) → STM32 3.3 V. Never connect a 3.7–4.2 V Li-Ion directly to STM32 VDD (max 3.6 V). The AMS1117 on the Blue Pill board requires 5 V input — unusable here.

## Flashing the STM32 (Clone Board)

The board uses a clone STM32F103 (`Revision ID: Rev X`). The flash controller fails on per-sector erase but always accepts full-chip erase. **Every flash must start with a full chip erase:**

- STM32CubeProgrammer → `Erasing & Programming` → check **`Full chip erase`** → `Start Programming`
- CLI:
  ```
  STM32_Programmer_CLI -c port=SWD mode=UR -e all -w F103First.hex 0x08000000 -v -rst
  ```

`HAL_DBGMCU_EnableDBGStopMode()` is called at startup so SWD stays alive in STOP mode — CubeProgrammer's *Under Reset* mode can halt the CPU mid-sleep cycle.

If flashing still fails after full-chip erase: set **BOOT0 = 1** jumper → RESET → flash → BOOT0 = 0 → RESET. This uses the ROM bootloader and bypasses user code entirely.

## Known Issues

1. **STOP-mode UART corruption.** After waking from STOP, the first USART1 TX sends garbage bytes. Fix: in `EnterStopMode()`, after `SystemClock_Config()`, do a full `HAL_UART_DeInit(&huart1)` + `MX_USART1_UART_Init()` + `HAL_UART_Receive_IT(...)`. Already applied in `Core/Src/main.c`.

2. **PA1 EXTI picks up antenna noise.** When ESP32 is in deep sleep, `GPIO25` is high-impedance. The wire from `GPIO25` to STM32 `PA1` acts as an antenna. The internal pull-down (~40 kΩ) is too weak — STM32 wakes at random. Quick fix: external 4.7–10 kΩ pull-down resistor between `PA1` and GND. Proper fix: remove the wire entirely and rely on `MEASURE\n` over UART (see roadmap item 4 in `CLAUDE.md`).

3. **PA3 floating without the voltage divider.** If the battery divider is not wired, PA3 reads random values. `battery` field will show noise. Not a bug — just an unconnected analog input.

4. **ADC wrong readings with CubeMX default sampling time.** The default 7.5-cycle sampling time is insufficient for the 50 kΩ Thévenin impedance of the 100 kΩ / 100 kΩ divider. Overridden to 71.5 cycles in `USER CODE ADC1_Init 2`. ADC calibration (`HAL_ADCEx_Calibration_Start`) is called in `USER CODE BEGIN 2` — CubeMX does not generate this call automatically.

## Further Reading

Full developer guide, roadmap (10 items), target hardware architecture (4 groups × 16 moisture sensors + 4 DS18B20), and design decisions for the irrigation downlink channel: **[`CLAUDE.md`](CLAUDE.md)**.
