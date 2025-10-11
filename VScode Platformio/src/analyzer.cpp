/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________ */
   
#include <Arduino.h> 
#include "../include/analyzer.h"
#include "../include/sleep_manager.h"
#include "../include/setting.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern Adafruit_NeoPixel pixels;

#define NRF24_CONFIG      0x00
#define NRF24_EN_AA       0x01
#define NRF24_RF_CH       0x05
#define NRF24_RF_SETUP    0x06
#define NRF24_RPD         0x09

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define CHANNELS 128

uint8_t spectrum[CHANNELS];
uint8_t peakSignal = 0;
uint8_t peakChannel = 0;
uint8_t avgSignal = 0;

uint8_t viewMode = 0;

#include "../include/pindefs.h"

#define CE1  RADIO_CE_PIN_1
#define CSN1 RADIO_CSN_PIN_1
#define CE2  RADIO_CE_PIN_2
#define CSN2 RADIO_CSN_PIN_2
#define CE3  RADIO_CE_PIN_3
#define CSN3 RADIO_CSN_PIN_3


void writeRegister(uint8_t csn, uint8_t reg, uint8_t value) {
    digitalWrite(csn, LOW);
    SPI.transfer(reg | 0x20);
    SPI.transfer(value);
    digitalWrite(csn, HIGH);
}

uint8_t readRegister(uint8_t csn, uint8_t reg) {
    digitalWrite(csn, LOW);
    SPI.transfer(reg & 0x1F);
    uint8_t result = SPI.transfer(0x00);
    digitalWrite(csn, HIGH);
    return result;
}

void setChannel(uint8_t csn, uint8_t channel) {
    writeRegister(csn, NRF24_RF_CH, channel);
}

void powerUP(uint8_t csn) {
    uint8_t config = readRegister(csn, NRF24_CONFIG);
    writeRegister(csn, NRF24_CONFIG, config | 0x02);
    delayMicroseconds(130);
}

void powerDOWN(uint8_t csn) {
    uint8_t config = readRegister(csn, NRF24_CONFIG);
    writeRegister(csn, NRF24_CONFIG, config & ~0x02);
}

void startListening(uint8_t ce, uint8_t csn) {
    uint8_t config = readRegister(csn, NRF24_CONFIG);
    writeRegister(csn, NRF24_CONFIG, config | 0x01);
    digitalWrite(ce, HIGH);
}

void stopListening(uint8_t ce) {
    digitalWrite(ce, LOW);
}

bool carrierDetected(uint8_t csn) {
    return readRegister(csn, NRF24_RPD) & 0x01;
}

void renderSpectrum();

void analyzerSetup(){

    Serial.begin(115200);

    esp_bt_controller_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();

    pinMode(CE1, OUTPUT);
    pinMode(CSN1, OUTPUT);
    pinMode(CE2, OUTPUT);
    pinMode(CSN2, OUTPUT);
    pinMode(CE3, OUTPUT);
    pinMode(CSN3, OUTPUT);

    SPI.begin(18, 19, 23, 17);
    SPI.setDataMode(SPI_MODE0);
    SPI.setFrequency(10000000);
    SPI.setBitOrder(MSBFIRST);

    digitalWrite(CSN1, HIGH);
    digitalWrite(CE1, LOW);
    digitalWrite(CSN2, HIGH);
    digitalWrite(CE2, LOW);
    digitalWrite(CSN3, HIGH);
    digitalWrite(CE3, LOW);

    powerUP(CSN1);
    writeRegister(CSN1, NRF24_EN_AA, 0x00);
    writeRegister(CSN1, NRF24_RF_SETUP, 0x0F);

    powerUP(CSN2);
    writeRegister(CSN2, NRF24_EN_AA, 0x00);
    writeRegister(CSN2, NRF24_RF_SETUP, 0x0F);

    powerUP(CSN3);
    writeRegister(CSN3, NRF24_EN_AA, 0x00);
    writeRegister(CSN3, NRF24_RF_SETUP, 0x0F);

}

