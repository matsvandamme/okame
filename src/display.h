#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "board.h"

// ST7735S 0.96" 80x160 IPS on the T-Dongle-S3, used in landscape (160x80).
class Display : public lgfx::LGFX_Device {
  lgfx::Panel_ST7735S _panel;
  lgfx::Bus_SPI _bus;

 public:
  Display() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 20000000;
      cfg.pin_sclk = PIN_LCD_SCLK;
      cfg.pin_mosi = PIN_LCD_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc = PIN_LCD_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs = PIN_LCD_CS;
      cfg.pin_rst = PIN_LCD_RST;
      cfg.pin_busy = -1;
      cfg.panel_width = 80;
      cfg.panel_height = 160;
      cfg.offset_x = 26;
      cfg.offset_y = 1;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.readable = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }
    setPanel(&_panel);
  }
};


