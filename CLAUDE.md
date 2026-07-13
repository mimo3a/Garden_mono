# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project layout

This is a monorepo for a smart garden monitoring system. Four cooperating components live side-by-side at the repo root:

```
STM/             STM32F103 (Blue Pill) firmware — reads sensors, emits JSON over UART
ESP/             ESP32 firmware (PlatformIO/Arduino) — UART↔WiFi bridge to MQTT
garden-backend/  Spring Boot 3.5 / Java 21 — MQTT listener + REST API + PostgreSQL
garden-frontend/ React 18 + Vite + Tailwind — dashboard, charts, admin
```

Top-level `docker-compose.yml` runs backend + frontend + postgres + mosquitto and is the file that gets deployed to the Hetzner server (`/srv/projects/garden/backend/`).

## Data flow (the big picture)

```
STM32 ──UART 115200──► ESP32 ──MQTT smartgarden/{deviceId}/data──► Mosquitto
                                                                       │
                                                                       ▼
                                         Spring Boot (MqttMessageHandler)
                                                                       │
                                                                       ▼
                                                                  PostgreSQL
                                                                       │
                                          React frontend ──HTTP /api──┘
```

Key contract — the JSON payload format flows end-to-end and must stay aligned across all four components:
```json
{"deviceId":1,"temperature":24.50,"soil":[78,45,23,90]}
```
- `soil` values are already **percentages 0–100** (calibration happens on the STM32 in `Moisture_ToPercent`).
- `temperature` may be `null` if DS18B20 read fails.
- The STM32 measures **on demand**: ESP32 pulses GPIO25 high (wired to STM32 PA1 EXTI rising-edge) **and** sends the literal string `MEASURE\n` over UART. STM32's interrupt handler / UART RX callback sets `measure_flag`, the main loop calls `MeasureAndDisplay()`, and the JSON line is written back over the same UART. Default cadence is 10 seconds, driven by the ESP32 loop.
- The backend's MQTT subscription is `smartgarden/+/data`; `deviceId` is parsed from the topic, **not** from the payload. If a `deviceId` is unknown, a new `Sensor` row is auto-created with `name = "Group {id}"`.
- Each MQTT message produces **one `Measurement` row per field**: one for `temperature`, plus one per soil index named `soil1`…`soil4`. The frontend reassembles a "current state" view by querying latest values per type.

## Common commands

### Backend (`garden-backend/`)
```bash
./mvnw spring-boot:run        # local run, defaults to localhost:5432 postgres, port 8083
./mvnw test                   # run tests
./mvnw clean package          # produces target/*.jar (used by Dockerfile)
```
Environment overrides: `SPRING_DATASOURCE_URL`, `SPRING_DATASOURCE_USERNAME`, `SPRING_DATASOURCE_PASSWORD`. `MQTT_BROKER` is read from env by docker-compose but the broker URI is currently hardcoded to `tcp://mqtt:1883` in `MqttConfig.java` — change there if you need a different host. MQTT credentials are also hardcoded (`esp32` / `GardenSensors`).

JPA uses `ddl-auto=update` — schema is managed by entity classes, no migrations.

### Frontend (`garden-frontend/`)
```bash
npm install
cp .env.example .env          # sets VITE_API_URL=http://localhost:8083
npm run dev                   # http://localhost:5173
npm run build                 # produces dist/, served by nginx in the Docker image
```
Axios `baseURL` comes from `VITE_API_URL`; falls back to same-origin so the nginx container can reverse-proxy `/api/*` to the backend.

### ESP32 (`ESP/`)
```bash
pio run                       # build (used by CI)
pio run -t upload             # flash via COM3 (configured in platformio.ini)
pio device monitor            # serial monitor at 115200
```
WiFi SSID/password and MQTT broker IP are **hardcoded** at the top of `src/main.cpp` — edit there.

### STM32 (`STM/`)
Primary IDE: STM32CubeIDE (`.cproject`, `.project`, `F103First.ioc` for CubeMX). For headless / CI builds:
```bash
make                          # uses Makefile at STM/, output in STM/build/
```
CI installs `gcc-arm-none-eabi` and just runs `make`. If you edit the `.ioc` and regenerate, the Makefile may need a re-sync.

### Full local stack
```bash
docker compose up -d          # backend, frontend, postgres, mosquitto on an external "mimozalab-network"
```
The compose file expects an external Docker network named `mimozalab-network` to already exist; create it or remove the `networks:` block for a self-contained dev run.

## Deployment

Push to `main` → GitHub Actions builds and deploys. Path-scoped workflows mean a change to `STM/` will **not** rebuild the backend, and vice versa:

