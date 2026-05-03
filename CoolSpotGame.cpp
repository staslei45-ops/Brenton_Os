#include "CoolSpotGame.h"
#include <Display.h>
#include <XPT2046_Touchscreen.h>
#include "Global.h"

extern BrentonDisplay bDisp;
extern XPT2046_Touchscreen ts;
extern void getPoint(int &x, int &y);

// --- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ОТРИСОВКИ ---

void CoolSpotGame::drawSpot(int sx, int sy, bool flipped) {
    auto* canvas = bDisp.getCanvas();

    // Тело с контуром для объема
    canvas->fillCircle(sx, sy, 9, 0xF800); // Красный[cite: 4]
    canvas->drawCircle(sx, sy, 9, 0x0000); // Черный контур

    // Белые перчатки (фишка персонажа)
    canvas->fillCircle(sx + (flipped ? 7 : -7), sy + 3, 3, 0xFFFF);

    // Очки "Авиаторы"
    int ox = flipped ? sx - 8 : sx + 0;
    canvas->fillRoundRect(ox, sy - 4, 8, 5, 2, 0x0000); // Оправа
    canvas->drawPixel(ox + 2, sy - 3, 0xFFFF);         // Блик слева
    canvas->drawPixel(ox + 6, sy - 3, 0xFFFF);         // Блик справа

    // Обувь (небольшие штрихи снизу)
    canvas->fillRect(sx - 4, sy + 7, 8, 3, 0xFFFF);
}

void CoolSpotGame::drawDetailedCoin(int cx, int cy) {
    auto* canvas = bDisp.getCanvas();

    // Динамический блеск в зависимости от времени[cite: 4]
    int shine = abs(sin(millis() / 150.0) * 4);

    canvas->fillCircle(cx, cy, 6, 0x9480); // Темное золото (граница)
    canvas->fillCircle(cx, cy, 5, 0xFD20); // Золото[cite: 4]
    canvas->fillCircle(cx, cy, 3, 0xFFE0); // Светлый центр

    // Бегающий блик
    canvas->drawFastVLine(cx - 2 + shine, cy - 2, 4, 0xFFFF);
}

// --- ОСНОВНАЯ ЛОГИКА (Остается прежней, меняем render) ---[cite: 4]

