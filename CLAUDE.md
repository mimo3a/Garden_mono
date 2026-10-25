# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

### Full stack (Docker)
```bash
docker compose up -d
# Requires an external Docker network named mimozalab-network
```

### Backend (Spring Boot 3.5, Java 21)
```bash
cd garden-backend
./mvnw spring-boot:run   # development
./mvnw test              # run tests
./mvnw clean package     # build jar
```

### Frontend (React 18 + Vite)
```bash
cd garden-frontend
cp .env.example .env
npm install
npm run dev    # development server with proxy to :8083
npm run build
```

### ESP32 firmware (PlatformIO)
```bash
cd ESP
pio run               # compile
pio run -t upload     # flash
pio device monitor    # serial monitor
```

### STM32L476 firmware (V2, `STM-L476-FreeRTOS/`, STM32CubeIDE only)
Primary IDE is STM32CubeIDE. No Makefile build for the L476 project yet, so there is no STM32 CI build.

V1 firmware (STM32F103) was removed from `develop`; it is preserved on the `v1-f103` branch and in releases `v1.0.x` (`cd STM && make` there).

## Architecture

### Data flow
```
Sensors → STM32 → UART 115200 → ESP32 → Wi-Fi → Mosquitto → Spring Boot → PostgreSQL
                                                                          ↑
                                                               React (REST API)
```

### Firmware communication protocol (STM32 ↔ ESP32)
Full specification (V1 and V2, codes, buffer sizes, ack and retry rules): `docs/protocol.md`. Hardware V2: `docs/hardware.md`.

V1 (installed F103, current ESP firmware):
1. ESP32 sends `MEASURE\n` over UART2 (GPIO16/17) — this also wakes STM32 from STOP mode.
2. STM32 reads all sensors and responds with a single JSON line.
3. ESP32 publishes to MQTT topic `smartgarden/{deviceId}/data`.
4. ESP32 sends `OK\n` ACK back to STM32.
5. STM32 returns to STOP mode; ESP32 enters deep sleep (60-minute interval).

**Critical timing**: ESP32 sends `MEASURE` immediately at startup, *before* connecting to Wi-Fi. By the time Wi-Fi connects, the STM32 measurement is already buffered in the UART RX buffer. The MQTT connection is only opened after the JSON is received — this avoids PubSubClient keepalive misses during the up-to-60-second STM32 wait window.

**ACK behavior**: ESP32 sends `OK` regardless of MQTT result. If no JSON arrives before the 60-second timeout, the ACK is withheld and the STM32 falls back to its own 30-second ACK timeout.

**V2 (spec, not implemented yet)**: ESP32 sends `OK` immediately after a valid JSON line, so the STM32 sleeps without waiting for the server. ESP32 then adds `sid` (random uint32 generated once when NVS is initialised), `seq` (monotonic counter in NVS) and `ts` (measurement time via SNTP, or `null`), publishes, and waits for `smartgarden/{id}/ack` from the backend (published after the DB commit). Unacknowledged measurements stay in a bounded queue and are resent on the next wake-up; the backend deduplicates by `deviceId + sid + seq`. `PAYLOAD_MAX_LEN` = 512 bytes including `
` (ESP UART RX buffer and PubSubClient buffer must be enlarged).

### MQTT payload schema
V1 (installed F103):
```json
{
  "deviceId": 2,
  "temperature": 24.50,
  "soil": [78, 45, 23, 90],
  "soilRaw": [8200, 11800, 14100, 7600],
  "adsReady": true,
  "battery": 3812
}
```
`temperature` may be `null` on DS18B20 failure. `battery` is in millivolts (`raw × 3300 × 2 / 4095`). `soil` values are 0–100% calibrated from raw ADC counts.

V2 (target, STM32L476; spec in `docs/protocol.md`, not implemented yet): 16 soil slots (4 modules × 4), `soilRaw` carries negative error codes (`-1` module absent, `-2` module not responding, `-3` open, `-4` short, `-5` conversion timeout, `-6` TCA channel fault), `modules` is the source of truth for module status:
```json
{"deviceId":3,"fw":"2.0.0","temperature":21.50,
 "soil":[78,null,23,90, null,null,null,null, null,null,null,null, 45,51,60,38],
 "soilRaw":[8200,-3,14100,7600, -1,-1,-1,-1, -2,-2,-2,-2, 11800,10500,9300,12700],
 "modules":["ok","absent","error","ok"],
 "air":{"t":12.4,"h":81,"p":1013.2},
 "battery":3812,"status":0,"sid":2864434397,"seq":1042,"ts":1761400000}
```
`sid`, `seq` and `ts` are added by the ESP32. Module `m` = zone `m+1`, channels `4m…4m+3`.

Any payload schema change must be coordinated across STM32 firmware, ESP32 firmware, `MqttMessageHandler.java`, and the frontend presentation layer.

