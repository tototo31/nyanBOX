/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________ */
   
#ifndef NYANBOX_ADVERTISER_H
#define NYANBOX_ADVERTISER_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include "level_system.h"
#include "about.h"

void initNyanboxAdvertiser();
void startNyanboxAdvertiser();
void stopNyanboxAdvertiser();
void updateNyanboxAdvertiser();
bool isNyanboxAdvertising();

#endif