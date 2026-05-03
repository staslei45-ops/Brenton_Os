#include "Global.h"
#include "Flappy.h"
#include "Display.h" 
#include <Preferences.h>
#include <XPT2046_Touchscreen.h>

extern XPT2046_Touchscreen ts;
extern void getPoint(int &x, int &y);
extern void showMenu(bool fullRedraw);

#define BG_COLOR    0x4EB1
#define PIPE_BODY   0x7644
#define PIPE_DARK   0x0500
#define BIRD_COLOR  0xFFE0
#define GRASS_COLOR 0x2E8B
#define CLOUD_COLOR 0xFFFF

float b_Y, b_V;
int p_X, g_Y;
int score = 0;
int flappy_highScore = 0;
bool isGameOver = false;

Preferences prefFlappy;

// Кнопка выхода (прозрачная или яркая — на твой вкус)
void drawFlappyExitBtn() {
    auto* canvas = bDisp.getCanvas();
    canvas->fillRect(280, 5, 35, 30, 0xB000); // Красный квадрат
    canvas->drawRect(280, 5, 35, 30, 0xFFFF); // Белая рамка
    canvas->setCursor(292, 12);
    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(2);
    canvas->print("X");
}

void drawStaticDecor() {
    auto* canvas = bDisp.getCanvas();
    // Земля и трава
    canvas->fillRect(0, 220, 320, 20, 0x9480);
    canvas->fillRect(0, 215, 320, 5, GRASS_COLOR);

    // Облако 1
    canvas->fillCircle(60, 50, 10, CLOUD_COLOR);
    canvas->fillCircle(75, 50, 12, CLOUD_COLOR);
    canvas->fillCircle(90, 50, 10, CLOUD_COLOR);

    // Облако 2
    canvas->fillCircle(240, 70, 8, CLOUD_COLOR);
    canvas->fillCircle(252, 70, 10, CLOUD_COLOR);
    canvas->fillCircle(264, 70, 8, CLOUD_COLOR);
}

void drawFlappyBird(int y) {
    auto* canvas = bDisp.getCanvas();
    canvas->fillRoundRect(50, y, 18, 12, 4, BIRD_COLOR); // Тело
    canvas->fillRect(62, y + 2, 3, 3, 0x0000);           // Глаз
    canvas->fillRect(65, y + 5, 5, 3, 0xFD20);           // Клюв
    canvas->fillRect(48, y + 5, 6, 4, 0xFFFF);           // Крыло
}

void drawFlappyPipe(int x, int y, int gap) {
    auto* canvas = bDisp.getCanvas();
    // Верхняя труба
    canvas->fillRect(x, 0, 40, y, PIPE_DARK);
    canvas->fillRect(x + 5, 0, 30, y, PIPE_BODY);
    canvas->fillRect(x - 2, y - 10, 44, 10, PIPE_BODY);

    // Нижняя труба
    canvas->fillRect(x, y + gap, 40, 215 - (y + gap), PIPE_DARK);
    canvas->fillRect(x + 5, y + gap, 30, 215 - (y + gap), PIPE_BODY);
    canvas->fillRect(x - 2, y + gap, 44, 10, PIPE_BODY);
}

void initFlappy() {
    prefFlappy.begin("flappy", true);
    flappy_highScore = prefFlappy.getInt("hscore", 0);
    prefFlappy.end();

    b_Y = 100;
    b_V = 0;
    p_X = 320;
    g_Y = random(30, 110);
    score = 0;
    isGameOver = false;
}

void tickFlappy() {
    auto* canvas = bDisp.getCanvas();

    // 1. ПРОВЕРКА ТАЧА (Мгновенная)
    if (ts.touched()) {
        int tx, ty;
        getPoint(tx, ty);

        if (tx > 270 && ty < 50) { // Нажали на EXIT
            showMenu(true);
            return;
        }

        if (isGameOver) { // Если проиграли — рестарт по тачу
            initFlappy();
            return;
        }
        b_V = -3.5; // Прыжок
    }

    // 2. ЛОГИКА
    if (!isGameOver) {
        b_V += 0.25;
        b_Y += b_V;
        p_X -= 5;

        if (p_X < -44) {
            p_X = 320;
            g_Y = random(30, 110);
            score++;
        }

        // Проверка столкновений
        bool hitPipe = (68 > p_X && 48 < p_X + 40);
        bool hitGap = (b_Y < g_Y || b_Y + 12 > g_Y + 90);
        if (b_Y > 203 || b_Y < 0 || (hitPipe && hitGap)) {
            isGameOver = true;
            if (score > flappy_highScore) {
                prefFlappy.begin("flappy", false);
                prefFlappy.putInt("hscore", score);
                prefFlappy.end();
                flappy_highScore = score;
            }
        }
    }

    // 3. ОТРИСОВКА (Всё в буфер)
    canvas->fillScreen(BG_COLOR); // Небо
    drawStaticDecor();            // Облака и трава
    drawFlappyPipe(p_X, g_Y, 90); // Трубы
    drawFlappyBird((int)b_Y);     // Птица
    drawFlappyExitBtn();          // Кнопка выхода

    // Интерфейс
    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(2);
    canvas->setCursor(150, 5);
    canvas->print(score);

    canvas->setTextSize(1);
    canvas->setCursor(5, 5);
    canvas->print("BEST: ");
    canvas->print(flappy_highScore);

    if (isGameOver) {
        canvas->setCursor(70, 100);
        canvas->setTextColor(0xF800);
        canvas->setTextSize(3);
        canvas->print("GAME OVER");
    }
}