void analyzerLoop(){

    static bool leftPressed = false;
    static bool rightPressed = false;
    bool leftNow = digitalRead(BUTTON_PIN_LEFT) == LOW;
    bool rightNow = digitalRead(BUTTON_PIN_RIGHT) == LOW;

    if ((leftNow && !leftPressed) || (rightNow && !rightPressed)) {
        viewMode = 1 - viewMode;
        delay(200);
    }
    leftPressed = leftNow;
    rightPressed = rightNow;

    memset(spectrum, 0, sizeof(spectrum));

    int sweeps = 50;
    for (int sweep = 0; sweep < sweeps; sweep++) {
        for (int ch = 0; ch < CHANNELS; ch += 3) {
            setChannel(CSN1, ch);
            setChannel(CSN2, ch + 1);
            setChannel(CSN3, ch + 2);

            startListening(CE1, CSN1);
            startListening(CE2, CSN2);
            startListening(CE3, CSN3);

            delayMicroseconds(128);

            stopListening(CE1);
            stopListening(CE2);
            stopListening(CE3);

            if (carrierDetected(CSN1)) spectrum[ch]++;
            if (carrierDetected(CSN2)) spectrum[ch + 1]++;
            if (carrierDetected(CSN3)) spectrum[ch + 2]++;
        }
    }

    peakSignal = 0;
    peakChannel = 0;
    uint16_t signalSum = 0;
    for (int i = 0; i < CHANNELS; i++) {
        if (spectrum[i] > peakSignal) {
            peakSignal = spectrum[i];
            peakChannel = i;
        }
        signalSum += spectrum[i];
    }
    avgSignal = signalSum / CHANNELS;

    renderSpectrum();
}

void renderSpectrum() {
    u8g2.clearBuffer();

    const int DISPLAY_TOP = 10;
    const int DISPLAY_BOTTOM = 54;
    const int DISPLAY_HEIGHT = DISPLAY_BOTTOM - DISPLAY_TOP;

    uint8_t scaleMax = peakSignal;
    if (scaleMax < 10) scaleMax = 10;

    for (int ch = 0; ch < CHANNELS; ch++) {
        if (spectrum[ch] > 0) {
            int barHeight = (spectrum[ch] * DISPLAY_HEIGHT) / scaleMax;

            if (barHeight > DISPLAY_HEIGHT) barHeight = DISPLAY_HEIGHT;
            if (barHeight < 1) barHeight = 1;

            int barTop = DISPLAY_BOTTOM - barHeight;
            u8g2.drawVLine(ch, barTop, barHeight);
        }
    }

    u8g2.setFont(u8g2_font_5x7_tr);

    if (viewMode == 0) {
        u8g2.drawStr(0, 7, "<CH>");
    } else {
        u8g2.drawStr(0, 7, "<MHz>");
    }

    u8g2.setCursor(42, 7);
    u8g2.print("LVL:");
    u8g2.print(peakSignal);

    if (viewMode == 0) {
        u8g2.print(" CH:");
        u8g2.print(peakChannel);
    } else {
        uint16_t peakFreq = 2400 + peakChannel;
        u8g2.print(" @");
        u8g2.print(peakFreq);
    }

    if (peakSignal > 30) {
        u8g2.drawStr(110, 7, "HI");
    } else if (peakSignal > 10) {
        u8g2.drawStr(110, 7, "MD");
    } else {
        u8g2.drawStr(110, 7, "LO");
    }

    u8g2.drawHLine(0, DISPLAY_BOTTOM, 128);

    if (viewMode == 0) {
        u8g2.drawStr(0, 62, "0");
        u8g2.drawStr(28, 62, "32");
        u8g2.drawStr(56, 62, "64");
        u8g2.drawStr(84, 62, "96");
        u8g2.drawStr(108, 62, "127");
    } else {
        u8g2.drawStr(0, 62, "2400");
        u8g2.drawStr(50, 62, "2450");
        u8g2.drawStr(98, 62, "2527");
    }

    u8g2.sendBuffer();
}
