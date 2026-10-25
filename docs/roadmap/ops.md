# Ops — CI/CD, сервер, безопасность, документация

Порядок выполнения задаётся вехами в `docs/ROADMAP.md`. Правила — `docs/AGENT.md`.
Сервер: Hetzner Cloud (`mimozalab.com`), Docker Compose, каталог `/srv/projects/garden/backend/`, внешняя сеть `mimozalab-network`.

## Исходное состояние (на 2026-10-25)
- Workflows: `backend-deploy`, `frontend-deploy` (сборка образа → GHCR `:latest` → деплой по SSH), `esp32-build`, `release-firmware` (по тегу `v*`, пока только ESP32). Все — **только на push в `main`**; на `develop` и PR проверок нет; тестов в пайплайне нет; сборки STM32 нет (`stm32-build` удалён вместе с F103).
- `docker-compose.yml`: пароль PostgreSQL `secret` в открытом виде; MQTT опубликован на `0.0.0.0:1883`.
- `mosquitto/mosquitto.conf` в репозитории: `allow_anonymous true`, без TLS. Совпадает ли он с конфигом на сервере — **не известно** (compose монтирует `./mosquitto.conf` и `./mosquitto_passwd` из каталога сервера).
- Бэкапов БД и мониторинга в репозитории нет.

## Справка для агента
- **Деплоить, заходить на сервер и менять GitHub secrets запрещено.** Агент готовит изменения и пошаговую инструкцию; на сервере всё выполняет Алекс.
- Workflow деплоя не ломать: изменения в нём — отдельной задачей, с описанием, как проверить и как откатить.

---

## OPS-0. CI

- [ ] **OPS-0-1. CI на `develop` и PR, деплой после тестов**
  - Сделать:
    - новый workflow `ci.yml` на push в `develop` и на PR в `develop`/`main`, с фильтрами путей и отдельными jobs: `firmware-l476` (сборка из FW-0-1 + хост-тесты FW-0-3), `esp32` (`pio run` с `secrets.h.example`), `backend` (`./mvnw test`), `frontend` (`npm ci`, `lint`, `test`, `build`);
    - в `backend-deploy` и `frontend-deploy` — шаг тестов перед сборкой образа (или `needs:` на job тестов);
    - jobs, для которых ещё нет задачи-основы (L476-сборка, тесты frontend), добавлять по мере выполнения FW-0-1 / FE-0-1 — не делать падающий CI.
  - Готово, когда: YAML валиден (actionlint), деплойные workflow ведут себя как раньше при зелёных тестах; первый прогон проверяет Алекс.

---

## OPS-1. Безопасность и эксплуатация

- [ ] **OPS-1-1. Секреты из `docker-compose.yml`**
  - Сделать: `POSTGRES_PASSWORD` / `SPRING_DATASOURCE_PASSWORD` — из переменных окружения (`${DB_PASSWORD:?}`), шаблон в корневом `.env.example`; процедура смены пароля существующей БД для Алекса (`ALTER USER` до перезапуска); добавить secret в деплойный workflow.
  - Риск: неверная последовательность оставит backend без доступа к БД — в инструкции порядок шагов и откат.

- [~] **OPS-1-2. MQTT: без анонимного доступа, отдельные пользователи, ACL** `[COORD]`
  - Сначала: Алекс проверяет конфиг на сервере и сообщает результат (шаг 0 процедуры). Если там `allow_anonymous true` и порт открыт наружу — это срочно: любой может публиковать данные в `smartgarden/+/data`.
  - Сделать: `allow_anonymous false`, `password_file`, `acl_file`; пользователи: `backend` (read `smartgarden/+/data`, write `smartgarden/+/ack` — для BE-1-4, позже write `+/cmd`), по одному на устройство `device-<id>` (write только своего `data`, read только своего `ack` — для ESP-2-4). Обновить `mosquitto/mosquitto.conf` в репозитории, шаблон ACL, `docs/SECURITY_SETUP.md`. Смена пользователя ESP32 — через `secrets.h.example` (ESP), backend — env.
  - Порядок выкатки описать так, чтобы устройства не потеряли связь: сначала добавить новых пользователей, потом перепрошить ESP, потом закрыть старого.
  - Решение (2026-10-25): совместить с плановой сменой паролей Wi-Fi и MQTT (пароли были в истории ветки `feature/stm32l467-freertos`, которая раньше была на GitHub). Процедура — шаги 0–8 в `docs/SECURITY_SETUP.md`, раздел «MQTT users and ACL»: брокер → backend → ESP32, старый общий пользователь `esp32` удаляется последним (шаг 7). Пароли вводятся интерактивно; `mosquitto_passwd` — во временном контейнере в каталоге сервера, а не на смонтированном файле; файлы `mosquitto_passwd`/`mosquitto_acl` — владелец 1883:1883, режим 600; после их изменения — `docker compose restart mqtt`, а не HUP.
  - Риски:
    - отказ ACL при публикации молча отбрасывается (MQTT 3.1.1);
    - ESP строит топик из `deviceId` в payload STM — при несовпадении с ACL пользователя устройства данные пропадают без ошибки; проверка топика в шаге 3c, устранение причины — ESP-1-2 (топик из `DEVICE_ID` шлюза);
    - Backend Deploy копирует `docker-compose.yml` из `main` на сервер — изменения OPS-1-2 должны попасть в `main` до шага 3 (шаг 3a);
    - перезапуск брокера или backend во время пробуждения ESP теряет одно измерение (QoS 0) — перезапуски сразу после измерения.
  - Заметка (2026-10-25): изменения репозитория готовы в ветке `feature/OPS-1-2-mqtt-acl` (локальный коммит): `mosquitto/mosquitto.conf`, `mosquitto/acl.example`, `docker-compose.yml` (монтирование `./mosquitto_acl`, `MQTT_USERNAME` с умолчанием `esp32`), `backend-deploy.yml` (`MQTT_USERNAME`), ESP `SECRET_MQTT_USER`, `.gitignore`, `docs/SECURITY_SETUP.md`. `pio run` с `secrets.h.example` — успешно. Шаги 0–8 на сервере выполняет Алекс; в `[x]` — после шага 7 и подтверждения.