| Workflow | Trigger paths | Action |
|---|---|---|
| `backend-deploy.yml` | `garden-backend/**`, `docker-compose.yml` | Build image → push to `ghcr.io/mimo3a/garden-backend:latest` → SCP compose file to Hetzner → `docker compose up -d --no-deps backend` |
| `frontend-deploy.yml` | `garden-frontend/**` | Same pattern for frontend |
| `stm32-build.yml` | `STM/**` | `make`, upload `.bin`/`.hex` as artifacts (no deploy) |
| `esp32-build.yml` | `ESP/**` | `pio run`, upload `firmware.bin` as artifact (no deploy) |

Server-side files live at `/srv/projects/garden/backend/`. Required GitHub secrets: `HETZNER_HOST`, `HETZNER_USER`, `HETZNER_SSH_KEY`, `GHCR_TOKEN`.

## REST API (consumed by the frontend)

All paths are keyed by `deviceId` (the hardware ID), not the internal `Sensor.id`:

- `GET    /api/sensors` — all sensors
- `GET    /api/sensors/{deviceId}/history` — all measurements, newest first
- `GET    /api/sensors/{deviceId}/latest-temperature` — last 5 temperature rows
- `PUT    /api/sensors/{deviceId}` — patch `name` / `location` (other fields ignored)
- `DELETE /api/sensors/{deviceId}` — cascades to measurements

`garden-frontend/FRONTEND_SPEC.md` is the design spec for pages; it lists some endpoints (`/latest`, filtered history) that are described as planned but not yet implemented on the backend.

## Target hardware architecture (not yet built)

Current hardware is a single STM32 board with **one** ADS1115 (4 moisture channels) + **one** DS18B20, sending one JSON per measurement. The target end-state is bigger:

- **4 × ADS1115** on the shared I2C1 bus, addressed via the ADDR pin (`GND`, `VDD`, `SDA`, `SCL` → 0x48, 0x49, 0x4A, 0x4B).
- **16 moisture sensors total** (4 per ADC) organised into **4 groups** ("зоны").
- **4 × DS18B20**, one per group, for per-zone temperature. Either share one 1-Wire bus with ROM-addressed devices, or use 4 separate GPIO pins (simpler firmware, more pins burned).
- **Sleep/wake cycle:** STM32 sleeps (STOP mode), wakes every **30 min** via RTC alarm, takes one round of measurements, ships data, sleeps again. Button press = external interrupt wake for an on-demand reading. The current 10-second always-on poll loop disappears.
- **Downlink command channel** (later): backend → MQTT → ESP32 → UART → STM32, to drive an irrigation system. Likely topic `smartgarden/{deviceId}/cmd`, payload like `{"action":"water","groupId":2,"durationSec":30}`. STM32 already has a UART RX state machine (`rx_buffer` in `main.c`) handling the literal string `MEASURE` — extending it to a small command parser is the path.

### Open design decisions (resolve before item 7 below)

1. **One `deviceId` per board, or per group?** Two options:
   - *Group-as-device:* each of the 4 groups maps to its own `Sensor` row (`deviceId` 1–4). Zero backend changes — the existing `smartgarden/{deviceId}/data` topic and `Sensor` table just get more rows. STM32 publishes 4 messages per wake.
   - *Board + group:* introduce a `Group` entity in the backend with `(deviceId, groupId)` composite key. More flexible (per-board grouping/admin), more code.

   Default recommendation: **group-as-device** — keeps backend simple, and "group" is really just a sensor cluster anyway.

2. **Who initiates the wake handshake?** When STM32 wakes on its RTC, the ESP32 may have lost WiFi or be off. Options: (a) ESP32 stays online and STM32 just pushes data when ready; (b) STM32 wakes the ESP32 via a GPIO line, ESP32 reconnects WiFi+MQTT, then STM32 dumps. (a) is much simpler if ESP32 power isn't a constraint.

3. **Command latency window.** If both devices sleep, downlink commands only arrive during the wake window. For irrigation that's probably fine (water within 30 min of asking is OK), but if low-latency is required, the ESP32 must stay online and queue commands over UART when STM32 next wakes.

## Things worth knowing before editing

