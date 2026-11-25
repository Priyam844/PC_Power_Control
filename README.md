# PC_Power_Control

ESP8266-based remote power control for a desktop PC. A small Django web app provides a LAN/internet dashboard, while an ESP8266 relay board presses the PC power button and reports the PC's on/off state from a switched 3.3 V signal.

The repository is intentionally sanitized: firmware credentials and device tokens are placeholders. Real values must be supplied locally and kept out of Git.

---

## Features

- Browser dashboard showing device connectivity and PC power state.
- Slide-to-confirm power control for both power on and power off, to avoid accidental presses.
- ESP8266 heartbeat every 4 seconds, with the server treating a device as offline after 30 seconds without a heartbeat.
- Server-side command queue with 120-second expiry.
- Command acknowledgement and final status tracking (`PENDING`, `ACKNOWLEDGED`, `EXECUTED`, `FAILED`, `EXPIRED`).
- Device-token authentication; only a SHA-256 hash is stored in the database.
- One-time token display and regeneration from the web UI.
- Separate diagnostics sketch that tests Wi-Fi and HTTP/HTTPS without pulsing the relay.
- Active-high PC power sense input on `D5`, avoiding the ESP8266 `D4`/GPIO2 boot constraint.

---

## Architecture

```text
Browser  <--session + CSRF-->  Django web app
                                  |  |
                                  |  +-- device API: token auth, commands, ACKs
                                  |
                            HTTP/HTTPS
                                  |
                              ESP8266
                               |     |
                        relay on D2  sensor on D5
                               |     |
                    PC front-panel button   switched 3.3 V
```

Request flow for a power press:

1. The user slides the dashboard power control.
2. `POST /api/pc/power/` creates a `PRESS_POWER` command without a body.
3. The ESP8266 posts its status to `/api/device/status/`, then polls `/api/device/commands/` on the same connection cycle.
4. The device receives the unexpired command, pulses the relay on `D2` for `RELAY_PULSE_MS` (default 400 ms), then ACKs it at `/api/device/commands/ack/`.
5. The dashboard polls `/api/pc/status/` and displays progress, expiry, and the latest command state.

The dashboard never sends the device token. Browser users authenticate with Django sessions; the ESP8266 authenticates with `Authorization: Token <device-token>`.

---

## Repository layout

```text
.
├── ESP8266_PC_Power_Controller_Project_Plan.md   # Original design document
├── README.md                                    # This file
├── create_new_user.txt                          # Short account setup reminder
├── render.yaml                                  # Render Blueprint for one-click deploy
├── backend/
│   ├── .env.example                             # Tracked local environment template
│   ├── accounts/                                # Login, dashboard, status/power APIs
│   ├── config/                                  # Django settings and URL root
│   ├── devices/                                 # Device model, token auth, device APIs
│   ├── static/                                  # CSS and dashboard JavaScript
│   ├── templates/                               # HTML pages
│   ├── manage.py
│   └── requirements.txt
└── firmware/
    ├── esp8266/
    │   ├── esp8266.ino                          # Main sketch, tracked/sanitized
    │   └── README.md                            # Firmware-specific setup notes
    └── esp8266_debug/
        └── esp8266_debug.ino                    # Diagnostics sketch, tracked/sanitized
```

`*.local.ino` and `*.local.txt` files are ignored by Git for local credentials. Do not keep a second `.ino` file inside a sketch folder: Arduino compiles every `.ino` there and will report redefinition errors. Local backups now use `.local.txt`.

---

## Security model

- The committed firmware uses `YOUR_WIFI_SSID`, `YOUR_WIFI_PASSWORD`, and `YOUR_DEVICE_TOKEN`.
- The database stores only a SHA-256 hash of the device token, never the raw token.
- The raw token is shown once when it is created or regenerated. Regeneration immediately invalidates the previous token.
- The browser app uses session cookies with `HttpOnly`, `SameSite=Lax`, and `Secure` enabled when `DEBUG=False`.
- CSRF protection is required for browser POSTs.
- Login is rate-limited to 5/min and power requests to 10/min.
- The development-only static login backend is enabled when `DEBUG=True`, or explicitly with `ENABLE_STATIC_DEV_USER=1`. Keep it disabled in production.
- The default static development password in source is a placeholder. Set `STATIC_DEV_PASSWORD` locally instead of relying on the default.
- `db.sqlite3`, `.env`, and `*.local.ino` are ignored. Supabase PostgreSQL is required for both local and deployed runs; do not commit database passwords, connection strings, or token values.

