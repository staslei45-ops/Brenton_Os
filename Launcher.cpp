#include "Global.h"
#include "Launcher.h"
#include "Display.h"
#include <XPT2046_Touchscreen.h>

// Игры
#include "Cube.h"
#include "Flappy.h"
#include "Snake.h"
#include "SpaceInvaders.h"
#include "MutantBlob.h"
#include "BlobWars.h"
//#include "ConstantC.h"
#include "About.h"
#include "Notepad.h"
#include "Calc.h"
#include <CoolSpotGame.h>

extern XPT2046_Touchscreen ts;
extern void getPoint(int &x, int &y);
extern void showMenu(bool);
extern State currentState;

// ========== КОНСТАНТЫ ==========
#define SCREEN_W    320
#define SCREEN_H    240
#define TAB_H       35
#define CONTENT_Y   TAB_H

// Цвета (ваши, не менял)
#define COLOR_BG        0x1082
#define COLOR_TAB_ACTIVE 0x34BF
#define COLOR_TAB_INACTIVE 0x2104
#define COLOR_CARD      0x3186
#define COLOR_TEXT      0xFFFF
#define COLOR_SECONDARY 0x8410
#define COLOR_ACCENT    0x07FF

// ========== ВКЛАДКИ ==========
enum TabType { TAB_GAMES = 0, TAB_APPS = 1, TAB_SETTINGS = 2, TAB_SYSTEM = 3, TAB_COUNT = 4 };
static TabType currentTab = TAB_GAMES;

// Анимация нажатия
struct TouchAnim { bool active; int px; int py; int radius; int timer; };
static TouchAnim ripple;

// Виджеты
static int volumeLevel = 70;
static bool darkMode = true;

// FPS
static int currentFPS = 0;
static int currentCPU = 0;
static unsigned long lastFPSUpdate = 0;
static int frameCount = 0;

// ========== ВСПОМОГАТЕЛЬНЫЕ ==========
static void drawRipple() {
    if (!ripple.active) return;
    auto* cv = bDisp.getCanvas();
    if (ripple.radius < 30) {
        cv->drawCircle(ripple.px, ripple.py, ripple.radius, 0xFFFF);
        if (ripple.radius > 4) cv->drawCircle(ripple.px, ripple.py, ripple.radius - 2, 0xFFFF);
    }
    ripple.radius += 4;
    ripple.timer--;
    if (ripple.timer <= 0 || ripple.radius > 60) ripple.active = false;
}

