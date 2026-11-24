# ESP8266 PC Power Controller

## Project Plan and Technical Specification

**Project type:** IoT-based remote PC power controller\
**Controller:** ESP8266 (NodeMCU / Wemos D1 Mini)\
**Backend:** Django + Django REST Framework\
**Hosting:** Render\
**Database:** PostgreSQL on Render; SQLite local fallback\
**Firmware:** Arduino C++ (single `esp8266.ino` sketch)\
**Frontend:** HTML, custom dark-console CSS, JavaScript (no Bootstrap)

------------------------------------------------------------------------

## 1. Project Overview

The project allows an authenticated user to monitor and control a PC
remotely through a website.

An ESP8266 is connected to:

-   A relay module that simulates a short press of the PC's motherboard
    power button.
-   A sensing circuit that detects whether the PC is running.
-   A Wi-Fi network so it can communicate with the web application
    hosted on Render.

The user signs in with a username and password. The initial credentials
are configured by the project owner. After signing in, the user can
change their username and password by entering the current password, the
new credentials, and password confirmation.

The website displays the PC's current state and provides a control to
send a momentary power-button press.

### Main objectives

1.  Remotely request a PC power-button press.
2.  Monitor whether the PC is ON or OFF.
3.  Provide authenticated access to the control dashboard.
4.  Allow the user to change their login credentials.
5.  Host the web application on Render.
6.  Keep the ESP8266 reachable without requiring inbound connections to
    the user's router.

------------------------------------------------------------------------

## 2. High-Level System Architecture

``` mermaid
flowchart TD
    A[User browser / mobile] -->|HTTPS login and controls| B[Django application on Render]
    B --> C[(PostgreSQL database)]
    B -->|Pending command / status API| D[ESP8266]
    D -->|Wi-Fi HTTPS polling| B
    D --> E[Relay module]
    E --> F[Motherboard PWR_SW pins]
    G[PC power-state sensing circuit] --> D
    F --> H[Target PC]
    H --> G
```

### Main components

  -----------------------------------------------------------------------
  Component                           Responsibility
  ----------------------------------- -----------------------------------
  User browser                        Login, dashboard, power-button
                                      request, credential settings

  Django backend                      Authentication, API, command queue,
                                      status management

  PostgreSQL                          Users, device information,
                                      commands, status history

  ESP8266                             Polls backend, operates relay,
                                      reads status input

  Relay                               Briefly connects the motherboard
                                      power-switch pins

  Sensing circuit                     Provides a safe logic-level
                                      indication of PC running state

  Render                              Hosts the backend and database
                                      service
  -----------------------------------------------------------------------

The ESP8266 initiates outbound HTTPS requests to Render. This avoids
needing to expose the ESP8266 directly to the public internet or
configure router port forwarding.

------------------------------------------------------------------------

## 3. Hardware Requirements

### 3.1 Required components

  -----------------------------------------------------------------------
  Component                           Purpose
  ----------------------------------- -----------------------------------
  ESP8266 NodeMCU or Wemos D1 Mini    Main microcontroller

  Suitable relay module               Simulates the PC power-button press

  Isolated sensing circuit /          Detects PC running state safely
  optocoupler

  Regulated power supply              Keeps ESP8266 powered when the PC
                                      is off

  Jumper wires and enclosure          Wiring and physical protection

  Optional status LED                 Indicates device state or Wi-Fi
                                      connection
  -----------------------------------------------------------------------

### 3.2 Relay connection

The relay should connect to the motherboard's front-panel `PWR_SW` pins.

The relay's normally-open (NO) and common (COM) contacts are connected
across the same two pins used by the physical PC case power button.

When activated, the relay briefly closes the contacts, simulating a
button press.

**Important safety rules:**

-   Use the relay as a momentary switch only.
-   A typical starting pulse is approximately 300--500 milliseconds;
    verify reliable operation with the chosen relay and motherboard.
-   Do not use this relay to switch the PC's mains AC supply.
-   Do not hold the relay closed for several seconds. A long press may
    force an abrupt shutdown.
-   Keep the physical case power button connected if desired; the relay
    contacts can be placed in parallel with it.
-   Verify the motherboard header pins using the motherboard manual.

A motherboard power button is a toggle-style input. A short press
generally starts a powered-off PC and requests the configured
operating-system power-button action when it is already running. It is
not a guaranteed dedicated "turn on" or "shut down" command.

