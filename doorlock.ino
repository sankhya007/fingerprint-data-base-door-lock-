/******************************************************
  Advanced ESP32 + R307S Fingerprint Door Lock with Database
  OPTIMIZED FOR INSTANT STARTUP with Last 3 Unlock Logs
******************************************************/

/* -------------- CONFIG -------------- */
#define BLYNK_TEMPLATE_ID   "TMPL3VcvkOhFq"
#define BLYNK_TEMPLATE_NAME "door lock esp "
#define BLYNK_AUTH_TOKEN    "_cEVyXFeQ_W6zW5-OQDeTzrQXFhQC58x"

const char WIFI_SSID[] = "SAMRAT";
const char WIFI_PASS[] = "SANKHYA007";
/* ----------------------------------- */

// ========== 1️⃣ INCLUDE REAL-TIME LIBRARY ==========
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>  // For std::sort
#include <time.h>     // For real-time clock - ADDED

WidgetTerminal terminal(V12);

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Fingerprint
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger(&mySerial);

// Pins
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

// Database structure
struct FingerprintRecord {
  uint16_t id;
  char name[32];
  unsigned long enrollmentTime;
  unsigned long lastAccessTime;
  uint32_t accessCount;
};

// Database management
std::vector<FingerprintRecord> fingerprintDB;
bool dbInitialized = false;

// Auto-refresh timing
unsigned long lastDBRefresh = 0;
const unsigned long DB_REFRESH_INTERVAL = 30000; // 30 seconds

// Sensor state
bool sensorReady = false;
unsigned long sensorCheckTime = 0;

// ========== 2️⃣ REPLACE WITH CIRCULAR BUFFER ==========
/* ---------------- Last 3 Unlock Logs ---------------- */
struct UnlockLog {
  uint16_t id;
  String name;
  String timeStr;  // Store actual unlock time
};

const int MAX_LOGS = 15;
UnlockLog lastUnlocks[MAX_LOGS];
int lastUnlockIndex = 0;

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
void initializeDatabase();
void saveDatabase();
void loadDatabase();
void addFingerprintRecord(uint16_t id, const String &name = "");
void updateAccessRecord(uint16_t id);
void deleteFingerprintRecord(uint16_t id);
String getFingerprintName(uint16_t id);
void setFingerprintName(uint16_t id, const String &name);
void syncFingerprintDatabase();
void sendDatabaseToBlynk();
void displayAllFingerprintsOnBlynk();
bool quickSensorCheck();
// ========== 3️⃣ ADD NEW FUNCTION DECLARATION ==========
void recordUnlockEvent(uint16_t id); // NEW: Record unlock logs
BLYNK_WRITE(V8); // Database sync command
BLYNK_WRITE(V9); // Get specific record
BLYNK_WRITE(V10); // Set fingerprint name
BLYNK_WRITE(V11); // Export database
BLYNK_WRITE(V13); // Display database button
BLYNK_WRITE(V14); // Show last unlock logs - UPDATED

// ---------------- Database Functions ----------------
void initializeDatabase() {
  if (!dbInitialized) {
    loadDatabase();
    syncFingerprintDatabase(); // Sync with physical sensor
    dbInitialized = true;
    Serial.println("Database initialized");
  }
}

void loadDatabase() {
  fingerprintDB.clear();
  prefs.begin("fpdb", true); // Read mode
  
  int count = prefs.getInt("count", 0);
  Serial.print("Loading database, count: "); Serial.println(count);
  
  for (int i = 0; i < count; i++) {
    FingerprintRecord record;
    String key = "fp_" + String(i);
    
    record.id = prefs.getUShort((key + "_id").c_str(), 0);
    
    // Get string - returns length of retrieved string
    size_t nameLen = prefs.getString((key + "_name").c_str(), record.name, sizeof(record.name));
    
    // If no name was stored or name is empty, use default
    if (nameLen == 0 || strlen(record.name) == 0) {
      snprintf(record.name, sizeof(record.name), "User_%d", record.id);
    }
    
    record.enrollmentTime = prefs.getULong((key + "_enroll").c_str(), 0);
    record.lastAccessTime = prefs.getULong((key + "_last").c_str(), 0);
    record.accessCount = prefs.getUInt((key + "_count").c_str(), 0);
    
    if (record.id > 0 && record.id <= MAX_IDS) {
      fingerprintDB.push_back(record);
    }
  }
  
  prefs.end();
}

