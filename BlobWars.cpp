// ================================================================
//  BlobWars — финальная версия с Prestige + анимированными лицами
//
//  PRESTIGE: когда игрок достигает MAX_SIZE (40px) —
//    взрыв-анимация, игрок делится на 3 маленьких блоба (r=12)
//    управляешь тем, на которого тапнул; активный — белая обводка
//    если все три погибли — Game Over
//    каждый prestige даёт x2 к очкам (множитель престижа)
//
//  АНИМИРОВАННЫЕ ЛИЦА:
//    глобальный blinkTimer — каждые 50 кадров глаза закрываются на 3 кадра
//    при ударе (flashTimer > 0) — лицо "боль": глаза-крестики, рот открыт
//    у босса — глаза всегда злые, зрачки смотрят на игрока
//
//  Остальное из прошлых версий:
//    хиттаймер, shake, HP-бар, пауза, HighScore, типы врагов,
//    цель волны, безопасный спавн, ограничение роста
// ================================================================

#include "Global.h"
#include "BlobWars.h"
#include "Engine2D.h"
#include "Display.h"
#include <XPT2046_Touchscreen.h>

extern XPT2046_Touchscreen ts;
extern void getPoint(int &x, int &y);
extern void showMenu(bool);
extern unsigned long lastTouch;

// ----------------------------------------------------------------
//  Группы
// ----------------------------------------------------------------
#define GRP_PLAYER  0
#define GRP_ENEMY   1
#define GRP_FOOD    2
#define GRP_SPARK   3    // частицы взрыва prestige

// ----------------------------------------------------------------
//  Константы
// ----------------------------------------------------------------
#define SCREEN_W          320
#define SCREEN_H          240
#define HUD_H              20
#define FOOD_COUNT          6
#define ENEMY_COUNT         3

#define PLAYER_START_R     10.f
#define PLAYER_MAX_SIZE    40.f   // порог prestige
#define PRESTIGE_SPLIT_R   12.f  // радиус каждого осколка
#define PRESTIGE_COUNT      3    // на сколько блобов делимся

#define HIT_COOLDOWN       45
#define FLASH_DURATION     20
#define BOSS_WAVE_STEP      5

#define SHAKE_FRAMES        8
#define SHAKE_AMP           4

#define BLINK_INTERVAL     50    // кадров между морганиями
#define BLINK_DURATION      3    // кадров закрытых глаз

// ----------------------------------------------------------------
//  Тип врага
// ----------------------------------------------------------------
enum EnemyType : uint8_t { ET_BOUNCER = 0, ET_HUNTER = 1, ET_BOSS = 2 };

// ----------------------------------------------------------------
//  Состояние
// ----------------------------------------------------------------
enum BWState { BW_PLAY, BW_PAUSE, BW_OVER, BW_PRESTIGE };
// BW_PRESTIGE: короткая анимация взрыва (30 кадров) перед спавном осколков

static Scene      scene;
static Renderer   rend;
static Input      input;
static FrameTimer ftimer(30);

// Prestige: массив ID трёх осколков
static EntityID   shardIds[PRESTIGE_COUNT];
static int        activeShardIdx  = 0;     // кем сейчас управляем
static int        prestigeCount   = 0;     // сколько раз сделали prestige
static int        prestigeTimer   = 0;     // кадров анимации взрыва

// Для совместимости: текущий активный игрок
static EntityID   activeShard() { return shardIds[activeShardIdx]; }

static BWState    bwState;
static int        score;
static int        wave;
static int        waveBannerFrames;

static int        hitCooldown     = 0;
static int        flashTimer      = 0;
static int        shakeTimer      = 0;

// Анимация моргания (глобальная для всех лиц)
static int        blinkCounter    = 0;  // счётчик до моргания
static bool       eyesClosed      = false;

static int        waveKillGoal    = 0;
static int        waveKilled      = 0;

static int        highScore       = 0;
static unsigned long lastHudTap   = 0;

// Позиция взрыва prestige (для анимации)
static Vec2       prestigePos;

