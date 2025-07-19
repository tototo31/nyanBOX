/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________ */

#include "../include/jammer.h"
#include "../include/pindefs.h"
#include "esp_wifi.h"
#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>
#include <U8g2lib.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// Button pins
#define CHANNEL_BTN BUTTON_PIN_DOWN
#define JAM_BTN BUTTON_PIN_UP
#define RATE_BTN BUTTON_PIN_RIGHT
#define PA_BTN BUTTON_PIN_LEFT

// Radio pins
#define CE_A RADIO_CE_PIN_1
#define CSN_A RADIO_CSN_PIN_1
#define CE_B RADIO_CE_PIN_2
#define CSN_B RADIO_CSN_PIN_2
#define CE_C RADIO_CE_PIN_3
#define CSN_C RADIO_CSN_PIN_3

RF24 *radioA = nullptr;
RF24 *radioB = nullptr;
RF24 *radioC = nullptr;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

const int num_channels = 64;
int value[num_channels];
int valuesDisplay[32];
int channels = 1;
const int num_reps = 50;
bool jamming = false;
const byte address[6] = "00001";

int wlanchannels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
int wifi_channels[] = {1,  2,  3,  4, 5, 6, 7, 8, 9, 10, 11,
                       12, 13, 14, 1, 2, 3, 4, 5, 6, 7};
byte wifiGroup1[] = {1, 2, 3, 4};
byte wifiGroup2[] = {5, 6, 7, 8};
byte wifiGroup3[] = {9, 10, 11, 12};

uint8_t dataRateIndex = 0;
uint8_t paLevelIndex = 0;

static bool chPrev = HIGH;
static unsigned long chLast = 0;
static bool jamPrev = HIGH;
static unsigned long jamLast = 0;
static bool ratePrev = HIGH;
static unsigned long rateLast = 0;
static bool paPrev = HIGH;
static unsigned long paLast = 0;
const unsigned long debounce = 200;

void setRadioParameters() {
  if (!radioA || !radioB || !radioC)
    return;

  switch (dataRateIndex) {
  case 0:
    radioA->setDataRate(RF24_250KBPS);
    radioB->setDataRate(RF24_250KBPS);
    radioC->setDataRate(RF24_250KBPS);
    break;
  case 1:
    radioA->setDataRate(RF24_1MBPS);
    radioB->setDataRate(RF24_1MBPS);
    radioC->setDataRate(RF24_1MBPS);
    break;
  case 2:
    radioA->setDataRate(RF24_2MBPS);
    radioB->setDataRate(RF24_2MBPS);
    radioC->setDataRate(RF24_2MBPS);
    break;
  }
  switch (paLevelIndex) {
  case 0:
    radioA->setPALevel(RF24_PA_MIN);
    radioB->setPALevel(RF24_PA_MIN);
    radioC->setPALevel(RF24_PA_MIN);
    break;
  case 1:
    radioA->setPALevel(RF24_PA_LOW);
    radioB->setPALevel(RF24_PA_LOW);
    radioC->setPALevel(RF24_PA_LOW);
    break;
  case 2:
    radioA->setPALevel(RF24_PA_HIGH);
    radioB->setPALevel(RF24_PA_HIGH);
    radioC->setPALevel(RF24_PA_HIGH);
    break;
  case 3:
    radioA->setPALevel(RF24_PA_MAX);
    radioB->setPALevel(RF24_PA_MAX);
    radioC->setPALevel(RF24_PA_MAX);
    break;
  }
}

void radioSetChannel(int ch) {
  if (!radioA || !radioB || !radioC)
    return;
  radioA->setChannel(ch);
  radioB->setChannel(ch);
  radioC->setChannel(ch);
}

void jammer() {
  if (!radioA || !radioB || !radioC)
    return;

  const char text[] = {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55,
                       0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};
  for (int i = ((channels * 5) + 1); i < ((channels * 5) + 23); i++) {
    radioSetChannel(i);
    radioA->write(&text, sizeof(text));
    radioB->write(&text, sizeof(text));
    radioC->write(&text, sizeof(text));
  }
}

void pressChannel() {
  if (++channels > 14)
    channels = 1;
}

void pressJam() { jamming = !jamming; }

void pressRate() {
  dataRateIndex = (dataRateIndex + 1) % 3;
  setRadioParameters();
}

void pressPa() {
  paLevelIndex = (paLevelIndex + 1) % 4;
  setRadioParameters();
}

void configure(RF24 &radio) {
  radio.begin();
  radio.setAutoAck(false);
  radio.disableDynamicPayloads();
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.printPrettyDetails();
}