void saveDatabase() {
  prefs.begin("fpdb", false); // Write mode
  
  // Clear all previous data
  prefs.clear();
  
  // Save count
  prefs.putInt("count", fingerprintDB.size());
  
  // Save each record
  for (size_t i = 0; i < fingerprintDB.size(); i++) {
    String key = "fp_" + String(i);
    const FingerprintRecord &record = fingerprintDB[i];
    
    prefs.putUShort((key + "_id").c_str(), record.id);
    
    // Save name (guaranteed to have a value from addFingerprintRecord)
    prefs.putString((key + "_name").c_str(), record.name);
    
    prefs.putULong((key + "_enroll").c_str(), record.enrollmentTime);
    prefs.putULong((key + "_last").c_str(), record.lastAccessTime);
    prefs.putUInt((key + "_count").c_str(), record.accessCount);
  }
  
  prefs.end();
  Serial.println("Database saved");
}

void addFingerprintRecord(uint16_t id, const String &name) {
  // Check if record already exists
  for (size_t i = 0; i < fingerprintDB.size(); i++) {
    if (fingerprintDB[i].id == id) {
      // Update existing record
      if (name.length() > 0) {
        name.toCharArray(fingerprintDB[i].name, sizeof(fingerprintDB[i].name));
      }
      fingerprintDB[i].enrollmentTime = millis();
      saveDatabase();
      return;
    }
  }
  
  // Create new record
  FingerprintRecord newRecord;
  newRecord.id = id;
  
  // Ensure name is never empty
  if (name.length() > 0) {
    name.toCharArray(newRecord.name, sizeof(newRecord.name));
  } else {
    snprintf(newRecord.name, sizeof(newRecord.name), "User_%d", id);
  }
  
  newRecord.enrollmentTime = millis();
  newRecord.lastAccessTime = 0;
  newRecord.accessCount = 0;
  
  fingerprintDB.push_back(newRecord);
  saveDatabase();
  
  Serial.print("Added fingerprint record: ID=");
  Serial.print(id);
  Serial.print(", Name=");
  Serial.println(newRecord.name);
}

void updateAccessRecord(uint16_t id) {
  for (size_t i = 0; i < fingerprintDB.size(); i++) {
    if (fingerprintDB[i].id == id) {
      fingerprintDB[i].lastAccessTime = millis();
      fingerprintDB[i].accessCount++;
      saveDatabase();
      return;
    }
  }
  
  // If ID not in database, add it
  addFingerprintRecord(id, "");
  updateAccessRecord(id); // Recursive call to update the newly added record
}

void deleteFingerprintRecord(uint16_t id) {
  for (auto it = fingerprintDB.begin(); it != fingerprintDB.end(); ++it) {
    if (it->id == id) {
      fingerprintDB.erase(it);
      saveDatabase();
      Serial.print("Deleted fingerprint record: ID=");
      Serial.println(id);
      return;
    }
  }
}

String getFingerprintName(uint16_t id) {
  for (size_t i = 0; i < fingerprintDB.size(); i++) {
    if (fingerprintDB[i].id == id) {
      return String(fingerprintDB[i].name);
    }
  }
  return "Unknown_" + String(id);
}

void setFingerprintName(uint16_t id, const String &name) {
  for (size_t i = 0; i < fingerprintDB.size(); i++) {
    if (fingerprintDB[i].id == id) {
      name.toCharArray(fingerprintDB[i].name, sizeof(fingerprintDB[i].name));
      saveDatabase();
      
      // Update Blynk dropdown
      populateStoredIDMenu();
      
      Serial.print("Updated name for ID ");
      Serial.print(id);
      Serial.print(" to: ");
      Serial.println(name);
      return;
    }
  }
  
  // If ID not found, create new record
  addFingerprintRecord(id, name);
}

