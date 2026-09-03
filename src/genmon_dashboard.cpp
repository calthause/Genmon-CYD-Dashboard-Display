#include "genmon_dashboard.h"
#include "config.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <time.h>

static DashboardConfig g_cfg;

static const char* PREFS_NS = "gencfg";
static const char* PREFS_KEY = "json";

static void defaultConfig(DashboardConfig& cfg)
{
  strlcpy(cfg.wifiSsid, WIFI_SSID, sizeof(cfg.wifiSsid));
  strlcpy(cfg.wifiPass, WIFI_PASSWORD, sizeof(cfg.wifiPass));
  strlcpy(cfg.genmonHost, GENMON_HOST, sizeof(cfg.genmonHost));
  cfg.genmonPort = GENMON_PORT;
}

static String configToJson(const DashboardConfig& cfg)
{
  JsonDocument doc;
  doc["wifi_ssid"] = cfg.wifiSsid;
  doc["wifi_pass"] = cfg.wifiPass;
  doc["genmon_host"] = cfg.genmonHost;
  doc["genmon_port"] = cfg.genmonPort;
  String out;
  serializeJson(doc, out);
  return out;
}

static bool jsonToConfig(const String& json, DashboardConfig& cfg)
{
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err)
  {
    return false;
  }

  strlcpy(cfg.wifiSsid, doc["wifi_ssid"] | cfg.wifiSsid, sizeof(cfg.wifiSsid));
  strlcpy(cfg.wifiPass, doc["wifi_pass"] | cfg.wifiPass, sizeof(cfg.wifiPass));
  strlcpy(cfg.genmonHost, doc["genmon_host"] | cfg.genmonHost, sizeof(cfg.genmonHost));
  cfg.genmonPort = doc["genmon_port"] | cfg.genmonPort;
  return true;
}

bool dashboardLoadConfig(DashboardConfig& cfg)
{
  defaultConfig(cfg);

  Preferences prefs;
  prefs.begin(PREFS_NS, true);
  String json = prefs.getString(PREFS_KEY, "");
  prefs.end();

  if (!json.isEmpty() && jsonToConfig(json, cfg))
  {
    Serial.println("[Config] Loaded from flash");
    return true;
  }

  Serial.println("[Config] Using defaults / config.h");
  return false;
}

static void saveConfig(const DashboardConfig& cfg)
{
  Preferences prefs;
  prefs.begin(PREFS_NS, false);
  prefs.putString(PREFS_KEY, configToJson(cfg));
  prefs.end();
  Serial.println("[Config] Saved to flash");
}

const DashboardConfig& dashboardGetConfig()
{
  return g_cfg;
}

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------
static const uint16_t CLR_BG       = 0x0000; // Black
static const uint16_t CLR_BLACK    = 0x0000; // Black
static const uint16_t CLR_HEADER   = 0x1D4E; // Dark blue
static const uint16_t CLR_CARD     = 0x1082; // Dark gray
static const uint16_t CLR_CARD_ALT = 0x2104; // Slightly lighter dark gray
static const uint16_t CLR_WHITE    = 0xFFFF; // White
static const uint16_t CLR_CYAN     = 0x07FF; // Bright cyan
static const uint16_t CLR_YELLOW   = 0xFFE0; // Bright yellow
static const uint16_t CLR_GREEN    = 0x07E0; // Bright green
static const uint16_t CLR_RED      = 0xF800; // Bright red
static const uint16_t CLR_GRAY     = 0xC618; // Light gray for labels
static const uint16_t CLR_ORANGE   = 0xFD20; // Orange
static const uint16_t CLR_LABEL    = 0xBDF7; // Very light gray (readable)
static const uint16_t CLR_GAUGE_OK = 0x07E0; // Green zone
static const uint16_t CLR_GAUGE_WRN = 0xFFE0; // Yellow zone
static const uint16_t CLR_GAUGE_BAD = 0xF800; // Red zone
static const uint16_t CLR_GAUGE_BG = 0xD6DA; // Light gray gauge tile background

