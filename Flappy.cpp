// ================================================================
//  Flappy Bird — чистый ремейк на Engine2D
//  Дизайн: день/ночь смена, параллакс-фон, анимированная птица,
//          детализированные трубы с шапкой, земля внизу
// ================================================================

#include "Global.h"
#include "Flappy.h"
#include "Engine2D.h"
#include "Display.h"
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>

extern XPT2046_Touchscreen ts;
extern void getPoint(int &x, int &y);
extern void showMenu(bool);

// ========== КОНСТАНТЫ ==========
#define SCREEN_W        320
#define SCREEN_H        240
#define HUD_H           24
#define GROUND_H        18

// Физика
#define GRAVITY         0.45f
#define JUMP_FORCE      -6.0f
#define MAX_FALL_SPEED  11.0f
#define PIPE_SPEED      3.0f
#define PIPE_GAP        80
#define PIPE_WIDTH      46
#define PIPE_SPACING    155
#define PIPE_CAP_H      10
#define PIPE_CAP_W      52

// Птица
#define BIRD_W          22
#define BIRD_H          16
#define BIRD_RADIUS     8

// Группы
#define GROUP_PIPE      0
#define GROUP_PARTICLE  1
#define GROUP_CLOUD     2
#define GROUP_GROUND    3

// Цвета (RGB565)
#define COL_SKY_DAY     0x867F   // #87CEEB светло-голубой
#define COL_SKY_NIGHT   0x0829   // тёмно-синий
#define COL_PIPE_BODY   0x2CA5   // зеленый
#define COL_PIPE_DARK   0x1443   // темнее для тени
#define COL_PIPE_LIGHT  0x5F29   // светлее для блика
#define COL_PIPE_CAP    0x3566   // шапка трубы
#define COL_PIPE_EDGE   0x03E0   // обводка
#define COL_GROUND_TOP  0xD6A5   // песок верх
#define COL_GROUND_BOT  0x8423   // земля низ
#define COL_GROUND_LINE 0xA541   // полоска на земле
#define COL_BIRD_BODY   0xFEE0   // жёлтый
#define COL_BIRD_WING   0xFD00   // оранжевый
#define COL_BIRD_BELLY  0xFFFF   // белый живот
#define COL_BIRD_EYE    0xFFFF   // белый глаз
#define COL_BIRD_PUPIL  0x0000   // зрачок
#define COL_BIRD_BEAK   0xFC00   // красно-оранжевый клюв
#define COL_BIRD_OUTLINE 0xC500  // обводка
#define COL_HUD_BG      0x0000   // чёрный HUD
#define COL_SCORE_TXT   0xFFFF   // белый текст
#define COL_CLOUD       0xFFFF   // белые облака

// ========== СОСТОЯНИЕ ==========
static Scene      scene;
static Renderer   rend;
static Input      input;
static FrameTimer ftimer(30);

static Preferences prefs;
static int   score        = 0;
static int   highScore    = 0;
static bool  isGameOver   = false;
static bool  isStartScreen = true;
static float birdX, birdY, birdVY;
static float birdAngle    = 0;      // наклон в градусах (для visual)
static int   wingFrame    = 0;      // 0,1,2 — кадр крыла
static int   wingTimer    = 0;
static float groundOffset = 0;      // скролл земли
static int   flashFrames  = 0;      // вспышка при смерти
static int   startBobTimer = 0;     // покачивание на старте
static bool  dayMode      = true;

// Следующая X для труб
static float nextPipeX;

struct Pipe {
    float x, gapY;
    bool  scored;
    bool  alive;
};
static Pipe pipes[4];
static int  pipeCount = 0;

// ========== ВСПОМОГАТЕЛЬНЫЕ ==========
static uint16_t lerpColor(uint16_t a, uint16_t b, float t) {
    uint8_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    uint8_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    uint8_t r = ar + (int)((br - ar) * t);
    uint8_t g = ag + (int)((bg - ag) * t);
    uint8_t bv= ab + (int)((bb - ab) * t);
    return (r << 11) | (g << 5) | bv;
}

// ========== ЧАСТИЦЫ ==========
static void spawnParticles(float px, float py) {
    for (int i = 0; i < 14; i++) {
        Entity* p = scene.spawn(GROUP_PARTICLE);
        if (!p) continue;
        p->transform = Transform(px, py, 3);
        uint16_t cols[] = { 0xFFE0, 0xFD20, 0xF800, 0xFFFF, 0xFD00 };
        p->sprite = Sprite::circle(cols[random(5)]);
        float angle = random(628) / 100.0f;
        float speed = random(10, 40) / 10.0f;
        p->transform.vel = Vec2(cosf(angle) * speed, sinf(angle) * speed);
        p->health = 20;
    }
}

