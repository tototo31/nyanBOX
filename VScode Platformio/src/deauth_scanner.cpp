/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#include "../include/deauth_scanner.h"
#include "../include/pindefs.h"
#include <U8g2lib.h>
#include <esp_wifi.h>
#include <WiFi.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define CHANNEL_MIN 1
#define CHANNEL_MAX 13
#define CHANNEL_HOP_INTERVAL 1500

static uint8_t currentChannel = CHANNEL_MIN;
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
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

    deauthCount = 0;
    totalDeauths = 0;
    macSeen = false;
    memset(lastDeauthMAC, 0, sizeof(lastDeauthMAC));
    lastDeauthChannel = 0;
    lastChannelHop = millis();
}

void renderDeauthStats() {
    char chanStr[32];
    char countStr[32];
    char totalStr[32];
    char macStr[18];
    char macChanStr[32];

    snprintf(chanStr, sizeof(chanStr), "Channel: %2d", currentChannel);
    snprintf(countStr, sizeof(countStr), "Deauths: %4d", deauthCount);
    snprintf(totalStr, sizeof(totalStr), "Total:   %4d", totalDeauths);
    formatMAC(macStr, lastDeauthMAC);

    if (!macSeen) {
        snprintf(macChanStr, sizeof(macChanStr), "Seen on: N/A");
    } else {
        snprintf(macChanStr, sizeof(macChanStr), "Seen on: CH %2d", lastDeauthChannel);
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, chanStr);
    u8g2.drawStr(0, 20, countStr);
    u8g2.drawStr(0, 30, totalStr);
    u8g2.drawStr(0, 45, "Last MAC:");
    u8g2.drawStr(0, 55, macStr);
    u8g2.drawStr(0, 64, macChanStr);
    u8g2.sendBuffer();
}

void hopChannel() {
    currentChannel++;
    if (currentChannel > CHANNEL_MAX) currentChannel = CHANNEL_MIN;

    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    deauthCount = 0;
}

void deauthScannerLoop() {
    unsigned long now = millis();
    if (now - lastChannelHop >= CHANNEL_HOP_INTERVAL) {
        hopChannel();
        lastChannelHop = now;
    }

    renderDeauthStats();
}