void CoolSpotGame::init() {
    isRunning = true;
    status = GS_MENU;
    score = 0; lives = 3; enemyCount = 3;
    uint8_t tempMap[12][40] = {
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {1,0,2,2,2,0,0,0,1,1,1,0,0,0,0,0,0,0,2,2,2,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,2,0,1},
        {1,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,1,1,1},
        {1,0,0,0,0,0,0,0,0,0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,0,0,0,1},
        {1,0,0,0,0,0,1,1,1,1,1,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1},
        {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {1,2,0,0,2,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {1,1,1,0,1,1,0,0,0,0,0,0,0,1,1,1,1,1,1,0,1,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,0,0,0,0,2,0,1},
        {1,0,0,0,0,0,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,0,0,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
    };
    memcpy(levelMap, tempMap, sizeof(levelMap));
    enemies[0] = {200, 190, 1, 150, 300};
    enemies[1] = {500, 170, -1, 400, 600};
    enemies[2] = {750, 190, 1, 700, 850};
    resetPlayer();
}

void CoolSpotGame::resetPlayer() {
    x = 40; y = 100; velX = 0; velY = 0; camX = 0; isGrounded = false; flip = false;
}

bool CoolSpotGame::checkCollision(float nextX, float nextY) {
    int tx = (int)nextX / 20; int ty = (int)nextY / 20;
    if (tx < 0 || tx >= mapW || ty < 0 || ty >= mapH) return true;
    return levelMap[ty][tx] == TILE_WALL;
}

void CoolSpotGame::update() {
    if (ts.touched()) {
        int tx, ty; getPoint(tx, ty);
        if (tx > 280 && ty < 45) { isRunning = false; return; }
        if (status == GS_MENU || status == GS_DEAD) {
            if (tx > 80 && tx < 240 && ty > 150) { init(); status = GS_PLAYING; delay(200); }
            return;
        }
        if (status == GS_PLAYING) {
            if (tx > 140 && tx < 180 && ty < 45) { status = GS_PAUSED; delay(200); return; }
            if (tx > 160) { if (isGrounded) { velY = -8.2; isGrounded = false; } }
            else { if (tx < 80) { velX -= 0.9; flip = true; } else { velX += 0.9; flip = false; } }
        }
        else if (status == GS_PAUSED) { if (ty > 45) { status = GS_PLAYING; delay(200); } return; }
    }
    if (status != GS_PLAYING) return;

    velY += 0.45; velX *= 0.85;
    float nX = x + velX;
    if (!checkCollision(nX + (velX > 0 ? 8 : -8), y)) x = nX; else velX = 0;
    y += velY;
    if (velY > 0) {
        if (checkCollision(x, y + 10)) { y = (int(y + 10) / 20) * 20 - 10; velY = 0; isGrounded = true; }
        else isGrounded = false;
    } else { if (checkCollision(x, y - 10)) velY = 0; }

    int tx = x / 20; int ty = y / 20;
    if (levelMap[ty][tx] == TILE_COIN) { levelMap[ty][tx] = TILE_EMPTY; score += 10; }

    for (int i = 0; i < enemyCount; i++) {
        enemies[i].x += enemies[i].dir * 1.8;
        if (enemies[i].x < enemies[i].minX || enemies[i].x > enemies[i].maxX) enemies[i].dir *= -1;
        if (abs(x - enemies[i].x) < 15 && abs(y - enemies[i].y) < 15) {
            lives--; if (lives <= 0) status = GS_DEAD; else resetPlayer();
        }
    }

    float targetCam = x - 160; camX = camX * 0.9 + targetCam * 0.1;
    if (camX < 0) camX = 0;
    if (camX > (mapW * 20) - 320) camX = (mapW * 20) - 320;
}

void CoolSpotGame::render() {
    auto* canvas = bDisp.getCanvas();

    if (status == GS_MENU) {
        canvas->fillScreen(0x2104);
        canvas->setTextSize(6); canvas->setTextColor(0xFFFF);
        canvas->setCursor(80, 40); canvas->print("7");
        float pulse = sin(millis() / 300.0) * 4;
        canvas->fillCircle(150, 65, 22 + pulse, 0xF800);
        canvas->fillCircle(145, 60, 4, 0xFFFF);
        canvas->setCursor(185, 40); canvas->print("UP");
        canvas->setTextSize(2); canvas->setCursor(75, 105);
        canvas->setTextColor(0x07E0); canvas->print("ADVENTURE");
        canvas->fillRoundRect(80, 155, 160, 45, 10, 0x03E0);
        canvas->drawRoundRect(80, 155, 160, 45, 10, 0xFFFF);
        canvas->setTextColor(0xFFFF); canvas->setCursor(130, 170); canvas->print("PLAY");
        return;
    }

    canvas->fillScreen(0x5AEB); // Небо[cite: 4]

    // Отрисовка уровня
    for (int ty = 0; ty < mapH; ty++) {
        for (int tx = (camX / 20); tx < (camX + 340) / 20; tx++) {
            if (tx >= mapW) continue;
            int dx = tx * 20 - (int)camX; int dy = ty * 20;
            if (levelMap[ty][tx] == TILE_WALL) {
                canvas->fillRect(dx, dy, 20, 20, 0x8400);
                canvas->drawRect(dx, dy, 20, 20, 0x4200);
            }
            else if (levelMap[ty][tx] == TILE_COIN) {
                drawDetailedCoin(dx + 10, dy + 10); // Красивая монетка[cite: 4]
            }
        }
    }

    // Враги (можно будет тоже заменить на "Злых Спотов")[cite: 4]
    for (int i = 0; i < enemyCount; i++) {
        canvas->fillTriangle(enemies[i].x - camX, enemies[i].y - 8, enemies[i].x - camX - 8, enemies[i].y + 8, enemies[i].x - camX + 8, enemies[i].y + 8, 0xF800);
    }

    // РИСУЕМ КРУТОГО СПОТА[cite: 4]
    drawSpot(x - camX, y, flip);

    if (status == GS_PAUSED) {
        canvas->fillRect(80, 80, 160, 60, 0x2104);
        canvas->setCursor(110, 100); canvas->setTextSize(2); canvas->print("PAUSE");
    }
    else if (status == GS_DEAD) {
        canvas->fillScreen(0x0000);
        canvas->setCursor(80, 110); canvas->setTextSize(3); canvas->print("GAME OVER");
    }

    // UI элементы[cite: 4]
    canvas->drawRoundRect(10, 185, 50, 45, 5, 0xFFFF);
    canvas->drawRoundRect(70, 185, 50, 45, 5, 0xFFFF);
    canvas->drawRoundRect(210, 185, 100, 45, 10, 0xFFFF);
    canvas->fillRect(285, 5, 30, 20, 0xF800);
    canvas->setCursor(295, 10); canvas->setTextSize(1); canvas->setTextColor(0xFFFF); canvas->print("X");

    // Иконки вместо текста (опционально)[cite: 4]
    canvas->setCursor(10, 5); canvas->printf("XP: %d  LIVES: %d", score, lives);
}
