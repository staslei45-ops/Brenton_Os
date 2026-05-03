#include "Global.h"
#include "Display.h"
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include "driver/temperature_sensor.h"

#include "Cube.h"
#include "Flappy.h"
#include "Snake.h"
#include "About.h"
#include "Notepad.h"
#include "SpaceInvaders.h"
#include "Calc.h"
#include <CoolSpotGame.h> 

#define TOUCH_CS 1
#define SCK_PIN 6
#define MISO_PIN 2
#define MOSI_PIN 7

XPT2046_Touchscreen ts(TOUCH_CS);
temperature_sensor_handle_t temp_sensor = NULL;
CoolSpotGame mySpot; 

State currentState = MENU; 
unsigned long lastTouch = 0; 
unsigned long exitTimer = 0; 
int scrollY = 0;            
int maxScroll = 300; 
int lastTouchY = -1;        
int startTouchY = -1;       
bool isScrolling = false;   

void getPoint(int &x, int &y) {
    TS_Point p = ts.getPoint();
    x = map(p.x, 200, 3800, 320, 0); 
    y = map(p.y, 200, 3800, 240, 0); 
}

void showMenu(bool fullRedraw) {
    currentState = MENU;
    exitTimer = millis();
    isScrolling = false;
    startTouchY = -1;
    if (fullRedraw) renderMenu();
}

// ГРАФИЧЕСКИЙ ДВИЖОК ИКОНОК (стиль 7-Up)
void drawButton(int x, int y, int w, int h, const char* label, uint16_t color = 0x4208) {
    auto* canvas = bDisp.getCanvas();
    int ry = y - scrollY; 
    if (ry + h > 32 && ry < 240) {
        // Фон и четкая рамка
        canvas->fillRoundRect(x, ry, w, h, 4, color); 
        canvas->drawRoundRect(x, ry, w, h, 4, 0xFFFF); 
        
        int ix = x + 22; // Центр иконки по X
        int iy = ry + h/2; // Центр иконки по Y

        // СТИЛЬНЫЕ ИКОНКИ
        if (strcmp(label, "CUBE 3D") == 0) {
            canvas->drawRect(ix-8, iy-8, 14, 14, 0xFFFF);
            canvas->drawRect(ix-4, iy-12, 14, 14, 0xFFFF);
            canvas->drawLine(ix-8, iy-8, ix-4, iy-12, 0xFFFF);
        } 
        else if (strcmp(label, "FLAPPY") == 0) {
            canvas->fillCircle(ix, iy, 8, 0xFFE0); // Тело
            canvas->fillCircle(ix+3, iy-2, 2, 0x0000); // Глаз
            canvas->fillRect(ix+6, iy, 5, 3, 0xF800); // Клюв
            canvas->fillRoundRect(ix-6, iy, 6, 4, 2, 0xFFFF); // Крыло
        } 
        else if (strcmp(label, "SNAKE") == 0) {
            canvas->fillRect(ix-10, iy, 15, 5, 0x07E0); // Тело
            canvas->fillRect(ix+2, iy-5, 5, 10, 0x07E0); // Голова
            canvas->fillRect(ix+5, iy-3, 2, 2, 0x0000); // Глаз
        } 
        else if (strcmp(label, "SPACE INVADERS") == 0) {
            canvas->fillRoundRect(ix-12, iy-2, 24, 8, 3, 0x07E0); // НЛО
            canvas->fillRect(ix-4, iy-6, 8, 4, 0x07E0); // Кабина
        } 
        else if (strcmp(label, "CALCULATOR") == 0) {
            canvas->drawRoundRect(ix-10, iy-10, 20, 20, 2, 0xFFFF);
            canvas->fillRect(ix-6, iy-6, 4, 4, 0xFFFF);
            canvas->fillRect(ix+2, iy-6, 4, 4, 0xFFFF);
            canvas->fillRect(ix-6, iy+2, 12, 4, 0xFFFF);
        }
        else if (strcmp(label, "NOTES") == 0) {
            canvas->fillRect(ix-8, iy-10, 16, 20, 0xFFFF);
            canvas->drawFastHLine(ix-5, iy-4, 10, 0x0000);
            canvas->drawFastHLine(ix-5, iy, 10, 0x0000);
        }

        // Текст (сдвинут вправо от иконки)
        canvas->setTextColor(0xFFFF);
        canvas->setTextSize(1);
        canvas->setCursor(x + 45, ry + (h / 2) - 4);
        canvas->print(label);
    }
}

void drawCoolSpotButton(int x, int y, int w, int h) {
    auto* canvas = bDisp.getCanvas();
    int ry = y - scrollY;
    if (ry + h <= 32 || ry >= 240) return;

    canvas->fillRoundRect(x, ry, w, h, 4, 0x03E0); 
    canvas->drawRoundRect(x, ry, w, h, 4, 0xFFFF);
    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(3);
    canvas->setCursor(x + 10, ry + 12);
    canvas->print("7");
    canvas->fillCircle(x + 42, ry + 25, 9, 0xF800); // Красный Спот
    canvas->fillCircle(x + 40, ry + 23, 2, 0xFFFF); 
    canvas->setCursor(x + 58, ry + 12);
    canvas->print("UP");
    canvas->setTextSize(1);
    canvas->setCursor(x + 10, ry + 38);
    canvas->print("ADVENTURE");
}