void jammerCleanup() {
  if (radioA) {
    delete radioA;
    radioA = nullptr;
  }
  if (radioB) {
    delete radioB;
    radioB = nullptr;
  }
  if (radioC) {
    delete radioC;
    radioC = nullptr;
  }
}

void jammerSetup() {
  jammerCleanup();
  Serial.begin(115200);
  esp_wifi_stop();
  esp_wifi_disconnect();
  pinMode(CHANNEL_BTN, INPUT_PULLUP);
  pinMode(JAM_BTN, INPUT_PULLUP);
  pinMode(RATE_BTN, INPUT_PULLUP);
  pinMode(PA_BTN, INPUT_PULLUP);
  SPI.begin();
  pinMode(CE_A, OUTPUT);
  pinMode(CSN_A, OUTPUT);
  pinMode(CE_B, OUTPUT);
  pinMode(CSN_B, OUTPUT);
  pinMode(CE_C, OUTPUT);
  pinMode(CSN_C, OUTPUT);
  radioA = new RF24(CE_A, CSN_A, 16000000);
  radioB = new RF24(CE_B, CSN_B, 16000000);
  radioC = new RF24(CE_C, CSN_C, 16000000);
  if (radioA && radioB && radioC) {
    configure(*radioA);
    configure(*radioB);
    configure(*radioC);
    setRadioParameters();
  } else {
    Serial.println("Failed to initialize one or more radio modules!");
  }
}

void jammerLoop() {
  if (!radioA || !radioB || !radioC) {
    jammerSetup();
    if (!radioA || !radioB || !radioC) {
      delay(1000);
      return;
    }
  }

  unsigned long now = millis();
  bool vCh = digitalRead(CHANNEL_BTN);
  if (!vCh && chPrev && now - chLast > debounce) {
    pressChannel();
    chLast = now;
  }
  chPrev = vCh;

  bool vJam = digitalRead(JAM_BTN);
  if (!vJam && jamPrev && now - jamLast > debounce) {
    pressJam();
    jamLast = now;
  }
  jamPrev = vJam;

  bool vRate = digitalRead(RATE_BTN);
  if (!vRate && ratePrev && now - rateLast > debounce) {
    pressRate();
    rateLast = now;
  }
  ratePrev = vRate;

  bool vPa = digitalRead(PA_BTN);
  if (!vPa && paPrev && now - paLast > debounce) {
    pressPa();
    paLast = now;
  }
  paPrev = vPa;

  u8g2.clearBuffer();
  
  u8g2.setFont(u8g2_font_helvB10_tr);
  const char* title = "WLAN Jammer";
  int titleWidth = u8g2.getUTF8Width(title);
  u8g2.drawStr((128 - titleWidth) / 2, 12, title);
  u8g2.drawHLine(16, 15, 96);
  
  u8g2.setFont(u8g2_font_helvB08_tr);
  if (jamming) {
    const char* status = "ACTIVE";
    int statusWidth = u8g2.getUTF8Width(status);
    u8g2.drawStr((128 - statusWidth) / 2, 26, status);
    setNeoPixelColour("red");
    jammer();
  } else {
    const char* status = "STOPPED";
    int statusWidth = u8g2.getUTF8Width(status);
    u8g2.drawStr((128 - statusWidth) / 2, 26, status);
    setNeoPixelColour("0");
  }
  
  u8g2.setFont(u8g2_font_helvR08_tr);
  char settingsText[48];
  const char* rateLabels[] = {"250K", "1M", "2M"};
  const char* powerLabels[] = {"MIN", "LOW", "HI", "MAX"};
  
  snprintf(settingsText, sizeof(settingsText), "Ch %d | %s | %s", channels, rateLabels[dataRateIndex], powerLabels[paLevelIndex]);
  int settingsWidth = u8g2.getUTF8Width(settingsText);
  u8g2.drawStr((128 - settingsWidth) / 2, 36, settingsText);
  
  u8g2.setFont(u8g2_font_4x6_tr);
  
  const char* line1 = "UP=START/STOP  DOWN=CH";
  const char* line2 = "R=RATE  L=PWR  SEL=EXIT";
  
  int line1Width = u8g2.getUTF8Width(line1);
  int line2Width = u8g2.getUTF8Width(line2);
  
  u8g2.drawStr((128 - line1Width) / 2, 54, line1);
  u8g2.drawStr((128 - line2Width) / 2, 62, line2);
  
  u8g2.sendBuffer();
}