/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#include "../include/beacon_spam.h"
#include "../include/sleep_manager.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// Disable frame sanity checks
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    return 0;
}

const uint8_t channels[] = {1, 6, 11};    // Common channels are 1, 6, 11
const bool wpa2 = false;                 // Open networks = better visibility

const char ssids[] PROGMEM = {
  "Mom Use This One\n"
  "Abraham Linksys\n"
  "Benjamin FrankLAN\n"
  "Martin Router King\n"
  "John Wilkes Bluetooth\n"
  "Pretty Fly for a Wi-Fi\n"
  "Bill Wi the Science Fi\n"
  "I Believe Wi Can Fi\n"
  "Tell My Wi-Fi Love Her\n"
  "No More Mister Wi-Fi\n"
  "Subscribe to TalkingSasquach\n"
  "jbohack was here\n"
  "zr_crackiin was here\n"
  "nyandevices.com\n"
  "LAN Solo\n"
  "The LAN Before Time\n"
  "Silence of the LANs\n"
  "House LANister\n"
  "Winternet Is Coming\n"
  "Ping's Landing\n"
  "The Ping in the North\n"
  "This LAN Is My LAN\n"
  "Get Off My LAN\n"
  "The Promised LAN\n"
  "The LAN Down Under\n"
  "FBI Surveillance Van 4\n"
  "Area 51 Test Site\n"
  "Drive-By Wi-Fi\n"
  "Planet Express\n"
  "Wu Tang LAN\n"
  "Darude LANstorm\n"
  "Never Gonna Give You Up\n"
  "Hide Yo Kids, Hide Yo Wi-Fi\n"
  "Loading…\n"
  "Searching…\n"
  "VIRUS.EXE\n"
  "Virus-Infected Wi-Fi\n"
  "Starbucks Wi-Fi\n"
  "Text ###-#### for Password\n"
  "Yell ____ for Password\n"
  "The Password Is 1234\n"
  "Free Public Wi-Fi\n"
  "No Free Wi-Fi Here\n"
  "Get Your Own Damn Wi-Fi\n"
  "It Hurts When IP\n"
  "Dora the Internet Explorer\n"
  "404 Wi-Fi Unavailable\n"
  "Porque-Fi\n"
  "Titanic Syncing\n"
  "Test Wi-Fi Please Ignore\n"
  "Drop It Like It's Hotspot\n"
  "Life in the Fast LAN\n"
  "The Creep Next Door\n"
  "Ye Olde Internet\n"
  "Lan Before Time\n"
  "Lan Of The Lost\n"
};

char emptySSID[32];
uint8_t macAddr[6];
uint8_t wifi_channel = 1;
uint32_t currentTime = 0;
uint32_t packetSize = 0;

uint8_t beaconPacket[100] = {
  0x80, 0x00, 0x00, 0x00,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
  0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x64, 0x00,
  0x21, 0x04,
  0x00, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x18, 0x30, 0x48,
  0x03, 0x01, 0x01
};

void randomMac() {
  for (int i = 0; i < 6; i++) {
    macAddr[i] = random(256);
  }
}

void nextChannel() {
  static uint8_t channelIndex = 0;
  wifi_channel = channels[channelIndex];
  channelIndex = (channelIndex + 1) % (sizeof(channels) / sizeof(channels[0]));
  esp_wifi_set_channel(wifi_channel, WIFI_SECOND_CHAN_NONE);
}

enum BeaconSpamMode { BEACON_SPAM_MENU, BEACON_SPAM_CLONE_ALL, BEACON_SPAM_CLONE_SELECTED, BEACON_SPAM_CUSTOM };
static BeaconSpamMode beaconSpamMode = BEACON_SPAM_MENU;
static int menuSelection = 0;
static int ssidIndex = 0;

struct ClonedSSID {
    char ssid[32];
    uint8_t channel;
    bool selected;
};
static std::vector<ClonedSSID> scannedSSIDs;
#define MAX_CLONE_SSIDS 50
#define SCAN_INTERVAL 30000
static unsigned long lastScanTime = 0;

void drawBeaconSpamMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 12, "Beacon Spam Mode:");
  u8g2.drawStr(0, 26, menuSelection == 0 ? "> Clone All" : "  Clone All");
  u8g2.drawStr(0, 38, menuSelection == 1 ? "> Clone Selected" : "  Clone Selected");
  u8g2.drawStr(0, 50, menuSelection == 2 ? "> Custom" : "  Custom");
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 62, "U/D=Move R=OK SEL=Exit");
  u8g2.sendBuffer();
}

static void drawSSIDList() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 12, "Select SSID to clone");
    if (!scannedSSIDs.empty()) {
        int idx = ssidIndex;
        char line1[32];
        snprintf(line1, sizeof(line1), "%s  Ch:%d", scannedSSIDs[idx].ssid, scannedSSIDs[idx].channel);
        u8g2.drawStr(0, 28, line1);
        u8g2.drawStr(0, 44, scannedSSIDs[idx].selected ? "[*] Selected" : "[ ] Not selected");
    } else {
        u8g2.drawStr(0, 30, "No SSIDs found");
    }
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 62, "U/D=Move R=Toggle L=Back");
    u8g2.sendBuffer();
}

