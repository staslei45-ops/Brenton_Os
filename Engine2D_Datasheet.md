# Engine2D — Датащит (полный справочник команд)
> ESP32 + ILI9341 + SplitCanvas · header-only · `#include "Engine2D.h"`

---

## Vec2 — 2D вектор

```cpp
Vec2 v;                       // (0, 0)
Vec2 v(x, y);                 // задать координаты

v + o   v - o   v * s         // арифметика (новый Vec2)
v += o  v -= o                // изменение на месте

v.length()                    // длина вектора
v.normalized()                // единичный вектор (не меняет v)
v.dot(o)                      // скалярное произведение
Vec2::dist(a, b)              // расстояние между двумя точками
```

---

## Rect — прямоугольник

```cpp
Rect r(x, y, w, h);

r.contains(Vec2 p)            // точка внутри?
r.overlaps(other)             // пересекается с другим Rect?
r.center()                    // → Vec2 центра
r.expand(d)                   // расширить на d пикселей со всех сторон
```

---

## Transform — позиция / скорость / размер

```cpp
Transform t;
Transform t(x, y, w, h);     // прямоугольный объект
Transform t(x, y, radius);   // круглый объект

t.pos                         // Vec2 — мировая позиция (центр)
t.vel                         // Vec2 — скорость (пикс/кадр)
t.size                        // Vec2 — ширина и высота
t.radius                      // float — радиус (0 = не круг)
t.angle                       // float — угол (рад), только для рендера

t.bounds()                    // → Rect AABB
t.applyVelocity()             // pos += vel (вызывается автоматически scene)
```

---

## Sprite — внешний вид

```cpp
// Фабричные методы:
Sprite::circle(color)
Sprite::circle(color, outlineColor, hasOutline)

Sprite::rect(color)
Sprite::rect(color, outlineColor, hasOutline)

Sprite::roundRect(color, cornerRadius, outlineColor, hasOutline)

Sprite::bitmap(data, w, h)
Sprite::bitmap(data, w, h, transpColor)   // цвет прозрачности

// Поля:
sp.shape          // SHAPE_CIRCLE / SHAPE_RECT / SHAPE_ROUND_RECT
                  // SHAPE_SPRITE / SHAPE_NONE
sp.color          // uint16_t RGB565
sp.outlineColor
sp.hasOutline
sp.cornerRadius   // для ROUND_RECT
sp.bitmapData     // const uint16_t* PROGMEM
sp.bitmapW / bitmapH
sp.transparentColor
```

---

## AnimFrame / Animation / AnimationPlayer

```cpp
// --- Создание анимации ---
Animation walk;
walk.loop = true;                          // зациклить (по умолчанию true)
walk.addFrame(bitmap_ptr, 100);            // кадр + задержка в мс
walk.addFrame(bitmap_ptr2, 100);
// до 16 кадров (MAX_ANIM_FRAMES)

// --- Воспроизведение (поле entity->anim) ---
entity->anim.play(&walk);     // запустить анимацию
entity->anim.stop();          // пауза
entity->anim.resume();        // продолжить

entity->anim.playing          // bool — играет?
entity->anim.finished         // bool — завершён oneShot?
entity->anim.currentFrame     // uint8_t — текущий кадр

// --- В игровом цикле ---
scene.updateAnims();          // обновить все анимации (вызывать каждый кадр)

// --- Вручную ---
entity->anim.update();        // → bool: кадр сменился?
entity->anim.applyTo(sprite); // применить кадр к спрайту
```

---

## Physics — физика

```cpp
Physics phys;
phys.gravity      = 0.5f;    // ускорение вниз (пикс/кадр²)
phys.friction     = 0.85f;   // торможение по X (0..1)
phys.jumpForce    = -10.0f;  // скорость прыжка (отрицательная!)
phys.maxFallSpeed = 15.0f;   // макс. скорость падения
phys.groundY      = 220.0f;  // Y простого пола
phys.useGround    = true;    // включить простой пол

// --- Применить физику к одному Transform ---
bool onGround = phys.applyTo(entity->transform);
// applyTo: добавляет gravity, умножает vel.x на friction,
//          двигает pos, проверяет простой пол

// --- Прыжок ---
if (onGround) phys.jump(entity->transform);

// --- Коллизия с произвольным прямоугольником ---
bool landed = phys.resolveRect(entity->transform, Rect(x,y,w,h));

// --- Коллизия с TileMap (см. ниже) ---
bool onGround = tileMap.resolveEntity(entity->transform, phys);
```

