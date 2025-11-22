/******************************************************
  Advanced ESP32 + R307S Fingerprint Door Lock (FULL)
******************************************************/

/* -------------- CONFIG -------------- */
#define BLYNK_TEMPLATE_ID   "TMPL3VcvkOhFq"
#define BLYNK_TEMPLATE_NAME "door lock esp "
#define BLYNK_AUTH_TOKEN    "_cEVyXFeQ_W6zW5-OQDeTzrQXFhQC58x"

const char WIFI_SSID[] = "SAMRAT";         // WiFi SSID
const char WIFI_PASS[] = "SANKHYA007";     // WiFi password
/* ----------------------------------- */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Preferences.h>

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Fingerprint (HW Serial 2: RX=16, TX=17)
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger(&mySerial);

// Pins & timing
const int RELAY_PIN = 4;
const int BUTTON_PIN = 15;
const unsigned long UNLOCK_DURATION = 5000;
unsigned long unlockTime = 0;

// State
bool doorLocked = true;
bool enrolling = false;
Preferences prefs;
int selectedId = 1;
const int MAX_IDS = 200;

// Button handling
unsigned long buttonPressedAt = 0;
const unsigned long LONG_PRESS_MS = 2000;
const unsigned long BUTTON_DEBOUNCE_MS = 50;
unsigned long lastButtonChange = 0;
int lastButtonState = HIGH;

// App message timing
unsigned long appMsgTimer = 0;
const unsigned long MSG_DURATION = 3500;

// Forward declarations
void updateDisplay();
void showOLED(const String &a, const String &b = "", const String &c = "");
void showAppStatus(const String &msg);
void clearAppStatus();
void lockDoor();
void unlockDoor();
int getFingerprintID();
bool enrollFingerprint(uint16_t id);
bool idExists(uint16_t id);
bool deleteID(uint16_t id);
void startEnrollment();
void handleFingerprint();
void handleButton();
void populateStoredIDMenu();

// ---------------- setup ----------------
void setup() {
  Serial.begin(115200);
  delay(20);

  prefs.begin("doorlock", false);
  selectedId = prefs.getInt("selectedId", 1);
  if (selectedId < 1) selectedId = 1;
  if (selectedId > MAX_IDS) selectedId = MAX_IDS;

  // OLED
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
  } else {
    display.clearDisplay();
    display.display();
  }

  // Fingerprint UART
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);
  delay(1000);

  // Pins
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(RELAY_PIN, LOW); // Start locked
  doorLocked = true;

  // Check sensor
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor OK");
    showOLED("Fingerprint", "Sensor OK", "");
    showAppStatus("Sensor OK");
  } else {
    Serial.println("Fingerprint sensor not responding");
    showOLED("ERROR", "Check sensor", "");
    showAppStatus("Sensor not responding");
  }
  delay(800);
  updateDisplay();

  // Blynk
  if (strlen(BLYNK_AUTH_TOKEN) == 0 || strlen(WIFI_SSID) == 0) {
    Serial.println("Blynk/WiFi not configured — skipping Blynk");
    showAppStatus("Blynk not configured");
  } else {
    Serial.println("Connecting to Blynk...");
    Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);
  }

  populateStoredIDMenu();
}

// ---------------- loop ----------------
void loop() {
  handleButton();
  if (!enrolling) handleFingerprint();

  // Clear app status after MSG_DURATION
  if (appMsgTimer != 0 && millis() - appMsgTimer > MSG_DURATION) {
    clearAppStatus();
  }

  // Auto lock
  if (!doorLocked && (millis() - unlockTime > UNLOCK_DURATION)) {
    lockDoor();
    Serial.println("Auto re-locked");
    updateDisplay();
  }

  if (strlen(BLYNK_AUTH_TOKEN) != 0 && strlen(WIFI_SSID) != 0) {
    Blynk.run();
  }

  delay(30);
}

