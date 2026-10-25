# AI Irrigation Agent – Phase 0 Discovery

Produced after reading the full repository. No code was changed.

---

## 1. Repository structure and build tools

```
STM-L476-FreeRTOS/      STM32L476 + FreeRTOS V2 firmware (C, STM32CubeIDE only)
ESP/                    ESP32 gateway firmware (C++, PlatformIO / Arduino)
garden-backend/         Spring Boot 3.5, Java 21, Maven
garden-frontend/        React 18, Vite, Tailwind CSS, npm
mosquitto/              Eclipse Mosquitto config + password file
docs/                   Documentation (this file, DEVELOPMENT.md, SECURITY_SETUP.md)
.github/workflows/      CI/CD (backend-deploy, frontend-deploy, esp32-build, release-firmware)
```

### Build commands

| Component | Command |
|---|---|
| Backend | `cd garden-backend && ./mvnw spring-boot:run` |
| Backend tests | `cd garden-backend && ./mvnw test` |
| Frontend dev | `cd garden-frontend && npm run dev` |
| ESP32 | `cd ESP && pio run && pio run -t upload` |
| STM32F103 (V1) | `git checkout v1-f103 && cd STM && make` |
| Full stack | `docker compose up -d` (requires external network `mimozalab-network`) |

### Tests

**There are zero test files in the repository.** The `garden-backend/src/test/` directory does not exist. The test dependency (`spring-boot-starter-test`) is declared in `pom.xml` but unused. This is a significant gap that Phase 2 must address before any safety-critical backend logic ships.

---

## 2. Backend: endpoints, entities, security

### REST endpoints (all currently existing)

| Method | Path | Auth | Description |
|---|---|---|---|
| GET | `/api/auth/csrf` | public | Returns CSRF token |
| POST | `/api/auth/login` | public | Session login |
| GET | `/api/auth/me` | session | Current user info |
| POST | `/api/auth/logout` | session | Invalidate session |
| POST | `/api/auth/accept-invitation` | public | Create account via token |
| GET | `/api/sensors` | session | List sensors visible to user |
| GET | `/api/sensors/{deviceId}/history` | session | All measurements for sensor |
| GET | `/api/sensors/{deviceId}/latest-temperature` | session | Last 5 temperature readings |
| PUT | `/api/sensors/{deviceId}` | session | Update sensor name/location |
| DELETE | `/api/sensors/{deviceId}` | session | Delete sensor + measurements |
| POST | `/api/admin/invitations` | ADMIN role | Create invitation token |

No irrigation, pump, agent, or audit endpoints exist yet.

### Database entities

```
AppUser          id, email, passwordHash, role (ADMIN|USER)
Sensor           id, deviceId (unique int), name, location, owner_id → AppUser
Measurement      id, sensor_id → Sensor, type (varchar), value (double), timestamp
Invitation       id, token (uuid), email, deviceIds[], expiresAt, acceptedAt
```

Schema is managed by Hibernate DDL (`spring.jpa.hibernate.ddl-auto=update`). There is no Flyway/Liquibase — migrations are applied automatically on startup.

**No zone concept exists.** The plan requires "4 zones × 4 sensors". Currently `Sensor` maps one device (by integer `deviceId`) to a flat list of `Measurement` rows with types `temperature`, `soil1`–`soil4`, `battery`. A zone grouping must be added in Phase 2.

### Authentication and security

- Session-based (HttpSession); no JWT.
- CSRF: `CookieCsrfTokenRepository`, custom `SpaCsrfTokenRequestHandler` for SPA.
- Roles: `ADMIN` and `USER`. Only ADMIN can access `/api/admin/**`.
- No service-account role for the agent exists yet. The plan requires a least-privilege `AGENT` role that cannot approve requests.
- Password minimum: 12 characters (enforced in `AuthController.acceptInvitation`).
- Secure cookies (`APP_SECURITY_COOKIE_SECURE=true`) in production.

---

## 3. Sensor data flow

```
Sensors (ADS1115 × 1-2, DS18B20, battery ADC)
  │
  ▼ I2C / one-wire / ADC
STM32 (measures on "MEASURE" command)
  │
  ▼ UART 115200 baud, single JSON line + \r\n
ESP32 (receives, validates, publishes)
  │
  ▼ MQTT QoS 1, topic: smartgarden/{deviceId}/data
Mosquitto broker (docker container, port 1883, password auth)
  │
  ▼ Spring Integration inbound adapter (subscribes to smartgarden/+/data)
MqttMessageHandler.java (parses JSON, auto-creates Sensor, saves Measurement rows)
  │
  ▼ PostgreSQL → REST API → React frontend
```

### UART protocol (both firmware versions)

