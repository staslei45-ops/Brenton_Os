#ifndef GLOBAL_H
#define GLOBAL_H

#include <Arduino.h>

enum State {
    MENU,
    GAME_CUBE,
    GAME_FLAPPY,
    GAME_SNAKE,
    SYSTEM_ABOUT,
    APP_NOTEPAD,
    GAME_SPACE,
    APP_CALC,
    GAME_COOLSPOT
};

extern State currentState;

// Простое и понятное объявление
void showMenu(bool fullRedraw);

#endif