static void updateParticles() {
    for (int i = 0; i < scene.count; i++) {
        Entity& p = scene.entities[i];
        if (!p.alive || p.group != GROUP_PARTICLE) continue;
        p.health -= 1.0f;
        if (p.health <= 0) { scene.kill(p.id); continue; }
        p.transform.vel.y += 0.15f;
        p.transform.pos += p.transform.vel;
        p.transform.radius = p.health / 7.0f;
    }
}

// ========== ТРУБЫ ==========
static void addPipe(float x) {
    if (pipeCount >= 4) return;
    int i = pipeCount++;
    pipes[i].x      = x;
    pipes[i].gapY   = random(50, SCREEN_H - GROUND_H - PIPE_GAP - 50);
    pipes[i].scored = false;
    pipes[i].alive  = true;
}

static void drawPipe(Adafruit_GFX* cv, float x, float gapY) {
    // === Верхняя труба ===
    int topH = (int)gapY;
    int px   = (int)x - PIPE_WIDTH / 2;

    // Тело трубы (с тенью слева и бликом справа)
    cv->fillRect(px, 0, PIPE_WIDTH, topH - PIPE_CAP_H, COL_PIPE_BODY);
    // Тень (левая полоска)
    cv->fillRect(px, 0, 4, topH - PIPE_CAP_H, COL_PIPE_DARK);
    // Блик (правая полоска)
    cv->fillRect(px + PIPE_WIDTH - 5, 0, 5, topH - PIPE_CAP_H, COL_PIPE_LIGHT);

    // Шапка трубы (шире)
    int capX = (int)x - PIPE_CAP_W / 2;
    cv->fillRect(capX, topH - PIPE_CAP_H, PIPE_CAP_W, PIPE_CAP_H, COL_PIPE_CAP);
    cv->fillRect(capX, topH - PIPE_CAP_H, 4, PIPE_CAP_H, COL_PIPE_DARK);
    cv->fillRect(capX + PIPE_CAP_W - 5, topH - PIPE_CAP_H, 5, PIPE_CAP_H, COL_PIPE_LIGHT);
    cv->drawRect(capX, topH - PIPE_CAP_H, PIPE_CAP_W, PIPE_CAP_H, COL_PIPE_EDGE);

    // === Нижняя труба ===
    int botY  = (int)(gapY + PIPE_GAP);
    int botH  = SCREEN_H - GROUND_H - botY;
    if (botH <= 0) return;

    // Шапка низа
    cv->fillRect(capX, botY, PIPE_CAP_W, PIPE_CAP_H, COL_PIPE_CAP);
    cv->fillRect(capX, botY, 4, PIPE_CAP_H, COL_PIPE_DARK);
    cv->fillRect(capX + PIPE_CAP_W - 5, botY, 5, PIPE_CAP_H, COL_PIPE_LIGHT);
    cv->drawRect(capX, botY, PIPE_CAP_W, PIPE_CAP_H, COL_PIPE_EDGE);

    // Тело
    cv->fillRect(px, botY + PIPE_CAP_H, PIPE_WIDTH, botH - PIPE_CAP_H, COL_PIPE_BODY);
    cv->fillRect(px, botY + PIPE_CAP_H, 4, botH - PIPE_CAP_H, COL_PIPE_DARK);
    cv->fillRect(px + PIPE_WIDTH - 5, botY + PIPE_CAP_H, 5, botH - PIPE_CAP_H, COL_PIPE_LIGHT);
}

static void updatePipes() {
    for (int i = 0; i < pipeCount; i++) {
        if (!pipes[i].alive) continue;
        pipes[i].x -= PIPE_SPEED;

        // Счёт
        if (!pipes[i].scored && pipes[i].x + PIPE_WIDTH / 2 < birdX) {
            pipes[i].scored = true;
            score++;
        }

        // Удалить ушедшую трубу
        if (pipes[i].x + PIPE_CAP_W / 2 < 0) {
            pipes[i] = pipes[--pipeCount];
            i--;
        }
    }

    // Спавн
    nextPipeX -= PIPE_SPEED;
    if (nextPipeX <= SCREEN_W) {
        addPipe(SCREEN_W + PIPE_CAP_W / 2);
        nextPipeX = SCREEN_W + PIPE_SPACING;
    }
}

