#include "Snake.h"
#include "Display.h" 
#include <Preferences.h>
#include <XPT2046_Touchscreen.h>

extern void showMenu(bool fullRedraw); // Используем showMenu для единства системы
extern void getPoint(int &x, int &y);
extern XPT2046_Touchscreen ts;

#define COL_BG      0x1084
#define COL_GRID    0x18C5
#define COL_SNAKE   0x07E0
#define COL_BODY    0x03E0
#define COL_APPLE   0xF800
#define COL_GLASSES 0x0000

int sn_X[100], sn_Y[100], sn_L, sn_D, f_X, f_Y;
unsigned long sn_T;
int sn_score = 0, sn_highScore = 0, last_sn_D;
bool isGameOverSnake = false; // Флаг состояния

Preferences prefSnake;

void drawSnakeHead(int x, int y, int dir) {
    auto* canvas = bDisp.getCanvas();
    canvas->fillRoundRect(x, y, 10, 10, 3, COL_SNAKE);
    canvas->fillRect(x + 1, y + 1, 8, 3, COL_GLASSES); // Очки
    canvas->drawPixel(x + 2, y + 1, 0xFFFF);
    canvas->drawPixel(x + 6, y + 1, 0xFFFF);
}

void drawFood(int x, int y) {
    auto* canvas = bDisp.getCanvas();
    canvas->fillCircle(x + 5, y + 6, 4, COL_APPLE);
    canvas->fillRect(x + 5, y + 1, 2, 3, 0x5140); // Черенок
    canvas->drawPixel(x + 6, y, 0x07E0);          // Листик
}

void drawSnakeExit() {
    auto* canvas = bDisp.getCanvas();
    canvas->fillRect(285, 5, 30, 25, 0xB000);
    canvas->drawRect(285, 5, 30, 25, 0xFFFF);
    canvas->setCursor(295, 10);
    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(2);
    canvas->print("X");
}

void spawnFood() {
    f_X = random(1, 30) * 10;
    f_Y = random(4, 22) * 10;
}

void initSnake() {
    prefSnake.begin("snake", true);
    sn_highScore = prefSnake.getInt("hscore", 0);
    prefSnake.end();

    sn_L = 4;
    sn_D = 1;
    last_sn_D = 1;
    sn_X[0] = 160;
    sn_Y[0] = 120;
    sn_score = 0;
    isGameOverSnake = false;

    for(int i=0; i<sn_L; i++) {
        sn_X[i] = 160 - (i * 10);
        sn_Y[i] = 120;
    }

    spawnFood();
    sn_T = millis();
}

void tickSnake() {
    auto* canvas = bDisp.getCanvas();

    // 1. ПРОВЕРКА ТАЧА (В начале, как в Кубе)
    if (ts.touched()) {
        int tx, ty;
        getPoint(tx, ty);

        // Мгновенный выход
        if (tx > 270 && ty < 40) {
            showMenu(true);
            return;
        }

        // Рестарт если проиграли
        if (isGameOverSnake) {
            initSnake();
            return;
        }

        // Управление направлением
        int dx = tx - sn_X[0], dy = ty - sn_Y[0];
        if (abs(dx) > abs(dy)) {
            if (dx > 10 && last_sn_D != 3) sn_D = 1;
            else if (dx < -10 && last_sn_D != 1) sn_D = 3;
        } else {
            if (dy > 10 && last_sn_D != 0) sn_D = 2;
            else if (dy < -10 && last_sn_D != 2) sn_D = 0;
        }
    }

    // Если игра окончена — рисуем экран смерти и ждём тача (без задержек!)
    if (isGameOverSnake) {
        canvas->fillScreen(0x0000);
        canvas->setTextColor(0xF800);
        canvas->setTextSize(3);
        canvas->setCursor(80, 90);
        canvas->print("GAME OVER");
        
        canvas->setTextColor(0xFFFF);
        canvas->setTextSize(2);
        canvas->setCursor(90, 140);
        canvas->print("SCORE: "); canvas->print(sn_score);
        
        drawSnakeExit();
        return; 
    }

    // 2. ЛОГИКА ДВИЖЕНИЯ
    if (millis() - sn_T > 110) {
        sn_T = millis();
        last_sn_D = sn_D;

        for(int i = sn_L - 1; i > 0; i--) {
            sn_X[i] = sn_X[i-1];
            sn_Y[i] = sn_Y[i-1];
        }

        if(sn_D == 0) sn_Y[0] -= 10;
        else if(sn_D == 1) sn_X[0] += 10;
        else if(sn_D == 2) sn_Y[0] += 10;
        else if(sn_D == 3) sn_X[0] -= 10;

        // Столкновения
        if(sn_X[0] < 0 || sn_X[0] >= 320 || sn_Y[0] < 35 || sn_Y[0] >= 240) {
            isGameOverSnake = true;
        }
        for(int i = 1; i < sn_L; i++) {
            if(sn_X[0] == sn_X[i] && sn_Y[0] == sn_Y[i]) isGameOverSnake = true;
        }

        if (isGameOverSnake) {
            if (sn_score > sn_highScore) {
                prefSnake.begin("snake", false);
                prefSnake.putInt("hscore", sn_score);
                prefSnake.end();
                sn_highScore = sn_score;
            }
            return;
        }

        // Поедание яблока
        if(abs(sn_X[0] - f_X) < 8 && abs(sn_Y[0] - f_Y) < 8) {
            if (sn_L < 100) sn_L++;
            sn_score += 10;
            spawnFood();
        }
    }

    // 3. ОТРИСОВКА КАДРА
    canvas->fillScreen(COL_BG);

    // Сетка
    for(int i=0; i<=320; i+=10) canvas->drawFastVLine(i, 35, 205, COL_GRID);
    for(int i=35; i<=240; i+=10) canvas->drawFastHLine(0, i, 320, COL_GRID);

    // Верхняя панель
    canvas->fillRect(0, 0, 320, 35, 0x34BF);
    canvas->drawFastHLine(0, 34, 320, 0xFFFF);
    drawSnakeExit();

    canvas->setCursor(10, 10);
    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(1);
    canvas->print("XP: "); canvas->print(sn_score);
    canvas->print(" | BEST: "); canvas->print(max(sn_score, sn_highScore));

    drawFood(f_X, f_Y); // Яблоко

    // Змейка
    for(int i = 1; i < sn_L; i++) {
        canvas->fillRoundRect(sn_X[i], sn_Y[i], 9, 9, 2, COL_BODY);
    }
    drawSnakeHead(sn_X[0], sn_Y[0], sn_D);
}