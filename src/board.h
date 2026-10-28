// Pin map for the AliExpress "T-Dongle-S3 N16R8" clone. It does NOT match LilyGO's
// T-Dongle-S3 docs: the LCD pins were read out of the seller's firmware over USB-JTAG.
#pragma once

#define PIN_BUTTON      0

#define PIN_LCD_SCLK    10
#define PIN_LCD_MOSI    11
#define PIN_LCD_CS      12
#define PIN_LCD_DC      13
#define PIN_LCD_RST     14
#define PIN_LCD_BL      38   // active high

// The clone has no RGB LED (LilyGO's APA102 pins 39/40 drive nothing).
#define PIN_LED_DI      40
#define PIN_LED_CI      39
