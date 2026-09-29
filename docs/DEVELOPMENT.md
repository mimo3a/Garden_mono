# Development Guide

This document describes the development workflow, architecture boundaries, build commands, and engineering notes for Smart Garden.

## Branching model

- `main` contains the current stable, reproducible release baseline.
- `develop` is the integration branch for ongoing work.
- `feature/*` branches are created from `develop` for individual changes and merged back into `develop` after testing.
- `develop` is merged into `main` only after the complete system has been verified on hardware and software integration tests pass.

Typical workflow:

```text
feature/* -> develop -> main
```

## Repository layout

```text
STM/                  Stable STM32F103 sensor-node firmware
STM-L476-FreeRTOS/    Next-generation STM32L476 + FreeRTOS firmware (develop branch)
ESP/                  ESP32 UART-to-Wi-Fi/MQTT gateway
garden-backend/       Spring Boot backend and PostgreSQL persistence
garden-frontend/      React dashboard
mosquitto/            MQTT broker configuration
.github/workflows/    CI/CD workflows
```

## System data flow

```text
Sensors -> STM32 -> UART -> ESP32 -> MQTT -> Mosquitto -> Spring Boot -> PostgreSQL
                                                        -> React REST API
```

The firmware/backend contract is the JSON telemetry payload. Any payload change must be coordinated across STM32, ESP32, backend parsing, and frontend presentation.

## Stable STM32F103 firmware

The stable firmware in `STM/` is responsible for:

- ADS1115 soil-moisture acquisition over I2C
- DS18B20 temperature acquisition
- battery-voltage measurement through ADC1
- UART telemetry to ESP32
- STOP/STANDBY low-power modes
- UART wake-up and reinitialization after STOP mode
- diagnostic LED patterns
- critical-battery protection

Primary IDE: STM32CubeIDE.

Headless build:

```bash
cd STM
make
```

CubeMX-generated source should keep project code inside `USER CODE BEGIN/END` regions so regeneration does not overwrite application logic.

## STM32L476 + FreeRTOS development

The next-generation firmware is developed in `STM-L476-FreeRTOS/` on `develop` and feature branches.

Current goals include:

- FreeRTOS task-based architecture
- I2C synchronization using mutexes
- ADS1115 acquisition in a dedicated sensor task
- JSON telemetry over UART
- lower-power operation using STM32L4 power modes
- separation of protocol, sensor, power-management, and diagnostics modules

The L476 implementation should remain outside `main` until it is verified as the working hardware baseline.

## ESP32 firmware

The ESP32 acts as the communication gateway:

- wakes or requests a measurement from STM32
- receives telemetry over UART
- connects to Wi-Fi and MQTT
- publishes to `smartgarden/{deviceId}/data`
- reports diagnostic status
- enters deep sleep between measurement cycles

Build and upload:

```bash
cd ESP
pio run
pio run -t upload
pio device monitor
```

Public repository credentials must remain redacted. Runtime configuration should eventually be moved out of firmware constants.

## Backend

```bash
cd garden-backend
./mvnw spring-boot:run
./mvnw test
./mvnw clean package
```

The backend subscribes to MQTT telemetry, stores measurements in PostgreSQL, and exposes the REST API used by the frontend.

## Frontend

```bash
cd garden-frontend
cp .env.example .env
npm install
npm run dev
npm run build
```

## Full local stack

```bash
docker compose up -d
```

The current compose setup expects the external Docker network `mimozalab-network`.

## CI/CD

GitHub Actions build the individual components using path-scoped workflows:

- `backend-deploy.yml` - backend build/deploy
- `frontend-deploy.yml` - frontend build/deploy
- `esp32-build.yml` - ESP32 firmware build
- `stm32-build.yml` - STM32F103 firmware build
- `release-firmware.yml` - firmware release packaging

Changes to the L476 development firmware should get a dedicated CI workflow before it becomes the stable implementation.

## Embedded engineering notes

### Low-power wake-up

After STM32 STOP-mode wake-up, USART and clocks must be restored before reliable communication. Low-power paths must always have communication timeouts so a failed network/UART transaction cannot leave the battery-powered device awake indefinitely.

### Battery measurement

The STM32F103 stable implementation measures a single-cell 18650 through a 100 kOhm / 100 kOhm divider on ADC1 IN3.

```text
mV = raw * 3300 * 2 / 4095
```

Eight samples are averaged. The longer ADC sampling time is required because of the high source impedance of the divider.

Current thresholds:

- low battery: 3400 mV
- critical battery: 3100 mV
- critical shutdown after three consecutive readings

Very low readings below the physically plausible protected-cell range are treated as an invalid/floating input rather than a real critical battery condition.

### Firmware diagnostics

The project uses LED blink patterns on both STM32 and ESP32 to make failures visible without a debugger. Keep the patterns documented in `README.md` whenever they change.

## Code-quality priorities

For future development, prefer:

- shorter, single-purpose firmware modules instead of growing `main.c`
- explicit protocol semantics for UART ACKs and error handling
- timeout-based Wi-Fi/MQTT connection attempts
- structured JSON parsing rather than manual substring parsing
- centralized sensor calibration constants
- unit/host-side tests for payload parsing and protocol edge cases
- measured current consumption for active and sleep states

## Release checklist

Before merging `develop` into `main`:

1. Build STM32, ESP32, backend, and frontend successfully.
2. Verify sensor acquisition on real hardware.
3. Verify STM32 <-> ESP32 UART communication.
4. Verify MQTT delivery and backend persistence.
5. Verify dashboard display from a fresh measurement.
6. Verify sleep/wake behavior and recovery after communication failure.
7. Confirm no credentials, build artifacts, IDE workspace files, or temporary debug files are tracked.
8. Update `README.md` if hardware, protocol, wiring, payload, or architecture changed.
