#include "Global.h"
#include "MutantBlob.h"
#include "Display.h"
#include <XPT2046_Touchscreen.h>

extern XPT2046_Touchscreen ts;
extern void getPoint(int &x, int &y);
extern void showMenu(bool fullRedraw);
extern unsigned long lastTouch;

// ============================================================
//  MUTANT BLOBS ATTACK v2
//  - Спрайты вместо простых кружков (глаза, шипы, блеск)
//  - Система волн (WAVE) вместо рандомных порогов очков
//  - ИСПРАВЛЕН баг: враги бесконечно плодились, когда игрок
//    был больше них (score%50==0 срабатывал каждый кадр,
//    т.к. +50 за съеденного врага сохраняло кратность 50)
// ============================================================

#define FIELD_TOP    22
#define FIELD_BOTTOM 240
#define FIELD_LEFT   0
#define FIELD_RIGHT  320

#define FOOD_COUNT      10
#define ENEMY_MAX       6
#define ENEMY_BASE      2
#define PLAYER_START_R  8
#define PLAYER_MAX_R    42
#define PLAYER_MIN_R    5

#define WAVE_BANNER_FRAMES 60

enum BlobGameState { BG_PLAY, BG_OVER };

struct Blob {
    float x, y;
    float r;
    float vx, vy; // только для врагов
    bool alive;
};

static BlobGameState bgState;
static float px, py, pr;
static float targetX, targetY;
static bool hasTarget;

static Blob food[FOOD_COUNT];
static Blob enemies[ENEMY_MAX];
static int enemyCount;

static int score;
static int invuln;

static int wave;
static int waveBanner; // кадров баннера осталось

// ---------------- Утилиты ----------------

static float dist(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return sqrtf(dx * dx + dy * dy);
}

static void spawnFood(int i) {
    food[i].x = random(FIELD_LEFT + 6, FIELD_RIGHT - 6);
    food[i].y = random(FIELD_TOP + 6, FIELD_BOTTOM - 6);
    food[i].r = 3;
    food[i].alive = true;
}

static void spawnEnemy(int i) {
    enemies[i].x = random(FIELD_LEFT + 20, FIELD_RIGHT - 20);
    enemies[i].y = random(FIELD_TOP + 20, FIELD_BOTTOM - 20);
    enemies[i].r = random(12, 20);

    float ang = random(0, 628) / 100.0f; // 0..2*PI
    float speed = 0.6f + (wave - 1) * 0.25f; // скорость растёт от волны, не от очков
    enemies[i].vx = cosf(ang) * speed;
    enemies[i].vy = sinf(ang) * speed;
    enemies[i].alive = true;
}

static void startWave(int w) {
    wave = w;
    waveBanner = WAVE_BANNER_FRAMES;

    enemyCount = min(ENEMY_BASE + (wave - 1), ENEMY_MAX);
    for (int i = 0; i < enemyCount; i++) spawnEnemy(i);
    for (int i = enemyCount; i < ENEMY_MAX; i++) enemies[i].alive = false;
}

// ---------------- Инициализация ----------------

void initMutantBlob() {
    bgState = BG_PLAY;

    px = (FIELD_LEFT + FIELD_RIGHT) / 2.0f;
    py = (FIELD_TOP + FIELD_BOTTOM) / 2.0f;
    pr = PLAYER_START_R;

    hasTarget = false;
    score = 0;
    invuln = 0;

    for (int i = 0; i < FOOD_COUNT; i++) spawnFood(i);

    startWave(1);
}

// ---------------- Обновление ----------------

static void updatePlayer() {
    if (!hasTarget) return;

    float dx = targetX - px;
    float dy = targetY - py;
    float d = sqrtf(dx * dx + dy * dy);

    float speed = 2.2f;
    if (d < speed) {
        px = targetX;
        py = targetY;
        hasTarget = false;
    } else {
        px += dx / d * speed;
        py += dy / d * speed;
    }

    if (px < pr) px = pr;
    if (px > FIELD_RIGHT - pr) px = FIELD_RIGHT - pr;
    if (py < FIELD_TOP + pr) py = FIELD_TOP + pr;
    if (py > FIELD_BOTTOM - pr) py = FIELD_BOTTOM - pr;
}