// ---------------------------------------------------------------------------
// Wi-Fi setup with captive portal
// ---------------------------------------------------------------------------
void dashboardConnectWiFi(LGFX* disp)
{
  if (disp == nullptr)
  {
    return;
  }

  dashboardLoadConfig(g_cfg);

  disp->fillScreen(CLR_BG);
  disp->fillRect(0, 0, disp->width(), 28, CLR_HEADER);
  disp->setTextColor(CLR_WHITE, CLR_HEADER);
  disp->setTextSize(1);
  disp->setCursor(8, 10);
  disp->print("GenMon Dashboard");

  WiFiManager wm;
  wm.setConfigPortalTimeout(0);
  wm.setAPCallback([&](WiFiManager* wifiManager) {
    disp->fillScreen(CLR_BG);
    disp->fillRect(0, 0, disp->width(), 28, CLR_HEADER);
    disp->setTextColor(CLR_WHITE, CLR_HEADER);
    disp->setTextSize(1);
    disp->setCursor(8, 10);
    disp->print("GenMon Dashboard");

    disp->setTextColor(CLR_WHITE, CLR_BG);
    disp->setTextSize(2);
    disp->setCursor(20, 60);
    disp->print("Setup WiFi");

    disp->setTextSize(1);
    disp->setTextColor(CLR_CYAN, CLR_BG);
    disp->setCursor(20, 90);
    disp->print("Connect to:");
    disp->setCursor(20, 106);
    disp->print("GenMon-Setup");
    disp->setCursor(20, 122);
    disp->print("Open browser:");
    disp->setCursor(20, 138);
    disp->print("192.168.4.1");
    disp->setCursor(20, 160);
    disp->setTextColor(CLR_YELLOW, CLR_BG);
    disp->print("Enter WiFi + GenMon IP");
  });

  WiFiManagerParameter hostParam("genmon_host", "GenMon host/IP", g_cfg.genmonHost, sizeof(g_cfg.genmonHost));
  char portStr[8];
  itoa(g_cfg.genmonPort, portStr, 10);
  WiFiManagerParameter portParam("genmon_port", "GenMon port", portStr, sizeof(portStr));
  wm.addParameter(&hostParam);
  wm.addParameter(&portParam);

  bool portal = false;
  if (strlen(g_cfg.wifiSsid) == 0 || WiFi.status() != WL_CONNECTED)
  {
    WiFi.mode(WIFI_STA);
    if (strlen(g_cfg.wifiSsid) > 0)
    {
      WiFi.begin(g_cfg.wifiSsid, g_cfg.wifiPass);
      int tries = 0;
      while (WiFi.status() != WL_CONNECTED && tries < 40) // ~10 seconds
      {
        delay(250);
        ++tries;
      }
    }

    if (WiFi.status() != WL_CONNECTED)
    {
      portal = true;
    }
  }

  if (portal)
  {
    if (!wm.startConfigPortal("GenMon-Setup"))
    {
      ESP.restart();
    }

    strlcpy(g_cfg.wifiSsid, wm.getWiFiSSID().c_str(), sizeof(g_cfg.wifiSsid));
    strlcpy(g_cfg.wifiPass, wm.getWiFiPass().c_str(), sizeof(g_cfg.wifiPass));
    strlcpy(g_cfg.genmonHost, hostParam.getValue(), sizeof(g_cfg.genmonHost));
    g_cfg.genmonPort = atoi(portParam.getValue());
    if (g_cfg.genmonPort <= 0) g_cfg.genmonPort = 8000;
    saveConfig(g_cfg);

    // Connect using the new credentials.
    WiFi.begin(g_cfg.wifiSsid, g_cfg.wifiPass);
  }

  // Wait for connection.
  int dots = 0;
  disp->fillScreen(CLR_BG);
  disp->fillRect(0, 0, disp->width(), 28, CLR_HEADER);
  disp->setTextColor(CLR_WHITE, CLR_HEADER);
  disp->setTextSize(1);
  disp->setCursor(8, 10);
  disp->print("GenMon Dashboard");

  disp->setTextColor(CLR_WHITE, CLR_BG);
  disp->setTextSize(2);
  disp->setCursor(20, 60);
  disp->print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(250);
    ++dots;

    disp->fillRect(20, 100, 280, 24, CLR_BG);
    disp->setTextColor(CLR_CYAN, CLR_BG);
    disp->setTextSize(1);
    disp->setCursor(20, 106);
    disp->printf("SSID: %s", g_cfg.wifiSsid);

    disp->fillRect(20, 130, 200, 20, CLR_BG);
    disp->setCursor(20, 132);
    disp->print("Status: ");
    switch (WiFi.status())
    {
      case WL_IDLE_STATUS:      disp->print("IDLE");     break;
      case WL_SCAN_COMPLETED:   disp->print("SCANNED");  break;
      case WL_NO_SSID_AVAIL:    disp->print("NO SSID");  break;
      case WL_CONNECT_FAILED:   disp->print("FAILED");   break;
      case WL_CONNECTION_LOST:  disp->print("LOST");     break;
      case WL_DISCONNECTED:     disp->print("DISCONN");  break;
      default:                  disp->print("UNKNOWN");  break;
    }

    int bar = (dots * 10) % 220;
    disp->drawRect(20, 160, 220, 12, CLR_CYAN);
    disp->fillRect(22, 162, bar, 8, CLR_CYAN);

    if (dots > 240) // ~60 seconds
    {
      disp->setTextColor(CLR_RED, CLR_BG);
      disp->setCursor(20, 190);
      disp->print("Timeout. Restarting...");
      delay(2000);
      ESP.restart();
    }
  }

  disp->fillScreen(CLR_BG);
  disp->fillRect(0, 0, disp->width(), 28, CLR_HEADER);
  disp->setTextColor(CLR_WHITE, CLR_HEADER);
  disp->setTextSize(1);
  disp->setCursor(8, 10);
  disp->print("GenMon Dashboard");

  disp->setTextColor(CLR_GREEN, CLR_BG);
  disp->setTextSize(2);
  disp->setCursor(20, 70);
  disp->print("WiFi Connected");

  disp->setTextColor(CLR_WHITE, CLR_BG);
  disp->setTextSize(1);
  disp->setCursor(20, 110);
  disp->printf("IP: %s", WiFi.localIP().toString().c_str());
  disp->setCursor(20, 130);
  disp->printf("GenMon: %s:%d", g_cfg.genmonHost, g_cfg.genmonPort);

  configTime(TIMEZONE_OFFSET_HOURS * 3600, 0, "pool.ntp.org", "time.nist.gov");

  delay(1500);
}

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------
static String convertIsoDateToUSA(const String& text)
{
  // GenMon dates are embedded in strings like "System in outage since 2026-09-03 06:12:13".
  // Find a YYYY-MM-DD pattern and rewrite it as MM/DD/YYYY, preserving the rest.
  String out = text;
  int len = out.length();
  for (int i = 0; i <= len - 10; ++i)
  {
    if (out.charAt(i + 4) == '-' && out.charAt(i + 7) == '-')
    {
      int y = out.substring(i, i + 4).toInt();
      int m = out.substring(i + 5, i + 7).toInt();
      int d = out.substring(i + 8, i + 10).toInt();
      if (y >= 2000 && y <= 2099 && m >= 1 && m <= 12 && d >= 1 && d <= 31)
      {
        String usa;
        if (m < 10) usa += "0";
        usa += m;
        usa += "/";
        if (d < 10) usa += "0";
        usa += d;
        usa += "/";
        usa += y;
        out = out.substring(0, i) + usa + out.substring(i + 10);
        len = out.length();
      }
    }
  }
  return out;
}