If credentials were ever pasted into chat, logs, or an issue, rotate them: regenerate the device token, change the Wi-Fi password if needed, and update the ignored local firmware copies.

---

## Backend setup (local development)

Requires Python 3.11+ (Django 5 requires Python 3.10+, but 3.11 is recommended).

From PowerShell:

```powershell
cd backend
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
Copy-Item .env.example .env
notepad .env
python manage.py migrate
python manage.py check
```

`backend/.env.example` is a tracked template. Copy it to `backend/.env`, replace `[YOUR-DATABASE-PASSWORD]` with your real Supabase DB password, and save. `.env` is gitignored. Use Supabase **Connect → Transaction pooler** (port `6543`) for `DATABASE_URL`, not the Direct connection or Session pooler. The Direct host is IPv6-only and usually fails on local Windows networks; Session pooler limits clients to the pool size, often `15`, and raises `EMAXCONNSESSION` when local processes and Render reconnect. Settings close DB connections after each request and disable server-side cursors for transaction pooling.

Create a website account:

```powershell
$env:INITIAL_USERNAME = "your-admin-user"
$env:INITIAL_PASSWORD = "a-strong-local-password"
python manage.py create_initial_user
```

Alternatively, pass values directly:

```powershell
python manage.py create_initial_user --username your-admin-user --password "a-strong-local-password"
```

Start the server on the LAN:

```powershell
$env:DEBUG = "1"
$env:ALLOWED_HOSTS = "localhost,127.0.0.1,192.168.0.103,10.0.27.133,10.198.222.3"
python manage.py runserver 0.0.0.0:8000
```

Replace `192.168.0.103` with the PC's actual LAN IP. On Windows, allow inbound TCP port 8000 through the firewall, and make sure the active network profile is Private if you want other LAN devices to reach it.

Open <http://127.0.0.1:8000/> or <http://192.168.0.103:8000/>.

For a quick development login without creating an account, set `STATIC_DEV_USERNAME` and `STATIC_DEV_PASSWORD` before starting the server. The static backend is only a local convenience.

---

## Device registration and token

There are two supported ways to create the device and obtain its one-time token.

### From the web UI

1. Sign in as a staff user or superuser.
2. On first sign-in, if the default device does not exist, the server creates `pc-controller-01` and stores the raw token in the session.
3. You are redirected to `/device-token/`, where the token is shown once.
4. Copy it into `DEVICE_TOKEN` in the firmware configuration.
5. If you lose it, open **Device token** and choose **Regenerate token**. The old token stops working immediately.

### From the management command

```powershell
cd backend
python manage.py create_device --name pc-controller-01
```

The command prints the raw token once and stores only its hash.

The browser dashboard and device token are separate credentials. Website username/password never go into the ESP8266.

---

## Firmware setup

### Tools

- Arduino IDE with the ESP8266 board package 3.x.
- ArduinoJson 6.x.
- Board: **NodeMCU 1.0 (ESP-12E Module)** or equivalent ESP8266 board.

Open one of:

- `firmware/esp8266/esp8266.ino` for normal operation.
- `firmware/esp8266_debug/esp8266_debug.ino` for connectivity diagnostics only.

Edit only the configuration block near the top. Do not change the request paths or the `Authorization: Token ...` header format.

### Configuration values

| Firmware constant | Example | Meaning |
| --- | --- | --- |
| `WIFI_SSID` | `"YOUR_WIFI_SSID"` | 2.4 GHz Wi-Fi network name |
| `WIFI_PASSWORD` | `"YOUR_WIFI_PASSWORD"` | Wi-Fi password |
| `DEVICE_TOKEN` | `"YOUR_DEVICE_TOKEN"` | One-time token from the web UI or `create_device` |
| `SERVER_BASE_URL` | `"http://192.168.0.103:8000"` | Django base URL, no trailing slash |

