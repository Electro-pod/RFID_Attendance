/* Fast ESP32 RFID -> Firebase Realtime DB with Buzzer
   - MFRC522 (SPI)
   - LiquidCrystal_I2C (I2C)
   - Posts JSON to: https://attendance-1edea-default-rtdb.firebaseio.com/attendance.json
   - No Firebase library, uses HTTPS REST API (fast compile)
   - Buzzer indicates upload success/failure
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include "time.h"

// ========== USER CONFIG ==========
#define WIFI_SSID     "Nothing"
#define WIFI_PASSWORD "123456789"

// Use your Firebase Realtime Database URL (no trailing slash)
const char FIREBASE_DB[] = "https://attendance-1edea-default-rtdb.firebaseio.com";

// NTP / timezone (IST)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800; // +5:30
const int   daylightOffset_sec = 0;

// MFRC522 pins (ESP32)
#define SS_PIN 5
#define RST_PIN 22
MFRC522 rfid(SS_PIN, RST_PIN);

// I2C LCD (common address 0x27 or 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Buzzer pin
#define BUZZER_PIN 17

// ========== HELPERS ==========

// Map UID -> human name (add more entries as needed)
String getNameFromUID(const String &uid) {
  String u = uid;
  u.toUpperCase();
  if (u == "F6D07D05") return "24L31A04F6";
  if (u == "9F845828") return "25L35A0414";
  if (u == "60704721") return "25L35A0415";
  if (u == "8035EC21") return "25L35A0418";
  if (u == "9A869902") return "25L35A0419";

  // add more below
  return "Unknown";
}

// Convert MFRC522 UID to uppercase hex string (2 chars per byte)
String uidToString(MFRC522::Uid &uid) {
  String s = "";
  for (byte i = 0; i < uid.size; i++) {
    char tmp[3];
    sprintf(tmp, "%02X", uid.uidByte[i]);
    s += String(tmp);
  }
  return s;
}

// Get date (YYYY-MM-DD)
String getDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "NA";
  char buf[20];
  strftime(buf, sizeof(buf), "%Y-%m-%d", &timeinfo);
  return String(buf);
}

// Get time (HH:MM:SS)
String getTimeNow() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "NA";
  char buf[20];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  return String(buf);
}

// Connect to WiFi (non-blocking friendly retry)
void wifiConnect() {
  if (WiFi.status() == WL_CONNECTED) return;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    lcd.setCursor(0,1);
    lcd.print("Please wait...   ");
    delay(500);
    // avoid forever lock — show message and continue trying
    if (millis() - start > 20000) {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("WiFi failed");
      lcd.setCursor(0,1);
      lcd.print("Check credentials");
      delay(2000);
      start = millis();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WiFi Connected");
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP().toString());
  delay(1000);
}

// Send JSON to Firebase Realtime DB (POST to /attendance.json -> creates unique key)
bool sendToFirebase(const String &name, const String &date, const String &timeS, const String &uidStr) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi");
    return false;
  }

  String firebaseEndpoint = String(FIREBASE_DB);
  if (firebaseEndpoint.endsWith("/")) firebaseEndpoint.remove(firebaseEndpoint.length() - 1);
  firebaseEndpoint += "/attendance.json";

  // Build JSON payload
  String payload = "{";
  payload += "\"name\":\"" + name + "\","; 
  payload += "\"uid\":\"" + uidStr + "\","; 
  payload += "\"date\":\"" + date + "\","; 
  payload += "\"time\":\"" + timeS + "\""; 
  payload += "}";

  Serial.println("Firebase endpoint: " + firebaseEndpoint);
  Serial.println("Payload: " + payload);

  WiFiClientSecure client;
  client.setInsecure(); // skips cert verification

  HTTPClient https;
  bool success = false;
  if (https.begin(client, firebaseEndpoint)) {
    https.addHeader("Content-Type", "application/json");
    int httpCode = https.POST(payload);
    Serial.print("HTTP code: ");
    Serial.println(httpCode);
    if (httpCode > 0) {
      String resp = https.getString();
      Serial.println("Response: ");
      Serial.println(resp);
      success = (httpCode == 200 || httpCode == 201 || httpCode == 204);
    } else {
      Serial.print("POST failed, error: ");
      Serial.println(httpCode);
    }
    https.end();
  } else {
    Serial.println("HTTP begin failed");
  }
  return success;
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);

  // Initialize pins
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // I2C LCD
  Wire.begin(21, 4);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("RFID Attendance");
  lcd.setCursor(0,1);
  lcd.print("Initializing...");
  delay(900);
  lcd.clear();

  // RFID init
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("RFID initialized");

  // WiFi
  wifiConnect();

  // NTP setup
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("NTP config done");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Time syncing...");
  delay(800);

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Ready to Scan");
  lcd.setCursor(0,1);
  lcd.print("Place your Card");
}

// ========== MAIN LOOP ==========
void loop() {
  // Ensure WiFi
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnect();
  }

  // look for a new card
  if (!rfid.PICC_IsNewCardPresent()) {
    delay(50);
    return;
  }
  if (!rfid.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }

  // Build UID string
  String uidStr = uidToString(rfid.uid);
  Serial.print("Card UID: ");
  Serial.println(uidStr);

  // Map to student name
  String studentName = getNameFromUID(uidStr);

  // Date & Time
  String dateStr = getDate();
  String timeStr = getTimeNow();

  // Show on LCD
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Name: ");
  lcd.print(studentName);
  lcd.setCursor(0,1);
  lcd.print(timeStr);

  // If unknown, double beep
  if (studentName == "Unknown") {
    for(int i=0;i<2;i++){
      digitalWrite(BUZZER_PIN, HIGH);
      delay(200);
      digitalWrite(BUZZER_PIN, LOW);
      delay(100);
    }
  }

  // Send to Firebase
  bool ok = sendToFirebase(studentName, dateStr, timeStr, uidStr);

  if (ok) {
    // Success: short beep
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Uploaded");
    lcd.setCursor(0,1);
    lcd.print(studentName);
  } else {
    // Failure: long beep
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Upload Failed");
    lcd.setCursor(0,1);
    lcd.print("Check logs");
  }

  // Cleanup
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  delay(1500);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Ready to Scan");
  lcd.setCursor(0,1);
  lcd.print("Place your Card");
}