// ----------------------------------------------------------------
//  Утилиты
// ----------------------------------------------------------------
static Vec2 safeSpawnPos(float minDist = 70.f) {
    // Берём первого живого игрока как ориентир
    Vec2 ppos(SCREEN_W / 2.f, SCREEN_H / 2.f);
    for (int i = 0; i < PRESTIGE_COUNT; i++) {
        Entity* e = scene.get(shardIds[i]);
        if (e) { ppos = e->transform.pos; break; }
    }
    Vec2 result;
    int attempts = 0;
    do {
        result = Vec2(random(20, SCREEN_W - 20),
                      random(HUD_H + 20, SCREEN_H - 20));
        attempts++;
    } while (Vec2::dist(result, ppos) < minDist && attempts < 25);
    return result;
}

static int livingShards() {
    int n = 0;
    for (int i = 0; i < PRESTIGE_COUNT; i++)
        if (scene.get(shardIds[i])) n++;
    return n;
}

// ----------------------------------------------------------------
//  Рисование ЛИЦА
//  cx,cy — центр; r — радиус блоба
//  mood: 0=нейтрал/улыбка, 1=злой, 2=босс
//  blink: true = глаза закрыты
//  pain:  true = лицо боли (удар)
//  lookAt: направление взгляда (нормализованный Vec2, для босса)
// ----------------------------------------------------------------
static void drawFace(Adafruit_GFX* cv,
                     int cx, int cy, int r,
                     uint8_t mood,
                     bool blink,
                     bool pain,
                     Vec2 lookDir = Vec2(0,0))
{
    if (r < 6) return;

    int eyeR  = max(1, r / 5);
    int eyeOX = r / 3;
    int eyeOY = r / 5;
    int lx    = cx - eyeOX,  rx2 = cx + eyeOX;
    int ey    = cy - eyeOY;

    // --- Брови (злые) ---
    if (mood >= 1 && r >= 10 && !pain) {
        int brow = ey - eyeR - 2;
        int tilt = (mood == 2) ? 4 : 2;
        cv->drawLine(lx - eyeR - 1, brow - tilt, lx + eyeR + 1, brow + tilt, 0x0000);
        cv->drawLine(rx2 - eyeR - 1, brow + tilt, rx2 + eyeR + 1, brow - tilt, 0x0000);
        if (r >= 12) { // вторая линия для толщины
            cv->drawLine(lx - eyeR - 1, brow - tilt + 1, lx + eyeR + 1, brow + tilt + 1, 0x0000);
            cv->drawLine(rx2 - eyeR - 1, brow + tilt + 1, rx2 + eyeR + 1, brow - tilt + 1, 0x0000);
        }
    }

    // --- Глаза ---
    if (pain) {
        // Крестики вместо глаз
        int s = eyeR + 1;
        cv->drawLine(lx - s, ey - s, lx + s, ey + s, 0x0000);
        cv->drawLine(lx + s, ey - s, lx - s, ey + s, 0x0000);
        cv->drawLine(rx2 - s, ey - s, rx2 + s, ey + s, 0x0000);
        cv->drawLine(rx2 + s, ey - s, rx2 - s, ey + s, 0x0000);
    } else if (blink) {
        // Закрытые глаза — горизонтальная дуга (линия)
        cv->drawLine(lx - eyeR, ey, lx + eyeR, ey, 0x0000);
        cv->drawLine(rx2 - eyeR, ey, rx2 + eyeR, ey, 0x0000);
        if (eyeR >= 2) {
            cv->drawLine(lx - eyeR + 1, ey + 1, lx + eyeR - 1, ey + 1, 0x0000);
            cv->drawLine(rx2 - eyeR + 1, ey + 1, rx2 + eyeR - 1, ey + 1, 0x0000);
        }
    } else {
        // Открытые глаза — белый с зрачком
        cv->fillCircle(lx,  ey, eyeR + 1, 0xFFFF);
        cv->fillCircle(rx2, ey, eyeR + 1, 0xFFFF);

        // Зрачок: для босса смотрит в сторону игрока
        int pox = 0, poy = 0;
        if (mood == 2 && (lookDir.x != 0 || lookDir.y != 0)) {
            pox = (int)(lookDir.x * eyeR * 0.5f);
            poy = (int)(lookDir.y * eyeR * 0.5f);
        }
        cv->fillCircle(lx  + pox, ey + poy, eyeR,     0x0000);
        cv->fillCircle(rx2 + pox, ey + poy, eyeR,     0x0000);

        // Блик (белая точка в зрачке)
        if (eyeR >= 2) {
            cv->fillCircle(lx  + pox - 1, ey + poy - 1, max(1, eyeR / 2 - 1), 0xFFFF);
            cv->fillCircle(rx2 + pox - 1, ey + poy - 1, max(1, eyeR / 2 - 1), 0xFFFF);
        }
    }

    // --- Рот ---
    int mouthY = cy + r / 3;
    int mw     = r / 2;

    if (pain) {
        // Открытый рот (овал)
        int oh = max(2, r / 5);
        cv->fillCircle(cx, mouthY, mw / 2, 0x0000);
    } else if (mood == 0) {
        // Улыбка
        cv->drawLine(cx - mw, mouthY,       cx,       mouthY + r/5, 0x0000);
        cv->drawLine(cx,      mouthY + r/5, cx + mw,  mouthY,       0x0000);
        if (r >= 14) {
            cv->drawLine(cx - mw, mouthY + 1,         cx,        mouthY + r/5 + 1, 0x0000);
            cv->drawLine(cx,      mouthY + r/5 + 1,   cx + mw,   mouthY + 1,       0x0000);
        }
    } else if (mood == 1) {
        // Прямой рот
        cv->drawLine(cx - mw, mouthY, cx + mw, mouthY, 0x0000);
        cv->drawLine(cx - mw, mouthY + 1, cx + mw, mouthY + 1, 0x0000);
    } else {
        // Оскал (вверх) с зубами — босс
        cv->drawLine(cx - mw, mouthY + r/5, cx,      mouthY,       0x0000);
        cv->drawLine(cx,      mouthY,        cx + mw, mouthY + r/5, 0x0000);
        if (r >= 14) {
            for (int t = -1; t <= 1; t++) {
                int tx = cx + t * (mw / 2);
                cv->drawLine(tx, mouthY, tx, mouthY + r / 6, 0xFFFF);
            }
        }
    }
}

