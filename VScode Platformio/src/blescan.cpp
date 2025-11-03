/*
   ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________
*/

#include "../include/blescan.h"
#include "../include/sleep_manager.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include <vector>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define BTN_UP BUTTON_PIN_UP
#define BTN_DOWN BUTTON_PIN_DOWN
#define BTN_RIGHT BUTTON_PIN_RIGHT
#define BTN_BACK BUTTON_PIN_LEFT

struct BLEDeviceData {
  char name[32];
  char address[18];
  int8_t rssi;
  bool hasName;
  unsigned long lastSeen;
};
static std::vector<BLEDeviceData> bleDevices;

const int MAX_DEVICES = 100;

int currentIndex = 0;
int listStartIndex = 0;
bool isDetailView = false;
unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200;

static bool isScanning = false;
static unsigned long lastScanTime = 0;
const unsigned long scanInterval = 180000;
const unsigned long scanDuration = 8;

static bool bleInitialized = false;
static bool scanCompleted = false;

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x100,
    .scan_window            = 0xA0,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

static void bda_to_string(uint8_t *bda, char *str, size_t size) {
    if (bda == NULL || str == NULL || size < 18) {
        return;
    }
    snprintf(str, size, "%02x:%02x:%02x:%02x:%02x:%02x",
             bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
}

static void process_scan_result(esp_ble_gap_cb_param_t *scan_result) {
    if (bleDevices.size() >= MAX_DEVICES) {
        return;
    }

    uint8_t *bda = scan_result->scan_rst.bda;
    char addrStr[18];
    bda_to_string(bda, addrStr, sizeof(addrStr));

    if (strlen(addrStr) < 12) return;

    for (size_t i = 0; i < bleDevices.size(); i++) {
        if (strcmp(bleDevices[i].address, addrStr) == 0) {
            bleDevices[i].rssi = scan_result->scan_rst.rssi;
            bleDevices[i].lastSeen = millis();
            
            if (!bleDevices[i].hasName) {
                uint8_t *adv_name = NULL;
                uint8_t adv_name_len = 0;
                adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                    ESP_BLE_AD_TYPE_NAME_CMPL,
                                                    &adv_name_len);
                
                if (adv_name == NULL) {
                    adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                        ESP_BLE_AD_TYPE_NAME_SHORT,
                                                        &adv_name_len);
                }

                if (adv_name != NULL && adv_name_len > 0 && adv_name_len < 32) {
                    memcpy(bleDevices[i].name, adv_name, adv_name_len);
                    bleDevices[i].name[adv_name_len] = '\0';
                    bleDevices[i].hasName = true;
                }
            }
            
            std::sort(bleDevices.begin(), bleDevices.end(),
                      [](const BLEDeviceData &a, const BLEDeviceData &b) {
                        return a.rssi > b.rssi;
                      });
            return;
        }
    }

    BLEDeviceData newDev = {};
    strncpy(newDev.address, addrStr, 17);
    newDev.address[17] = '\0';
    newDev.rssi = scan_result->scan_rst.rssi;
    newDev.lastSeen = millis();
    
    strcpy(newDev.name, "Unknown");
    newDev.hasName = false;

    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                        ESP_BLE_AD_TYPE_NAME_CMPL,
                                        &adv_name_len);
    
    if (adv_name == NULL) {
        adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                            ESP_BLE_AD_TYPE_NAME_SHORT,
                                            &adv_name_len);
    }

    if (adv_name != NULL && adv_name_len > 0 && adv_name_len < 32) {
        memcpy(newDev.name, adv_name, adv_name_len);
        newDev.name[adv_name_len] = '\0';
        newDev.hasName = true;
    }

    bleDevices.push_back(newDev);
    
    std::sort(bleDevices.begin(), bleDevices.end(),
              [](const BLEDeviceData &a, const BLEDeviceData &b) {
                return a.rssi > b.rssi;
              });
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        if (param->scan_param_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            isScanning = true;
            esp_ble_gap_start_scanning(scanDuration);
            lastScanTime = millis();
        }
        break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            isScanning = false;
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
        switch (param->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            process_scan_result(param);
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            isScanning = false;
            lastScanTime = millis();
            scanCompleted = true;
            break;
        default:
            break;
        }
        break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        isScanning = false;
        scanCompleted = true;
        break;
    default:
        break;
    }
}

void blescanSetup() {
    bleDevices.clear();
    bleDevices.reserve(MAX_DEVICES);
    currentIndex = listStartIndex = 0;
    isDetailView = false;
    lastButtonPress = 0;
    isScanning = true;
    scanCompleted = false;

    u8g2.begin();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Scanning for");
    u8g2.drawStr(0, 20, "BLE devices...");
    char countStr[32];
    snprintf(countStr, sizeof(countStr), "%d/%d devices", 0, MAX_DEVICES);
    u8g2.drawStr(0, 35, countStr);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 60, "Press SEL to exit");
    u8g2.sendBuffer();

    if (!btStarted()) {
        btStart();
    }

    esp_bluedroid_status_t bt_state = esp_bluedroid_get_status();
    if (bt_state == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
        esp_bluedroid_init();
    }
    if (bt_state != ESP_BLUEDROID_STATUS_ENABLED) {
        esp_bluedroid_enable();
    }

    esp_ble_gap_register_callback(esp_gap_cb);
    esp_ble_gap_set_scan_params(&ble_scan_params);

    bleInitialized = true;

    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_BACK, INPUT_PULLUP);
}

