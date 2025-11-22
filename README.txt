ESP32 Fingerprint + Blynk Smart Door Lock

A complete IoT-based biometric + app-controlled door lock system using
ESP32, R307S fingerprint sensor, OLED display, relay-controlled solenoid
lock, and Blynk IoT.

This project provides a dual‑authentication door lock that can toggle
locking/unlocking using: - Fingerprint authentication (R307S) - Blynk
app (remote control) - Local physical button (short/long press) -
Auto-locking logic
- OLED status display
- Persistent storage for selected ID using ESP32 Preferences

------------------------------------------------------------------------

🔧 Hardware Used

-   ESP32 Dev Board (with PSRAM/VROM)
-   R307S / 302SF / 302S Fingerprint Sensor (UART based)
-   0.96” SSD1306 OLED Display (I2C)
-   Solenoid Door Lock
-   Buck Converter (12V → 5V for ESP32)
-   Relay Module (to control solenoid)
-   Push Button (short press = enroll, long press = manual lock)

------------------------------------------------------------------------

📌 Features

✔ Fingerprint System

-   Enroll up to 200 fingerprints
-   Prevent duplicate ID enrollment
-   Delete individual IDs
-   Auto-detection of stored IDs
-   Fast search + confidence filtering

✔ Smart Lock Logic

-   Lock/unlock via:
    -   Fingerprint
    -   Blynk app (toggle switch)
    -   Long button press (manual lock)
-   Auto re-lock after configured time
-   OLED feedback for every operation

✔ Blynk IoT Integration

Virtual pins: | Pin | Function | |—–|———-| | V0 | Lock/Unlock switch | |
V2 | Start enrollment | | V3 | Last matched ID | | V4 | Stored ID
dropdown | | V5 | Delete ID | | V6 | Set selected ID | | V7 | Status
messages |

------------------------------------------------------------------------

🖥 Software Dependencies

The project uses the following Arduino libraries:

-   WiFi.h (built-in)
-   BlynkSimpleEsp32.h
-   Adafruit_SSD1306
-   Adafruit_GFX
-   Adafruit Fingerprint Sensor Library
-   Preferences.h (ESP32 storage)

------------------------------------------------------------------------

📂 Repository Structure

    /ESP32-Fingerprint-Doorlock
    │── main.ino             → Full project code
    │── README.txt           → Documentation (this file)
    │── requirements.txt     → Required Arduino libraries

------------------------------------------------------------------------

📜 Setup Instructions

1.  Install libraries from Arduino Library Manager:
    -   Adafruit SSD1306
    -   Adafruit GFX
    -   Adafruit Fingerprint Sensor Library
    -   Blynk (legacy support)
2.  Set OLED pins (default uses GPIO21/22)
3.  Connect fingerprint sensor to:
    -   TX → GPIO17
    -   RX → GPIO16
4.  Set relay to GPIO4
5.  Configure WiFi + Blynk token in code
6.  Upload and test

------------------------------------------------------------------------

👆 Button Controls

  Action               Behavior
  -------------------- ------------------------------
  Short press          Start fingerprint enrollment
  Long press (2 sec)   Manual lock
  Finger scan          Toggle lock/unlock

------------------------------------------------------------------------

📌 Notes

-   Enrollment follows 2-step capture (same finger twice)
-   OLED shows all required feedback
-   Blynk messages auto-clear after timeout
-   Sensor must respond successfully for full features

------------------------------------------------------------------------

🤝 Contribution

Feel free to fork and contribute improvements!
You may add: - Logging system - RTC-based auto locking schedule - Face
unlock module - Web dashboard for remote management

------------------------------------------------------------------------

📞 Support

For hardware or code questions, open an issue in the repository.
