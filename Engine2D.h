#ifndef ENGINE2D_H
#define ENGINE2D_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <math.h>

// ================================================================
//  ENGINE2D — лёгкий 2D движок для ESP32 + ILI9341 + SplitCanvas
//
//  Архитектура:
//    Vec2             — вектор/точка (x,y)
//    Rect             — прямоугольник (x,y,w,h) + методы
//    Transform        — позиция + размер + скорость объекта
//    Sprite           — что рисовать (цвет, форма, иконка)
//    AnimFrame        — один кадр анимации (bitmap + delay)
//    Animation        — набор кадров + loop
//    AnimationPlayer  — воспроизведение анимации на Entity
//    Physics          — гравитация, трение, прыжки, платформы
//    TileMap          — тайловая карта (PROGMEM, слои, коллизии)
//    Entity           — игровой объект: Transform + Sprite + id + alive
//    Camera           — смещение мира (для скролла/следования)
//    Scene            — список Entity + обновление + рендер
//    Input            — тач-состояние (XPT2046)
//    Renderer         — обёртка над SplitCanvas/Adafruit_GFX
//    Collider         — проверки столкновений (AABB + круг)
//    Timer            — счётчик кадров / интервальные события
//
//  Пример игры в конце Engine2D.h (секция EXAMPLE)
// ================================================================

// ----------------------------------------------------------------
//  Vec2 — 2D вектор
// ----------------------------------------------------------------
struct Vec2 {
    float x, y;

    Vec2(float x = 0, float y = 0) : x(x), y(y) {}

    Vec2  operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2  operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2  operator*(float s)       const { return {x * s, y * s}; }
    Vec2& operator+=(const Vec2& o)      { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o)      { x -= o.x; y -= o.y; return *this; }

    float length() const { return sqrtf(x * x + y * y); }
    Vec2  normalized() const {
        float l = length();
        return l > 0.0001f ? Vec2(x / l, y / l) : Vec2(0, 0);
    }
    float dot(const Vec2& o) const { return x * o.x + y * o.y; }

    static float dist(Vec2 a, Vec2 b) {
        float dx = a.x - b.x, dy = a.y - b.y;
        return sqrtf(dx * dx + dy * dy);
    }
};

// ----------------------------------------------------------------
//  Rect — прямоугольник (AABB)
// ----------------------------------------------------------------
struct Rect {
    float x, y, w, h;

    Rect(float x = 0, float y = 0, float w = 0, float h = 0)
        : x(x), y(y), w(w), h(h) {}

    bool contains(Vec2 p) const {
        return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
    }
    bool overlaps(const Rect& o) const {
        return x < o.x + o.w && x + w > o.x &&
               y < o.y + o.h && y + h > o.y;
    }
    Vec2 center() const { return {x + w / 2.f, y + h / 2.f}; }
    Rect expand(float d) const { return {x - d, y - d, w + d * 2, h + d * 2}; }
};

// ----------------------------------------------------------------
//  Transform — позиция, скорость, размер
// ----------------------------------------------------------------
struct Transform {
    Vec2  pos;
    Vec2  vel;       // скорость (пикселей/кадр)
    Vec2  size;      // ширина и высота (для AABB)
    float radius;    // радиус (для круглых объектов)
    float angle;     // угол поворота (0..2*PI), только для рендера

    Transform() : radius(0), angle(0) {}
    Transform(float x, float y, float w, float h)
        : pos(x, y), size(w, h), radius(0), angle(0) {}
    Transform(float x, float y, float r)
        : pos(x, y), size(r * 2, r * 2), radius(r), angle(0) {}

    Rect bounds() const { return {pos.x - size.x / 2, pos.y - size.y / 2, size.x, size.y}; }
    void applyVelocity() { pos += vel; }
};

// ----------------------------------------------------------------
//  Sprite — описание внешнего вида Entity
// ----------------------------------------------------------------
enum SpriteShape {
    SHAPE_CIRCLE,    // заливка кругом
    SHAPE_RECT,      // заливка прямоугольником
    SHAPE_ROUND_RECT,// скруглённый прямоугольник
    SHAPE_SPRITE,    // PROGMEM-массив пикселей RGB565 (uint16_t[])
    SHAPE_NONE,      // невидимый (только логика)
};

