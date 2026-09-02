# CYD ESP32 Template

A minimal, reusable PlatformIO project for the **Cheap Yellow Display (CYD)** ESP32 board.

This template contains the CYD hardware scaffolding ready to copy into new projects:

- ILI9341 240x320 display via SPI
- XPT2046 resistive touch controller
- PWM backlight control
- Built-in speaker test using PWM/LEDC on GPIO26
- Touch calibration saved to NVS (Preferences)
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

## Build & Upload

```bash
pio run -e esp32dev
pio run --target upload -e esp32dev
pio device monitor
```

## Audio notes

The speaker is driven by **PWM/LEDC on GPIO26** using the ESP32's `ledcWriteTone()` API. On the board used to test this template, the I2S/DAC path to GPIO26 stayed silent even though the raw DAC and PWM paths worked, so the template uses PWM for reliable UI beeps and alerts.

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
    <new-library> @ ^<version>
```

## Notes

- The touch calibration is stored in NVS. Hold the screen at boot to recalibrate.
- Display rotation is set to `1` (landscape). Change `display.setRotation()` if needed.
- Replace the contents of `loop()` with your application logic; keep the LGFX class and audio helpers for the hardware setup.
