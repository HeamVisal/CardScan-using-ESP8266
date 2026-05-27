/************************************************************
 * CARD KEY SYSTEM (Fixed)
 * - RFID RC522
 * - MAX7219 LED Matrix (4x8x8)
 * - Active Buzzer (3-wire)
 * - WiFi + Web (must open / to start scanning)
 * - Google Sheet via Apps Script Web App:
 *   GET  /exec?action=users  -> JSON array of users
 *   POST /exec               -> append log
 ************************************************************/

#include <SPI.h>
#include <MFRC522.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>   // IMPORTANT: better HTTPS + redirects
#include <ArduinoJson.h>
#include <time.h>

/* ================= USER STRUCT (must be early) ================= */
struct UserRec {
  String uid;
  String firstName;
  String lastName;
  String position;
  int age;
};


/* ================= WIFI SETTINGS ================= */
const char* WIFI_SSID = "HakSeng";
const char* WIFI_PASS = "0969856032";

bool serverStarted = false;
unsigned long webStateStartMs = 0;
bool webStateDone = false;

/* ================= RFID SETUP ================= */
#define SS_PIN  D8
#define RST_PIN D3
MFRC522 rfid(SS_PIN, RST_PIN);

/* ================= LED MATRIX SETUP ================= */
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define DIN_PIN D2
#define CLK_PIN D0
#define CS_PIN  D1
MD_Parola P(HARDWARE_TYPE, DIN_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

/* ================= BUZZER SETUP ================= */
#define BUZZER_PIN D4
const bool BUZZER_ACTIVE_LOW = true;

void buzzerOn()  { digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_LOW ? LOW : HIGH); }
void buzzerOff() { digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_LOW ? HIGH : LOW); }

void buzz(int onMs, int offMs = 150) {
  buzzerOn();  delay(onMs);
  buzzerOff(); delay(offMs);
}
void soundScan()    { buzz(30); buzz(30); }
void soundSuccess() { buzz(40); buzz(40); buzz(80); }
void soundFail()    { buzz(80); buzz(80); }

/* ================= DISPLAY ================= */
void showText(const char* text, int brightness) {
  P.displayClear();
  P.setIntensity(brightness);
  P.displayText(text, PA_CENTER, 50, 0, PA_PRINT, PA_NO_EFFECT);
  while (!P.displayAnimate()) {}
}

/* ================= GOOGLE APPS SCRIPT URL ================= */
const char* GS_BASE =
"https://script.google.com/macros/s/AKfycbya_mDCEcOzxI1cMlDQ2uYGq5k8k1q6q8dkDV4ulb__j4mIyAfqJcD4YmpQH3kiWHJYEA/exec";

/* ================= USERS CACHE ================= */
#define MAX_USERS 60
UserRec users[MAX_USERS];
int userCount = 0;

bool findUserByUID(const String& uid, UserRec& out) {
  for (int i = 0; i < userCount; i++) {
    if (users[i].uid == uid) {
      out = users[i];
      return true;
    }
  }
  return false;
}

/* ================= TIME SETTINGS ================= */
#define TZ_OFFSET 7 * 3600  // Cambodia UTC+7

void waitForTime() {
  // waits until NTP time is valid
  while (time(nullptr) < 1700000000) delay(500);
}

void getDateTime(String& timeStr, int& d, int& m, int& y) {
  time_t now = time(nullptr) + TZ_OFFSET;
  struct tm t;
  gmtime_r(&now, &t);

  char buf[9];
  sprintf(buf, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
  timeStr = buf;

  d = t.tm_mday;
  m = t.tm_mon + 1;
  y = t.tm_year + 1900;
}

/* ================= HTTPS (GET/POST) using HTTPClient ================= */
String httpsGET(const String& url) {
  WiFiClientSecure client;
  client.setInsecure();               // easiest for ESP8266

  HTTPClient https;
  https.setTimeout(15000);
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!https.begin(client, url)) return "";

  int code = https.GET();
  String body = (code > 0) ? https.getString() : "";
  Serial.print("GET code: "); Serial.println(code);

  https.end();
  return body;
}