### 3.3 PC status sensing

A practical option is to sense the PSU's main 12V rail:

-   The main 12V rail is generally present while the PSU is actively
    powering the PC.
-   It is generally absent when the PC is shut down, even though 5V
    standby power may remain available.
-   Use a properly designed optocoupler or isolated interface, with
    suitable current limiting and protection.
-   The ESP8266 GPIO must receive a safe 3.3V-compatible logic signal
    only.

**Never connect a 12V rail directly to an ESP8266 GPIO.** ESP8266 GPIO
pins are not 12V tolerant.

Other possible sensing methods include a suitable isolated USB-power
detector or a dedicated PC status signal. The final circuit should be
selected and tested against the actual motherboard and PSU.

### 3.4 Powering the ESP8266

The ESP8266 must remain powered while the PC is off; otherwise, it
cannot receive a remote start request.

Possible approaches:

1.  Use the PSU's 5V standby rail (5VSB) with a properly designed
    regulator and protection.
2.  Use a separate, continuously powered USB supply.

For an initial prototype, a separate USB power supply is often simpler.
A later version can use the PSU standby rail after the circuit has been
designed and tested safely.

------------------------------------------------------------------------

## 4. Website Requirements

### 4.1 Login page

The login page contains:

-   Username field
-   Password field
-   Login button

The initial account is created securely by the project owner or through
a controlled deployment setup process. Do not hard-code a production
password in source code or commit it to GitHub.

### 4.2 Dashboard

The dashboard should show:

-   PC state: ON, OFF, or UNKNOWN
-   ESP8266 connection state: ONLINE or OFFLINE
-   Last device contact time
-   Power-button control
-   Relay state, where useful
-   Account settings link
-   Device token link for staff/superusers

The interface should distinguish between the PC state and the ESP8266
connection state. For example, if the ESP8266 stops reporting, the
dashboard should show the PC state as stale or UNKNOWN rather than
incorrectly assuming that the PC is OFF.

### 4.3 Power control

The dashboard should provide a button labelled:

**Press Power Button**

When the user clicks it:

1.  The backend verifies that the user is authenticated.
2.  The backend creates a unique pending command.
3.  The ESP8266 retrieves the command.
4.  The ESP8266 validates the command and activates the relay for the
    configured short pulse.
5.  The ESP8266 releases the relay.
6.  The ESP8266 acknowledges the command.
7.  The dashboard displays the command result and updated PC state when
    available.

The website should prevent accidental repeated clicks and should show a
pending state while waiting for acknowledgement.

### 4.4 Change credentials page

The account settings page should support changing the username and
password.

Suggested fields:

-   Current password
-   New username (optional)
-   New password
-   Confirm new password
-   Update credentials button

Validation rules:

1.  Verify the current password.
2.  Ensure the new username is available if it is being changed.
3.  Ensure the new password and confirmation match.
4.  Enforce a reasonable password policy.
5.  Save the password using Django's password-hashing system.
6.  Invalidate existing sessions or require re-authentication after a
    credential change.
7.  Display a clear success or error message.

The current password is required to authorize a credential change. Never
store passwords in plain text.

### 4.5 Device token page

On the first staff/superuser login, the server creates the device record
and redirects to `/device-token/`, where the random token is displayed
once. The raw token is not stored; only its SHA-256 hash is kept.

The page can be reopened from the **Device token** navigation link. If the
token was lost or compromised, **Regenerate token** creates a new token
and immediately invalidates the previous one. The ESP8266 sketch must be
updated with the newly displayed token.

------------------------------------------------------------------------

## 5. Backend Technology

  -----------------------------------------------------------------------
  Layer                               Recommended technology
  ----------------------------------- -----------------------------------
  Backend                             Python + Django

  API                                 Django REST Framework

  Authentication                      Django session authentication for
                                      website

  Password storage                    Django password hashers

  Database                            PostgreSQL

   Frontend                            HTML, custom CSS, JavaScript


  Hosting                             Render Web Service

  Database hosting                    Render PostgreSQL or another
                                      persistent PostgreSQL provider

  Device communication                HTTPS REST polling
  -----------------------------------------------------------------------

Django is suitable because it already provides user management, password
hashing, sessions, CSRF protection, database models, and administrative
tools.

### 5.1 Suggested database models

#### User