static String valueToString(JsonVariant value)
{
  if (value.isNull())
  {
    return "";
  }

  if (value.is<const char*>())
  {
    String s = value.as<const char*>();
    return convertIsoDateToUSA(s);
  }
  if (value.is<int>())
  {
    return String(value.as<int>());
  }
  if (value.is<float>())
  {
    return String(value.as<float>(), 2);
  }
  if (value.is<JsonObject>())
  {
    // Numeric JSON format: {"value": 13.8, "unit": "V"}
    JsonObject obj = value.as<JsonObject>();
    String val;
    if (obj["value"].is<float>())
    {
      val = String(obj["value"].as<float>(), 2);
    }
    else if (obj["value"].is<int>())
    {
      val = String(obj["value"].as<int>());
    }
    else
    {
      val = obj["value"].as<String>();
    }
    String unit = obj["unit"] | "";
    if (unit.length() > 0)
    {
      val += " ";
      val += unit;
    }
    return val;
  }
  return "";
}

// Recursively search a JsonVariant for a key and return its value as a string.
static String findValue(JsonVariant root, const char* key)
{
  if (root.isNull())
  {
    return "";
  }

  if (root.is<JsonObject>())
  {
    JsonObject obj = root.as<JsonObject>();
    for (JsonPair kv : obj)
    {
      if (strcmp(kv.key().c_str(), key) == 0)
      {
        return valueToString(kv.value());
      }

      String result = findValue(kv.value(), key);
      if (!result.isEmpty())
      {
        return result;
      }
    }
  }
  else if (root.is<JsonArray>())
  {
    JsonArray arr = root.as<JsonArray>();
    for (JsonVariant item : arr)
    {
      String result = findValue(item, key);
      if (!result.isEmpty())
      {
        return result;
      }
    }
  }

  return "";
}

static String extractValue(JsonDocument& doc, const char* key)
{
  return findValue(doc.as<JsonVariant>(), key);
}

static String fetchEndpoint(const char* command, String& errorOut)
{
  HTTPClient http;
  http.setTimeout(8000);
  http.setConnectTimeout(8000);

  String url = "http://";
  url += g_cfg.genmonHost;
  url += ":";
  url += g_cfg.genmonPort;
  url += "/cmd/";
  url += command;

  Serial.print("[GenMon] GET ");
  Serial.println(url);

  if (strlen(GENMON_AUTH_USER) > 0)
  {
    http.setAuthorization(GENMON_AUTH_USER, GENMON_AUTH_PASS);
  }

  http.begin(url);
  int code = http.GET();
  Serial.print("[GenMon] ");
  Serial.print(command);
  Serial.print(" response code: ");
  Serial.println(code);

  if (code != HTTP_CODE_OK)
  {
    errorOut = "HTTP ";
    errorOut += code;
    errorOut += " on ";
    errorOut += command;
    http.end();
    return "";
  }

  String payload = http.getString();
  http.end();

  // Debug: print payload length and full raw content.
  Serial.print("[GenMon] ");
  Serial.print(command);
  Serial.print(" payload length: ");
  Serial.println(payload.length());
  Serial.print("[GenMon] RAW ");
  Serial.print(command);
  Serial.print(": ");
  Serial.println(payload);

  // Detect an HTML login page (GenMon auth redirect).
  if (payload.indexOf("<html") >= 0 || payload.indexOf("<!DOCTYPE") >= 0 ||
      payload.indexOf("<form") >= 0 || payload.indexOf("login") >= 0)
  {
    errorOut = "Auth/login page from ";
    errorOut += command;
    return "";
  }

  return payload;
}