void syncFingerprintDatabase() {
  Serial.println("Syncing database with fingerprint sensor...");
  
  // First, check which IDs exist in the sensor
  for (uint16_t id = 1; id <= MAX_IDS; id++) {
    if (finger.loadModel(id) == FINGERPRINT_OK) {
      // Check if this ID exists in our database
      bool found = false;
      for (size_t i = 0; i < fingerprintDB.size(); i++) {
        if (fingerprintDB[i].id == id) {
          found = true;
          break;
        }
      }
      
      // If not found, add to database
      if (!found) {
        addFingerprintRecord(id, "");
        Serial.print("Added missing ID from sensor: ");
        Serial.println(id);
      }
    } else {
      // Check if we have this ID in database but not in sensor
      for (auto it = fingerprintDB.begin(); it != fingerprintDB.end(); ++it) {
        if (it->id == id) {
          // ID in database but not in sensor - remove from database
          fingerprintDB.erase(it);
          saveDatabase();
          Serial.print("Removed orphaned ID from database: ");
          Serial.println(id);
          break;
        }
      }
    }
    delay(5);
  }
  
  Serial.print("Database sync complete. Total records: ");
  Serial.println(fingerprintDB.size());
}

// ========== 3️⃣ REPLACE recordUnlockEvent() FUNCTION ==========
void recordUnlockEvent(uint16_t id) {
    if (id == 0 || id > MAX_IDS) {
        Serial.println("Invalid fingerprint ID, skipping log.");
        return;
    }

    // Store ID and name
    lastUnlocks[lastUnlockIndex].id = id;
    lastUnlocks[lastUnlockIndex].name = getFingerprintName(id);

    // Get current time safely
    time_t now = time(nullptr);
    if (now == ((time_t)-1)) {
        Serial.println("Failed to obtain current time");
        lastUnlocks[lastUnlockIndex].timeStr = "Unknown";
    } else {
        struct tm timeinfo;
        if (!localtime_r(&now, &timeinfo)) {
            Serial.println("localtime conversion failed");
            lastUnlocks[lastUnlockIndex].timeStr = "Unknown";
        } else {
            char buffer[32];
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
            lastUnlocks[lastUnlockIndex].timeStr = String(buffer);
        }
    }

    // Increment circular log index
    lastUnlockIndex = (lastUnlockIndex + 1) % MAX_LOGS;

    // Optional debug print
    Serial.printf("Unlock recorded: ID=%d, Name=%s, Time=%s\n",
                  lastUnlocks[lastUnlockIndex].id,
                  lastUnlocks[lastUnlockIndex].name.c_str(),
                  lastUnlocks[lastUnlockIndex].timeStr.c_str());
}


// ========== QUICK SENSOR CHECK ==========
bool quickSensorCheck() {
  // Quick check - try to read sensor status
  uint8_t p = finger.getImage();
  if (p == FINGERPRINT_OK || p == FINGERPRINT_NOFINGER || p == FINGERPRINT_PACKETRECIEVEERR) {
    return true; // Sensor is responding
  }
  
  // Fallback to password verification
  return finger.verifyPassword();
}

// ========== TERMINAL DATABASE DISPLAY ==========
void displayAllFingerprintsOnBlynk() {
  terminal.clear();
  delay(200);

  if (fingerprintDB.size() == 0) {
    terminal.println("No users registered");
    terminal.flush();
    return;
  }

  terminal.println("===== FINGERPRINT DATABASE =====");

  // Sort the DB
  std::vector<FingerprintRecord> sortedDB = fingerprintDB;
  std::sort(sortedDB.begin(), sortedDB.end(),
            [](const FingerprintRecord &a, const FingerprintRecord &b) {
              return a.id < b.id;
            });

  for (auto &rec : sortedDB) {
    terminal.print("ID: ");
    terminal.print(rec.id);
    terminal.print(" | Name: ");
    terminal.println(rec.name);
  }

  terminal.println("================================");
  terminal.flush();
}

void sendDatabaseToBlynk() {
  displayAllFingerprintsOnBlynk();
}

