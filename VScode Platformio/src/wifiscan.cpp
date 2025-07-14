/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________ */

#include "../include/wifiscan.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define BTN_UP BUTTON_PIN_UP
#define BTN_DOWN BUTTON_PIN_DOWN
#define BTN_RIGHT BUTTON_PIN_RIGHT
#define BTN_BACK BUTTON_PIN_LEFT

struct WiFiNetworkData {
  char ssid[32];
  char bssid[18];
  int8_t rssi;
  uint8_t channel;
  uint8_t encryption;
  unsigned long lastSeen;
};
static std::vector<WiFiNetworkData> wifiNetworks;

const int MAX_NETWORKS = 100;

int currentIndex = 0;
int listStartIndex = 0;
bool isDetailView = false;
unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200;

static bool isScanning = false;
static unsigned long lastScanTime = 0;
const unsigned long scanInterval = 180000;

void wifiscanSetup() {
  wifiNetworks.clear();
  currentIndex = listStartIndex = 0;
  isDetailView = false;
  lastButtonPress = 0;
  isScanning = false;

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Scanning for");
  u8g2.drawStr(0, 20, "WiFi networks...");
  u8g2.sendBuffer();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  
  WiFi.scanNetworks(true);
  isScanning = true;
  lastScanTime = millis();
}

void wifiscanLoop() {
  unsigned long now = millis();

  if (isScanning && now - lastScanTime > 5000) {
    int foundNetworks = WiFi.scanComplete();
    if (foundNetworks >= 0) {
              for (int i = 0; i < foundNetworks && wifiNetworks.size() < MAX_NETWORKS; i++) {
          String ssid = WiFi.SSID(i);
          String bssid = WiFi.BSSIDstr(i);
          int8_t rssi = WiFi.RSSI(i);
          uint8_t channel = WiFi.channel(i);
          uint8_t encryption = WiFi.encryptionType(i);

          bool found = false;
          for (auto &net : wifiNetworks) {
            if (strcmp(net.bssid, bssid.c_str()) == 0) {
              net.rssi = rssi;
              net.lastSeen = now;
              if (!ssid.isEmpty()) {
                strncpy(net.ssid, ssid.c_str(), sizeof(net.ssid) - 1);
                net.ssid[sizeof(net.ssid) - 1] = '\0';
              }
              found = true;
              break;
            }
          }

          if (!found) {
            WiFiNetworkData newNetwork;
            memset(&newNetwork, 0, sizeof(newNetwork));
            strncpy(newNetwork.bssid, bssid.c_str(), sizeof(newNetwork.bssid) - 1);
            newNetwork.rssi = rssi;
            newNetwork.channel = channel;
            newNetwork.encryption = encryption;
            newNetwork.lastSeen = now;
            if (!ssid.isEmpty()) {
              strncpy(newNetwork.ssid, ssid.c_str(), sizeof(newNetwork.ssid) - 1);
              newNetwork.ssid[sizeof(newNetwork.ssid) - 1] = '\0';
            } else {
              strcpy(newNetwork.ssid, "Hidden");
            }
            wifiNetworks.push_back(newNetwork);
          }
        }

        if (wifiNetworks.size() >= MAX_NETWORKS) {
          isScanning = false;
          lastScanTime = now;
        }

      std::sort(wifiNetworks.begin(), wifiNetworks.end(),
                [](const WiFiNetworkData &a, const WiFiNetworkData &b) {
                  return a.rssi > b.rssi;
                });

      isScanning = false;
      lastScanTime = now;
    }
  } else if (!isScanning && now - lastScanTime > scanInterval && wifiNetworks.size() < MAX_NETWORKS) {
    wifiNetworks.clear();
    WiFi.scanNetworks(true);
    isScanning = true;
    lastScanTime = now;
  }

  static bool showingRefresh = false;
  if (!isScanning && now - lastScanTime > scanInterval - 1000) {
    showingRefresh = true;
  } else if (isScanning && showingRefresh) {
    showingRefresh = false;
  }

  if (now - lastButtonPress > debounceTime) {
    if (!isDetailView && digitalRead(BTN_UP) == LOW && currentIndex > 0) {
      --currentIndex;
      if (currentIndex < listStartIndex)
        --listStartIndex;
      lastButtonPress = now;
    } else if (!isDetailView && digitalRead(BTN_DOWN) == LOW &&
               currentIndex < (int)wifiNetworks.size() - 1) {
      ++currentIndex;
      if (currentIndex >= listStartIndex + 5)
        ++listStartIndex;
      lastButtonPress = now;
    } else if (!isDetailView && digitalRead(BTN_RIGHT) == LOW &&
               !wifiNetworks.empty()) {
      isDetailView = true;
      lastButtonPress = now;
    } else if (digitalRead(BTN_BACK) == LOW) {
      isDetailView = false;
      lastButtonPress = now;
    }
  }

  if (wifiNetworks.empty()) {
    currentIndex = listStartIndex = 0;
    isDetailView = false;
  } else {
    currentIndex = constrain(currentIndex, 0, (int)wifiNetworks.size() - 1);
    listStartIndex =
        constrain(listStartIndex, 0, max(0, (int)wifiNetworks.size() - 5));
  }

  u8g2.clearBuffer();
  if (showingRefresh) {
    u8g2.drawStr(0, 10, "Refreshing");
    u8g2.drawStr(0, 20, "WiFi networks...");
    u8g2.sendBuffer();
  } else if (wifiNetworks.empty()) {
    u8g2.drawStr(0, 10, "Scanning for");
    u8g2.drawStr(0, 20, "WiFi networks...");
    u8g2.drawStr(0, 45, "Press SEL to stop");
  } else if (isDetailView) {
    auto &net = wifiNetworks[currentIndex];
    u8g2.setFont(u8g2_font_5x8_tr);
    char buf[32];
    snprintf(buf, sizeof(buf), "SSID: %s", net.ssid);
    u8g2.drawStr(0, 10, buf);
    snprintf(buf, sizeof(buf), "BSSID: %s", net.bssid);
    u8g2.drawStr(0, 20, buf);
    snprintf(buf, sizeof(buf), "RSSI: %d", net.rssi);
    u8g2.drawStr(0, 30, buf);
    snprintf(buf, sizeof(buf), "Channel: %d", net.channel);
    u8g2.drawStr(0, 40, buf);
    snprintf(buf, sizeof(buf), "Age: %lus", (now - net.lastSeen) / 1000);
    u8g2.drawStr(0, 50, buf);
    u8g2.drawStr(0, 60, "Press LEFT to go back");
  } else {
    u8g2.setFont(u8g2_font_6x10_tr);
    char header[32];
    snprintf(header, sizeof(header), "WiFi Networks: %d/%d", (int)wifiNetworks.size(), MAX_NETWORKS);
    u8g2.drawStr(0, 10, header);
    for (int i = 0; i < 5; ++i) {
      int idx = listStartIndex + i;
      if (idx >= (int)wifiNetworks.size())
        break;
      auto &n = wifiNetworks[idx];
      if (idx == currentIndex)
        u8g2.drawStr(0, 20 + i * 10, ">");
      char line[32];
      snprintf(line, sizeof(line), "%.8s | RSSI %d",
               n.ssid[0] ? n.ssid : "Hidden", n.rssi);
      u8g2.drawStr(10, 20 + i * 10, line);
    }
  }
  u8g2.sendBuffer();
}
