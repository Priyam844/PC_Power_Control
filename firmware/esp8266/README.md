# ESP8266 sketch setup

Open `firmware/esp8266/esp8266.ino`. You only need to edit the small configuration block near the top (currently lines 7-23). Do not change the request paths or the `Authorization: Token ...` header.

## Values to fill in

| Where | Example | Put this |
| --- | --- | --- |
| `WIFI_SSID` | `"YOUR_WIFI_SSID"` | Your 2.4 GHz Wi-Fi network name |
| `WIFI_PASSWORD` | `"YOUR_WIFI_PASSWORD"` | That Wi-Fi network's password |
| `DEVICE_TOKEN` | `"YOUR_DEVICE_TOKEN"` | The one-time token shown at `/device-token/` after your first login |
| `SERVER_BASE_URL` | `"https://YOUR_APP.onrender.com"` | Your deployed Django URL, e.g. `https://my-pc-control.onrender.com` |

Use `https://YOUR_APP.onrender.com` for Render, or `http://192.168.x.x:8000` for a local Django server. A bare `192.168.x.x:8000` is also treated as HTTP. HTTPS connects on 443; HTTP uses the port in the URL (default 80). Do not end the URL with `/`.

For local HTTP, start Django with `python manage.py runserver 0.0.0.0:8000` and include the PC's LAN IP in `ALLOWED_HOSTS`, for example `ALLOWED_HOSTS=192.168.0.103,localhost,127.0.0.1`.

## Pins and hardware

| Setting | Default | Meaning |
| --- | --- | --- |
| `RELAY_PIN` | `D2` | GPIO that drives the relay module input |
| `SENSOR_PIN` | `D5` | GPIO read from the PC power signal |
| `RELAY_ACTIVE_LOW` | `true` | Relay triggers when the pin is pulled LOW |
| `SENSOR_ACTIVE_LOW` | `false` | PC is ON when the sensor drives this pin HIGH |

The relay input on D2 is active LOW: `LOW` energizes the relay, `HIGH` releases it. The sketch forces D2 `HIGH` during boot so the relay stays off while Wi-Fi and HTTPS start.

D5 is used with `INPUT` and a 10kΩ pulldown to GND. A switched 3.3V signal drives the pin HIGH when the PC is on; `SENSOR_ACTIVE_LOW` is `false`. D5 avoids the boot constraint of D4/GPIO2, which must be HIGH during reset.

## Timing

| Setting | Default | Purpose |
| --- | --- | --- |
| `RELAY_PULSE_MS` | `400` | How long the relay is held closed for one power-button press |
| `POLL_INTERVAL_MS` | `4000` | How often the device sends status and checks for commands |
| `WIFI_CONNECT_TIMEOUT_MS` | `20000` | Startup wait before retrying Wi-Fi in the main loop |
| `HTTP_TIMEOUT_MS` | `15000` | Network read timeout in seconds |

## Serial trace

Open Serial Monitor at **115200** baud. Each line starts with elapsed
milliseconds, for example `[1234 ms] WiFi: link up, IP=192.168.0.50`.

Expected messages include:

- `Boot:` configuration summary, with the token shown only as its length.
- `WiFi:` connection, waiting, link loss, and retry status.
- `API:` HTTP/HTTPS mode, target host, status codes, and connect failures.
- `Sensor:` the PC state read from D5.
- `Heartbeat:` whether the status POST succeeded.
- `Command poll:` no command, command ID with expiry, or error reason.
- `Relay:` pulse start and safe inactive state.
- `ACK:` whether the acknowledgement reached the server.
- `Loop:` skipped polling while Wi-Fi is offline.

The sketch never prints the device token or Wi-Fi password.

## Diagnosing an offline device

Open `firmware/esp8266_debug/esp8266_debug.ino` as a separate Arduino
sketch. It prints Wi-Fi state changes, IP/RSSI, HTTP status codes, and
plain-language hints for `400` (ALLOWED_HOSTS), `401/403` (token), and
`404` (URL). It does not pulse the relay.

Upload this sketch first. If it reports `WiFi: CONNECTED` and `HTTP 200`
for both checks, the main sketch should also come online.

## Certificate

Leave `ROOT_CA` unchanged for Render. Replace it only if your HTTPS provider uses a different root CA. Do not paste a device token or Wi-Fi password into `ROOT_CA`.

## Before uploading

1. In Arduino IDE, install the **ESP8266** board package (3.x) via Boards Manager.
2. Select **NodeMCU 1.0 (ESP-12E Module)** (or your ESP8266 board).
3. Install **ArduinoJson** (version 6) via Library Manager.
4. Fill in the four placeholders above.
5. Upload, then open Serial Monitor at **115200** baud to see Wi-Fi status and received commands.

## Getting the device token

Sign in to the web console at `/login/`. On the first sign-in, the server creates the device and opens `/device-token/` where the token is shown once.

Copy it into `DEVICE_TOKEN`. It cannot be shown again. Open **Device token** in the top bar and choose **Regenerate token** if you lose it; the previous token stops working immediately.

Alternative from `D:\MY PROJECT\Server_for_PC_on\backend`:

```powershell
python manage.py create_device --name pc-controller-01
```
