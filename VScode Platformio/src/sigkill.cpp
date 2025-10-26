/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________ */

#include <Arduino.h>
#include "../include/sigkill.h"
#include "../include/sleep_manager.h"
#include "../include/icon.h"
#include "../include/pindefs.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

RF24 radio_1(RADIO_CE_PIN_1, RADIO_CSN_PIN_1, 16000000);
RF24 radio_2(RADIO_CE_PIN_2, RADIO_CSN_PIN_2, 16000000);
RF24 radio_3(RADIO_CE_PIN_3, RADIO_CSN_PIN_3, 16000000);

enum SigKillMode { SIG_MENU, SIG_JAMMING };
enum ProtocolType { ALL, WIFI, VIDEO_TX, RC, BLE, BLUETOOTH, USB_WIRELESS, ZIGBEE, NRF24 };

static SigKillMode currentMode = SIG_MENU;
static ProtocolType selectedProtocol = WIFI;
static int menuSelection = 0;
static unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 200;

// Channel groups for nRF24 radios
byte channelGroup_1[] = {2, 5, 8, 11};
byte channelGroup_2[] = {26, 29, 32, 35};
byte channelGroup_3[] = {80, 83, 86, 89};

// Protocol channel definitions
const byte bluetooth_channels[]        = {32,34,46,48,50,52,0,1,2,4,6,8,22,24,26,28,30,74,76,78,80};
const byte ble_channels[]              = {2,26,80};
const byte wifi_channels[]             = {1,2,3,4,5,6,7,8,9,10,11,12};
const byte usbWireless_channels[]      = {40,50,60};
const byte videoTransmitter_channels[] = {70,75,80};
const byte rc_channels[]               = {1,3,5,7};
const byte zigbee_channels[]           = {11,15,20,25};
const byte nrf24_channels[]            = {76,78,79};

const char* protocolNames[] = {
  "All", "WiFi", "Video TX", "RC", "BLE", "Bluetooth",
  "USB Wireless", "Zigbee", "NRF24"
};

void configureRadio(RF24 &radio, const byte *channels, size_t size) {
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.printPrettyDetails();
  for (size_t i = 0; i < size; i++) {
    radio.setChannel(channels[i]);
    radio.startConstCarrier(RF24_PA_MAX, channels[i]);
  }
}

void initializeRadios() {
  if (radio_1.begin()) configureRadio(radio_1, channelGroup_1, sizeof(channelGroup_1));
  if (radio_2.begin()) configureRadio(radio_2, channelGroup_2, sizeof(channelGroup_2));
  if (radio_3.begin()) configureRadio(radio_3, channelGroup_3, sizeof(channelGroup_3));
}

void powerDownRadios() {
  radio_1.powerDown();
  radio_2.powerDown();
  radio_3.powerDown();
  delay(100);
}

static void drawSigMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "SigKill Mode:");

  int start = (menuSelection / 4) * 4;

  for (int i = 0; i < 4 && (start + i) < 9; i++) {
    int idx = start + i;
    char line[24];
    snprintf(line, sizeof(line), "%s %s",
             (idx == menuSelection) ? ">" : " ",
             protocolNames[idx]);
    u8g2.drawStr(0, 22 + (i * 10), line);
  }

  int scrollbarX = 122;
  int scrollbarWidth = 4;
  int scrollbarY = 14;
  int scrollbarHeight = 34;

  u8g2.drawFrame(scrollbarX, scrollbarY, scrollbarWidth, scrollbarHeight);

  int thumbHeight = (scrollbarHeight * 4) / 9;
  int maxThumbTravel = scrollbarHeight - thumbHeight - 2;
  int thumbY = scrollbarY + 1 + ((menuSelection * maxThumbTravel) / 8);

  u8g2.drawBox(scrollbarX + 1, thumbY, scrollbarWidth - 2, thumbHeight);

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 62, "U/D=Move R=Start SEL=Exit");
  u8g2.sendBuffer();
}

static void drawActiveJamming(const char* protocolName) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  char title[32];
  snprintf(title, sizeof(title), "%s Jamming", protocolName);
  u8g2.drawStr(0, 12, title);
  u8g2.drawStr(0, 28, "Status: Active");
  u8g2.setFont(u8g2_font_5x8_tr);
  char radioStatus[32];
  snprintf(radioStatus, sizeof(radioStatus), "R1:%s R2:%s R3:%s",
           radio_1.isChipConnected() ? "ON" : "OFF",
           radio_2.isChipConnected() ? "ON" : "OFF",
           radio_3.isChipConnected() ? "ON" : "OFF");
  u8g2.drawStr(0, 42, radioStatus);
  u8g2.drawStr(0, 62, "L=Back SEL=Exit");
  u8g2.sendBuffer();
}

void sigkillSetup() {
  Serial.begin(115200);

  esp_wifi_stop();
  esp_wifi_disconnect();

  pinMode(BUTTON_PIN_UP, INPUT_PULLUP);
  pinMode(BUTTON_PIN_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_PIN_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_PIN_LEFT, INPUT_PULLUP);

  currentMode = SIG_MENU;
  menuSelection = 0;
  selectedProtocol = ALL;
  powerDownRadios();
  drawSigMenu();
}