static void drawGameIcon(int cx, int cy, int gameId) {
    auto* cv = bDisp.getCanvas();
    
    switch(gameId) {
        case 0: // CUBE — 3D куб
            cv->drawRect(cx - 8, cy - 8, 16, 16, 0xFFFF);
            cv->drawLine(cx - 4, cy - 12, cx + 12, cy - 12, 0xFFFF);
            cv->drawLine(cx - 4, cy - 12, cx - 4, cy + 4, 0xFFFF);
            cv->drawLine(cx + 12, cy - 12, cx + 12, cy + 4, 0xFFFF);
            cv->drawLine(cx - 4, cy + 4, cx + 12, cy + 4, 0xFFFF);
            break;
            
        case 1: // FLAPPY — птица
            cv->fillCircle(cx - 4, cy - 2, 6, 0xFFE0);
            cv->fillTriangle(cx - 2, cy - 6, cx + 6, cy - 2, cx - 2, cy + 2, 0xFFE0);
            cv->fillCircle(cx - 6, cy - 4, 2, 0xFFFF);
            cv->fillCircle(cx - 7, cy - 4, 1, 0x0000);
            cv->drawLine(cx - 10, cy - 2, cx - 14, cy, 0xFFFF);
            cv->drawLine(cx - 14, cy, cx - 10, cy + 2, 0xFFFF);
            cv->drawLine(cx + 2, cy - 4, cx + 8, cy - 8, 0xFFE0);
            cv->drawLine(cx + 2, cy, cx + 10, cy, 0xFFE0);
            cv->drawLine(cx + 2, cy + 4, cx + 8, cy + 8, 0xFFE0);
            break;
            
        case 2: // SNAKE — змейка
            cv->fillCircle(cx - 8, cy, 4, 0x07FF);
            cv->fillCircle(cx - 2, cy - 4, 4, 0x07FF);
            cv->fillCircle(cx + 4, cy - 2, 4, 0x07FF);
            cv->fillCircle(cx + 10, cy + 2, 4, 0x07FF);
            cv->fillCircle(cx - 10, cy - 2, 5, 0x07FF);
            cv->fillCircle(cx - 12, cy - 4, 2, 0xFFFF);
            cv->fillCircle(cx - 12, cy + 1, 2, 0xFFFF);
            cv->fillCircle(cx - 13, cy - 4, 1, 0x0000);
            cv->fillCircle(cx - 13, cy + 1, 1, 0x0000);
            cv->drawLine(cx - 15, cy - 2, cx - 18, cy - 4, 0xF800);
            cv->drawLine(cx - 15, cy - 2, cx - 18, cy, 0xF800);
            break;
            
        case 3: // INVADERS — пришелец
            cv->drawRect(cx - 6, cy - 8, 12, 8, 0xF800);
            cv->fillCircle(cx - 4, cy - 6, 2, 0xFFFF);
            cv->fillCircle(cx + 4, cy - 6, 2, 0xFFFF);
            cv->fillCircle(cx - 4, cy - 6, 1, 0x0000);
            cv->fillCircle(cx + 4, cy - 6, 1, 0x0000);
            cv->drawLine(cx - 4, cy, cx - 6, cy + 4, 0xF800);
            cv->drawLine(cx, cy, cx, cy + 4, 0xF800);
            cv->drawLine(cx + 4, cy, cx + 6, cy + 4, 0xF800);
            cv->drawLine(cx - 4, cy - 8, cx - 6, cy - 12, 0xF800);
            cv->drawLine(cx + 4, cy - 8, cx + 6, cy - 12, 0xF800);
            cv->fillCircle(cx - 6, cy - 12, 2, 0xFFE0);
            cv->fillCircle(cx + 6, cy - 12, 2, 0xFFE0);
            break;
            
        case 4: // BLOBS — капля
            cv->fillCircle(cx, cy - 4, 7, 0xFD20);
            cv->fillTriangle(cx - 6, cy, cx + 6, cy, cx, cy + 10, 0xFD20);
            cv->fillCircle(cx - 3, cy - 7, 2, 0xFFFF);
            cv->drawLine(cx - 3, cy + 3, cx + 3, cy + 3, 0x0000);
            cv->drawLine(cx - 2, cy + 4, cx + 2, cy + 4, 0x0000);
            break;
            
        case 5: // WARS — ракета
            cv->drawRect(cx - 3, cy - 10, 6, 14, 0xF81F);
            cv->drawTriangle(cx - 5, cy - 10, cx + 5, cy - 10, cx, cy - 14, 0xF81F);
            cv->drawLine(cx - 5, cy - 4, cx - 10, cy + 2, 0xF81F);
            cv->drawLine(cx + 5, cy - 4, cx + 10, cy + 2, 0xF81F);
            cv->fillCircle(cx, cy - 6, 2, 0x07FF);
            cv->drawLine(cx - 2, cy + 4, cx, cy + 10, 0xFFE0);
            cv->drawLine(cx + 2, cy + 4, cx, cy + 10, 0xFFE0);
            cv->drawLine(cx, cy + 4, cx, cy + 10, 0xF800);
            break;
    }
}
// ========== ВКЛАДКИ ==========
static void drawTabs() {
    auto* cv = bDisp.getCanvas();
    int tabW = SCREEN_W / TAB_COUNT;
    const char* names[] = {"GAMES", "APPS", "SETTINGS", "SYSTEM"};

    for (int i = 0; i < TAB_COUNT; i++) {
        int x = i * tabW;
        bool active = (currentTab == i);
        cv->fillRect(x, 0, tabW, TAB_H, active ? COLOR_TAB_ACTIVE : COLOR_TAB_INACTIVE);
        if (i > 0) cv->drawFastVLine(x, 0, TAB_H, 0x0000);

        cv->setTextColor(active ? 0xFFFF : 0x8410);
        cv->setTextSize(1);
        cv->setCursor(x + tabW/2 - strlen(names[i])*3, TAB_H - 10);
        cv->print(names[i]);

        if (active) cv->fillRect(x + 5, TAB_H - 3, tabW - 10, 3, COLOR_ACCENT);
    }
}