### Backend internals
- **MQTT ingestion**: `MqttMessageHandler` (Spring Integration) subscribes to `smartgarden/+/data`, parses `deviceId` from the topic path (`parts[1]`), and auto-creates a `Sensor` entity on first message.
- **Measurement types** stored: `temperature`, `soil1`…`soilN` (one per element of the `soil` array, any length), `battery`. A `null` in `soil` is currently stored as `0.0` (fixed by BE-1-1).
- **Auth**: session-based (no JWT). CSRF uses `CookieCsrfTokenRepository` with a custom `SpaCsrfTokenRequestHandler` so the SPA can submit the raw `XSRF-TOKEN` cookie value in `X-XSRF-TOKEN`. The three auth endpoints (`/api/auth/csrf`, `/api/auth/login`, `/api/auth/accept-invitation`) bypass CSRF because they are either pre-session or establish the session.
- **Access control**: `SensorAccessService` filters sensor visibility by owner or invitation.
- **Bootstrap admin**: created once from `APP_BOOTSTRAP_ADMIN_EMAIL` / `APP_BOOTSTRAP_ADMIN_PASSWORD` env vars.

### Frontend internals
- All API calls go through the Axios instance in `src/api/sensors.js`, which sets `withCredentials: true` and the XSRF header automatically.
- Vite proxies `/api` to `http://localhost:8083` in dev; production uses `VITE_API_URL`.
- `AuthContext` (`src/auth/AuthContext.jsx`) holds session state; protected routes check it.
- `src/utils/sensorData.js` owns the `isOnline()` / `measurementWarnings()` logic — the threshold for "online" is derived here, not on the backend.

### STM32L476 FreeRTOS firmware (develop)
- Single FreeRTOS task `SensorTask` in `STM-L476-FreeRTOS/STM_Garden_V2/Core/Src/freertos.c`.
- Reads 2× ADS1115 (8 channels total: `ADS_COUNT=2`, `CH_PER_ADS=4`) under an I2C mutex (`i2c1MutexHandle`).
- Current JSON output format differs from the stable payload: `{"channels":[{"dev":0,"ch":0,"status":0,"raw":8200},…]}` — not yet compatible with the backend parser. DS18B20 is not yet integrated in this version.

## Branching model
- `main` — working baseline. CI deploys from here.
- `develop` — active development; V2 firmware (STM32L476) lives here.
- `v1-f103` — frozen V1 firmware (STM32F103), maintenance fixes only.
- `feature/*` → `develop` → `main` when verified on hardware.

## Key embedded engineering constraints
- After STM32 STOP-mode wake-up, USART and clocks must be reinitialized before transmitting.
- The 100 kΩ / 100 kΩ battery divider requires a longer ADC sampling time (71.5 cycles); readings below the physically plausible range (< ~2800 mV) must be treated as floating input, not real critical-battery.
- Battery protection: shutdown after three consecutive readings below 3100 mV; warning below 3400 mV.
- Li-ion must never be charged below 0 °C (V2). The cold/hot charge inhibit (below 0 °C, above +45 °C, re-enable at +3 °C, battery NTC) is enforced **in hardware without the MCU** (charger NTC input or a comparator powered from the charge input). The STM32 may only read the charge state and add an extra inhibit; it must never be the only thing enabling charging, otherwise a deeply discharged battery with the STM32 in Shutdown/brownout can never recharge. See `docs/hardware.md`.
- All low-power paths (STM32 and ESP32) must have communication timeouts — a failed UART/MQTT transaction must never leave the device awake indefinitely.
- ESP32 `DEVICE_ID` and Wi-Fi/MQTT credentials are defined as constants in `ESP/src/main.cpp` (from `secrets.h`). They are intentionally not in version control.

## AI irrigation agent (in progress)

The full specification is in `docs/AI_AGENT_PLAN.md`. Read it before working on anything related to
irrigation, the pump, the agent, approvals or the audit log.

Rules that always apply:
- Safety never depends on the agent alone: every hard limit (max litres per zone/day, tank reserve,
  frost, battery, once per zone per day, approval above 15 l) is enforced in the backend, the most
  critical ones also in the firmware.
- Work phase by phase (Phase 0 discovery first). Present a plan and wait for approval before each phase.
- Never flash firmware or deploy to the server; prepare the steps instead.
- No secrets in the repo; use environment variables and `.env.example`.
- Reference prototype for the agent: https://github.com/mimo3a/smart-garden-agent

## Project roadmap

All development work follows the milestone plan in `docs/ROADMAP.md`; tasks per component are in
`docs/roadmap/` (`firmware.md` — STM32L476 + FreeRTOS, `esp.md`, `backend.md`, `frontend.md`, `ops.md`).
Execution rules are in `docs/AGENT.md`. Read `ROADMAP.md` and `AGENT.md` before starting any task.
On conflict: CLAUDE.md > docs/AI_AGENT_PLAN.md (in its scope) > AGENT.md > ROADMAP.md > docs/roadmap/*.md.

## Release checklist (develop → main)
1. Build all four components successfully.
2. Verify sensor acquisition and UART communication on real hardware.
3. Verify MQTT delivery, backend persistence, and dashboard display from a fresh measurement.
4. Verify sleep/wake recovery after a communication failure.
5. Confirm no credentials, build artifacts, or IDE workspace files are tracked.
6. Update `README.md` if payload, wiring, or architecture changed.