**Trigger**: ESP32 sends `MEASURE\n` (also `MEASURE\r\n` accepted). The UART line falling edge wakes the STM32 from STOP mode.

**STM32F103 response** (stable, `main`):
```json
{"deviceId":2,"temperature":24.50,"soil":[78,45,23,90],"adsReady":true,"soilRaw":[8200,11800,14100,7600],"battery":3812}
```

**STM32L476 response** (`develop`, current — not yet compatible with backend):
```json
{"channels":[{"dev":0,"ch":0,"status":0,"raw":8200},{"dev":0,"ch":1,"status":0,"raw":11800},…]}
```
The L476 format is different from what `MqttMessageHandler` expects. It does not include `temperature`, `battery`, or calibrated `soil` values yet.

**ACK**: ESP32 sends `OK\n` after MQTT publish (regardless of MQTT result). STM32 waits up to 30 s for ACK, then enters STOP mode anyway.

### MqttMessageHandler parsing

Extracts from JSON: `temperature` (nullable), `soil[0..n]` stored as `soil1`, `soil2`, …, `battery`. Device ID comes from the MQTT **topic** (`smartgarden/{deviceId}/data`), not from the JSON body. Auto-creates a `Sensor` entity named `"Group {deviceId}"` on first message.

### MQTT backend: inbound only

The backend has **no outbound MQTT capability**. `MqttConfig` configures only an inbound adapter (subscribes to `smartgarden/+/data`). To send commands to the STM32 via ESP32, Phase 2 must add a Spring Integration outbound channel adapter that publishes to a new topic (e.g. `smartgarden/{deviceId}/cmd`).

---

## 4. Pump / valve control path

**Does not exist anywhere in the codebase.**

| Layer | Status |
|---|---|
| STM32F103 firmware | No pump/valve GPIO, no irrigation command handling |
| STM32L476 firmware | No pump/valve GPIO, no irrigation command handling |
| ESP32 firmware | No MQTT subscribe, no command forwarding to STM32 |
| Backend | No irrigation API, no MQTT outbound, no command routing |
| Frontend | No irrigation controls |

The full pump/valve path must be designed and built from scratch in Phases 1 and 2.

**GPIO availability**: The STM32F103 currently uses PA9/PA10 (UART), PB12 (LED), PC13 (LED), PA3 (ADC battery), I2C1 (PB6/PB7), one-wire DS18B20. Several GPIO pins are available for pump/valve control. The L476 uses similar peripherals. Pin assignment must be confirmed before Phase 1.

---

## 5. Gaps vs. the plan and proposal for Phases 1–5

### Critical gaps

| Gap | Impact |
|---|---|
| No tests exist | Must establish test infrastructure in Phase 2 before any safety logic ships |
| No MQTT outbound in backend | Needed for Phase 2 to send commands to ESP32/STM32 |
| No zone concept | Required for the agent's per-zone watering decisions |
| No pump/valve hardware wiring | Phase 1 cannot proceed without hardware confirmation |
| L476 JSON differs from backend format | L476 cannot talk to backend until formats align |
| No AGENT role / service account | Required for least-privilege agent access in Phase 3 |
| No flow sensor / flow rate config | Litres→seconds conversion needs a configurable constant per zone |
| No tank level sensor | The plan requires tank reserve enforcement; currently not sensed |
| No air temperature sensor | Required for frost interlock (plan: < 3 °C blocks irrigation) |

### Clarifications confirmed

- **V1 = STM32F103** (archived on the `v1-f103` branch). **V2 = STM32L476 + FreeRTOS** (`STM-L476-FreeRTOS/`, `develop` branch) — this is the active platform.
- **Pump/valve control is planned after sensor reading is complete on L476.** Phase 1 firmware work must wait until DS18B20, battery ADC, and a backend-compatible JSON format are working on L476.

### Open questions before Phase 1 (pump)

1. **Pump/valve wiring**: Which GPIO pins on the L476 Nucleo are available and wired? How many zones physically exist?
2. **Tank level sensor**: Is one planned? Without it, the 20 l reserve interlock cannot be enforced in firmware.
3. **Air temperature sensor**: Needed for frost interlock (< 3 °C). Is one wired?

### Proposed adapted phase plan

#### Pre-Phase 1 prerequisite – Complete L476 sensor reading
Before any pump work, complete on `develop`:
- All 8 ADS1115 channels read and calibrated
- DS18B20 temperature integrated in FreeRTOS task
- Battery ADC integrated
- JSON output compatible with backend (`MqttMessageHandler` format)

