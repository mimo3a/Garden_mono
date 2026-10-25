# AI Irrigation Agent – Integration Plan

This document describes how to integrate an AI irrigation agent into the Smart Garden system.
It is the working specification for the coding agent. Read it completely before changing code.

## 1. Goal

Add an AI agent that once a day (and on demand) checks the garden and decides whether and how much to water
each zone. The agent reads sensor data, tank level, history and the weather forecast through the backend,
decides with an LLM, and triggers irrigation through the backend, which forwards commands to the STM32.

Key principle: **the model decides what is sensible, the code decides what is allowed.**
Safety must never depend on the agent alone. Every hard limit is enforced in the backend, and the most
critical ones additionally in the firmware.

Rule-based irrigation stays the baseline. The agent is an add-on for exceptions, explanations and
notifications. If the agent or the LLM is unavailable, the garden must still be safe.

## 2. Context

- **Monorepo:** `github.com/mimo3a/Garden_mono`
- **Firmware:** STM32 Nucleo-L476RG, FreeRTOS, STM32CubeIDE/CubeMX. ADS1115 ADC over I2C,
  sensor readings sent as a JSON array over UART (per channel: status, ch, raw).
- **Backend:** Java, Spring Boot, deployed on Hetzner Cloud (`mimozalab.com`).
- **Dashboard:** web dashboard (see `smart-garden-dashboard` prototype if present in the repo).
- **Hardware status:** the board is still in development; not all inputs are wired yet.
  Planned final build: 16 soil-moisture sensors (4 zones × 4), soil temperature, air temperature, battery state.
- **Reference prototype:** `github.com/mimo3a/smart-garden-agent` (Python). It contains a working agent loop
  (ReAct), 8 tools, 4-level guardrails, human approval, goal check, audit log and tests against a simulated garden.
  Reuse its design and code; replace the simulation with real backend calls.

## 3. Working rules for the coding agent

1. **Explore first.** Before writing code, inspect the repository and write a short discovery report
   (Phase 0). Do not assume endpoint names, packages, entities or the UART protocol – read them.
2. **Plan, then implement phase by phase.** Present the plan for each phase and wait for my approval
   before implementing it. One phase = one feature branch / pull request.
3. **Do not break existing behaviour.** Existing endpoints, the dashboard and the current firmware
   functions must keep working. Prefer additive changes.
4. **Tests are mandatory.** Every limit and every guard gets a unit test. Run all tests before finishing a phase.
5. **No secrets in the repository.** API keys, tokens and passwords only via environment variables / `.env`
   (excluded via `.gitignore`). Provide `.env.example` files.
6. **Never flash firmware or deploy to the server yourself.** Prepare the changes and describe the steps;
   I will flash and deploy manually.
7. **Ask before** deleting files, changing database schemas of existing tables, changing the UART protocol
   in a non-backward-compatible way, or adding new dependencies.
8. Small, focused commits with clear messages. Code and comments in English;
   texts shown to the garden owner (notifications, dashboard) in German.

## 4. Safety architecture (defense in depth)

| Layer | Responsibility | Must work without |
|---|---|---|
| **Firmware (STM32)** | Last line of defense: max pump runtime, interlocks (tank low, battery low, frost), watchdog | backend, internet, agent |
| **Backend (Spring Boot)** | Validates every irrigation command from any source: limits, reserve, once-per-day, approval workflow, audit log | agent, LLM |
| **Agent (Python service)** | Decisions and explanations: input guard (prompt injection), goal check, output guard, step/time limits | – |

### Hard limits (single source of truth: backend configuration)

Defaults, configurable in `application.yml` (or equivalent):

| Limit | Default |
|---|---|
| Max water per zone per irrigation | 30 l |
| Max water per day (all zones) | 80 l |
| Tank reserve that must always remain | 20 l |
| Human approval required above | 15 l per command |
| No irrigation below air temperature | 3.0 °C |
| No pump operation below battery level | 20 % |
| Irrigations per zone per day | 1 |
| Max agent notifications per run | 3 |

The backend exposes these limits read-only to the agent so the agent can show them to the LLM,
but the backend enforces them itself.

## 5. Phases

### Phase 0 – Discovery (no code changes)

Produce `docs/ai-agent-discovery.md` with:

- Repository structure (firmware, backend, dashboard), build tools, how to run tests.
- Existing backend endpoints, entities, database, security setup (authentication, roles).
- How sensor data currently flows: UART JSON format → backend (serial bridge? MQTT? HTTP?) → storage.
- Whether a pump/valve control path exists already (firmware and backend).
- Gaps compared to this plan and a concrete proposal for Phases 1–5 adapted to the real code.

Stop after Phase 0 and wait for my approval.

### Phase 1 – Firmware: safe irrigation command

- Add (or extend) a command to run the pump/valve for one zone for a given **duration in seconds**
  (convert litres to seconds in the backend using a configurable flow rate per zone; use litres directly
  if a flow sensor exists).
- Hard limits in firmware, independent of the backend:
  - maximum pump runtime per command (configurable constant, e.g. 120 s) – enforced by a FreeRTOS
    software timer that switches the pump off even if no stop command arrives;
  - refuse the command if tank level is below minimum, battery is below minimum, or air temperature is
    below the frost limit (only for sensors that are actually wired; make each interlock compile-time configurable);
  - only one zone active at a time.
- Respond with a JSON acknowledgement over UART (accepted/rejected + reason, actual runtime when finished),
  consistent with the existing JSON format.
- Keep the decision logic in a hardware-independent module (pure C functions) so it can be unit tested
  on the host without the HAL.