// Рисуем лица для всех живых Entity
static void drawAllFaces(int shakeX, int shakeY) {
    auto* cv     = bDisp.getCanvas();
    bool  isPain = (flashTimer > 0);

    // Находим позицию активного игрока (для взгляда босса)
    Vec2 playerPos(SCREEN_W / 2.f, SCREEN_H / 2.f);
    Entity* ap = scene.get(activeShard());
    if (ap) playerPos = ap->transform.pos;

    for (int i = 0; i < scene.count; i++) {
        Entity& e = scene.entities[i];
        if (!e.alive) continue;
        if (e.group == GRP_FOOD || e.group == GRP_SPARK) continue;

        int cx = (int)e.transform.pos.x + shakeX;
        int cy = (int)e.transform.pos.y + shakeY;
        int r  = (int)e.transform.radius;

        uint8_t mood = 0;
        bool    pain = false;
        Vec2    look(0, 0);

        if (e.group == GRP_PLAYER) {
            pain = isPain;
        } else if (e.group == GRP_ENEMY) {
            EnemyType t = (EnemyType)(uintptr_t)e.userData;
            mood = (t == ET_BOSS) ? 2 : 1;
            if (t == ET_BOSS) {
                look = (playerPos - e.transform.pos).normalized();
            }
        }

        drawFace(cv, cx, cy, r, mood, eyesClosed, pain, look);
    }
}

// ----------------------------------------------------------------
//  Анимация взрыва (частицы)
// ----------------------------------------------------------------
static void spawnSparks(Vec2 pos, uint16_t color) {
    for (int i = 0; i < 8; i++) {
        Entity* s = scene.spawn(GRP_SPARK);
        if (!s) return;
        s->transform = Transform(pos.x, pos.y, 2.f);
        float ang    = (i / 8.f) * 6.28f;
        float spd    = 2.f + random(0, 30) / 10.f;
        s->transform.vel = Vec2(cosf(ang) * spd, sinf(ang) * spd);
        s->sprite    = Sprite::circle(color);
        s->health    = 18.f;  // time-to-live в кадрах
    }
}

static void updateSparks() {
    for (int i = 0; i < scene.count; i++) {
        Entity& e = scene.entities[i];
        if (!e.alive || e.group != GRP_SPARK) continue;
        e.health -= 1.f;
        e.transform.radius = max(1.f, e.health / 9.f);
        if (e.health <= 0.f) scene.kill(e.id);
    }
}

