#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// RFID Pins
#define SS_PIN  5
#define RST_PIN 22

MFRC522 rfid(SS_PIN, RST_PIN);

// LCD: address 0x27, size 16x2
LiquidCrystal_I2C lcd(0x27, 16, 2);

// RFID UID → Name Mapping
struct User {
  const char* uid;
  const char* name;
};

User users[] = {
  {"F6 D0 7D 05", "24L31A04F6"},
  {"9F 84 58 28", "25L31A0419"},
  {"60 70 47 21", "25L35A0414"},
  {"80 35 EC 21", "25L35A0415"},
  {"9A 86 99 02", "25L35A0418"}
};

int usersCount = sizeof(users) / sizeof(users[0]);

// Convert UID to String
String getUID(MFRC522::Uid uid) {
  String uidStr = "";
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) uidStr += "0";
    uidStr += String(uid.uidByte[i], HEX);
    if (i < uid.size - 1) uidStr += " ";
  }
  uidStr.toUpperCase();
  return uidStr;
}

// Match UID to Name
String getUserName(String uid) {
  for (int i = 0; i < usersCount; i++) {
    if (uid == users[i].uid) {
      return users[i].name;
    }
  }
  return "Unknown";
}

void setup() {
  Serial.begin(115200);

  // RFID Init
  SPI.begin();
  rfid.PCD_Init();

  // LCD using custom I2C pins
  Wire.begin(21, 4);  // SDA=21, SCL=4

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("RFID Attendance");
  lcd.setCursor(0, 1);
  lcd.print("Place a Card...");
  
  Serial.println("Ready... Place RFID Card");
}

void loop() {
  // Wait for card
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Read UID
  String uid = getUID(rfid.uid);
  // Serial.print("Card UID: ");
  // Serial.println(uid);

  // Get Assigned Name
  String userName = getUserName(uid);
  Serial.print("HEY!");
  Serial.println(userName);

  // LCD Output
  lcd.clear();
  // lcd.setCursor(0, 0);
  // lcd.print("UID:");
  // lcd.setCursor(4, 0);
  // lcd.print(uid);

  lcd.setCursor(0, 0);
  lcd.print("Hey !");
  lcd.setCursor(6, 0);
  lcd.print(userName);

  delay(2500);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place Next Card");

  // Stop RFID
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
