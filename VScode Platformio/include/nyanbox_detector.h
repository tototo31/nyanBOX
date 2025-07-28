/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________ */
   
#ifndef NYANBOX_DETECTOR_H
#define NYANBOX_DETECTOR_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include <BLEScan.h>
#include <U8g2lib.h>
#include "neopixel.h"
#include "pindefs.h"

void nyanboxDetectorSetup();
void nyanboxDetectorLoop();

#endif