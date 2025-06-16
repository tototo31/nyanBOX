/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#ifndef DEAUTH_SCANNER_H
#define DEAUTH_SCANNER_H

#include <WiFi.h>
#include "esp_wifi.h"

void deauthScannerSetup();
void deauthScannerLoop();

#endif