// ---------------------------------------------------------------------------
// Public fetch
// ---------------------------------------------------------------------------
bool dashboardFetchData(GenMonData& data)
{
  String error;
  JsonDocument statusDoc;
  JsonDocument maintDoc;
  JsonDocument outageDoc;

  String statusPayload = fetchEndpoint("status_json", error);
  String maintPayload  = fetchEndpoint("maint_json", error);
  String outagePayload = fetchEndpoint("outage_json", error);

  bool anyGood = false;

  if (!statusPayload.isEmpty())
  {
    DeserializationError err = deserializeJson(statusDoc, statusPayload);
    if (err)
    {
      error = "status JSON error: ";
      error += err.c_str();
      Serial.print("[GenMon] status parse error: ");
      Serial.println(err.c_str());
    }
    else
    {
      anyGood = true;
      data.engineState     = extractValue(statusDoc, "Engine State");
      data.switchState     = extractValue(statusDoc, "Switch State");
      data.generatorStatus = extractValue(statusDoc, "Generator Status");
      data.batteryVoltage  = extractValue(statusDoc, "Battery Voltage");
      data.rpm             = extractValue(statusDoc, "RPM");
      data.frequency       = extractValue(statusDoc, "Frequency");
      data.outputVoltage   = extractValue(statusDoc, "Output Voltage");
      data.outputCurrent   = extractValue(statusDoc, "Output Current");
      data.outputPower     = extractValue(statusDoc, "Output Power");

      // Some controllers use different keys.
      if (data.outputPower.isEmpty())
      {
        data.outputPower = extractValue(statusDoc, "Output Power (Single Phase)");
      }
      if (data.outputVoltage.isEmpty())
      {
        data.outputVoltage = extractValue(statusDoc, "Average Voltage");
      }
      if (data.outputCurrent.isEmpty())
      {
        data.outputCurrent = extractValue(statusDoc, "Average Current");
      }

      Serial.print("[GenMon] Engine State: ");
      Serial.println(data.engineState);
      Serial.print("[GenMon] Switch State: ");
      Serial.println(data.switchState);
      Serial.print("[GenMon] Battery Voltage: ");
      Serial.println(data.batteryVoltage);
      Serial.print("[GenMon] Output Voltage: ");
      Serial.println(data.outputVoltage);
    }
  }

  if (!maintPayload.isEmpty())
  {
    DeserializationError err = deserializeJson(maintDoc, maintPayload);
    if (err)
    {
      Serial.print("[GenMon] maint parse error: ");
      Serial.println(err.c_str());
    }
    if (!err)
    {
      anyGood = true;
      data.runHours    = extractValue(maintDoc, "Total Run Hours");
      data.serviceDue  = extractValue(maintDoc, "Hours till next service");

      // If no numeric service hours, fall back to next due string.
      if (data.serviceDue.isEmpty())
      {
        data.serviceDue = extractValue(maintDoc, "Service A Due");
      }

      // Prefer gallons remaining for liquid/propane fuel displays.
      data.fuelLevel = extractValue(maintDoc, "Estimated Fuel In Tank ");
      if (data.fuelLevel.isEmpty())
      {
        data.fuelLevel = extractValue(maintDoc, "Fuel In Tank (Sensor)");
      }
      if (data.fuelLevel.isEmpty())
      {
        data.fuelLevel = extractValue(maintDoc, "Fuel Level State");
      }
      if (data.fuelLevel.isEmpty())
      {
        data.fuelLevel = extractValue(maintDoc, "Fuel Level Sensor");
      }

      Serial.print("[GenMon] Run Hours: ");
      Serial.println(data.runHours);
      Serial.print("[GenMon] Fuel Level: ");
      Serial.println(data.fuelLevel);
    }
  }

  if (!outagePayload.isEmpty())
  {
    DeserializationError err = deserializeJson(outageDoc, outagePayload);
    if (err)
    {
      Serial.print("[GenMon] outage parse error: ");
      Serial.println(err.c_str());
    }
    if (!err)
    {
      anyGood = true;
      data.utilityVoltage = extractValue(outageDoc, "Utility Voltage");
      data.outageStatus   = extractValue(outageDoc, "Status");

      if (data.outageStatus.isEmpty())
      {
        data.outageStatus = extractValue(outageDoc, "System In Outage");
      }

      Serial.print("[GenMon] Utility Voltage: ");
      Serial.println(data.utilityVoltage);
      Serial.print("[GenMon] Outage Status: ");
      Serial.println(data.outageStatus);
    }
  }

  if (!anyGood)
  {
    data.online = false;
    if (error.isEmpty())
    {
      data.error = "No response from GenMon";
    }
    else
    {
      data.error = error;
    }
    Serial.print("[GenMon] Fetch failed: ");
    Serial.println(data.error);
    return false;
  }

  data.online = true;
  data.error = "";

  time_t now = time(nullptr);
  if (now > 100000)
  {
    struct tm* ti = localtime(&now);
    char buf[32];
    // USA format: MM/DD/YYYY HH:MM:SS
    strftime(buf, sizeof(buf), "%m/%d/%Y %H:%M:%S", ti);
    data.lastUpdate = buf;
  }
  else
  {
    data.lastUpdate = String(millis() / 1000) + "s";
  }

  return true;
}

// ---------------------------------------------------------------------------
// Rendering helpers
// ---------------------------------------------------------------------------
static uint16_t stateColor(const String& state)
{
  String s = state;
  s.toLowerCase();

  if (s.indexOf("run") >= 0 || s.indexOf("exercis") >= 0)
  {
    return CLR_GREEN;
  }
  if (s.indexOf("alarm") >= 0 || s.indexOf("fault") >= 0 || s.indexOf("error") >= 0)
  {
    return CLR_RED;
  }
  if (s.indexOf("start") >= 0 || s.indexOf("cool") >= 0)
  {
    return CLR_ORANGE;
  }
  if (s.indexOf("ready") >= 0 || s.indexOf("off") >= 0 || s.indexOf("auto") >= 0)
  {
    return CLR_CYAN;
  }
  return CLR_WHITE;
}