- The `STM/Release/` and `STM/Debug/` directories contain build artifacts (`.o`, `.d`, `.elf`, `.su`) that get touched every build and show up dirty in `git status`. Ignore them unless you're intentionally cleaning. Same for `ESP/.pio/`.
- The repository contains `STM32IDE/` (the IDE itself, checked in) and `.metadata/` (Eclipse workspace metadata). Don't modify these unless you understand why.
- `Core/MyLib/` under STM holds project-specific drivers (`ads1115`, `ds18b20`, `liquidcrystal_i2c`). User logic in `main.c` lives between `/* USER CODE BEGIN */` and `/* USER CODE END */` markers — CubeMX regeneration preserves only those regions.
- The backend has effectively no test coverage (the `src/test/` tree is empty besides scaffolding) and `MqttMessageHandler` catches all exceptions and just `printStackTrace()`s — bad payloads silently no-op.
- Comments in STM32 and ESP code are mixed Russian/English; keep that style if extending.

## Firmware operational features

### Flash-safe boot on STM32 (PA3 hold-to-recover)

Because the firmware enters STOP mode almost immediately after each measurement, a normal SWD flash session can race the sleep and fail with `Error: failed to download Sector[0]` / `The core is locked up` in STM32CubeProgrammer. Two safeguards are in place:

- `HAL_DBGMCU_EnableDBGStopMode()` is called at the top of `main()` — SWD stays alive across STOP so an *Under Reset* connection can still halt the CPU.
- **PA3 hold-to-recover:** at boot, immediately after peripheral init, `main.c` reads PA3. If it's pulled LOW (button held), the MCU enters an infinite fast-blink loop on PC13 and never touches STOP / ADS / DS18B20. This is the safety net: any firmware — even if the app itself is broken — can be reflashed by holding the button.

**Recovery procedure:**
1. Hold PA3 button.
2. Press RESET (or power-cycle) — keep the button held.
3. PC13 LED blinks fast (~12 Hz) → safe-boot active.
4. Flash normally in CubeProgrammer.
5. Release button, press RESET → new firmware runs.

**First-time bootstrap** (safe-boot not yet in flash): flash once using **BOOT0 = 1** jumper (system memory bootloader), then the button trap is available for every subsequent flash.

### Diagnostic LEDs (sleep-cycle status)

Both firmwares blink a single LED to trace what happened during each wake cycle. Patterns are timed 120 ms ON / 200 ms OFF, with a 400 ms gap between groups so `1+2` reads distinctly from `3`.

**STM32 — built-in PC13 (active LOW) + external PB12 (active HIGH), driven in sync from `LED_Blink()`.** The two LEDs light and darken together across every diagnostic pulse and across the safe-boot fast-blink. PB12 wiring: `PB12 → R 330 Ω → LED anode → cathode → GND`.

| Event | Blinks |
|---|---|
| Woke, starting measurement | 1 |
| `HAL_UART_Transmit` returned `HAL_OK` (2 s timeout) | 1 |
| UART TX failed / timed out | 2 |

**ESP32 — external LED on GPIO 26** (active HIGH, wire: GPIO 26 → resistor → LED anode → cathode → GND):
| Event | Blinks |
|---|---|
| `setup()` entry after wake from deep sleep | 1 |
| `client.publish()` returned `true` | 1 |
| `client.publish()` returned `false` | 2 |
| `readMeasurement()` timed out (no valid JSON from STM) | 3 |

Pins 2 and 4 are reserved for TFT (DC/RST), 25 is `WAKE_STM_PIN`, 33 is the wake button — hence GPIO 26 for the diag LED.

**Typical readable sequences** (STM group + ESP group per wake):
- Healthy: `1 + 1` / `1 + 1`
- STM sent OK, MQTT publish failed: `1 + 1` / `1 + 2`
- STM sent OK, ESP didn't receive it (noisy UART): `1 + 1` / `1 + 3`
- STM UART TX broken: `1 + 2` / `1 + 3`
- ESP woke, STM never woke: `—` / `1 + 3`

### PC13 current budget (important for external LED plan)

PC13/14/15 sit on the backup domain and are limited to **~3 mA drive current** — much less than the 20 mA of other STM32F103 pins. The built-in LED already draws ~1.5 mA through its 1 kΩ resistor. Any *external* LED wired in parallel to PC13 must use a resistor ≥ 1 kΩ (2.2 kΩ recommended) so total stays under 3 mA. Do **not** wire external LEDs as active-HIGH from PC13 to GND — you'll fight the built-in circuit and the direction is opposite to what the code drives.

A bright external duplicate is already implemented on **PB12** (push-pull, active HIGH, 20 mA capable). Config lives in the `USER CODE BEGIN MX_GPIO_Init_2` block in `main.c`, and `LED_Blink()` drives PC13 and PB12 in sync (inverted polarity on each side). If you'd rather move it to a different pin, replace `GPIO_PIN_12` / `GPIOB` in three places: the GPIO init in `MX_GPIO_Init`, the two `HAL_GPIO_WritePin` calls in `LED_Blink`, and the sync writes in the safe-boot loop.

