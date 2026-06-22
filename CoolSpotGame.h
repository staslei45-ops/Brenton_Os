#ifndef COOLSPOTGAME_H
#define COOLSPOTGAME_H

#include <Arduino.h>
#include "Engine2D.h"

#define TILE_EMPTY 0
#define TILE_WALL  1
#define TILE_COIN  2

enum GameStatus { GS_MENU, GS_PLAYING, GS_PAUSED, GS_DEAD };

// Группы Entity в сцене
#define GRP_PLAYER 0
#define GRP_ENEMY  1

// Размер тайла и карты
#define TILE_SIZE  20
#define MAP_W      40
#define MAP_H      12

class CoolSpotGame {
public:
    void init();
    void update();
    void render();
    bool isRunning = true;

private:
    // ---- Engine2D ----
    Scene       scene;
    Renderer    rend;
    Input       input;
    FrameTimer  ftimer;
    Camera      cam;

    EntityID    playerId;

    // ---- Тайловая карта ----
    uint8_t     levelMap[MAP_H][MAP_W];

    // ---- Состояние ----
    GameStatus  status;
    int         score;
    int         lives;
    int         enemyCount;
    int         invuln;     // кадры неуязвимости после удара

    // ---- Камера ----
    float       camX;       // горизонтальный скролл

    // ---- Вспомогательные ----
    void resetPlayer();
    bool tileAt(float wx, float wy);  // есть ли стена в мировых координатах
    void resolvePlayerTiles(Entity* p);
    void spawnEnemies();

    void drawTilemap();
    void drawSpot(int sx, int sy, bool flipped);
    void drawEnemy(int sx, int sy);
    void drawHUD();
    void drawMenu();
    void drawPause();
    void drawGameOver();
};

#endif
