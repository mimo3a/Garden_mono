# Smart Garden

Система мониторинга сада: температура почвы и влажность в реальном времени.

## Архитектура

```
STM32 --UART--> ESP32 --MQTT--> Mosquitto --> Spring Boot --> PostgreSQL
                                                                  |
                                                             React Frontend
```

## Компоненты

| Папка | Технология | Назначение |
|-------|-----------|------------|
| `garden-backend/` | Spring Boot 3.5 + Java 21 | REST API, MQTT listener, PostgreSQL |
| `garden-frontend/` | React 18 + Vite + Tailwind | Веб-интерфейс |
| `ESP/` | ESP32 + PlatformIO | WiFi мост: UART от STM32 → MQTT |
| `STM/` | STM32F103 (Blue Pill) | Чтение датчиков, отправка по UART |

## Железо

- **STM32F103C8** — DS18B20 (температура) + ADS1115 + 4 датчика влажности почвы
- **ESP32 DOIT DevKit V1** — TFT дисплей, WiFi, MQTT клиент
- Измерения каждые **10 секунд**
- Данные влажности уже в % (0–100)

### MQTT

- Топик: `smartgarden/{deviceId}/data`
- Payload: `{"deviceId":1,"temperature":24.50,"soil":[78,45,23,90]}`
- Брокер: Mosquitto, порт 1883

## API

| Method | Endpoint | Описание |
|--------|----------|----------|
| GET | `/api/sensors` | Все датчики |
| GET | `/api/sensors/{id}/history` | История измерений |
| GET | `/api/sensors/{id}/latest-temperature` | Последние 5 температур |
| PUT | `/api/sensors/{id}` | Обновить name/location |
| DELETE | `/api/sensors/{id}` | Удалить датчик |

## Frontend — страницы

| Страница | URL | Описание |
|----------|-----|----------|
| Dashboard | `/` | Карточки датчиков с текущими значениями |
| Sensors | `/sensors` | Список всех датчиков, статус online/offline |
| Sensor Detail | `/sensors/:id` | Графики температуры и влажности, экспорт CSV |
| Measurements | `/measurements` | Все измерения с фильтрами, экспорт CSV |
| Admin | `/admin` | Редактировать и удалять датчики |

## CI/CD

Push в `main` → GitHub Actions → Docker образ в GHCR → деплой на Hetzner по SSH.

| Workflow | Триггер | Действие |
|----------|---------|----------|
| `backend-deploy.yml` | изменения в `garden-backend/` | сборка + деплой |
| `frontend-deploy.yml` | изменения в `garden-frontend/` | сборка + деплой |
| `esp32-build.yml` | изменения в `ESP/` | компиляция, артефакт |
| `stm32-build.yml` | изменения в `STM/` | компиляция, артефакт |

### GitHub Secrets

| Secret | Описание |
|--------|----------|
| `HETZNER_HOST` | IP сервера |
| `HETZNER_USER` | SSH пользователь |
| `HETZNER_SSH_KEY` | Приватный SSH ключ |
| `GHCR_TOKEN` | GitHub PAT (`write:packages`) |

## Локальный запуск

### Backend
```bash
cd garden-backend
./mvnw spring-boot:run
```

### Frontend
```bash
cd garden-frontend
cp .env.example .env
npm install
npm run dev
# http://localhost:5173
```

## Сервер (Hetzner)

Файлы на сервере: `/srv/projects/garden/backend/`

```bash
# Логи backend
docker logs -f garden-backend

# Перезапуск
cd /srv/projects/garden/backend
docker compose up -d --no-deps backend
```

## Current hardware wiring and sleep cycle

Power cycle:

1. ESP32 wakes up by timer every 30 minutes, or by the manual wake button.
2. ESP32 wakes STM32 with a pulse from `GPIO25` to STM32 `PA1`.
3. STM32 measures sensors and sends one JSON line over UART.
4. ESP32 receives the JSON, publishes it to MQTT, then enters deep sleep.
5. STM32 enters STOP mode after the measurement.

Required wiring:

| Signal | ESP32 | STM32 / other side |
|---|---|---|
| STM32 wake | `GPIO25` | `PA1` |
| Manual wake button | `GPIO33` | Button contact 1 |
| Manual wake button GND | `GND` | Button contact 2 |
| Common ground | `GND` | STM32 `GND` |

Yes: one button contact goes to ESP32 `GPIO33`, the other button contact goes to `GND`.
The code uses `INPUT_PULLUP`, so no external pull-up resistor is required for a basic button.

STM32 CubeMX settings:

- `PA1`: `GPIO_EXTI1`, rising edge, pull-down.
- `EXTI1_IRQn`: enabled.
- `PA3`: no interrupt; old STM32 button is not used for the main wake cycle.
- RTC on STM32 is not required for the current 30-minute cycle; ESP32 owns the timer.

## STM32 command-line build

From PowerShell:

```powershell
cd C:\Garden\Workspace\STM
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Config Release -Clean
```

Firmware outputs:

```text
C:\Garden\Workspace\STM\build\cli\Release\F103First.elf
C:\Garden\Workspace\STM\build\cli\Release\F103First.hex
C:\Garden\Workspace\STM\build\cli\Release\F103First.bin
```

For STM32CubeProgrammer, select:

```text
C:\Garden\Workspace\STM\build\cli\Release\F103First.hex
```

If flashing the `.bin` instead, use start address:

```text
0x08000000
```
