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

// ---------------- Состояние блокнота ----------------

#define NOTE_COUNT 5
#define NOTE_MAXLEN 180

enum NotepadScreen { NP_LIST, NP_EDIT };

static NotepadScreen npScreen = NP_LIST;
static String notes[NOTE_COUNT];
static int currentNote = -1;   // какую заметку редактируем
static int lastOpenedNote = -1; // какую заметку открывали последний раз (для подсветки)
static bool needRedraw = true; // перерисовываем только когда что-то изменилось

static const char keys[3][11] = {
    "QWERTYUIOP",
    "ASDFGHJKL<",
    "ZXCVBNM _#"
};

// ---------------- Память ----------------

static void loadAllNotes() {
    prefs.begin("notepad", false);
    for (int i = 0; i < NOTE_COUNT; i++) {
        notes[i] = prefs.getString(("note" + String(i)).c_str(), "");
    }
    lastOpenedNote = prefs.getInt("last", -1);
    prefs.end();
}

static void saveNote(int idx) {
    if (idx < 0 || idx >= NOTE_COUNT) return;
    prefs.begin("notepad", false);
    prefs.putString(("note" + String(idx)).c_str(), notes[idx]);
    prefs.end();
}

static void saveLastOpened(int idx) {
    prefs.begin("notepad", false);
    prefs.putInt("last", idx);
    prefs.end();
}

// ---------------- Инициализация ----------------

void initNotepad() {
    loadAllNotes();
    npScreen = NP_LIST;
    currentNote = -1;
    needRedraw = true;
}

// ---------------- Экран списка заметок ----------------

static void drawListScreen() {
    auto* canvas = bDisp.getCanvas();
    canvas->fillScreen(0x0000);

    // Шапка
    canvas->fillRect(0, 0, 320, 30, 0x34BF);
    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(2);
    canvas->setCursor(10, 7);
    canvas->print("NOTES");

    // EXIT (в главное меню)
    canvas->fillRect(265, 2, 53, 26, 0xB000);
    canvas->drawRect(265, 2, 53, 26, 0xFFFF);
    canvas->setTextSize(1);
    canvas->setCursor(282, 10);
    canvas->print("EXIT");

    // Список заметок
    for (int i = 0; i < NOTE_COUNT; i++) {
        int y = 36 + i * 40;
        bool isLast = (i == lastOpenedNote);

        // Фон строки - подсвечиваем последнюю открытую
        uint16_t bg = isLast ? 0x2945 : 0x2104;
        canvas->fillRoundRect(5, y, 310, 36, 5, bg);
        canvas->drawRoundRect(5, y, 310, 36, 5, 0x4208);

        // Заголовок заметки
        canvas->setTextColor(0xFFFF);
        canvas->setTextSize(1);
        canvas->setCursor(12, y + 4);
        canvas->printf("Note %d%s", i + 1, isLast ? " (last)" : "");

        // Превью текста (первая строка, обрезанная)
        String preview = notes[i];
        int nl = preview.indexOf('\n');
        if (nl >= 0) preview = preview.substring(0, nl);
        if (preview.length() == 0) preview = "[empty]";
        if (preview.length() > 42) preview = preview.substring(0, 42) + "...";

        canvas->setTextColor(0xAD55);
        canvas->setCursor(12, y + 18);
        canvas->print(preview);

        // Кнопка очистки заметки
        canvas->fillRect(265, y + 4, 40, 28, 0x6000);
        canvas->drawRect(265, y + 4, 40, 28, 0xFFFF);
        canvas->setTextColor(0xFFFF);
        canvas->setCursor(275, y + 12);
        canvas->print("DEL");
    }

    bDisp.update();
}

static void touchListScreen() {
    if (!ts.touched()) return;
    if (millis() - lastTouch < 250) return;

    int x, y;
    getPoint(x, y);
    lastTouch = millis();

    // EXIT
    if (y < 30 && x > 265) {
        showMenu(true);
        return;
    }

    for (int i = 0; i < NOTE_COUNT; i++) {
        int ry = 36 + i * 40;
        if (y < ry || y > ry + 36) continue;

        // Кнопка DEL для этой заметки
        if (x > 265 && x < 305) {
            notes[i] = "";
            saveNote(i);
            if (lastOpenedNote == i) {
                lastOpenedNote = -1;
                saveLastOpened(-1);
            }
            needRedraw = true;
            return;
        }

        // Открыть заметку на редактирование
        if (x >= 5 && x <= 265) {
            currentNote = i;
            lastOpenedNote = i;
            saveLastOpened(i);
            npScreen = NP_EDIT;
            needRedraw = true;
            return;
        }
    }
}