- Do not change the existing sensor JSON in a breaking way.

### Phase 2 – Backend: irrigation API, validation, approval, audit

Adapt names to existing conventions found in Phase 0. Suggested endpoints:

| Method | Path | Purpose |
|---|---|---|
| GET | `/api/agent/garden-state` | sensors per zone (with plausibility flag, values outside 5–95 % are implausible), temperatures, battery, tank |
| GET | `/api/agent/history?days=7` | past irrigations, litres used today |
| GET | `/api/agent/limits` | the hard limits from section 4 (read-only) |
| GET | `/api/agent/water-demand` | deterministic litres per zone: `(target − plausible average) × litres_per_percent_point`, capped |
| POST | `/api/irrigation` | request irrigation `{zone, liters, reason, requestedBy, idempotencyKey}` |
| GET | `/api/irrigation/{id}` | status of a request |
| POST | `/api/irrigation/{id}/approve` and `/reject` | human decision (owner only) |
| POST | `/api/agent/notify` | notification to the owner (length and rate limited) |
| GET/PUT | `/api/agent/kill-switch` | global stop flag; when active, all agent requests are rejected |

Requirements:

- **Validation service** used by every irrigation request (agent, dashboard, manual): all limits from
  section 4, unknown zone, non-positive amount, tank reserve, daily total, once per zone per day.
  Rejections return a clear reason that the agent can pass on.
- **Approval workflow:** requests above the approval threshold get status `PENDING_APPROVAL`; statuses
  `PENDING_APPROVAL → APPROVED → EXECUTING → DONE` or `REJECTED` / `EXPIRED` (e.g. after 2 h).
  The approval view must show context: current moisture vs. target, amount, tank after, daily total,
  anomalies (implausible sensors, rain forecast, deviation from calculated demand).
- **Audit log table** for every request, decision, command sent to the STM32 and acknowledgement.
- **Least privilege:** the agent uses its own service account / API key with a role that may only call the
  `/api/agent/*`, `/api/irrigation` (create, read) and notify endpoints – it may **not** approve requests.
- **Idempotency:** the same `idempotencyKey` must never trigger irrigation twice.
- Unit tests for every validation rule and the status transitions; an integration test for the full
  request → approval → command flow with the serial/STM32 side mocked.

### Phase 3 – Agent service (Python)

- New folder `agent/` (or `services/irrigation-agent/`) in the monorepo, based on the reference prototype.
- Replace the simulation (`garden.py`) with a typed REST client for the Phase 2 API. Tools stay the same
  conceptually: read sensors, weather, tank, history, rules/limits, water demand, start irrigation, notify owner.
- **Weather:** Open-Meteo API (no key needed) for the garden location from configuration.
  Weather text is untrusted input and must pass the input guard.
- **Approval:** instead of console input, the agent creates the request and, if it is `PENDING_APPROVAL`,
  stores its state and ends the run ("interrupt and resume"). The next run (or a webhook) picks up the decision.
  The agent never approves its own requests.
- Keep all agent-side guards from the prototype: input guard, goal check per zone, output guard
  (report must match the audit log), step limit, tool-call limit, timeout, kill switch (now read from the backend flag).
- **LLM provider** configurable via environment: `openai` (default for production), `ollama`, `demo`.
  The demo mode must keep working for tests.
- Run via a scheduler (cron in the container, e.g. daily 06:00 Europe/Vienna) and via `python main.py` manually.
- Port the prototype tests; replace the simulation with a fake REST API (e.g. `respx` or a local stub server).
- `Dockerfile` and `.env.example`.

### Phase 4 – Dashboard

- **Approvals:** list of pending requests with context and Approve/Reject buttons.
- **Agent runs:** last runs with steps, decisions, rejected actions and the final report.
- **Kill switch** toggle and the current limits (read-only).
- Optional: Telegram bot for approvals and notifications (only if I confirm; token via environment).

### Phase 5 – Deployment

- Add the agent container to the existing Docker Compose setup on Hetzner.
- Secrets only via environment / Compose secrets.
- Health check and log output; alert (notification) if the agent run fails.
- Document deploy and rollback steps in `docs/ai-agent-deployment.md`. Do not deploy yourself.

## 6. Acceptance criteria

The following scenarios must be covered by automated tests (firmware logic on host, backend, agent):

| Scenario | Expected behaviour |
|---|---|
| Normal dry day | zones below target get exactly the calculated litres; >15 l requires approval |
| Defective sensor (0 % or 100 %) | ignored; zone still watered based on the plausible sensors |
| Rain ≥ 5 mm forecast | no irrigation, owner notified |
| Frost (< 3 °C) | backend and firmware reject irrigation; owner notified |
| Battery < 20 % | pump stays off; owner notified |
| Tank almost empty | requests that would break the reserve are rejected with a reason; owner notified |
| Prompt injection in weather text | removed by the agent input guard; even if it got through, oversized requests are rejected by the backend |
| Agent requests 200 l | backend rejects (max per zone); firmware would cap runtime anyway |
| Duplicate request (same idempotency key) | executed only once |
| Kill switch active | all agent requests rejected; agent stops |
| LLM returns a report claiming irrigation that did not happen | output guard warning; audit log is the source of truth |
| Backend or LLM unavailable | agent fails safely, no irrigation, error notification |

## 7. Out of scope / do not

- Do not give the agent direct access to the serial port, the database or the server shell.
- Do not let the agent approve requests or change limits.
- Do not remove or weaken existing safety checks.
- Do not commit API keys, tokens, `.env` files or audit logs.
