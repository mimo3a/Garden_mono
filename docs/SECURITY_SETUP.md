# User access and invitations

The application uses server-side sessions in `HttpOnly`, `Secure`, `SameSite=Strict` cookies. Passwords are stored as one-way hashes by Spring Security. Public registration is intentionally disabled.

## First administrator

Before deploying the security branch, add two GitHub Actions secrets in **Settings → Secrets and variables → Actions**:

- `APP_BOOTSTRAP_ADMIN_EMAIL` — the administrator's email address
- `APP_BOOTSTRAP_ADMIN_PASSWORD` — a unique password of at least 12 characters

The backend creates this administrator only when no administrator exists. It never logs the password and it is not stored in the repository. Keep the secrets in place for later deploys; they do not reset an existing administrator.

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