#### Phase 1 – Firmware: safe irrigation command (L476 + FreeRTOS)
After sensor reading is complete. Add to `STM-L476-FreeRTOS/`:
- New UART command: `IRRIGATE {zone} {seconds}\n`
- GPIO output for pump/valve per zone (pin TBD, requires hardware confirmation)
- Maximum runtime cap via FreeRTOS software timer (switches pump off even if no stop command arrives)
- Interlocks (compile-time configurable): battery < 3400 mV, frost (if sensor exists), tank low (if sensor exists)
- Only one zone active at a time
- JSON ACK: `{"cmd":"irrigate","zone":1,"accepted":true,"runtime_s":60}` or rejected with reason
- Keep existing MEASURE/OK protocol untouched; put irrigation logic in a separate hardware-independent module

Also add to **ESP32** (`ESP/`):
- Subscribe to `smartgarden/{deviceId}/cmd` MQTT topic
- Forward irrigation command to STM32 over UART
- Forward STM32 ACK back to MQTT on `smartgarden/{deviceId}/ack`

#### Phase 2 – Backend: irrigation API, validation, approval, audit
Additions to `garden-backend/`:
- **Test infrastructure first**: add `src/test/java/`, at minimum one `@SpringBootTest` smoke test and unit tests for every validation rule
- **MQTT outbound adapter** in `MqttConfig` (publish to `smartgarden/{deviceId}/cmd`)
- **Zone entity or config**: map zone IDs to device IDs and flow rates (l/s); start with a simple `application.yml` config block
- **IrrigationRequest entity**: id, zone, liters, reason, requestedBy, idempotencyKey, status (PENDING_APPROVAL / APPROVED / EXECUTING / DONE / REJECTED / EXPIRED), timestamps
- **AuditLog entity**: id, request_id, event, actor, timestamp, detail
- **IrrigationController**: POST `/api/irrigation`, GET `/api/irrigation/{id}`, POST `/api/irrigation/{id}/approve|reject`
- **AgentController**: GET `/api/agent/garden-state`, `/api/agent/history`, `/api/agent/limits`, `/api/agent/water-demand`, POST `/api/agent/notify`, GET+PUT `/api/agent/kill-switch`
- **ValidationService**: all hard limits from the plan (max per zone, daily total, tank reserve, once-per-day, frost, battery, idempotency)
- **AGENT role**: new role that can call `/api/agent/**` and POST `/api/irrigation` but cannot approve
- Approval expiry scheduler (mark PENDING_APPROVAL → EXPIRED after 2 h)

#### Phase 3 – Agent service (Python)
New folder `agent/` in monorepo:
- Based on `github.com/mimo3a/smart-garden-agent` prototype (ReAct loop, guardrails, audit)
- Replace simulation client with typed REST client for Phase 2 API
- Weather via Open-Meteo (garden location from config)
- Interrupt-and-resume for approval: agent creates request, stores run state, exits; resumes on next scheduled run or webhook
- LLM provider configurable: `openai` / `ollama` / `demo`
- `Dockerfile`, `.env.example`, cron schedule (06:00 Europe/Vienna)
- Notifications to owner in **German** (per coding rules)

#### Phase 4 – Dashboard
Additions to `garden-frontend/`:
- Approvals page: pending requests with context (soil vs. target, amount, tank after, daily total)
- Agent runs log: steps, decisions, rejections, final report
- Kill switch toggle
- Limits display (read-only)
- Optional Telegram bot (confirm separately)

#### Phase 5 – Deployment
- Add `agent` service to `docker-compose.yml`
- Document deploy and rollback steps in `docs/ai-agent-deployment.md`
- Health check endpoint in the agent
- Alert notification on agent run failure

---

## Summary table

| Item | Current state | Needed for agent |
|---|---|---|
| Sensor readings (4 ch) | ✅ Working end-to-end | – |
| Pump/valve control | ❌ Not implemented | Phase 1 + ESP32 |
| Backend irrigation API | ❌ Not implemented | Phase 2 |
| Approval workflow | ❌ Not implemented | Phase 2 |
| Audit log | ❌ Not implemented | Phase 2 |
| MQTT outbound (backend) | ❌ Not implemented | Phase 2 |
| Zone concept | ❌ Not implemented | Phase 2 |
| AGENT service account | ❌ Not implemented | Phase 2 |
| Backend tests | ❌ Zero test files | Phase 2 (prerequisite) |
| Kill switch | ❌ Not implemented | Phase 2 |
| Agent Python service | ❌ Not implemented | Phase 3 |
| Dashboard approval UI | ❌ Not implemented | Phase 4 |
| Tank level sensor | ❓ Not yet wired | Clarify before Phase 1 |
| Air temp sensor | ❓ Not yet wired | Clarify before Phase 1 |
| L476 JSON ↔ backend | ❌ Incompatible | Before L476 goes to main |
