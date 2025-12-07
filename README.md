# ESP32 Fingerprint + Blynk Smart Door Lock (Updated)

A complete IoT-based biometric and app-controlled smart door lock system using:

- ESP32 Dev Board
- R307S Fingerprint Sensor
- OLED Display (SSD1306)
- Relay-controlled Solenoid Lock
- Blynk IoT app for remote control

This upgraded version provides:

- Fast, **instant startup**
- Persistent fingerprint database with **up to 200 fingerprints**
- **Last unlock logs** (up to 15 recent events)
- Real-time clock support for accurate timestamps
- Circular buffer to maintain unlock history
- Multi-authentication: Fingerprint, Blynk app, and physical button

---

## 🔧 Hardware Required

- ESP32 Dev Board (with PSRAM/VROM)
- R307S / 302SF / 302S Fingerprint Sensor (UART)
- 0.96” SSD1306 OLED Display (I2C)
- Solenoid Door Lock
- Buck Converter (12V → 5V for ESP32)
- Relay Module
- Push Button

---

## 📌 Key Features

### Fingerprint System

- Enroll **up to 200 fingerprints**
- Prevent duplicate ID enrollment
- Delete individual IDs
- Auto-detect stored IDs in sensor
- Fast search with confidence filtering
- Persistent storage using ESP32 **Preferences**
- Real-time unlock logs with timestamps

### Smart Lock Logic

- Lock/unlock via:
  - Fingerprint scan
  - Blynk app toggle switch
  - Long button press (manual lock)
- Auto re-lock after configurable time
- OLED feedback for all operations
- Instant display on boot and fast startup

### Blynk Integration

| Virtual Pin | Function |
|------------|---------|
| V0 | Lock/Unlock switch |
| V2 | Start fingerprint enrollment |
| V3 | Last matched ID |
| V4 | Stored ID dropdown |
| V5 | Delete ID |
| V6 | Set selected ID |
| V7 | Status messages |
| V8 | Sync fingerprint database |
| V9 | Get specific fingerprint record |
| V10 | Set fingerprint name |
| V11 | Export database display |
| V13 | Display database |
| V14 | Show last unlock logs |

---

## 🖥 Software Dependencies

- **WiFi.h** (built-in)
- **BlynkSimpleEsp32.h**
- **Adafruit_SSD1306**
- **Adafruit_GFX**
- **Adafruit Fingerprint Library**
- **Preferences.h** (ESP32 storage)

---

## 📂 Repository Structure

```
/ESP32-Fingerprint-Doorlock
│── main.ino             → Full project code
│── README.md            → Documentation (this file)
│── requirements.txt     → Required Arduino libraries
```

---

## 📜 Setup Instructions

1. Install libraries via Arduino Library Manager:
   - Adafruit SSD1306
   - Adafruit GFX
   - Adafruit Fingerprint Library
   - Blynk (legacy)
2. Connect OLED (default: SDA → GPIO21, SCL → GPIO22)
3. Connect fingerprint sensor (TX → GPIO17, RX → GPIO16)
4. Relay → GPIO4, Push Button → GPIO15
5. Configure WiFi SSID, password, and Blynk auth token in the code
6. Upload and test

---

## 👆 Button Controls

| Action | Behavior |
|--------|---------|
| Short press | Start fingerprint enrollment |
| Long press (2s) | Manual lock |
| Finger scan | Toggle lock/unlock |

---

## 📌 Notes

- Enrollment requires **two-step capture** (same finger twice)
- OLED provides all operational feedback
- Blynk messages auto-clear after a timeout
- Unlock logs are stored in a **circular buffer** with real timestamps
- Database sync ensures fingerprint sensor and stored IDs match

---

## 🤝 Contribution

Feel free to fork and contribute improvements:

- Logging system enhancements
- RTC-based auto-lock scheduling
- Additional biometric methods (face unlock)
- Web dashboard for remote monitoring

---

## 📞 Support

For hardware or code questions, open an issue in the repository.  

---

**This version optimizes startup, adds persistent logs, and integrates robust database handling for a professional IoT smart lock experience.**