struct Sprite {
    SpriteShape  shape;
    uint16_t     color;
    uint16_t     outlineColor;
    bool         hasOutline;
    int          cornerRadius;           // для ROUND_RECT
    const uint16_t* bitmapData;          // для SHAPE_SPRITE
    int          bitmapW, bitmapH;       // размер bitmap
    uint16_t     transparentColor;       // цвет-маркер прозрачности (0xFFFF = нет)
    bool         progmem;                // true = данные в PROGMEM, false = RAM

    // Конструкторы-хелперы
    Sprite() : shape(SHAPE_NONE), color(0), outlineColor(0xFFFF),
               hasOutline(false), cornerRadius(4),
               bitmapData(nullptr), bitmapW(0), bitmapH(0),
               transparentColor(0xFFFF), progmem(true) {}

    static Sprite circle(uint16_t color, uint16_t outline = 0, bool hasOutline = false) {
        Sprite s;
        s.shape = SHAPE_CIRCLE;
        s.color = color;
        s.outlineColor = outline;
        s.hasOutline = hasOutline;
        return s;
    }
    static Sprite rect(uint16_t color, uint16_t outline = 0, bool hasOutline = false) {
        Sprite s;
        s.shape = SHAPE_RECT;
        s.color = color;
        s.outlineColor = outline;
        s.hasOutline = hasOutline;
        return s;
    }
    static Sprite roundRect(uint16_t color, int r = 4, uint16_t outline = 0xFFFF, bool hasOutline = true) {
        Sprite s;
        s.shape = SHAPE_ROUND_RECT;
        s.color = color;
        s.outlineColor = outline;
        s.hasOutline = hasOutline;
        s.cornerRadius = r;
        return s;
    }
    // inProgmem=true  → данные в PROGMEM (const PROGMEM uint16_t[])
    // inProgmem=false → данные в RAM (обычный массив)
    static Sprite bitmap(const uint16_t* data, int w, int h,
                         uint16_t transpColor = 0x0000, bool inProgmem = true) {
        Sprite s;
        s.shape      = SHAPE_SPRITE;
        s.bitmapData = data;
        s.bitmapW    = w;
        s.bitmapH    = h;
        s.transparentColor = transpColor;
        s.progmem    = inProgmem;
        return s;
    }
};

// ----------------------------------------------------------------
//  AnimationPlayer — покадровая анимация спрайтов
// ----------------------------------------------------------------
#define MAX_ANIM_FRAMES 16

struct AnimFrame {
    const uint16_t* data;   // PROGMEM-данные кадра
    uint16_t        delay;  // задержка в мс
};

struct Animation {
    AnimFrame frames[MAX_ANIM_FRAMES];
    uint8_t   frameCount;
    bool      loop;

    Animation() : frameCount(0), loop(true) {}

    void addFrame(const uint16_t* data, uint16_t delayMs) {
        if (frameCount < MAX_ANIM_FRAMES) {
            frames[frameCount++] = { data, delayMs };
        }
    }
};

struct AnimationPlayer {
    Animation*    anim;
    uint8_t       currentFrame;
    unsigned long _lastFrameTime;
    bool          playing;
    bool          finished;   // true если oneShot завершён

    AnimationPlayer() : anim(nullptr), currentFrame(0),
                        _lastFrameTime(0), playing(false), finished(false) {}

    void play(Animation* a) {
        anim         = a;
        currentFrame = 0;
        playing      = true;
        finished     = false;
        _lastFrameTime = millis();
    }

    void stop() { playing = false; }
    void resume() { if (anim) playing = true; }

    // Вызывать каждый кадр. Возвращает true если кадр сменился.
    bool update() {
        if (!playing || !anim || anim->frameCount == 0) return false;
        unsigned long now = millis();
        uint16_t delay = anim->frames[currentFrame].delay;
        if (now - _lastFrameTime < delay) return false;
        _lastFrameTime = now;
        currentFrame++;
        if (currentFrame >= anim->frameCount) {
            if (anim->loop) {
                currentFrame = 0;
            } else {
                currentFrame = anim->frameCount - 1;
                playing  = false;
                finished = true;
            }
        }
        return true;
    }

    // Применить текущий кадр к спрайту Entity (вызывать после update())
    void applyTo(Sprite& sprite) {
        if (!anim || anim->frameCount == 0) return;
        sprite.shape      = SHAPE_SPRITE;
        sprite.bitmapData = anim->frames[currentFrame].data;
    }
};

