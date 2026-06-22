#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "Global.h"
#include "Display.h"
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// Игры
#include "Cube.h"
#include "Flappy.h"
#include "Snake.h"
#include "About.h"
#include "Notepad.h"
#include "SpaceInvaders.h"
#include "Calc.h"
#include <CoolSpotGame.h>
#include "MutantBlob.h"
#include "BlobWars.h"
// #include "ConstantC.h"  // временно отключено

// Лаунчер
#include "Launcher.h"

#define TOUCH_CS 5
#define SCK_PIN  TFT_SCLK
#define MISO_PIN TFT_MISO
#define MOSI_PIN TFT_MOSI

XPT2046_Touchscreen ts(TOUCH_CS);
CoolSpotGame mySpot;

State currentState = LAUNCHER;
unsigned long lastTouch = 0;
unsigned long exitTimer = 0;

void getPoint(int &x, int &y) {
  TS_Point p = ts.getPoint();
  x = map(p.x, 200, 3800, 320, 0);
  y = map(p.y, 200, 3800, 240, 0);
}

void showMenu(bool fullRedraw) {
  currentState = LAUNCHER;
  exitTimer = millis();
  if (fullRedraw) initLauncher();
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  delay(200);
  Serial.println("=== BOOT ===");

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
  bDisp.init();
  ts.begin();
  ts.setRotation(1);

  initLauncher();
}

void loop() {
  if (currentState == LAUNCHER) {
    tickLauncher();
  } else {
    switch (currentState) {
      case GAME_CUBE:      tickCube(); break;
      case GAME_FLAPPY:    tickFlappy(); break;
      case GAME_SNAKE:     tickSnake(); break;
      case SYSTEM_ABOUT:   tickAbout(); break;
      case APP_NOTEPAD:    tickNotepad(); break;
      case GAME_SPACE:     tickSpaceInvaders(); break;
      case APP_CALC:       tickCalc(); break;
      case GAME_COOLSPOT:  mySpot.update(); mySpot.render(); if (!mySpot.isRunning) showMenu(true); break;
      case GAME_BLOBS:     tickMutantBlob(); break;
      case GAME_BLOBWARS:  tickBlobWars(); break;
      // case GAME_CONSTANTC: tickConstantC(); break;
      default: break;
    }

    if (currentState != LAUNCHER &&
        currentState != SYSTEM_ABOUT &&
        currentState != APP_NOTEPAD &&
        currentState != APP_CALC &&
        currentState != GAME_BLOBWARS) {
      bDisp.update();
    }
  }
}