Use Django's built-in user model or a carefully designed custom user
model.

  Field         Description
  ------------- --------------------------------
  id            Unique user ID
  username      Login username
  password      Django-managed password hash
  is_active     Whether the account is enabled
  date_joined   Account creation time

Do not create a separate plain-text password field.

#### Device

  -----------------------------------------------------------------------
  Field                               Description
  ----------------------------------- -----------------------------------
  id                                  Unique device ID

  device_name                         Friendly PC/device name

  device_token_hash                   Stored hash or secure
                                      representation of device credential

  pc_status                           ON / OFF / UNKNOWN

  connection_status                   ONLINE / OFFLINE

  last_seen                           Last successful device contact

  created_at                          Device registration time
  -----------------------------------------------------------------------

#### Command

  Field             Description
  ----------------- ------------------------------------------------------
  id                Unique command ID
  device_id         Target device
  command           For example, PRESS_POWER
  status            PENDING / ACKNOWLEDGED / EXECUTED / FAILED / EXPIRED
  created_at        Command creation time
  acknowledged_at   Device acknowledgement time
  executed_at       Execution time
  expires_at        Command expiry time

Optional future models include `DeviceStatusLog` for recording status
history and `AuditLog` for tracking user actions.

------------------------------------------------------------------------

## 6. API Design

The following is a suggested API structure. Exact request and response
formats should be defined during implementation.

  -----------------------------------------------------------------------------------------
  Endpoint                          Method            Purpose             Authentication
  --------------------------------- ----------------- ------------------- -----------------
  `/api/auth/login/`                POST              Website login       Public endpoint
                                                                          with throttling

  `/api/auth/logout/`               POST              Website logout      User session

  `/api/auth/change-credentials/`   POST              Change              User session +
                                                      username/password   current password

  `/api/pc/status/`                 GET               Get PC and device   User session
                                                      status

  `/api/pc/power/`                  POST              Create power-button User session
                                                      command

  `/api/device/status/`             POST              Report PC state and Device token
                                                      heartbeat

  `/api/device/commands/`           GET               Fetch pending       Device token
                                                      device commands

  `/api/device/commands/ack/`       POST              Acknowledge command Device token
                                                      result
  -----------------------------------------------------------------------------------------

### 6.1 User authentication

The website user signs in with their username and password. Django
verifies the password hash and establishes an authenticated session.

Use HTTPS for all production login and authenticated requests. Protect
session-based POST requests with CSRF protection.

### 6.2 Device authentication

The ESP8266 must use a separate device credential, not the website
user's password.

-   Generate a long, random device token.
-   Store it securely in the ESP8266 configuration.
-   Send it only over HTTPS.
-   Validate it on every device API request.
-   Show it once on a staff-only authenticated page; never expose it to
    browser JavaScript or repeat the display later.
-   Provide a way to rotate the token if it is compromised.

### 6.3 Example command

``` json
{
  "command_id": "unique-command-id",
  "command": "PRESS_POWER",
  "expires_in_seconds": 30
}
```

The device should only accept known, valid commands. It should reject
expired or previously executed command IDs.

### 6.4 Example device status report

``` json
{
  "device_id": "pc-controller-01",
  "pc_status": "ON",
  "relay_state": "INACTIVE"
}
```

The real API should also validate timestamps or use server-side receipt
times, and should not trust arbitrary status values without validation.

------------------------------------------------------------------------

## 7. Device-to-Server Communication

### Recommended method: ESP8266 polling

The ESP8266 periodically sends HTTPS requests to the Render backend to:

-   Announce that it is online.
-   Report the PC status input.
-   Retrieve pending commands.
-   Acknowledge completed commands.

Suggested initial polling interval: 3--5 seconds, subject to testing and
hosting limits. A slower interval reduces network traffic and server
requests but increases command delay.

### Command flow

``` mermaid
sequenceDiagram
    actor User
    participant Web as Django on Render
    participant DB as PostgreSQL
    participant ESP as ESP8266
    participant Relay

    User->>Web: Click Press Power Button
    Web->>DB: Save unique PENDING command
    Web-->>User: Command queued
    ESP->>Web: Poll for commands
    Web->>DB: Find pending command
    Web-->>ESP: Return command
    ESP->>ESP: Validate ID and expiry
    ESP->>Relay: Activate briefly
    Relay-->>ESP: Release after pulse
    ESP->>Web: Acknowledge command
    Web->>DB: Mark command executed
    ESP->>Web: Send PC status heartbeat
    Web->>DB: Update device status
    User->>Web: Refresh dashboard
    Web-->>User: Show latest status
```