---

## TileMap — тайловая карта

```cpp
// --- Объявление карты ---
const uint8_t mapData[] = {
    0,0,0,1,1,
    0,0,0,0,0,
    1,1,1,1,1,
};
TileMap tmap;
tmap.init(mapData, cols=5, rows=3, tileW=16, tileH=16);

// --- Определение тайлов ---
//   id=0 обычно пустой тайл (невидимый, нет флагов)
tmap.setTileDef(0, 0x0000, 0, nullptr, false);  // пустой
tmap.setTileDef(1, 0x07E0, TILE_FLAG_SOLID);    // зелёная земля
tmap.setTileDef(2, 0xF800, TILE_FLAG_SPIKE);    // шип (красный)
tmap.setTileDef(3, 0xFFE0, TILE_FLAG_PLATFORM); // жёлтая платформа (1-сторонняя)
tmap.setTileDef(4, 0x001F, TILE_FLAG_LADDER);   // синяя лестница
// С bitmap:
tmap.setTileDef(5, 0, TILE_FLAG_SOLID, grassBitmap);

// Флаги тайлов:
// TILE_FLAG_SOLID    — непроходимый (блокирует со всех сторон)
// TILE_FLAG_LADDER   — лестница (логика в игре)
// TILE_FLAG_SPIKE    — наносит урон (логика в игре)
// TILE_FLAG_PLATFORM — односторонняя (блокирует только сверху)

// --- Рисование карты ---
tmap.draw(canvas, scene.camera, screenW=320, screenH=240);
// (вызывать до render() — тайлы под Entity)

// --- Коллизии ---
bool onGround = tmap.resolveEntity(entity->transform, phys);

// --- Утилиты ---
tmap.getTile(col, row)              // → uint8_t id тайла
tmap.worldToTile(wx, wy, col, row)  // мировые → тайловые координаты
tmap.tileRect(col, row)             // → Rect тайла в мире
tmap.hasFlag(wx, wy, TILE_FLAG_SPIKE) // есть флаг в точке мира?
```

---

## Entity — игровой объект

```cpp
// Поля:
entity->id           // EntityID (uint8_t)
entity->alive        // bool
entity->transform    // Transform
entity->sprite       // Sprite
entity->group        // uint8_t — группа (0=player, 1=enemy…)
entity->health       // float
entity->userData     // void* — любые данные
entity->anim         // AnimationPlayer
```

---

## Camera — камера

```cpp
scene.camera.pos           // Vec2 — левый верхний угол обзора
scene.camera.zoom          // float — масштаб (1.0 = нормально)

scene.camera.follow(targetPos, screenW, screenH)
scene.camera.follow(targetPos, screenW, screenH, lerp=0.1f)
// lerp: 0.0=неподвижна, 1.0=мгновенно, 0.1=плавно

scene.camera.worldToScreen(Vec2 world)  // → Vec2 экранные координаты
```

---

## Scene — хранилище объектов

```cpp
Scene scene;
scene.setRenderer(&rend);         // подключить рендерер

// --- Создание / удаление ---
Entity* e = scene.spawn();        // group=0
Entity* e = scene.spawn(group);
scene.kill(id);                   // убить по id
scene.killAll();                  // убить всех + сброс count
scene.killAll(group);             // убить всех в группе

// --- Доступ ---
Entity* e = scene.get(id);        // nullptr если не живой

// --- Обновление ---
scene.applyVelocities();          // pos += vel для всех живых
scene.updateAnims();              // обновить все AnimationPlayer

// --- Рендер ---
scene.render();                   // очистка чёрным + рисовать всех
scene.render(clearColor);         // своим цветом (RGB565)

// --- Коллизии ---
Entity* hit = scene.firstCollision(entity, group);
// → первый живой Entity из group, столкнувшийся с entity (nullptr если нет)

// --- Статистика ---
int n = scene.countAlive(group);  // сколько живых в группе
```

