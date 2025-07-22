/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#include "../include/channel_analyzer.h"
#include "../include/sleep_manager.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define MAX_CHANNELS 14
#define SCAN_INTERVAL 60000
#define NON_OVERLAP_CHANNELS 3

const int nonOverlapChannels[] = {0, 5, 10};

struct ChannelInfo {
    int networkCount;
    int maxRSSI;
};

static ChannelInfo channels[MAX_CHANNELS];
static unsigned long lastScanTime = 0;
static int currentView = 0;
static bool scanInProgress = false;

void initChannelData() {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        channels[i].networkCount = 0;
        channels[i].maxRSSI = -100;
    }
}

void performChannelScan() {
    if (scanInProgress) return;
    
    scanInProgress = true;
    initChannelData();
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvR08_tr);
    const char* scanText = "Scanning Networks...";
    int scanWidth = u8g2.getUTF8Width(scanText);
    u8g2.drawStr((128 - scanWidth) / 2, 32, scanText);
    u8g2.sendBuffer();
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    int n = WiFi.scanNetworks(false, true);
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            int channel = WiFi.channel(i);
            int rssi = WiFi.RSSI(i);
            
            if (channel >= 1 && channel <= 14) {
                int idx = channel - 1;
                channels[idx].networkCount++;
                
                if (rssi > channels[idx].maxRSSI) {
                    channels[idx].maxRSSI = rssi;
                }
            }
        }
    }
    WiFi.scanDelete();
    scanInProgress = false;
}

void drawChannelBars() {
    u8g2.clearBuffer();
    
    if (currentView == 0) {
        drawNetworkCountView();
    } else {
        drawSignalStrengthView();
    }
    
    u8g2.sendBuffer();
}

void drawNetworkCountView() {
    u8g2.setFont(u8g2_font_helvB08_tr);
    const char* title = "Channel Activity";
    int titleWidth = u8g2.getUTF8Width(title);
    u8g2.drawStr((128 - titleWidth) / 2, 12, title);
    
    u8g2.drawHLine(16, 16, 96);
    
    int totalNetworks = 0;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        totalNetworks += channels[i].networkCount;
    }
    
    u8g2.setFont(u8g2_font_helvR08_tr);
    int y = 26;
    int shown = 0;
    
    for (int i = 0; i < NON_OVERLAP_CHANNELS; i++) {
        int chIdx = nonOverlapChannels[i];
        char line[16];
        snprintf(line, sizeof(line), "Ch %d: %d", chIdx + 1, channels[chIdx].networkCount);
        int lineWidth = u8g2.getUTF8Width(line);
        u8g2.drawStr((128 - lineWidth) / 2, y, line);
        y += 12;
        shown++;
    }
    
    if (shown == 0) {
        const char* noData = "No activity";
        int noDataWidth = u8g2.getUTF8Width(noData);
        u8g2.drawStr((128 - noDataWidth) / 2, 32, noData);
    }
    
    u8g2.setFont(u8g2_font_4x6_tr);
    const char* instructions = "U/D=Move R=SCN SEL=EXIT";
    int instWidth = u8g2.getUTF8Width(instructions);
    u8g2.drawStr((128 - instWidth) / 2, 62, instructions);
}

void drawSignalStrengthView() {
    u8g2.setFont(u8g2_font_helvB08_tr);
    const char* title = "Signal Strength";
    int titleWidth = u8g2.getUTF8Width(title);
    u8g2.drawStr((128 - titleWidth) / 2, 12, title);
    
    u8g2.drawHLine(16, 16, 96);
    
    u8g2.setFont(u8g2_font_helvR08_tr);
    int y = 28;
    int shown = 0;
    
    for (int i = 0; i < NON_OVERLAP_CHANNELS; i++) {
        int chIdx = nonOverlapChannels[i];
        char line[16];
        
        if (channels[chIdx].networkCount > 0) {
            const char* strength = getSignalStrengthLabel(channels[chIdx].maxRSSI);
            snprintf(line, sizeof(line), "Ch %d: %s", chIdx + 1, strength);
        } else {
            snprintf(line, sizeof(line), "Ch %d: Clear", chIdx + 1);
        }
        
        int lineWidth = u8g2.getUTF8Width(line);
        u8g2.drawStr((128 - lineWidth) / 2, y, line);
        y += 10;
        shown++;
    }
    
    if (shown == 0) {
        const char* noData = "No signals";
        int noDataWidth = u8g2.getUTF8Width(noData);
        u8g2.drawStr((128 - noDataWidth) / 2, 33, noData);
    }
    
    u8g2.setFont(u8g2_font_4x6_tr);
    const char* instructions = "U/D=Move R=SCN SEL=EXIT";
    int instWidth = u8g2.getUTF8Width(instructions);
    u8g2.drawStr((128 - instWidth) / 2, 62, instructions);
}

