#include "Global.h"
#include "Display.h"
#include "esp_heap_caps.h"

// ---------------- SplitCanvas ----------------

SplitCanvas::SplitCanvas() : Adafruit_GFX(SCREEN_W, SCREEN_H), top(nullptr), bottom(nullptr) {}

bool SplitCanvas::begin() {
  top    = new GFXcanvas16(SCREEN_W, HALF_H);
  bottom = new GFXcanvas16(SCREEN_W, HALF_H);

  if (top == nullptr || bottom == nullptr) return false;
  if (top->getBuffer() == nullptr || bottom->getBuffer() == nullptr) return false;
  return true;
}

// Главная "магия": каждый пиксель уходит в нужную половину буфера.
// Все остальные функции рисования из Adafruit_GFX (fillRect,
// drawLine, fillCircle, текст и т.д.) в итоге вызывают drawPixel(),
// так что весь игровой код продолжает работать без изменений.
void SplitCanvas::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;

  if (y < HALF_H) {
    top->drawPixel(x, y, color);
  } else {
    bottom->drawPixel(x, y - HALF_H, color);
  }
}

// Полная заливка экрана через GFXcanvas16::fillScreen (memset),
// а не через цикл drawPixel x2 (76800 пикселей). Это вызывается
// каждый кадр в renderMenu() и в большинстве игр перед перерисовкой -
// главный источник просадки FPS на split-буфере.
void SplitCanvas::fillScreen(uint16_t color) {
  top->fillScreen(color);
  bottom->fillScreen(color);
}

// Горизонтальная линия (используется в fillRect/fillTriangle/fillRoundRect
// построчно) - целым отрезком уходит в нужную половину, без
// per-pixel double-dispatch.
void SplitCanvas::writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (y < 0 || y >= SCREEN_H || w <= 0) return;

  int16_t x0 = x, x1 = x + w;
  if (x0 < 0) x0 = 0;
  if (x1 > SCREEN_W) x1 = SCREEN_W;
  if (x0 >= x1) return;
  int16_t ww = x1 - x0;

  if (y < HALF_H) {
    top->drawFastHLine(x0, y, ww, color);
  } else {
    bottom->drawFastHLine(x0, y - HALF_H, ww, color);
  }
}

// Вертикальная линия - если пересекает границу половин, делится на 2 куска.
void SplitCanvas::writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (x < 0 || x >= SCREEN_W || h <= 0) return;

  int16_t y0 = y, y1 = y + h;
  if (y0 < 0) y0 = 0;
  if (y1 > SCREEN_H) y1 = SCREEN_H;
  if (y0 >= y1) return;

  if (y1 <= HALF_H) {
    top->drawFastVLine(x, y0, y1 - y0, color);
  } else if (y0 >= HALF_H) {
    bottom->drawFastVLine(x, y0 - HALF_H, y1 - y0, color);
  } else {
    top->drawFastVLine(x, y0, HALF_H - y0, color);
    bottom->drawFastVLine(x, 0, y1 - HALF_H, color);
  }
}

// Прямоугольник (кнопки меню, грани куба и т.д.) - делится максимум
// на 2 прямоугольника по половинам и заливается через fillRect
// каждого под-канваса.
void SplitCanvas::writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (w <= 0 || h <= 0) return;

  int16_t x0 = x, y0 = y, x1 = x + w, y1 = y + h;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > SCREEN_W) x1 = SCREEN_W;
  if (y1 > SCREEN_H) y1 = SCREEN_H;
  if (x0 >= x1 || y0 >= y1) return;
  int16_t ww = x1 - x0;

  if (y1 <= HALF_H) {
    top->fillRect(x0, y0, ww, y1 - y0, color);
  } else if (y0 >= HALF_H) {
    bottom->fillRect(x0, y0 - HALF_H, ww, y1 - y0, color);
  } else {
    top->fillRect(x0, y0, ww, HALF_H - y0, color);
    bottom->fillRect(x0, 0, ww, y1 - HALF_H, color);
  }
}

// ---------------- BrentonDisplay ----------------

BrentonDisplay::BrentonDisplay() : tft(TFT_CS, TFT_DC, TFT_RST) {}

void BrentonDisplay::init() {
  Serial.printf("Free heap before canvas alloc: %u\n", ESP.getFreeHeap());
  Serial.printf("Largest free block: %u\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

  tft.begin();
  tft.setRotation(1);

  if (!canvas.begin()) {
    // Если даже два куска по 75 КБ не выделились - не крашимся,
    // а показываем сообщение.
    tft.fillScreen(BG);
    tft.setTextColor(0xF800);
    tft.setTextSize(1);
    tft.setCursor(10, 10);
    tft.print("Canvas alloc failed!");
    while (true) { delay(1000); }
  }

  Serial.println("Checkpoint: split canvas allocated OK");

  canvas.fillScreen(BG);
  tft.fillScreen(BG);
}

Adafruit_GFX* BrentonDisplay::getCanvas() {
  return &canvas;
}

void BrentonDisplay::update() {
  // Шлём обе половины буфера на экран как два кадра подряд -
  // визуально пользователь видит один цельный кадр без мерцания.
  tft.drawRGBBitmap(0, 0,       canvas.top->getBuffer(),    SCREEN_W, HALF_H);
  tft.drawRGBBitmap(0, HALF_H,  canvas.bottom->getBuffer(), SCREEN_W, HALF_H);
}

BrentonDisplay bDisp;