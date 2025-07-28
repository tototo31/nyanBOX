/*
   ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#include "../include/nyanbox_advertiser.h"
#include "../include/nyanbox_common.h"

static BLEAdvertising *pAdvertiserAdvertising = nullptr;
static bool advertiserActive = false;
static bool advertiserEnabled = false;

String generateAdvertiserDeviceName() {
  uint64_t chipid = ESP.getEfuseMac();
  uint16_t chip = (uint16_t)(chipid >> 32);
  char deviceName[32];
  snprintf(deviceName, sizeof(deviceName), "nyanBOX-%04X", chip);
  return String(deviceName);
}

uint32_t parseAdvertiserVersionToNumber(const char *versionStr) {
  if (!versionStr || versionStr[0] != 'v')
    return 0;

  int major = 0, minor = 0, patch = 0;
  sscanf(versionStr + 1, "%d.%d.%d", &major, &minor, &patch);

  return (major * 10000) + (minor * 100) + patch;
}

void createManufacturerData(char *manufData) {
  manufData[0] = 0xFF;
  manufData[1] = 0xFF;
  uint16_t level = getCurrentLevel();
  uint32_t version = parseAdvertiserVersionToNumber(NYANBOX_VERSION);
  manufData[2] = (level >> 8) & 0xFF;
  manufData[3] = level & 0xFF;
  manufData[4] = (version >> 24) & 0xFF;
  manufData[5] = (version >> 16) & 0xFF;
  manufData[6] = (version >> 8) & 0xFF;
  manufData[7] = version & 0xFF;
}

void initNyanboxAdvertiser() {
  advertiserActive = false;
  advertiserEnabled = false;
  pAdvertiserAdvertising = nullptr;
}

void startNyanboxAdvertiser() {
  if (advertiserEnabled) return;
  
  advertiserEnabled = true;
  
  String deviceName = generateAdvertiserDeviceName();
  
  if (!BLEDevice::getInitialized()) {
    BLEDevice::init(deviceName.c_str());
  }
  
  pAdvertiserAdvertising = BLEDevice::getAdvertising();
  pAdvertiserAdvertising->addServiceUUID(NYANBOX_SERVICE_UUID);

  char manufData[8];
  createManufacturerData(manufData);

  BLEAdvertisementData scanResponseData;
  scanResponseData.setName(deviceName.c_str());
  scanResponseData.setManufacturerData(std::string(manufData, 8));

  pAdvertiserAdvertising->setScanResponseData(scanResponseData);
  pAdvertiserAdvertising->setScanResponse(true);
  pAdvertiserAdvertising->start();
  advertiserActive = true;
}

void stopNyanboxAdvertiser() {
  advertiserEnabled = false;
  
  if (advertiserActive) {
    esp_ble_gap_stop_advertising();
    BLEDevice::deinit();
    advertiserActive = false;
  }
}

void updateNyanboxAdvertiser() {
  if (!advertiserEnabled || advertiserActive) return;
  
  if (!advertiserActive) {
    String deviceName = generateAdvertiserDeviceName();
    
    if (!BLEDevice::getInitialized()) {
      BLEDevice::init(deviceName.c_str());
    }
    
    pAdvertiserAdvertising = BLEDevice::getAdvertising();
    pAdvertiserAdvertising->addServiceUUID(NYANBOX_SERVICE_UUID);

    char manufData[8];
    createManufacturerData(manufData);

    BLEAdvertisementData scanResponseData;
    scanResponseData.setName(deviceName.c_str());
    scanResponseData.setManufacturerData(std::string(manufData, 8));

    pAdvertiserAdvertising->setScanResponseData(scanResponseData);
    pAdvertiserAdvertising->setScanResponse(true);
    pAdvertiserAdvertising->start();
    advertiserActive = true;
  }
}

bool isNyanboxAdvertising() {
  return advertiserActive;
}