For Render or another HTTPS host, use `https://YOUR_APP.onrender.com`. For local development, use `http://PC_LAN_IP:8000`. A bare host such as `192.168.0.103:8000` is treated as HTTP. HTTPS uses port 443; HTTP uses the port in the URL or 80 by default. Do not include a trailing `/`.

Useful timing defaults:

| Constant | Default | Purpose |
| --- | --- | --- |
| `RELAY_PULSE_MS` | `400` | Duration of one power-button press |
| `POLL_INTERVAL_MS` | `4000` | Status/command polling interval |
| `WIFI_CONNECT_TIMEOUT_MS` | `20000` | Startup Wi-Fi wait before retrying |
| `HTTP_TIMEOUT_MS` | `15000` | Network read timeout |

### Wiring

| NodeMCU pin | Connection | Notes |
| --- | --- | --- |
| `D2` | Relay module input | Active LOW; sketch drives it HIGH at boot to keep the relay off |
| `D5` | Switched 3.3 V PC power signal | Active HIGH; 10 kΩ pulldown from `D5` to GND |
| `GND` | Common ground | ESP8266, relay module, and PC signal ground must be common |

Relay contacts should be wired across the motherboard front-panel power switch header (COM and NO), not across a motherboard LED header. Use an optocoupler or properly isolated relay module; never feed 5 V or 12 V into an ESP8266 GPIO pin.

`D4`/GPIO2 is deliberately avoided because it must be HIGH during boot. The sense input uses `D5` with `SENSOR_ACTIVE_LOW = false`, so a HIGH reading means the PC is on. The serial monitor prints `Sensor: D5=HIGH` or `Sensor: D5=LOW`.

Power the ESP8266 from a suitable 5 V/3.3 V supply. Check your relay module's JD-VCC/VCC jumper requirements.

### Upload and verify

1. Fill in the configuration placeholders.
2. Upload `esp8266.ino`.
3. Open Serial Monitor at **115200** baud.
4. Look for `WiFi: CONNECTED`, a successful heartbeat, and `Command poll:` messages.
5. If the device does not come online, upload `esp8266_debug.ino` first. It tests time/status requests and prints plain-language hints for common `400`, `401/403`, and `404` errors. The diagnostics sketch does not poll commands or pulse the relay.

The sketch prints the token length only, never the token or Wi-Fi password.

---

## Using the dashboard

- **Offline**: device card is grey and does not ripple.
- **Online + PC OFF**: red signal ripple; the slide control says “Slide to power on”.
- **Online + PC ON**: green signal ripple; the slide control says “Slide to power off”.
- **UNKNOWN**: yellow signal ripple while the device is online but the sensor state is unavailable.
- **Command pending/expired/executed**: shown beneath the power control based on `/api/pc/status/`.

The power control is deliberately a slide gesture rather than a single click. Drag the knob to the end, or focus it and press `Enter` twice / `ArrowRight`. While offline, the control is disabled.

Change credentials from **Settings**. Changing the username or password signs you out; sign in again with the new values.

---

## API reference

### Browser APIs

These use Django session authentication and a CSRF token header for POSTs.

| Method | Path | Purpose | Body |
| --- | --- | --- | --- |
| GET | `/` | Dashboard page | — |
| GET/POST | `/login/` | Login page | Form |
| GET | `/settings/` | Credential settings page | — |
| GET/POST | `/device-token/` | Show/regenerate device token, staff only | Form on POST |
| POST | `/api/auth/login/` | JSON login | `{"username": "...", "password": "..."}` |
| POST | `/api/auth/logout/` | Sign out | Empty |
| POST | `/api/auth/change-credentials/` | Change username/password | `{"new_username": "...", "new_password": "..."}` |
| GET | `/api/pc/status/` | Dashboard status | — |
| POST | `/api/pc/power/` | Queue a `PRESS_POWER` command | Empty |

`GET /api/pc/status/` returns:

```json
{
  "username": "logged-in-user",
  "device_name": "pc-controller-01",
  "device_status": "ONLINE | OFFLINE",
  "pc_status": "ON | OFF | UNKNOWN",
  "last_seen": "ISO-8601 timestamp or null",
  "relay_state": "ACTIVE | INACTIVE",
  "pending_command": true,
  "last_command_status": "PENDING | ACKNOWLEDGED | EXECUTED | FAILED | EXPIRED | null"
}
```

