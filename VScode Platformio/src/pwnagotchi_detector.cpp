/*
   ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#include "../include/pwnagotchi_detector.h"
#include "../include/sleep_manager.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define BTN_UP BUTTON_PIN_UP
#define BTN_DOWN BUTTON_PIN_DOWN
#define BTN_SELECT BUTTON_PIN_RIGHT
#define BTN_BACK BUTTON_PIN_LEFT

struct PwnagotchiData {
  String name;
  String version;
  int pwnd;
  bool deauth;
  int uptime;
  int channel;
  int rssi;
};
static std::vector<PwnagotchiData> pwnagotchi;

int currentIndex = 0;
int listStartIndex = 0;
bool isDetailView = false;
unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200;

static uint8_t pwnChannel = 1;
static uint32_t lastHop = 0;

void IRAM_ATTR pwnagotchiSnifferCallback(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT)
    return;

  auto *pkt = reinterpret_cast<wifi_promiscuous_pkt_t *>(buf);
  const uint8_t *pl = pkt->payload;
  int len = pkt->rx_ctrl.sig_len;
  if (len <= 4)
    return; // too short
  len -= 4; // strip FCS
  if (len < 38 || pl[0] != 0x80)
    return; // not a beacon

  // filter on Pwnagotchi's MAC (Addr #2 @ offset 10)
  char addr[18];
  snprintf(addr, sizeof(addr), "%02x:%02x:%02x:%02x:%02x:%02x", pl[10], pl[11],
           pl[12], pl[13], pl[14], pl[15]);
  if (String(addr) != "de:ad:be:ef:de:ad")
    return;

  // extract the SSID IE (offset 38, length = len-37)
  int ssidLen = len - 37;
  if (ssidLen <= 0)
    return;

  String essid;
  for (int i = 0; i < ssidLen; ++i) {
    char c = (char)pl[38 + i];
    if (c == '\0')
      break;
    essid.concat(c);
  }

  JsonDocument doc; // adjusts automatically on ArduinoJson v7 (if changed to v6, use 1024)
  if (deserializeJson(doc, essid))
    return;

  const char *jsName = doc["name"];
  const char *jsVer = doc["version"];
  int jsPwnd = doc["pwnd_tot"];
  bool jsDeauth = doc["policy"]["deauth"];
  int jsUptime = doc["uptime"];
  int ch = pkt->rx_ctrl.channel;
  int rssi = pkt->rx_ctrl.rssi;

  for (auto &e : pwnagotchi) {
    if (e.name == jsName) {
      e.version = jsVer;
      e.pwnd = jsPwnd;
      e.deauth = jsDeauth;
      e.uptime = jsUptime;
      e.channel = ch;
      e.rssi = rssi;
      return;
    }
  }

  pwnagotchi.push_back(
      {String(jsName), String(jsVer), jsPwnd, jsDeauth, jsUptime, ch, rssi});
}

void pwnagotchiDetectorSetup() {
  pwnagotchi.clear();
  currentIndex = 0;
  listStartIndex = 0;
  isDetailView = false;
  lastButtonPress = 0;

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  esp_wifi_set_promiscuous_rx_cb(&pwnagotchiSnifferCallback);
  wifi_promiscuous_filter_t flt = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
  esp_wifi_set_promiscuous_filter(&flt);
  esp_wifi_set_promiscuous(true);

  esp_wifi_set_channel(pwnChannel, WIFI_SECOND_CHAN_NONE);
  lastHop = millis();
}

void pwnagotchiDetectorLoop() {
  unsigned long now = millis();

  if (now - lastHop > 2000) {
    pwnChannel = (pwnChannel % 13) + 1;
    esp_wifi_set_channel(pwnChannel, WIFI_SECOND_CHAN_NONE);
    lastHop = now;
  }

  if (now - lastButtonPress > debounceTime) {
    if (!isDetailView && digitalRead(BTN_UP) == LOW && currentIndex > 0) {
      --currentIndex;
      if (currentIndex < listStartIndex)
        --listStartIndex;
      updateLastActivity();
      lastButtonPress = now;
    } else if (!isDetailView && digitalRead(BTN_DOWN) == LOW &&
               currentIndex < (int)pwnagotchi.size() - 1) {
      ++currentIndex;
      if (currentIndex >= listStartIndex + 5)
        ++listStartIndex;
      updateLastActivity();
      lastButtonPress = now;
    } else if (!isDetailView && digitalRead(BTN_SELECT) == LOW) {
      isDetailView = true;
      updateLastActivity();
      lastButtonPress = now;
    } else if (digitalRead(BTN_BACK) == LOW) {
      if (isDetailView)
        isDetailView = false;
      updateLastActivity();
      lastButtonPress = now;
    }
  }

  u8g2.clearBuffer();

  if (pwnagotchi.empty()) {
    u8g2.drawStr(0, 10, "Scanning for");
    u8g2.drawStr(0, 20, "Pwnagotchis...");
    u8g2.drawStr(0, 45, "Press SEL to stop");
  } else if (isDetailView) {
    auto &e = pwnagotchi[currentIndex];
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 10, ("Name: " + e.name).c_str());
    u8g2.drawStr(0, 20, ("Ver:  " + e.version).c_str());
    u8g2.drawStr(0, 30, ("Pwnd: " + String(e.pwnd)).c_str());
    u8g2.drawStr(0, 40, ("Deauth: " + String(e.deauth ? "Yes" : "No")).c_str());
    u8g2.drawStr(0, 50, ("Uptime: " + String(e.uptime / 60) + "m").c_str());
    u8g2.drawStr(0, 60, ("Press LEFT to go back"));
  } else {
    u8g2.drawStr(0, 10, "Pwnagotchi list:");
    u8g2.setFont(u8g2_font_6x10_tr);
    for (int i = 0; i < 5; ++i) {
      int idx = listStartIndex + i;
      if (idx >= (int)pwnagotchi.size())
        break;
      auto &e = pwnagotchi[idx];
      if (idx == currentIndex)
        u8g2.drawStr(0, 20 + i * 10, ">");
      String line = e.name.substring(0, 7) + " | RSSI " + String(e.rssi);
      u8g2.drawStr(10, 20 + i * 10, line.c_str());
    }
  }

  u8g2.sendBuffer();
}