// ========== GAMES TAB ==========
static void drawGamesTab() {
    auto* cv = bDisp.getCanvas();
    int cols = 3;
    int tileW = 98;
    int tileH = 75;
    int startX = (SCREEN_W - cols * tileW) / 2;
    int startY = CONTENT_Y + 6;

    struct { const char* name; uint16_t color; } games[] = {
        {"CUBE", 0x07E0}, {"FLAPPY", 0xFFE0}, {"SNAKE", 0x07FF},
        {"INVADERS", 0xF800}, {"BLOBS", 0xFD20}, {"WARS", 0xF81F}
    };

    for (int i = 0; i < 6; i++) {
        int col = i % cols, row = i / cols;
        int x = startX + col * tileW, y = startY + row * (tileH + 5);

        cv->fillRoundRect(x, y, tileW, tileH, 6, COLOR_CARD);
        cv->drawRoundRect(x, y, tileW, tileH, 6, COLOR_SECONDARY);
        
        // Цветной круг
        cv->fillCircle(x + tileW/2, y + 28, 14, games[i].color);
        cv->drawCircle(x + tileW/2, y + 28, 14, 0xFFFF);
        
        // Иконка
        drawGameIcon(x + tileW/2, y + 28, i);

        // Название
        cv->setTextColor(COLOR_TEXT);
        cv->setTextSize(1);
        cv->setCursor(x + tileW/2 - strlen(games[i].name)*3, y + tileH - 12);
        cv->print(games[i].name);
    }
}

// ========== APPS TAB ==========
static void drawAppsTab() {
    auto* cv = bDisp.getCanvas();
    const char* apps[] = {"Calculator", "Notepad", "About System"};

    for (int i = 0; i < 3; i++) {
        int y = CONTENT_Y + 10 + i * 42;
        cv->fillRoundRect(10, y, SCREEN_W - 20, 36, 6, COLOR_CARD);
        cv->drawRoundRect(10, y, SCREEN_W - 20, 36, 6, COLOR_SECONDARY);
        cv->fillCircle(30, y + 18, 10, COLOR_ACCENT);

        cv->setTextColor(COLOR_TEXT);
        cv->setCursor(50, y + 12);
        cv->print(apps[i]);

        cv->drawLine(SCREEN_W - 20, y + 18, SCREEN_W - 10, y + 18, COLOR_SECONDARY);
        cv->drawLine(SCREEN_W - 15, y + 13, SCREEN_W - 10, y + 18, COLOR_SECONDARY);
        cv->drawLine(SCREEN_W - 15, y + 23, SCREEN_W - 10, y + 18, COLOR_SECONDARY);
    }
}