// ---------------- Экран редактирования ----------------

static void drawEditScreen() {
    auto* canvas = bDisp.getCanvas();
    canvas->fillScreen(0x0000);

    // Шапка
    canvas->fillRect(0, 0, 320, 30, 0x34BF);
    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(2);
    canvas->setCursor(10, 7);
    canvas->printf("NOTE %d", currentNote + 1);

    // BACK -> в список (с автосохранением)
    canvas->fillRect(160, 2, 50, 26, 0x4208);
    canvas->drawRect(160, 2, 50, 26, 0xFFFF);
    canvas->setTextSize(1);
    canvas->setCursor(170, 10);
    canvas->print("BACK");

    // SAVE
    canvas->fillRect(215, 2, 50, 26, 0x0500);
    canvas->drawRect(215, 2, 50, 26, 0xFFFF);
    canvas->setCursor(225, 10);
    canvas->print("SAVE");

    // CLEAR
    canvas->fillRect(270, 2, 48, 26, 0xB000);
    canvas->drawRect(270, 2, 48, 26, 0xFFFF);
    canvas->setCursor(278, 10);
    canvas->print("CLR");

    // Поле текста
    canvas->fillRect(5, 35, 310, 100, 0x1084);
    canvas->drawRect(5, 35, 310, 100, 0x4208);
    canvas->setCursor(12, 45);
    canvas->setTextColor(0xFFFF);
    canvas->setTextSize(1);
    canvas->print(notes[currentNote]);

    // Курсор (мигающий, по длине текста - очень грубо, без учёта переноса строк)
    if ((millis() / 500) % 2 == 0) {
        int len = notes[currentNote].length();
        int cx = 12 + (len % 50) * 6;
        int cy = 45 + (len / 50) * 8;
        canvas->fillRect(cx, cy, 6, 8, 0x07E0);
    }

    // Клавиатура
    canvas->setTextSize(2);
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 10; c++) {
            int x = 5 + c * 31;
            int y = 145 + r * 32;
            canvas->fillRoundRect(x, y, 28, 28, 4, 0x2104);
            canvas->drawRoundRect(x, y, 28, 28, 4, 0x4208);
            canvas->setCursor(x + 8, y + 7);
            canvas->print(keys[r][c]);
        }
    }

    bDisp.update();
}

static void touchEditScreen() {
    if (!ts.touched()) return;
    if (millis() - lastTouch < 250) return;

    int x, y;
    getPoint(x, y);
    lastTouch = millis();

    // BACK -> сохраняем и идём в список
    if (y < 30 && x > 160 && x < 210) {
        saveNote(currentNote);
        npScreen = NP_LIST;
        needRedraw = true;
        return;
    }

    // SAVE
    if (y < 30 && x > 210 && x < 265) {
        saveNote(currentNote);
        needRedraw = true;
        return;
    }

    // CLEAR
    if (y < 30 && x > 265) {
        notes[currentNote] = "";
        needRedraw = true;
        return;
    }

    // Клавиатура
    if (y > 140) {
        int r = (y - 145) / 32;
        int c = (x - 5) / 31;
        if (r >= 0 && r < 3 && c >= 0 && c < 10) {
            char p = keys[r][c];
            String &note = notes[currentNote];
            if (p == '<') {
                if (note.length() > 0) note.remove(note.length() - 1);
            } else if (p == '_') {
                note += " ";
            } else if (p == '#') {
                note += "\n";
            } else {
                if (note.length() < NOTE_MAXLEN) note += p;
            }
            needRedraw = true;
        }
    }
}

// ---------------- Главный цикл блокнота ----------------

void tickNotepad() {
    if (npScreen == NP_LIST) {
        if (needRedraw) {
            drawListScreen();
            needRedraw = false;
        }
        touchListScreen();
    } else {
        // Курсор мигает - перерисовываем экран регулярно
        static unsigned long lastBlink = 0;
        if (needRedraw || millis() - lastBlink > 500) {
            drawEditScreen();
            needRedraw = false;
            lastBlink = millis();
        }
        touchEditScreen();
    }
}