// ----------------------------------------------------------------
//  Physics — гравитация, трение, прыжки, платформенные коллизии
// ----------------------------------------------------------------
struct Physics {
    float gravity;        // ускорение вниз (пикс/кадр²), обычно 0.4–0.8
    float friction;       // коэф. торможения по X (0.0–1.0, 0.85 = хорошо)
    float jumpForce;      // скорость вверх при прыжке (отрицательная, -8..-12)
    float maxFallSpeed;   // ограничение скорости падения (обычно 12–20)
    float groundY;        // Y пола (если используется простой пол без тайлов)
    bool  useGround;      // включить простой пол по groundY

    Physics()
        : gravity(0.5f), friction(0.85f), jumpForce(-10.0f),
          maxFallSpeed(15.0f), groundY(0), useGround(false) {}

    // Применить гравитацию и трение к одному Entity
    // Возвращает true если Entity стоит на земле
    bool applyTo(Transform& t) {
        // Гравитация
        t.vel.y += gravity;
        if (t.vel.y > maxFallSpeed) t.vel.y = maxFallSpeed;

        // Трение по горизонтали — стабильно, без FPU-тяжёлых функций
        t.vel.x *= friction;

        // Движение с sub-step (2 шага) — предотвращает tunneling при высокой скорости
        t.pos.x += t.vel.x * 0.5f;
        t.pos.y += t.vel.y * 0.5f;
        t.pos.x += t.vel.x * 0.5f;
        t.pos.y += t.vel.y * 0.5f;

        // Простой пол
        if (useGround) {
            float bottom = t.pos.y + t.size.y / 2;
            if (bottom >= groundY) {
                t.pos.y = groundY - t.size.y / 2;
                t.vel.y = 0;
                return true;  // на земле
            }
        }
        return false;
    }

    // Прыжок — вызывать только когда onGround == true
    void jump(Transform& t) {
        t.vel.y = jumpForce;
    }

    // Разрешение коллизии Entity с прямоугольной платформой (AABB push-out)
    // Возвращает true если Entity приземлился сверху на платформу
    bool resolveRect(Transform& t, const Rect& platform) {
        Rect eb = t.bounds();
        if (!eb.overlaps(platform)) return false;

        float overlapL = (eb.x + eb.w) - platform.x;
        float overlapR = (platform.x + platform.w) - eb.x;
        float overlapT = (eb.y + eb.h) - platform.y;
        float overlapB = (platform.y + platform.h) - eb.y;

        float minX = fminf(overlapL, overlapR);
        float minY = fminf(overlapT, overlapB);

        bool landedOnTop = false;
        if (minY < minX) {
            if (overlapT < overlapB) {
                // сверху → приземление
                t.pos.y = platform.y - t.size.y / 2;
                if (t.vel.y > 0) t.vel.y = 0;
                landedOnTop = true;
            } else {
                // снизу → удар головой
                t.pos.y = platform.y + platform.h + t.size.y / 2;
                if (t.vel.y < 0) t.vel.y = 0;
            }
        } else {
            if (overlapL < overlapR) {
                t.pos.x = platform.x - t.size.x / 2;
                if (t.vel.x > 0) t.vel.x = 0;
            } else {
                t.pos.x = platform.x + platform.w + t.size.x / 2;
                if (t.vel.x < 0) t.vel.x = 0;
            }
        }
        return landedOnTop;
    }
};

// ----------------------------------------------------------------
//  TileMap — тайловая карта
// ----------------------------------------------------------------
#define TILEMAP_MAX_TILES 64   // максимум типов тайлов
#define TILEMAP_MAX_COLS  40   // максимальная ширина карты в тайлах
#define TILEMAP_MAX_ROWS  30   // максимальная высота карты в тайлах

// Флаги тайла
#define TILE_FLAG_SOLID    0x01   // непроходимый (участвует в коллизиях)
#define TILE_FLAG_LADDER   0x02   // лестница
#define TILE_FLAG_SPIKE    0x04   // наносит урон
#define TILE_FLAG_PLATFORM 0x08   // односторонняя платформа (только сверху)

struct TileDef {
    uint16_t        color;         // цвет заливки (если нет bitmap)
    const uint16_t* bitmap;        // PROGMEM-данные (nullptr = цвет)
    uint8_t         flags;         // TILE_FLAG_*
    bool            visible;       // рисовать тайл?

    TileDef() : color(0x0000), bitmap(nullptr), flags(0), visible(false) {}
};