---

## Renderer — рисование

```cpp
Renderer rend;
rend.setCanvas(canvas);           // Adafruit_GFX* (SplitCanvas)
rend.setCamera(&scene.camera);    // (делает setRenderer автоматически)

// --- Мировые координаты (с камерой) ---
rend.drawEntity(entity);          // нарисовать Entity
rend.toScreen(Vec2 world)         // → Vec2 экранные координаты
rend.clear(color);                // залить экран

// --- Текст в мире ---
rend.text("str", Vec2(x,y), color, size);
rend.textf(Vec2(x,y), color, size, "Score: %d", score);

// --- HUD (без камеры, прямо на экран) ---
rend.hud_rect(x, y, w, h, color);
rend.hud_text("str", x, y, color, size);
```

---

## Collider — проверки столкновений

```cpp
Collider::circle_circle(entityA, entityB)   // → bool
Collider::aabb_aabb(entityA, entityB)       // → bool
Collider::circle_rect(circEntity, rectEntity) // → bool
Collider::point_in_entity(Vec2 p, entity)   // → bool
```

---

## Input — тач-ввод

```cpp
Input input;
input.update(touched, rawX, rawY);   // вызывать каждый кадр

input.touched          // bool — экран касается сейчас
input.justPressed      // bool — только первый кадр нажатия
input.justReleased     // bool — только первый кадр отпускания
input.pos              // Vec2 — текущая позиция пальца
input.pressPos         // Vec2 — где нажали
input.dragDelta        // Vec2 — смещение за кадр (для свайпа/скролла)
```

---

## Timer / FrameTimer

```cpp
// --- Timer (интервал в мс) ---
Timer t(1000);                // каждую секунду
Timer t(500, true);           // oneShot — только один раз
t.reset();                    // перезапустить
bool fired = t.tick();        // вызывать каждый кадр

// --- FrameTimer (ограничение FPS) ---
FrameTimer ft(30);            // 30 fps
if (!ft.ready()) return;      // вставить в начало tickGame()
float dt = ft.deltaSeconds(); // реальное время кадра в секундах
```

---

## Типичный игровой цикл

```cpp
void tickGame() {
    if (!ftimer.ready()) return;       // FPS lock

    // 1. Ввод
    input.update(ts.touched(), tx, ty);

    // 2. Логика / физика
    phys.applyTo(player->transform);
    bool onGround = tmap.resolveEntity(player->transform, phys);
    if (onGround && input.justPressed) phys.jump(player->transform);

    // 3. Анимации
    scene.updateAnims();

    // 4. Коллизии между Entity
    Entity* hit = scene.firstCollision(*player, GROUP_ENEMY);
    if (hit) { /* game over */ }

    // 5. Камера
    scene.camera.follow(player->transform.pos, 320, 240);

    // 6. Рендер
    rend.clear(0x0000);
    tmap.draw(canvas, scene.camera, 320, 240);  // тайлы
    scene.render(0x0000);                        // Entity поверх
    rend.hud_text("SCORE: 0", 8, 4, 0xFFFF, 1); // HUD
    bDisp.update();
}
```

---

## Константы

| Константа | Значение | Описание |
|---|---|---|
| `MAX_ENTITIES` | 32 | Макс. объектов в Scene |
| `MAX_ANIM_FRAMES` | 16 | Макс. кадров в Animation |
| `TILEMAP_MAX_TILES` | 64 | Макс. типов тайлов |
| `TILEMAP_MAX_COLS` | 40 | Макс. ширина карты |
| `TILEMAP_MAX_ROWS` | 30 | Макс. высота карты |
| `ENTITY_NONE` | 255 | Пустой EntityID |
| `TILE_FLAG_SOLID` | 0x01 | Непроходимый |
| `TILE_FLAG_LADDER` | 0x02 | Лестница |
| `TILE_FLAG_SPIKE` | 0x04 | Шип |
| `TILE_FLAG_PLATFORM` | 0x08 | Одностороняя платформа |
