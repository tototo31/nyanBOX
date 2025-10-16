/*
   ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#include "../include/axon_detector.h"
#include "../include/sleep_manager.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <vector>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define BTN_UP BUTTON_PIN_UP
#define BTN_DOWN BUTTON_PIN_DOWN
#define BTN_RIGHT BUTTON_PIN_RIGHT
#define BTN_BACK BUTTON_PIN_LEFT
#define BTN_CENTER BUTTON_PIN_CENTER

struct AxonDeviceData {
  char name[32];
  char address[18];
  int8_t rssi;
  unsigned long lastSeen;
};

static std::vector<AxonDeviceData> axonDevices;
const int MAX_DEVICES = 100;

int currentIndex = 0;
int listStartIndex = 0;
bool isDetailView = false;
unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200;

static BLEScan *pBLEScan = nullptr;
static bool isScanning = false;
static unsigned long lastScanTime = 0;
const unsigned long SCAN_INTERVAL = 30000;
const unsigned long SCAN_DURATION = 5000;

static int axonCallbackCount = 0;
static unsigned long lastCallbackTime = 0;

class AxonAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    axonCallbackCount++;

    unsigned long now = millis();
    if (now - lastCallbackTime < 50) {
      return;
    }
    lastCallbackTime = now;

    if (axonCallbackCount > 500 || axonDevices.size() >= MAX_DEVICES) {
      if (isScanning && pBLEScan) {
        pBLEScan->stop();
        isScanning = false;
      }
      return;
    }

    if (axonDevices.size() > 80 && axonCallbackCount % 2 != 0) return;

    BLEAddress addr = advertisedDevice.getAddress();
    const char* addrCStr = addr.toString().c_str();

    char addrStr[18];
    strncpy(addrStr, addrCStr, 17);
    addrStr[17] = '\0';

    if (strlen(addrStr) < 12) return;

    if (strncasecmp(addrStr, "00:25:df", 8) != 0) {
      return;
    }

    int8_t deviceRSSI = advertisedDevice.getRSSI();

    for (int i = 0; i < axonDevices.size(); i++) {
      if (strcmp(axonDevices[i].address, addrStr) == 0) {
        axonDevices[i].rssi = deviceRSSI;
        axonDevices[i].lastSeen = millis();

        if (advertisedDevice.haveName()) {
          std::string nameStd = advertisedDevice.getName();
          if (nameStd.length() > 0 && nameStd.length() < 32) {
            strncpy(axonDevices[i].name, nameStd.c_str(), 31);
            axonDevices[i].name[31] = '\0';
          }
        }

        std::sort(axonDevices.begin(), axonDevices.end(),
                  [](const AxonDeviceData &a, const AxonDeviceData &b) {
                    return a.rssi > b.rssi;
                  });
        return;
      }
    }

    AxonDeviceData newDev = {};
    strncpy(newDev.address, addrStr, 17);
    newDev.address[17] = '\0';
    newDev.rssi = deviceRSSI;
    newDev.lastSeen = millis();

    if (advertisedDevice.haveName()) {
      std::string nameStd = advertisedDevice.getName();
      if (nameStd.length() > 0 && nameStd.length() < 32) {
        strncpy(newDev.name, nameStd.c_str(), 31);
        newDev.name[31] = '\0';
      } else {
        strcpy(newDev.name, "Axon Device");
      }
    } else {
      strcpy(newDev.name, "Axon Device");
    }

    axonDevices.push_back(newDev);

    std::sort(axonDevices.begin(), axonDevices.end(),
              [](const AxonDeviceData &a, const AxonDeviceData &b) {
                return a.rssi > b.rssi;
              });
  }
};

static AxonAdvertisedDeviceCallbacks axonCallbacks;

void axonDetectorSetup() {
  axonDevices.clear();
  axonDevices.reserve(MAX_DEVICES);
  currentIndex = listStartIndex = 0;
  isDetailView = false;
  lastButtonPress = 0;
  isScanning = false;
  axonCallbackCount = 0;
  lastCallbackTime = 0;

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Axon Detector");
  u8g2.drawStr(0, 20, "Initializing...");
  u8g2.sendBuffer();

  BLEDevice::init("AxonDetector");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(&axonCallbacks);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(1000);
  pBLEScan->setWindow(200);

  pBLEScan->start(SCAN_DURATION / 1000, false);
  isScanning = true;
  lastScanTime = millis();

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(BTN_CENTER, INPUT_PULLUP);
}

void axonDetectorLoop() {
  unsigned long now = millis();

  if (now - lastScanTime > 10000) {
    axonCallbackCount = 0;
  }

  if (isScanning && now - lastScanTime > SCAN_DURATION) {
    pBLEScan->stop();
    isScanning = false;
    axonCallbackCount = 0;
    lastScanTime = now;
  }
  else if (!isScanning && now - lastScanTime > SCAN_INTERVAL) {
    if (axonDevices.size() >= MAX_DEVICES) {
      std::sort(axonDevices.begin(), axonDevices.end(),
                [](const AxonDeviceData &a, const AxonDeviceData &b) {
                  if (a.lastSeen != b.lastSeen) {
                    return a.lastSeen < b.lastSeen;
                  }
                  return a.rssi < b.rssi;
                });

      int devicesToRemove = MAX_DEVICES / 4;
      if (devicesToRemove > 0) {
        axonDevices.erase(axonDevices.begin(),
                            axonDevices.begin() + devicesToRemove);
      }

      currentIndex = listStartIndex = 0;
    }

    axonCallbackCount = 0;
    pBLEScan->start(SCAN_DURATION / 1000, false);
    isScanning = true;
    lastScanTime = now;
  }

  static bool showingRefresh = false;
  if (!isScanning && now - lastScanTime > SCAN_INTERVAL - 1000) {
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
               currentIndex < (int)axonDevices.size() - 1) {
      ++currentIndex;
      if (currentIndex >= listStartIndex + 5)
        ++listStartIndex;
      lastButtonPress = now;
    } else if (!isDetailView && digitalRead(BTN_RIGHT) == LOW &&
               !axonDevices.empty()) {
      isDetailView = true;
      lastButtonPress = now;
    } else if (digitalRead(BTN_BACK) == LOW) {
      isDetailView = false;
      lastButtonPress = now;
    } else if (digitalRead(BTN_CENTER) == LOW) {
      lastButtonPress = now;
      return;
    }
  }

  if (axonDevices.empty()) {
    currentIndex = listStartIndex = 0;
    isDetailView = false;
  } else {
    currentIndex = constrain(currentIndex, 0, (int)axonDevices.size() - 1);
    listStartIndex = constrain(listStartIndex, 0, max(0, (int)axonDevices.size() - 5));
  }

  u8g2.clearBuffer();
  if (showingRefresh) {
    u8g2.drawStr(0, 10, "Refreshing");
    u8g2.drawStr(0, 20, "Axon Devices...");
    u8g2.sendBuffer();
  } else if (axonDevices.empty()) {
    u8g2.drawStr(0, 10, "Scanning for");
    u8g2.drawStr(0, 20, "Axon Devices...");
    u8g2.drawStr(0, 35, "Nearby: n/a");
    u8g2.drawStr(0, 50, "Press CENTER to exit");
  } else if (isDetailView) {
    auto &dev = axonDevices[currentIndex];
    u8g2.setFont(u8g2_font_5x8_tr);
    char buf[32];
    snprintf(buf, sizeof(buf), "Name: %s", dev.name);
    u8g2.drawStr(0, 10, buf);
    snprintf(buf, sizeof(buf), "MAC: %s", dev.address);
    u8g2.drawStr(0, 20, buf);
    snprintf(buf, sizeof(buf), "RSSI: %d dBm", dev.rssi);
    u8g2.drawStr(0, 30, buf);
    snprintf(buf, sizeof(buf), "Age: %lus", (millis() - dev.lastSeen) / 1000);
    u8g2.drawStr(0, 40, buf);
    u8g2.drawStr(0, 60, "Press LEFT to go back");
  } else {
    u8g2.setFont(u8g2_font_6x10_tr);
    char header[32];
    snprintf(header, sizeof(header), "Axon Devices: %d/%d",
             (int)axonDevices.size(), MAX_DEVICES);
    u8g2.drawStr(0, 10, header);

    for (int i = 0; i < 5; ++i) {
      int idx = listStartIndex + i;
      if (idx >= (int)axonDevices.size())
        break;
      auto &d = axonDevices[idx];
      if (idx == currentIndex)
        u8g2.drawStr(0, 20 + i * 10, ">");
      char line[32];
      snprintf(line, sizeof(line), "%.8s | RSSI %d",
               d.name[0] ? d.name : "Axon Device", d.rssi);
      u8g2.drawStr(10, 20 + i * 10, line);
    }
  }
  u8g2.sendBuffer();
}