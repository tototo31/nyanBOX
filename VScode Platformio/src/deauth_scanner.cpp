/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#include "../include/deauth_scanner.h"
#include "../include/sleep_manager.h"
#include "../include/pindefs.h"
#include <U8g2lib.h>
#include <esp_wifi.h>
#include <WiFi.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define CHANNEL_MIN 1
#define CHANNEL_MAX 13
#define CHANNEL_HOP_INTERVAL 1500

static bool useMainChannels = true;  // true = channels 1,6,11 only, false = all channels 1-13
static const uint8_t mainChannels[] = {1, 6, 11};
static const int numMainChannels = sizeof(mainChannels) / sizeof(mainChannels[0]);
static int currentChannelIndex = 0;
static uint8_t currentChannel = 1;
static uint16_t deauthCount = 0;
static uint16_t totalDeauths = 0;

static uint8_t lastDeauthMAC[6] = {0};
static uint8_t lastDeauthChannel = 0;
bool macSeen = false;

unsigned long lastChannelHop = 0;

// Bypass sanity checks for raw 802.11 frames
extern "C" int ieee80211_raw_frame_sanity_check(int32_t, int32_t, int32_t) {
    return 0;
}

typedef struct {
    uint16_t frame_ctrl;
    uint16_t duration_id;
    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];
    uint16_t sequence_ctrl;
} __attribute__((packed)) wifi_ieee80211_mac_hdr_t;

void formatMAC(char *output, const uint8_t *mac) {
    if (!macSeen) {
        snprintf(output, 18, "N/A");
    } else {
        snprintf(output, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

void packetSniffer(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;

    wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t *)buf;
    if (packet->rx_ctrl.sig_len < sizeof(wifi_ieee80211_mac_hdr_t)) return;

    wifi_ieee80211_mac_hdr_t *hdr = (wifi_ieee80211_mac_hdr_t *)packet->payload;
    uint16_t fc = hdr->frame_ctrl;

    if ((fc & 0xFC) == 0xC0) {  // Deauth frame filter
        memcpy(lastDeauthMAC, hdr->addr2, 6);
        lastDeauthChannel = currentChannel;
        macSeen = true;
        deauthCount++;
        totalDeauths++;
    }
}

void deauthScannerSetup() {
    WiFi.mode(WIFI_MODE_STA);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    delay(100);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_start();

    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT
    };

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(packetSniffer);
    esp_wifi_set_promiscuous(true);
    
    useMainChannels = true;
    currentChannelIndex = 0;
    currentChannel = mainChannels[currentChannelIndex];
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

    deauthCount = 0;
    totalDeauths = 0;
    macSeen = false;
    memset(lastDeauthMAC, 0, sizeof(lastDeauthMAC));
    lastDeauthChannel = 0;
    lastChannelHop = millis();
}

void renderDeauthStats() {
    char headerStr[32];
    char countStr[32];
    char totalStr[32];
    char macStr[18];

    const char* modeText = useMainChannels ? "Main Channels" : "All Channels";
    snprintf(headerStr, sizeof(headerStr), "CH:%2d | %s", currentChannel, modeText);
    snprintf(totalStr, sizeof(totalStr), "Total: %4d", totalDeauths);
    formatMAC(macStr, lastDeauthMAC);

    u8g2.clearBuffer();
    
    u8g2.setFont(u8g2_font_helvR08_tr);
    int headerWidth = u8g2.getUTF8Width(headerStr);
    u8g2.drawStr((128 - headerWidth) / 2, 12, headerStr);
    
    char currentStr[32];
    snprintf(currentStr, sizeof(currentStr), "Current: %4d", deauthCount);
    int currentWidth = u8g2.getUTF8Width(currentStr);
    u8g2.drawStr((128 - currentWidth) / 2, 24, currentStr);
    
    int totalWidth = u8g2.getUTF8Width(totalStr);
    u8g2.drawStr((128 - totalWidth) / 2, 36, totalStr);
    
    u8g2.setFont(u8g2_font_5x8_tr);
    if (macSeen) {
        const char* macLabel = "Last MAC:";
        int macLabelWidth = u8g2.getUTF8Width(macLabel);
        u8g2.drawStr((128 - macLabelWidth) / 2, 46, macLabel);
        
        char macChanStr[24];
        snprintf(macChanStr, sizeof(macChanStr), "%s CH%d", macStr, lastDeauthChannel);
        int macChanWidth = u8g2.getUTF8Width(macChanStr);
        u8g2.drawStr((128 - macChanWidth) / 2, 54, macChanStr);
    } else {
        const char* waitMsg = "Scanning for deauths...";
        int waitWidth = u8g2.getUTF8Width(waitMsg);
        u8g2.drawStr((128 - waitWidth) / 2, 50, waitMsg);
    }
    
    u8g2.setFont(u8g2_font_4x6_tr);
    const char* instruction = "LEFT=Mode  SEL=Exit";
    int instrWidth = u8g2.getUTF8Width(instruction);
    u8g2.drawStr((128 - instrWidth) / 2, 64, instruction);
    
    u8g2.sendBuffer();
}

void hopChannel() {
    if (useMainChannels) {
        currentChannelIndex = (currentChannelIndex + 1) % numMainChannels;
        currentChannel = mainChannels[currentChannelIndex];
    } else {
        currentChannel++;
        if (currentChannel > CHANNEL_MAX) currentChannel = CHANNEL_MIN;
    }

    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    deauthCount = 0;
}

void deauthScannerLoop() {
    unsigned long now = millis();
    
    if (digitalRead(BUTTON_PIN_LEFT) == LOW) {
        useMainChannels = !useMainChannels;
        
        if (useMainChannels) {
            currentChannelIndex = 0;
            currentChannel = mainChannels[currentChannelIndex];
        } else {
            currentChannel = CHANNEL_MIN;
        }
        
        esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
        deauthCount = 0;
        updateLastActivity();
        delay(200);
    }
    
    if (now - lastChannelHop >= CHANNEL_HOP_INTERVAL) {
        hopChannel();
        lastChannelHop = now;
    }

    renderDeauthStats();
}