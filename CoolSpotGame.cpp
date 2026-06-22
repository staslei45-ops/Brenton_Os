// ================================================================
//  CoolSpotGame — переписан под Engine2D
//  Было: физика, коллизии, рендер вручную в одной куче.
//  Стало: Entity/Scene/Renderer/Input/FrameTimer из Engine2D.
//
//  Оптимизации против лагов:
//  - sin() на монетках убран, заменён простым XOR-миганием
//  - Тайлмап рисуется только видимая область (как было)
//  - FrameTimer(30) — не тратим кадры впустую
//  - Физика и коллизии игрока через Transform.vel
//  - Враги — Entity группы GRP_ENEMY в Scene
// ================================================================

#include "CoolSpotGame.h"
#include "Display.h"
#include "Global.h"
#include <XPT2046_Touchscreen.h>

extern BrentonDisplay bDisp;
extern XPT2046_Touchscreen ts;
extern void getPoint(int &x, int &y);
extern unsigned long lastTouch;

// ----------------------------------------------------------------
//  Тайловая карта
// ----------------------------------------------------------------
static const uint8_t defaultMap[MAP_H][MAP_W] = {
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

// ----------------------------------------------------------------
//  Инициализация
// ----------------------------------------------------------------
void CoolSpotGame::init() {
    isRunning   = true;
    status      = GS_MENU;
    score       = 0;
    lives       = 3;
    enemyCount  = 3;
    camX        = 0;
    invuln      = 0;

    // Копируем карту (она изменяется при сборе монет)
    memcpy(levelMap, defaultMap, sizeof(levelMap));

    // Настраиваем движок
    rend.setCanvas(bDisp.getCanvas());
    ftimer = FrameTimer(30);
    scene.setRenderer(&rend);
    scene.killAll();

    playerId = ENTITY_NONE;
    spawnEnemies();
}

void CoolSpotGame::resetPlayer() {
    // Убиваем старого игрока если есть
    if (playerId != ENTITY_NONE) scene.kill(playerId);

    Entity* p    = scene.spawn(GRP_PLAYER);
    p->transform = Transform(40, 100, 9.f);
    p->transform.vel = Vec2(0, 0);
    p->sprite    = Sprite::circle(0xF800, 0x0000, true);
    p->userData  = (void*)0; // 0 = не перевёрнут
    playerId     = p->id;
    invuln       = 0;
}

void CoolSpotGame::spawnEnemies() {
    // Убиваем старых врагов
    scene.killAll(GRP_ENEMY);

    struct { float x, y, minX, maxX; } edata[] = {
        {200, 190, 150, 300},
        {500, 170, 400, 600},
        {750, 190, 700, 850},
    };
    for (int i = 0; i < enemyCount; i++) {
        Entity* e    = scene.spawn(GRP_ENEMY);
        e->transform = Transform(edata[i].x, edata[i].y, 8.f);
        e->transform.vel = Vec2(1.8f, 0);
        // Храним minX/maxX в userData как упакованные int16
        // (простой трюк без доп. аллокаций)
        uint32_t packed = ((uint32_t)(int16_t)edata[i].minX << 16) | (uint16_t)(int16_t)edata[i].maxX;
        e->userData  = (void*)(uintptr_t)packed;
        e->sprite    = Sprite::circle(0xF800, 0x0000, true);
    }
}

// ----------------------------------------------------------------
//  Тайловые коллизии
// ----------------------------------------------------------------
bool CoolSpotGame::tileAt(float wx, float wy) {
    int tx = (int)wx / TILE_SIZE;
    int ty = (int)wy / TILE_SIZE;
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return true;
    return levelMap[ty][tx] == TILE_WALL;
}

void CoolSpotGame::resolvePlayerTiles(Entity* p) {
    Vec2& pos = p->transform.pos;
    Vec2& vel = p->transform.vel;
    float r   = p->transform.radius;
    bool grounded = false;

    // Горизонталь
    float nx = pos.x + vel.x;
    if (!tileAt(nx + (vel.x > 0 ? r : -r), pos.y))
        pos.x = nx;
    else
        vel.x = 0;

    // Вертикаль
    pos.y += vel.y;
    if (vel.y > 0) {
        if (tileAt(pos.x, pos.y + r)) {
            pos.y = floorf((pos.y + r) / TILE_SIZE) * TILE_SIZE - r;
            vel.y = 0;
            grounded = true;
        }
    } else if (vel.y < 0) {
        if (tileAt(pos.x, pos.y - r)) {
            vel.y = 0;
        }
    }

    // Сохраняем grounded в userData: бит 31
    uintptr_t ud = (uintptr_t)p->userData;
    if (grounded) ud |= 0x80000000;
    else          ud &= ~0x80000000;
    p->userData = (void*)ud;
}

// ----------------------------------------------------------------
//  Логика (update)
// ----------------------------------------------------------------
void CoolSpotGame::update() {
    // Читаем тач ОДИН РАЗ — XPT2046 не любит двойное чтение за один цикл
    bool tch = ts.touched();
    int rx = 160, ry = 120;
    if (tch) getPoint(rx, ry);
    input.update(tch, rx, ry);

    // Кнопка X — всегда работает на любом экране
    if (input.justPressed && rx > 275 && ry < 35) {
        isRunning = false;
        return;
    }

    if (status == GS_MENU || status == GS_DEAD) {
        if (input.justPressed && rx > 80 && rx < 240 && ry > 150) {
            score = 0; lives = 3;
            memcpy(levelMap, defaultMap, sizeof(levelMap));
            scene.killAll();
            spawnEnemies();
            resetPlayer();
            status = GS_PLAYING;
        }
        return;
    }

    if (status == GS_PAUSED) {
        if (input.justPressed && ry > 45) { status = GS_PLAYING; }
        return;
    }

    if (!ftimer.ready()) return;

    Entity* player = scene.get(playerId);
    if (!player) return;

    bool grounded = (uintptr_t)player->userData & 0x80000000;

    if (input.touched && input.pos.y < 45 && input.pos.x > 120 && input.pos.x < 200) {
        status = GS_PAUSED;
        
        return;
    }

    if (input.touched) {
        float px = input.pos.x;
        float py = input.pos.y;
        if (py > 45) {
            if (px > 160) {
                // Прыжок — правая половина
                if (grounded) player->transform.vel.y = -8.2f;
            } else if (px < 80) {
                // Влево
                player->transform.vel.x -= 0.9f;
                player->userData = (void*)((uintptr_t)player->userData | 1); // flip
            } else {
                // Вправо
                player->transform.vel.x += 0.9f;
                player->userData = (void*)((uintptr_t)player->userData & ~1); // no flip
            }
        }
    }

    // ---- Физика игрока ----
    player->transform.vel.y += 0.45f; // гравитация
    player->transform.vel.x *= 0.85f; // трение

    resolvePlayerTiles(player);

    // ---- Сбор монет ----
    Vec2 pp = player->transform.pos;
    int mapTx = (int)pp.x / TILE_SIZE;
    int mapTy = (int)pp.y / TILE_SIZE;
    if (mapTx >= 0 && mapTx < MAP_W && mapTy >= 0 && mapTy < MAP_H) {
        if (levelMap[mapTy][mapTx] == TILE_COIN) {
            levelMap[mapTy][mapTx] = TILE_EMPTY;
            score += 10;
        }
    }

    // ---- Враги ----
    for (int i = 0; i < scene.count; i++) {
        Entity& e = scene.entities[i];
        if (!e.alive || e.group != GRP_ENEMY) continue;

        uint32_t packed = (uint32_t)(uintptr_t)e.userData;
        int16_t minX = (int16_t)(packed >> 16);
        int16_t maxX = (int16_t)(packed & 0xFFFF);

        e.transform.pos.x += e.transform.vel.x;
        if (e.transform.pos.x < minX || e.transform.pos.x > maxX)
            e.transform.vel.x *= -1;

        // Столкновение с игроком (с неуязвимостью)
        if (invuln == 0 && Collider::circle_circle(*player, e)) {
            lives--;
            invuln = 40;
            if (lives <= 0) {
                status = GS_DEAD;
                return;
            }
            resetPlayer();
            player = scene.get(playerId);
            if (!player) return;
        }
    }
    if (invuln > 0) invuln--;

    // ---- Камера (плавная) ----
    float targetCam = player->transform.pos.x - 160;
    camX = camX * 0.88f + targetCam * 0.12f;
    if (camX < 0) camX = 0;
    float maxCamX = (MAP_W * TILE_SIZE) - 320;
    if (camX > maxCamX) camX = maxCamX;
}

// ----------------------------------------------------------------
//  Спрайты
// ----------------------------------------------------------------
void CoolSpotGame::drawSpot(int sx, int sy, bool flipped) {
    auto* c = bDisp.getCanvas();
    c->fillCircle(sx, sy, 9, 0xF800);
    c->drawCircle(sx, sy, 9, 0x0000);
    c->fillCircle(sx + (flipped ? 7 : -7), sy + 3, 3, 0xFFFF); // перчатка
    int ox = flipped ? sx - 8 : sx;
    c->fillRoundRect(ox, sy - 4, 8, 5, 2, 0x0000); // очки
    c->drawPixel(ox + 2, sy - 3, 0xFFFF);
    c->drawPixel(ox + 6, sy - 3, 0xFFFF);
    c->fillRect(sx - 4, sy + 7, 8, 3, 0xFFFF); // обувь

    // Мигание при неуязвимости
    if (invuln > 0 && (invuln / 4) % 2 == 0) {
        c->drawCircle(sx, sy, 11, 0xFFFF);
    }
}

void CoolSpotGame::drawEnemy(int sx, int sy) {
    auto* c = bDisp.getCanvas();
    // Злой треугольный Спот-враг
    c->fillTriangle(sx, sy - 10, sx - 9, sy + 8, sx + 9, sy + 8, 0xF800);
    c->fillCircle(sx - 3, sy, 2, 0x0000); // глаз
    c->fillCircle(sx + 3, sy, 2, 0x0000);
}

// ----------------------------------------------------------------
//  Тайлмап
// ----------------------------------------------------------------
void CoolSpotGame::drawTilemap() {
    auto* c = bDisp.getCanvas();
    int startTx = (int)camX / TILE_SIZE;
    int endTx   = startTx + (320 / TILE_SIZE) + 2;
    if (endTx > MAP_W) endTx = MAP_W;

    // Заменяем sin() на простое мигание по millis()
    bool coinBlink = (millis() / 300) % 2;

    for (int ty = 0; ty < MAP_H; ty++) {
        for (int tx = startTx; tx < endTx; tx++) {
            int dx = tx * TILE_SIZE - (int)camX;
            int dy = ty * TILE_SIZE;

            if (levelMap[ty][tx] == TILE_WALL) {
                c->fillRect(dx, dy, TILE_SIZE, TILE_SIZE, 0x8400);
                c->drawRect(dx, dy, TILE_SIZE, TILE_SIZE, 0x4200);
            } else if (levelMap[ty][tx] == TILE_COIN) {
                int cx = dx + 10, cy = dy + 10;
                c->fillCircle(cx, cy, 6, 0x9480);
                c->fillCircle(cx, cy, 5, 0xFD20);
                c->fillCircle(cx, cy, 3, 0xFFE0);
                if (coinBlink) c->fillRect(cx - 1, cy - 3, 2, 5, 0xFFFF);
            }
        }
    }
}

// ----------------------------------------------------------------
//  Рендер HUD / экраны
// ----------------------------------------------------------------
void CoolSpotGame::drawHUD() {
    auto* c = bDisp.getCanvas();
    c->fillRect(0, 0, 320, 22, 0x10A2);
    c->setTextColor(0xFFFF);
    c->setTextSize(1);
    c->setCursor(6, 7);
    c->printf("XP: %d   LIVES: %d", score, lives);
    // Кнопка PAUSE
    c->fillRect(140, 3, 40, 16, 0x4208);
    c->setCursor(148, 7);
    c->print("MENU");
    // Кнопка X
    c->fillRect(285, 3, 30, 16, 0xB000);
    c->setCursor(296, 7);
    c->print("X");
}

void CoolSpotGame::drawMenu() {
    auto* c = bDisp.getCanvas();
    c->fillScreen(0x2104);
    c->setTextSize(6); c->setTextColor(0xFFFF);
    c->setCursor(80, 40); c->print("7");
    c->fillCircle(150, 65, 22, 0xF800);
    c->fillCircle(145, 60, 4, 0xFFFF);
    c->setCursor(185, 40); c->print("UP");
    c->setTextSize(2); c->setTextColor(0x07E0);
    c->setCursor(75, 105); c->print("ADVENTURE");
    c->fillRoundRect(80, 155, 160, 45, 10, 0x03E0);
    c->drawRoundRect(80, 155, 160, 45, 10, 0xFFFF);
    c->setTextColor(0xFFFF); c->setTextSize(2);
    c->setCursor(125, 170); c->print("PLAY");

    // Кнопка выхода
    c->fillRect(285, 3, 30, 16, 0xB000);
    c->setTextSize(1);
    c->setCursor(296, 7); c->print("X");
}

void CoolSpotGame::drawPause() {
    auto* c = bDisp.getCanvas();
    c->fillRect(70, 75, 180, 70, 0x2104);
    c->drawRect(70, 75, 180, 70, 0xFFFF);
    c->setTextColor(0xFFFF); c->setTextSize(2);
    c->setCursor(115, 100); c->print("PAUSE");
    c->setTextSize(1);
    c->setCursor(90, 128); c->print("Tap screen to continue");
}

void CoolSpotGame::drawGameOver() {
    auto* c = bDisp.getCanvas();
    c->fillScreen(0x0000);
    c->setTextColor(0xF800); c->setTextSize(3);
    c->setCursor(30, 60); c->print("GAME OVER");
    c->setTextColor(0xFFFF); c->setTextSize(2);
    c->setCursor(90, 110); c->printf("XP: %d", score);
    c->fillRoundRect(80, 155, 160, 45, 10, 0x03E0);
    c->drawRoundRect(80, 155, 160, 45, 10, 0xFFFF);
    c->setTextColor(0xFFFF); c->setCursor(115, 170); c->print("RETRY");
    c->fillRect(285, 3, 30, 16, 0xB000);
    c->setTextSize(1); c->setCursor(296, 7); c->print("X");
}

// ----------------------------------------------------------------
//  Главный render()
// ----------------------------------------------------------------
void CoolSpotGame::render() {
    auto* c = bDisp.getCanvas();

    if (status == GS_MENU) {
        drawMenu();
        bDisp.update();
        return;
    }

    if (status == GS_DEAD) {
        drawGameOver();
        bDisp.update();
        return;
    }

    // FPS ограничивается только в update()

    c->fillScreen(0x5AEB); // Небо

    drawTilemap();

    // Враги
    for (int i = 0; i < scene.count; i++) {
        Entity& e = scene.entities[i];
        if (!e.alive || e.group != GRP_ENEMY) continue;
        drawEnemy((int)(e.transform.pos.x - camX), (int)e.transform.pos.y);
    }

    // Игрок
    Entity* player = scene.get(playerId);
    if (player) {
        bool flipped = (uintptr_t)player->userData & 1;
        drawSpot((int)(player->transform.pos.x - camX), (int)player->transform.pos.y, flipped);
    }

    drawHUD();

    if (status == GS_PAUSED) drawPause();

    bDisp.update();
}