`POST /api/pc/power/` returns `201` with the new command ID and expiry, or `409` if another command is already pending. If no device is registered, it returns `503`.

### Device APIs

These use `Authorization: Token <device-token>`. No session or CSRF token is needed.

| Method | Path | Purpose | JSON body / response |
| --- | --- | --- | --- |
| POST | `/api/device/status/` | Heartbeat and state update | Body: `pc_status`, `relay_state`; response: `ok`, `server_time` |
| GET | `/api/device/commands/` | Fetch the next unexpired command | `{"command": {"command_id": "...", "command": "PRESS_POWER", "expires_in_seconds": 119}}` or `{"command": null}` |
| POST | `/api/device/commands/ack/` | Report final command result | Body: `command_id`, `status` of `EXECUTED` or `FAILED`, optional `result` |
| GET | `/api/device/time/` | Server time | `server_time`, `server_epoch`, `command_expiry_seconds` |

Fetching a `PENDING` command automatically marks it `ACKNOWLEDGED`; the ACK POST then sets it to `EXECUTED` or `FAILED`.

Example heartbeat:

```http
POST /api/device/status/
Authorization: Token YOUR_DEVICE_TOKEN
Content-Type: application/json

{
  "pc_status": "ON",
  "relay_state": "INACTIVE"
}
```

---

## Configuration reference

Environment variables are read in `backend/config/settings.py`. For local runs, `backend/.env` is loaded automatically; for Render, set the same variables in the dashboard.

| Variable | Default | Purpose |
| --- | --- | --- |
| `DJANGO_SECRET_KEY` | Development fallback | Set a strong random value in production |
| `DEBUG` | `1` | Must be `0` in production |
| `ALLOWED_HOSTS` | `localhost,127.0.0.1,testserver,192.168.0.103,10.0.27.133,10.198.222.3` | Comma-separated hostnames/IPs; add your current PC LAN IP if it changes |
| `CSRF_TRUSTED_ORIGINS` | Empty | Comma-separated HTTPS origins |
| `DATABASE_URL` | Required | Full Supabase Transaction pooler PostgreSQL URL (port `6543`); SQLite is not used |
| `STATIC_DEV_USERNAME` | `admin` | Development-only login username |
| `STATIC_DEV_PASSWORD` | `change-me-in-local-dev` | Development-only login password |
| `ENABLE_STATIC_DEV_USER` | Empty | Set to `1`/`true` to enable dev login when `DEBUG=False` |
| `INITIAL_USERNAME` | Empty | Used by `create_initial_user` |
| `INITIAL_PASSWORD` | Empty | Used by `create_initial_user` |
| `TZ` | Not set in source | Set to `Asia/Kolkata` in the example deployment notes |

Application constants:

| Constant | Default | Meaning |
| --- | --- | --- |
| `DEVICE_ONLINE_SECONDS` | `30` | Heartbeat age before the device is shown offline |
| `COMMAND_EXPIRY_SECONDS` | `120` | Time before an unacknowledged command expires |

---

## Deployment

The app is compatible with a Render Web Service plus Supabase PostgreSQL. The repository includes `render.yaml`, so **New → Blueprint** can deploy it directly. Create the service, then supply the real Supabase **Transaction pooler** URL when Render prompts for `DATABASE_URL`. Render generates `DJANGO_SECRET_KEY` and exposes `RENDER_EXTERNAL_HOSTNAME`, which settings use automatically for `ALLOWED_HOSTS` and `CSRF_TRUSTED_ORIGINS`. Health checks use `/health/`.

The same service can be created manually with these settings and **Root Directory** set to `backend`:

**Build command**

```bash
pip install -r requirements.txt
python manage.py collectstatic --noinput
python manage.py migrate --noinput
```

**Start command**

```bash
gunicorn config.wsgi:application --bind 0.0.0.0:$PORT
```

Required production environment variables:

```text
DJANGO_SECRET_KEY=<long-random-value>
DEBUG=0
DATABASE_URL=<supabase-transaction-pooler-url-with-password>
RENDER_EXTERNAL_HOSTNAME=<provided-automatically-by-render>
```