### Important reliability rules

-   Give every command a unique ID.
-   Make command execution idempotent: the same command ID must never
    trigger the relay twice.
-   Expire commands that were not retrieved in time.
-   Acknowledge commands only after the relay pulse has completed.
-   If acknowledgement is lost, the device must remember executed
    command IDs so a retry cannot trigger the relay again.
-   On ESP8266 restart, initialize the relay to its safe inactive state.
-   Do not execute commands received with invalid authentication or
    expired timestamps.
-   Use a device heartbeat to determine online/offline state.

------------------------------------------------------------------------

## 8. ESP8266 Firmware Responsibilities

The firmware should perform these tasks:

1.  Initialize GPIO pins in safe states.
2.  Connect to the configured Wi-Fi network.
3.  Establish HTTPS connections with certificate validation.
4.  Authenticate to the device API using its device token.
5.  Read the PC status input.
6.  Send periodic heartbeat and status updates.
7.  Poll for pending commands.
8.  Validate command ID, type, and expiry.
9.  Trigger the relay for a short, configured pulse.
10. Release the relay and acknowledge the command.
11. Recover from Wi-Fi or server failures without unsafe relay
    activation.

### Firmware layout

The firmware is a single `firmware/esp8266/esp8266.ino` sketch. It
contains the configuration block, Wi-Fi recovery, HTTPS requests through
`WiFiClientSecure`, relay control, status sampling, command deduplication,
`setup()`, and `loop()`. See `firmware/esp8266/README.md` for the exact
values to fill in and the Arduino IDE setup steps.

### Suggested logic

``` text
START
  |
  v
Initialize relay as INACTIVE
  |
  v
Connect to Wi-Fi
  |
  v
Connect to backend securely
  |
  v
Read PC status input
  |
  v
Send heartbeat and status
  |
  v
Check for pending command
  |
  +---- No command ----> Wait briefly and repeat
  |
  v
Validate command ID, type and expiry
  |
  v
Has this command already executed?
  |
  +---- Yes ----> Return previous result
  |
  v
Activate relay briefly
  |
  v
Set relay INACTIVE
  |
  v
Record command ID as executed
  |
  v
Acknowledge command
  |
  v
Repeat
```

The firmware must not energize the relay merely because Wi-Fi reconnects
or the ESP8266 restarts.

------------------------------------------------------------------------

## 9. Suggested Project Folder Structure

``` text
pc-power-controller/
|
|-- firmware/
|   `-- esp8266/
|       |-- esp8266.ino
|       `-- README.md
|
|-- backend/
|   |-- manage.py
|   |-- requirements.txt
|   |-- config/
|   |   |-- settings.py
|   |   |-- urls.py
|   |   `-- wsgi.py
|   |
|   |-- accounts/
|   |   |-- views.py
|   |   |-- forms.py
|   |   |-- serializers.py
|   |   |-- backends.py
|   |   |-- urls.py
|   |   `-- management/commands/create_initial_user.py
|   |
|   |-- devices/
|   |   |-- models.py
|   |   |-- views.py
|   |   |-- serializers.py
|   |   |-- authentication.py
|   |   |-- urls.py
|   |   `-- management/commands/create_device.py
|   |
|   |-- templates/
|   |   |-- base.html
|   |   |-- login.html
|   |   |-- dashboard.html
|   |   |-- settings.html
|   |   `-- device_token.html
|   |
|   `-- static/
|       |-- css/
|       `-- js/
|
|-- docs/
|   |-- hardware.md
|   |-- api.md
|   `-- deployment.md
|
`-- README.md
```

------------------------------------------------------------------------

## 10. Render Deployment Plan

### 10.1 Backend deployment

1.  Create the Django project and test it locally.
2.  Add production dependencies to `requirements.txt`.
3.  Configure Django for PostgreSQL.
4.  Configure static files for production.
5.  Use a production application server such as Gunicorn.
6.  Push the code to a private or public GitHub repository as
    appropriate.
7.  Create a Render Web Service connected to the repository.
8.  Configure the build and start commands.
9.  Add environment variables in Render.
10. Run database migrations.
11. Create the initial user securely.
12. Test the website and device API over HTTPS.