bool httpsPOST(const String& url, const String& jsonBody) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.setTimeout(5000);
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!https.begin(client, url)) return false;

  https.addHeader("Content-Type", "application/json");
  int code = https.POST((uint8_t*)jsonBody.c_str(), jsonBody.length());
  String body = (code > 0) ? https.getString() : "";
  Serial.print("POST code: "); Serial.println(code);
  Serial.println(body);

  https.end();
  return (code >= 200 && code < 300);
}

/* ================= LOAD USERS FROM SHEET ================= */
bool loadUsers() {
  showText("SYNC", 1);

  String url = String(GS_BASE) + "?action=users";
  String json = httpsGET(url);

  Serial.println("----- USERS RESPONSE START -----");
  Serial.println(json);
  Serial.println("----- USERS RESPONSE END -----");

  if (json.length() == 0) return false;

  DynamicJsonDocument doc(16000);
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
    return false;
  }

  if (!doc.is<JsonArray>()) {
    Serial.println("Response is not a JSON array.");
    return false;
  }

  userCount = 0;

  for (JsonObject u : doc.as<JsonArray>()) {
    if (userCount >= MAX_USERS) break;

    String uid = u["uid"] | "";
    uid.toUpperCase();
    uid.trim();
    if (uid.length() == 0) continue;

    users[userCount].uid       = uid;
    users[userCount].firstName = String(u["firstName"] | "");
    users[userCount].lastName  = String(u["lastName"] | "");
    users[userCount].position  = String(u["position"] | "");
    users[userCount].age       = u["age"] | 0;

    userCount++;
  }

  Serial.print("Users loaded: ");
  Serial.println(userCount);

  return (userCount > 0);
}

/* ================= SEND LOG TO SHEET ================= */
void sendLog(const String& uid, const String& name, const String& pos, const String& result) {
  String timeStr;
  int d, m, y;
  getDateTime(timeStr, d, m, y);

  StaticJsonDocument<256> doc;
  doc["uid"] = uid;
  doc["name"] = name;
  doc["position"] = pos;
  doc["result"] = result;
  doc["time"] = timeStr;
  doc["day"] = d;
  doc["month"] = m;
  doc["year"] = y;

  String payload;
  serializeJson(doc, payload);

  // POST to /exec (no query needed)
  httpsPOST(String(GS_BASE), payload);
}

/* ================= WEB SERVER ================= */
ESP8266WebServer server(80);
bool webVisited = false;

/* ================= UID FORMAT HELPER ================= */
String readUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    byte b = rfid.uid.uidByte[i];
    if (b < 0x10) uid += "0";          // leading zero
    uid += String(b, HEX);
  }
  uid.toUpperCase();
  return uid;
}

// Show text no blinking
String lastLedText = "";

void showTextFixed(const String& text, int brightness) {
  if (text == lastLedText) return;
  lastLedText = text;

  P.displayClear();
  P.setIntensity(brightness);

  // Speed=0, Pause=0, still need animate calls to render
  P.displayText(text.c_str(), PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);

  // Render a bit longer so it fully shows
  unsigned long t0 = millis();
  while (millis() - t0 < 250) {   // was 80
    P.displayAnimate();
    delay(1);
  }
}

// =============== Web Setup ===============

/* ================= RECENT SCANS TRACKING ================= */
#define MAX_RECENT_SCANS 10
struct ScanRecord {
  String uid;
  String name;
  String result;
  String timestamp;
};
ScanRecord recentScans[MAX_RECENT_SCANS];
int scanIndex = 0;

void addScanRecord(const String& uid, const String& name, const String& result) {
  String timeStr;
  int d, m, y;
  getDateTime(timeStr, d, m, y);
  
  recentScans[scanIndex].uid = uid;
  recentScans[scanIndex].name = name;
  recentScans[scanIndex].result = result;
  recentScans[scanIndex].timestamp = String(d) + "/" + String(m) + "/" + String(y) + " " + timeStr;
  
  scanIndex = (scanIndex + 1) % MAX_RECENT_SCANS;
}


