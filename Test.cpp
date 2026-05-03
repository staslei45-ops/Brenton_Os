#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

extern void showMenu(bool fullRedraw);
extern void getPoint(int &x, int &y);
extern Adafruit_ILI9341 tft;
extern XPT2046_Touchscreen ts;

int activeSubTest = 0;
int last_tx = -1;
int last_ty = -1;

#define TEST_HEADER 0xF800
#define TEST_BG     0x0000
#define BTN_COLOR   0x4208

void drawTestMenu() {
    tft.fillScreen(TEST_BG);

    tft.fillRect(0, 0, 320, 35, TEST_HEADER);
    tft.setCursor(10, 10);
    tft.setTextColor(0xFFFF);
    tft.setTextSize(2);
    tft.print("ENGINEER MENU");

    tft.fillRect(10, 50, 300, 40, BTN_COLOR);
    tft.drawRect(10, 50, 300, 40, 0xFFFF);
    tft.setCursor(20, 62);
    tft.print("1. PIXEL NOISE");

    tft.fillRect(10, 100, 300, 40, BTN_COLOR);
    tft.drawRect(10, 100, 300, 40, 0xFFFF);
    tft.setCursor(20, 112);
    tft.print("2. TOUCH DRAW");

    tft.fillRect(10, 190, 300, 40, 0x34BF);
    tft.setCursor(20, 202);
    tft.print("< EXIT TO OS");
}

void initTest() {
    activeSubTest = 0;
    last_tx = -1;
    last_ty = -1;
    drawTestMenu();
}

void tickTest() {

    if (activeSubTest == 0) {

        if (ts.touched()) {
            int tx, ty;
            getPoint(tx, ty);

            if (tx > 10 && tx < 310) {

                if (ty > 50 && ty < 90) {
                    activeSubTest = 1;
                    tft.fillScreen(0x0000);
                }

                else if (ty > 100 && ty < 140) {
                    activeSubTest = 2;
                    tft.fillScreen(0x0000);
                    tft.fillRect(0, 0, 320, 25, BTN_COLOR);
                }

                else if (ty > 190 && ty < 230) {
                    showMenu(true);
                }
            }
        }
    }

    else if (activeSubTest == 1) {

        for (int i = 0; i < 200; i++) {
            tft.drawPixel(random(320), 40 + random(160), random(0xFFFF));
        }

        if (ts.touched()) {
            int tx, ty;
            getPoint(tx, ty);

            if (ty > 200) {
                initTest();
            }
        }
    }

    else if (activeSubTest == 2) {

        if (ts.touched()) {
            int tx, ty;
            getPoint(tx, ty);

            if (last_tx != -1) {
                tft.drawLine(last_tx, last_ty, tx, ty, 0x07E0);
            } else {
                tft.drawPixel(tx, ty, 0x07E0);
            }

            last_tx = tx;
            last_ty = ty;
        } else {
            last_tx = -1;
            last_ty = -1;
        }

        if (ts.touched()) {
            int tx, ty;
            getPoint(tx, ty);

            if (tx > 280 && ty < 30) {
                initTest();
            }
        }
    }
}
