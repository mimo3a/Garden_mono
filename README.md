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
