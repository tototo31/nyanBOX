/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________ */

#ifndef ABOUT_H
#define ABOUT_H

#include <U8g2lib.h>
#include "pindefs.h"

#define NYANBOX_VERSION "v2.9.26"
extern const char* nyanboxVersion;

void aboutSetup();
void aboutLoop();
void aboutCleanup();

#endif