static void drawCard(LGFX* disp, int x, int y, int w, int h, uint16_t color)
{
  disp->fillRoundRect(x, y, w, h, 4, color);
  disp->drawRoundRect(x, y, w, h, 4, CLR_GRAY);
}

static void drawWrappedText(LGFX* disp, int x, int y, int maxW,
                            const String& text, uint16_t fgColor,
                            uint16_t bgColor)
{
  disp->setTextColor(fgColor, bgColor);
  disp->setTextSize(1);

  int maxChars = maxW / 6;
  if (maxChars <= 0)
  {
    return;
  }

  int start = 0;
  int lineH = 10;
  while (start < static_cast<int>(text.length()))
  {
    int chunk = text.length() - start;
    if (chunk > maxChars)
    {
      chunk = maxChars;
    }
    disp->setCursor(x, y);
    disp->print(text.substring(start, start + chunk));
    start += chunk;
    y += lineH;
  }
}

static void drawHeader(LGFX* disp, const GenMonData& data)
{
  disp->fillRect(0, 0, disp->width(), 28, CLR_HEADER);
  disp->setTextColor(CLR_WHITE, CLR_HEADER);
  disp->setTextSize(1);
  disp->setCursor(8, 10);
  disp->print("GenMon Dashboard");

  disp->setCursor(disp->width() - 80, 10);
  if (data.online)
  {
    disp->setTextColor(CLR_GREEN, CLR_HEADER);
    disp->print("ONLINE");
  }
  else
  {
    disp->setTextColor(CLR_RED, CLR_HEADER);
    disp->print("OFFLINE");
  }
}

// Clamp text to a pixel width using the actual font width.
static void printClipped(lgfx::LGFX_Device* disp, int x, int y, int maxW,
                         const String& text, uint16_t fgColor, uint16_t bgColor)
{
  if (text.isEmpty())
  {
    return;
  }

  disp->setTextColor(fgColor, bgColor);
  disp->setTextSize(1);

  int chars = text.length();
  while (chars > 0 && disp->textWidth(text.substring(0, chars).c_str()) > maxW)
  {
    --chars;
  }

  disp->setCursor(x, y);
  disp->print(text.substring(0, chars));
}

static void drawLabelValue(LGFX* disp, int x, int y, int labelW, int valueW,
                           const char* label, const String& value,
                           uint16_t valueColor, uint16_t bgColor)
{
  disp->setTextColor(CLR_LABEL, bgColor);
  disp->setTextSize(1);
  disp->setCursor(x, y);
  disp->print(label);

  printClipped(disp, x + labelW + 4, y, valueW, value.isEmpty() ? "--" : value,
               valueColor, bgColor);
}
static float parseNumericValue(const String& text)
{
  if (text.isEmpty())
  {
    return 0.0f;
  }

  String s = text;
  s.trim();
  int len = s.length();
  int numEnd = 0;
  while (numEnd < len &&
         (isdigit(s.charAt(numEnd)) || s.charAt(numEnd) == '.' ||
          s.charAt(numEnd) == '-' || s.charAt(numEnd) == '+'))
  {
    ++numEnd;
  }

  if (numEnd == 0)
  {
    return 0.0f;
  }

  return s.substring(0, numEnd).toFloat();
}

// Helper: value-to-angle for a 180-degree arc (180=left, 360=right)
static float valueToAngle(float v, float minVal, float maxVal)
{
  float pct = 0.0f;
  if (maxVal > minVal)
  {
    pct = (v - minVal) / (maxVal - minVal);
  }
  if (pct < 0.0f) pct = 0.0f;
  if (pct > 1.0f) pct = 1.0f;
  return 180.0f + (pct * 180.0f);
}