### 10.2 Environment variables

  Variable                 Purpose
  ------------------------ ------------------------------------------------
  `DJANGO_SECRET_KEY`      Django secret key
  `DATABASE_URL`           PostgreSQL connection URL
  `DEBUG`                  Must be false in production
  `ALLOWED_HOSTS`          Permitted hostnames
  `CSRF_TRUSTED_ORIGINS`   Trusted HTTPS website origins
   `INITIAL_USERNAME`       Optional initial account setup
   `INITIAL_PASSWORD`       Optional one-time initial account setup secret
   `STATIC_DEV_USERNAME`    Optional dev-only login username (default `admin`)
   `STATIC_DEV_PASSWORD`    Optional dev-only login password
   `ENABLE_STATIC_DEV_USER` Explicit opt-in for the dev backend when `DEBUG=False`

The initial password should be used only by a secure one-time setup
process and should not remain exposed in logs or source code. Remove or
rotate it after account creation.

The static development login is enabled by `DEBUG=True` (local default)
or `ENABLE_STATIC_DEV_USER=1`; keep it disabled in production.

The device token is provisioned by the first login at `/device-token/`,
regenerated from the same page, and must not be confused with website
login credentials. On Render Free there is no Shell/one-off job access,
so the web page is the supported way to obtain or rotate it.


### 10.3 Hosting considerations

-   Check the current Render pricing and service limits before
    deployment.
-   Free web services may spin down after inactivity, causing delays on
    the next request.
-   Device polling may keep a service active because it generates
    inbound requests, but do not treat this as a guaranteed uptime
    arrangement.
-   Free database plans may have expiration or storage limitations.
-   For dependable 24/7 remote access, choose a hosting and database
    plan with suitable uptime and persistence.
-   Monitor server logs, API failures, database availability, and device
    heartbeat.

------------------------------------------------------------------------

## 11. Security Requirements

Security is essential because this system can physically control a
computer.

### Website security

-   Use Django's password hashing.
-   Require authentication for the dashboard and power-control API.
-   Use HTTPS.
-   Enable CSRF protection for session-authenticated POST requests.
-   Add login throttling and reasonable password rules.
-   Validate all input on the server.
-   Do not expose secrets in frontend JavaScript.
-   Keep `DEBUG=False` in production.
-   Use secure session and cookie settings.

### Device security

-   Use a unique, high-entropy device token.
-   Validate TLS certificates; do not disable certificate verification.
-   Restrict the device API to the registered device.
-   Validate command type, expiry, and unique ID.
-   Prevent replay and duplicate execution.
-   Keep relay inactive during boot, reset, and network failures.
-   Store Wi-Fi and device credentials carefully.
-   Rotate compromised credentials.

### Physical safety

-   Never connect a 12V signal directly to an ESP8266 GPIO.
-   Use isolation and suitable voltage/current protection.
-   Do not switch mains power as part of the normal PC power-button
    function.
-   Use an appropriate enclosure and avoid exposed conductive wiring.
-   Disconnect power before changing motherboard wiring.
-   Verify all connections against the motherboard and PSU manuals.

------------------------------------------------------------------------

## 12. Development Roadmap

### Phase 1 --- Hardware prototype

-   [ ] Select ESP8266 board.
-   [ ] Test Wi-Fi connection.
-   [ ] Test relay with a low-voltage test circuit.
-   [ ] Verify momentary relay pulse.
-   [ ] Design and test isolated PC status sensing.
-   [ ] Ensure ESP8266 remains powered while PC is off.
-   [ ] Verify safe behavior after reset and power loss.

### Phase 2 --- Django backend

-   [ ] Create Django project.
-   [ ] Configure PostgreSQL.
-   [ ] Configure user authentication.
-   [ ] Create initial user securely.
-   [ ] Implement credential change.
-   [ ] Create device model.
-   [ ] Create command model.
-   [ ] Implement authenticated device APIs.

### Phase 3 --- ESP8266 communication

-   [ ] Configure Wi-Fi.
-   [ ] Configure secure HTTPS.
-   [ ] Authenticate using device token.
-   [ ] Send heartbeat and PC status.
-   [ ] Retrieve pending commands.
-   [ ] Execute relay command once.
-   [ ] Acknowledge command.
-   [ ] Test reconnection and duplicate-command handling.