// ----------------------------------------------------------------
//  Спавн еды
// ----------------------------------------------------------------
static void spawnFood() {
    // Проверяем: есть ли хоть один игрок не на максимуме
    bool anySmall = false;
    for (int i = 0; i < PRESTIGE_COUNT; i++) {
        Entity* e = scene.get(shardIds[i]);
        if (e && e->transform.radius < PLAYER_MAX_SIZE - 0.5f) { anySmall = true; break; }
    }
    if (!anySmall) return;

    int toSpawn = FOOD_COUNT - scene.countAlive(GRP_FOOD);
    for (int i = 0; i < toSpawn; i++) {
        Entity* f = scene.spawn(GRP_FOOD);
        if (!f) return;
        f->transform = Transform(random(10, SCREEN_W - 10),
                                 random(HUD_H + 10, SCREEN_H - 10), 4.f);
        f->sprite    = Sprite::circle(0xFFE0);
    }
}

// ----------------------------------------------------------------
//  Спавн врагов
// ----------------------------------------------------------------
static void spawnOneEnemy(EnemyType type, float radius, float speedMult) {
    Entity* e = scene.spawn(GRP_ENEMY);
    if (!e) return;
    e->transform     = Transform(0, 0, radius);
    e->transform.pos = safeSpawnPos();
    e->health        = radius;
    e->userData      = reinterpret_cast<void*>((uintptr_t)type);
    float ang   = random(0, 628) / 100.f;
    float speed = speedMult * (0.5f + wave * 0.12f);
    if (speed > 3.0f) speed = 3.0f;
    e->transform.vel = Vec2(cosf(ang) * speed, sinf(ang) * speed);
    switch (type) {
        case ET_BOUNCER: e->sprite = Sprite::circle(0xF800, 0xFFFF, true); break;
        case ET_HUNTER:  e->sprite = Sprite::circle(0xFC00, 0xFFFF, true); break;
        case ET_BOSS:    e->sprite = Sprite::circle(0x780F, 0xF81F, true); break;
    }
}

static void spawnWave() {
    bool bossWave  = (wave % BOSS_WAVE_STEP == 0);
    int  count     = ENEMY_COUNT + wave - 1;
    waveKillGoal   = count + (bossWave ? 1 : 0);
    waveKilled     = 0;
    if (bossWave) spawnOneEnemy(ET_BOSS, 28.f, 1.0f);
    int hunters  = wave / 3;
    if (hunters > count - 1) hunters = count - 1;
    for (int i = 0; i < count - hunters; i++)
        spawnOneEnemy(ET_BOUNCER, random(12, 20), 1.0f);
    for (int i = 0; i < hunters; i++)
        spawnOneEnemy(ET_HUNTER, random(10, 16), 1.2f);
}

// ----------------------------------------------------------------
//  Prestige: игрок достиг макса — взрыв и деление
// ----------------------------------------------------------------
static void triggerPrestige(Entity* player) {
    prestigePos   = player->transform.pos;
    prestigeTimer = 25;   // кадров анимации
    bwState       = BW_PRESTIGE;
    prestigeCount++;

    spawnSparks(prestigePos, 0x07FF);  // циановые искры
    spawnSparks(prestigePos, 0xFFFF);

    scene.kill(player->id);
    // shardIds[] будут заспавнены когда prestigeTimer истечёт
}

static void spawnShards() {
    for (int i = 0; i < PRESTIGE_COUNT; i++) {
        Entity* s = scene.spawn(GRP_PLAYER);
        if (!s) { shardIds[i] = ENTITY_NONE; continue; }

        float ang = (i / (float)PRESTIGE_COUNT) * 6.28f;
        float d   = 20.f;
        s->transform = Transform(prestigePos.x + cosf(ang) * d,
                                 prestigePos.y + sinf(ang) * d,
                                 PRESTIGE_SPLIT_R);
        s->transform.vel = Vec2(cosf(ang) * 1.5f, sinf(ang) * 1.5f);
        s->sprite    = Sprite::circle(0x07E0, 0xFFFF, true);
        s->health    = PRESTIGE_SPLIT_R;
        shardIds[i]  = s->id;
    }
    // Активный = первый живой
    activeShardIdx = 0;
    bwState = BW_PLAY;
}