// ========== ЗЕМЛЯ ==========
static void drawGround(Adafruit_GFX* cv) {
    int gy = SCREEN_H - GROUND_H;
    cv->fillRect(0, gy, SCREEN_W, GROUND_H, COL_GROUND_BOT);
    cv->fillRect(0, gy, SCREEN_W, 6, COL_GROUND_TOP);
    cv->fillRect(0, gy + 6, SCREEN_W, 2, COL_GROUND_LINE);

    // Движущиеся полоски на земле
    for (int x = -(int)groundOffset % 40; x < SCREEN_W; x += 40) {
        cv->fillRect(x, gy + 9, 18, 4, COL_GROUND_LINE);
    }
}

// ========== ОБЛАКА ==========
static void spawnCloud(float x) {
    Entity* c = scene.spawn(GROUP_CLOUD);
    if (!c) return;
    float y = random(30, 100);
    float r = random(12, 22);
    c->transform = Transform(x, y, r);
    c->transform.vel = Vec2(-0.5f, 0);
    c->sprite = Sprite::circle(COL_CLOUD);
    c->health = r; // храним радиус
}

static void updateClouds() {
    static int cTimer = 0;
    cTimer++;
    for (int i = 0; i < scene.count; i++) {
        Entity& c = scene.entities[i];
        if (!c.alive || c.group != GROUP_CLOUD) continue;
        c.transform.pos.x -= 0.4f;
        if (c.transform.pos.x + c.transform.radius + 30 < 0) scene.kill(c.id);
    }
    if (cTimer > 80 && scene.countAlive(GROUP_CLOUD) < 4) {
        cTimer = 0;
        spawnCloud(SCREEN_W + 30);
    }
}

static void drawClouds(Adafruit_GFX* cv) {
    for (int i = 0; i < scene.count; i++) {
        Entity& c = scene.entities[i];
        if (!c.alive || c.group != GROUP_CLOUD) continue;
        int cx = (int)c.transform.pos.x;
        int cy = (int)c.transform.pos.y;
        int r  = (int)c.health;
        // Простые прямоугольники вместо кругов — намного быстрее
        cv->fillRect(cx - r,     cy - r/3, r*2,   r*2/3, 0xFFFF);
        cv->fillRect(cx - r/2,   cy - r,   r,     r*2/3, 0xFFFF);
    }
}

// ========== ПТИЦА ==========
static void drawBird(Adafruit_GFX* cv) {
    int cx = (int)birdX;
    int cy = (int)birdY;

    // Тело — два прямоугольника (быстро)
    cv->fillRect(cx - BIRD_RADIUS, cy - BIRD_RADIUS + 2, BIRD_RADIUS*2, BIRD_RADIUS*2 - 2, COL_BIRD_BODY);
    // Белый живот (маленький прямоугольник)
    cv->fillRect(cx - 2, cy, 8, BIRD_RADIUS - 2, COL_BIRD_BELLY);

    // Крыло
    int wingOff = (wingFrame == 2) ? -4 : (wingFrame == 0) ? 2 : 0;
    cv->fillRect(cx - BIRD_RADIUS - 4, cy + wingOff, 8, 5, COL_BIRD_WING);

    // Глаз (один круг)
    cv->fillCircle(cx + 4, cy - 3, 3, COL_BIRD_EYE);
    cv->fillCircle(cx + 5, cy - 3, 1, COL_BIRD_PUPIL);

    // Клюв (один треугольник)
    cv->fillTriangle(cx + BIRD_RADIUS, cy - 2, cx + BIRD_RADIUS + 6, cy, cx + BIRD_RADIUS, cy + 2, COL_BIRD_BEAK);
}

// ========== КОЛЛИЗИЯ ==========
static bool checkHit() {
    // Потолок
    if (birdY - BIRD_RADIUS < HUD_H) return true;
    // Земля
    if (birdY + BIRD_RADIUS > SCREEN_H - GROUND_H) return true;

    for (int i = 0; i < pipeCount; i++) {
        if (!pipes[i].alive) continue;
        float px = pipes[i].x;
        float gy = pipes[i].gapY;

        // AABB: левый/правый край трубы (с учётом шапки)
        float pLeft  = px - PIPE_CAP_W / 2;
        float pRight = px + PIPE_CAP_W / 2;
        if (birdX + BIRD_RADIUS < pLeft || birdX - BIRD_RADIUS > pRight) continue;

        // Верхняя труба
        if (birdY - BIRD_RADIUS < gy) return true;
        // Нижняя труба
        if (birdY + BIRD_RADIUS > gy + PIPE_GAP) return true;
    }
    return false;
}

