<div align="center">
  <img src="https://github.com/user-attachments/assets/0eba90bc-2ff5-40df-88a1-92e23396d1d3" alt="logo" width="100" height="auto" />
  
  <h1>nyanBOX</h1>
  <p>All-in-One Gadget for BLE and 2.4GHz Networks</p>
  <p>Maintained by jbohack & zr_crackiin</p>

  <!-- Badges -->
  <p>
    <a href="https://github.com/jbohack/nyanBOX" title="GitHub repo">
      <img src="https://img.shields.io/static/v1?label=nyanBOX&message=jbohack&color=purple&logo=github" alt="nyanBOX - jbohack">
    </a>
    <a href="https://github.com/cifertech/nrfbox" title="Original Project">
      <img src="https://img.shields.io/badge/original%20project-CiferTech%20nRFBox-blue" alt="Original Project">
    </a>
    <a href="https://github.com/jbohack/nyanBOX">
      <img src="https://img.shields.io/github/stars/jbohack/nyanBOX?style=social" alt="stars - nyanBOX">
    </a>
    <a href="https://github.com/jbohack/nyanBOX">
      <img src="https://img.shields.io/github/forks/jbohack/nyanBOX?style=social" alt="forks - nyanBOX">
    </a>
  </p>

  <p>
    <a href="https://defcon.lullaby.cafe">Website</a> ·
    <a href="https://discord.gg/J5A3zDC2y8">Discord</a> ·
    <a href="https://github.com/cifertech/nrfbox">Original Project</a>
  </p>
</div>

---


## 📖 About nyanBOX

nyanBOX is a fork of the original nRFBOX project by CiferTech, now maintained by jbohack & zr_crackiin. This version includes various improvements and new features while maintaining compatibility with the original hardware.

### 🆕 What's New
- Updated UI with improved display layout
- Enhanced stability and performance
- New features and improvements to existing functionality
- Active maintenance and support

## :star2: Project Overview

nyanBOX is a wireless toolkit designed to explore, analyze, and interact with various wireless communication protocols. Based on the original nRFBOX by CiferTech, this fork retains all the powerful features while introducing new improvements and providing ongoing maintenance.

The device combines the ESP32 Wroom32U, NRF24 modules, an OLED display, and other components to create a multifunctional device that can act as a scanner, analyzer, jammer, BLE jammer, BLE spoofer, and perform advanced tasks such as the "Sour Apple" attack.

<div align="center">
  <img src="https://github.com/user-attachments/assets/530e5686-09db-4f02-aabe-80a8abcbb036" alt="nyanBOX Interface" width="650" />
</div>


## 🎯 Features

### 📶 WiFi Tools
- **WiFi Scanner** – Detects nearby WiFi access points
- **Channel Analyzer** – Monitors WiFi channel utilization and signal strength for network planning
- **WiFi Deauther** – Disrupts 2.4GHz WiFi device communication
- **Deauth Scanner** – Monitors and analyzes WiFi deauthentication frames in real-time
- **Beacon Spam** – Broadcasts multiple fake WiFi networks for testing. Choose to clone real nearby networks, select specific SSIDs, or use a list of random names.
- **Evil Portal** – Creates captive portal with multiple templates (Generic, Facebook, Google) that automatically scans nearby networks for realistic SSID spoofing and credential capture.
- **Pwnagotchi Detector** – Detects nearby Pwnagotchi devices and displays their information
- **Pwnagotchi Spam** - Pwnagotchi grid flooding tool that generates fake beacon frames with randomized identities, faces, names, and versions (contains optional DoS mode).
- **WLAN Jammer** – Jams wireless communication on selected channels

### 🔵 Bluetooth (BLE) Tools
- **BLE Scanner** – Detects nearby BLE devices
- **nyanBOX Detector** – Discovers nearby nyanBOX devices and displays their information including level, version, and signal strength.
- **Flipper Scanner** – Detects nearby Flipper Zero devices
- **BLE Spammer** – Broadcasts BLE advertisement packets for testing
- **BLE Jammer** – Disrupts BLE device communication
- **Sour Apple** – Mimics Apple Bluetooth signals like AirPods pairing pop-up to test device resilience against protocol exploits.
- **BLE Spoofer** – Simulates BLE devices for testing and research

### 📡 Signal & Protocol Tools
- **Proto Kill Mode** – Advanced tool for disrupting various wireless protocols
- **Scanner** – Scans the 2.4GHz frequency band to detect active channels and devices
- **Analyzer** – Analyzes detected signals and provides detailed activity information

### 🎮 Leveling System
nyanBOX features a built-in RPG-style leveling system that tracks your usage throughout using the device:

