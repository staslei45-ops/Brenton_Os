#include "Display.h"

// Создаем канвас СРАЗУ 320x240, чтобы не крутить его потом
BrentonDisplay::BrentonDisplay() : tft(TFT_CS, TFT_DC, TFT_RST), canvas(320, 240) {}

void BrentonDisplay::init() {
    tft.begin();
    tft.setRotation(1); // Железный поворот в Альбом

    // ВАЖНО: Канвас НЕ КРУТИМ. Он уже создан как 320 (W) x 240 (H).
    // Это гарантирует, что координаты 0,0 в памяти и на экране совпадут.
    canvas.fillScreen(BG);
}

GFXcanvas16* BrentonDisplay::getCanvas() {
    return &canvas;
}

void BrentonDisplay::update() {
    // Копируем буфер. Поскольку размеры совпадают (320x240),
    // данные лягут пиксель в пиксель без искажений и разворотов.
    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 320, 240);
}

BrentonDisplay bDisp;