// ----------------------------------------------------------------
//  Инициализация
// ----------------------------------------------------------------
void initBlobWars() {
    rend.setCanvas(bDisp.getCanvas());
    scene.setRenderer(&rend);
    scene.killAll();

    score          = 0;
    wave           = 1;
    waveBannerFrames = 60;
    bwState        = BW_PLAY;
    hitCooldown    = 0;
    flashTimer     = 0;
    shakeTimer     = 0;
    prestigeCount  = 0;
    prestigeTimer  = 0;
    blinkCounter   = 0;
    eyesClosed     = false;
    waveKillGoal   = 0;
    waveKilled     = 0;

    // Один стартовый блоб; остальные — ENTITY_NONE
    for (int i = 0; i < PRESTIGE_COUNT; i++) shardIds[i] = ENTITY_NONE;

    Entity* p    = scene.spawn(GRP_PLAYER);
    p->transform = Transform(SCREEN_W / 2.f, SCREEN_H / 2.f, PLAYER_START_R);
    p->sprite    = Sprite::circle(0x07E0, 0xFFFF, true);
    p->health    = PLAYER_START_R;
    shardIds[0]  = p->id;
    activeShardIdx = 0;

    spawnFood();
    spawnWave();
}

// ----------------------------------------------------------------
//  Логика: выбор активного блоба по тапу
// ----------------------------------------------------------------
static void handleShardSwitch() {
    if (!input.justPressed || input.pos.y < HUD_H) return;

    // Ищем ближайший живой блоб к месту тапа
    float bestDist = 999.f;
    int   bestIdx  = -1;
    for (int i = 0; i < PRESTIGE_COUNT; i++) {
        Entity* e = scene.get(shardIds[i]);
        if (!e) continue;
        float d = Vec2::dist(input.pos, e->transform.pos);
        if (d < bestDist) { bestDist = d; bestIdx = i; }
    }
    if (bestIdx >= 0 && bestDist < 40.f)
        activeShardIdx = bestIdx;
}

// ----------------------------------------------------------------
//  Логика одного блоба-игрока
// ----------------------------------------------------------------
static void updateOneShard(Entity* player, bool isActive) {
    if (!player) return;

    // Активный тянется к пальцу, остальные тормозят
    if (isActive && input.touched && input.pos.y > HUD_H) {
        Vec2 dir = (input.pos - player->transform.pos).normalized();
        player->transform.vel = dir * 2.5f;
    } else {
        player->transform.vel = player->transform.vel * 0.85f;
    }

    // Подсветка активного
    player->sprite.outlineColor = isActive ? 0xFFFF : 0x4208;

    // Границы
    Vec2&  p = player->transform.pos;
    float  r = player->transform.radius;
    if (p.x < r)            { p.x = r;            player->transform.vel.x *= -0.5f; }
    if (p.x > SCREEN_W - r) { p.x = SCREEN_W - r; player->transform.vel.x *= -0.5f; }
    if (p.y < HUD_H + r)    { p.y = HUD_H + r;    player->transform.vel.y *= -0.5f; }
    if (p.y > SCREEN_H - r) { p.y = SCREEN_H - r; player->transform.vel.y *= -0.5f; }
}

static void updateAllShards() {
    if (hitCooldown > 0) hitCooldown--;
    if (shakeTimer  > 0) shakeTimer--;

    if (flashTimer > 0) {
        flashTimer--;
        bool show = (flashTimer % 4) < 2;
        // Мигаем только активным
        Entity* ap = scene.get(activeShard());
        if (ap) ap->sprite.color = show ? 0xFFFF : 0x07E0;
    } else {
        Entity* ap = scene.get(activeShard());
        if (ap) ap->sprite.color = 0x07E0;
    }

    for (int i = 0; i < PRESTIGE_COUNT; i++) {
        Entity* e = scene.get(shardIds[i]);
        updateOneShard(e, i == activeShardIdx);
    }
}

