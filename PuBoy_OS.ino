#include "Display.h"
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

#include "Cube.h"
#include "Flappy.h"
#include "Snake.h"
#include "About.h"
#include "Notepad.h"
#include "SpaceInvaders.h"

// Пины подключения
#define TOUCH_CS 1
#define SCK_PIN 6
#define MISO_PIN 2
#define MOSI_PIN 7

#define INVERT_X 1
#define INVERT_Y 1

// Цвета TWRP Style
#define TWRP_BG    0x2104
#define TWRP_BLUE  0x34BF
#define TWRP_GRAY  0x4208
#define TWRP_TEXT  0xFFFF

XPT2046_Touchscreen ts(TOUCH_CS);

enum State { MENU, GAME_CUBE, GAME_FLAPPY, GAME_SNAKE, SYSTEM_ABOUT, APP_NOTEPAD, GAME_SPACE };
State currentState = MENU;

// --- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (Важно для линковки с Notepad.cpp) ---
unsigned long lastTouch = 0; 
unsigned long exitTimer = 0; 
int scrollY = 0;            
int maxScroll = 300;        
int lastTouchY = -1;        
int startTouchY = -1;       
bool isScrolling = false;   

void getPoint(int &x, int &y) {
    TS_Point p = ts.getPoint();
    x = map(p.x, 200, 3800, 0, 320);
    y = map(p.y, 200, 3800, 0, 240);
    if (INVERT_X) x = 320 - x;
    if (INVERT_Y) y = 240 - y;
}

// Отрисовка плитки (кнопки)
void drawButton(int x, int y, int w, int h, const char* label) {
    auto* canvas = bDisp.getCanvas();
    int renderY = y - scrollY; 
    
    if (renderY + h > 35 && renderY < 240) {
        canvas->fillRoundRect(x, renderY, w, h, 4, TWRP_GRAY);
        canvas->drawRoundRect(x, renderY, w, h, 4, 0x5AEB); 
        canvas->setTextColor(TWRP_TEXT);
        canvas->setTextSize(1);
        canvas->setCursor(x + 10, renderY + (h / 2) - 4);
        canvas->print(label);
    }
}

void renderMenu() {
    auto* canvas = bDisp.getCanvas();
    canvas->fillScreen(TWRP_BG);
    
    // Сетка плиток (как ты просил)
    drawButton(5, 45, 150, 60, "CUBE 3D");
    drawButton(165, 45, 150, 60, "FLAPPY");
    
    drawButton(5, 115, 150, 60, "SNAKE");
    drawButton(165, 115, 150, 60, "NOTES");
    
    drawButton(5, 185, 310, 60, "ABOUT SYSTEM");
    drawButton(5, 255, 310, 60, "SPACE INVADERS");
    drawButton(5, 325, 310, 60, "TOOLS & SETTINGS");

    // Шапка поверх всего
    canvas->fillRect(0, 0, 320, 35, TWRP_BLUE); 
    canvas->setCursor(10, 8);
    canvas->setTextSize(2);
    canvas->setTextColor(TWRP_TEXT);
    canvas->print("Brenton OS");

    bDisp.update();
}

void showMenu(bool fullRedraw) {
    currentState = MENU;
    exitTimer = millis();
    isScrolling = false;
    startTouchY = -1;
    if (fullRedraw) renderMenu();
}

void setup() {
    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
    bDisp.init(); 
    ts.begin();
    ts.setRotation(1);
    renderMenu();
}

void loop() {
    if (currentState == MENU) {
        if (ts.touched() && (millis() - exitTimer > 500)) {
            int tx, ty;
            getPoint(tx, ty);
            
            if (startTouchY == -1) {
                startTouchY = ty;
                lastTouchY = ty;
            }

            int diffY = lastTouchY - ty;
            if (abs(ty - startTouchY) > 15) {
                isScrolling = true;
            }

            if (isScrolling) {
                scrollY += diffY;
                if (scrollY < 0) scrollY = 0;
                if (scrollY > maxScroll) scrollY = maxScroll;
                lastTouchY = ty;
                renderMenu();
            }
        } 
        else {
            // Если отпустили палец
            if (startTouchY != -1) {
                if (!isScrolling) {
                    int tx, ty;
                    getPoint(tx, ty);
                    int cy = ty + scrollY; // Виртуальный Y

                    if (ty > 35 && millis() > lastTouch) {
                        if (cy > 45 && cy < 105) {
                            if (tx < 155) { currentState = GAME_CUBE; initCube(); }
                            else { currentState = GAME_FLAPPY; initFlappy(); }
                        }
                        else if (cy > 115 && cy < 175) {
                            if (tx < 155) { currentState = GAME_SNAKE; initSnake(); }
                            else { currentState = APP_NOTEPAD; initNotepad(); }
                        }
                        else if (cy > 185 && cy < 245) { currentState = SYSTEM_ABOUT; initAbout(); }
                        else if (cy > 255 && cy < 315) { currentState = GAME_SPACE; initSpaceInvaders(); }
                        
                        lastTouch = millis() + 500;
                    }
                }
                startTouchY = -1;
                lastTouchY = -1;
                isScrolling = false;
            }
        }
    } 
    else {
        // Выполнение игр/приложений
        switch (currentState) {
            case GAME_CUBE:      tickCube();   break;
            case GAME_FLAPPY:    tickFlappy(); break;
            case GAME_SNAKE:     tickSnake();  break;
            case SYSTEM_ABOUT:   tickAbout();  break;
            case APP_NOTEPAD:    tickNotepad();break;
            case GAME_SPACE:     tickSpaceInvaders(); break;
        }

        if (currentState == MENU) return; 
        
        // Обновляем экран для игр
        if (currentState != SYSTEM_ABOUT && currentState != APP_NOTEPAD) {
            bDisp.update();
        }
    }
}