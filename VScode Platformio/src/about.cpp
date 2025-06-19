/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________ */
   
#include <Arduino.h>
#include "../include/about.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

void aboutSetup() {
 // N/A currently
}

void aboutLoop() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB14_tr);
  int16_t w = u8g2.getUTF8Width("nyan-BOX");
  u8g2.setCursor((128-w)/2, 20);
  u8g2.print("nyan-BOX");

  u8g2.setFont(u8g2_font_6x10_tf);
  const char* discord = "jbohack & zr_crackiin";
  w = u8g2.getUTF8Width(discord);
  u8g2.setCursor((128-w)/2, 40);
  u8g2.print(discord);
  u8g2.drawStr(7, 60, "defcon.lullaby.cafe");

  u8g2.sendBuffer();
}