## Roadmap of fixes (do incrementally, top to bottom)

Each item lists the files involved and the "done" condition. Pick one, finish it, commit, move on. Mark done by replacing `[ ]` with `[x]`.

### 1. `[ ]` Add `/api/sensors/{deviceId}/latest` endpoint
Smallest change, biggest visible impact — Dashboard currently has to pull full history just to show current values.
- Backend: in `MeasurementRepository` add `findFirstBySensorAndTypeOrderByTimestampDesc(Sensor, String)`; in `SensorController` add `GET /api/sensors/{deviceId}/latest` returning a map `{ "temperature": 24.5, "soil1": 78, ... }` built by querying each type once.
- Frontend: switch `Dashboard.jsx` (and `SensorCard`) to use it instead of `getSensorHistory`.
- **Done when:** Dashboard loads with one request per sensor instead of pulling the whole history.

### 2. `[ ]` Move hardcoded secrets/config out of source
- ESP `src/main.cpp`: move `WIFI_SSID`, `WIFI_PASS`, `MQTT_SERVER`, `MQTT_USER`, `MQTT_PASS`, `DEVICE_ID` into `platformio.ini` `build_flags` as `-DWIFI_SSID=\"...\"`; create `ESP/secrets.ini` (gitignored) and `ESP/secrets.ini.example`, include via `extra_configs = secrets.ini` in `platformio.ini`.
- Backend `MqttConfig.java`: read `MQTT_BROKER` (already declared in `docker-compose.yml` but ignored), plus `MQTT_USERNAME` / `MQTT_PASSWORD` via `@Value("${mqtt.broker}")`. Add defaults to `application.properties`.
- **Done when:** `git grep -E '(FRITZ|GardenSensors|46\.224\.)'` returns nothing in tracked files.

### 3. `[ ]` Real logging + proper HTTP error responses
- Replace every `System.out.println` and `e.printStackTrace()` with SLF4J (`private static final Logger log = LoggerFactory.getLogger(...)`). In `MqttMessageHandler.handleMessage`, log the topic and the first ~200 chars of the payload on error so we can diagnose bad messages.
- In `SensorController`, replace `throw new RuntimeException("Sensor not found")` with a `SensorNotFoundException` annotated `@ResponseStatus(NOT_FOUND)` (or return `ResponseEntity.notFound()`).
- **Done when:** A malformed MQTT payload produces a useful log line; missing-sensor REST calls return 404, not 500.

