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

The broker accepts only authenticated clients, and every client may use only its own topics:

| User | May publish | May subscribe | Credentials live in |
|---|---|---|---|
| `backend` | `smartgarden/+/ack` | `smartgarden/+/data` | GitHub secrets `MQTT_USERNAME`, `MQTT_PASSWORD` |
| `device-<id>` (one per device, e.g. `device-2`) | `smartgarden/<id>/data` | `smartgarden/<id>/ack` | `ESP/include/secrets.h` (`SECRET_MQTT_USER`, `SECRET_MQTT_PASS`), not in git |

Templates: `mosquitto/mosquitto.conf` and `mosquitto/acl.example`. The real files on the server are `mosquitto.conf`, `mosquitto_passwd` and `mosquitto_acl` in `/srv/projects/garden/backend/`, next to `docker-compose.yml`. They are never committed (`.gitignore`).

### Pitfalls

- **Denied publishes are silent.** MQTT 3.1.1 does not report an ACL rejection to the publisher: `client.publish()` on the ESP32 still returns `true` and the message is dropped. A measurement that is missing in the dashboard while the ESP32 reports success is most likely an ACL mismatch.
- **The ESP32 builds the topic from `deviceId` in the STM32 payload**, not from its own `DEVICE_ID`. If the STM32 sends a different id than the one in the ACL entry of the device user, every publish is dropped silently. Until ESP-1-2 (topic always from the gateway's `DEVICE_ID`) is done, check the topic with `mosquitto_sub` before switching a device to its own user (step 6).
- **Do not run `mosquitto_passwd` on the bind-mounted file inside the running container.** The files are mounted one by one; rewriting them there can fail with `Device or resource busy`. Edit them in the host directory with a temporary container (step 2).
- **Restart instead of reload.** Single-file bind mounts keep pointing to the old file when a tool replaces the file instead of rewriting it. After changing `mosquitto_passwd` or `mosquitto_acl`, run `docker compose restart mqtt` (a few seconds) rather than relying on `SIGHUP`.
- **`mosquitto_passwd -c` overwrites the whole file.** Use `-c` only when the file does not exist yet.
- **Permissions.** The broker runs as `mosquitto` (uid/gid 1883 in the `eclipse-mosquitto` image). `mosquitto_passwd` and `mosquitto_acl` must be owned by 1883:1883 with mode 600; newer Mosquitto versions refuse a world-readable password file.
- **`docker-compose.yml` on the server is overwritten by every Backend Deploy** with the version from `main`. A compose change made by hand on the server is lost on the next deploy, so compose changes go through `main` (step 3a).
- **Restarts lose a measurement only if they hit a wake-up.** The ESP32 publishes with QoS 0 once per hour. Restart the broker or the backend right after a measurement has arrived.

### Changing passwords and introducing per-device users without losing the connection

All commands run on the server in `/srv/projects/garden/backend/`. Old and new access work in parallel at every step until step 7; the order is broker → backend → ESP32.

**Step 0 — current state (read only).**
```bash
cd /srv/projects/garden/backend
cat mosquitto.conf
docker exec garden-mqtt mosquitto -h | head -1
ls -ln mosquitto.conf mosquitto_passwd mosquitto_acl
cut -d: -f1 mosquitto_passwd
docker exec garden-mqtt sh -c "grep -h \"u'\" /mosquitto/log/mosquitto.log | tail -5"
```
Check:
- `allow_anonymous` and `password_file` in the current config;
- Mosquitto version;
- owner and mode of `mosquitto_passwd` (and `mosquitto_acl`, if it exists) — target `1883 1883`, `-rw-------`;
- which user names exist (names only, no hashes);
- whether the backend and the ESP32 really log in as `esp32` (`u'esp32'` in the connect lines). With `allow_anonymous true` a client with a wrong password may still be working — then step 2 must also (re)create `esp32` with the password currently used by the ESP32 and the backend;
- the topic the device publishes to (must be `smartgarden/2/data` for `device-2`):
  ```bash
  docker exec garden-mqtt sh -c "grep -h 'PUBLISH' /mosquitto/log/mosquitto.log | tail -3"
  ```
  (only if publish logging is enabled; otherwise check in step 3c with `mosquitto_sub`);
- whether port 1883 is reachable from outside (`ss -ltnp | grep 1883` and the firewall).

**Step 1 — backup.**
```bash
cp -p mosquitto.conf  mosquitto.conf.bak-$(date +%F)
cp -p mosquitto_passwd mosquitto_passwd.bak-$(date +%F)
cp -p docker-compose.yml docker-compose.yml.bak-$(date +%F)
```
Rollback for any later step: copy the `.bak` files back and run `docker compose up -d mqtt`.

**Step 2 — new users with new passwords (no change to running clients).** The password is typed at the prompt, so it does not end up in the shell history. No `-c` while the file exists.
```bash
docker run --rm -it -v "$PWD":/work eclipse-mosquitto mosquitto_passwd /work/mosquitto_passwd backend
docker run --rm -it -v "$PWD":/work eclipse-mosquitto mosquitto_passwd /work/mosquitto_passwd device-2
# only if step 0 showed that esp32 is missing or has a different password:
docker run --rm -it -v "$PWD":/work eclipse-mosquitto mosquitto_passwd /work/mosquitto_passwd esp32
sudo chown 1883:1883 mosquitto_passwd && sudo chmod 600 mosquitto_passwd
cut -d: -f1 mosquitto_passwd     # esp32, backend, device-2
```
The running broker does not see the new users yet; nothing changes for the device.

**Step 3 — ACL and closing anonymous access.**
- **3a (repository):** the OPS-1-2 changes must be on `main` before this step, because Backend Deploy copies `docker-compose.yml` from `main` to the server. The deploy recreates only the backend; with `MQTT_USERNAME` not set it still logs in as `esp32`, so nothing changes yet. Check on the server that `docker-compose.yml` now mounts `./mosquitto_acl`.
- **3b:** create the ACL from `mosquitto/acl.example` (it still contains the transitional `esp32` entry) and the new config from `mosquitto/mosquitto.conf`:
  ```bash
  # copy the two templates from your machine first, e.g. scp ... acl.example server:/srv/projects/garden/backend/mosquitto_acl
  sudo chown 1883:1883 mosquitto_acl && sudo chmod 600 mosquitto_acl
  diff mosquitto.conf.bak-* mosquitto.conf
  ```
- **3c:** right after a measurement has arrived:
  ```bash
  docker compose up -d mqtt
  docker logs --tail 20 garden-mqtt
  ```
  Check:
  - the broker starts without errors about the password or ACL file;
  - anonymous access is refused: `docker exec garden-mqtt mosquitto_sub -h localhost -t 'smartgarden/#' -C 1 -W 5` must fail with "not authorised";
  - the backend reconnects (as `esp32`) and the next measurement appears in the dashboard;
  - the topic is right: `read -s P` (type the backend password), then `docker exec garden-mqtt mosquitto_sub -h localhost -u backend -P "$P" -t 'smartgarden/+/data' -v -C 1` shows the next measurement on `smartgarden/2/data`; `unset P` afterwards;
  - `device-2` cannot read other devices: the same command with `-u device-2` receives nothing.

  Rollback: `.bak` files back, `docker compose up -d mqtt`.

**Step 4 — backend to its own user.** Right after a measurement: set the GitHub secrets `MQTT_USERNAME` = `backend` and `MQTT_PASSWORD` = the new backend password, then run **Backend Deploy** manually.
Check: `docker logs garden-backend` shows no MQTT authentication errors; the next measurement appears in the dashboard; the broker log shows `u'backend'`.
Rollback: delete the `MQTT_USERNAME` secret (the compose default is `esp32`), restore the old `MQTT_PASSWORD`, deploy again.

**Step 5 — Wi-Fi password.** Change the Wi-Fi password on the router directly before step 6. The ESP32 loses Wi-Fi until it is reflashed; at most one hourly measurement is lost. **Every other device on this Wi-Fi network** (phones, laptops, TV, smart-home devices, repeaters) also needs the new password — list them before changing it.

**Step 6 — ESP32 to `device-2`.** In `ESP/include/secrets.h` (not in git) set `SECRET_WIFI_PASS` (new), `SECRET_MQTT_USER` = `device-2` and `SECRET_MQTT_PASS` (new); `SECRET_MQTT_USER` is a new define — copy it from `secrets.h.example`. Build and flash the ESP32. The STM32 is not touched.
Check: the next measurement appears in the dashboard; the broker log shows `u'device-2'` from client `esp32-2`.
Rollback: flash the previous ESP32 firmware — `esp32` still works until step 7 (with the new Wi-Fi password in its `secrets.h`).

**Step 7 — remove the old shared user.** After at least one successful day with `device-2` and `backend` (no `u'esp32'` in the log):
```bash
docker run --rm -it -v "$PWD":/work eclipse-mosquitto mosquitto_passwd -D /work/mosquitto_passwd esp32
# delete the "user esp32" block from mosquitto_acl
sudo chown 1883:1883 mosquitto_passwd mosquitto_acl && sudo chmod 600 mosquitto_passwd mosquitto_acl
docker compose restart mqtt     # right after a measurement
```
Check: the next measurement arrives; for 24 h the log shows no `u'esp32'` and no authentication failures. The leaked password is now invalid everywhere. Remove the `.bak` files once everything works — they contain old password hashes.
Rollback: restore `mosquitto_passwd` / `mosquitto_acl` from the state before this step and restart.

**Step 8 — local clean-up (developer machine).** The old local branches still contain the leaked passwords in their history (`feature/stm32l467-freertos` was on GitHub earlier, which is why the passwords are rotated in steps 2–7). Do not push them; delete them locally:
```bash
git branch -D backup/main-before-reset feature/stm32l467-freertos main-from-github feature/stm32l476-json
```

Port 1883 stays open to the internet without TLS until OPS-1-3 / ESP-2-1.
