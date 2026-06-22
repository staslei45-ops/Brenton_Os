#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// Твои пины (WROOM-32U)
#define TFT_CS    15
#define TFT_DC    22
#define TFT_RST   4
#define TFT_MOSI  23
#define TFT_SCLK  18
#define TFT_MISO  19

#define SCREEN_W 320
#define SCREEN_H 240
#define HALF_H   120

// "Виртуальный" канвас на весь экран (320x240), но физически
// состоящий из ДВУХ буферов по 320x120 (по 76 800 байт каждый).
// Суммарно та же память, что и у одного буфера 320x240
// (153 600 байт), но два куска по 75 КБ гораздо проще
// выделить в раздробленной куче ESP32, чем один кусок 150 КБ.
//
// Для всего остального кода (Cube, Snake, Flappy, Notepad,
// Calc, SpaceInvaders, About, CoolSpotGame) ничего не меняется -
// они как и раньше делают "auto* canvas = bDisp.getCanvas();"
// и вызывают обычные методы рисования (fillRect, drawLine,
// print и т.д.) из Adafruit_GFX. Вся "магия" разруливается
// внутри drawPixel().
class SplitCanvas : public Adafruit_GFX {
public:
  SplitCanvas();

  // Выделяет оба буфера. true = успех, false = не хватило памяти.
  bool begin();

  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void fillScreen(uint16_t color) override;
  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;

  GFXcanvas16* top;    // y: 0..119
  GFXcanvas16* bottom; // y: 120..239
};

class BrentonDisplay {
public:
  BrentonDisplay();
  void init();
  void update();
  Adafruit_GFX* getCanvas();

  static const uint16_t BG = 0x0000;
  static const uint16_t TEXT = 0xFFFF;
  static const uint16_t SYSTEM = 0x07E0;

private:
  Adafruit_ILI9341 tft;
  SplitCanvas canvas;
};

extern BrentonDisplay bDisp;

#endif