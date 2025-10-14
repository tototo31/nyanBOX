/*
   ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#include "../include/nyanbox_detector.h"
#include "../include/nyanbox_common.h"
#include "../include/sleep_manager.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define BTN_UP BUTTON_PIN_UP
#define BTN_DOWN BUTTON_PIN_DOWN
#define BTN_RIGHT BUTTON_PIN_RIGHT
#define BTN_BACK BUTTON_PIN_LEFT
#define BTN_CENTER BUTTON_PIN_CENTER

struct NyanBoxDevice {
  char name[32];
  char address[18];
  int8_t rssi;
  unsigned long lastSeen;
  uint16_t level;
  char version[16];
};

static std::vector<NyanBoxDevice> nyanBoxDevices;
const int MAX_DEVICES = 100;

int currentIndex = 0;
int listStartIndex = 0;
bool isDetailView = false;
unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200;

static BLEScan *pNyanboxScan = nullptr;
static bool nyanboxScanning = false;
static unsigned long nyanboxLastScanTime = 0;
const unsigned long SCAN_INTERVAL = 30000;
const unsigned long SCAN_DURATION = 5000;

static int nyanboxCallbackCount = 0;
static unsigned long lastCallbackTime = 0;

void parseManufacturerData(const std::string &manufData, uint16_t &level,
                           char *version) {
  if (manufData.length() < 8 || manufData[0] != (char)0xFF || manufData[1] != (char)0xFF) {
    level = 0;
    strcpy(version, "Unknown");
    return;
  }

  level = ((uint8_t)manufData[2] << 8) | (uint8_t)manufData[3];
  uint32_t versionNum = ((uint8_t)manufData[4] << 24) |
                        ((uint8_t)manufData[5] << 16) |
                        ((uint8_t)manufData[6] << 8) | (uint8_t)manufData[7];

  int major = versionNum / 10000;
  int minor = (versionNum / 100) % 100;
  int patch = versionNum % 100;

  if (minor == 0 && patch == 0)
    snprintf(version, 16, "v%d", major);
  else if (patch == 0)
    snprintf(version, 16, "v%d.%d", major, minor);
  else
    snprintf(version, 16, "v%d.%d.%d", major, minor, patch);
}

class NyanboxAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    nyanboxCallbackCount++;

    unsigned long now = millis();
    if (now - lastCallbackTime < 50) {
      return;
    }
    lastCallbackTime = now;

    if (nyanboxCallbackCount > 500 || nyanBoxDevices.size() >= MAX_DEVICES) {
      if (nyanboxScanning && pNyanboxScan) {
        pNyanboxScan->stop();
        nyanboxScanning = false;
      }
      return;
    }

    if (nyanBoxDevices.size() > 80 && nyanboxCallbackCount % 2 != 0) return;

    if (!advertisedDevice.isAdvertisingService(BLEUUID(NYANBOX_SERVICE_UUID)))
      return;

    BLEAddress addr = advertisedDevice.getAddress();
    const char *addrCStr = addr.toString().c_str();

    char deviceAddress[18];
    strncpy(deviceAddress, addrCStr, 17);
    deviceAddress[17] = '\0';

    int8_t deviceRSSI = advertisedDevice.getRSSI();

    for (auto &dev : nyanBoxDevices) {
      if (strcmp(dev.address, deviceAddress) == 0) {
        dev.rssi = deviceRSSI;
        dev.lastSeen = millis();
        if (advertisedDevice.haveManufacturerData()) {
          parseManufacturerData(advertisedDevice.getManufacturerData(),
                                dev.level, dev.version);
        }
        return;
      }
    }

    NyanBoxDevice newDev = {};
    strncpy(newDev.address, deviceAddress, sizeof(newDev.address) - 1);
    newDev.rssi = deviceRSSI;
    newDev.lastSeen = millis();
    strcpy(newDev.version, "Unknown");

    if (advertisedDevice.haveManufacturerData()) {
      parseManufacturerData(advertisedDevice.getManufacturerData(),
                            newDev.level, newDev.version);
    }

    const char *deviceName = advertisedDevice.getName().c_str();
    if (deviceName && deviceName[0]) {
      strncpy(newDev.name, deviceName, sizeof(newDev.name) - 1);
    } else {
      strcpy(newDev.name, "Unknown");
    }

    nyanBoxDevices.push_back(newDev);
    std::sort(nyanBoxDevices.begin(), nyanBoxDevices.end(),
              [](const NyanBoxDevice &a, const NyanBoxDevice &b) {
                return a.rssi > b.rssi;
              });
  }
};

static NyanboxAdvertisedDeviceCallbacks nyanboxDeviceCallbacks;


void nyanboxDetectorSetup() {

  nyanBoxDevices.clear();
  nyanBoxDevices.reserve(MAX_DEVICES);
  currentIndex = listStartIndex = 0;
  isDetailView = false;
  lastButtonPress = 0;
  nyanboxScanning = false;
  nyanboxCallbackCount = 0;
  lastCallbackTime = 0;

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "nyanBOX Detector");
  u8g2.drawStr(0, 20, "Initializing...");
  u8g2.sendBuffer();
  BLEDevice::init("nyanBOXDetector");
  delay(100);
  
  pNyanboxScan = BLEDevice::getScan();
  pNyanboxScan->setAdvertisedDeviceCallbacks(&nyanboxDeviceCallbacks);
  pNyanboxScan->setActiveScan(true);
  pNyanboxScan->setInterval(1000);
  pNyanboxScan->setWindow(200);
  
  try {
    pNyanboxScan->start(SCAN_DURATION / 1000, false);
    nyanboxScanning = true;
    nyanboxLastScanTime = millis();
  } catch (...) {
    nyanboxLastScanTime = millis() - SCAN_INTERVAL + 2000;
    nyanboxScanning = false;
  }

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(BTN_CENTER, INPUT_PULLUP);
}

void nyanboxDetectorLoop() {
  unsigned long now = millis();

  if (now - nyanboxLastScanTime > 10000) {
    nyanboxCallbackCount = 0;
  }

  if (!nyanboxScanning && now - nyanboxLastScanTime > SCAN_INTERVAL) {
    if (pNyanboxScan) {
      try {
        pNyanboxScan->start(SCAN_DURATION / 1000, false);
        nyanboxScanning = true;
        nyanboxLastScanTime = now;
      } catch (...) {
        nyanboxLastScanTime = now - SCAN_INTERVAL + 5000;
      }
    }
  }

  if (nyanboxScanning && now - nyanboxLastScanTime > SCAN_DURATION) {
    if (pNyanboxScan) {
      try {
        pNyanboxScan->stop();
      } catch (...) {
      }
    }
    nyanboxScanning = false;
  }

  static unsigned long lastCleanup = 0;
  if (now - lastCleanup > 60000 && nyanBoxDevices.size() >= MAX_DEVICES) {
    try {
      std::sort(nyanBoxDevices.begin(), nyanBoxDevices.end(),
                [](const NyanBoxDevice &a, const NyanBoxDevice &b) {
                  return a.lastSeen < b.lastSeen;
                });
      int devicesToRemove = MAX_DEVICES / 4;
      if (devicesToRemove > 0 && devicesToRemove <= (int)nyanBoxDevices.size()) {
        nyanBoxDevices.erase(nyanBoxDevices.begin(),
                             nyanBoxDevices.begin() + devicesToRemove);
      }
      lastCleanup = now;
    } catch (...) {
      nyanBoxDevices.clear();
      currentIndex = listStartIndex = 0;
      lastCleanup = now;
    }
  }

  if (now - lastButtonPress > debounceTime) {
    if (digitalRead(BTN_BACK) == LOW && isDetailView) {
      isDetailView = false;
      lastButtonPress = now;
    }

    if (!isDetailView && !nyanBoxDevices.empty()) {
      if (digitalRead(BTN_UP) == LOW && currentIndex > 0) {
        --currentIndex;
        if (currentIndex < listStartIndex)
          --listStartIndex;
        lastButtonPress = now;
      } else if (digitalRead(BTN_DOWN) == LOW &&
                 currentIndex < (int)nyanBoxDevices.size() - 1) {
        ++currentIndex;
        if (currentIndex >= listStartIndex + 5)
          ++listStartIndex;
        lastButtonPress = now;
      } else if (digitalRead(BTN_RIGHT) == LOW) {
        isDetailView = true;
        lastButtonPress = now;
      }
    }
  }

  if (nyanBoxDevices.empty()) {
    currentIndex = listStartIndex = 0;
    isDetailView = false;
  } else {
    currentIndex = constrain(currentIndex, 0, (int)nyanBoxDevices.size() - 1);
    listStartIndex =
        constrain(listStartIndex, 0, max(0, (int)nyanBoxDevices.size() - 5));
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  if (!nyanboxScanning && now - nyanboxLastScanTime > SCAN_INTERVAL - 1000) {
    u8g2.drawStr(0, 10, "Refreshing");
    u8g2.drawStr(0, 20, "nyanBOX Devices...");
  } else if (nyanBoxDevices.empty()) {
    u8g2.drawStr(0, 10, "Scanning for");
    u8g2.drawStr(0, 20, "nyanBOX Devices...");
    u8g2.drawStr(0, 35, "Nearby: n/a");
    u8g2.drawStr(0, 50, "Press CENTER to exit");
  } else if (isDetailView && !nyanBoxDevices.empty() && currentIndex >= 0 && currentIndex < (int)nyanBoxDevices.size()) {
    auto &dev = nyanBoxDevices[currentIndex];
    u8g2.setFont(u8g2_font_5x8_tr);
    char buf[32];

    snprintf(buf, sizeof(buf), "Name: %s", dev.name);
    u8g2.drawStr(0, 10, buf);

    snprintf(buf, sizeof(buf), "Addr: %s", dev.address);
    u8g2.drawStr(0, 20, buf);

    if (dev.level > 0) {
      snprintf(buf, sizeof(buf), "Level: %u", dev.level);
    } else {
      snprintf(buf, sizeof(buf), "Level: Unknown");
    }
    u8g2.drawStr(0, 30, buf);

    snprintf(buf, sizeof(buf), "Version: %s", dev.version);
    u8g2.drawStr(0, 40, buf);

    snprintf(buf, sizeof(buf), "RSSI: %d", dev.rssi);
    u8g2.drawStr(0, 50, buf);

    snprintf(buf, sizeof(buf), "%lus", (millis() - dev.lastSeen) / 1000);
    u8g2.drawStr(90, 60, buf);
    u8g2.drawStr(0, 60, "<- Back");
  } else {
    char header[32];
    snprintf(header, sizeof(header), "Badges: %d/%d", (int)nyanBoxDevices.size(), MAX_DEVICES);
    u8g2.drawStr(0, 10, header);

    for (int i = 0; i < 5; ++i) {
      int idx = listStartIndex + i;
      if (idx >= (int)nyanBoxDevices.size()) break;

      auto &d = nyanBoxDevices[idx];
      if (idx == currentIndex)
        u8g2.drawStr(0, 20 + i * 10, ">");

      char line[32];
      const char *nameToShow = (d.name[0]) ? d.name : "Unknown";
      if (d.level > 0) {
        snprintf(line, sizeof(line), "%.8s | L%d %d", nameToShow, d.level, d.rssi);
      } else {
        snprintf(line, sizeof(line), "%.8s | L? %d", nameToShow, d.rssi);
      }
      u8g2.drawStr(10, 20 + i * 10, line);
    }
  }

  u8g2.sendBuffer();
}