struct TileMap {
    const uint8_t* data;       // 2D-массив id тайлов (PROGMEM или RAM)
    int            cols;       // ширина карты в тайлах
    int            rows;       // высота карты в тайлах
    int            tileW;      // ширина тайла в пикселях
    int            tileH;      // высота тайла в пикселях
    TileDef        tiles[TILEMAP_MAX_TILES]; // таблица тайлов (заполнить вручную)

    TileMap() : data(nullptr), cols(0), rows(0), tileW(16), tileH(16) {}

    // Инициализация карты
    void init(const uint8_t* mapData, int c, int r, int tw = 16, int th = 16) {
        data  = mapData;
        cols  = c;
        rows  = r;
        tileW = tw;
        tileH = th;
    }

    // Определить тайл: id, цвет, флаги, опционально bitmap
    void setTileDef(uint8_t id, uint16_t color, uint8_t flags = 0,
                    const uint16_t* bmp = nullptr, bool visible = true) {
        if (id >= TILEMAP_MAX_TILES) return;
        tiles[id].color   = color;
        tiles[id].bitmap  = bmp;
        tiles[id].flags   = flags;
        tiles[id].visible = visible;
    }

    // Получить id тайла по тайловым координатам
    uint8_t getTile(int col, int row) const {
        if (!data || col < 0 || row < 0 || col >= cols || row >= rows) return 0;
        return data[row * cols + col];
    }

    // Получить тайловые координаты по мировым
    void worldToTile(float wx, float wy, int& col, int& row) const {
        col = (int)(wx / tileW);
        row = (int)(wy / tileH);
    }

    // Проверить флаг тайла по мировым координатам
    bool hasFlag(float wx, float wy, uint8_t flag) const {
        int c, r;
        worldToTile(wx, wy, c, r);
        uint8_t id = getTile(c, r);
        return (tiles[id].flags & flag) != 0;
    }

    // Rect тайла в мировых координатах
    Rect tileRect(int col, int row) const {
        return Rect(col * tileW, row * tileH, tileW, tileH);
    }

    // draw() объявлен как свободная функция ниже (после Camera):
    // tilemap_draw(tmap, canvas, cam, screenW, screenH)

    // Разрешить коллизии Entity со всеми solid/platform тайлами
    // Возвращает true если Entity стоит на твёрдом тайле
    bool resolveEntity(Transform& t, Physics& phys) const {
        if (!data) return false;
        Rect eb = t.bounds();
        bool onGround = false;

        // Проверяем тайлы вокруг Entity (3x3 area)
        int c0, r0, c1, r1;
        worldToTile(eb.x,          eb.y,          c0, r0);
        worldToTile(eb.x + eb.w,   eb.y + eb.h,   c1, r1);
        c0--; r0--; c1++; r1++;
        c0 = c0 < 0 ? 0 : c0;
        r0 = r0 < 0 ? 0 : r0;
        c1 = c1 >= cols ? cols - 1 : c1;
        r1 = r1 >= rows ? rows - 1 : r1;

        for (int r = r0; r <= r1; r++) {
            for (int c = c0; c <= c1; c++) {
                uint8_t id = getTile(c, r);
                if (id == 0) continue;
                uint8_t flags = tiles[id].flags;
                if (!(flags & (TILE_FLAG_SOLID | TILE_FLAG_PLATFORM))) continue;

                Rect platform = tileRect(c, r);

                // Односторонняя платформа — только если падаем
                if ((flags & TILE_FLAG_PLATFORM) && !(flags & TILE_FLAG_SOLID)) {
                    if (t.vel.y <= 0) continue;  // игнорируем если прыгаем вверх
                    // Только верхний край
                    float bottom = t.pos.y + t.size.y / 2;
                    float prevBottom = bottom - t.vel.y;
                    if (prevBottom > platform.y) continue; // были ниже — не цепляем
                }

                if (phys.resolveRect(t, platform)) onGround = true;
            }
        }
        return onGround;
    }
};

// ----------------------------------------------------------------
//  Entity — базовый игровой объект
// ----------------------------------------------------------------
#define MAX_ENTITIES 32

using EntityID = uint8_t;
static const EntityID ENTITY_NONE = 255;

struct Entity {
    EntityID  id;
    bool      alive;
    bool      dynamic;   // false = статичный (не двигается physics/velocity)
    Transform transform;
    Sprite    sprite;
    uint8_t   group;       // группа/тип (0=player, 1=enemy, 2=food и т.д.)
    float     health;
    void*     userData;    // любые дополнительные данные

