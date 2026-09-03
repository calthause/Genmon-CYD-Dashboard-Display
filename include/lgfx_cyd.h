#ifndef LGFX_CYD_H
#define LGFX_CYD_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

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

#endif // LGFX_CYD_H