// ---------------- OPTIMIZED SETUP (INSTANT STARTUP) ----------------
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Fingerprint Lock Booting ===");

    // Initialize unlock logs array
    for (int i = 0; i < MAX_LOGS; i++) {
        lastUnlocks[i].id = 0;
        lastUnlocks[i].name = "";
        lastUnlocks[i].timeStr = "";
    }

    // 1. INSTANT OLED DISPLAY (100ms)
    Wire.begin(21, 22);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED init failed");
    } else {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        display.println("FINGERPRINT LOCK");
        display.println("Initializing...");
        display.display();
    }

    // 2. INITIALIZE PINS (instant)
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(RELAY_PIN, LOW);
    doorLocked = true;

    // 3. START FINGERPRINT SENSOR IN BACKGROUND (non-blocking)
    mySerial.begin(57600, SERIAL_8N1, 16, 17);
    finger.begin(57600);
    sensorCheckTime = millis(); // Start sensor check timer

    // 4. LOAD DATABASE (fast)
    initializeDatabase();

    // 5. LOAD SELECTED ID
    prefs.begin("doorlock", false);
    selectedId = prefs.getInt("selectedId", 1);
    if (selectedId < 1) selectedId = 1;
    if (selectedId > MAX_IDS) selectedId = MAX_IDS;
    prefs.end();

    // 6. CONNECT TO WIFI (non-blocking)
    if (strlen(WIFI_SSID) > 0 && strlen(WIFI_PASS) > 0) {
        Serial.print("Connecting to WiFi");
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        unsigned long startWiFi = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startWiFi < 5000) {
            delay(500);
            Serial.print(".");
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected: " + String(WiFi.localIP()));

            // 7. CONFIGURE NTP
            configTime(19800, 0, "pool.ntp.org"); // IST: UTC+5:30
            Serial.print("Syncing time with NTP");
            time_t now = time(nullptr);
            unsigned long startTime = millis();
            while (now < 1672502400 && millis() - startTime < 5000) { // Wait until 2023+
                delay(500);
                Serial.print(".");
                now = time(nullptr);
            }
            if (now >= 1672502400) {
                Serial.println("\nTime synchronized");
            } else {
                Serial.println("\nNTP sync failed, using default time");
            }
        } else {
            Serial.println("\nWiFi connection failed");
        }
    }

    // 8. SHOW READY SCREEN IMMEDIATELY
    updateDisplay();
    showAppStatus("System ready");

    // 9. START BLYNK IN BACKGROUND (non-blocking)
    if (strlen(BLYNK_AUTH_TOKEN) != 0 && strlen(WIFI_SSID) != 0) {
        Serial.println("Starting Blynk connection...");
        Blynk.config(BLYNK_AUTH_TOKEN);
    }

    Serial.println("=== System Ready (Instant) ===");
}




// ---------------- OPTIMIZED LOOP ----------------
void loop() {
  handleButton();
  
  // NON-BLOCKING SENSOR CHECK (runs in background)
  if (!sensorReady) {
    if (millis() - sensorCheckTime > 500) { // Check every 500ms
      if (quickSensorCheck()) {
        sensorReady = true;
        Serial.println("✓ Fingerprint sensor ready");
        showAppStatus("Sensor ready");
      } else if (millis() > 5000) { // Give up after 5 seconds
        sensorReady = true; // Continue without sensor
        Serial.println("⚠ Continuing without sensor");
        showAppStatus("Sensor offline - will retry");
      }
      sensorCheckTime = millis();
    }
  }
  
  // Only process fingerprints if sensor is ready
  if (!enrolling && sensorReady) {
    handleFingerprint();
  }
  
  // Clear app status after MSG_DURATION
  if (appMsgTimer != 0 && millis() - appMsgTimer > MSG_DURATION) {
    clearAppStatus();
  }

  // Auto lock
  if (!doorLocked && (millis() - unlockTime > UNLOCK_DURATION)) {
    lockDoor();
    updateDisplay();
  }

  // Auto-refresh database display every 30 seconds
  if (millis() - lastDBRefresh > DB_REFRESH_INTERVAL) {
    if (Blynk.connected()) {
      displayAllFingerprintsOnBlynk();
      lastDBRefresh = millis();
    }
  }

  // Run Blynk (non-blocking)
  if (strlen(BLYNK_AUTH_TOKEN) != 0 && strlen(WIFI_SSID) != 0) {
    Blynk.run();
  }

  delay(30); // Small delay for stability
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
  if (!sensorReady) return;
  
  int id = getFingerprintID();
  if (id > 0) {
    Serial.print("Matched ID "); Serial.println(id);
    
    // Update access record
    updateAccessRecord(id);
    
    // Get fingerprint name
    String userName = getFingerprintName(id);
    
    // Send to Blynk
    Blynk.virtualWrite(V3, id);
    Blynk.virtualWrite(V7, "User: " + userName);
    
    // ========== 5️⃣ UPDATE TO CALL NEW FUNCTION ==========
    // Record unlock event (only when door is locked and gets unlocked)
    if (doorLocked) {
      recordUnlockEvent(id); // stores in circular buffer with real-time timestamp
    }
    
    // Auto-refresh database display
    if (Blynk.connected()) {
      displayAllFingerprintsOnBlynk();
    }
    
    if (doorLocked) {
      unlockDoor();
      showOLED("ACCESS GRANTED", userName, "ID: " + String(id));
      showAppStatus(userName + " entered");
      delay(700);
      updateDisplay();
    } else {
      lockDoor();
      showOLED("LOCKED", userName + " exited", "");
      showAppStatus(userName + " exited");
      delay(400);
      updateDisplay();
    }
  } else if (id == 0) {
    Serial.println("No match");
    showOLED("ACCESS DENIED", "Not recognized", "");
    Blynk.virtualWrite(V3, -1);
    Blynk.virtualWrite(V7, "No match - Unknown user");
    showAppStatus("No match");
    delay(600);
    updateDisplay();
  }
}

