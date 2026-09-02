#include <Arduino.h>
#include <Preferences.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "config.h"
#include <math.h>

// CYD28 Display configuration - ILI9341 with SPI
class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ILI9341  _panel_instance;
  lgfx::Bus_SPI        _bus_instance;
  lgfx::Light_PWM      _light_instance;
  lgfx::Touch_XPT2046  _touch_instance;

public:
  LGFX(void)
  {
    {   // SPI bus - display (HSPI)
      auto cfg = _bus_instance.config();
      cfg.spi_host    = HSPI_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 55000000;
      cfg.freq_read   = 20000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = 1;

      cfg.pin_sclk    = 14;
      cfg.pin_mosi    = 13;
      cfg.pin_miso    = 12;
      cfg.pin_dc      = 2;

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {   // ILI9341 panel
      auto cfg = _panel_instance.config();

      cfg.pin_cs           = 15;
      cfg.pin_rst          = -1;
      cfg.pin_busy         = -1;

      cfg.memory_width     = 320;
      cfg.memory_height    = 240;
      cfg.panel_width      = 320;
      cfg.panel_height     = 240;

      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 7;

      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = true;
      cfg.invert           = false;
      cfg.rgb_order        = true;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;

      _panel_instance.config(cfg);
    }

    {   // Backlight PWM
      auto cfg = _light_instance.config();
      cfg.pin_bl      = 21;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    {   // Touch - XPT2046
      auto cfg = _touch_instance.config();

      cfg.x_min           = 300;
      cfg.x_max           = 3900;
      cfg.y_min           = 200;
      cfg.y_max           = 3700;

      cfg.pin_int         = 36;
      cfg.bus_shared      = true;
      cfg.offset_rotation = 3;

      cfg.spi_host        = -1;
      cfg.freq            = 2500000;

      cfg.pin_sclk        = 25;
      cfg.pin_mosi        = 32;
      cfg.pin_miso        = 39;
      cfg.pin_cs          = 33;

      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};

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

  // Use PWM/LEDC on GPIO26. The I2S DAC path is silent on this board,
  // but PWM works reliably for UI beeps and alerts.
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
// Non-blocking: uses delay() internally so the tone completes.
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

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("CYD template starting...");

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

  display.fillScreen(COLOR_BG);
  display.fillRect(0, 0, display.width(), 30, COLOR_HEADER);
  display.setTextColor(COLOR_WHITE, COLOR_HEADER);
  display.setTextSize(1);
  display.setCursor(10, 10);
  display.print("CYD Template");

  display.setTextColor(COLOR_WHITE, COLOR_BG);
  display.setTextSize(2);
  display.setCursor(20, 70);
  display.print("CYD Ready");

  display.setTextSize(1);
  display.setCursor(20, 110);
  display.print("Touch the screen to test");

  // Initialize the built-in speaker (PWM/LEDC on GPIO26) and play test tones.
  speakerReady = initSpeaker();
  if (speakerReady)
  {
    display.setCursor(20, 130);
    display.setTextColor(COLOR_WHITE, COLOR_BG);
    display.print("Speaker test...");

    playTone(880, 500);
    delay(200);
    playTone(1175, 500);

    display.fillRect(20, 130, 240, 20, COLOR_BG);
    display.setCursor(20, 130);
    display.print("Speaker OK  (touch to beep)");
    Serial.println("Speaker test complete");
  }
  else
  {
    display.setCursor(20, 130);
    display.setTextColor(COLOR_WHITE, COLOR_BG);
    display.print("Speaker init failed");
    Serial.println("Speaker init failed");
  }

  Serial.println("CYD template initialized");
}

void loop()
{
  uint16_t touchX = 0;
  uint16_t touchY = 0;

  if (display.getTouch(&touchX, &touchY))
  {
    Serial.printf("Touch: X=%d Y=%d\n", touchX, touchY);

    display.fillRect(20, 140, 280, 40, COLOR_BG);
    display.setTextColor(COLOR_CYAN, COLOR_BG);
    display.setCursor(20, 150);
    display.printf("X=%d  Y=%d", touchX, touchY);

    // Let you test the speaker repeatedly by touching the screen.
    if (speakerReady)
    {
      playTone(1000, 150);
    }

    delay(100);
  }

  delay(50);
}
