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
- Payload: `{"deviceId":1,"temperature":24.50,"soil":[78,45,23,90],"battery":3812}`
- Поле `battery` — милливольты (integer), добавлено 2026-07. Бэкенд/фронт пока игнорируют неизвестные поля — forward-compatible.
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
| STM32 wake | `GPIO25` | `PA1` *(нестабилен, см. ниже)* |
| STM32 UART RX | `GPIO16` (`RXD2`) | `PA9` (USART1 TX) |
| STM32 UART TX | `GPIO17` (`TXD2`) | `PA10` (USART1 RX) |
| Manual wake button | `GPIO33` | Button contact 1 |
| Manual wake button GND | `GND` | Button contact 2 |
| Diag LED (ESP) | `GPIO26` | LED anode → R → cathode → GND |
| Diag LED (STM, bright) | — | `PB12` → R 330 Ω → LED anode → cathode → GND |
| Common ground | `GND` | STM32 `GND` |

Yes: one button contact goes to ESP32 `GPIO33`, the other button contact goes to `GND`.
The code uses `INPUT_PULLUP`, so no external pull-up resistor is required for a basic button.

STM32 CubeMX settings:

- `PA1`: `GPIO_EXTI1`, rising edge, pull-down. **Требуется внешний резистор 4.7–10 кΩ PA1→GND** — внутренняя подтяжка (~40 кΩ) слабее наводки на проводе к ESP GPIO25 (в deep sleep у ESP этот пин в high-Z, провод работает как антенна). Без внешней подтяжки STM просыпается хаотично.
- `EXTI1_IRQn`: enabled.
- `PA3`: `ADC1_IN3` (analog input) — измерение напряжения батареи через делитель. Старая кнопка safe-boot удалена.
- `PB12`: GPIO output push-pull — дублирующий диагностический LED (яркий, active HIGH). Синхронно с встроенным PC13.
- RTC на STM32 пока не используется — таймером владеет ESP32.

## Диагностические LED

Схема: короткие импульсы 120 мс ON / 200 мс OFF, пауза 400 мс между группами, чтобы `1+2` читалось отдельно от `3`.

**STM32 — встроенный PC13 (active LOW) + внешний PB12 (active HIGH), синхронно из `LED_Blink()`.**

| Событие | Вспышек |
|---|---|
| Проснулся, начал измерение | 1 |
| UART TX успех (`HAL_UART_Transmit` = `HAL_OK`) | 1 |
| UART TX провалился / таймаут | 2 |

**ESP32 — внешний LED на GPIO 26 (active HIGH).**

| Событие | Вспышек |
|---|---|
| `setup()` после пробуждения из deep sleep | 1 |
| `client.publish()` = `true` | 1 |
| `client.publish()` = `false` | 2 |
| `readMeasurement()` таймаут (JSON от STM не пришёл) | 3 |

**Читаемые последовательности** (STM + ESP за один цикл):

- Всё ОК: `1 + 1` / `1 + 1`
- STM отправил, MQTT провалился: `1 + 1` / `1 + 2`
- STM отправил, ESP не принял (шум по UART): `1 + 1` / `1 + 3`
- STM не отправил: `1 + 2` / `1 + 3`
- ESP проснулся, STM вообще не проснулся: `—` / `1 + 3`

Пин `GPIO26` на ESP выбран потому что 2 и 4 заняты TFT, 25 — `WAKE_STM_PIN`, 33 — кнопка пробуждения.
На STM пины `PC13/14/15` из backup-домена и тянут максимум ~3 мА, поэтому яркий дубль сделан на `PB12` (обычный GPIO, 20 мА).

## Мониторинг батареи (18650)

