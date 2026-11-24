#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <string.h>

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

unsigned long lastReconnect = 0;
unsigned long lastTest = 0;
unsigned long lastStatusLog = 0;
bool wasConnected = false;

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
  if (useTls()) {
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

bool httpRequest(const char* method, const char* path, const String& body, int& statusCode, String& payload) {
  payload = "";
  statusCode = 0;

  const String url = SERVER_BASE_URL;
  const bool tls = useTls();
  if (!tls && url.indexOf("://") >= 0 && !url.startsWith("http://")) {
    trace("CONFIG: SERVER_BASE_URL must start with http:// or https://");
    return false;
  }

  const String host = serverHost();
  const int port = serverPort();
  Client* client = tls ? static_cast<Client*>(&secureClient) : static_cast<Client*>(&plainClient);
  String hostHeader = host;
  if (!tls && port != 80) {
    hostHeader += String(":") + String(port);
  }

  if (host.length() == 0 || !client->connect(host.c_str(), port)) {
    trace(String(tls ? "TLS" : "TCP") + " connect failed to " + host + ":" + String(port));
    client->stop();
    return false;
  }
  if (!client->connected()) {
    trace("HTTP: connection dropped before request");
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
  client->println("User-Agent: ESP8266-debug");
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
    trace("HTTP: invalid response, raw=" + raw);
    client->stop();
    return false;
  }
  statusCode = statusLine.substring(space + 1).toInt();
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

String preview(const String& text) {
  String value = text.length() > 140 ? text.substring(0, 140) : text;
  value.replace("\n", " ");
  value.replace("\r", " ");
  return value;
}

bool requestWithRetry(const char* method, const char* path, const String& body, int& statusCode, String& payload) {
  for (uint8_t attempt = 1; attempt <= 3; attempt++) {
    if (httpRequest(method, path, body, statusCode, payload)) {
      return true;
    }
    if (attempt < 3) {
      trace("HTTP: retry " + String(attempt) + " for " + String(path));
      delay(400 * attempt);
    }
  }
  return false;
}

void explainCode(int code) {
  if (code == 200) {
    trace("HINT: request OK");
  } else if (code == 400) {
    trace("HINT: HTTP 400 - add this IP to Django ALLOWED_HOSTS");
  } else if (code == 401 || code == 403) {
    trace("HINT: token rejected - regenerate/copy DEVICE_TOKEN");
  } else if (code == 404) {
    trace("HINT: wrong server URL or API path");
  } else if (code >= 500) {
    trace("HINT: server error - check Django logs");
  }
}

void testServer() {
  trace("TEST: starting HTTP checks");
  int code = 0;
  String payload;

  if (requestWithRetry("GET", "/api/device/time/", "", code, payload)) {
    trace("TEST time -> HTTP " + String(code) + " body=" + preview(payload));
    explainCode(code);
  } else {
    trace("TEST time -> request failed");
  }

  delay(500);
  code = 0;
  payload = "";
  const String body = "{\"pc_status\":\"OFF\",\"relay_state\":\"INACTIVE\"}";
  if (requestWithRetry("POST", "/api/device/status/", body, code, payload)) {
    trace("TEST status -> HTTP " + String(code) + " body=" + preview(payload));
    explainCode(code);
  } else {
    trace("TEST status -> request failed");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  trace("DEBUG: ESP8266 diagnostics starting");
  trace("DEBUG: server=" + String(SERVER_BASE_URL));
  trace("DEBUG: device token length=" + String(strlen(DEVICE_TOKEN)) + " (value hidden)");

  plainClient.setTimeout(15);
  secureClient.setTimeout(15);
  if (useTls()) {
    secureClient.setTrustAnchors(&trustAnchors);
  }
  trace("DEBUG: mode=" + String(useTls() ? "HTTPS" : "HTTP"));

  WiFi.persistent(false);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  trace("WiFi: connecting to " + String(WIFI_SSID));
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wasConnected) {
      wasConnected = true;
      trace("WiFi: CONNECTED, IP=" + WiFi.localIP().toString() + ", RSSI=" + String(WiFi.RSSI()) + " dBm");
      trace("WiFi: gateway=" + WiFi.gatewayIP().toString() + ", MAC=" + WiFi.macAddress());
      lastTest = 0;
    }
    if (millis() - lastTest >= 10000) {
      lastTest = millis();
      testServer();
    }
  } else {
    if (wasConnected) {
      wasConnected = false;
      trace("WiFi: DISCONNECTED, status=" + String(wifiStatusName()));
    }
    if (millis() - lastStatusLog >= 5000) {
      lastStatusLog = millis();
      trace("WiFi: status=" + String(wifiStatusName()));
    }
    if (millis() - lastReconnect >= 5000) {
      lastReconnect = millis();
      trace("WiFi: calling reconnect()");
      WiFi.reconnect();
    }
  }
  delay(250);
}