    AnimationPlayer anim;   // анимация (необязательна)

    Entity() : id(ENTITY_NONE), alive(false), dynamic(true),
               group(0), health(1.0f), userData(nullptr) {}
};

// ----------------------------------------------------------------
//  Camera — камера (смещение мира)
// ----------------------------------------------------------------
struct Camera {
    Vec2 pos;      // левый верхний угол того, что видно
    float zoom;

    Camera() : zoom(1.0f) {}

    // Перевод мировых координат в экранные
    Vec2 worldToScreen(Vec2 world) const {
        return (world - pos) * zoom;
    }

    // Плавное следование за целью
    void follow(Vec2 target, float screenW, float screenH, float lerp = 0.1f) {
        Vec2 desired = target - Vec2(screenW / 2, screenH / 2);
        pos.x += (desired.x - pos.x) * lerp;
        pos.y += (desired.y - pos.y) * lerp;
    }
};

// ----------------------------------------------------------------
//  tilemap_draw — рисование TileMap (объявлено после Camera)
//  Использование: tilemap_draw(tmap, canvas, cam, 320, 240);
// ----------------------------------------------------------------
inline void tilemap_draw(const TileMap& tm, Adafruit_GFX* canvas,
                         const Camera& cam, int screenW, int screenH) {
    if (!tm.data) return;
    float z = cam.zoom < 0.01f ? 0.01f : cam.zoom;
    int c0 = (int)(cam.pos.x / tm.tileW);
    int r0 = (int)(cam.pos.y / tm.tileH);
    int c1 = c0 + (int)(screenW / (tm.tileW * z)) + 2;
    int r1 = r0 + (int)(screenH / (tm.tileH * z)) + 2;
    c0 = c0 < 0 ? 0 : c0;
    r0 = r0 < 0 ? 0 : r0;
    c1 = c1 > tm.cols ? tm.cols : c1;
    r1 = r1 > tm.rows ? tm.rows : r1;

    for (int r = r0; r < r1; r++) {
        for (int c = c0; c < c1; c++) {
            uint8_t id = tm.getTile(c, r);
            if (id == 0) continue;
            const TileDef& td = tm.tiles[id];
            if (!td.visible) continue;

            Vec2 sc = cam.worldToScreen(Vec2(c * tm.tileW, r * tm.tileH));
            int sx = (int)sc.x;
            int sy = (int)sc.y;
            int sw = (int)(tm.tileW * z);
            int sh = (int)(tm.tileH * z);

            if (sx + sw <= 0 || sy + sh <= 0 || sx >= screenW || sy >= screenH)
                continue;

            if (td.bitmap) {
                canvas->drawRGBBitmap(sx, sy, td.bitmap, sw, sh);
            } else {
                canvas->fillRect(sx, sy, sw, sh, td.color);
            }
        }
    }
}

// ----------------------------------------------------------------
//  Input — состояние тача (XPT2046)
// ----------------------------------------------------------------
struct Input {
    bool  touched;
    bool  justPressed;   // только первый кадр касания
    bool  justReleased;  // только первый кадр отпускания
    Vec2  pos;           // экранные координаты
    Vec2  dragDelta;     // смещение за кадр (для скролла/свайпа)
    Vec2  pressPos;      // где нажали (для drag distance)

private:
    bool _prevTouched;

public:
    Input() : touched(false), justPressed(false), justReleased(false), _prevTouched(false) {}

    // Вызывается движком каждый кадр
    void update(bool nowTouched, int rawX, int rawY) {
        justPressed  = nowTouched && !_prevTouched;
        justReleased = !nowTouched && _prevTouched;

        if (justPressed) pressPos = Vec2(rawX, rawY);

        // dragDelta: вычисляем ДО обновления pos, и только если оба кадра были touched
        Vec2 newPos = Vec2(rawX, rawY);
        dragDelta = (nowTouched && _prevTouched) ? (newPos - pos) : Vec2(0, 0);

        touched      = nowTouched;
        pos          = newPos;
        _prevTouched = nowTouched;
    }
};

// ----------------------------------------------------------------
//  Renderer — рисование через Adafruit_GFX (SplitCanvas)
// ----------------------------------------------------------------
class Renderer {
public:
    Adafruit_GFX* canvas;
    Camera*       camera;

    Renderer() : canvas(nullptr), camera(nullptr) {}