- **Делитель:** `TP4056 OUT+` → R1 (100 кΩ) → `PA3` → R2 (100 кΩ) → `GND`.
- **НЕ подключай к `B+`** — там нет защиты DW01A, батарея убьётся от переразряда.
- Проверка мультиметром до подачи питания: напряжение на PA3 должно быть **ровно половина** от `OUT+`. Если больше 3.3 В — не подключай к STM, делитель разведён неправильно.
- Все земли соединить: `TP4056 OUT-` ↔ STM `GND` ↔ ESP `GND` ↔ минус держателя батареи.
- В JSON добавляется поле `"battery": XXXX` — милливольты.
- Заряженная 18650 показывает 4020–4050 мВ, стабильно ±3 мВ.

**Пороги** (в `main.c`):

- `BATTERY_LOW_MV = 3400` — информационно, для будущего флага `"state":"low"`.
- `BATTERY_CRITICAL_MV = 3100` — при 3 подряд отсчётах в диапазоне `2400 < mV < 3100` STM уходит в STANDBY (5 медленных вспышек и вырубается). Проснуться можно только по NRST после замены/зарядки.
- Порог 2400 мВ (нижняя граница) совпадает со срабатыванием защиты DW01A — реальная защищённая батарея физически не может опуститься ниже. Значения меньше 2400 = висящий или неподключенный PA3, это шум, не критика.

## Прошивка (клон STM32F103 — важно!)

Плата **клон**. Флеш-контроллер клонов ломается на per-sector erase, но всегда принимает full-chip erase. **Каждая прошивка должна начинаться с полного стирания:**

- STM32CubeProgrammer → `Erasing & Programming` → галочка **`Full chip erase`** → `Start Programming`.
- CLI:
  ```
  STM32_Programmer_CLI -c port=SWD mode=UR -e all -w F103First.hex 0x08000000 -v -rst
  ```

`HAL_DBGMCU_EnableDBGStopMode()` включен в `main()`, SWD выживает и в STOP — CubeProgrammer подключается через **Under Reset** и может остановить чип посреди цикла сна.

Если после `Full chip erase` всё равно не прошивается — **BOOT0=1 jumper** → RESET → прошить → BOOT0=0 → RESET. Это встроенный ROM-загрузчик, минует пользовательский код.

## Известные грабли (2026-07 debug session)

1. **STOP-mode UART corruption.** После пробуждения из STOP первая передача через USART1 идёт мусором (`␀␀␀…`). Peripheral в неопределённом состоянии. Фикс: в `EnterStopMode()` после `SystemClock_Config()` — полный `HAL_UART_DeInit(&huart1) + MX_USART1_UART_Init() + HAL_UART_Receive_IT(...)`. Уже применён в `Core/Src/main.c`.
2. **PA1 EXTI ловит наводку.** Провод от ESP GPIO25 к STM PA1 в deep sleep ESP превращается в антенну. Внутренняя подтяжка PA1 (~40 кΩ) слабее наводки — STM просыпается хаотично. Быстрый фикс: внешний резистор 4.7–10 кΩ PA1→GND. Правильный фикс — вообще убрать провод (roadmap п.4 в `CLAUDE.md`), но сначала нужно чтобы ESP слал `MEASURE\n` по UART вместо импульса на пине.
3. **PA3 болтается без делителя.** Если делитель батареи ещё не припаян, PA3 показывает случайные значения (0–3300 мВ). Отсюда `battery:884` и подобная ерунда. Ничего не сломано — просто аналоговый вход в воздухе.
4. **ADC даёт неправильные значения с CubeMX-дефолтом.** Sampling time 7.5 циклов не хватает для делителя 100 к / 100 к (Thevenin ~50 кΩ). В `USER CODE ADC1_Init 2` переопределено на 71.5 циклов. Калибровка `HAL_ADCEx_Calibration_Start(&hadc1)` вызывается в `USER CODE BEGIN 2` — CubeMX её не генерирует.

## Дальнейшая документация

Детальный developer guide — **[`CLAUDE.md`](CLAUDE.md)**. Там же roadmap известных TODO (10 пунктов, приоритезированных), архитектура целевого железа (4 группы × 4 датчика влажности + 4 DS18B20), design decisions по downlink command channel для полива.

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