- **Level Progression** – Gain XP by using different tools and features
- **Rank System** – Progress through 9 different ranks
- **Usage Tracking** – Different XP rates for scanning, attacks, and utilities/misc
- **Session Bonuses** – Extra XP for extended tool usage
- **Level Display** – Current level shown on main menu, detailed stats accessible via RIGHT arrow
- **Progress Persistence** – Level data saved to EEPROM, survives power cycles
- **XP Reset** – Reset progress via Settings menu if desired
- **Device Networking** – Your level and version are automatically broadcasted to nearby nyanBOX devices for discovery

Hit RIGHT in the main menu to check your stats. Level up by tinkering with RF signals and unlock ranks as you progress. Other nyanBOX users can see your progress when they scan for nearby devices!

---

## 📟 Hardware
- **Main Controller**: ESP32 Wroom32U
- **Wireless Modules**: NRF24 GTmini x3
- **Display**: 0.96" OLED
- **Connectivity**: USB-C, UART

---

## 🔧 Prerequisites

### USB Drivers
Install the required USB drivers for your ESP32 board:
- **CP210x-based boards**: [CP210x USB to UART Bridge VCP Drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
- **CH340/CH341-based boards**: [CH340 Drivers](https://www.wch.cn/download/CH341SER_EXE.html)

---
## 🚀 Development Setup

### Prerequisites
- [Visual Studio Code](https://code.visualstudio.com/)
- [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) extension

### Getting Started
1. Clone this repository:
   ```bash
   git clone https://github.com/jbohack/nyanBOX.git
   ```
2. Open the project in VS Code:
   - Select `File > Open Folder`
   - Choose the nyanBOX directory

### Configuration
- Pre-configured for ESP32-DevKitC in `platformio.ini`
- For other ESP32 boards, update the `board` parameter in `platformio.ini`

### Build and Upload
1. **Build**: Click the checkmark icon in the status bar
2. **Connect**: Plug in your ESP32 via USB
3. **Upload**: Click the right-arrow icon
4. **Monitor**: Click the plug icon to open the serial monitor

### Troubleshooting
- **Upload Failing?** Try holding the BOOT button during upload
- **Connection Issues?** Verify the correct COM port is selected
- **Driver Problems?** Ensure proper USB drivers are installed (see Prerequisites)

---

## ⚠️ Legal Disclaimer

This project is provided for **educational and authorized security research purposes only**.  
The creators and maintainers of **nyanBOX** disclaim all responsibility for any unauthorized or unlawful use of this software or hardware.  
It is the sole responsibility of the user to ensure compliance with all applicable local, state, federal, and international laws.

### 📌 Important Notes

- Unauthorized scanning, jamming, or interference with wireless communications may be **illegal** in your jurisdiction.  
- Always obtain **explicit permission** before testing on networks or devices you do not own.  
- Certain features may be **restricted or prohibited** in some countries or regions.  
- The developers assume **no liability** for any damages, legal consequences, or misuse resulting from this project.

By using **nyanBOX**, you agree to use it **only for lawful, ethical, and educational purposes**, and acknowledge your responsibility to remain informed about and compliant with all relevant laws and regulations.

---

## 📜 License

Distributed under the MIT License. See [LICENSE](LICENSE) for more information.

---

## 💝 Support & Contact

If you find nyanBox useful, please consider supporting the project:
- [jbohack's Ko-fi](https://ko-fi.com/jbohack)
- [zr_crackiin's Ko-fi](https://ko-fi.com/zrcrackiin)

### Maintainers
- [jbohack](https://github.com/jbohack)
- [zr_crackiin](https://github.com/zRCrackiiN)

## 🙏 Acknowledgements 

### Open Source Projects Used
- [Poor Man's 2.4 GHz Scanner](https://forum.arduino.cc/t/poor-mans-2-4-ghz-scanner/54846)
- [arduino_oled_menu](https://github.com/upiir/arduino_oled_menu)
- [nRF24L01-WiFi-Jammer](https://github.com/hugorezende/nRF24L01-WiFi-Jammer)
- [Universal-RC-system](https://github.com/alexbeliaev/Universal-RC-system)
- [AppleJuice](https://github.com/ECTO-1A/AppleJuice)
- [ESP32-Sour-Apple](https://github.com/RapierXbox/ESP32-Sour-Apple)
- [PwnGridSpam](https://github.com/7h30th3r0n3/PwnGridSpam)
- [Original nRFBOX Project](https://github.com/cifertech/nrfbox)

### Community
A big thank you to all contributors and community members who have helped improve nyanBOX!


#BadgeLife