static void updateEnemies() {
    for (int i = 0; i < enemyCount; i++) {
        if (!enemies[i].alive) continue;

        enemies[i].x += enemies[i].vx;
        enemies[i].y += enemies[i].vy;

        if (enemies[i].x < enemies[i].r || enemies[i].x > FIELD_RIGHT - enemies[i].r) {
            enemies[i].vx *= -1;
        }
        if (enemies[i].y < FIELD_TOP + enemies[i].r || enemies[i].y > FIELD_BOTTOM - enemies[i].r) {
            enemies[i].vy *= -1;
        }
    }
}

static void checkCollisions() {
    // Еда
    for (int i = 0; i < FOOD_COUNT; i++) {
        if (!food[i].alive) continue;
        if (dist(px, py, food[i].x, food[i].y) < pr + food[i].r) {
            food[i].alive = false;
            spawnFood(i);
            score += 10;
            if (pr < PLAYER_MAX_R) pr += 1.0f;
        }
    }

    // Враги
    if (invuln > 0) {
        invuln--;
    } else {
        for (int i = 0; i < enemyCount; i++) {
            if (!enemies[i].alive) continue;
            float d = dist(px, py, enemies[i].x, enemies[i].y);
            if (d < pr + enemies[i].r) {
                if (pr > enemies[i].r + 3) {
                    // Съели врага - просто респавним его (НЕ увеличиваем enemyCount!)
                    score += 50;
                    if (pr < PLAYER_MAX_R) pr += 3.0f;
                    spawnEnemy(i);
                } else {
                    pr -= 5.0f;
                    invuln = 25;
                    if (pr < PLAYER_MIN_R) {
                        bgState = BG_OVER;
                    }
                }
            }
        }
    }

    // ---- Прогресс волны ----
    // Новая волна каждые 100 очков. enemyCount растёт ТОЛЬКО здесь,
    // и только когда баннер предыдущей волны уже погас - так
    // не будет повторных срабатываний на одном и том же score.
    int targetWave = (score / 100) + 1;
    if (targetWave > wave && waveBanner == 0) {
        startWave(targetWave);
    }

    if (waveBanner > 0) waveBanner--;
}

// ---------------- Спрайты ----------------

// Глаза, смотрящие в направлении (dx,dy) (нормализованный вектор)
static void drawEyes(int x, int y, int r, float dx, float dy, uint16_t pupilColor) {
    auto* canvas = bDisp.getCanvas();
    int eyeR = max(2, r / 3);
    float ex = x + dx * r * 0.35f;
    float ey = y + dy * r * 0.35f;

    float pX = -dy, pY = dx;
    float off = r * 0.4f;

    for (int s = -1; s <= 1; s += 2) {
        int gx = (int)(ex + pX * off * s);
        int gy = (int)(ey + pY * off * s);
        canvas->fillCircle(gx, gy, eyeR, 0xFFFF);
        canvas->fillCircle(gx + (int)(dx * eyeR * 0.5f), gy + (int)(dy * eyeR * 0.5f),
                            max(1, eyeR / 2), pupilColor);
    }
}

static void drawPlayerSprite() {
    auto* canvas = bDisp.getCanvas();
    bool blink = (invuln > 0) && ((invuln / 4) % 2 == 0);
    if (blink) return;

    canvas->fillCircle((int)px, (int)py, (int)pr, 0x07E0);
    canvas->drawCircle((int)px, (int)py, (int)pr, 0xFFFF);

    float dx = 0, dy = -1;
    if (hasTarget) {
        dx = targetX - px; dy = targetY - py;
        float d = sqrtf(dx * dx + dy * dy);
        if (d > 0.01f) { dx /= d; dy /= d; }
    }
    drawEyes((int)px, (int)py, (int)pr, dx, dy, 0x0000);
}

static void drawFoodSprite(const Blob &b) {
    auto* canvas = bDisp.getCanvas();
    canvas->fillCircle((int)b.x, (int)b.y, (int)b.r, 0xFFE0);
    canvas->fillCircle((int)(b.x - b.r * 0.4f), (int)(b.y - b.r * 0.4f), 1, 0xFFFF);
}

