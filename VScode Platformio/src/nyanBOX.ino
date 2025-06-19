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

RF24 radios[] = {
  RF24(RADIO_CE_PIN_1, RADIO_CSN_PIN_1),
  RF24(RADIO_CE_PIN_2, RADIO_CSN_PIN_2),
  RF24(RADIO_CE_PIN_3, RADIO_CSN_PIN_3)
};

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Adafruit_NeoPixel pixels(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
extern uint8_t oledBrightness;

struct MenuItem {
  const char* name;
  const unsigned char* icon;
  void (*setup)();
  void (*loop)();
};

void about() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB14_tr);
  int16_t w = u8g2.getUTF8Width("nyan-BOX");
  u8g2.setCursor((128-w)/2, 20);
  u8g2.print("nyan-BOX");

  u8g2.setFont(u8g2_font_6x10_tf);
  const char* discord = "jbohack & zr_crackiin";
  w = u8g2.getUTF8Width(discord);
  u8g2.setCursor((128-w)/2, 40);
  u8g2.print(discord);
  u8g2.drawStr(7, 60, "defcon.lullaby.cafe");

  u8g2.sendBuffer();
}

MenuItem menu[] = {
  { "Scanner",        bitmap_icon_scanner,       scannerSetup,        scannerLoop       },
  { "Analyzer",       bitmap_icon_analyzer,      analyzerSetup,       analyzerLoop      },
  { "WLAN Jammer",    bitmap_icon_jammer,        jammerSetup,         jammerLoop        },
  { "Proto Kill",     bitmap_icon_kill,          blackoutSetup,       blackoutLoop      },
  { "BLE Jammer",     bitmap_icon_ble_jammer,    blejammerSetup,      blejammerLoop     },
  { "BLE Spoofer",    bitmap_icon_spoofer,       spooferSetup,        spooferLoop       },
  { "Sour Apple",     bitmap_icon_apple,         sourappleSetup,      sourappleLoop     },
  { "BLE Scan",       bitmap_icon_ble,           blescanSetup,        nullptr           },
  { "BLE Spammer",    bitmap_icon_ble,           bleSpamSetup,        bleSpamLoop       },
  { "Flipper Scan",   bitmap_icon_ble,           flipperSetup,        nullptr           },
  { "WiFi Scan",      bitmap_icon_wifi,          wifiscanSetup,       wifiscanLoop      },
  { "WiFi Deauther",  bitmap_icon_wifi,          deauthSetup,         deauthLoop        },
  { "Deauth Scanner", bitmap_icon_wifi,          deauthScannerSetup,  deauthScannerLoop },
  { "Beacon Spam",    bitmap_icon_wifi,          beaconSpamSetup,     beaconSpamLoop    },
  { "About",          bitmap_icon_about,         about,               about             },
  { "Setting",        bitmap_icon_setting,       settingSetup,        settingLoop       }
};

constexpr int NUM_ITEMS = sizeof(menu) / sizeof(menu[0]);
constexpr uint8_t BUTTON_UP    = BUTTON_PIN_UP;
constexpr uint8_t BUTTON_SEL   = BUTTON_PIN_CENTER;
constexpr uint8_t BUTTON_DOWN  = BUTTON_PIN_DOWN;

uint8_t item_selected = 0;
bool upPrev = false, downPrev = false, selPrev = false;

bool justPressed(uint8_t pin, bool &prev) {
  bool now = digitalRead(pin) == LOW;
  if (now && !prev) { prev = true; return true; }
  if (!now) prev = false;
  return false;
}

void runApp(int idx) {
  menu[idx].setup();
  if (menu[idx].loop) {
    while (digitalRead(BUTTON_SEL) == LOW);
    while (true) {
      menu[idx].loop();
      if (digitalRead(BUTTON_SEL) == LOW) {
        while (digitalRead(BUTTON_SEL) == LOW);
        break;
      }
    }
    u8g2.clearBuffer();
  }
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

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_ncenB14_tr); 
  int16_t nameWidth = u8g2.getUTF8Width("nyan-BOX"); 
  int16_t nameX = (128 - nameWidth) / 2;            
  u8g2.setCursor(nameX, 25);                      
  u8g2.print("nyan-BOX");

  u8g2.setFont(u8g2_font_ncenB08_tr); 
  int16_t creditWidth = u8g2.getUTF8Width("by jbohack & zr_crackiin");
  int16_t creditX = (106 - creditWidth) / 2;
  u8g2.setCursor(creditX, 40);
  u8g2.print("by jbohack & zr_crackiin");

  u8g2.setFont(u8g2_font_6x10_tf); 
  int16_t versionWidth = u8g2.getUTF8Width("v2.8.0");
  int16_t versionX = (128 - versionWidth) / 2;
  u8g2.setCursor(versionX, 60);
  u8g2.print("v2.8.0");
  
  u8g2.sendBuffer(); 
  delay(1500);

  u8g2.clearBuffer();

  u8g2.drawXBMP(0, 0, 128, 64, logo_nyanbox);

  u8g2.sendBuffer(); 
  delay(1000);
  
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_SEL, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
}

void loop() {
  if (justPressed(BUTTON_UP, upPrev))
    item_selected = (item_selected + NUM_ITEMS - 1) % NUM_ITEMS;
  if (justPressed(BUTTON_DOWN, downPrev))
    item_selected = (item_selected + 1) % NUM_ITEMS;
  if (justPressed(BUTTON_SEL, selPrev))
    runApp(item_selected);

  u8g2.clearBuffer();
  u8g2.drawXBMP(0,22,128,21, bitmap_item_sel_outline);
  static const int yPos[3] = {15,37,59};
  static const uint8_t *fonts[3] = {u8g_font_7x14, u8g_font_7x14B, u8g_font_7x14};

  for (int i = 0; i < 3; i++) {
    int idx = (item_selected + (i-1) + NUM_ITEMS) % NUM_ITEMS;
    u8g2.setFont(fonts[i]);
    u8g2.drawStr(25, yPos[i], menu[idx].name);
    int iconY = i==0 ? 2 : (i==1 ? 24 : 46);
    u8g2.drawXBMP(4, iconY, 16,16, menu[idx].icon);
  }

  u8g2.drawXBMP(120,0,8,64, bitmap_scrollbar_background);
  u8g2.drawBox(125, (64/NUM_ITEMS)*item_selected, 3, 64/NUM_ITEMS);
  u8g2.sendBuffer();
}