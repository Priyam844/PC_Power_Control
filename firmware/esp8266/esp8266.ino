#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <string.h>

constexpr uint8_t RELAY_PIN = D2;
constexpr uint8_t SENSOR_PIN = D5;

constexpr bool RELAY_ACTIVE_LOW = true;
constexpr bool SENSOR_ACTIVE_LOW = false;

constexpr unsigned long RELAY_PULSE_MS = 400;
constexpr unsigned long POLL_INTERVAL_MS = 4000;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr unsigned long HTTP_TIMEOUT_MS = 15000;
constexpr size_t MAX_EXECUTED_IDS = 32;

const char* const WIFI_SSID = "YOUR_WIFI_SSID";
const char* const WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* const DEVICE_TOKEN = "YOUR_DEVICE_TOKEN";
const char* const SERVER_BASE_URL = "192.168.0.103:8000";

const char* const ROOT_CA = R"EOF(-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

X509List trustAnchors(ROOT_CA);
WiFiClient plainClient;
WiFiClientSecure secureClient;

unsigned long wifiLastAttempt = 0;
unsigned long wifiLastLog = 0;
bool wifiWasConnected = false;
String executedIds[MAX_EXECUTED_IDS];
size_t executedCount = 0;

void trace(const String& message) {
  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.println(message);
}

const char* wifiStatusName() {
  switch (WiFi.status()) {
    case WL_CONNECTED: return "CONNECTED";
    case WL_NO_SHIELD: return "NO_SHIELD";
    case WL_IDLE_STATUS: return "IDLE";
    case WL_NO_SSID_AVAIL: return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "SCAN_COMPLETED";
    case WL_CONNECT_FAILED: return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED: return "DISCONNECTED";
    default: return "UNKNOWN";
  }
}

void relayWrite(bool active) {
  const bool level = RELAY_ACTIVE_LOW ? !active : active;
  digitalWrite(RELAY_PIN, level ? HIGH : LOW);
}

void beginRelay() {
  pinMode(RELAY_PIN, OUTPUT);
  relayWrite(false);
}

void pulseRelay(unsigned long durationMs) {
  trace("Relay: activating for " + String(durationMs) + " ms");
  relayWrite(true);
  delay(durationMs);
  relayWrite(false);
  trace("Relay: inactive");
}

void beginSensor() {
  pinMode(SENSOR_PIN, INPUT);
}

const char* readPcStatus() {
  uint8_t highSamples = 0;
  uint8_t lowSamples = 0;

  for (uint8_t i = 0; i < 5; i++) {
    if (digitalRead(SENSOR_PIN) == HIGH) {
      highSamples++;
    } else {
      lowSamples++;
    }
    delay(4);
  }

  if (highSamples != 5 && lowSamples != 5) {
    return "UNKNOWN";
  }

  const bool pinHigh = highSamples == 5;
  const bool pcOn = SENSOR_ACTIVE_LOW ? !pinHigh : pinHigh;
  return pcOn ? "ON" : "OFF";
}

