/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________ */

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <stdint.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
#include <RF24.h>

#include "../include/icon.h"
#include "../include/neopixel.h"
#include "../include/setting.h"

#include "../include/scanner.h"
#include "../include/analyzer.h"
#include "../include/jammer.h" 
#include "../include/blejammer.h"
#include "../include/spoofer.h"
#include "../include/sourapple.h"
#include "../include/blescan.h"
#include "../include/ble_spammer.h"
#include "../include/flipper.h"
#include "../include/wifiscan.h"
#include "../include/deauth.h"
#include "../include/deauth_scanner.h"
#include "../include/beacon_spam.h"
#include "../include/pindefs.h"
#include "../include/blackout.h"
#include "../include/about.h"

RF24 radios[] = {
  RF24(RADIO_CE_PIN_1, RADIO_CSN_PIN_1),
  RF24(RADIO_CE_PIN_2, RADIO_CSN_PIN_2),
  RF24(RADIO_CE_PIN_3, RADIO_CSN_PIN_3)
};

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Adafruit_NeoPixel pixels(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
extern uint8_t oledBrightness;

const char* nyanboxVersion = NYANBOX_VERSION;
const unsigned long idleTimeout = 120000;
static unsigned long lastActivity = 0;
static bool displayOff = false;

void updateLastActivity() {
  lastActivity = millis();
}

bool anyButtonPressed() {
  return digitalRead(BUTTON_PIN_UP)    == LOW ||
        digitalRead(BUTTON_PIN_DOWN)  == LOW ||
        digitalRead(BUTTON_PIN_CENTER)== LOW;
}

void wakeDisplay() {
  u8g2.setPowerSave(0);
  displayOff = false;
  while (anyButtonPressed()) {}
  updateLastActivity();
}

void checkIdle() {
  if (!displayOff && millis() - lastActivity >= idleTimeout) {
    u8g2.setPowerSave(1);
    displayOff = true;
    return;
  }
  if (displayOff && anyButtonPressed()) {
    wakeDisplay();
  }
}

unsigned long upLastMillis    = 0;
unsigned long upNextRepeat    = 0;
bool         upPressed        = false;

unsigned long downLastMillis  = 0;
unsigned long downNextRepeat  = 0;
bool         downPressed      = false;

const unsigned long initialDelay   = 500;
const unsigned long repeatInterval = 250;

struct MenuItem {
  const char* name;
  const unsigned char* icon;
  void (*setup)();
  void (*loop)();
};

enum AppMenuState { APP_MAIN, APP_BLE, APP_WIFI, APP_OTHER };
AppMenuState currentState = APP_MAIN;
MenuItem*    currentMenuItems = nullptr;
int          currentMenuSize  = 0;
int          item_selected    = 0;

constexpr uint8_t BUTTON_UP    = BUTTON_PIN_UP;
constexpr uint8_t BUTTON_SEL   = BUTTON_PIN_CENTER;
constexpr uint8_t BUTTON_DOWN  = BUTTON_PIN_DOWN;
bool selPrev = false;

bool justPressed(uint8_t pin, bool &prev) {
  bool now = digitalRead(pin) == LOW;
  if (now && !prev) { prev = true; return true; }
  if (!now) prev = false;
  return false;
}

void enterMenu(AppMenuState st);
void runApp(MenuItem &mi);

MenuItem mainMenu[] = {
  { "WiFi",  bitmap_icon_wifi,    nullptr, nullptr },
  { "BLE",   bitmap_icon_ble,     nullptr, nullptr },
  { "Other", bitmap_icon_scanner, nullptr, nullptr }
};
constexpr int MAIN_MENU_SIZE = sizeof(mainMenu) / sizeof(mainMenu[0]);

MenuItem wifiMenu[] = {
  { "WiFi Scan",       nullptr, wifiscanSetup,       wifiscanLoop       },
  { "WiFi Deauther",   nullptr, deauthSetup,         deauthLoop         },
  { "Deauth Scanner",  nullptr, deauthScannerSetup,  deauthScannerLoop },
  { "Beacon Spam",     nullptr, beaconSpamSetup,     beaconSpamLoop     },
  { "WLAN Jammer",     nullptr, jammerSetup,         jammerLoop         },
  { "Back",            nullptr, nullptr,             nullptr            }
};
constexpr int WIFI_MENU_SIZE = sizeof(wifiMenu) / sizeof(wifiMenu[0]);

MenuItem bleMenu[] = {
  { "BLE Scan",     nullptr, blescanSetup,      nullptr          },
  { "Flipper Scan", nullptr, flipperSetup,      nullptr          },
  { "BLE Spammer",  nullptr, bleSpamSetup,      bleSpamLoop      },
  { "BLE Jammer",   nullptr, blejammerSetup,    blejammerLoop    },
  { "Sour Apple",   nullptr, sourappleSetup,    sourappleLoop    },
  { "BLE Spoofer",  nullptr, spooferSetup,      spooferLoop      },
  { "Back",         nullptr, nullptr,           nullptr          }
};
constexpr int BLE_MENU_SIZE = sizeof(bleMenu) / sizeof(bleMenu[0]);

MenuItem otherMenu[] = {
  { "Proto Kill",   nullptr, blackoutSetup,   blackoutLoop   },
  { "Scanner",      nullptr, scannerSetup,    scannerLoop    },
  { "Analyzer",     nullptr, analyzerSetup,   analyzerLoop   },
  { "Setting",      nullptr, settingSetup,    settingLoop    },
  { "About",        nullptr, aboutSetup,      aboutLoop      },
  { "Back",         nullptr, nullptr,         nullptr        }
};
constexpr int OTHER_MENU_SIZE = sizeof(otherMenu) / sizeof(otherMenu[0]);

void enterMenu(AppMenuState st) {
  currentState = st;
  item_selected = 0;
  switch (st) {
    case APP_MAIN:
      currentMenuItems = mainMenu;
      currentMenuSize  = MAIN_MENU_SIZE;
      break;
    case APP_WIFI:
      currentMenuItems = wifiMenu;
      currentMenuSize  = WIFI_MENU_SIZE;
      break;
    case APP_BLE:
      currentMenuItems = bleMenu;
      currentMenuSize  = BLE_MENU_SIZE;
      break;
    case APP_OTHER:
      currentMenuItems = otherMenu;
      currentMenuSize  = OTHER_MENU_SIZE;
      break;
  }
}

void runApp(MenuItem &mi) {
  if (!mi.setup) return;
  mi.setup();
  updateLastActivity();
  displayOff = false;
  u8g2.setPowerSave(0);

  if (!mi.loop) return;
  while (digitalRead(BUTTON_SEL) == LOW);

  while (true) {
    checkIdle();
    mi.loop();
    if (digitalRead(BUTTON_SEL) == LOW) {
      updateLastActivity();
      while (digitalRead(BUTTON_SEL) == LOW);
      if (mi.setup == blackoutSetup || mi.setup == jammerSetup || mi.setup == blejammerSetup) {
        for (auto &r : radios) r.powerDown();
        esp_wifi_start();
      }
      break;
    }
  }

  u8g2.clearBuffer();
}

void setup() {
  neopixelSetup();
  for (auto &r : radios) {
    r.begin();
    r.setAutoAck(false);
    r.stopListening();
    r.setRetries(0,0);
    r.setPALevel(RF24_PA_MAX, true);
    r.setDataRate(RF24_2MBPS);
    r.setCRCLength(RF24_CRC_DISABLED);
  }

  EEPROM.begin(512);
  oledBrightness = EEPROM.read(1);

  u8g2.begin();
  u8g2.setContrast(oledBrightness);
  u8g2.setBitmapMode(1);

  updateLastActivity();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB14_tr);
  const char* title = "nyan-BOX";
  int16_t titleW = u8g2.getUTF8Width(title);
  u8g2.setCursor((128 - titleW) / 2, 16);
  u8g2.print(title);

  u8g2.setFont(u8g2_font_6x10_tf);
  const char* url = "nyanBOX.lullaby.cafe";
  int16_t urlW = u8g2.getUTF8Width(url);
  u8g2.setCursor((128 - urlW) / 2, 32);
  u8g2.print(url);

  u8g2.setFont(u8g2_font_ncenB08_tr);
  int16_t creditWidth = u8g2.getUTF8Width("jbohack & zr_crackiin");
  int16_t creditX = (128 - creditWidth) / 2;
  u8g2.setCursor(creditX, 50);
  u8g2.print("jbohack & zr_crackiin");

  u8g2.setFont(u8g2_font_6x10_tf);
  int16_t verW = u8g2.getUTF8Width(nyanboxVersion);
  u8g2.setCursor((128 - verW) / 2, 62);
  u8g2.print(nyanboxVersion);

  u8g2.sendBuffer();
  delay(1500);

  u8g2.clearBuffer();
  u8g2.drawXBMP(0, 0, 128, 64, logo_nyanbox);
  u8g2.sendBuffer();
  delay(1000);

  pinMode(BUTTON_PIN_UP, INPUT_PULLUP);
  pinMode(BUTTON_PIN_CENTER, INPUT_PULLUP);
  pinMode(BUTTON_PIN_DOWN, INPUT_PULLUP);

  enterMenu(APP_MAIN);
}

