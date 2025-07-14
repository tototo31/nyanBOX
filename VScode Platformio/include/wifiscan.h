/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________ */
   
#ifndef wifiscan_H
#define wifiscan_H

#include <WiFi.h>
#include <U8g2lib.h>
#include "neopixel.h"
#include "pindefs.h"
#include <Arduino.h>

void wifiscanSetup();
void wifiscanLoop();

#endif