// ========== HUD ==========
static void drawHUD(Adafruit_GFX* cv) {
    // Тёмная полоска вверху
    cv->fillRect(0, 0, SCREEN_W, HUD_H, 0x0000);
    cv->drawFastHLine(0, HUD_H, SCREEN_W, 0x39E7);

    // Очки по центру
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", score);
    // Тень текста
    cv->setTextColor(0x4208);
    cv->setTextSize(2);
    int tw = strlen(buf) * 12;
    cv->setCursor(SCREEN_W/2 - tw/2 + 1, 5);
    cv->print(buf);
    // Основной текст
    cv->setTextColor(COL_SCORE_TXT);
    cv->setCursor(SCREEN_W/2 - tw/2, 4);
    cv->print(buf);

    // Рекорд слева
    snprintf(buf, sizeof(buf), "BEST %d", highScore);
    cv->setTextSize(1);
    cv->setTextColor(0xFFE0);
    cv->setCursor(5, 8);
    cv->print(buf);

    // Кнопка выхода справа
    cv->fillRect(SCREEN_W - 28, 2, 26, HUD_H - 4, 0x8000);
    cv->drawRect(SCREEN_W - 28, 2, 26, HUD_H - 4, 0xF800);
    cv->setTextColor(0xFFFF);
    cv->setTextSize(1);
    cv->setCursor(SCREEN_W - 21, 8);
    cv->print("X");
}

static void drawStartScreen(Adafruit_GFX* cv) {
    // Покачивание птицы
    float bob = sinf(startBobTimer * 0.08f) * 4.0f;

    // Рамка в центре
    int bx = SCREEN_W/2 - 70, by = 80;
    cv->fillRoundRect(bx, by, 140, 70, 8, 0x0000);
    cv->drawRoundRect(bx, by, 140, 70, 8, 0xFFFF);
    cv->setTextColor(0xFFFF);
    cv->setTextSize(2);
    cv->setCursor(SCREEN_W/2 - 60, by + 10);
    cv->print("FLAPPY");
    cv->setCursor(SCREEN_W/2 - 42, by + 30);
    cv->print("BIRD");
    cv->setTextSize(1);
    cv->setTextColor(0xFFE0);
    cv->setCursor(SCREEN_W/2 - 38, by + 52);
    cv->print("TAP TO START");

    // Стрелка вниз (анимирована)
    int ay = 165 + (int)(sinf(startBobTimer * 0.12f) * 3);
    cv->fillTriangle(SCREEN_W/2 - 10, ay, SCREEN_W/2 + 10, ay, SCREEN_W/2, ay + 8, 0xFFE0);
}

static void drawGameOver(Adafruit_GFX* cv) {
    // Полупрозрачная рамка
    cv->fillRect(45, 65, 230, 110, 0x0000);
    cv->drawRect(45, 65, 230, 110, 0xFFFF);
    cv->drawRect(47, 67, 226, 106, 0x4208);

    // Заголовок
    cv->setTextColor(0xF800);
    cv->setTextSize(2);
    cv->setCursor(75, 80);
    cv->print("GAME OVER");

    // Разделитель
    cv->drawFastHLine(60, 102, 200, 0x4208);

    // Очки
    cv->setTextSize(1);
    cv->setTextColor(0xFFFF);
    cv->setCursor(65, 110);
    cv->printf("SCORE : %d", score);
    cv->setCursor(65, 125);
    cv->printf("BEST  : %d", highScore);

    // Новый рекорд!
    if (score >= highScore && score > 0) {
        cv->setTextColor(0xFFE0);
        cv->setCursor(140, 110);
        cv->print("NEW!");
    }

    // Подсказка
    cv->setTextColor(0x7BEF);
    cv->setCursor(90, 148);
    cv->print("TAP TO RETRY");
}

// ========== ИНИЦИАЛИЗАЦИЯ ==========
void initFlappy() {
    rend.setCanvas(bDisp.getCanvas());
    scene.setRenderer(&rend);
    scene.killAll();

    score        = 0;
    isGameOver   = false;
    isStartScreen = true;
    flashFrames  = 0;
    groundOffset = 0;
    nextPipeX    = SCREEN_W + 60;
    pipeCount    = 0;
    wingFrame    = 0;
    wingTimer    = 0;
    startBobTimer = 0;
    dayMode      = (random(2) == 0);

    birdX = 80;
    birdY = SCREEN_H / 2;
    birdVY = 0;

    prefs.begin("flappy", true);
    highScore = prefs.getInt("highscore", 0);
    prefs.end();

    // Начальные облака
    scene.killAll();
    for (int i = 0; i < 3; i++) {
        spawnCloud(random(40, SCREEN_W - 40));
    }
}

