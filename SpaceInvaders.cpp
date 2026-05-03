#include "Global.h"
#include "SpaceInvaders.h"
#include "Display.h" 
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>

extern XPT2046_Touchscreen ts;
extern void getPoint(int &x, int &y);

// --- НАСТРОЙКИ ОБЪЕКТОВ ---
#define MAX_BULLETS 6
#define MAX_ENEMIES 6
#define MAX_STARS 15
#define SHIP_Y 210

struct Bullet { float x, y; bool active; };
struct Enemy { float x, y; bool active; float speed; int hp; bool isBoss; };
struct Star { float x, y; float speed; };

static Bullet bullets[MAX_BULLETS];
static Enemy enemies[MAX_ENEMIES];
static Star stars[MAX_STARS];

static float shipX = 148;
static int score = 0;
static int highScore = 0;
static Preferences prefs;
static unsigned long lastShot = 0;
static unsigned long lastEnemySpawn = 0;

// --- РИСОВАНИЕ ---

static void drawShip(int x, int y) {
    auto* canvas = bDisp.getCanvas();
    canvas->fillRect(x + 10, y, 4, 6, 0xFFFF);      // Нос (белый)
    canvas->fillRect(x + 4, y + 6, 16, 8, 0x34BF);  // Корпус (синий TWRP)
    canvas->fillRect(x, y + 10, 24, 4, 0xF800);     // Крылья (красные)
}

static void drawEnemy(Enemy &e) {
    auto* canvas = bDisp.getCanvas();
    if (e.isBoss) {
        // Жирный красный босс
        canvas->fillRoundRect(e.x, e.y, 40, 20, 4, 0xF800); 
        canvas->fillRect(e.x + 10, e.y + 20, 20, 5, 0xF800);
        canvas->setTextColor(0xFFFF); canvas->setCursor(e.x + 15, e.y + 5);
        canvas->printf("%d", e.hp);
    } else {
        // Обычный зеленый пришелец
        canvas->fillRect(e.x + 4, e.y, 12, 8, 0x07E0);
        canvas->fillRect(e.x, e.y + 4, 4, 4, 0x07E0);
        canvas->fillRect(e.x + 16, e.y + 4, 4, 4, 0x07E0);
    }
}

// --- ЛОГИКА ---

void initSpaceInvaders() {
    score = 0; shipX = 148;
    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
    // Генерим звезды
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = random(0, 320);
        stars[i].y = random(0, 240);
        stars[i].speed = random(1, 4); // Разная скорость для эффекта глубины
    }
    prefs.begin("space", true);
    highScore = prefs.getInt("hs", 0);
    prefs.end();
}

void tickSpaceInvaders() {
    auto* canvas = bDisp.getCanvas();
    canvas->fillScreen(0x0000); // Глубокий космос

    // 1. ЭФФЕКТ ЗВЕЗД (Параллакс)
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].y += stars[i].speed;
        if (stars[i].y > 240) { stars[i].y = 0; stars[i].x = random(0, 320); }
        uint16_t sCol = (stars[i].speed > 2) ? 0xFFFF : 0x7BEF; // Быстрые звезды ярче
        canvas->drawPixel(stars[i].x, stars[i].y, sCol);
    }

    // 2. УПРАВЛЕНИЕ
    if (ts.touched()) {
        int tx, ty; getPoint(tx, ty);
        if (tx > 270 && ty < 50) { // ВЫХОД
            if (score > highScore) { prefs.begin("space", false); prefs.putInt("hs", score); prefs.end(); }
            showMenu(true); return;
        }
        shipX = constrain(tx - 12, 0, 320 - 24);
        if (millis() - lastShot > 300) { // Автострельба
            for (int i = 0; i < MAX_BULLETS; i++) {
                if (!bullets[i].active) {
                    bullets[i].active = true; bullets[i].x = shipX + 11; bullets[i].y = SHIP_Y;
                    lastShot = millis(); break;
                }
            }
        }
    }

    // 3. СПАВН ВРАГОВ И БОССОВ
    if (millis() - lastEnemySpawn > 1200) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!enemies[i].active) {
                enemies[i].active = true;
                enemies[i].x = random(20, 280);
                enemies[i].y = -20;
                // Каждый 10-й враг - Босс
                if (score > 0 && score % 10 == 0) {
                    enemies[i].isBoss = true; enemies[i].hp = 5; enemies[i].speed = 0.5;
                } else {
                    enemies[i].isBoss = false; enemies[i].hp = 1; enemies[i].speed = 1.2 + (score/15.0);
                }
                lastEnemySpawn = millis(); break;
            }
        }
    }

    // 4. ПУЛИ И КОЛЛИЗИИ
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        bullets[i].y -= 7;
        canvas->fillRect(bullets[i].x, bullets[i].y, 2, 6, 0xFFE0); // Желтый лазер

        if (bullets[i].y < 0) bullets[i].active = false;

        for (int j = 0; j < MAX_ENEMIES; j++) {
            if (enemies[j].active) {
                float hitW = enemies[j].isBoss ? 40 : 20;
                float hitH = enemies[j].isBoss ? 25 : 12;
                if (bullets[i].x > enemies[j].x && bullets[i].x < enemies[j].x + hitW &&
                    bullets[i].y > enemies[j].y && bullets[i].y < enemies[j].y + hitH) {
                    bullets[i].active = false;
                    enemies[j].hp--;
                    if (enemies[j].hp <= 0) { enemies[j].active = false; score++; }
                }
            }
        }
    }

    // 5. ДВИЖЕНИЕ ВРАГОВ
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        enemies[i].y += enemies[i].speed;
        drawEnemy(enemies[i]);
        if (enemies[i].y > 240) { initSpaceInvaders(); return; } // Game Over
    }

    // 6. ИНТЕРФЕЙС
    drawShip((int)shipX, SHIP_Y);
    canvas->fillRect(0, 0, 320, 25, 0x34BF);
    canvas->setTextColor(0xFFFF); canvas->setTextSize(1);
    canvas->setCursor(10, 8); canvas->printf("SCORE: %d  BEST: %d  TEMP: 19C", score, highScore);
    
    canvas->fillRect(290, 2, 25, 21, 0xF800); // Кнопка X
    canvas->setCursor(298, 6); canvas->print("X");
}