// ---------------- enrollment ----------------
void startEnrollment() {
  if (!sensorReady) {
    showOLED("SENSOR", "Not ready", "Please wait");
    showAppStatus("Sensor not ready");
    return;
  }
  
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

  enrolling = true;
  showOLED("ENROLLMENT", "Starting...", "ID: " + String(selectedId));
  showAppStatus("Enroll ID: " + String(selectedId));
  Serial.print("Starting enrollment at ID "); Serial.println(selectedId);

  if (enrollFingerprint((uint16_t)selectedId)) {
    // Add to database
    addFingerprintRecord(selectedId, "User_" + String(selectedId));
    
    showOLED("SUCCESS", "Enrolled ID:", String(selectedId));
    showAppStatus("Enrolled ID: " + String(selectedId));
    Blynk.virtualWrite(V3, selectedId);
    Blynk.virtualWrite(V7, "Enrolled ID: " + String(selectedId));
    
    // Update menus and refresh display
    populateStoredIDMenu();
    if (Blynk.connected()) {
      displayAllFingerprintsOnBlynk();
    }
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
  if (!sensorReady) return false;
  return (finger.loadModel(id) == FINGERPRINT_OK);
}

bool deleteID(uint16_t id) {
  if (!sensorReady) return false;
  bool success = (finger.deleteModel(id) == FINGERPRINT_OK);
  if (success) {
    deleteFingerprintRecord(id);
    if (Blynk.connected()) {
      displayAllFingerprintsOnBlynk(); // Refresh display
    }
  }
  return success;
}

void populateStoredIDMenu() {
  if (!sensorReady) return;
  
  String csv = "";
  int count = 0;
  for (uint16_t i = 1; i <= MAX_IDS; ++i) {
    if (finger.loadModel(i) == FINGERPRINT_OK) {
      if (csv.length()) csv += ",";
      
      // Get name from database
      String userName = getFingerprintName(i);
      csv += String(i) + ":" + userName;
      
      ++count;
    }
    delay(5);
  }
  if (Blynk.connected()) {
    Blynk.virtualWrite(V4, csv); // Dropdown (stored IDs)
    Blynk.virtualWrite(V7, "Database: " + String(fingerprintDB.size()) + " users");
  }
  Serial.print("Stored templates: "); Serial.println(count);
}

// ---------------- match ----------------
int getFingerprintID() {
  if (!sensorReady) return -1;
  
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
  if (Blynk.connected()) {
    Blynk.virtualWrite(V1, "Unlocked");
  }
  updateDisplay();
}

void lockDoor() {
  digitalWrite(RELAY_PIN, LOW);
  doorLocked = true;
  showAppStatus("Door locked");
  if (Blynk.connected()) {
    Blynk.virtualWrite(V1, "Locked");
  }
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
  display.print("Users: "); display.println(fingerprintDB.size());
  display.print("Sensor: "); display.println(sensorReady ? "OK" : "Wait");
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
  delay(700);
}

// ---------------- app status ----------------
void showAppStatus(const String &msg) {
  appMsgTimer = millis();
  if (Blynk.connected()) {
    Blynk.virtualWrite(V7, msg);
  }
  Serial.println(msg);
}

void clearAppStatus() {
  appMsgTimer = 0;
  if (Blynk.connected()) {
    Blynk.virtualWrite(V7, "");
  }
}

// ========== BLYNK WIDGET HANDLERS ==========

// V8 - Sync database command
BLYNK_WRITE(V8) {
  if (param.asInt() == 1) {
    syncFingerprintDatabase();
    displayAllFingerprintsOnBlynk();
    Blynk.virtualWrite(V7, "Database synced");
    Blynk.virtualWrite(V8, 0); // Reset button
  }
}

// V9 - Get specific user info
BLYNK_WRITE(V9) {
  int id = param.asInt();
  if (id > 0 && id <= MAX_IDS) {
    for (size_t i = 0; i < fingerprintDB.size(); i++) {
      if (fingerprintDB[i].id == id) {
        String info = "ID: " + String(id) + "\n";
        info += "Name: " + String(fingerprintDB[i].name) + "\n";
        info += "Access Count: " + String(fingerprintDB[i].accessCount) + "\n";
        
        if (fingerprintDB[i].lastAccessTime > 0) {
          unsigned long secondsAgo = (millis() - fingerprintDB[i].lastAccessTime) / 1000;
          info += "Last Access: " + String(secondsAgo) + " seconds ago";
        } else {
          info += "Last Access: Never";
        }
        
        Blynk.virtualWrite(V7, info);
        return;
      }
    }
    Blynk.virtualWrite(V7, "ID " + String(id) + " not found in database");
  }
}

// V10 - Set fingerprint name (format: "ID:Name")
BLYNK_WRITE(V10) {
  String data = param.asStr();
  int colonIndex = data.indexOf(':');
  
  if (colonIndex > 0) {
    int id = data.substring(0, colonIndex).toInt();
    String name = data.substring(colonIndex + 1);
    
    if (id > 0 && id <= MAX_IDS && name.length() > 0) {
      setFingerprintName(id, name);
      Blynk.virtualWrite(V7, "Name set for ID " + String(id) + ": " + name);
      displayAllFingerprintsOnBlynk(); // Refresh display
    } else {
      Blynk.virtualWrite(V7, "Invalid format. Use: ID:Name (ID 1-200)");
    }
  } else {
    Blynk.virtualWrite(V7, "Invalid format. Use: ID:Name (Example: 3:John)");
  }
}

// V11 - Export database (trigger)
BLYNK_WRITE(V11) {
  if (param.asInt() == 1) {
    displayAllFingerprintsOnBlynk();
    Blynk.virtualWrite(V7, "Database exported to display");
    Blynk.virtualWrite(V11, 0);
  }
}

// V13 - SHOW DATABASE BUTTON
BLYNK_WRITE(V13) {
  if (param.asInt() == 1) {
    displayAllFingerprintsOnBlynk();
    Blynk.virtualWrite(V13, 0); // Reset button
  }
}

// ========== 4️⃣ UPDATE V14 HANDLER TO SHOW LAST 3 USERS ==========
// V14 - SHOW LAST 3 UNLOCK LOGS
BLYNK_WRITE(V14) {
  if (param.asInt() == 1) {
    terminal.clear();
    delay(200);

    bool anyLogged = false;
    terminal.println("===== LAST 10 UNLOCKS =====");
    for (int i = 0; i < MAX_LOGS; i++) {
      int idx = (lastUnlockIndex + i) % MAX_LOGS;
      if (lastUnlocks[idx].id != 0) {
        // Get current real-time
        time_t now;
        time(&now);
        struct tm *timeinfo = localtime(&now);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

        terminal.print("ID: "); terminal.print(lastUnlocks[idx].id);
        terminal.print(" | Name: "); terminal.print(lastUnlocks[idx].name);
        terminal.print(" | Time: "); terminal.println(lastUnlocks[idx].timeStr);

        anyLogged = true;
      }
    }

    if (!anyLogged) terminal.println("No unlock events recorded yet");
    terminal.println("==========================");
    terminal.flush();
    Blynk.virtualWrite(V14, 0);
  }
}



// ---------------- EXISTING BLYNK handlers ----------------

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
      if (deleteID(selectedId)) {
        showAppStatus("Deleted ID: " + String(selectedId));
        displayAllFingerprintsOnBlynk();
        populateStoredIDMenu();
      } else {
        showAppStatus("Delete failed");
      }
    } else {
      showAppStatus("ID not found");
    }
    Blynk.virtualWrite(V5, 0);
  }
}

// V6 - numeric input for selected ID
BLYNK_WRITE(V6) {
  int v = param.asInt();
  if (v < 1) v = 1; if (v > MAX_IDS) v = MAX_IDS;
  selectedId = v;
  
  prefs.begin("doorlock", false);
  prefs.putInt("selectedId", selectedId);
  prefs.end();
  
  showAppStatus("Selected ID: " + String(selectedId));
}