    void setCanvas(Adafruit_GFX* c) { canvas = c; }
    void setCamera(Camera* cam)     { camera = cam; }

    void clear(uint16_t color = 0x0000) {
        canvas->fillScreen(color);
    }

    // Перевод мировых → экранных с учётом камеры
    Vec2 toScreen(Vec2 world) const {
        if (camera) return camera->worldToScreen(world);
        return world;
    }

    void drawEntity(const Entity& e) {
        if (!e.alive || e.sprite.shape == SHAPE_NONE) return;

        Vec2  sc = toScreen(e.transform.pos);
        int   x  = (int)sc.x;
        int   y  = (int)sc.y;
        float r  = e.transform.radius;
        Vec2  sz = e.transform.size;

        switch (e.sprite.shape) {
        case SHAPE_CIRCLE:
            canvas->fillCircle(x, y, (int)r, e.sprite.color);
            if (e.sprite.hasOutline)
                canvas->drawCircle(x, y, (int)r, e.sprite.outlineColor);
            break;

        case SHAPE_RECT: {
            int rx = x - (int)(sz.x / 2);
            int ry = y - (int)(sz.y / 2);
            canvas->fillRect(rx, ry, (int)sz.x, (int)sz.y, e.sprite.color);
            if (e.sprite.hasOutline)
                canvas->drawRect(rx, ry, (int)sz.x, (int)sz.y, e.sprite.outlineColor);
            break;
        }
        case SHAPE_ROUND_RECT: {
            int rx = x - (int)(sz.x / 2);
            int ry = y - (int)(sz.y / 2);
            canvas->fillRoundRect(rx, ry, (int)sz.x, (int)sz.y, e.sprite.cornerRadius, e.sprite.color);
            if (e.sprite.hasOutline)
                canvas->drawRoundRect(rx, ry, (int)sz.x, (int)sz.y, e.sprite.cornerRadius, e.sprite.outlineColor);
            break;
        }
        case SHAPE_SPRITE: {
            if (!e.sprite.bitmapData) break;  // защита от nullptr
            int bw = e.sprite.bitmapW;
            int bh = e.sprite.bitmapH;
            if (bw <= 0 || bh <= 0) break;    // защита от плохих размеров
            int rx = x - bw / 2;
            int ry = y - bh / 2;
            if (e.sprite.transparentColor != 0xFFFF) {
                // Попиксельный рендер с пропуском прозрачного цвета
                uint16_t tc  = e.sprite.transparentColor;
                int      max = bw * bh;
                for (int py = 0; py < bh; py++) {
                    for (int px = 0; px < bw; px++) {
                        int idx = py * bw + px;
                        if (idx >= max) continue;  // защита от выхода за буфер
                        uint16_t c = e.sprite.progmem
                            ? pgm_read_word(&e.sprite.bitmapData[idx])
                            : e.sprite.bitmapData[idx];
                        if (c != tc) canvas->drawPixel(rx + px, ry + py, c);
                    }
                }
            } else {
                canvas->drawRGBBitmap(rx, ry, e.sprite.bitmapData, bw, bh);
            }
            break;
        }
        default: break;
        }
    }

    // Текст
    void text(const char* str, Vec2 pos, uint16_t color = 0xFFFF, int size = 1) {
        canvas->setTextColor(color);
        canvas->setTextSize(size);
        canvas->setCursor((int)pos.x, (int)pos.y);
        canvas->print(str);
    }
    void textf(Vec2 pos, uint16_t color, int size, const char* fmt, ...) {
        char buf[64];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        text(buf, pos, color, size);
    }

    // Примитивы напрямую (без камеры, для HUD)
    void hud_rect(int x, int y, int w, int h, uint16_t color) {
        canvas->fillRect(x, y, w, h, color);
    }
    void hud_text(const char* str, int x, int y, uint16_t color = 0xFFFF, int size = 1) {
        canvas->setTextColor(color);
        canvas->setTextSize(size);
        canvas->setCursor(x, y);
        canvas->print(str);
    }
};