void blescanLoop() {
    if (!bleInitialized) return;

    unsigned long now = millis();

    if (isScanning) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(0, 10, "Scanning for");
        u8g2.drawStr(0, 20, "BLE devices...");
        
        char countStr[32];
        snprintf(countStr, sizeof(countStr), "%d/%d devices", (int)bleDevices.size(), MAX_DEVICES);
        u8g2.drawStr(0, 35, countStr);
        
        int barWidth = 120;
        int barHeight = 10;
        int barX = (128 - barWidth) / 2;
        int barY = 42;
        
        u8g2.drawFrame(barX, barY, barWidth, barHeight);
        
        int fillWidth = (bleDevices.size() * (barWidth - 4)) / MAX_DEVICES;
        if (fillWidth > 0) {
            u8g2.drawBox(barX + 2, barY + 2, fillWidth, barHeight - 4);
        }
        
        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.drawStr(0, 62, "Press SEL to exit");
        
        u8g2.sendBuffer();
        return;
    }

    if (!isScanning && scanCompleted && now - lastScanTime > scanInterval) {
        if (bleDevices.size() >= MAX_DEVICES) {
            std::sort(bleDevices.begin(), bleDevices.end(),
                    [](const BLEDeviceData &a, const BLEDeviceData &b) {
                        if (a.lastSeen != b.lastSeen) {
                            return a.lastSeen < b.lastSeen;
                        }
                        return a.rssi < b.rssi;
                    });
            
            int devicesToRemove = MAX_DEVICES / 4;
            if (devicesToRemove > 0) {
                bleDevices.erase(bleDevices.begin(), 
                                bleDevices.begin() + devicesToRemove);
            }
            
            currentIndex = listStartIndex = 0;
        }
        
        scanCompleted = false;
        isScanning = true;
        esp_ble_gap_start_scanning(scanDuration);
        lastScanTime = now;
        return;
    }

    if (scanCompleted && now - lastButtonPress > debounceTime) {
        if (!isDetailView && digitalRead(BTN_UP) == LOW && currentIndex > 0) {
            --currentIndex;
            if (currentIndex < listStartIndex)
                --listStartIndex;
            lastButtonPress = now;
        } else if (!isDetailView && digitalRead(BTN_DOWN) == LOW &&
                   currentIndex < (int)bleDevices.size() - 1) {
            ++currentIndex;
            if (currentIndex >= listStartIndex + 5)
                ++listStartIndex;
            lastButtonPress = now;
        } else if (!isDetailView && digitalRead(BTN_RIGHT) == LOW &&
                   !bleDevices.empty()) {
            isDetailView = true;
            lastButtonPress = now;
        } else if (digitalRead(BTN_BACK) == LOW) {
            isDetailView = false;
            lastButtonPress = now;
        }
    }

    if (bleDevices.empty()) {
        currentIndex = listStartIndex = 0;
        isDetailView = false;
    } else {
        currentIndex = constrain(currentIndex, 0, (int)bleDevices.size() - 1);
        listStartIndex =
            constrain(listStartIndex, 0, max(0, (int)bleDevices.size() - 5));
    }

    u8g2.clearBuffer();
    
    if (bleDevices.empty()) {
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(0, 10, "No devices found");
        u8g2.setFont(u8g2_font_5x8_tr);
        char timeStr[32];
        unsigned long timeLeft = (scanInterval - (now - lastScanTime)) / 1000;
        snprintf(timeStr, sizeof(timeStr), "Scanning in %lus", timeLeft);
        u8g2.drawStr(0, 30, timeStr);
        u8g2.drawStr(0, 45, "Press SEL to exit");
    } else if (isDetailView) {
        auto &dev = bleDevices[currentIndex];
        u8g2.setFont(u8g2_font_5x8_tr);
        char buf[32];
        snprintf(buf, sizeof(buf), "Name: %s", dev.name);
        u8g2.drawStr(0, 10, buf);
        snprintf(buf, sizeof(buf), "Addr: %s", dev.address);
        u8g2.drawStr(0, 20, buf);
        snprintf(buf, sizeof(buf), "RSSI: %d", dev.rssi);
        u8g2.drawStr(0, 30, buf);
        snprintf(buf, sizeof(buf), "Age: %lus", (millis() - dev.lastSeen) / 1000);
        u8g2.drawStr(0, 40, buf);
        u8g2.drawStr(0, 60, "L=Back SEL=Exit");
    } else {
        u8g2.setFont(u8g2_font_6x10_tr);
        char header[32];
        snprintf(header, sizeof(header), "BLE Devices: %d/%d", (int)bleDevices.size(), MAX_DEVICES);
        u8g2.drawStr(0, 10, header);
        for (int i = 0; i < 5; ++i) {
            int idx = listStartIndex + i;
            if (idx >= (int)bleDevices.size())
                break;
            auto &d = bleDevices[idx];
            if (idx == currentIndex)
                u8g2.drawStr(0, 20 + i * 10, ">");
            char line[32];
            const char* displayName = d.hasName && d.name[0] ? d.name : "Unknown";
            snprintf(line, sizeof(line), "%.8s | RSSI %d", displayName, d.rssi);
            u8g2.drawStr(10, 20 + i * 10, line);
        }
    }
    u8g2.sendBuffer();
}