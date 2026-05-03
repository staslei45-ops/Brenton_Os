#include "SpaceInvaders.h"
#include "Display.h" // Наш движок
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>

extern XPT2046_Touchscreen ts;
extern void getPoint(int &x, int &y);
extern void renderMenu(); 

#define SHIP_W 24
#define MAX_BULLETS 4
#define SHIP_Y 220

struct Bullet {
    float x, y;
    bool active;
};

static Bullet bullets[MAX_BULLETS];
static float shipX = 148;
static int score = 0;
static int highScore = 0;

static Preferences prefs;
static unsigned long lastShot = 0;

// Теперь рисуем всегда в канвас
static void drawShip(int x, int y, uint16_t color) {
    auto* canvas = bDisp.getCanvas();
    canvas->fillRect(x + 10, y, 4, 4, color);     // Нос
    canvas->fillRect(x + 6, y + 4, 12, 4, color); // Корпус
    canvas->fillRect(x, y + 8, 24, 4, color);     // Крылья
}

static void saveScore() {
    if (score > highScore) {
        highScore = score;
        prefs.begin("space", false);
        prefs.putInt("hs", highScore);
        prefs.end();
    }
}

void initSpaceInvaders() {
    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
    score = 0;
    shipX = 148;

    prefs.begin("space", true);
    highScore = prefs.getInt("hs", 0);
    prefs.end();

    // В буферном режиме нам не нужно чистить экран в init, 
    // это сделает первый тик.
}

void tickSpaceInvaders() {
    auto* canvas = bDisp.getCanvas();

    // 1. Очистка кадра (Черный космос)
    canvas->fillScreen(0x0000);

    // 2. Управление
    if (ts.touched()) {
        int tx, ty;
        getPoint(tx, ty);

        // Кнопка выхода
        if (tx > 280 && ty < 40) {
            saveScore();
            renderMenu();
            return;
        }

        // Корабль следует за пальцем
        shipX = constrain(tx - 12, 0, 320 - 24);
        
        // Автострельба
        if (millis() - lastShot > 400) {
            for (int i = 0; i < MAX_BULLETS; i++) {
                if (!bullets[i].active) {
                    bullets[i].active = true;
                    bullets[i].x = shipX + 11;
                    bullets[i].y = SHIP_Y - 5;
                    lastShot = millis();
                    break;
                }
            }
        }
    }

    // 3. Логика пуль
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            bullets[i].y -= 5; // Скорость пули
            if (bullets[i].y < 35) {
                bullets[i].active = false;
            } else {
                canvas->fillRect(bullets[i].x, bullets[i].y, 2, 5, 0xFFFF);
            }
        }
    }

    // 4. Отрисовка корабля
    drawShip((int)shipX, SHIP_Y, 0x07FF);

    // 5. Интерфейс
    // Кнопка выхода (красная)
    canvas->fillRect(290, 5, 25, 25, 0xB000);
    canvas->setCursor(298, 10); 
    canvas->setTextColor(0xFFFF); 
    canvas->setTextSize(2); 
    canvas->print("X");

    // Статистика
    canvas->setTextColor(0xFFFF); 
    canvas->setTextSize(1);
    canvas->setCursor(5, 5);
    canvas->printf("SCORE: %d  HI: %d", score, highScore);

    // bDisp.update() произойдет в MainLoop автоматически!
}