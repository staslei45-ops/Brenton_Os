#include "About.h"
#include "Display.h" 
#include <XPT2046_Touchscreen.h>

extern void showMenu(bool fullRedraw); 
extern void getPoint(int &x, int &y); 
extern XPT2046_Touchscreen ts;

#define TWRP_BG 0x2104
#define TWRP_BLUE 0x34BF

static bool aboutNeedsRedraw = true;
static unsigned long entryTime = 0; // Время входа в приложение

void initAbout() {
    aboutNeedsRedraw = true; 
    entryTime = millis(); // Запоминаем, когда мы открыли "О системе"
}

void tickAbout() {
    auto* canvas = bDisp.getCanvas();

    // 1. ОТРИСОВКА (Один раз)
    if (aboutNeedsRedraw) {
        canvas->fillScreen(TWRP_BG);
        
        // Шапка
        canvas->fillRect(0, 0, 320, 35, TWRP_BLUE);
        canvas->setTextColor(0xFFFF);
        canvas->setTextSize(2);
        canvas->setCursor(10, 10);
        canvas->print("SYSTEM INFO");

        // Инфо
        canvas->setTextSize(1);
        canvas->setCursor(15, 60);  canvas->print("Device: Brenton Phone");
        canvas->setCursor(15, 80);  canvas->print("CPU: ESP32-C6");
        canvas->setCursor(15, 100); canvas->print("Arch: RISC-V 32-bit");
        canvas->setCursor(15, 120); canvas->print("Clock: 160 MHz");
        canvas->setCursor(15, 140); canvas->print("Dev: staslei45");

        canvas->drawRoundRect(5, 50, 310, 110, 5, TWRP_BLUE);

        canvas->setCursor(80, 210);
        canvas->setTextColor(0x7BEF);
        canvas->print("Tap anywhere to return");

        bDisp.update(); 
        aboutNeedsRedraw = false; 
    }

    // 2. ТАЧ С ЗАЩИТОЙ
    if (ts.touched()) {
        // Если с момента входа прошло МЕНЬШЕ 500 мс — игнорируем нажатие
        // Это отсекает тот самый "двойной тап" от кнопки меню
        if (millis() - entryTime < 500) {
            return; 
        }

        int tx, ty;
        getPoint(tx, ty); 
        
        // Небольшая задержка перед выходом, чтобы меню не "схватило" нажатие обратно
        delay(150); 
        
        aboutNeedsRedraw = true; 
        showMenu(true); 
        return;
    }
}