// ---------------- button handling ----------------
void handleButton() {
  int state = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  if (state != lastButtonState) lastButtonChange = now;
  if (now - lastButtonChange < BUTTON_DEBOUNCE_MS) {
    lastButtonState = state;
    return;
  }

  if (state == LOW) { // pressed
    if (buttonPressedAt == 0) buttonPressedAt = now;
    else if (now - buttonPressedAt >= LONG_PRESS_MS) {
      if (!doorLocked) {
        lockDoor();
        showAppStatus("Manual lock");
        updateDisplay();
      } else {
        showAppStatus("Already locked");
      }
      while (digitalRead(BUTTON_PIN) == LOW) delay(10);
      buttonPressedAt = 0;
      delay(150);
    }
  } else { // released
    if (buttonPressedAt != 0 && (now - buttonPressedAt > 30 && now - buttonPressedAt < LONG_PRESS_MS)) {
      startEnrollment();
    }
    buttonPressedAt = 0;
  }

  lastButtonState = state;
}

// ---------------- fingerprint handling ----------------
void handleFingerprint() {
  if (!finger.verifyPassword()) return;
  int id = getFingerprintID();
  if (id > 0) {
    Serial.print("Matched ID "); Serial.println(id);
    Blynk.virtualWrite(V3, id);
    Blynk.virtualWrite(V7, "Matched ID: " + String(id));

    if (doorLocked) {
      unlockDoor();
      showOLED("ACCESS GRANTED", "ID: " + String(id), "");
      showAppStatus("Matched ID: " + String(id));
      delay(700);
      updateDisplay();
    } else {
      lockDoor();
      showOLED("LOCKED", "Door Secured", "");
      showAppStatus("Locked");
      delay(400);
      updateDisplay();
    }
  } else if (id == 0) {
    Serial.println("No match");
    showOLED("ACCESS DENIED", "Not recognized", "");
    Blynk.virtualWrite(V3, -1);
    Blynk.virtualWrite(V7, "No match");
    showAppStatus("No match");
    delay(600);
    updateDisplay();
  }
}

// ---------------- enrollment ----------------
void startEnrollment() {
  if (selectedId < 1 || selectedId > MAX_IDS) {
    showOLED("ENROLL", "Invalid ID", "1..200");
    showAppStatus("Invalid ID");
    Blynk.virtualWrite(V7, "Invalid ID");
    return;
  }

  if (idExists((uint16_t)selectedId)) {
    showOLED("ENROLL", "ID exists", "Delete first");
    showAppStatus("ID exists — delete first");
    Blynk.virtualWrite(V7, "ID exists — delete first");
    return;
  }

  if (!finger.verifyPassword()) {
    showOLED("ERROR", "Sensor not ready", "");
    showAppStatus("Sensor not ready");
    Blynk.virtualWrite(V7, "Sensor not ready");
    return;
  }

  enrolling = true;
  showOLED("ENROLLMENT", "Starting...", "ID: " + String(selectedId));
  showAppStatus("Enroll ID: " + String(selectedId));
  Serial.print("Starting enrollment at ID "); Serial.println(selectedId);

  if (enrollFingerprint((uint16_t)selectedId)) {
    showOLED("SUCCESS", "Enrolled ID:", String(selectedId));
    showAppStatus("Enrolled ID: " + String(selectedId));
    Blynk.virtualWrite(V3, selectedId);
    Blynk.virtualWrite(V7, "Enrolled ID: " + String(selectedId));
    populateStoredIDMenu();
  } else {
    showOLED("FAILED", "Enrollment Failed", "");
    showAppStatus("Enrollment failed");
    Blynk.virtualWrite(V7, "Enrollment failed");
  }

  enrolling = false;
  updateDisplay();
}

// ---------------- helper functions ----------------
bool enrollFingerprint(uint16_t id) {
  int p = -1;
  unsigned long start;
  // First press
  showOLED("Enrollment", "Place finger (1)", "");
  start = millis();
  while (true) {
    p = finger.getImage();
    if (p == FINGERPRINT_OK) break;
    if (millis() - start > 12000) return false;
    delay(40);
  }
  if (finger.image2Tz(1) != FINGERPRINT_OK) return false;

  showOLED("Enrollment", "Remove finger", "");
  delay(1000);
  start = millis();
  while (finger.getImage() != FINGERPRINT_NOFINGER) {
    if (millis() - start > 8000) return false;
    delay(40);
  }

  // Second press
  showOLED("Enrollment", "Place same finger (2)", "");
  start = millis();
  while (true) {
    p = finger.getImage();
    if (p == FINGERPRINT_OK) break;
    if (millis() - start > 12000) return false;
    delay(40);
  }
  if (finger.image2Tz(2) != FINGERPRINT_OK) return false;
  if (finger.createModel() != FINGERPRINT_OK) return false;
  if (finger.storeModel(id) != FINGERPRINT_OK) return false;

  Serial.print("✅ Enrolled ID #"); Serial.println(id);
  return true;
}