static void drawEnemySprite(const Blob &b) {
    auto* canvas = bDisp.getCanvas();
    bool dangerous = (pr <= b.r + 3);
    uint16_t body = dangerous ? 0xF800 : 0xFD20; // красный опасный / оранж съедобный

    if (dangerous) {
        int spikes = 8;
        for (int k = 0; k < spikes; k++) {
            float ang = (2.0f * PI * k) / spikes;
            int sx = (int)(b.x + cosf(ang) * (b.r + 4));
            int sy = (int)(b.y + sinf(ang) * (b.r + 4));
            canvas->fillCircle(sx, sy, max(2, (int)(b.r / 5)), 0x8000);
        }
    }

    canvas->fillCircle((int)b.x, (int)b.y, (int)b.r, body);
    canvas->drawCircle((int)b.x, (int)b.y, (int)b.r, 0xFFFF);

    float d = sqrtf(b.vx * b.vx + b.vy * b.vy);
    float dx = (d > 0.01f) ? b.vx / d : 0;
    float dy = (d > 0.01f) ? b.vy / d : -1;
    drawEyes((int)b.x, (int)b.y, (int)b.r, dx, dy, dangerous ? 0xF800 : 0x0000);
}

// ---------------- Рендер ----------------

static void drawPlayField() {
    auto* canvas = bDisp.getCanvas();
    canvas->fillScreen(0x0000);

    canvas->fillRect(0, 0, 320, FIELD_TOP, 0x10A2);
    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(1);
    canvas->setCursor(4, 6);
    canvas->printf("SCORE:%d  SIZE:%d  WAVE:%d", score, (int)pr, wave);

    canvas->setCursor(290, 6);
    canvas->print("X");

    for (int i = 0; i < FOOD_COUNT; i++) {
        if (food[i].alive) drawFoodSprite(food[i]);
    }

    for (int i = 0; i < enemyCount; i++) {
        if (enemies[i].alive) drawEnemySprite(enemies[i]);
    }

    drawPlayerSprite();

    if (waveBanner > 0) {
        canvas->fillRect(80, 90, 160, 40, 0x0000);
        canvas->drawRect(80, 90, 160, 40, 0xFFFF);
        canvas->setTextSize(2);
        canvas->setTextColor(0x07FF);
        canvas->setCursor(100, 102);
        canvas->printf("WAVE %d", wave);
    }

    bDisp.update();
}

static void drawGameOver() {
    auto* canvas = bDisp.getCanvas();
    canvas->fillScreen(0x0000);

    canvas->setTextColor(0xF800);
    canvas->setTextSize(3);
    canvas->setCursor(60, 60);
    canvas->print("GAME OVER");

    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(2);
    canvas->setCursor(90, 110);
    canvas->printf("SCORE: %d", score);
    canvas->setCursor(90, 135);
    canvas->printf("WAVE:  %d", wave);

    canvas->setTextSize(1);
    canvas->setCursor(70, 170);
    canvas->print("Tap anywhere to retry");

    canvas->fillRect(120, 195, 80, 30, 0x0500);
    canvas->drawRect(120, 195, 80, 30, 0xFFFF);
    canvas->setCursor(140, 205);
    canvas->print("RETRY");

    canvas->fillRect(10, 195, 90, 30, 0xB000);
    canvas->drawRect(10, 195, 90, 30, 0xFFFF);
    canvas->setCursor(25, 205);
    canvas->print("EXIT");

    bDisp.update();
}

// ---------------- Тач ----------------

static void touchPlay() {
    if (!ts.touched()) return;

    int x, y;
    getPoint(x, y);

    if (y < FIELD_TOP && x > 280) {
        if (millis() - lastTouch > 250) {
            lastTouch = millis();
            showMenu(true);
        }
        return;
    }

    if (y >= FIELD_TOP) {
        targetX = x;
        targetY = y;
        hasTarget = true;
    }
}

static void touchGameOver() {
    if (!ts.touched()) return;
    if (millis() - lastTouch < 250) return;

    int x, y;
    getPoint(x, y);
    lastTouch = millis();

    if (y > 195 && y < 225 && x > 120 && x < 200) {
        initMutantBlob();
        return;
    }

    if (y > 195 && y < 225 && x > 10 && x < 100) {
        showMenu(true);
        return;
    }
}

// ---------------- Главный тик ----------------

void tickMutantBlob() {
    if (bgState == BG_PLAY) {
        touchPlay();
        updatePlayer();
        updateEnemies();
        checkCollisions();
        drawPlayField();
    } else {
        drawGameOver();
        touchGameOver();
    }
}
