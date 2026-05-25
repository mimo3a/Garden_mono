# Smart Garden — Frontend Specification

## Hardware stack (actual)

### STM32F103C8 (Blue Pill)
- DS18B20 — температура (1-Wire)
- ADS1115 — 16-bit ADC, 4 канала (I2C) → 4 датчика влажности почвы
- Каждые 10 секунд отправляет по UART (115200):
  ```json
  {"deviceId":1,"temperature":24.50,"soil":[78,45,23,90]}
  ```
- `soil` — уже в % (0–100), калибровка: dry=25000, wet=12000

### ESP32 (DOIT DevKit V1)
- TFT дисплей (TFT_eSPI)
- Читает UART от STM32
- Публикует в MQTT брокер на топик `sensors/{deviceId}/data`
- **Текущий статус**: код-placeholder, нужно переписать (сейчас шлёт HTTP POST с захардкоженными значениями)

### Поток данных
```
STM32 --UART--> ESP32 --MQTT--> Mosquitto ---> Spring Boot ---> PostgreSQL
```

MQTT топик: `sensors/1/data`
MQTT payload:
```json
{"temperature": 24.50, "soil": [78, 45, 23, 90]}
```

---

## Backend data model

### Sensor
| Field    | Type    | Notes                       |
|----------|---------|-----------------------------|
| id       | Long    | internal PK                 |
| deviceId | Integer | уникальный, с железа        |
| name     | String  | "Group 1", редактируемо     |
| location | String  | "Грядка А", редактируемо    |

### Measurement
| Field     | Type          | Notes                                         |
|-----------|---------------|-----------------------------------------------|
| id        | Long          |                                               |
| sensor    | Sensor        | FK                                            |
| type      | String        | "temperature", "soil1", "soil2", "soil3", "soil4" |
| value     | Double        | °C для temperature, % для soil                |
| timestamp | LocalDateTime | UTC, auto                                     |

---

## API endpoints

### Существующие
| Method | Path                                       | Возвращает                      |
|--------|--------------------------------------------|---------------------------------|
| GET    | /api/sensors                               | Все датчики                     |
| GET    | /api/sensors/{deviceId}/history            | Все измерения датчика           |
| GET    | /api/sensors/{deviceId}/latest-temperature | Последние 5 измерений температуры |

### Нужно добавить в backend
| Method | Path                                                        | Назначение                      |
|--------|-------------------------------------------------------------|---------------------------------|
| GET    | /api/sensors/{deviceId}/latest                              | Последнее значение каждого типа |
| PUT    | /api/sensors/{deviceId}                                     | Изменить name / location        |
| DELETE | /api/sensors/{deviceId}                                     | Удалить датчик и все измерения  |
| GET    | /api/sensors/{deviceId}/history?type=soil1&from=...&to=...  | История с фильтром              |

---

## Страницы

### 1. Dashboard `/`
Быстрый обзор всего сада.

**Компоненты:**
- Заголовок с временем последнего обновления
- Сетка карточек SensorCard (по одной на датчик):
  - Имя + локация датчика
  - Текущая температура
  - 4 полоски влажности (soil1–soil4) с цветовым индикатором
  - Зелёный / жёлтый / красный по порогам
  - Ссылка на детальную страницу
- Предупреждение если датчик молчит >30 мин

**Данные:** `GET /api/sensors` + `GET /api/sensors/{id}/latest` на каждый

---

### 2. Список датчиков `/sensors`
Таблица всех зарегистрированных датчиков.

**Компоненты:**
- Таблица: deviceId | name | location | последний сигнал | статус
- Клик по строке → детальная страница датчика

**Данные:** `GET /api/sensors`

---

### 3. Детальная страница датчика `/sensors/:deviceId`
Полная история и графики одного датчика.

**Компоненты:**
- Заголовок: имя, локация, deviceId
- Выбор диапазона дат
- График температуры (линейный, ось X — время)
- График влажности: 4 линии (soil1–soil4) на одном графике
- Таблица измерений (пагинация по 50): timestamp | type | value
- Кнопка «Экспорт CSV»

**Данные:** `GET /api/sensors/{deviceId}/history?from=...&to=...`

---

### 4. Все измерения `/measurements`
Сырые данные по всем датчикам.

**Компоненты:**
- Фильтры: датчик (dropdown), тип (dropdown), диапазон дат
- Таблица: имя датчика | тип | значение | timestamp
- Пагинация (50 строк)
- Кнопка «Экспорт CSV»

---

### 5. Администрирование `/admin`
Управление датчиками.

**Компоненты:**
- Таблица с inline-редактированием: name, location
- Кнопки Сохранить / Отменить на каждой строке
- Кнопка Удалить с диалогом подтверждения
- Счётчик зарегистрированных датчиков

**Данные:**
- `GET /api/sensors`
- `PUT /api/sensors/{deviceId}`
- `DELETE /api/sensors/{deviceId}`

---

## Навигация
```
[Dashboard]  [Sensors]  [Measurements]  [Admin]
```
Верхний navbar, мобильный — hamburger.

---

## Пороговые значения (цветовые индикаторы)

| Тип         | Норма (зелёный) | Предупреждение (жёлтый) | Критично (красный) |
|-------------|-----------------|-------------------------|---------------------|
| temperature | 10–35 °C        | 5–10 °C, 35–40 °C       | <5 °C, >40 °C       |
| soil (%)    | 40–80 %         | 20–40 %                 | <20 %               |

---

## Tech stack
- React 18 + Vite
- React Router v6
- Recharts (графики)
- Axios (HTTP)
- TailwindCSS

---

## Структура проекта
```
garden-frontend/
├── src/
│   ├── api/          axios client + функции для каждого endpoint
│   ├── components/   SensorCard, Chart, Table, Navbar, StatusBadge
│   ├── pages/        Dashboard, SensorList, SensorDetail, Measurements, Admin
│   ├── constants/    пороги, VITE_API_URL
│   └── main.jsx
├── Dockerfile
├── nginx.conf
└── FRONTEND_SPEC.md
```

Env переменная: `VITE_API_URL=https://mimozalab.com`

---

## Заметки
- Все времена отображать в timezone браузера
- Аутентификации нет (частная сеть / личный проект)
- Интервал измерений: 10 секунд — графики могут быть плотными, нужна агрегация по минутам/часам при длинных периодах
