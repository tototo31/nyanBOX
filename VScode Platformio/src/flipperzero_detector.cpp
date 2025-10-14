/*
   ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#include "../include/flipperzero_detector.h"
#include "../include/sleep_manager.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define BTN_UP BUTTON_PIN_UP
#define BTN_DOWN BUTTON_PIN_DOWN
#define BTN_RIGHT BUTTON_PIN_RIGHT
#define BTN_BACK BUTTON_PIN_LEFT
#define BTN_CENTER BUTTON_PIN_CENTER

struct FlipperZeroDeviceData {
  char name[32];
  char address[18];
  char color[16];
  int8_t rssi;
  bool hasName;
  unsigned long lastSeen;
  bool isFlipperZero;
};
static std::vector<FlipperZeroDeviceData> flipperZeroDevices;

const int MAX_DEVICES = 100;
const String FLIPPERZERO_MAC_PREFIX_1 = "80:e1:26";
const String FLIPPERZERO_MAC_PREFIX_2 = "80:e1:27";
const String FLIPPERZERO_MAC_PREFIX_3 = "0C:FA:22";

int currentIndex = 0;
int listStartIndex = 0;
bool isDetailView = false;
unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200;

static BLEScan *pBLEScan;
static bool isScanning = false;
static unsigned long lastScanTime = 0;
const unsigned long scanInterval = 30000;

static int flipperCallbackCount = 0;
static unsigned long lastCallbackTime = 0;

const char* getFlipperColor(BLEAdvertisedDevice &device) {
  if (!device.haveServiceUUID()) {
    return "Unknown";
  }

  BLEUUID primaryUUID = device.getServiceUUID();
  std::string uuidStr = primaryUUID.toString();

  for (auto& c : uuidStr) c = tolower(c);

  if (uuidStr == "00003081-0000-1000-8000-00805f9b34fb") {
    return "Black";
  }
  if (uuidStr == "00003082-0000-1000-8000-00805f9b34fb") {
    return "White";
  }
  if (uuidStr == "00003083-0000-1000-8000-00805f9b34fb") {
    return "Transparent";
  }

  if (uuidStr.find("0000308") == 0 &&
      uuidStr.find("0000-1000-8000-00805f9b34fb") != std::string::npos) {
    return "Generic";
  }

  return "Unknown";
}

class MyFlipperAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    flipperCallbackCount++;

    unsigned long now = millis();
    if (now - lastCallbackTime < 50) {
      return;
    }
    lastCallbackTime = now;

    if (flipperCallbackCount > 500 || flipperZeroDevices.size() >= MAX_DEVICES) {
      if (isScanning && pBLEScan) {
        pBLEScan->stop();
        isScanning = false;
      }
      return;
    }

    if (flipperZeroDevices.size() > 80 && flipperCallbackCount % 2 != 0) return;

    BLEAddress addr = advertisedDevice.getAddress();
    const char* addrCStr = addr.toString().c_str();

    char addrStr[18];
    strncpy(addrStr, addrCStr, 17);
    addrStr[17] = '\0';

    if (strlen(addrStr) < 12) return;

    bool isFlipperZero = (strncmp(addrStr, "80:e1:26", 8) == 0) ||
                         (strncmp(addrStr, "80:e1:27", 8) == 0) ||
                         (strncmp(addrStr, "0c:fa:22", 8) == 0) ||
                         (strncmp(addrStr, "0C:FA:22", 8) == 0);
    
    if (!isFlipperZero) {
      return;
    }

    int8_t deviceRSSI = advertisedDevice.getRSSI();

    const char* detectedColor = getFlipperColor(advertisedDevice);

    for (int i = 0; i < flipperZeroDevices.size(); i++) {
      if (strcmp(flipperZeroDevices[i].address, addrStr) == 0) {
        flipperZeroDevices[i].rssi = deviceRSSI;
        flipperZeroDevices[i].lastSeen = millis();

        if (strcmp(detectedColor, "Unknown") != 0 && strcmp(detectedColor, "Generic") != 0) {
          strncpy(flipperZeroDevices[i].color, detectedColor, 15);
          flipperZeroDevices[i].color[15] = '\0';
        }

        if (!flipperZeroDevices[i].hasName && advertisedDevice.haveName()) {
          std::string nameStd = advertisedDevice.getName();
          if (nameStd.length() > 0 && nameStd.length() < 32) {
            strncpy(flipperZeroDevices[i].name, nameStd.c_str(), 31);
            flipperZeroDevices[i].name[31] = '\0';
            flipperZeroDevices[i].hasName = true;
          }
        }

        std::sort(flipperZeroDevices.begin(), flipperZeroDevices.end(),
                  [](const FlipperZeroDeviceData &a, const FlipperZeroDeviceData &b) {
                    return a.rssi > b.rssi;
                  });
        return;
      }
    }

    FlipperZeroDeviceData newDev = {};
    strncpy(newDev.address, addrStr, 17);
    newDev.address[17] = '\0';
    newDev.rssi = deviceRSSI;
    newDev.lastSeen = millis();
    newDev.isFlipperZero = true;

    strncpy(newDev.color, detectedColor, 15);
    newDev.color[15] = '\0';

    strcpy(newDev.name, "Flipper Zero");
    newDev.hasName = false;

    if (advertisedDevice.haveName()) {
      std::string nameStd = advertisedDevice.getName();
      if (nameStd.length() > 0 && nameStd.length() < 32) {
        strncpy(newDev.name, nameStd.c_str(), 31);
        newDev.name[31] = '\0';
        newDev.hasName = true;
      }
    }

    flipperZeroDevices.push_back(newDev);
    
    std::sort(flipperZeroDevices.begin(), flipperZeroDevices.end(),
              [](const FlipperZeroDeviceData &a, const FlipperZeroDeviceData &b) {
                return a.rssi > b.rssi;
              });
  }
};

static MyFlipperAdvertisedDeviceCallbacks flipperCallbacks;

void flipperZeroDetectorSetup() {
  flipperZeroDevices.clear();
  flipperZeroDevices.reserve(MAX_DEVICES);
  currentIndex = listStartIndex = 0;
  isDetailView = false;
  lastButtonPress = 0;
  isScanning = false;
  flipperCallbackCount = 0;
  lastCallbackTime = 0;

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Scanning for");
  u8g2.drawStr(0, 20, "Flippers...");
  u8g2.sendBuffer();

  BLEDevice::init("FlipperZeroDetector");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(&flipperCallbacks);
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
  pinMode(BTN_CENTER, INPUT_PULLUP);
}

void flipperZeroDetectorLoop() {
  unsigned long now = millis();

  if (now - lastScanTime > 10000) {
    flipperCallbackCount = 0;
  }

  if (isScanning && now - lastScanTime > 5000) {
    pBLEScan->stop();
    isScanning = false;
    flipperCallbackCount = 0;
    lastScanTime = now;
  } 
  else if (!isScanning && now - lastScanTime > scanInterval) {
    if (flipperZeroDevices.size() >= MAX_DEVICES) {
      std::sort(flipperZeroDevices.begin(), flipperZeroDevices.end(),
                [](const FlipperZeroDeviceData &a, const FlipperZeroDeviceData &b) {
                  if (a.lastSeen != b.lastSeen) {
                    return a.lastSeen < b.lastSeen;
                  }
                  return a.rssi < b.rssi;
                });
      
      int devicesToRemove = MAX_DEVICES / 4;
      if (devicesToRemove > 0) {
        flipperZeroDevices.erase(flipperZeroDevices.begin(), 
                                flipperZeroDevices.begin() + devicesToRemove);
      }
      
      currentIndex = listStartIndex = 0;
    }
    
    flipperCallbackCount = 0;
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
               currentIndex < (int)flipperZeroDevices.size() - 1) {
      ++currentIndex;
      if (currentIndex >= listStartIndex + 5)
        ++listStartIndex;
      lastButtonPress = now;
    } else if (!isDetailView && digitalRead(BTN_RIGHT) == LOW &&
               !flipperZeroDevices.empty()) {
      isDetailView = true;
      lastButtonPress = now;
    } else if (digitalRead(BTN_BACK) == LOW) {
      isDetailView = false;
      lastButtonPress = now;
    }
  }

  if (flipperZeroDevices.empty()) {
    currentIndex = listStartIndex = 0;
    isDetailView = false;
  } else {
    currentIndex = constrain(currentIndex, 0, (int)flipperZeroDevices.size() - 1);
    listStartIndex =
        constrain(listStartIndex, 0, max(0, (int)flipperZeroDevices.size() - 5));
  }

  u8g2.clearBuffer();
  if (showingRefresh) {
    u8g2.drawStr(0, 10, "Refreshing");
    u8g2.drawStr(0, 20, "Flippers...");
    u8g2.sendBuffer();
  } else if (flipperZeroDevices.empty()) {
    u8g2.drawStr(0, 10, "Scanning for");
    u8g2.drawStr(0, 20, "Flippers...");
    u8g2.drawStr(0, 35, "Nearby: n/a");
    u8g2.drawStr(0, 50, "Press CENTER to exit");
  } else if (isDetailView) {
    auto &dev = flipperZeroDevices[currentIndex];
    u8g2.setFont(u8g2_font_5x8_tr);
    char buf[32];
    snprintf(buf, sizeof(buf), "Name: %s", dev.name);
    u8g2.drawStr(0, 10, buf);
    snprintf(buf, sizeof(buf), "MAC: %s", dev.address);
    u8g2.drawStr(0, 20, buf);
    snprintf(buf, sizeof(buf), "Color: %s", dev.color);
    u8g2.drawStr(0, 30, buf);
    snprintf(buf, sizeof(buf), "RSSI: %d dBm", dev.rssi);
    u8g2.drawStr(0, 40, buf);
    snprintf(buf, sizeof(buf), "Age: %lus", (millis() - dev.lastSeen) / 1000);
    u8g2.drawStr(0, 50, buf);
    u8g2.drawStr(0, 60, "Press LEFT to go back");
  } else {
    u8g2.setFont(u8g2_font_6x10_tr);
    char header[32];
    snprintf(header, sizeof(header), "Flippers: %d/%d",
             (int)flipperZeroDevices.size(), MAX_DEVICES);
    u8g2.drawStr(0, 10, header);

    for (int i = 0; i < 5; ++i) {
      int idx = listStartIndex + i;
      if (idx >= (int)flipperZeroDevices.size())
        break;
      auto &d = flipperZeroDevices[idx];
      if (idx == currentIndex)
        u8g2.drawStr(0, 20 + i * 10, ">");
      char line[32];
      snprintf(line, sizeof(line), "%.8s | RSSI %d",
               d.name[0] ? d.name : "Flipper", d.rssi);
      u8g2.drawStr(10, 20 + i * 10, line);
    }
  }
  u8g2.sendBuffer();
}
