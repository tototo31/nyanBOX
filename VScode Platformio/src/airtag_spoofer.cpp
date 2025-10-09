/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#include "../include/airtag_spoofer.h"
#include "../include/airtag_detector.h"
#include "../include/sleep_manager.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern std::vector<AirTagDeviceData> airtagDevices;

#define BTN_UP BUTTON_PIN_UP
#define BTN_DOWN BUTTON_PIN_DOWN
#define BTN_RIGHT BUTTON_PIN_RIGHT
#define BTN_BACK BUTTON_PIN_LEFT
#define BTN_CENTER BUTTON_PIN_CENTER

enum AirTagSpooferState {
  SPOOFER_MENU,
  SPOOFER_CLONE_SELECT,
  SPOOFER_CLONE_RUNNING,
  SPOOFER_CLONE_ALL_RUNNING
};

static AirTagSpooferState currentState = SPOOFER_MENU;
static BLEAdvertising *pAdvertising;
static BLEServer *pServer;

static int menuSelection = 0;
static int cloneTargetIndex = 0;

static bool isAdvertising = false;
static unsigned long lastAdvertiseTime = 0;
static int currentCloneAllIndex = 0;
static unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200;
const unsigned long advertiseInterval = 10;

void drawMainMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 12, "AirTag Spoofer");

  if (airtagDevices.empty()) {
    u8g2.drawStr(0, 28, "No AirTags found!");
    u8g2.drawStr(0, 44, "Run Detector first");
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 62, "SEL=Exit");
  } else {
    const char* menuItems[] = {
      "Clone Target",
      "Clone All (Spam)"
    };

    for (int i = 0; i < 2; i++) {
      char itemStr[32];
      bool selected = (menuSelection == i);
      snprintf(itemStr, sizeof(itemStr), "%s %s",
              selected ? ">" : " ", menuItems[i]);
      u8g2.drawStr(0, 28 + i * 16, itemStr);
    }

    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 62, "U/D=NAV R=OK SEL=Exit");
  }

  u8g2.sendBuffer();
}

void drawCloneSelect() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  char headerStr[32];
  snprintf(headerStr, sizeof(headerStr), "Select Target %d/%d",
           cloneTargetIndex + 1, (int)airtagDevices.size());
  u8g2.drawStr(0, 12, headerStr);

  auto &target = airtagDevices[cloneTargetIndex];
  char targetInfo[32];
  snprintf(targetInfo, sizeof(targetInfo), "%.14s", target.name);
  u8g2.drawStr(0, 28, targetInfo);

  char addrInfo[32];
  snprintf(addrInfo, sizeof(addrInfo), "%.17s", target.address);
  u8g2.drawStr(0, 44, addrInfo);

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 62, "U/D=Scroll R=Start L=Back SEL=Exit");

  u8g2.sendBuffer();
}

void drawCloneRunning() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 12, "Cloning Target");

  auto &target = airtagDevices[cloneTargetIndex];

  char nameStr[20];
  snprintf(nameStr, sizeof(nameStr), "%.14s", target.name);
  u8g2.drawStr(0, 28, nameStr);

  char addrStr[20];
  snprintf(addrStr, sizeof(addrStr), "%.17s", target.address);
  u8g2.drawStr(0, 44, addrStr);

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 62, "L=Back SEL=Exit");

  u8g2.sendBuffer();
}

void drawCloneAllRunning() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 12, "Clone All Spam");

  char countStr[32];
  snprintf(countStr, sizeof(countStr), "Targets: %d", (int)airtagDevices.size());
  u8g2.drawStr(0, 28, countStr);

  u8g2.drawStr(0, 44, "Advertising...");

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 62, "L=Back SEL=Exit");

  u8g2.sendBuffer();
}

void initializeAdvertising() {
  if (!pServer) {
    BLEDevice::init("AirTagSpoofer");
    pServer = BLEDevice::createServer();
    pAdvertising = pServer->getAdvertising();
    pAdvertising->setAdvertisementType(ADV_TYPE_NONCONN_IND);
  }
}