static void showScanningSSIDs() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 30, "Scanning SSIDs...");
  u8g2.sendBuffer();
}

static void updateSSIDList() {
    showScanningSSIDs();
    std::vector<ClonedSSID> oldList = scannedSSIDs;
    scannedSSIDs.clear();
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n && (int)scannedSSIDs.size() < MAX_CLONE_SSIDS; i++) {
        String ssid = WiFi.SSID(i);
        uint8_t channel = WiFi.channel(i);
        if (ssid.length() > 0 && ssid.length() < 32) {
            ClonedSSID entry;
            strncpy(entry.ssid, ssid.c_str(), sizeof(entry.ssid) - 1);
            entry.ssid[sizeof(entry.ssid) - 1] = '\0';
            entry.channel = channel;
            entry.selected = false;
            for (const auto& old : oldList) {
                if (strcmp(old.ssid, entry.ssid) == 0 && old.channel == entry.channel) {
                    entry.selected = old.selected;
                    break;
                }
            }
            scannedSSIDs.push_back(entry);
        }
    }
}

void beaconSpamSetup() {
  for (int i = 0; i < 32; i++) emptySSID[i] = ' ';
  randomSeed((uint32_t)esp_random());

  packetSize = sizeof(beaconPacket);
  if (!wpa2) {
    beaconPacket[34] = 0x21;
    packetSize -= 26;
  }

  WiFi.mode(WIFI_STA);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channels[0], WIFI_SECOND_CHAN_NONE);

  beaconSpamMode = BEACON_SPAM_MENU;
  menuSelection = 0;
  ssidIndex = 0;
  lastScanTime = millis();
  scannedSSIDs.clear();
  drawBeaconSpamMenu();
}

