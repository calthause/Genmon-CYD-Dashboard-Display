# CYD ESP32 GenMon Dashboard

A PlatformIO project for the **Cheap Yellow Display (CYD)** ESP32 board that displays a live generator dashboard from a [GenMon](https://github.com/jgyates/genmon) server.

https://youtu.be/iO4U5OVR2iI?si=pvoUVCivC8r910wh

Features:

- ILI9341 240x320 display via SPI
- XPT2046 resistive touch controller
- PWM backlight control
- Built-in speaker using PWM/LEDC on GPIO26
- Touch calibration saved to NVS (Preferences)
- Connects to Wi-Fi and polls GenMon REST API
- Displays engine state, switch state, battery voltage, output voltage/current/power, utility voltage, run hours, fuel level, outage status, and service info
- PlatformIO + Arduino-ESP32 framework ready

## Hardware

Tested on the common **ESP32-2432S028R** "Cheap Yellow Display" variant. The pin mappings in `src/main.cpp` match the CYD's wiring:

| Function | Pin(s) |
|---|---|
| TFT SPI | SCK=14, MOSI=13, MISO=12, DC=2, CS=15 |
| TFT Backlight PWM | 21 |
| Touch XPT2046 | SCK=25, MOSI=32, MISO=39, CS=33, IRQ=36 |
| Built-in speaker amp | GPIO26 |

## Folder contents

```
.
├── CMakeLists.txt          # ESP-IDF project wrapper
├── platformio.ini          # PlatformIO environment & libraries
├── sdkconfig.defaults      # Required ESP-IDF defaults
├── include/
│   └── config.h            # Project config placeholders
└── src/
    ├── CMakeLists.txt      # IDF source glob
    └── main.cpp            # CYD init + touch/speaker demo
```

## Configure

Edit `include/config.h` with your Wi-Fi credentials and GenMon server details:

```cpp
#define WIFI_SSID       "YOUR_WIFI_NAME"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

#define GENMON_HOST     "192.168.1.100"   // IP of your GenMon Raspberry Pi
#define GENMON_PORT     8000              // GenMon default web port
#define GENMON_POLL_MS  5000              // Refresh interval
```

If your GenMon web UI has HTTP basic authentication enabled, also set:

```cpp
#define GENMON_AUTH_USER "admin"
#define GENMON_AUTH_PASS "your_password"
```

## Build & Upload

```bash
pio run -e esp32dev
pio run --target upload -e esp32dev
pio device monitor
```

## Audio notes

The speaker is driven by **PWM/LEDC on GPIO26** using the ESP32's `ledcWriteTone()` API. This works reliably on the CYD board used to test this template.

You can change the speaker pin or PWM channel in `include/config.h`:

```cpp
#define CYD_SPEAKER_PIN 26
#define CYD_SPEAKER_PWM_CHANNEL 0
```

## Adding libraries

Edit `platformio.ini` under `lib_deps`:

```ini
lib_deps =
    lovyan03/LovyanGFX @ ^1.1.16
    tzapu/WiFiManager @ ^2.0.17
    bblanchon/ArduinoJson @ ^7.0.4
    <new-library> @ ^<version>
```

## Notes

- The touch calibration is stored in NVS. Hold the screen at boot to recalibrate.
- Display rotation is set to `1` (landscape). Change `display.setRotation()` if needed.
- The dashboard polls GenMon every `GENMON_POLL_MS` milliseconds.
- Touch the screen to force an immediate refresh.
- Engine-state colors: green = running/exercising, cyan = ready/auto/off, orange = starting/cooling, red = alarm/fault.
- The dashboard uses GenMon's `/cmd/status_json`, `/cmd/maint_json`, and `/cmd/outage_json` endpoints.
