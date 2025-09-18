/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________ */

#include <EEPROM.h>
#include <U8g2lib.h>
#include <Arduino.h>

#include "../include/setting.h"
#include "../include/sleep_manager.h"
#include "../include/level_system.h"
#include "../include/legal_disclaimer.h"
#include "../include/pindefs.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define EEPROM_ADDRESS_NEOPIXEL 0
#define EEPROM_ADDRESS_BRIGHTNESS 1
#define EEPROM_ADDRESS_DANGEROUS_MODE 2

int currentSetting = 0;
int totalSettings = 4;
bool neoPixelActive = false;
uint8_t oledBrightness = 100;
extern bool dangerousActionsEnabled;
bool showResetConfirm = false;

void handleDangerousActions() {
  if (!dangerousActionsEnabled) {
    if (showLegalDisclaimer()) {
      dangerousActionsEnabled = true;
      EEPROM.write(EEPROM_ADDRESS_DANGEROUS_MODE, 1);
      EEPROM.commit();
    }
  } else {
    dangerousActionsEnabled = false;
    EEPROM.write(EEPROM_ADDRESS_DANGEROUS_MODE, 0);
    EEPROM.commit();
  }
}


void settingSetup() {
  uint8_t neoPixelValue = EEPROM.read(EEPROM_ADDRESS_NEOPIXEL);
  uint8_t brightnessValue = EEPROM.read(EEPROM_ADDRESS_BRIGHTNESS);

  neoPixelActive = (neoPixelValue == 1);

  if (brightnessValue > 255) {
    oledBrightness = 128;
  } else {
    oledBrightness = brightnessValue;
  }

  u8g2.setContrast(oledBrightness);

  currentSetting = 0;
  showResetConfirm = false;
}

void settingLoop() {
  static bool upPressed = false;
  static bool downPressed = false;
  static bool rightPressed = false;
  static bool leftPressed = false;

  checkIdle();

  bool up = !digitalRead(BUTTON_PIN_UP);
  bool down = !digitalRead(BUTTON_PIN_DOWN);
  bool right = !digitalRead(BUTTON_PIN_RIGHT);
  bool left = !digitalRead(BUTTON_PIN_LEFT);

  if (up || down || right || left) {
    updateLastActivity();
  }

  if (up) {
    if (!upPressed) {
      upPressed = true;
      if (!showResetConfirm) {
        currentSetting = (currentSetting - 1 + totalSettings) % totalSettings;
      }
    }
  } else {
    upPressed = false;
  }

  if (down) {
    if (!downPressed) {
      downPressed = true;
      if (!showResetConfirm) {
        currentSetting = (currentSetting + 1) % totalSettings;
      }
    }
  } else {
    downPressed = false;
  }

  if (right) {
    if (!rightPressed) {
      rightPressed = true;

      if (showResetConfirm) {
        resetXPData();
        showResetConfirm = false;
      } else {
        switch (currentSetting) {
          case 0:
            neoPixelActive = !neoPixelActive;
            EEPROM.write(EEPROM_ADDRESS_NEOPIXEL, neoPixelActive ? 1 : 0);
            EEPROM.commit();
            break;

          case 1:
            {
              uint8_t percent = map(oledBrightness, 0, 255, 0, 100);
              percent += 10;
              if (percent > 100) percent = 0;
              oledBrightness = map(percent, 0, 100, 0, 255);
              u8g2.setContrast(oledBrightness);
              EEPROM.write(EEPROM_ADDRESS_BRIGHTNESS, oledBrightness);
              EEPROM.commit();
            }
            break;

          case 2:
            handleDangerousActions();
            break;

          case 3:
            showResetConfirm = true;
            break;
        }
      }
    }
  } else {
    rightPressed = false;
  }

  if (left) {
    if (!leftPressed) {
      leftPressed = true;
      if (showResetConfirm) {
        showResetConfirm = false;
      }
    }
  } else {
    leftPressed = false;
  }

  u8g2.clearBuffer();

  if (showResetConfirm) {
    u8g2.setFont(u8g2_font_helvB08_tr);
    int titleWidth = u8g2.getUTF8Width("Reset XP Data?");
    u8g2.drawStr((128 - titleWidth) / 2, 20, "Reset XP Data?");

    u8g2.setFont(u8g2_font_6x10_tr);
    int messageWidth = u8g2.getUTF8Width("Reset to Level 1");
    u8g2.drawStr((128 - messageWidth) / 2, 35, "Reset to Level 1");

    u8g2.setFont(u8g2_font_4x6_tr);
    int buttonWidth = u8g2.getUTF8Width("LEFT=Cancel  RIGHT=Confirm");
    u8g2.drawStr((128 - buttonWidth) / 2, 55, "LEFT=Cancel  RIGHT=Confirm");
  } else {
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.drawStr(45, 12, "Settings");

    u8g2.setFont(u8g2_font_6x10_tr);

    if (currentSetting == 0) u8g2.drawStr(2, 25, ">");
    u8g2.drawStr(10, 25, "NeoPixel:");
    u8g2.drawStr(85, 25, neoPixelActive ? "On" : "Off");

    if (currentSetting == 1) u8g2.drawStr(2, 37, ">");
    u8g2.drawStr(10, 37, "Brightness:");
    char brightStr[8];
    sprintf(brightStr, "%d%%", (int)map(oledBrightness, 0, 255, 0, 100));
    u8g2.drawStr(85, 37, brightStr);

    if (currentSetting == 2) u8g2.drawStr(2, 49, ">");
    u8g2.drawStr(10, 49, "Dangerous:");
    u8g2.drawStr(85, 49, dangerousActionsEnabled ? "On" : "Off");

    if (currentSetting == 3) u8g2.drawStr(2, 61, ">");
    u8g2.drawStr(10, 61, "Reset XP:");
    char lvlStr[8];
    sprintf(lvlStr, "Lv%d", getCurrentLevel());
    u8g2.drawStr(85, 61, lvlStr);
  }

  u8g2.sendBuffer();
}

bool isDangerousActionsEnabled() {
  return dangerousActionsEnabled;
}