The app requires Supabase PostgreSQL everywhere. SQLite is no longer a fallback. Use the Transaction pooler URL so both Render and local machines connect over IPv4 without hitting the Session pooler's client cap.

Do not enable `ENABLE_STATIC_DEV_USER` in production.

After the service is live:

1. Sign in with a staff account.
2. Open `/device-token/` and copy the generated token.
3. Set `SERVER_BASE_URL` in the firmware to the HTTPS URL, with no trailing slash.
4. Re-upload the firmware.
5. Confirm the dashboard shows the device online.

Render Free limitations:

- Free web services may spin down after inactivity, causing a delay on the next request.
- Device polling generates inbound requests and often keeps the service awake, but this is not a guaranteed uptime arrangement.
- Free Supabase projects may pause after periods of low database activity; restore them from the Supabase Dashboard if that happens.
- Render Free does not provide reliable Shell/one-off job access. Use `/device-token/` in the browser to create or rotate the token.
- For dependable 24/7 control, use a paid always-on service and database, or run the backend on the local LAN PC itself.

---

## Troubleshooting

### Database reports `EMAXCONNSESSION`

- Change `DATABASE_URL` to the Supabase **Transaction pooler** URL on port `6543`.
- Do not use the Session pooler (`5432`) for this app; its client cap is commonly `15`.
- Restart every Django/Render process after changing the URL so stale session-mode connections are released.

### Device shows OFFLINE

- Confirm the serial monitor reports `WiFi: CONNECTED` and a successful `Heartbeat`.
- Confirm the ESP is on a 2.4 GHz network; most ESP8266 boards do not support 5 GHz.
- Check `API:` lines for `400`, `401/403`, or `404`.
- `400`: add the host/IP to `ALLOWED_HOSTS`.
- `401/403`: token mismatch; regenerate the token, update firmware, and re-upload.
- `404`: wrong `SERVER_BASE_URL`; check the base URL and that the server is running from the `backend` directory.
- For local HTTP, verify Windows firewall allows inbound TCP 8000 and that the ESP uses the PC's LAN IP, not `127.0.0.1`.
- Upload `esp8266_debug.ino` to isolate Wi-Fi/HTTP problems from relay logic.

### Relay clicks but the PC does not power on

- Verify relay COM/NO are wired across the front-panel power switch header.
- Do not wire across a motherboard LED header.
- Increase `RELAY_PULSE_MS` slightly if the motherboard needs a longer press.
- Make sure the relay module is powered correctly, including its JD-VCC jumper if applicable.

### PC state is wrong or UNKNOWN

- Check the serial trace for `Sensor: D5=HIGH/LOW`.
- Confirm switched 3.3 V goes to `D5` when the PC is on.
- Confirm the 10 kΩ pulldown from `D5` to GND.
- Confirm common ground between ESP8266, relay module, and PC signal.
- Never feed 5 V or 12 V directly into `D5`.

### Command stays pending

- Ensure the main sketch is uploaded; the debug sketch does not poll commands.
- Check that the device is online and heartbeats are succeeding.
- Commands expire after 120 seconds. Look for `Command poll: ... expired` in the serial trace.
- Check `last_command_status` in `/api/pc/status/`.

### Dashboard looks stale

- Hard-refresh the browser; static assets are cache-busted with a version query string.
- Check the browser console for failed fetches or CSRF errors.
- Confirm you are signed in; all dashboard APIs require a session.

---

## Development checks

```powershell
cd backend
python manage.py check
python manage.py migrate --check
```

For JavaScript syntax:

```bash
node --check static/js/dashboard.js
node --check static/js/common.js
node --check static/js/settings.js
```

Before committing:

- Ensure no real Wi-Fi password, device token, user password, or `db.sqlite3` is staged.
- Keep real firmware values in ignored `*.local.ino` files.
- Run `git diff --cached --check` to catch whitespace errors.

---

## Notes

- This project is designed for a LAN-first deployment. Expose it to the internet only with HTTPS, strong credentials, and a device token you can rotate quickly.
- The device token is not a substitute for good Wi-Fi and host security.
- The original design rationale and feature plan are preserved in `ESP8266_PC_Power_Controller_Project_Plan.md`.
