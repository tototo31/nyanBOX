/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#ifndef CHANNEL_MONITOR_H
#define CHANNEL_MONITOR_H

#include <WiFi.h>
#include "esp_wifi.h"
#include <U8g2lib.h>
#include <Arduino.h>
#include "pindefs.h"

void channelAnalyzerSetup();
void channelAnalyzerLoop();

void drawNetworkCountView();
void drawSignalStrengthView();
const char* getSignalStrengthLabel(int rssi);

#endif