const char* getSignalStrengthLabel(int rssi) {
    if (rssi > -50) return "Strong";
    if (rssi > -70) return "Medium";
    return "Weak";
}

void drawSummaryView() {
    u8g2.clearBuffer();
    
    u8g2.setFont(u8g2_font_helvB08_tr);
    const char* title = "Overview";
    int titleWidth = u8g2.getUTF8Width(title);
    u8g2.drawStr((128 - titleWidth) / 2, 12, title);
    
    u8g2.drawHLine(16, 16, 96);
    
    int busiestChannel = 1;
    int maxNetworks = 0;
    int totalNetworks = 0;
    int quietestChannel = 1;
    int minNetworks = 999;
    
    for (int i = 0; i < MAX_CHANNELS; i++) {
        totalNetworks += channels[i].networkCount;
    }
    
    for (int i = 0; i < NON_OVERLAP_CHANNELS; i++) {
        int chIdx = nonOverlapChannels[i];
        if (channels[chIdx].networkCount > maxNetworks) {
            maxNetworks = channels[chIdx].networkCount;
            busiestChannel = chIdx + 1;
        }
        if (channels[chIdx].networkCount < minNetworks) {
            minNetworks = channels[chIdx].networkCount;
            quietestChannel = chIdx + 1;
        }
    }
    
    u8g2.setFont(u8g2_font_helvR08_tr);
    
    char totalStr[18];
    snprintf(totalStr, sizeof(totalStr), "%d networks", totalNetworks);
    int totalWidth = u8g2.getUTF8Width(totalStr);
    u8g2.drawStr((128 - totalWidth) / 2, 26, totalStr);
    
    if (maxNetworks > 0) {
        char busiestStr[18];
        snprintf(busiestStr, sizeof(busiestStr), "Busiest: Ch %d", busiestChannel);
        int busiestWidth = u8g2.getUTF8Width(busiestStr);
        u8g2.drawStr((128 - busiestWidth) / 2, 38, busiestStr);
        
        char quietestStr[18];
        snprintf(quietestStr, sizeof(quietestStr), "Quietest: Ch %d", quietestChannel);
        int quietestWidth = u8g2.getUTF8Width(quietestStr);
        u8g2.drawStr((128 - quietestWidth) / 2, 50, quietestStr);
    }
    
    u8g2.setFont(u8g2_font_4x6_tr);
    const char* instructions = "U/D=Move R=SCN SEL=EXIT";
    int instWidth = u8g2.getUTF8Width(instructions);
    u8g2.drawStr((128 - instWidth) / 2, 62, instructions);
    
    u8g2.sendBuffer();
}

void channelAnalyzerSetup() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    pinMode(BUTTON_PIN_UP, INPUT_PULLUP);
    pinMode(BUTTON_PIN_DOWN, INPUT_PULLUP);
    pinMode(BUTTON_PIN_RIGHT, INPUT_PULLUP);
    
    currentView = 0;
    lastScanTime = 0;
    initChannelData();
    
    performChannelScan();
    lastScanTime = millis();
}

void channelAnalyzerLoop() {
    unsigned long now = millis();
    
    if (now - lastScanTime >= SCAN_INTERVAL) {
        performChannelScan();
        lastScanTime = now;
    }
    
    static bool upPressed = false;
    static bool downPressed = false;
    static bool rightPressed = false;
    
    bool upNow = digitalRead(BUTTON_PIN_UP) == LOW;
    bool downNow = digitalRead(BUTTON_PIN_DOWN) == LOW;
    bool rightNow = digitalRead(BUTTON_PIN_RIGHT) == LOW;
    
    if (upNow && !upPressed) {
        currentView = (currentView - 1 + 3) % 3;
        updateLastActivity();
        delay(200);
    }
    
    if (downNow && !downPressed) {
        currentView = (currentView + 1) % 3;
        updateLastActivity();
        delay(200);
    }
    
    if (rightNow && !rightPressed) {
        performChannelScan();
        lastScanTime = now;
        delay(200);
    }
    
    upPressed = upNow;
    downPressed = downNow;
    rightPressed = rightNow;
    
    if (currentView == 2) {
        drawSummaryView();
    } else {
        drawChannelBars();
    }
    
    delay(50);
}