/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();

  SPI.begin();
  rfid.PCD_Init();

  P.begin();
  P.displayClear();

  // ---------------- WiFi connect (retry forever) ----------------
  WiFi.mode(WIFI_STA);
  showText("WIFI", 2);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    showText("....", 1);
  }

  soundSuccess();

  Serial.print("Connected IP: ");
  Serial.println(WiFi.localIP());

  // ---------------- NTP time ----------------
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  waitForTime();

  // ---------------- Load users (retry forever) ----------------
  while (!loadUsers()) {
    showText("RETRY", 1);
    delay(1500);
  }

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ WebSetup
  // ---------------- Web routes ----------------
  server.on("/", []() {
    String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Card Access System</title><style>"
    "* { margin:0; padding:0; box-sizing:border-box; } "
    "body { font-family:'Segoe UI',Tahoma,sans-serif; background:linear-gradient(135deg,#667eea 0%,#764ba2 100%); min-height:100vh; padding:20px; } "
    ".container { max-width:1200px; margin:0 auto; } "
    ".header { background:#fff; padding:25px; border-radius:15px; box-shadow:0 10px 30px rgba(0,0,0,0.2); margin-bottom:20px; text-align:center; } "
    ".header h1 { color:#667eea; font-size:2em; margin-bottom:10px; } "
    ".header p { color:#666; font-size:1.1em; } "
    ".stats { display:grid; grid-template-columns:repeat(auto-fit,minmax(200px,1fr)); gap:15px; margin-bottom:20px; } "
    ".stat-card { background:#fff; padding:20px; border-radius:12px; box-shadow:0 5px 15px rgba(0,0,0,0.15); text-align:center; } "
    ".stat-value { font-size:2.5em; font-weight:bold; color:#667eea; margin-bottom:5px; } "
    ".stat-label { color:#888; font-size:0.9em; text-transform:uppercase; letter-spacing:1px; } "
    ".card { background:#fff; padding:25px; border-radius:15px; box-shadow:0 10px 30px rgba(0,0,0,0.2); } "
    ".card h2 { color:#333; margin-bottom:20px; font-size:1.5em; border-bottom:3px solid #667eea; padding-bottom:10px; } "
    ".scan-item { padding:15px; margin-bottom:10px; border-radius:8px; background:#f8f9fa; border-left:4px solid #667eea; display:flex; justify-content:space-between; align-items:center; } "
    ".scan-item.success { border-left-color:#28a745; background:#d4edda; } "
    ".scan-item.deny { border-left-color:#dc3545; background:#f8d7da; } "
    ".scan-name { font-weight:600; font-size:1.1em; color:#333; } "
    ".scan-time { color:#666; font-size:0.85em; } "
    ".badge { padding:5px 12px; border-radius:20px; font-size:0.85em; font-weight:600; } "
    ".badge.ok { background:#28a745; color:#fff; } "
    ".badge.deny { background:#dc3545; color:#fff; } "
    ".status-live { display:inline-block; width:10px; height:10px; background:#28a745; border-radius:50%; margin-right:8px; animation:pulse 2s infinite; } "
    "@keyframes pulse { 0%,100% { opacity:1; } 50% { opacity:0.5; } } "
    ".no-data { text-align:center; color:#999; padding:30px; font-style:italic; } "
    "@media (max-width:768px) { .header h1 { font-size:1.5em; } .stat-value { font-size:2em; } }"
    "</style></head><body>"
    "<div class='container'>"
    "<div class='header'><h1>Card Access Control</h1><p><span class='status-live'></span>System Online</p></div>"
    "<div class='stats'>"
    "<div class='stat-card'><div class='stat-value' id='userCount'>-</div><div class='stat-label'>Registered Users</div></div>"
    "<div class='stat-card'><div class='stat-value' id='totalScans'>-</div><div class='stat-label'>Total Scans</div></div>"
    "<div class='stat-card'><div class='stat-value' id='uptime'>-</div><div class='stat-label'>Uptime</div></div>"
    "</div>"
    "<div class='card'><h2>Recent Activity</h2><div id='scans'><div class='no-data'>No scans yet...</div></div></div>"
    "</div>"
    "<script>"
    "let totalScans = 0; const startTime = Date.now();"
    "function updateData() {"
    "  fetch('/api/data').then(r => r.json()).then(data => {"
    "    document.getElementById('userCount').textContent = data.users;"
    "    totalScans = data.scans.filter(s => s.name).length;"
    "    document.getElementById('totalScans').textContent = totalScans;"
    "    const upMins = Math.floor((Date.now() - startTime) / 60000);"
    "    document.getElementById('uptime').textContent = upMins < 60 ? upMins + 'm' : Math.floor(upMins/60) + 'h';"
    "    const scansDiv = document.getElementById('scans');"
    "    if (data.scans.length === 0 || !data.scans[0].name) {"
    "      scansDiv.innerHTML = '<div class=\"no-data\">No scans yet...</div>';"
    "    } else {"
    "      scansDiv.innerHTML = data.scans.map(s => {"
    "        if (!s.name) return '';"
    "        const cls = s.result === 'OK' ? 'success' : 'deny';"
    "        const badge = s.result === 'OK' ? 'ok' : 'deny';"
    "        return `<div class='scan-item ${cls}'><div><div class='scan-name'>${s.name}</div><div class='scan-time'>${s.time}</div></div><span class='badge ${badge}'>${s.result}</span></div>`;"
    "      }).join('');"
    "    }"
    "  }).catch(e => console.error(e));"
    "}"
    "updateData(); setInterval(updateData, 2000);"
    "</script></body></html>");
    server.send(200, "text/html", html);
  });

  server.on("/api/data", []() {
    String json = "{\"users\":" + String(userCount) + ",\"scans\":[";
    bool first = true;
    for (int i = 0; i < MAX_RECENT_SCANS; i++) {
      int idx = (scanIndex - 1 - i + MAX_RECENT_SCANS) % MAX_RECENT_SCANS;
      if (recentScans[idx].name.length() > 0) {
        if (!first) json += ",";
        json += "{\"name\":\"" + recentScans[idx].name + "\",";
        json += "\"result\":\"" + recentScans[idx].result + "\",";
        json += "\"time\":\"" + recentScans[idx].timestamp + "\"}";
        first = false;
      }
    }
    json += "]}";
    server.send(200, "application/json", json);
  });

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ WebSetup

  // ---------------- Start Web (retry until OK) ----------------
  while (true) {
    // Show WEB at least 2 seconds
    showText("WEB", 2);

    // Print web IP
    Serial.print("Web IP: http://");
    Serial.println(WiFi.localIP());

    // Try to start web server
    server.begin();

    // Keep WEB on screen for >= 2 seconds while handling clients
    unsigned long t0 = millis();
    while (millis() - t0 < 2000) {
      server.handleClient();
      delay(5);
    }

    // Simple "health check": WiFi must still be connected
    if (WiFi.status() == WL_CONNECTED) {
      break;  // OK -> continue
    }

    // If not OK -> show FAIL and retry
    showText("FAIL", 2);
    delay(800);

    // Reconnect WiFi before retrying web
    showText("WIFI", 2);
    WiFi.disconnect();
    delay(300);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      showText("....", 1);
    }
  }

  // ---------------- Ready for scan ----------------
  showTextFixed("READY", 2);
  soundScan();
}


/* ================= LOOP ================= */
void loop() {
  server.handleClient();

  showTextFixed("READY", 2);

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  soundScan();
  showTextFixed("SCAN", 2);

  String uid = readUID();
  Serial.print("UID: ");
  Serial.println(uid);

  UserRec u;
  if (findUserByUID(uid, u)) {
    String name = u.firstName + " " + u.lastName;
    showTextFixed(u.firstName, 2);
    soundSuccess();
    addScanRecord(uid, name, "OK");
    delay(1500); // Show name for 1.5 seconds
    // Send log in background (non-blocking would be better, but this improves UX)
    sendLog(uid, name, u.position, "OK");
  } else {
    showTextFixed("DENY", 2);
    soundFail();
    addScanRecord(uid, "Unknown Card", "DENY");
    delay(1500); // Show DENY for 1.5 seconds
    sendLog(uid, "-", "-", "DENY");
  }



  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  // delay(300);
}