// ========== SETTINGS TAB ==========
static void drawSettingsTab() {
    auto* cv = bDisp.getCanvas();
    int yOff = CONTENT_Y + 10;

    // Volume slider
    cv->fillRoundRect(10, yOff, SCREEN_W - 20, 40, 6, COLOR_CARD);
    cv->setCursor(20, yOff + 15);
    cv->print("Volume");
    int slX = 100, slW = SCREEN_W - 120, fillW = slW * volumeLevel / 100;
    cv->drawRect(slX, yOff + 14, slW, 12, COLOR_SECONDARY);
    cv->fillRect(slX + 1, yOff + 15, fillW - 2, 10, COLOR_ACCENT);
    cv->fillCircle(slX + fillW, yOff + 20, 8, COLOR_TEXT);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", volumeLevel);
    cv->setCursor(slX + slW + 5, yOff + 15);
    cv->print(buf);

    // Dark Mode
    yOff += 55;
    cv->fillRoundRect(10, yOff, SCREEN_W - 20, 40, 6, COLOR_CARD);
    cv->setCursor(20, yOff + 15);
    cv->print("Dark Mode");
    int swX = SCREEN_W - 70;
    cv->drawRoundRect(swX, yOff + 8, 50, 24, 12, COLOR_SECONDARY);
    if (darkMode) {
        cv->fillRoundRect(swX + 2, yOff + 10, 23, 20, 10, 0x07E0);
        cv->fillCircle(swX + 36, yOff + 20, 9, COLOR_ACCENT);
    } else {
        cv->fillRoundRect(swX + 2, yOff + 10, 23, 20, 10, 0xF800);
        cv->fillCircle(swX + 14, yOff + 20, 9, COLOR_SECONDARY);
    }

    // Копирайт
    yOff += 55;
    cv->fillRoundRect(10, yOff, SCREEN_W - 20, 40, 6, COLOR_CARD);
    cv->setTextColor(COLOR_TEXT);
    cv->setTextSize(1);
    cv->setCursor(SCREEN_W/2 - 40, yOff + 16);
    cv->print("© 2026 StasLei45");
}

// ========== SYSTEM TAB ==========
static void drawSystemTab() {
    auto* cv = bDisp.getCanvas();
    int yOff = CONTENT_Y + 10;

    cv->fillRoundRect(10, yOff, SCREEN_W - 20, 110, 6, COLOR_CARD);
    cv->drawRoundRect(10, yOff, SCREEN_W - 20, 110, 6, COLOR_SECONDARY);

    cv->setTextColor(COLOR_TEXT);
    cv->setCursor(20, yOff + 20);
    cv->print("System Information");
    cv->drawFastHLine(20, yOff + 30, SCREEN_W - 40, COLOR_SECONDARY);

    // FPS
    cv->setCursor(20, yOff + 45);
    cv->printf("FPS: %d", currentFPS);
    if (currentFPS >= 50) { cv->setTextColor(0x07E0); cv->setCursor(100, yOff + 45); cv->print("STABLE"); cv->setTextColor(COLOR_TEXT); }

    // CPU
    cv->setCursor(20, yOff + 65);
    cv->printf("CPU Load: %d%%", currentCPU);
    int cpuBarW = SCREEN_W - 80;
    cv->drawRect(100, yOff + 62, cpuBarW, 8, COLOR_SECONDARY);
    cv->fillRect(101, yOff + 63, cpuBarW * currentCPU / 100 - 2, 6, (currentCPU > 70) ? 0xFD20 : 0x07E0);

    // RAM
    int freeHeap = ESP.getFreeHeap() / 1024;
    cv->setCursor(20, yOff + 85);
    cv->printf("RAM: %d / 320 KB", freeHeap);
    cv->drawRect(100, yOff + 82, cpuBarW, 8, COLOR_SECONDARY);
    cv->fillRect(101, yOff + 83, cpuBarW * (320 - freeHeap) / 320 - 2, 6, COLOR_ACCENT);

    // Version
    cv->setCursor(20, yOff + 105);
    cv->printf("Brenton OS v2.0");

    // Battery
    yOff += 120;
    cv->fillRoundRect(10, yOff, SCREEN_W - 20, 40, 6, COLOR_CARD);
    cv->setCursor(20, yOff + 15);
    cv->print("Battery");
    int batBarW = SCREEN_W - 100;
    cv->drawRect(90, yOff + 12, batBarW, 16, COLOR_SECONDARY);
    cv->fillRect(91, yOff + 13, batBarW * 87 / 100 - 2, 14, 0x07E0);
    cv->setCursor(SCREEN_W - 45, yOff + 18);
    cv->print("87%");
}

// ========== ОБНОВЛЕНИЕ FPS ==========
static void updateFPS() {
    frameCount++;
    if (millis() - lastFPSUpdate >= 1000) {
        currentFPS = frameCount;
        frameCount = 0;
        lastFPSUpdate = millis();
        currentCPU = random(15, 85);
    }
}