void renderMenu() {
    auto* canvas = bDisp.getCanvas();
    canvas->fillScreen(0x2104); 
    
    // Сетка (Твои координаты, новые цвета)
    drawButton(5, 40, 150, 50, "CUBE 3D", 0x2104);
    drawButton(165, 40, 150, 50, "FLAPPY", 0x5599);
    drawButton(5, 95, 150, 50, "SNAKE", 0x0000);
    drawButton(165, 95, 150, 50, "NOTES", 0x632C);
    drawButton(5, 150, 310, 50, "SPACE INVADERS", 0x0000);
    drawButton(5, 205, 310, 50, "CALCULATOR", 0x52AA); 
    drawButton(5, 260, 310, 50, "ABOUT SYSTEM", 0x4208);

    drawCoolSpotButton(5, 315, 150, 50); 
    drawButton(165, 315, 150, 50, "[ EMPTY ]", 0x2104);
    drawButton(5, 370, 150, 50, "[ EMPTY ]", 0x2104);
    drawButton(165, 370, 150, 50, "[ EMPTY ]", 0x2104);

    // Бар
    canvas->fillRect(0, 0, 320, 32, 0x34BF); 
    canvas->setCursor(10, 8);
    canvas->setTextSize(2);
    canvas->setTextColor(0xFFFF);
    canvas->print("Brenton OS");

    float tsens_out;
    temperature_sensor_get_celsius(temp_sensor, &tsens_out);
    canvas->setTextSize(1);
    canvas->setCursor(200, 4);
    canvas->printf("RAM: %u KB", esp_get_free_heap_size() / 1024);
    canvas->setCursor(200, 18);
    canvas->printf("CPU: %.1f C", tsens_out);

    bDisp.update();
}

void setup() {
    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
    bDisp.init(); ts.begin(); ts.setRotation(1);
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    temperature_sensor_install(&temp_sensor_config, &temp_sensor);
    temperature_sensor_enable(temp_sensor);
    renderMenu();
}

void loop() {
    if (currentState == MENU) {
        if (ts.touched() && (millis() - exitTimer > 500)) {
            int tx, ty; getPoint(tx, ty);
            if (startTouchY == -1) { startTouchY = ty; lastTouchY = ty; }
            int diffY = lastTouchY - ty;
            if (abs(ty - startTouchY) > 10) isScrolling = true;
            if (isScrolling) {
                scrollY += diffY;
                if (scrollY < 0) scrollY = 0;
                if (scrollY > maxScroll) scrollY = maxScroll;
                lastTouchY = ty;
                renderMenu();
            }
        } 
        else if (startTouchY != -1) {
            if (!isScrolling) {
                int tx, ty; getPoint(tx, ty);
                int cy = ty + scrollY; 
                if (ty > 32 && millis() > lastTouch) {
                    if (cy > 40 && cy < 90) {
                        if (tx < 155) { currentState = GAME_CUBE; initCube(); }
                        else { currentState = GAME_FLAPPY; initFlappy(); }
                    }
                    else if (cy > 95 && cy < 145) {
                        if (tx < 155) { currentState = GAME_SNAKE; initSnake(); }
                        else { currentState = APP_NOTEPAD; initNotepad(); }
                    }
                    else if (cy > 150 && cy < 200) { currentState = GAME_SPACE; initSpaceInvaders(); }
                    else if (cy > 205 && cy < 255) { currentState = APP_CALC; initCalc(); }
                    else if (cy > 260 && cy < 310) { currentState = SYSTEM_ABOUT; initAbout(); }
                    else if (cy > 315 && cy < 365) { if (tx < 155) { currentState = GAME_COOLSPOT; mySpot.init(); } }
                    lastTouch = millis() + 400;
                }
            }
            startTouchY = -1; lastTouchY = -1; isScrolling = false;
        }
    } 
    else {
        switch (currentState) {
            case GAME_CUBE:      tickCube();   break;
            case GAME_FLAPPY:    tickFlappy(); break;
            case GAME_SNAKE:     tickSnake();  break;
            case SYSTEM_ABOUT:   tickAbout();  break;
            case APP_NOTEPAD:    tickNotepad();break;
            case GAME_SPACE:     tickSpaceInvaders(); break;
            case APP_CALC:       tickCalc();   break;
            case GAME_COOLSPOT:  mySpot.update(); mySpot.render(); if (!mySpot.isRunning) showMenu(true); break;
        }
        if (currentState != MENU && currentState != SYSTEM_ABOUT && currentState != APP_NOTEPAD && currentState != APP_CALC) bDisp.update();
        
    }
}