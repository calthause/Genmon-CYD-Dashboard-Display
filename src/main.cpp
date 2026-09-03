#include <Arduino.h>
#include <Preferences.h>
#include "lgfx_cyd.h"
#include "config.h"
#include "genmon_dashboard.h"
#include <math.h>

// Expose the runtime config so main.cpp can reference it if needed.
extern const DashboardConfig& dashboardGetConfig();

static LGFX display;

const uint16_t COLOR_BG     = 0x080F;
const uint16_t COLOR_HEADER = 0x1D4E;
const uint16_t COLOR_CARD   = 0x1D1D;
const uint16_t COLOR_WHITE  = 0xFFFF;
const uint16_t COLOR_CYAN   = 0x07FF;

const char* TOUCH_PREFERENCES_NAMESPACE = "touch";
const char* TOUCH_CALIBRATION_KEY = "calibration";

// ---------- Audio (CYD built-in speaker amp on GPIO26) ----------
static bool speakerReady = false;

void releaseSpeakerPin()
{
  ledcDetachPin(CYD_SPEAKER_PIN);
}

bool initSpeaker()
{
  releaseSpeakerPin();

  // Use PWM/LEDC on GPIO26. Reliable on CYD boards where I2S/DAC is silent.
  pinMode(CYD_SPEAKER_PIN, OUTPUT);
  ledcSetup(CYD_SPEAKER_PWM_CHANNEL, 1000, 8);
  ledcAttachPin(CYD_SPEAKER_PIN, CYD_SPEAKER_PWM_CHANNEL);
  ledcWrite(CYD_SPEAKER_PWM_CHANNEL, 0);

  return true;
}

void deinitSpeaker()
{
  ledcWrite(CYD_SPEAKER_PWM_CHANNEL, 0);
  ledcDetachPin(CYD_SPEAKER_PIN);
  speakerReady = false;
}

// Play a simple PWM tone on GPIO26 using LEDC.
// Uses delay() internally so the tone completes before returning.
void playTone(uint16_t frequencyHz, uint16_t durationMs)
{
  if (frequencyHz == 0 || durationMs == 0 || !speakerReady)
  {
    return;
  }

  ledcWriteTone(CYD_SPEAKER_PWM_CHANNEL, frequencyHz);
  delay(durationMs);
  ledcWriteTone(CYD_SPEAKER_PWM_CHANNEL, 0);
}

bool loadTouchCalibration()
{
  uint16_t calibration[8];
  Preferences preferences;
  preferences.begin(TOUCH_PREFERENCES_NAMESPACE, true);
  const size_t calibrationSize = preferences.getBytes(TOUCH_CALIBRATION_KEY,
                                                      calibration, sizeof(calibration));
  preferences.end();
  if (calibrationSize != sizeof(calibration))
  {
    return false;
  }

  display.setTouchCalibrate(calibration);
  return true;
}

bool shouldCalibrateTouch()
{
  display.fillScreen(COLOR_BG);
  display.setTextColor(COLOR_WHITE, COLOR_BG);
  display.setTextSize(1);
  display.setCursor(28, 84);
  display.print("Hold screen to calibrate");
  display.setCursor(28, 102);
  display.print("or wait for dashboard");

  const unsigned long promptStart = millis();
  while (millis() - promptStart < 3000)
  {
    uint16_t rawX = 0;
    uint16_t rawY = 0;
    if (display.getTouch(&rawX, &rawY))
    {
      const unsigned long holdStart = millis();
      while (millis() - holdStart < 1200)
      {
        if (!display.getTouch(&rawX, &rawY))
        {
          return false;
        }
        delay(25);
      }
      return true;
    }
    delay(25);
  }
  return false;
}

void calibrateTouchAtBoot()
{
  display.fillScreen(COLOR_BG);
  display.setTextColor(COLOR_WHITE, COLOR_BG);
  display.setTextSize(2);
  display.setCursor(28, 70);
  display.print("Touch calibration");
  display.setTextSize(1);
  display.setCursor(28, 105);
  display.print("Touch each target as it appears");

  uint16_t calibration[8] = {};
  display.calibrateTouch(calibration, COLOR_CYAN, COLOR_BG, 10);

  Preferences preferences;
  preferences.begin(TOUCH_PREFERENCES_NAMESPACE, false);
  preferences.putBytes(TOUCH_CALIBRATION_KEY, calibration, sizeof(calibration));
  preferences.end();
  display.setTouchCalibrate(calibration);
  display.fillScreen(COLOR_BG);
}

static GenMonData genmonData;
static unsigned long lastPollMs = 0;
static unsigned long lastPageSwitchMs = 0;
static uint8_t currentPage = 0;
static const unsigned long PAGE_SWITCH_MS = 8000;

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("CYD GenMon dashboard starting...");

  display.init();
  display.setColorDepth(16);
  display.setRotation(1);

  if (shouldCalibrateTouch())
  {
    calibrateTouchAtBoot();
  }
  else
  {
    loadTouchCalibration();
  }

  // Brief speaker chirp to confirm audio hardware.
  speakerReady = initSpeaker();
  if (speakerReady)
  {
    playTone(880, 150);
    Serial.println("Speaker OK");
  }
  else
  {
    Serial.println("Speaker init failed");
  }

  // Connect to Wi-Fi and show progress on the display.
  dashboardConnectWiFi(&display);

  // Initial GenMon fetch and render.
  dashboardFetchData(genmonData);
  dashboardRenderPage(&display, genmonData, currentPage);

  Serial.println("CYD GenMon dashboard initialized");
}

void loop()
{
  unsigned long now = millis();
  bool forceRefresh = false;
  bool switchPage = false;

  // Touch anywhere to force an immediate refresh and page switch.
  uint16_t touchX = 0;
  uint16_t touchY = 0;
  if (display.getTouch(&touchX, &touchY))
  {
    Serial.printf("Touch: X=%d Y=%d\n", touchX, touchY);
    forceRefresh = true;
    switchPage = true;

    if (speakerReady)
    {
      playTone(1000, 80);
    }

    // Simple debounce.
    delay(150);
  }

  if (forceRefresh || (now - lastPollMs >= GENMON_POLL_MS))
  {
    lastPollMs = now;

    Serial.println("Polling GenMon...");
    bool ok = dashboardFetchData(genmonData);
    if (ok)
    {
      Serial.println("GenMon poll OK");
    }
    else
    {
      Serial.print("GenMon poll failed: ");
      Serial.println(genmonData.error);
    }
  }

  if (switchPage || (now - lastPageSwitchMs >= PAGE_SWITCH_MS))
  {
    lastPageSwitchMs = now;
    if (switchPage || currentPage == 0)
    {
      currentPage = (currentPage + 1) % 2;
    }
    else
    {
      currentPage = 0;
    }
    dashboardRenderPage(&display, genmonData, currentPage);
  }

  delay(50);
}