### 4. `[ ]` Remove redundant wake-pin trigger
The ESP both pulses `WAKE_STM_PIN` (GPIO25 → STM32 PA1 EXTI) **and** sends `MEASURE\n` over UART. Pick one — keep UART, drop the pin:
- ESP `src/main.cpp`: remove the `WAKE_STM_PIN` `pinMode`/`digitalWrite` lines in `setup()` and `loop()`.
- STM32 `Core/Src/main.c`: remove the `GPIO_PIN_1` branch from `HAL_GPIO_EXTI_Callback`; in `MX_GPIO_Init` drop the PA1 EXTI config and the `EXTI1_IRQn` enable. (Keep PA3 button — that's the local user button.)
- **Done when:** Disconnecting GPIO25 from PA1 has no effect on measurement cadence.

### 5. `[ ]` Collapse `Measurement` schema: one row per sample (group-aware)
Currently every sample writes 5 rows. With the target 30-min cadence × 4 groups that's only ~192 rows/group/day, but "latest state" still needs one query per field. Design the new schema so it's already compatible with the 4-group target architecture (see §Target hardware architecture).
- New entity `Sample(id, sensor, temperature, soil1, soil2, soil3, soil4, timestamp)` — one row per group per wake. Assumes the **group-as-device** decision; if board+group is chosen instead, add a `groupId` column.
- Migration plan (don't drop the old table immediately):
  1. Add `Sample` entity + repo; `MqttMessageHandler` writes both old `Measurement` rows **and** new `Sample` row.
  2. Add new endpoints reading from `Sample`; switch frontend over.
  3. Backfill old data into `Sample` with a one-shot SQL script.
  4. Stop writing `Measurement`; drop the table in a follow-up commit.
- **Done when:** Frontend works only against `Sample`; `Measurement` table dropped; `Sample` row count == previous `Measurement` count ÷ 5.

### 6. `[ ]` Lock down public MQTT exposure
The compose file binds Mosquitto to `0.0.0.0:1883` (everything else is `127.0.0.1:`). Options, pick one:
- **Easiest:** enable TLS on Mosquitto (port 8883), update ESP to `WiFiClientSecure`, update backend URI to `ssl://mqtt:8883`. Generate a cert with Let's Encrypt on the Hetzner box.
- **Cheapest:** put the ESP on a Tailscale/WireGuard tunnel into the server and rebind MQTT to `127.0.0.1:1883`.
- **Done when:** `nmap` from outside the server can't reach port 1883 in plaintext, or it's TLS-only.

### 7. `[ ]` Multi-ADS1115 driver + 4-group reading
Prerequisite: pick I2C addresses by jumpering the ADDR pin of each ADS1115 (`GND/VDD/SDA/SCL` → `0x48/0x49/0x4A/0x4B`).
- STM32 `Core/MyLib/ads1115.{c,h}`: the driver already takes an instance struct — just instantiate 4 `ADS1115_t` and call `ADS1115_Init` per address. Verify `ADS1115_IsReady` on each at boot, log which addresses respond.
- `Core/Src/moisture_sensor.c`: extend `MoistureSensor_Read` to accept an ADC instance (or add `MoistureSensor_ReadFrom(ads, channel)`).
- `main.c` `MeasureAndDisplay()`: loop over 4 groups × 4 channels. Send one MQTT message per group (see decision §1 in target architecture).
- ESP `src/main.cpp`: in the UART RX loop, treat any complete line starting with `{` as a payload and republish to `smartgarden/{deviceIdFromPayload}/data`. STM32 must include `deviceId` in each line (already does).
- **Done when:** Backend shows 4 sensor rows, each receiving its own readings.

### 8. `[ ]` Per-group DS18B20
Pick one of:
- **Shared 1-Wire bus** (one GPIO, 4 devices, ROM-addressed): need to upgrade `ds18b20.c` to enumerate ROM IDs via Search ROM and read each by address. More driver work, fewer pins.
- **Separate pins** (4 GPIO, one DS18B20 each): trivial — instantiate the existing driver 4 times with different pin macros. Recommended for first cut.
- **Done when:** `temperature` field in each group's MQTT payload reflects its own DS18B20.

### 9. `[ ]` Sleep/wake cycle (30-min RTC alarm + button)
- STM32 `Core/Src/main.c`: configure RTC alarm (CubeMX → `.ioc` → activate RTC, alarm A every 30 min). On alarm interrupt → `measure_flag = 1`. After `MeasureAndDisplay()` returns, call `HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI)`. On wake, re-init clocks (HSE is off in STOP).
- Keep PA3 button EXTI as a manual wake source — it already sets `measure_flag`, just make sure EXTI is configured as a STOP-mode wake source.
- ESP: decide per design decision §2. If ESP stays online, no firmware change beyond removing the 10-second poll loop (already irrelevant once STM32 self-times). If ESP also sleeps, add a wake-from-STM32 GPIO line (reverse of the now-removed `WAKE_STM_PIN`).
- **Done when:** Multimeter shows STM32 in STOP between cycles; button press still triggers an immediate measurement.

### 10. `[ ]` Downlink command channel (irrigation foundation)
- Backend: new endpoint `POST /api/sensors/{deviceId}/cmd` that publishes to MQTT `smartgarden/{deviceId}/cmd` with payload `{"action":"water","groupId":N,"durationSec":S}`. Add a `CommandLog` table so we can audit what was sent.
- ESP `src/main.cpp`: subscribe to `smartgarden/+/cmd` in `connectMQTT()`. On message, write the raw JSON line + `\n` to `Serial2`.
- STM32 `Core/Src/main.c`: extend the UART RX state machine — currently it only recognises `"MEASURE"`. Parse incoming JSON commands (lightweight: jsmn or hand-rolled, since payloads are small) and set a `pending_cmd` struct. Main loop reacts: drive a relay GPIO HIGH for `durationSec`. Define a GPIO for the relay output now (e.g. PB10–PB13 for 4 groups), even if no relay is wired yet.
- **Done when:** `curl -X POST /api/sensors/1/cmd -d '...'` toggles the right GPIO on the STM32 for the requested duration.

### Not on the list (intentionally)
- Auth on the REST API — README explicitly says private project, no auth by design.
- `ddl-auto=update` → Flyway — fine while the schema is one developer, revisit only when item 5 lands or a second deployment exists.
- Backend tests — won't add for its own sake; add when fixing a bug that a test would have caught.