// ----------------------------------------------------------------
//  Логика врагов
// ----------------------------------------------------------------
static void updateEnemies() {
    Entity* ap = scene.get(activeShard());
    Vec2 target = ap ? ap->transform.pos : Vec2(SCREEN_W/2.f, SCREEN_H/2.f);

    for (int i = 0; i < scene.count; i++) {
        Entity& e = scene.entities[i];
        if (!e.alive || e.group != GRP_ENEMY) continue;
        EnemyType type = (EnemyType)(uintptr_t)e.userData;
        float r = e.transform.radius;
        Vec2& p = e.transform.pos;

        if (type == ET_HUNTER || type == ET_BOSS) {
            Vec2  toPlayer = (target - p).normalized();
            float pull     = (type == ET_BOSS) ? 0.07f : 0.045f;
            e.transform.vel = e.transform.vel + toPlayer * pull;
            float maxSpd    = (type == ET_BOSS) ? 2.2f : 2.8f;
            float spd = e.transform.vel.length();
            if (spd > maxSpd)
                e.transform.vel = e.transform.vel.normalized() * maxSpd;
        }

        if (p.x < r || p.x > SCREEN_W - r) e.transform.vel.x *= -1.f;
        if (p.y < HUD_H + r || p.y > SCREEN_H - r) e.transform.vel.y *= -1.f;
        p.x = constrain(p.x, r, SCREEN_W - r);
        p.y = constrain(p.y, HUD_H + r, SCREEN_H - r);
    }
}

// ----------------------------------------------------------------
//  Коллизии
// ----------------------------------------------------------------
static void checkCollisions() {
    int scoreMulti = 1 + prestigeCount;  // множитель престижа

    for (int pi = 0; pi < PRESTIGE_COUNT; pi++) {
        Entity* player = scene.get(shardIds[pi]);
        if (!player) continue;

        // vs еда
        for (int i = 0; i < scene.count; i++) {
            Entity& f = scene.entities[i];
            if (!f.alive || f.group != GRP_FOOD) continue;
            if (Collider::circle_circle(*player, f)) {
                scene.kill(f.id);
                score += 10 * scoreMulti;
                if (player->transform.radius < PLAYER_MAX_SIZE)
                    player->transform.radius = min(PLAYER_MAX_SIZE,
                                                   player->transform.radius + 0.8f);
                if (player->health < player->transform.radius)
                    player->health = min(player->transform.radius, player->health + 0.5f);

                // Prestige!
                if (player->transform.radius >= PLAYER_MAX_SIZE - 0.1f) {
                    triggerPrestige(player);
                    shardIds[pi] = ENTITY_NONE;
                    return;
                }
            }
        }

        // vs враги
        for (int i = 0; i < scene.count; i++) {
            Entity& e = scene.entities[i];
            if (!e.alive || e.group != GRP_ENEMY) continue;
            if (!Collider::circle_circle(*player, e)) continue;

            EnemyType type = (EnemyType)(uintptr_t)e.userData;

            if (player->transform.radius > e.transform.radius + 3.f) {
                scene.kill(e.id);
                score += (type == ET_BOSS ? 200 : 50) * scoreMulti;
                waveKilled++;
                if (player->transform.radius < PLAYER_MAX_SIZE)
                    player->transform.radius = min(PLAYER_MAX_SIZE,
                        player->transform.radius + (type == ET_BOSS ? 4.f : 2.f));
                spawnSparks(e.transform.pos, (type == ET_BOSS) ? 0xF81F : 0xF800);
            } else {
                if (hitCooldown == 0 && pi == activeShardIdx) {
                    float dmg   = (type == ET_BOSS) ? 6.f : 3.f;
                    player->transform.radius -= dmg;
                    player->health            = player->transform.radius;
                    hitCooldown = HIT_COOLDOWN;
                    flashTimer  = FLASH_DURATION;
                    shakeTimer  = SHAKE_FRAMES;

                    if (player->transform.radius < 5.f) {
                        spawnSparks(player->transform.pos, 0x07E0);
                        scene.kill(player->id);
                        shardIds[pi] = ENTITY_NONE;

                        // Переключаем на следующего живого
                        for (int j = 0; j < PRESTIGE_COUNT; j++) {
                            if (scene.get(shardIds[j])) { activeShardIdx = j; break; }
                        }

                        if (livingShards() == 0) {
                            if (score > highScore) highScore = score;
                            bwState = BW_OVER;
                        }
                        return;
                    }
                }
            }
        }
    }

    // Новая волна
    if (scene.countAlive(GRP_ENEMY) == 0) {
        wave++;
        waveBannerFrames = 60;
        spawnFood();
        spawnWave();
    } else {
        spawnFood();
    }
}

