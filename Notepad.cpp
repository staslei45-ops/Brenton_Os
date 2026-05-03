#include "Global.h"
#include "Notepad.h"
#include "Display.h" 
#include <Preferences.h>
#include <XPT2046_Touchscreen.h>

extern XPT2046_Touchscreen ts;
extern void getPoint(int &x, int &y);
extern void showMenu(bool fullRedraw);
extern unsigned long lastTouch;

static Preferences prefs;
static String note = "";
static bool isLoaded = false;

static const char keys[3][11] = {
    "QWERTYUIOP",
    "ASDFGHJKL<", 
    "ZXCVBNM _#"  
};

void initNotepad() {
    note = ""; 
    isLoaded = false;
    // Сразу читаем из памяти при входе, чтобы не было черного экрана
    prefs.begin("notepad", false);
    note = prefs.getString("note", "");
    prefs.end();
    isLoaded = true;
}

void tickNotepad() {
    auto* canvas = bDisp.getCanvas();

    // 1. ПОЛНАЯ ОТРИСОВКА (Каждый кадр)
    canvas->fillScreen(0x0000); // Черный фон
    
    // Шапка (Синяя полоса)
    canvas->fillRect(0, 0, 320, 30, 0x34BF); 
    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(2); // Сделаем покрупнее
    canvas->setCursor(10, 7);
    canvas->print("NOTES");

    // Кнопка EXIT (Красная)
    canvas->fillRect(270, 2, 48, 26, 0xB000); 
    canvas->drawRect(270, 2, 48, 26, 0xFFFF);
    canvas->setCursor(282, 7);
    canvas->print("X");

    // Кнопка SAVE (Зеленая)
    canvas->fillRect(215, 2, 50, 26, 0x0500);
    canvas->setCursor(220, 7);
    canvas->setTextSize(1);
    canvas->print("SAVE");

    // Поле текста
    canvas->fillRect(5, 35, 310, 100, 0x1084);
    canvas->drawRect(5, 35, 310, 100, 0x4208); // Рамка поля
    canvas->setCursor(12, 45);
    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(1);
    canvas->print(note);

    // Клавиатура
    canvas->setTextSize(2); // Крупные буквы на кнопках
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 10; c++) {
            int x = 5 + c * 31;
            int y = 145 + r * 32;
            canvas->fillRoundRect(x, y, 28, 28, 4, 0x2104); // Кнопка
            canvas->drawRoundRect(x, y, 28, 28, 4, 0x4208); // Контур
            canvas->setCursor(x + 8, y + 7);
            canvas->print(keys[r][c]);
        }
    }

    // 2. ЛОГИКА ТАЧА
    if (ts.touched()) {
        int x, y;
        getPoint(x, y);

        if (millis() - lastTouch > 250) {
            lastTouch = millis();

            // EXIT (Верхний правый угол)
            if (y < 35 && x > 270) {
                showMenu(true);
                return;
            }

            // SAVE
            if (y < 35 && x > 210 && x < 265) {
                prefs.begin("notepad", false);
                prefs.putString("note", note);
                prefs.end();
            }

            // КЛАВИАТУРА
            if (y > 140) {
                int r = (y - 145) / 32;
                int c = (x - 5) / 31;
                if (r >= 0 && r < 3 && c >= 0 && c < 10) {
                    char p = keys[r][c];
                    if (p == '<') {
                        if (note.length() > 0) note.remove(note.length() - 1);
                    } else if (p == '_') {
                        note += " ";
                    } else if (p == '#') {
                        note += "\n";
                    } else {
                        if (note.length() < 100) note += p;
                    }
                }
            }
        }
    }

    // КРИТИЧЕСКИ ВАЖНО: Если в Main.ino нет bDisp.update(), 
    // то добавляем его сюда принудительно:
    bDisp.update(); 
}