// ========== ГЛАВНЫЙ ТИК ==========
void tickFlappy() {
    if (!ftimer.ready()) return;

    // Ввод
    bool touched = ts.touched();
    int tx = 160, ty = 120;
    if (touched) getPoint(tx, ty);
    input.update(touched, tx, ty);

    // Кнопка выхода
    if (input.justPressed && input.pos.y < HUD_H && input.pos.x > SCREEN_W - 28) {
        showMenu(true);
        return;
    }

    // ---- ЛОГИКА ----
    startBobTimer++;
    groundOffset += PIPE_SPEED;

    if (isStartScreen) {
        // Лёгкое покачивание птицы на старте
        birdY = SCREEN_H / 2 + sinf(startBobTimer * 0.07f) * 5.0f;
        birdVY = 0;

        if (input.justPressed && input.pos.y > HUD_H) {
            isStartScreen = false;
            birdVY = JUMP_FORCE;
            wingFrame = 2;
        }
    } else if (!isGameOver) {
        // Гравитация
        birdVY += GRAVITY;
        if (birdVY > MAX_FALL_SPEED) birdVY = MAX_FALL_SPEED;

        // Прыжок
        if (input.justPressed && input.pos.y > HUD_H) {
            birdVY = JUMP_FORCE;
            wingFrame = 2;
            // Мини-частицы при прыжке
            for (int i = 0; i < 4; i++) {
                Entity* p = scene.spawn(GROUP_PARTICLE);
                if (!p) continue;
                p->transform = Transform(birdX - 8, birdY, 2);
                p->sprite = Sprite::circle(0xFFE0);
                p->transform.vel = Vec2(random(-15, -5) / 10.0f, random(-10, 10) / 10.0f);
                p->health = 8;
            }
        }

        birdY += birdVY;

        // Обновление
        updatePipes();
        updateClouds();
        updateParticles();

        // Анимация крыла
        wingTimer++;
        if (wingTimer >= 6) {
            wingTimer = 0;
            wingFrame = (wingFrame + 1) % 3;
        }

        // Коллизия
        if (checkHit()) {
            isGameOver = true;
            flashFrames = 10;
            spawnParticles(birdX, birdY);

            if (score > highScore) {
                highScore = score;
                prefs.begin("flappy", false);
                prefs.putInt("highscore", highScore);
                prefs.end();
            }
        }
    } else {
        // Game over: частицы ещё летят
        updateParticles();
        if (flashFrames > 0) flashFrames--;

        // Рестарт
        if (input.justPressed && input.pos.y > HUD_H && flashFrames == 0) {
            scene.killAll();
            pipeCount = 0;
            score = 0;
            isGameOver = false;
            isStartScreen = false;
            birdX = 80;
            birdY = SCREEN_H / 2;
            birdVY = JUMP_FORCE;
            nextPipeX = SCREEN_W + 60;
            groundOffset = 0;
            wingFrame = 2;
            for (int i = 0; i < 3; i++) spawnCloud(random(40, SCREEN_W - 40));
        }
    }

    // ---- РЕНДЕР ----
    Adafruit_GFX* cv = bDisp.getCanvas();

    // Фон
    uint16_t skyColor = dayMode ? COL_SKY_DAY : COL_SKY_NIGHT;
    if (flashFrames > 0) skyColor = 0xFFFF;
    cv->fillScreen(skyColor);

    // Облака (рисуем вручную, не через Scene)
    if (flashFrames == 0) drawClouds(cv);

    // Трубы
    if (!isStartScreen) {
        for (int i = 0; i < pipeCount; i++) {
            if (pipes[i].alive) drawPipe(cv, pipes[i].x, pipes[i].gapY);
        }
    }

    // Земля
    drawGround(cv);

    // Частицы из scene (поверх земли)
    for (int i = 0; i < scene.count; i++) {
        Entity& p = scene.entities[i];
        if (!p.alive || p.group != GROUP_PARTICLE) continue;
        if (p.transform.pos.y > SCREEN_H - GROUND_H) continue;
        cv->fillCircle((int)p.transform.pos.x, (int)p.transform.pos.y,
                       (int)p.transform.radius, p.sprite.color);
    }

    // Птица (только если живая или до смерти)
    if (!isGameOver || flashFrames > 0) {
        drawBird(cv);
    }

    // HUD поверх всего
    drawHUD(cv);

    // Экраны
    if (isStartScreen) drawStartScreen(cv);
    if (isGameOver && flashFrames == 0) drawGameOver(cv);

    bDisp.update();
}
