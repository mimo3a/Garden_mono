# User access and invitations

The application uses server-side sessions in `HttpOnly`, `Secure`, `SameSite=Strict` cookies. Passwords are stored as one-way hashes by Spring Security. Public registration is intentionally disabled.

## First administrator

Before deploying the security branch, add two GitHub Actions secrets in **Settings → Secrets and variables → Actions**:

- `APP_BOOTSTRAP_ADMIN_EMAIL` — the administrator's email address
- `APP_BOOTSTRAP_ADMIN_PASSWORD` — a unique password of at least 12 characters

The backend creates this administrator only when no administrator exists. It never logs the password and it is not stored in the repository. Keep the secrets in place for later deploys; they do not reset an existing administrator.

## Resetting the administrator password

If the initial password is lost, change `APP_BOOTSTRAP_ADMIN_PASSWORD` to a new, unique password and temporarily create the Actions secret `APP_BOOTSTRAP_ADMIN_RESET_PASSWORD` with the value `true`. Run **Backend Deploy** manually from the GitHub Actions page. It resets only the account matching `APP_BOOTSTRAP_ADMIN_EMAIL`; it will fail rather than change a different account. After a successful sign-in, delete the temporary reset secret and run **Backend Deploy** once more.

## Inviting users

1. Sign in as the administrator.
2. Open **Admin**.
3. Enter the recipient's email and select one or more unassigned sensors.
4. Create the invitation and privately send the generated link to the recipient.
5. The recipient chooses a password. The invitation expires after seven days and works once.

Users see and can manage only sensors assigned to their account. A direct request for another sensor is denied by the backend.

## Operational notes

- Use HTTPS for the public site. `APP_SECURITY_COOKIE_SECURE` is set to `true` in Docker deployment.
- Development over `http://localhost` requires `APP_SECURITY_COOKIE_SECURE=false` for the backend.
- Do not expose PostgreSQL, MQTT, or the backend port publicly; the deployed compose file binds backend and PostgreSQL to localhost only.

## MQTT users and ACL (OPS-1-2)

### Current state (2026-10-25)

- One shared user `esp32` is used by the backend and the ESP32. Its password was changed on 2026-10-25 (the old one was in the history of a local branch that had been on GitHub); the procedure is below.
- The V1 device (STM32F103) is out of service; there is no device connected until the first V2 release.
- Per-user accounts and the ACL below are **rolled out together with the first ESP32 firmware for V2** (OPS-1-2). No transition period with a running device is needed.

### Target: users and ACL

| User | May publish | May subscribe | Credentials live in |
|---|---|---|---|
| `backend` | `smartgarden/+/ack` | `smartgarden/+/data` | GitHub secrets `MQTT_USERNAME`, `MQTT_PASSWORD` |
| `device-<id>` (one per device, e.g. `device-2`) | `smartgarden/<id>/data` | `smartgarden/<id>/ack` | `ESP/include/secrets.h` (`SECRET_MQTT_USER`, `SECRET_MQTT_PASS`), not in git |

Templates: `mosquitto/mosquitto.conf` and `mosquitto/acl.example`. The real files on the server are `mosquitto.conf`, `mosquitto_passwd` and `mosquitto_acl` in `/srv/projects/garden/backend/`, next to `docker-compose.yml`. They are never committed (`.gitignore`).

### Pitfalls