// ----------------------------------------------------------------
//  HUD
// ----------------------------------------------------------------
static void drawHUD() {
    // Берём активного игрока для HP
    Entity* ap = scene.get(activeShard());
    float sz   = ap ? ap->transform.radius : 0.f;
    float hp   = ap ? ap->health : 0.f;

    rend.hud_rect(0, 0, SCREEN_W, HUD_H, 0x10A2);

    char buf[48];
    snprintf(buf, sizeof(buf), "SC:%d W:%d", score, wave);
    rend.hud_text(buf, 4, 4, 0xFFFF, 1);

    // Цель волны
    snprintf(buf, sizeof(buf), "K:%d/%d", waveKilled, waveKillGoal);
    rend.hud_text(buf, 88, 4, 0xFD20, 1);

    // HP-бар
    int barX = 142, barY = 5, barW = 55, barH = 9;
    int filled = (int)(barW * (hp / PLAYER_MAX_SIZE));
    filled = constrain(filled, 0, barW);
    auto* cv = bDisp.getCanvas();
    cv->fillRect(barX, barY, barW, barH, 0x4208);
    uint16_t barCol = (hp > PLAYER_MAX_SIZE * 0.5f) ? 0x07E0
                    : (hp > PLAYER_MAX_SIZE * 0.25f) ? 0xFD20 : 0xF800;
    cv->fillRect(barX, barY, filled, barH, barCol);
    cv->drawRect(barX, barY, barW, barH, 0xFFFF);

    // Живые блобы (иконки)
    int alive = livingShards();
    for (int i = 0; i < PRESTIGE_COUNT; i++) {
        bool live = (scene.get(shardIds[i]) != nullptr);
        uint16_t c = live ? (i == activeShardIdx ? 0x07FF : 0x07E0) : 0x4208;
        cv->fillCircle(205 + i * 12, 10, 4, c);
        cv->drawCircle(205 + i * 12, 10, 4, 0xFFFF);
    }

    // Prestige счётчик
    if (prestigeCount > 0) {
        snprintf(buf, sizeof(buf), "x%d", 1 + prestigeCount);
        rend.hud_text(buf, 236, 4, 0xFFE0, 1);
    }

    rend.hud_text("||", 278, 4, 0xFFE0, 1);
    rend.hud_text("X",  308, 4, 0xF800, 1);
}

// ----------------------------------------------------------------
//  Баннер волны
// ----------------------------------------------------------------
static void drawWaveBanner() {
    if (waveBannerFrames <= 0) return;
    bool isBoss = (wave % BOSS_WAVE_STEP == 0);
    auto* cv = bDisp.getCanvas();
    cv->fillRect(60, 88, 200, 52, 0x0000);
    cv->drawRect(60, 88, 200, 52, isBoss ? 0xF81F : 0xFFFF);
    cv->setTextSize(2);
    cv->setTextColor(isBoss ? 0xF81F : 0x07FF);
    cv->setCursor(isBoss ? 68 : 100, 97);
    if (isBoss) cv->printf("!! BOSS W%d !!", wave);
    else        cv->printf("WAVE %d", wave);
    cv->setTextSize(1);
    cv->setTextColor(0xFD20);
    cv->setCursor(88, 121);
    cv->printf("Kill %d enemies", waveKillGoal);
    waveBannerFrames--;
}

// ----------------------------------------------------------------
//  Экран prestige-взрыва
// ----------------------------------------------------------------
static void drawPrestigeAnim() {
    scene.camera.pos = Vec2(0, 0);
    scene.render(0x0000);
    drawAllFaces(0, 0);

    // Текст в центре
    auto* cv = bDisp.getCanvas();
    cv->setTextColor(0xFFE0);
    cv->setTextSize(2);
    cv->setCursor(80, 105);
    cv->printf("PRESTIGE x%d!", prestigeCount);
    drawHUD();
    bDisp.update();
}

// ----------------------------------------------------------------
//  Пауза
// ----------------------------------------------------------------
static void drawPause() {
    auto* cv = bDisp.getCanvas();
    cv->fillRect(80, 85, 160, 70, 0x0000);
    cv->drawRect(80, 85, 160, 70, 0xFFFF);
    cv->setTextColor(0xFFE0);
    cv->setTextSize(2);
    cv->setCursor(115, 96);
    cv->print("PAUSED");
    cv->setTextColor(0xFFFF);
    cv->setTextSize(1);
    cv->setCursor(95, 128);
    cv->print("Tap || to resume");
}

