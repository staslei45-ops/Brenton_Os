#include <Arduino.h>
#include "Display.h" 
#include <XPT2046_Touchscreen.h>
#include <math.h>

// Подключаем внешние объекты и функции из Main.ino
extern XPT2046_Touchscreen ts;
extern void getPoint(int &x, int &y);
extern void showMenu(bool fullRedraw); // ИСПОЛЬЗУЕМ ЭТУ ФУНКЦИЮ

#define EXIT_COLOR 0xF800

float cubePoints[8][3] = {
    {-30,-30, 30}, {30,-30, 30}, {30, 30, 30}, {-30, 30, 30},
    {-30,-30,-30}, {30,-30,-30}, {30, 30,-30}, {-30, 30,-30}
};

int16_t curX[8], curY[8];
float cRotX = 0;
float cRotY = 0;

unsigned long fpsTimer = 0;
int fpsCounter = 0;
int fps = 0;

static inline void drawExitButton() {
    auto* canvas = bDisp.getCanvas();
    canvas->fillRect(110, 205, 100, 30, EXIT_COLOR);
    canvas->drawRect(110, 205, 100, 30, 0xFFFF);
    canvas->setCursor(140, 212);
    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(2);
    canvas->print("EXIT");
}

void initCube() {
    bDisp.getCanvas()->fillScreen(0x0000);
    for (int i = 0; i < 8; i++) {
        curX[i] = 0;
        curY[i] = 0;
    }
    cRotX = 0;
    cRotY = 0;
    fpsTimer = millis();
    fpsCounter = 0;
    fps = 0;
}

static inline void drawLines(int16_t* x, int16_t* y, uint16_t color) {
    auto* canvas = bDisp.getCanvas();
    for (int i = 0; i < 4; i++) {
        int n = (i + 1) & 3;
        canvas->drawLine(x[i], y[i], x[n], y[n], color);
        canvas->drawLine(x[i + 4], y[i + 4], x[n + 4], y[n + 4], color);
        canvas->drawLine(x[i], y[i], x[i + 4], y[i + 4], color);
    }
}

static inline void drawFPS() {
    auto* canvas = bDisp.getCanvas();
    canvas->setCursor(5, 5);
    canvas->setTextColor(0x07E0);
    canvas->setTextSize(1);
    canvas->print("FPS: ");
    canvas->print(fps);
}

void tickCube() {
    auto* canvas = bDisp.getCanvas();
    
    // 1. Проверка тача В САМОМ НАЧАЛЕ
    if (ts.touched()) {
        int tx, ty;
        getPoint(tx, ty);
        // Если нажали на кнопку EXIT
        if (tx > 110 && tx < 210 && ty > 200) {
            showMenu(true); // Меняем состояние на MENU и рисуем его
            return;         // ВАЖНО: Выходим из тика куба немедленно!
        }
    }

    // 2. Очистка кадра
    canvas->fillScreen(0x0000); 

    // 3. Математика
    cRotX += 0.05f;
    cRotY += 0.04f;

    float cx = cosf(cRotX);
    float sx = sinf(cRotX);
    float cy = cosf(cRotY);
    float sy = sinf(cRotY);

    for (int i = 0; i < 8; i++) {
        float x = cubePoints[i][0];
        float y = cubePoints[i][1];
        float z = cubePoints[i][2];

        float ty = y * cx - z * sx;
        float tz = y * sx + z * cx;
        y = ty;
        z = tz;

        float tx = x * cy + z * sy;
        tz = -x * sy + z * cy;
        x = tx;
        z = tz;

        float denom = z + 150.0f;
        if (denom < 20.0f) denom = 20.0f;
        float f = 150.0f / denom;

        curX[i] = (int16_t)(x * f) + 160;
        curY[i] = (int16_t)(y * f) + 110;
    }

    // 4. Отрисовка
    drawLines(curX, curY, 0x34BF);
    drawFPS();
    drawExitButton();

    // 5. FPS
    fpsCounter++;
    unsigned long now = millis();
    if (now - fpsTimer >= 1000) {
        fps = fpsCounter;
        fpsCounter = 0;
        fpsTimer = now;
    }
}