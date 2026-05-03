#include "Global.h"
#include "Calc.h"
#include "Display.h"
#include <XPT2046_Touchscreen.h>

extern XPT2046_Touchscreen ts;
extern void getPoint(int &x, int &y);
extern void showMenu(bool fullRedraw);

String currentInput = "";
double firstNum = 0;
char op = ' ';
bool clearNext = false;

// Рисуем одну кнопку калькулятора
void drawCalcBtn(int x, int y, int w, int h, const char* label, uint16_t color = 0x4208) {
    auto* canvas = bDisp.getCanvas();
    canvas->fillRoundRect(x, y, w, h, 4, color);
    canvas->drawRoundRect(x, y, w, h, 4, 0x5AEB);
    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(2);
    canvas->setCursor(x + (w/2) - 8, y + (h/2) - 8);
    canvas->print(label);
}

void initCalc() {
    currentInput = "0";
    firstNum = 0;
    op = ' ';

    auto* canvas = bDisp.getCanvas();
    canvas->fillScreen(0x2104); // TWRP BG

    // Шапка
    canvas->fillRect(0, 0, 320, 35, 0x34BF); // TWRP BLUE
    canvas->setCursor(10, 8);
    canvas->setTextSize(2);
    canvas->print("Calculator");

    // Кнопка выхода (назад)
    canvas->fillRoundRect(260, 5, 55, 25, 4, 0xB000); // Красная
    canvas->setTextSize(1);
    canvas->setCursor(270, 13);
    canvas->print("EXIT");
}

void tickCalc() {
    auto* canvas = bDisp.getCanvas();

    // 1. Рисуем дисплей калькулятора
    canvas->fillRect(10, 45, 300, 40, 0x0000);
    canvas->drawRect(10, 45, 300, 40, 0x5AEB);
    canvas->setCursor(20, 55);
    canvas->setTextSize(2);
    canvas->setTextColor(0x07E0); // Зеленый шрифт как на старых калькуляторах
    canvas->print(currentInput);

    // 2. Рисуем кнопки (4 ряда по 4 кнопки)
    const char* keys[4][4] = {
        {"7", "8", "9", "/"},
        {"4", "5", "6", "*"},
        {"1", "2", "3", "-"},
        {"C", "0", "=", "+"}
    };

    for(int r=0; r<4; r++) {
        for(int c=0; c<4; c++) {
            drawCalcBtn(10 + c*77, 95 + r*35, 72, 30, keys[r][c], (c == 3) ? 0x34BF : 0x4208);
        }
    }

    // 3. Обработка касаний
    if (ts.touched()) {
        int tx, ty;
        getPoint(tx, ty);

        // Кнопка выхода
        if (ty < 40 && tx > 250) {
            showMenu(true);
            return;
        }

        // Простая логика нажатий на сетку
        if (ty > 90) {
            int col = (tx - 10) / 77;
            int row = (ty - 95) / 35;

            if (col >= 0 && col < 4 && row >= 0 && row < 4) {
                const char* key = keys[row][col];

                if (key[0] >= '0' && key[0] <= '9') {
                    if (currentInput == "0" || clearNext) currentInput = "";
                    currentInput += key;
                    clearNext = false;
                }
                else if (key[0] == 'C') {
                    currentInput = "0";
                    firstNum = 0;
                    op = ' ';
                }
                else if (key[0] == '=') {
                    double secondNum = currentInput.toDouble();
                    if (op == '+') firstNum += secondNum;
                    else if (op == '-') firstNum -= secondNum;
                    else if (op == '*') firstNum *= secondNum;
                    else if (op == '/') if(secondNum != 0) firstNum /= secondNum;
                    currentInput = String(firstNum);
                    clearNext = true;
                }
                else { // Операции +, -, *, /
                    firstNum = currentInput.toDouble();
                    op = key[0];
                    clearNext = true;
                }
                delay(200); // Анти-дребезг тача
            }
        }
    }
    bDisp.update();
}