bool idExists(uint16_t id) {
  return (finger.loadModel(id) == FINGERPRINT_OK);
}

bool deleteID(uint16_t id) {
  return (finger.deleteModel(id) == FINGERPRINT_OK);
}

void populateStoredIDMenu() {
  String csv = "";
  int count = 0;
  for (uint16_t i = 1; i <= MAX_IDS; ++i) {
    if (finger.loadModel(i) == FINGERPRINT_OK) {
      if (csv.length()) csv += ",";
      csv += String(i);
      ++count;
    }
    delay(5);
  }
  Blynk.virtualWrite(V4, csv); // Dropdown (stored IDs)
  Blynk.virtualWrite(V7, "Stored count: " + String(count));
  Serial.print("Stored templates count: "); Serial.println(count);
}

// ---------------- match ----------------
int getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p == FINGERPRINT_NOFINGER) return -1;
  if (p != FINGERPRINT_OK) return -1;
  if (finger.image2Tz() != FINGERPRINT_OK) return -1;
  if (finger.fingerFastSearch() != FINGERPRINT_OK) return 0;
  if (finger.confidence < 50) return 0;
  return finger.fingerID;
}

// ---------------- lock/unlock ----------------
void unlockDoor() {
  digitalWrite(RELAY_PIN, HIGH);
  doorLocked = false;
  unlockTime = millis();
  showAppStatus("Door unlocked");
  Blynk.virtualWrite(V1, "Unlocked");
  updateDisplay();
}

void lockDoor() {
  digitalWrite(RELAY_PIN, LOW);
  doorLocked = true;
  showAppStatus("Door locked");
  Blynk.virtualWrite(V1, "Locked");
  updateDisplay();
}

// ---------------- OLED ----------------
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("FINGERPRINT LOCK");
  display.println("----------------");
  display.print("Status: "); display.println(doorLocked ? "LOCKED" : "UNLOCKED");
  display.println("----------------");
  display.println("Scan finger to toggle");
  display.println("Short press = Enroll");
  display.println("Hold (2s) = Manual Lock");
  display.println("----------------");
  display.print("Selected ID: "); display.println(selectedId);
  display.display();
}

void showOLED(const String &a, const String &b, const String &c) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(a);
  if (b.length()) display.println(b);
  if (c.length()) display.println(c);
  display.display();
  delay(700); // readable pause for OLED messages
}

// ---------------- app status ----------------
void showAppStatus(const String &msg) {
  appMsgTimer = millis();
  Blynk.virtualWrite(V7, msg);
  Serial.println(msg);
}

void clearAppStatus() {
  appMsgTimer = 0;
  Blynk.virtualWrite(V7, "");
}

// ---------------- BLYNK handlers ----------------

// V0 - lock/unlock switch
BLYNK_WRITE(V0) {
  if (param.asInt() == 1) unlockDoor(); else lockDoor();
}

// V2 - start enrollment
BLYNK_WRITE(V2) {
  if (param.asInt() == 1 && !enrolling) startEnrollment();
  Blynk.virtualWrite(V2, 0);
}

// V5 - delete selected ID
BLYNK_WRITE(V5) {
  if (param.asInt() == 1) {
    if (idExists(selectedId)) {
      if (deleteID(selectedId)) showAppStatus("Deleted ID: " + String(selectedId));
      else showAppStatus("Delete failed");
      populateStoredIDMenu();
    } else showAppStatus("ID not found");
    Blynk.virtualWrite(V5, 0);
  }
}

// V6 - numeric input for selected ID
BLYNK_WRITE(V6) {
  int v = param.asInt();
  if (v < 1) v = 1; if (v > MAX_IDS) v = MAX_IDS;
  selectedId = v;
  prefs.putInt("selectedId", selectedId);
  showAppStatus("Selected ID: " + String(selectedId));
}