void beginWifi() {
  trace("WiFi: connecting to configured network");
  WiFi.persistent(false);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool waitForWifi(unsigned long timeoutMs) {
  const unsigned long started = millis();
  unsigned long lastReport = 0;
  while (!wifiConnected() && millis() - started < timeoutMs) {
    if (millis() - lastReport >= 2000) {
      lastReport = millis();
      trace("WiFi: waiting, status=" + String(wifiStatusName()));
    }
    delay(250);
  }
  return wifiConnected();
}

void maintainWifi() {
  if (wifiConnected()) {
    if (!wifiWasConnected) {
      trace("WiFi: CONNECTED, IP=" + WiFi.localIP().toString() + ", RSSI=" + String(WiFi.RSSI()) + " dBm");
      wifiWasConnected = true;
    }
    wifiLastAttempt = millis();
    return;
  }
  if (wifiWasConnected) {
    trace("WiFi: DISCONNECTED, retrying");
    wifiWasConnected = false;
  }
  if (millis() - wifiLastAttempt < 1000) {
    return;
  }
  wifiLastAttempt = millis();
  if (millis() - wifiLastLog >= 10000) {
    wifiLastLog = millis();
    trace("WiFi: still OFFLINE, status=" + String(wifiStatusName()));
  }
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool useTls() {
  return String(SERVER_BASE_URL).startsWith("https://");
}

String serverAuthority() {
  const String url = SERVER_BASE_URL;
  int start = url.indexOf("://");
  start = start >= 0 ? start + 3 : 0;
  int end = url.indexOf('/', start);
  if (end < 0) {
    end = url.length();
  }
  return url.substring(start, end);
}

String serverHost() {
  const String authority = serverAuthority();
  const int colon = authority.indexOf(':');
  return colon >= 0 ? authority.substring(0, colon) : authority;
}

int serverPort() {
  if (String(SERVER_BASE_URL).startsWith("https://")) {
    return 443;
  }
  const String authority = serverAuthority();
  const int colon = authority.indexOf(':');
  if (colon < 0) {
    return 80;
  }
  const int port = authority.substring(colon + 1).toInt();
  return port > 0 ? port : 80;
}

void beginApi() {
  plainClient.setTimeout(HTTP_TIMEOUT_MS / 1000);
  secureClient.setTimeout(HTTP_TIMEOUT_MS / 1000);
  if (useTls()) {
    secureClient.setTrustAnchors(&trustAnchors);
  }
  trace("API: mode=" + String(useTls() ? "HTTPS" : "HTTP") + ", host=" + serverHost() + ":" + String(serverPort()));
}

bool isSuccess(int code) {
  return code >= 200 && code < 300;
}

bool httpRequest(const char* method, const char* path, const String& body, String& payload, int& statusCode) {
  payload = "";
  statusCode = 0;

  const String url = SERVER_BASE_URL;
  const bool tls = useTls();
  if (!tls && url.indexOf("://") >= 0 && !url.startsWith("http://")) {
    trace("API: SERVER_BASE_URL must start with http:// or https://");
    return false;
  }

  const String host = serverHost();
  const int port = serverPort();
  Client* client = tls ? static_cast<Client*>(&secureClient) : static_cast<Client*>(&plainClient);
  String hostHeader = host;
  if (!tls && port != 80) {
    hostHeader += String(":") + String(port);
  }

  trace(String(method) + " " + path + " -> " + String(tls ? "HTTPS" : "HTTP") + " " + host + ":" + String(port));
  if (host.length() == 0 || !client->connect(host.c_str(), port)) {
    trace(String(tls ? "TLS" : "TCP") + " connect failed to " + host + ":" + String(port));
    client->stop();
    return false;
  }
  if (!client->connected()) {
    trace("API: connection dropped before request");
    client->stop();
    return false;
  }

  client->print(method);
  client->print(" ");
  client->print(path);
  client->println(" HTTP/1.1");
  client->print("Host: ");
  client->println(hostHeader);
  client->print("Authorization: Token ");
  client->println(DEVICE_TOKEN);
  client->println("User-Agent: ESP8266");
  client->println("Connection: close");
  if (body.length() > 0) {
    client->println("Content-Type: application/json");
    client->print("Content-Length: ");
    client->println(body.length());
  }
  client->println();
  if (body.length() > 0) {
    client->print(body);
  }
  client->flush();

  const unsigned long started = millis();
  while (!client->available() && client->connected() && millis() - started < 15000) {
    delay(10);
  }

  const String statusLine = client->readStringUntil('\n');
  const int space = statusLine.indexOf(' ');
  if (space < 0) {
    String raw = statusLine;
    raw.replace("\n", " ");
    raw.replace("\r", " ");
    if (raw.length() > 60) {
      raw = raw.substring(0, 60);
    }
    trace("API: invalid HTTP response, raw=" + raw);
    client->stop();
    return false;
  }
  statusCode = statusLine.substring(space + 1).toInt();
  trace(String(method) + " " + path + " -> HTTP " + String(statusCode));

  int contentLength = -1;
  bool headersDone = false;
  const unsigned long headerStart = millis();
  while (!headersDone && millis() - headerStart < 15000) {
    if (client->available()) {
      const String line = client->readStringUntil('\n');
      if (line.length() <= 1) {
        headersDone = true;
        break;
      }
      const int colon = line.indexOf(':');
      if (colon > 0 && line.substring(0, colon).equalsIgnoreCase("Content-Length")) {
        contentLength = line.substring(colon + 1).toInt();
      }
    } else {
      delay(2);
    }
  }

  payload = "";
  if (contentLength > 0) {
    const unsigned long bodyStart = millis();
    while (static_cast<int>(payload.length()) < contentLength && millis() - bodyStart < 15000) {
      if (client->available()) {
        payload += static_cast<char>(client->read());
      } else {
        delay(2);
      }
    }
  } else {
    payload = client->readString();
  }
  client->stop();
  return true;
}

bool httpRequestWithRetry(const char* method, const char* path, const String& body, String& payload, int& statusCode) {
  for (uint8_t attempt = 1; attempt <= 3; attempt++) {
    if (httpRequest(method, path, body, payload, statusCode)) {
      return true;
    }
    if (attempt < 3) {
      trace("API: retry " + String(attempt) + " for " + String(path));
      delay(400 * attempt);
    }
  }
  return false;
}

bool postStatus(const char* pcStatus, const char* relayState) {
  StaticJsonDocument<192> doc;
  doc["pc_status"] = pcStatus;
  doc["relay_state"] = relayState;

  String body;
  serializeJson(doc, body);

  String response;
  int code = 0;
  return httpRequestWithRetry("POST", "/api/device/status/", body, response, code) && isSuccess(code);
}

bool fetchCommand(String& commandId, int& expiresInSeconds) {
  commandId = "";
  expiresInSeconds = 0;

  String payload;
  int code = 0;
  if (!httpRequestWithRetry("GET", "/api/device/commands/", "", payload, code)) {
    trace("Command poll: request failed");
    return false;
  }
  if (!isSuccess(code)) {
    trace("Command poll: HTTP " + String(code));
    return false;
  }

  StaticJsonDocument<1024> doc;
  const DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    String raw = payload;
    raw.replace("\n", " ");
    raw.replace("\r", " ");
    if (raw.length() > 180) {
      raw = raw.substring(0, 180);
    }
    trace("Command poll: invalid JSON payload=" + raw);
    return false;
  }

  JsonObject command = doc["command"];
  if (command.isNull()) {
    trace("Command poll: no pending command");
    return false;
  }

  const char* id = command["command_id"];
  const char* type = command["command"];
  if (!id || !type || strcmp(type, "PRESS_POWER") != 0) {
    trace("Command poll: unsupported command");
    return false;
  }

  commandId = id;
  expiresInSeconds = command["expires_in_seconds"] | 0;
  if (expiresInSeconds <= 0) {
    trace("Command poll: command expired");
    return false;
  }

  trace("Command poll: " + commandId + ", expires in " + String(expiresInSeconds) + "s");
  return true;
}

bool ackCommand(const char* commandId, const char* status, const char* result) {
  StaticJsonDocument<256> doc;
  doc["command_id"] = commandId;
  doc["status"] = status;
  if (result && result[0] != '\0') {
    doc["result"] = result;
  }

  String body;
  serializeJson(doc, body);

  String response;
  int code = 0;
  return httpRequestWithRetry("POST", "/api/device/commands/ack/", body, response, code) && isSuccess(code);
}

bool wasExecuted(const String& commandId) {
  for (size_t i = 0; i < executedCount; i++) {
    if (executedIds[i] == commandId) {
      return true;
    }
  }
  return false;
}

void rememberExecuted(const String& commandId) {
  if (wasExecuted(commandId)) {
    return;
  }
  if (executedCount < MAX_EXECUTED_IDS) {
    executedIds[executedCount++] = commandId;
    return;
  }
  for (size_t i = 1; i < MAX_EXECUTED_IDS; i++) {
    executedIds[i - 1] = executedIds[i];
  }
  executedIds[MAX_EXECUTED_IDS - 1] = commandId;
}

void setup() {
  Serial.begin(115200);
  delay(100);

  trace("Boot: ESP8266 PC Power Controller starting");
  trace("Boot: relay pin=D2, sensor pin=D5, relay starts INACTIVE");
  trace("Boot: device token configured (" + String(strlen(DEVICE_TOKEN)) + " chars, value hidden)");
  trace("Boot: server=" + String(SERVER_BASE_URL));

  beginRelay();
  beginSensor();
  beginWifi();
  beginApi();

  if (waitForWifi(WIFI_CONNECT_TIMEOUT_MS)) {
    wifiWasConnected = true;
    trace("WiFi: CONNECTED at boot, IP=" + WiFi.localIP().toString() + ", RSSI=" + String(WiFi.RSSI()) + " dBm");
  } else {
    trace("WiFi: not connected at boot; retrying in main loop");
  }
}

void loop() {
  maintainWifi();

  if (wifiConnected()) {
    const char* pcStatus = readPcStatus();
    trace("Sensor: D5=" + String(digitalRead(SENSOR_PIN) == HIGH ? "HIGH" : "LOW") + ", PC status=" + String(pcStatus));
    if (postStatus(pcStatus, "INACTIVE")) {
      trace("Heartbeat: sent");
    } else {
      trace("Heartbeat: failed");
    }

    String commandId;
    int expiresInSeconds = 0;

    if (fetchCommand(commandId, expiresInSeconds)) {
      if (wasExecuted(commandId)) {
        trace("Command: duplicate, skipping relay");
        if (ackCommand(commandId.c_str(), "EXECUTED", "duplicate")) {
          trace("ACK: sent (duplicate)");
        } else {
          trace("ACK: failed");
        }
      } else {
        pulseRelay(RELAY_PULSE_MS);
        rememberExecuted(commandId);
        if (ackCommand(commandId.c_str(), "EXECUTED", "ok")) {
          trace("ACK: sent (ok)");
        } else {
          trace("ACK: failed");
        }
      }
    }
  } else {
    trace("Loop: WiFi OFFLINE, skipping polling");
  }

  delay(POLL_INTERVAL_MS);
}