// Draw a gauge tile with the same dark card style as all others.
static void drawGenericGaugeTile(LGFX* disp, int x, int y, int w, int h,
                                 const char* label, const String& value,
                                 float minVal, float maxVal,
                                 const float* labels, int labelCount,
                                 const float* zoneLimits, const uint16_t* zoneColors, int zoneCount,
                                 const char* unit)
{
  uint16_t bg = CLR_CARD;
  disp->fillRoundRect(x, y, w, h, 6, bg);
  disp->drawRoundRect(x, y, w, h, 6, CLR_GRAY);

  // Label
  disp->setTextColor(CLR_LABEL, bg);
  disp->setTextSize(1);
  int labelW = disp->textWidth(label);
  disp->setCursor(x + (w - labelW) / 2, y + 4);
  disp->print(label);

  // Gauge geometry: thin ring so it doesn't overlap the title
  int cx = x + w / 2;
  int cy = y + h / 2 + 10;
  int rOut = (w < h ? w : h) / 2 - 16;
  if (rOut < 22) rOut = 22;
  int rIn = rOut - 6;

  // Colored zones
  float prevAngle = 180.0f;
  for (int i = 0; i < zoneCount; ++i)
  {
    float endAngle = valueToAngle(zoneLimits[i], minVal, maxVal);
    if (endAngle > prevAngle)
    {
      disp->fillArc(cx, cy, rOut, rIn, prevAngle, endAngle - prevAngle, zoneColors[i]);
    }
    prevAngle = endAngle;
  }

  // Tick marks and number labels
  int labelRadius = rOut + 8;
  disp->setTextColor(CLR_WHITE, bg);
  disp->setTextSize(1);
  for (int i = 0; i < labelCount; ++i)
  {
    float v = labels[i];
    float rad = valueToAngle(v, minVal, maxVal) * DEG_TO_RAD;

    int x1 = cx + rOut * cos(rad);
    int y1 = cy + rOut * sin(rad);
    int x2 = cx + (rOut - 6) * cos(rad);
    int y2 = cy + (rOut - 6) * sin(rad);
    disp->drawLine(x1, y1, x2, y2, CLR_WHITE);

    String s;
    if (v == (int)v)
    {
      s = String((int)v);
    }
    else
    {
      s = String(v, 1);
    }
    int tw = disp->textWidth(s.c_str());
    int tx = cx + labelRadius * cos(rad) - tw / 2;
    int ty = cy + labelRadius * sin(rad) - 4;
    disp->setCursor(tx, ty);
    disp->print(s);
  }

  // Needle
  float numeric = parseNumericValue(value);
  float needleAngle = valueToAngle(numeric, minVal, maxVal) * DEG_TO_RAD;
  int needleLen = rOut - 4;
  int nx = cx + needleLen * cos(needleAngle);
  int ny = cy + needleLen * sin(needleAngle);

  // White needle with uniform black outline (perpendicular offsets so size
  // stays the same regardless of needle angle)
  float dx = nx - cx;
  float dy = ny - cy;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 1.0f) len = 1.0f;
  float px = -dy / len;
  float py =  dx / len;

  auto nline = [&](float o, uint16_t c) {
    disp->drawLine(cx + px * o, cy + py * o, nx + px * o, ny + py * o, c);
  };

  nline(-2.0f, CLR_BLACK);
  nline( 2.0f, CLR_BLACK);
  nline(-1.0f, CLR_WHITE);
  nline( 1.0f, CLR_WHITE);
  nline( 0.0f, CLR_WHITE);

  // Pivot
  disp->fillCircle(cx, cy, 5, CLR_BLACK);
  disp->drawCircle(cx, cy, 5, CLR_WHITE);
  disp->fillCircle(cx, cy, 3, CLR_WHITE);

  // Value
  disp->setTextColor(CLR_WHITE, bg);
  disp->setTextSize(2);
  String val = value.isEmpty() ? "--" : value;
  if (unit != nullptr && strlen(unit) > 0 && !value.isEmpty())
  {
    // Keep the numeric part from GenMon and append the unit for consistent look.
    val = String(numeric, (maxVal < 100.0f ? 1 : 0));
    val += " ";
    val += unit;
  }
  int valW = disp->textWidth(val.c_str());
  disp->setCursor(x + (w - valW) / 2, y + h - 20);
  disp->print(val);
}

static void drawBatteryGaugeTile(LGFX* disp, int x, int y, int w, int h, const String& value)
{
  const float labels[] = {0.0f, 4.0f, 8.0f, 12.0f, 16.0f};
  const float zones[] = {11.0f, 12.0f, 14.5f, 16.0f};
  const uint16_t colors[] = {CLR_GAUGE_BAD, CLR_GAUGE_WRN, CLR_GAUGE_OK, CLR_GAUGE_BAD};
  drawGenericGaugeTile(disp, x, y, w, h, "Battery V", value, 0.0f, 16.0f,
                       labels, 5, zones, colors, 4, "V");
}

static void drawUtilityGaugeTile(LGFX* disp, int x, int y, int w, int h, const String& value)
{
  const float labels[] = {0.0f, 50.0f, 95.0f, 145.0f, 190.0f, 240.0f, 280.0f};
  const float zones[] = {190.0f, 220.0f, 250.0f, 280.0f};
  const uint16_t colors[] = {CLR_GAUGE_BAD, CLR_GAUGE_WRN, CLR_GAUGE_OK, CLR_GAUGE_BAD};
  drawGenericGaugeTile(disp, x, y, w, h, "Utility V", value, 0.0f, 280.0f,
                       labels, 7, zones, colors, 4, "V");
}

static void drawOutputVoltageGaugeTile(LGFX* disp, int x, int y, int w, int h, const String& value)
{
  const float labels[] = {0.0f, 50.0f, 95.0f, 145.0f, 190.0f, 240.0f, 280.0f};
  const float zones[] = {190.0f, 220.0f, 250.0f, 280.0f};
  const uint16_t colors[] = {CLR_GAUGE_BAD, CLR_GAUGE_WRN, CLR_GAUGE_OK, CLR_GAUGE_BAD};
  drawGenericGaugeTile(disp, x, y, w, h, "Output V", value, 0.0f, 280.0f,
                       labels, 7, zones, colors, 4, "V");
}

static void drawFrequencyGaugeTile(LGFX* disp, int x, int y, int w, int h, const String& value)
{
  const float labels[] = {0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f};
  const float zones[] = {55.0f, 58.0f, 62.0f, 65.0f, 70.0f};
  const uint16_t colors[] = {CLR_GAUGE_BAD, CLR_GAUGE_WRN, CLR_GAUGE_OK, CLR_GAUGE_WRN, CLR_GAUGE_BAD};
  drawGenericGaugeTile(disp, x, y, w, h, "Freq Hz", value, 0.0f, 70.0f,
                       labels, 8, zones, colors, 5, "Hz");
}

