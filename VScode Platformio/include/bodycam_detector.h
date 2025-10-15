/*
   ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#ifndef BODYCAM_DETECTOR_H
#define BODYCAM_DETECTOR_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

void bodycamDetectorSetup();
void bodycamDetectorLoop();

#endif