- [ ] **OPS-1-3. MQTT через TLS** `[DECISION]`
  - Зависит от: OPS-1-2
  - Варианты: (a) listener 8883 в Mosquitto с сертификатом Let's Encrypt (через уже имеющийся reverse-proxy/certbot на сервере); (b) собственный CA для MQTT; (c) только 1883 в закрытой сети через WireGuard.
  - Рекомендация: (a). Порт 1883 снаружи закрыть после перевода ESP32 (ESP-2-1).

- [ ] **OPS-1-4. Неизменяемые теги образов и откат**
  - Сделать: образы с тегом `sha-<короткий хеш>` + `latest`; compose использует переменную тега; инструкция отката на предыдущий тег в `docs/DEVELOPMENT.md`.

- [ ] **OPS-1-5. Бэкапы PostgreSQL и проверка восстановления**
  - Сделать: скрипт `ops/backup.sh` (`pg_dump -Fc` из контейнера, хранение N дней, копия вне сервера — Hetzner Storage Box или иное по решению Алекса), cron-строка, скрипт и инструкция восстановления на тестовую БД. Без секретов в репозитории.
  - Готово, когда: Алекс один раз восстановил бэкап в отдельный контейнер и сравнил число строк.

- [ ] **OPS-1-6. Healthcheck и мониторинг**
  - Сделать: Spring Boot Actuator `/actuator/health` (только health, без деталей наружу), `healthcheck` в compose для backend/db/mqtt; внешняя проверка доступности (например, бесплатный uptime-монитор — выбор за Алексом); ротация логов Docker.

---

## OPS-2. Релиз

- [ ] **OPS-2-1. Релиз develop → main (L476 становится стабильной версией)** `[HW]`
  - Зависит от: все задачи вехи M2
  - Сделать:
    - пройти Release checklist из `CLAUDE.md`; агент готовит чек-лист с результатами сборок и тестов, проверки на железе — Алекс;
    - `release-firmware.yml`: добавить сборку L476 (`.elf`/`.bin`/`.hex`) в релиз; первый релиз V2 — тег `v2.0.0`;
    - `CLAUDE.md`, `README.md`, `docs/DEVELOPMENT.md`: L476 — рабочая версия на устройстве; `v1-f103` остаётся архивом;
    - процедура перехода устройства с F103 на L476 и отката на `v1.0.x` — для Алекса.

---

## OPS-3. Портфолио

- [ ] **OPS-3-1. README прошивки L476**: назначение, схема подключения (пины из `.ioc`), диаграмма состояний (STOP → wake → MEASURE → measure → TX → ACK → STOP), сборка, ссылки на `docs/protocol.md`, `docs/power.md`, `docs/calibration.md`, `docs/hardware.md`.
- [ ] **OPS-3-2. Диаграммы**: архитектура системы, задачи FreeRTOS, sequence STM32 ↔ ESP32 ↔ MQTT ↔ backend (Mermaid в README).
- [ ] **OPS-3-3. Результаты**: таблица тока и автономности из `docs/power.md`, график батареи за неделю (скриншот dashboard), фото стенда (Алекс).
- [ ] **OPS-3-4. Engineering notes**: раздел «Решённые проблемы» — EXTI на RX и потеря первого байта, VREFINT/VDD, IWDG в STOP (F1 против L4), I2C recovery, ложные 100 % влажности, окно online, буфер PubSubClient. Короткие, с причиной, решением и проверкой.

---

## Задачи, найденные при работе
(агент добавляет сюда новые задачи с пометкой `(найдено при <ID>)`)