### Phase 4 --- Frontend

-   [ ] Create login page.
-   [ ] Create dashboard.
-   [ ] Display PC ON/OFF/UNKNOWN state.
-   [ ] Display ESP8266 ONLINE/OFFLINE state.
-   [ ] Add Press Power Button control.
-   [ ] Add credential settings page.
-   [ ] Display command status and errors.

### Phase 5 --- Testing and security

-   [ ] Test invalid login.
-   [ ] Test credential changes.
-   [ ] Test unauthorized API access.
-   [ ] Test expired commands.
-   [ ] Test duplicate commands.
-   [ ] Test Wi-Fi disconnection.
-   [ ] Test Render downtime or restart.
-   [ ] Test ESP8266 restart.
-   [ ] Verify relay remains inactive during faults.
-   [ ] Verify PC status is not shown as current when stale.

### Phase 6 --- Deployment

-   [ ] Push project to GitHub.
-   [ ] Deploy Django on Render.
-   [ ] Configure PostgreSQL.
-   [ ] Configure production environment variables.
-   [ ] Run migrations.
-   [ ] Create initial user.
-   [ ] Upload firmware to ESP8266.
-   [ ] Test remote access.
-   [ ] Test the complete system over the internet.

------------------------------------------------------------------------

## 13. Testing Scenarios

  -----------------------------------------------------------------------
  Scenario                            Expected result
  ----------------------------------- -----------------------------------
  PC is OFF and user requests a press Relay briefly activates; PC should
                                      start

  PC is ON and user requests a press  Relay briefly activates; OS follows
                                      configured power-button behavior

  ESP8266 loses Wi-Fi                 Dashboard eventually reports device
                                      offline

  Render is temporarily unavailable   ESP8266 keeps relay inactive and
                                      retries later

  User is not logged in               Power-control request is rejected

  Invalid device token                Device API request is rejected

  Same command is delivered twice     Relay executes it only once

  Command is expired                  Device rejects it

  ESP8266 restarts                    Relay initializes inactive

  Status sensor is disconnected or    PC state is reported as UNKNOWN or
  invalid                             fault
  -----------------------------------------------------------------------

------------------------------------------------------------------------

## 14. Future Enhancements

After the basic version works, possible improvements include:

-   Multiple PC controllers from one account.
-   PC status history.
-   Power-button action audit history.
-   Configurable relay pulse duration within safe limits.
-   Device health and Wi-Fi signal information.
-   Email or notification alerts when a device goes offline.
-   A maintenance mode that disables remote commands.
-   A local network fallback interface.
-   A more advanced status detector that distinguishes shutdown, sleep,
    and running states.

------------------------------------------------------------------------

## 15. Final Recommended Design

For the first version, keep the architecture simple:

-   **ESP8266 NodeMCU** as the controller.
-   **Momentary relay** connected across the motherboard's `PWR_SW`
    pins.
-   **Isolated status sensing** based on a suitable PC power signal.
-   **Independent or standby power** so the ESP8266 stays online when
    the PC is off.
-   **Django + Django REST Framework** for the website and APIs.
-   **PostgreSQL** for users, devices, commands, and status.
-   **Render** for hosting.
-   **HTTPS polling** from the ESP8266 to the backend.
-   **Django sessions** for website users and a separate device token
    for ESP8266 authentication.
-   **One-time token display** at `/device-token/` with regeneration.

This design is feasible and suitable for a small IoT project. The most
important part to validate before building the complete software is the
hardware: the ESP8266 must remain powered while the PC is off, the relay
must safely simulate a short power-button press, and the status-sensing
circuit must provide a safe 3.3V-compatible signal.

------------------------------------------------------------------------

## 16. Decisions to Confirm Before Implementation

The following details should be finalized before writing the circuit
diagram and production firmware:

1.  Exact ESP8266 board: NodeMCU, Wemos D1 Mini, or ESP-01.
2.  Exact relay module and whether it is active-high or active-low.
3.  Motherboard model and the correct `PWR_SW` header pins.
4.  PC status sensing method and electrical interface.
5.  ESP8266 power source: external USB supply or PSU 5V standby.
6.  Whether access is required from anywhere on the internet or only on
    the local network.
7.  Render service and database plan.

**Implementation order:** Complete and validate the hardware prototype
first, then build the Django APIs, then firmware communication, then the
website dashboard, and finally production deployment.
