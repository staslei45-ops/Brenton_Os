#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS 18
#define TFT_DC 11
#define TFT_RST 10

class BrentonDisplay {
public:
    BrentonDisplay();
    void init();
    void update();
    GFXcanvas16* getCanvas();

    static const uint16_t BG = 0x0000;
    static const uint16_t TEXT = 0xFFFF;
    static const uint16_t SYSTEM = 0x07E0;

private:
    Adafruit_ILI9341 tft;
    GFXcanvas16 canvas;
};

extern BrentonDisplay bDisp;

#endif
