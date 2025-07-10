/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#ifndef DEAUTH_H
#define DEAUTH_H

#include <WiFi.h>
#include "esp_wifi.h"
#include <U8g2lib.h>
#include <Arduino.h>

void deauthSetup();
void deauthLoop();

#endif