void sigkillLoop() {
  unsigned long now = millis();
  static SigKillMode previousMode = SIG_MENU;

  bool up = digitalRead(BUTTON_PIN_UP) == LOW;
  bool down = digitalRead(BUTTON_PIN_DOWN) == LOW;
  bool left = digitalRead(BUTTON_PIN_LEFT) == LOW;
  bool right = digitalRead(BUTTON_PIN_RIGHT) == LOW;

  switch (currentMode) {
    case SIG_MENU:
      drawSigMenu();

      if (now - lastButtonPress > debounceDelay) {
        if (up) {
          menuSelection = (menuSelection - 1 + 9) % 9;
          lastButtonPress = now;
        } else if (down) {
          menuSelection = (menuSelection + 1) % 9;
          lastButtonPress = now;
        } else if (right) {
          selectedProtocol = (ProtocolType)menuSelection;
          currentMode = SIG_JAMMING;
          initializeRadios();
          drawActiveJamming(protocolNames[selectedProtocol]);
          lastButtonPress = now;
        }
      }
      break;

    case SIG_JAMMING:
      if (currentMode != previousMode) {
        drawActiveJamming(protocolNames[selectedProtocol]);
        previousMode = currentMode;
      }

      {
        int randomIndex;
        int channel;

        switch (selectedProtocol) {
          case ALL:
            {
              static int protocolIndex = 0;
              const byte* channelArray;
              int arraySize;

              switch (protocolIndex % 8) {
                case 0: channelArray = wifi_channels; arraySize = sizeof(wifi_channels); break;
                case 1: channelArray = ble_channels; arraySize = sizeof(ble_channels); break;
                case 2: channelArray = bluetooth_channels; arraySize = sizeof(bluetooth_channels); break;
                case 3: channelArray = videoTransmitter_channels; arraySize = sizeof(videoTransmitter_channels); break;
                case 4: channelArray = rc_channels; arraySize = sizeof(rc_channels); break;
                case 5: channelArray = usbWireless_channels; arraySize = sizeof(usbWireless_channels); break;
                case 6: channelArray = zigbee_channels; arraySize = sizeof(zigbee_channels); break;
                case 7: channelArray = nrf24_channels; arraySize = sizeof(nrf24_channels); break;
              }

              randomIndex = random(0, arraySize / sizeof(byte));
              channel = channelArray[randomIndex];
              radio_1.setChannel(channel);
              radio_2.setChannel(channel);
              radio_3.setChannel(channel);

              protocolIndex++;
            }
            break;

          case BLE:
            randomIndex = random(0, sizeof(ble_channels) / sizeof(ble_channels[0]));
            channel = ble_channels[randomIndex];
            radio_1.setChannel(channel);
            radio_2.setChannel(channel);
            radio_3.setChannel(channel);
            break;

          case BLUETOOTH:
            randomIndex = random(0, sizeof(bluetooth_channels) / sizeof(bluetooth_channels[0]));
            channel = bluetooth_channels[randomIndex];
            radio_1.setChannel(channel);
            radio_2.setChannel(channel);
            radio_3.setChannel(channel);
            break;

          case WIFI:
            randomIndex = random(0, sizeof(wifi_channels) / sizeof(wifi_channels[0]));
            channel = wifi_channels[randomIndex];
            radio_1.setChannel(channel);
            radio_2.setChannel(channel);
            radio_3.setChannel(channel);
            break;

          case USB_WIRELESS:
            randomIndex = random(0, sizeof(usbWireless_channels) / sizeof(usbWireless_channels[0]));
            channel = usbWireless_channels[randomIndex];
            radio_1.setChannel(channel);
            radio_2.setChannel(channel);
            radio_3.setChannel(channel);
            break;

          case VIDEO_TX:
            randomIndex = random(0, sizeof(videoTransmitter_channels) / sizeof(videoTransmitter_channels[0]));
            channel = videoTransmitter_channels[randomIndex];
            radio_1.setChannel(channel);
            radio_2.setChannel(channel);
            radio_3.setChannel(channel);
            break;

          case RC:
            randomIndex = random(0, sizeof(rc_channels) / sizeof(rc_channels[0]));
            channel = rc_channels[randomIndex];
            radio_1.setChannel(channel);
            radio_2.setChannel(channel);
            radio_3.setChannel(channel);
            break;

          case ZIGBEE:
            randomIndex = random(0, sizeof(zigbee_channels) / sizeof(zigbee_channels[0]));
            channel = zigbee_channels[randomIndex];
            radio_1.setChannel(channel);
            radio_2.setChannel(channel);
            radio_3.setChannel(channel);
            break;

          case NRF24:
            randomIndex = random(0, sizeof(nrf24_channels) / sizeof(nrf24_channels[0]));
            channel = nrf24_channels[randomIndex];
            radio_1.setChannel(channel);
            radio_2.setChannel(channel);
            radio_3.setChannel(channel);
            break;
        }
      }

      if (left && now - lastButtonPress > debounceDelay) {
        powerDownRadios();
        currentMode = SIG_MENU;
        previousMode = SIG_JAMMING;
        lastButtonPress = now;
      }
      break;
  }
}