// ----------------------------------------------------------------
//  Game Over
// ----------------------------------------------------------------
static void drawGameOver() {
    auto* cv = bDisp.getCanvas();
    rend.clear(0x0000);
    cv->setTextColor(0xF800);
    cv->setTextSize(3);
    cv->setCursor(55, 38);
    cv->print("GAME OVER");
    cv->setTextColor(0xFFFF);
    cv->setTextSize(2);
    cv->setCursor(70, 88);
    cv->printf("SCORE:    %d", score);
    cv->setCursor(70, 111);
    cv->printf("WAVE:     %d", wave);
    cv->setCursor(70, 134);
    cv->printf("PRESTIGE: %d", prestigeCount);
    cv->setTextColor(0xFFE0);
    cv->setCursor(70, 157);
    cv->printf("BEST:     %d", highScore);
    cv->fillRect(10,  195, 90, 30, 0xB000);
    cv->drawRect(10,  195, 90, 30, 0xFFFF);
    cv->setTextColor(0xFFFF);
    cv->setTextSize(1);
    cv->setCursor(28, 206);
    cv->print("EXIT");
    cv->fillRect(120, 195, 80, 30, 0x0500);
    cv->drawRect(120, 195, 80, 30, 0xFFFF);
    cv->setCursor(135, 206);
    cv->print("RETRY");
    bDisp.update();
}

static void touchGameOver() {
    if (!ts.touched()) return;
    if (millis() - lastTouch < 250) return;
    int x, y; getPoint(x, y);
    lastTouch = millis();
    if (y > 195 && y < 225) {
        if (x > 10  && x < 100) { showMenu(true); return; }
        if (x > 120 && x < 200) { initBlobWars(); return; }
    }
}

// ----------------------------------------------------------------
//  Главный тик
// ----------------------------------------------------------------
void tickBlobWars() {
    if (bwState == BW_OVER) {
        drawGameOver();
        touchGameOver();
        return;
    }

    // Анимация взрыва prestige
    if (bwState == BW_PRESTIGE) {
        if (!ftimer.ready()) return;
        updateSparks();
        scene.applyVelocities();
        drawPrestigeAnim();
        prestigeTimer--;
        if (prestigeTimer <= 0) spawnShards();
        return;
    }

    if (!ftimer.ready()) return;

    // --- Тач ---
    bool tch = ts.touched();
    int tx = 160, ty = 120;
    if (tch) getPoint(tx, ty);
    input.update(tch, tx, ty);

    // EXIT
    if (input.justPressed && input.pos.y < HUD_H && input.pos.x > 299) {
        showMenu(true); return;
    }

    // ПАУЗА
    if (input.justPressed && input.pos.y < HUD_H) {
        bool pauseBtn  = (input.pos.x > 272 && input.pos.x < 299);
        bool doubleTap = (millis() - lastHudTap < 350);
        lastHudTap = millis();
        if (pauseBtn || doubleTap)
            bwState = (bwState == BW_PAUSE) ? BW_PLAY : BW_PAUSE;
    }

    // --- Shake offset ---
    int shakeX = 0, shakeY = 0;
    if (shakeTimer > 0) {
        int ph = shakeTimer % 4;
        shakeX = (ph == 0 || ph == 2) ?  SHAKE_AMP : -SHAKE_AMP;
        shakeY = (ph == 0 || ph == 3) ?  SHAKE_AMP : -SHAKE_AMP;
    }

    // --- Моргание ---
    blinkCounter++;
    if (!eyesClosed && blinkCounter >= BLINK_INTERVAL) {
        eyesClosed   = true;
        blinkCounter = 0;
    } else if (eyesClosed && blinkCounter >= BLINK_DURATION) {
        eyesClosed   = false;
        blinkCounter = 0;
    }

    if (bwState == BW_PAUSE) {
        scene.camera.pos = Vec2(0, 0);
        scene.render(0x0000);
        drawAllFaces(0, 0);
        drawHUD();
        drawPause();
        bDisp.update();
        return;
    }

    // --- Переключение блоба ---
    handleShardSwitch();

    // --- Логика ---
    updateAllShards();
    updateEnemies();
    updateSparks();
    scene.applyVelocities();
    checkCollisions();

    // --- Рендер ---
    scene.camera.pos = Vec2((float)shakeX, (float)shakeY);
    scene.render(0x0000);
    drawAllFaces(shakeX, shakeY);
    drawHUD();
    drawWaveBanner();
    bDisp.update();
}