static void drawRpmGaugeTile(LGFX* disp, int x, int y, int w, int h, const String& value)
{
  const float labels[] = {0.0f, 900.0f, 1800.0f, 2700.0f, 3600.0f};
  const float zones[] = {2800.0f, 3300.0f, 3500.0f, 3600.0f};
  const uint16_t colors[] = {CLR_GAUGE_BAD, CLR_GAUGE_WRN, CLR_GAUGE_OK, CLR_GAUGE_BAD};
  drawGenericGaugeTile(disp, x, y, w, h, "RPM", value, 0.0f, 3600.0f,
                       labels, 5, zones, colors, 4, "");
}

static void drawCurrentGaugeTile(LGFX* disp, int x, int y, int w, int h, const String& value)
{
  const float labels[] = {0.0f, 15.0f, 35.0f, 50.0f, 65.0f, 85.0f, 99.0f};
  const float zones[] = {50.0f, 65.0f, 85.0f, 99.0f};
  const uint16_t colors[] = {CLR_GAUGE_OK, CLR_GAUGE_WRN, CLR_GAUGE_BAD, CLR_GAUGE_BAD};
  drawGenericGaugeTile(disp, x, y, w, h, "Current A", value, 0.0f, 99.0f,
                       labels, 7, zones, colors, 4, "A");
}

static void drawPowerGaugeTile(LGFX* disp, int x, int y, int w, int h, const String& value)
{
  const float labels[] = {0.0f, 5.0f, 10.0f, 15.0f, 20.0f};
  const float zones[] = {15.0f, 17.0f, 20.0f};
  const uint16_t colors[] = {CLR_GAUGE_OK, CLR_GAUGE_WRN, CLR_GAUGE_BAD};
  drawGenericGaugeTile(disp, x, y, w, h, "Power kW", value, 0.0f, 20.0f,
                       labels, 5, zones, colors, 3, "kW");
}

static void drawIconTile(LGFX* disp, int x, int y, int w, int h,
                         const char* label, const String& value,
                         uint16_t iconColor)
{
  uint16_t bg = CLR_CARD_ALT;
  disp->fillRoundRect(x, y, w, h, 6, bg);
  disp->drawRoundRect(x, y, w, h, 6, CLR_GRAY);

  // Label
  disp->setTextColor(CLR_LABEL, bg);
  disp->setTextSize(1);
  int labelW = strlen(label) * 6;
  disp->setCursor(x + (w - labelW) / 2, y + 4);
  disp->print(label);

  // Simple icon box
  int cx = x + w / 2;
  int cy = y + h / 2 + 2;
  int s = (w < h ? w : h) / 2 - 12;
  disp->fillRect(cx - s/2, cy - s/2, s, s, iconColor);
  disp->drawRect(cx - s/2 - 1, cy - s/2 - 1, s + 2, s + 2, CLR_WHITE);

  // Value
  disp->setTextColor(CLR_WHITE, bg);
  disp->setTextSize(1);
  String val = value.isEmpty() ? "--" : value;
  int valW = val.length() * 6;
  disp->setCursor(x + (w - valW) / 2, y + h - 12);
  disp->print(val);
}
// Word-wrap long strings into up to maxLines, each clipped to maxW.
static void printWrapped(lgfx::LGFX_Device* disp, int x, int y, int maxW,
                         int lineH, int maxLines, const String& text,
                         uint16_t fgColor, uint16_t bgColor)
{
  if (text.isEmpty())
  {
    return;
  }

  disp->setTextColor(fgColor, bgColor);
  disp->setTextSize(1);

  String remaining = text;
  for (int line = 0; line < maxLines && remaining.length() > 0; ++line)
  {
    int chars = remaining.length();
    while (chars > 0 && disp->textWidth(remaining.substring(0, chars).c_str()) > maxW)
    {
      --chars;
    }

    // Try to break at a space for nicer wrapping.
    if (chars < remaining.length())
    {
      int spaceIdx = remaining.lastIndexOf(' ', chars);
      if (spaceIdx > 0)
      {
        chars = spaceIdx;
      }
    }

    // Fill the line background before printing to avoid leftover pixels.
    disp->fillRect(x, y + line * lineH, maxW, lineH, bgColor);
    disp->setCursor(x, y + line * lineH);
    disp->print(remaining.substring(0, chars));

    // Advance past the printed chunk and any trailing space.
    while (chars < remaining.length() && remaining.charAt(chars) == ' ')
    {
      ++chars;
    }
    remaining = remaining.substring(chars);
  }
}

static bool s_blinkState = false;
static unsigned long s_lastBlinkMs = 0;
static const unsigned long BLINK_PERIOD_MS = 1000;

static bool isGeneratorRunning(const String& state)
{
  String s = state;
  s.toLowerCase();
  return s.indexOf("run") >= 0 || s.indexOf("exercis") >= 0;
}

static bool isInOutage(const String& status)
{
  String s = status;
  s.toLowerCase();
  return s.indexOf("yes") >= 0 || s.indexOf("true") >= 0 ||
         s.indexOf("outage") >= 0 || s.indexOf("active") >= 0;
}

// ---------------------------------------------------------------------------
// Public render
// ---------------------------------------------------------------------------
void dashboardRender(LGFX* disp, const GenMonData& data)
{
  dashboardRenderPage(disp, data, 0);
}

