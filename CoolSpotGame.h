#ifndef COOLSPOTGAME_H
#define COOLSPOTGAME_H

#include <Arduino.h>

#define TILE_EMPTY 0
#define TILE_WALL  1
#define TILE_COIN  2

enum GameStatus { GS_MENU, GS_PLAYING, GS_PAUSED, GS_DEAD };

struct EnemySpot {
    float x, y;
    int dir;
    int minX, maxX;
};

class CoolSpotGame {
public:
    void init();
    void update();
    void render();
    bool isRunning = true;

private:
    float x, y, velX, velY, camX;
    bool isGrounded, flip;
    int score, lives;
    GameStatus status;

    static const int maxEnemies = 5;
    EnemySpot enemies[maxEnemies];
    int enemyCount;

    void resetPlayer();
    bool checkCollision(float nextX, float nextY);

    // Новые функции для "красивой" отрисовки
    void drawSpot(int sx, int sy, bool flipped);
    void drawDetailedCoin(int cx, int cy);

    static const int mapW = 40;
    static const int mapH = 12;
    uint8_t levelMap[mapH][mapW];
};

#endif