void loop() {
  checkIdle();

  bool upNow = (digitalRead(BUTTON_PIN_UP) == LOW);
  bool downNow = (digitalRead(BUTTON_PIN_DOWN) == LOW);

  if (upNow) {
    updateLastActivity();
    if (!upPressed) {
      if (item_selected > 0) item_selected--;
      upLastMillis = millis();
      upNextRepeat = upLastMillis + initialDelay;
    } else if (millis() >= upNextRepeat) {
      if (item_selected > 0) item_selected--;
      upNextRepeat += repeatInterval;
    }
  }
  upPressed = upNow;

  if (downNow) {
    updateLastActivity();
    if (!downPressed) {
      if (item_selected < currentMenuSize - 1) item_selected++;
      downLastMillis = millis();
      downNextRepeat = downLastMillis + initialDelay;
    } else if (millis() >= downNextRepeat) {
      if (item_selected < currentMenuSize - 1) item_selected++;
      downNextRepeat += repeatInterval;
    }
  }
  downPressed = downNow;

  if (justPressed(BUTTON_SEL, selPrev)) {
    updateLastActivity();
    MenuItem &sel = currentMenuItems[item_selected];
    if (currentState == APP_MAIN) {
      if (strcmp(sel.name, "WiFi") == 0) enterMenu(APP_WIFI);
      else if (strcmp(sel.name, "BLE") == 0) enterMenu(APP_BLE);
      else if (strcmp(sel.name, "Other") == 0) enterMenu(APP_OTHER);
    } else {
      if (strcmp(sel.name, "Back") == 0) {
        enterMenu(APP_MAIN);
      } else {
        runApp(sel);
      }
    }
  }

  u8g2.clearBuffer();
  int start;
  if (item_selected == 0) start = 0;
  else if (item_selected == currentMenuSize - 1) start = max(0, currentMenuSize - 3);
  else start = item_selected - 1;

  static const int yPos[3]   = {15, 37, 59};
  static const int boxY[3]   = { 0, 22, 44};
  static const uint8_t *fonts[3] = { u8g_font_7x14, u8g_font_7x14B, u8g_font_7x14 };

  int highlight = item_selected - start;
  u8g2.drawXBMP(0, boxY[highlight], 128, 21, bitmap_item_sel_outline);

  for (int i = 0; i < 3; i++) {
    int idx = start + i;
    if (idx < currentMenuSize) {
      u8g2.setFont(fonts[i]);
      u8g2.drawStr(25, yPos[i], currentMenuItems[idx].name);
      if (currentMenuItems[idx].icon) {
        int iconY = (i == 0 ? 2 : i == 1 ? 24 : 46);
        u8g2.drawXBMP(4, iconY, 16, 16, currentMenuItems[idx].icon);
      }
    }
  }

  int boxH = 64 / currentMenuSize;
  u8g2.drawXBMP(120, 0, 8, 64, bitmap_scrollbar_background);
  u8g2.drawBox(125, boxH * item_selected, 3, boxH);

  u8g2.sendBuffer();
}