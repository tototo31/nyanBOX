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

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    if (bleDevices.size() >= MAX_DEVICES) {
      if (isScanning) {
        pBLEScan->stop();
        isScanning = false;
      }
      return;
    }

    std::string nameStd = advertisedDevice.getName();
    const char *deviceName = nameStd.c_str();
    std::string addrStd = advertisedDevice.getAddress().toString();
    const char *deviceAddress = addrStd.c_str();
    int8_t deviceRSSI = advertisedDevice.getRSSI();
    bool hasName = advertisedDevice.haveName();

    for (auto &dev : bleDevices) {
      if (strcmp(dev.address, deviceAddress) == 0) {
        dev.rssi = deviceRSSI;
        dev.lastSeen = millis();
        if (hasName && deviceName[0]) {
          strncpy(dev.name, deviceName, sizeof(dev.name) - 1);
          dev.name[sizeof(dev.name) - 1] = '\0';
          dev.hasName = true;
        }
        return;
      }
    }

    BLEDeviceData newDev;
    memset(&newDev, 0, sizeof(newDev));
    strncpy(newDev.address, deviceAddress, sizeof(newDev.address) - 1);
    newDev.rssi = deviceRSSI;
    newDev.lastSeen = millis();
    if (hasName && deviceName[0]) {
      strncpy(newDev.name, deviceName, sizeof(newDev.name) - 1);
      newDev.name[sizeof(newDev.name) - 1] = '\0';
      newDev.hasName = true;
    } else {
      strcpy(newDev.name, "Unknown");
      newDev.hasName = false;
    }
    bleDevices.push_back(newDev);

    std::sort(bleDevices.begin(), bleDevices.end(),
              [](const BLEDeviceData &a, const BLEDeviceData &b) {
                return a.rssi > b.rssi;
              });
  }
};

void blescanSetup() {
  bleDevices.clear();
  currentIndex = listStartIndex = 0;
  isDetailView = false;
  lastButtonPress = 0;
  isScanning = false;

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Scanning for");
  u8g2.drawStr(0, 20, "BLE devices...");
  u8g2.sendBuffer();

  BLEDevice::init("BLEScanner");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
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

  if (isScanning && now - lastScanTime > 5000) {
    pBLEScan->stop();
    isScanning = false;
    lastScanTime = now;
  } else if (!isScanning && now - lastScanTime > scanInterval) {
    bleDevices.clear();
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
      updateLastActivity();
      lastButtonPress = now;
    } else if (!isDetailView && digitalRead(BTN_DOWN) == LOW &&
               currentIndex < (int)bleDevices.size() - 1) {
      ++currentIndex;
      if (currentIndex >= listStartIndex + 5)
        ++listStartIndex;
      updateLastActivity();
      lastButtonPress = now;
    } else if (!isDetailView && digitalRead(BTN_RIGHT) == LOW &&
               !bleDevices.empty()) {
      isDetailView = true;
      updateLastActivity();
      lastButtonPress = now;
    } else if (digitalRead(BTN_BACK) == LOW) {
      isDetailView = false;
      updateLastActivity();
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
      snprintf(line, sizeof(line), "%.8s | RSSI %d",
               d.name[0] ? d.name : "Unknown", d.rssi);
      u8g2.drawStr(10, 20 + i * 10, line);
    }
  }
  u8g2.sendBuffer();
}