// ----------------------------------------------------------------
//  Collider — проверки столкновений
// ----------------------------------------------------------------
namespace Collider {
    inline bool circle_circle(const Entity& a, const Entity& b) {
        return Vec2::dist(a.transform.pos, b.transform.pos)
               < (a.transform.radius + b.transform.radius);
    }
    inline bool aabb_aabb(const Entity& a, const Entity& b) {
        return a.transform.bounds().overlaps(b.transform.bounds());
    }
    inline bool circle_rect(const Entity& circ, const Entity& rect) {
        Rect rb = rect.transform.bounds();
        Vec2 cp = circ.transform.pos;
        float cx = fmaxf(rb.x, fminf(cp.x, rb.x + rb.w));
        float cy = fmaxf(rb.y, fminf(cp.y, rb.y + rb.h));
        float dx = cp.x - cx, dy = cp.y - cy;
        float r  = circ.transform.radius;
        return (dx * dx + dy * dy) < (r * r);  // без sqrtf — быстрее
    }
    inline bool point_in_entity(Vec2 p, const Entity& e) {
        if (e.transform.radius > 0) {
            return Vec2::dist(p, e.transform.pos) < e.transform.radius;
        }
        return e.transform.bounds().contains(p);
    }
}

// ----------------------------------------------------------------
//  Timer — таймеры и интервальные события
// ----------------------------------------------------------------
struct Timer {
    unsigned long interval;   // миллисекунды
    unsigned long _last;
    bool          oneShot;
    bool          fired;

    Timer(unsigned long ms = 1000, bool oneShot = false)
        : interval(ms), _last(0), oneShot(oneShot), fired(false) {}

    void reset() { _last = millis(); fired = false; }

    // Вернёт true, если прошёл интервал (и, если oneShot, только один раз)
    bool tick() {
        if (oneShot && fired) return false;
        if (millis() - _last >= interval) {
            _last += interval;   // без дрейфа: шагаем вперёд ровно на interval
            fired = true;
            return true;
        }
        return false;
    }
};

// ----------------------------------------------------------------
//  FrameTimer — ограничитель FPS (без delay, на millis)
// ----------------------------------------------------------------
struct FrameTimer {
    unsigned long targetMs;   // целевой мс/кадр (16 = ~60fps, 33 = ~30fps)
    unsigned long _last;
    unsigned long _delta;     // реальное время последнего кадра в мс

    FrameTimer(int targetFps = 30) : targetMs(1000 / targetFps), _last(0), _delta(0) {}

    // Вернёт true когда пора рисовать следующий кадр
    bool ready() {
        unsigned long now = millis();
        if (now - _last >= targetMs) {
            _delta = now - _last;
            _last  = now;
            return true;
        }
        return false;
    }

    float deltaSeconds() const { return _delta / 1000.0f; }
};

// ----------------------------------------------------------------
//  Scene — хранилище Entity + update + render
// ----------------------------------------------------------------
class Scene {
public:
    Entity    entities[MAX_ENTITIES];
    Renderer* renderer;
    Camera    camera;
    // count для совместимости с игровым кодом;
    // обновляется в spawn/killAll автоматически
    int       count;

    Scene() : renderer(nullptr), count(MAX_ENTITIES) {}

    void setRenderer(Renderer* r) {
        renderer = r;
        renderer->setCamera(&camera);
    }

    // Создать новый Entity и вернуть указатель на него
    Entity* spawn(uint8_t group = 0) {
        for (int i = 0; i < MAX_ENTITIES; i++) {
            if (!entities[i].alive) {
                entities[i] = Entity();
                entities[i].id    = i;
                entities[i].alive = true;
                entities[i].group = group;
                return &entities[i];
            }
        }
        return nullptr; // нет места
    }

    void kill(EntityID id) {
        if (id < MAX_ENTITIES) entities[id].alive = false;
    }

    Entity* get(EntityID id) {
        if (id < MAX_ENTITIES && entities[id].alive) return &entities[id];
        return nullptr;
    }

    // Обновить анимации всех живых Entity (вызывать каждый кадр)
    void updateAnims() {
        for (int i = 0; i < MAX_ENTITIES; i++) {
            if (!entities[i].alive) continue;
            if (entities[i].anim.update())
                entities[i].anim.applyTo(entities[i].sprite);
        }
    }

    // Применить скорость ко всем живым динамическим Entity
    void applyVelocities() {
        for (int i = 0; i < MAX_ENTITIES; i++) {
            if (entities[i].alive && entities[i].dynamic)
                entities[i].transform.applyVelocity();
        }
    }

    // Отрисовать все живые Entity (в порядке добавления)
    void render(uint16_t clearColor = 0x0000) {
        if (!renderer) return;
        renderer->clear(clearColor);
        for (int i = 0; i < MAX_ENTITIES; i++) {
            if (entities[i].alive)
                renderer->drawEntity(entities[i]);
        }
    }

