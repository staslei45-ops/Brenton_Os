#ifndef GLOBAL_H
#define GLOBAL_H
#include <Arduino.h>
#include <XPT2046_Touchscreen.h>

enum State {
    LAUNCHER, // Заменили MENU на LAUNCHER, чтобы всё совпадало с главным файлом!
    GAME_CUBE,
    GAME_FLAPPY,
    GAME_SNAKE,
    SYSTEM_ABOUT,
    APP_NOTEPAD,
    GAME_SPACE,
    APP_CALC,
    GAME_COOLSPOT,
    GAME_BLOBS,
    GAME_BLOBWARS
};

extern XPT2046_Touchscreen ts;
extern void getPoint(int &x, int &y);
extern void showMenu(bool fullRedraw);
extern unsigned long lastTouch;
extern State currentState;

#endif