void beaconSpamLoop() {
  currentTime = millis();
  static unsigned long lastDisplayUpdate = 0;
  static uint32_t lastButtonCheck = 0;
  static uint32_t lastRandom = 0;

  bool up = digitalRead(BUTTON_PIN_UP) == LOW;
  bool down = digitalRead(BUTTON_PIN_DOWN) == LOW;
  bool left = digitalRead(BUTTON_PIN_LEFT) == LOW;
  bool right = digitalRead(BUTTON_PIN_RIGHT) == LOW;
  
  if (up || down || left || right) {
    updateLastActivity();
  }

  bool anySelected = false;
  for (const auto& entry : scannedSSIDs) if (entry.selected) { anySelected = true; break; }

  if ((beaconSpamMode == BEACON_SPAM_CLONE_ALL || beaconSpamMode == BEACON_SPAM_CLONE_SELECTED) && !anySelected && currentTime - lastScanTime >= SCAN_INTERVAL) {
    updateSSIDList();
    lastScanTime = currentTime;
    if (ssidIndex >= (int)scannedSSIDs.size()) ssidIndex = 0;
  }

  switch (beaconSpamMode) {
    case BEACON_SPAM_MENU:
      drawBeaconSpamMenu();
      if (up) {
        menuSelection = (menuSelection - 1 + 3) % 3;
        delay(200);
      }
      if (down) {
        menuSelection = (menuSelection + 1) % 3;
        delay(200);
      }
      if (right) {
        beaconSpamMode = (menuSelection == 0 ? BEACON_SPAM_CLONE_ALL : (menuSelection == 1 ? BEACON_SPAM_CLONE_SELECTED : BEACON_SPAM_CUSTOM));
        if (beaconSpamMode == BEACON_SPAM_CLONE_ALL || beaconSpamMode == BEACON_SPAM_CLONE_SELECTED) {
          updateSSIDList();
        }
        delay(200);
      }
      if (left) {
        beaconSpamMode = BEACON_SPAM_MENU;
        delay(200);
      }
      break;
    case BEACON_SPAM_CLONE_ALL:
      if (currentTime - lastDisplayUpdate >= 250) {
        lastDisplayUpdate = currentTime;
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(0, 10, "Clone All SSIDs");
        char buf[32];
        snprintf(buf, sizeof(buf), "Count: %d", (int)scannedSSIDs.size());
        u8g2.drawStr(0, 25, buf);
        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.drawStr(0, 55, "L=Back SEL=Exit");
        u8g2.sendBuffer();
      }
      for (const auto& entry : scannedSSIDs) {
        randomMac();
        memcpy(&beaconPacket[10], macAddr, 6);
        memcpy(&beaconPacket[16], macAddr, 6);
        memset(&beaconPacket[38], ' ', 32);
        size_t len = strlen(entry.ssid);
        memcpy(&beaconPacket[38], entry.ssid, len);
        beaconPacket[37] = len;
        beaconPacket[82] = entry.channel;
        uint32_t timestamp = micros();
        memcpy(&beaconPacket[24], &timestamp, 4);
        esp_wifi_set_channel(entry.channel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_80211_tx(WIFI_IF_STA, beaconPacket, packetSize, false);
        delayMicroseconds(100);
      }
      if (left) {
        beaconSpamMode = BEACON_SPAM_MENU;
        delay(200);
      }
      break;
    case BEACON_SPAM_CLONE_SELECTED:
      drawSSIDList();
      if (up && !scannedSSIDs.empty()) {
        ssidIndex = (ssidIndex - 1 + scannedSSIDs.size()) % scannedSSIDs.size();
        delay(200);
      }
      if (down && !scannedSSIDs.empty()) {
        ssidIndex = (ssidIndex + 1) % scannedSSIDs.size();
        delay(200);
      }
      if (right && !scannedSSIDs.empty()) {
        scannedSSIDs[ssidIndex].selected = !scannedSSIDs[ssidIndex].selected;
        delay(200);
      }
      if (left) {
        beaconSpamMode = BEACON_SPAM_MENU;
        delay(200);
      }
      
      if (anySelected) {
        static unsigned long lastBeacon = 0;
        const unsigned long beaconInterval = 20;
        if (currentTime - lastBeacon > beaconInterval) {
          for (const auto& entry : scannedSSIDs) {
            if (entry.selected) {
              randomMac();
              memcpy(&beaconPacket[10], macAddr, 6);
              memcpy(&beaconPacket[16], macAddr, 6);
              memset(&beaconPacket[38], ' ', 32);
              size_t len = strlen(entry.ssid);
              memcpy(&beaconPacket[38], entry.ssid, len);
              beaconPacket[37] = len;
              beaconPacket[82] = entry.channel;
              uint32_t timestamp = micros();
              memcpy(&beaconPacket[24], &timestamp, 4);
              esp_wifi_set_channel(entry.channel, WIFI_SECOND_CHAN_NONE);
              esp_wifi_80211_tx(WIFI_IF_STA, beaconPacket, packetSize, false);
              delayMicroseconds(100);
            }
          }
          lastBeacon = currentTime;
        }
      }
      break;
    case BEACON_SPAM_CUSTOM:
      if (currentTime - lastDisplayUpdate >= 250) {
        lastDisplayUpdate = currentTime;
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(0, 10, "Beacon Spam: Custom");
        char status[32];
        snprintf(status, sizeof(status), "Channel: %d", wifi_channel);
        u8g2.drawStr(0, 25, status);
        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.drawStr(0, 55, "L=Back SEL=Exit");
        u8g2.sendBuffer();
      }
      {
        static const int batchSize = 5;
        static int batchIndices[batchSize] = {0};
        static int batchCycle = 0;
        static int batchBeacon = 0;
        static uint32_t lastBatch = 0;
        static uint8_t randomBatchesSinceSwitch = 0;
        static int totalSSIDs = -1;
        if (totalSSIDs == -1) {
          totalSSIDs = 0;
          for (int i = 0; i < strlen_P(ssids); i++) {
            if (pgm_read_byte(ssids + i) == '\n') totalSSIDs++;
          }
        }
        if (batchCycle == 0 || batchBeacon >= 10) {
          for (int i = 0; i < batchSize; i++) {
            batchIndices[i] = random(totalSSIDs);
          }
          batchBeacon = 0;
        }
        if (currentTime - lastBatch > 50) {
          lastBatch = currentTime;
          for (int b = 0; b < batchSize; b++) {
            int randomSSID = batchIndices[b];
            int ssidStart = 0;
            int found = 0;
            for (int i = 0; i < strlen_P(ssids); i++) {
              if (pgm_read_byte(ssids + i) == '\n') {
                if (found == randomSSID) {
                  ssidStart = i + 1;
                  break;
                }
                found++;
              }
            }
            int j = 0;
            char tmp;
            do {
              tmp = pgm_read_byte(ssids + ssidStart + j);
              j++;
            } while (tmp != '\n' && j <= 32 && (ssidStart + j) < (int)strlen_P(ssids));
            uint8_t ssidLen = j - 1;
            randomMac();
            memcpy(&beaconPacket[10], macAddr, 6);
            memcpy(&beaconPacket[16], macAddr, 6);
            memcpy(&beaconPacket[38], emptySSID, 32);
            for (int k = 0; k < ssidLen; k++) {
              beaconPacket[38 + k] = pgm_read_byte(ssids + ssidStart + k);
            }
            beaconPacket[37] = ssidLen;
            beaconPacket[82] = wifi_channel;
            uint32_t timestamp = micros();
            memcpy(&beaconPacket[24], &timestamp, 4);
            esp_wifi_80211_tx(WIFI_IF_STA, beaconPacket, packetSize, false);
          }
          batchBeacon++;
        }
        batchCycle++;
        if (batchCycle >= 10) batchCycle = 0;
        if (++randomBatchesSinceSwitch >= 4) {
          randomBatchesSinceSwitch = 0;
          nextChannel();
        }
      }
      if (left) {
        beaconSpamMode = BEACON_SPAM_MENU;
        delay(200);
      }
      break;
  }
}