void startSingleClone() {
  if (airtagDevices.empty() || isAdvertising) return;

  initializeAdvertising();

  auto &device = airtagDevices[cloneTargetIndex];

  esp_bd_addr_t airtagAddr;

  sscanf(device.address, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
         &airtagAddr[0], &airtagAddr[1], &airtagAddr[2],
         &airtagAddr[3], &airtagAddr[4], &airtagAddr[5]);

  pAdvertising->setDeviceAddress(airtagAddr, BLE_ADDR_TYPE_RANDOM);

  BLEAdvertisementData advData;

  std::string payloadData((char*)device.payload, device.payloadLength);
  advData.addData(payloadData);

  pAdvertising->setAdvertisementData(advData);
  pAdvertising->start();

  isAdvertising = true;
  lastAdvertiseTime = millis();
}

void startCloneAllSpam() {
  if (airtagDevices.empty()) return;

  currentCloneAllIndex = 0;
  cloneTargetIndex = 0;
  startSingleClone();
}

void stopAdvertising() {
  if (!isAdvertising) return;
  pAdvertising->stop();
  delay(5);
  isAdvertising = false;
}

void airtagSpooferSetup() {
  menuSelection = 0;
  cloneTargetIndex = 0;
  currentCloneAllIndex = 0;
  isAdvertising = false;
  lastButtonPress = 0;
  currentState = SPOOFER_MENU;
  pServer = nullptr;
  pAdvertising = nullptr;

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(BTN_CENTER, INPUT_PULLUP);

  randomSeed((uint32_t)esp_random());
}

void airtagSpooferLoop() {
  unsigned long now = millis();

  static bool upPressed = false, downPressed = false;
  static bool rightPressed = false, leftPressed = false;
  bool upNow = digitalRead(BTN_UP) == LOW;
  bool downNow = digitalRead(BTN_DOWN) == LOW;
  bool rightNow = digitalRead(BTN_RIGHT) == LOW;
  bool leftNow = digitalRead(BTN_BACK) == LOW;
  bool centerNow = digitalRead(BTN_CENTER) == LOW;


  switch (currentState) {
    case SPOOFER_MENU:
      if (!airtagDevices.empty()) {
        if (upNow && !upPressed) {
          menuSelection = (menuSelection - 1 + 2) % 2;
          delay(200);
        }
        if (downNow && !downPressed) {
          menuSelection = (menuSelection + 1) % 2;
          delay(200);
        }
        if (rightNow && !rightPressed) {
          switch (menuSelection) {
            case 0:
              currentState = SPOOFER_CLONE_SELECT;
              break;
            case 1:
              startCloneAllSpam();
              currentState = SPOOFER_CLONE_ALL_RUNNING;
              break;
          }
          delay(200);
        }
      }
      if (centerNow) {
        delay(200);
        return;
      }

      drawMainMenu();
      break;

    case SPOOFER_CLONE_SELECT:
      if (upNow && !upPressed) {
        cloneTargetIndex = (cloneTargetIndex - 1 + airtagDevices.size()) % airtagDevices.size();
        delay(200);
      }
      if (downNow && !downPressed) {
        cloneTargetIndex = (cloneTargetIndex + 1) % airtagDevices.size();
        delay(200);
      }
      if (rightNow && !rightPressed) {
        startSingleClone();
        currentState = SPOOFER_CLONE_RUNNING;
        delay(200);
      }
      if (leftNow && !leftPressed) {
        currentState = SPOOFER_MENU;
        delay(200);
      }
      if (centerNow) {
        delay(200);
        return;
      }

      drawCloneSelect();
      break;

    case SPOOFER_CLONE_RUNNING:
      if (leftNow && !leftPressed) {
        stopAdvertising();
        currentState = SPOOFER_CLONE_SELECT;
        delay(200);
      }
      if (centerNow) {
        stopAdvertising();
        delay(200);
        return;
      }

      if (isAdvertising && now - lastAdvertiseTime > 100) {
        stopAdvertising();
        delay(5);
        startSingleClone();
      }

      drawCloneRunning();
      break;

    case SPOOFER_CLONE_ALL_RUNNING:
      if (leftNow && !leftPressed) {
        stopAdvertising();
        currentState = SPOOFER_MENU;
        delay(200);
      }
      if (centerNow) {
        stopAdvertising();
        delay(200);
        return;
      }

      if (isAdvertising && now - lastAdvertiseTime > advertiseInterval) {
        stopAdvertising();
        delay(5);

        currentCloneAllIndex = (currentCloneAllIndex + 1) % airtagDevices.size();
        cloneTargetIndex = currentCloneAllIndex;
        startSingleClone();
      }

      drawCloneAllRunning();
      break;
  }

  upPressed = upNow;
  downPressed = downNow;
  rightPressed = rightNow;
  leftPressed = leftNow;

  delay(5);
}