void dashboardRenderPage(LGFX* disp, const GenMonData& data, uint8_t page)
{
  if (disp == nullptr)
  {
    return;
  }

  unsigned long nowMs = millis();
  static bool prevBlink = false;
  if (nowMs - s_lastBlinkMs >= BLINK_PERIOD_MS)
  {
    s_lastBlinkMs = nowMs;
    s_blinkState = !s_blinkState;
  }

  disp->fillScreen(CLR_BG);
  drawHeader(disp, data);

  if (page == 1)
  {
    // Page 1: gauge / icon tiles
    drawBatteryGaugeTile(disp, 4, 32, 100, 96, data.batteryVoltage);
    drawUtilityGaugeTile(disp, 110, 32, 100, 96, data.utilityVoltage);
    drawOutputVoltageGaugeTile(disp, 216, 32, 100, 96, data.outputVoltage);

    drawFrequencyGaugeTile(disp, 4, 134, 100, 96, data.frequency);
    drawRpmGaugeTile(disp, 110, 134, 100, 96, data.rpm);
    drawPowerGaugeTile(disp, 216, 134, 100, 96, data.outputPower);

    // Page indicator
    disp->fillRect(150, 228, 8, 4, CLR_CYAN);
    disp->drawRect(164, 228, 8, 4, CLR_GRAY);
  }
  else
  {
    // Page 0: summary text
    drawCard(disp, 4, 32, 152, 84, CLR_CARD);
    disp->setTextColor(CLR_LABEL, CLR_CARD);
    disp->setTextSize(1);
    disp->setCursor(12, 40);
    disp->print("ENGINE STATE");

    bool runningInOutage = isGeneratorRunning(data.engineState) && isInOutage(data.outageStatus);
    uint16_t stateCol = stateColor(data.engineState);

    // Show engine state. If running during an outage, invert colors every second.
    if (runningInOutage && s_blinkState)
    {
      disp->fillRect(10, 54, 134, 22, stateCol);
      disp->setTextColor(CLR_BLACK, stateCol);
    }
    else
    {
      disp->setTextColor(stateCol, CLR_CARD);
    }
    disp->setTextSize(2);
    disp->setCursor(12, 56);
    disp->print(data.engineState.isEmpty() ? "--" : data.engineState.c_str());

    drawLabelValue(disp, 12, 80, 44, 80, "Switch:", data.switchState, CLR_WHITE, CLR_CARD);
    drawLabelValue(disp, 12, 96, 44, 80, "Battery:", data.batteryVoltage, CLR_YELLOW, CLR_CARD);

    // Power card (right)
    drawCard(disp, 164, 32, 152, 84, CLR_CARD_ALT);
    disp->setTextColor(CLR_LABEL, CLR_CARD_ALT);
    disp->setTextSize(1);
    disp->setCursor(172, 40);
    disp->print("OUTPUT");

    drawLabelValue(disp, 177, 56, 52, 67, "Voltage:", data.outputVoltage, CLR_WHITE, CLR_CARD_ALT);
    drawLabelValue(disp, 177, 72, 52, 67, "Current:", data.outputCurrent, CLR_WHITE, CLR_CARD_ALT);
    drawLabelValue(disp, 177, 88, 52, 67, "Power:",   data.outputPower,   CLR_GREEN, CLR_CARD_ALT);

    // Maintenance / outage card (full width)
    drawCard(disp, 4, 122, 312, 96, CLR_CARD);
    disp->setTextColor(CLR_LABEL, CLR_CARD);
    disp->setTextSize(1);
    disp->setCursor(12, 130);
    disp->print("MAINTENANCE & OUTAGE");

    // Top row: three compact columns with consistent left alignment
    drawLabelValue(disp, 12, 148, 28, 70, "Run:",      data.runHours,      CLR_CYAN,  CLR_CARD);
    drawLabelValue(disp, 112, 148, 44, 54, "Utility:",  data.utilityVoltage, CLR_YELLOW, CLR_CARD);
    drawLabelValue(disp, 212, 148, 28, 70, "Fuel:",     data.fuelLevel,     CLR_GREEN,  CLR_CARD);

    // Service and outage get full-width rows so long text stays inside the card.
    disp->setTextColor(CLR_LABEL, CLR_CARD);
    disp->setTextSize(1);
    disp->setCursor(12, 166);
    disp->print("Service:");
    printClipped(disp, 63, 166, 241, data.serviceDue, CLR_WHITE, CLR_CARD);

    disp->setCursor(12, 184);
    disp->print("Outage:");
    printWrapped(disp, 63, 184, 241, 9, 2, data.outageStatus,
                 stateColor(data.outageStatus), CLR_CARD);

    // Page indicator
    disp->drawRect(150, 228, 8, 4, CLR_GRAY);
    disp->fillRect(164, 228, 8, 4, CLR_CYAN);
  }

  // Footer
  disp->setTextColor(CLR_LABEL, CLR_BG);
  disp->setTextSize(1);
  disp->setCursor(8, 228);
  if (data.error.length() > 0)
  {
    printClipped(disp, 8, 228, 130, data.error, CLR_RED, CLR_BG);
  }
  else
  {
    String footer = "Upd: ";
    footer += data.lastUpdate;
    printClipped(disp, 8, 228, 130, footer, CLR_LABEL, CLR_BG);
  }
}