    // Найти первый Entity в группе, столкнувшийся с данным
    Entity* firstCollision(const Entity& e, uint8_t group) {
        for (int i = 0; i < MAX_ENTITIES; i++) {
            Entity& o = entities[i];
            if (!o.alive || o.group != group || o.id == e.id) continue;
            bool hit;
            if (e.transform.radius > 0 && o.transform.radius > 0)
                hit = Collider::circle_circle(e, o);
            else if (e.transform.radius > 0)
                hit = Collider::circle_rect(e, o);
            else if (o.transform.radius > 0)
                hit = Collider::circle_rect(o, e);
            else
                hit = Collider::aabb_aabb(e, o);
            if (hit) return &o;
        }
        return nullptr;
    }

    // Количество живых Entity в группе
    int countAlive(uint8_t group) {
        int n = 0;
        for (int i = 0; i < MAX_ENTITIES; i++)
            if (entities[i].alive && entities[i].group == group) n++;
        return n;
    }

    void killAll(uint8_t group) {
        for (int i = 0; i < MAX_ENTITIES; i++)
            if (entities[i].alive && entities[i].group == group)
                entities[i].alive = false;
    }

    void killAll() {
        for (int i = 0; i < MAX_ENTITIES; i++) entities[i].alive = false;
    }
};

// ================================================================
//  EXAMPLE — Как написать игру поверх движка
//
//  Группы объектов:
//    GROUP_PLAYER = 0
//    GROUP_ENEMY  = 1
//    GROUP_FOOD   = 2
//
//  Код игры в MyGame.h / MyGame.cpp:
//
//  ---- MyGame.h ----
//    #include "Engine2D.h"
//    void initMyGame();
//    void tickMyGame();
//
//  ---- MyGame.cpp ----
//    #include "MyGame.h"
//    #include "Display.h"
//    #include <XPT2046_Touchscreen.h>
//    extern XPT2046_Touchscreen ts;
//    extern void getPoint(int&, int&);
//    extern void showMenu(bool);
//
//    static Scene    scene;
//    static Renderer rend;
//    static Input    input;
//    static FrameTimer ftimer(30);
//    static EntityID playerId = ENTITY_NONE;
//
//    void initMyGame() {
//        rend.setCanvas(bDisp.getCanvas());
//        scene.setRenderer(&rend);
//        scene.killAll();
//
//        // Создаём игрока
//        Entity* p = scene.spawn(0); // group 0 = player
//        p->transform = Transform(160, 120, 10.f); // pos + radius
//        p->sprite    = Sprite::circle(0x07E0, 0xFFFF, true);
//        playerId     = p->id;
//
//        // Несколько врагов
//        for (int i = 0; i < 3; i++) {
//            Entity* e = scene.spawn(1); // group 1 = enemy
//            e->transform = Transform(random(20,300), random(30,220), 15.f);
//            e->sprite    = Sprite::circle(0xF800, 0xFFFF, true);
//            float ang = random(0, 628) / 100.f;
//            e->transform.vel = Vec2(cosf(ang)*1.5f, sinf(ang)*1.5f);
//        }
//    }
//
//    void tickMyGame() {
//        if (!ftimer.ready()) return; // ограничение FPS
//
//        // --- Тач ---
//        bool tch = ts.touched();
//        int tx = 160, ty = 120;
//        if (tch) getPoint(tx, ty);
//        input.update(tch, tx, ty);
//
//        // --- Логика ---
//        Entity* player = scene.get(playerId);
//        if (player && input.touched) {
//            Vec2 dir = (input.pos - player->transform.pos).normalized();
//            player->transform.vel = dir * 2.5f;
//        }
//        scene.applyVelocities();
//        scene.updateAnims();   // обновить анимации
//
//        // Столкновения игрок vs враги
//        if (player) {
//            Entity* hit = scene.firstCollision(*player, 1);
//            if (hit) { scene.killAll(); initMyGame(); } // game over
//        }
//
//        // --- Рендер ---
//        scene.render(0x0000);
//        rend.hud_rect(0, 0, 320, 18, 0x10A2);
//        rend.hud_text("MY GAME", 8, 4, 0xFFFF, 1);
//        bDisp.update();
//    }
//
// ================================================================

#endif // ENGINE2D_H