- **Denied publishes are silent.** MQTT 3.1.1 does not report an ACL rejection to the publisher: `client.publish()` on the ESP32 still returns `true` and the message is dropped. A measurement that is missing in the dashboard while the ESP32 reports success is most likely an ACL mismatch.
- **The ESP32 builds the topic from `deviceId` in the STM32 payload**, not from its own `DEVICE_ID`. If the STM32 sends a different id than the one in the ACL entry of the device user, every publish is dropped silently. ESP-1-2 (topic always from the gateway's `DEVICE_ID`) removes this risk; until then check the topic with `mosquitto_sub`.
- **Do not run `mosquitto_passwd` on the bind-mounted file inside the running container.** The files are mounted one by one; rewriting them there can fail with `Device or resource busy`. Edit them in the host directory with a temporary container:
  `docker run --rm -it -v "$PWD":/work eclipse-mosquitto mosquitto_passwd /work/mosquitto_passwd <user>`
  The password is typed at the prompt and does not end up in the shell history.
- **`mosquitto_passwd -c` overwrites the whole file.** Use `-c` only when the file does not exist yet.
- **Restart instead of reload.** Single-file bind mounts keep pointing to the old file when a tool replaces the file instead of rewriting it. After changing `mosquitto_passwd` or `mosquitto_acl`, run `docker compose restart mqtt` rather than relying on `SIGHUP`.
- **Permissions.** The broker runs as `mosquitto` (uid/gid 1883 in the `eclipse-mosquitto` image). `mosquitto_passwd` and `mosquitto_acl` must be owned by 1883:1883 with mode 600:
  `sudo chown 1883:1883 mosquitto_passwd mosquitto_acl && sudo chmod 600 mosquitto_passwd mosquitto_acl`
- **`docker-compose.yml` on the server is overwritten by every Backend Deploy** with the version from `main`. The compose file mounts `./mosquitto_acl`: the file must exist on the server before the `mqtt` container is recreated, otherwise Docker creates a directory with that name and the broker does not start.

### Changing the password of one user

Done for `esp32` on 2026-10-25. All commands on the server in `/srv/projects/garden/backend/`.

1. Back up and set the new password:
   ```bash
   cp -p mosquitto_passwd mosquitto_passwd.bak-$(date +%F)
   docker run --rm -it -v "$PWD":/work eclipse-mosquitto mosquitto_passwd /work/mosquitto_passwd esp32
   sudo chown 1883:1883 mosquitto_passwd && sudo chmod 600 mosquitto_passwd
   docker compose restart mqtt
   ```
2. Update the GitHub secret `MQTT_PASSWORD` and run **Backend Deploy** manually.
3. Update `SECRET_MQTT_PASS` in `ESP/include/secrets.h` of every ESP32 that uses this user before it goes back into service.
4. Check: `docker logs garden-backend` shows no MQTT authentication errors; `docker logs garden-mqtt` shows the backend connecting with `u'esp32'`.
5. Delete the `.bak` file once everything works — it contains the old password hash.

Rollback: restore the `.bak` file, `docker compose restart mqtt`, restore the old secret and deploy.

### Rolling out users and ACL with the first V2 ESP32 firmware

1. Create `backend` and `device-<id>` with `mosquitto_passwd` (as above, no `-c`), copy `mosquitto/acl.example` to `mosquitto_acl` without the transitional `esp32` block, fix owner and mode.
2. Update `mosquitto.conf` from `mosquitto/mosquitto.conf` and recreate the broker: `docker compose up -d mqtt`. Check that anonymous access is refused:
   `docker exec garden-mqtt mosquitto_sub -h localhost -t 'smartgarden/#' -C 1 -W 5` must fail with "not authorised".
3. Set the GitHub secrets `MQTT_USERNAME` = `backend` and `MQTT_PASSWORD`, run **Backend Deploy**; check the backend log.
4. Flash the V2 ESP32 firmware with `SECRET_MQTT_USER` = `device-<id>`. Check the topic: `read -s P`, then `docker exec garden-mqtt mosquitto_sub -h localhost -u backend -P "$P" -t 'smartgarden/+/data' -v -C 1` shows the next measurement on `smartgarden/<id>/data`; `unset P`.
5. Remove `esp32`: `mosquitto_passwd -D` (in a temporary container as above), `docker compose restart mqtt`. Check that nothing logs in as `esp32`.

### Local clean-up (developer machine)

The old local branches `backup/main-before-reset`, `feature/stm32l467-freertos`, `main-from-github` and `feature/stm32l476-json` contain the old passwords in their history. Do not push them; delete them locally:
```bash
git branch -D backup/main-before-reset feature/stm32l467-freertos main-from-github feature/stm32l476-json
```

Port 1883 stays open to the internet without TLS until OPS-1-3 / ESP-2-1.
