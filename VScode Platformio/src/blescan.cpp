/*
   ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#include "../include/blescan.h"
#include "../include/sleep_manager.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define BTN_UP BUTTON_PIN_UP
#define BTN_DOWN BUTTON_PIN_DOWN
#define BTN_RIGHT BUTTON_PIN_RIGHT
#define BTN_BACK BUTTON_PIN_LEFT

struct BLEDeviceData {
  char name[32];
  char address[18];
  int8_t rssi;
  bool hasName;
  unsigned long lastSeen;
};
static std::vector<BLEDeviceData> bleDevices;

const int MAX_DEVICES = 100;

int currentIndex = 0;
int listStartIndex = 0;
bool isDetailView = false;
unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200;

static BLEScan *pBLEScan;
static bool isScanning = false;
static unsigned long lastScanTime = 0;
const unsigned long scanInterval = 180000;

static int callbackCount = 0;
static unsigned long lastCallbackTime = 0;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    callbackCount++;

    unsigned long now = millis();
    if (now - lastCallbackTime < 50) {
      return;
    }
    lastCallbackTime = now;

    if (callbackCount > 500 || bleDevices.size() >= MAX_DEVICES) {
      if (isScanning && pBLEScan) {
        pBLEScan->stop();
        isScanning = false;
      }
      return;
    }

    if (bleDevices.size() > 80 && callbackCount % 2 != 0) return;

    BLEAddress addr = advertisedDevice.getAddress();
    const char* addrCStr = addr.toString().c_str();

    char addrStr[18];
    strncpy(addrStr, addrCStr, 17);
    addrStr[17] = '\0';

    if (strlen(addrStr) < 12) return;

    for (int i = 0; i < bleDevices.size(); i++) {
      if (strcmp(bleDevices[i].address, addrStr) == 0) {
        bleDevices[i].rssi = advertisedDevice.getRSSI();
        bleDevices[i].lastSeen = millis();
        
        std::sort(bleDevices.begin(), bleDevices.end(),
                  [](const BLEDeviceData &a, const BLEDeviceData &b) {
                    return a.rssi > b.rssi;
                  });
        return;
      }
    }

    BLEDeviceData newDev = {};
    strncpy(newDev.address, addrStr, 17);
    newDev.address[17] = '\0';
    newDev.rssi = advertisedDevice.getRSSI();
    newDev.lastSeen = millis();
    
    strcpy(newDev.name, "Unknown");
    newDev.hasName = false;

    if (advertisedDevice.haveName()) {
      std::string nameStd = advertisedDevice.getName();
      if (nameStd.length() > 0 && nameStd.length() < 32) {
        strncpy(newDev.name, nameStd.c_str(), 31);
        newDev.name[31] = '\0';
        newDev.hasName = true;
      }
    }
    
    if (!newDev.hasName && advertisedDevice.haveServiceData()) {
    }

    bleDevices.push_back(newDev);
    
    std::sort(bleDevices.begin(), bleDevices.end(),
              [](const BLEDeviceData &a, const BLEDeviceData &b) {
                return a.rssi > b.rssi;
              });
  }
};

static MyAdvertisedDeviceCallbacks blescanCallbacks;

void blescanSetup() {
  bleDevices.clear();
  bleDevices.reserve(MAX_DEVICES);
  currentIndex = listStartIndex = 0;
  isDetailView = false;
  lastButtonPress = 0;
  isScanning = false;
  callbackCount = 0;
  lastCallbackTime = 0;

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Scanning for");
  u8g2.drawStr(0, 20, "BLE devices...");
  u8g2.sendBuffer();

  BLEDevice::init("BLEScanner");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(&blescanCallbacks);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(1000);
  pBLEScan->setWindow(200);
  
  pBLEScan->start(5, false);
  isScanning = true;
  lastScanTime = millis();

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
}

void blescanLoop() {
  unsigned long now = millis();

  if (now - lastScanTime > 10000) {
    callbackCount = 0;
  }

  if (isScanning && now - lastScanTime > 5000) {
    pBLEScan->stop();
    isScanning = false;
    callbackCount = 0;
    lastScanTime = now;
  } 
  else if (!isScanning && now - lastScanTime > scanInterval) {
    if (bleDevices.size() >= MAX_DEVICES) {
      std::sort(bleDevices.begin(), bleDevices.end(),
                [](const BLEDeviceData &a, const BLEDeviceData &b) {
                  if (a.lastSeen != b.lastSeen) {
                    return a.lastSeen < b.lastSeen;
                  }
                  return a.rssi < b.rssi;
                });
      
      int devicesToRemove = MAX_DEVICES / 4;
      if (devicesToRemove > 0) {
        bleDevices.erase(bleDevices.begin(), 
                        bleDevices.begin() + devicesToRemove);
      }
      
      currentIndex = listStartIndex = 0;
    }
    
    callbackCount = 0;
    pBLEScan->start(5, false);
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
               currentIndex < (int)bleDevices.size() - 1) {
      ++currentIndex;
      if (currentIndex >= listStartIndex + 5)
        ++listStartIndex;
      lastButtonPress = now;
    } else if (!isDetailView && digitalRead(BTN_RIGHT) == LOW &&
               !bleDevices.empty()) {
      isDetailView = true;
      lastButtonPress = now;
    } else if (digitalRead(BTN_BACK) == LOW) {
      isDetailView = false;
      lastButtonPress = now;
    }
  }

  if (bleDevices.empty()) {
    currentIndex = listStartIndex = 0;
    isDetailView = false;
  } else {
    currentIndex = constrain(currentIndex, 0, (int)bleDevices.size() - 1);
    listStartIndex =
        constrain(listStartIndex, 0, max(0, (int)bleDevices.size() - 5));
  }

  u8g2.clearBuffer();
  if (showingRefresh) {
    u8g2.drawStr(0, 10, "Refreshing");
    u8g2.drawStr(0, 20, "BLE devices...");
    u8g2.sendBuffer();
  } else if (bleDevices.empty()) {
    u8g2.drawStr(0, 10, "Scanning for");
    u8g2.drawStr(0, 20, "BLE devices...");
    u8g2.drawStr(0, 45, "Press SEL to stop");
  } else if (isDetailView) {
    auto &dev = bleDevices[currentIndex];
    u8g2.setFont(u8g2_font_5x8_tr);
    char buf[32];
    snprintf(buf, sizeof(buf), "Name: %s", dev.name);
    u8g2.drawStr(0, 10, buf);
    snprintf(buf, sizeof(buf), "Addr: %s", dev.address);
    u8g2.drawStr(0, 20, buf);
    snprintf(buf, sizeof(buf), "RSSI: %d", dev.rssi);
    u8g2.drawStr(0, 30, buf);
    snprintf(buf, sizeof(buf), "Age: %lus", (millis() - dev.lastSeen) / 1000);
    u8g2.drawStr(0, 40, buf);
    u8g2.drawStr(0, 60, "Press LEFT to go back");
  } else {
    u8g2.setFont(u8g2_font_6x10_tr);
    char header[32];
    snprintf(header, sizeof(header), "BLE Devices: %d/%d", (int)bleDevices.size(), MAX_DEVICES);
    u8g2.drawStr(0, 10, header);
    for (int i = 0; i < 5; ++i) {
      int idx = listStartIndex + i;
      if (idx >= (int)bleDevices.size())
        break;
      auto &d = bleDevices[idx];
      if (idx == currentIndex)
        u8g2.drawStr(0, 20 + i * 10, ">");
      char line[32];
      const char* displayName = d.hasName && d.name[0] ? d.name : "Unknown";
      snprintf(line, sizeof(line), "%.8s | RSSI %d", displayName, d.rssi);
      u8g2.drawStr(10, 20 + i * 10, line);
    }
  }
  u8g2.sendBuffer();
}