// ========== ОБРАБОТКА ТАЧА ==========
static void handleTabs(int x, int y) {
    if (y > TAB_H) return;
    int tabW = SCREEN_W / TAB_COUNT;
    int newTab = x / tabW;
    if (newTab >= 0 && newTab < TAB_COUNT && newTab != currentTab) {
        currentTab = (TabType)newTab;
        ripple.active = true; ripple.px = x; ripple.py = y; ripple.radius = 5; ripple.timer = 10;
    }
}

static void handleGames(int x, int y) {
    if (currentTab != TAB_GAMES) return;
    int cols = 3, tileW = 98, tileH = 75;
    int startX = (SCREEN_W - cols * tileW) / 2;
    int startY = CONTENT_Y + 6;

    for (int i = 0; i < 6; i++) {
        int col = i % cols, row = i / cols;
        int tx = startX + col * tileW, ty = startY + row * (tileH + 5);
        if (x >= tx && x <= tx + tileW && y >= ty && y <= ty + tileH) {
            ripple.active = true; ripple.px = x; ripple.py = y; ripple.radius = 5; ripple.timer = 10;
            switch(i) {
                case 0: currentState = GAME_CUBE; initCube(); break;
                case 1: currentState = GAME_FLAPPY; initFlappy(); break;
                case 2: currentState = GAME_SNAKE; initSnake(); break;
                case 3: currentState = GAME_SPACE; initSpaceInvaders(); break;
                case 4: currentState = GAME_BLOBS; initMutantBlob(); break;
                case 5: currentState = GAME_BLOBWARS; initBlobWars(); break;
            }
            return;
        }
    }
}

static void handleApps(int x, int y) {
    if (currentTab != TAB_APPS) return;
    if (y >= CONTENT_Y + 10 && y <= CONTENT_Y + 46) {
        currentState = APP_CALC; initCalc();
    } else if (y >= CONTENT_Y + 52 && y <= CONTENT_Y + 88) {
        currentState = APP_NOTEPAD; initNotepad();
    } else if (y >= CONTENT_Y + 94 && y <= CONTENT_Y + 130) {
        currentState = SYSTEM_ABOUT; initAbout();
    }
}

static void handleSettings(int x, int y) {
    if (currentTab != TAB_SETTINGS) return;
    int yOff = CONTENT_Y + 10;

    // Volume slider
    if (y > yOff && y < yOff + 40 && x > 100 && x < SCREEN_W - 40) {
        volumeLevel = (x - 100) * 100 / (SCREEN_W - 140);
        if (volumeLevel < 0) volumeLevel = 0;
        if (volumeLevel > 100) volumeLevel = 100;
    }
    
    // Dark Mode toggle
    if (y > yOff + 55 && y < yOff + 95 && x > SCREEN_W - 70) {
        darkMode = !darkMode;
    }
}

// ========== РЕНДЕР ==========
static void renderLauncher() {
    auto* cv = bDisp.getCanvas();
    cv->fillScreen(COLOR_BG);
    drawTabs();
    switch(currentTab) {
        case TAB_GAMES: drawGamesTab(); break;
        case TAB_APPS: drawAppsTab(); break;
        case TAB_SETTINGS: drawSettingsTab(); break;
        case TAB_SYSTEM: drawSystemTab(); break;
    }
    drawRipple();
    bDisp.update();
}

// ========== ПУБЛИЧНЫЕ ФУНКЦИИ ==========
void initLauncher() {
    currentTab = TAB_GAMES;
    currentFPS = 0;
    frameCount = 0;
    lastFPSUpdate = millis();
    ripple.active = false;
}

void tickLauncher() {
    updateFPS();
    if (ts.touched()) {
        int x, y; getPoint(x, y);
        handleTabs(x, y);
        handleGames(x, y);
        handleApps(x, y);
        handleSettings(x, y);
        if (!ripple.active) { ripple.active = true; ripple.px = x; ripple.py = y; ripple.radius = 5; ripple.timer = 8; }
    }
    renderLauncher();
}

void launchGame(int gameId) {
    // Заглушка для совместимости
}