#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  D4      // SDA
#define RST_PIN D3

MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("Scan an RFID Tag to read UID...");
}

void loop() {
  // Look for card
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print("UID: ");

  String uidString = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    Serial.print(rfid.uid.uidByte[i], HEX);

    uidString += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    uidString += String(rfid.uid.uidByte[i], HEX);

    if (i < rfid.uid.size - 1) {
      Serial.print(":");
      uidString += ":";
    }
  }

  Serial.println();
  Serial.println("UID String: " + uidString);
  Serial.println("------------------------");

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(1000);
}


// UID: 9A:86:99:02
//UID String: 9a:86:99:02 tag k
// UID: 60:70:47:21
// UID String: 60:70:47:21 tag chirman
//UID: 80:35:EC:21
//UID String: 80:35:ec:21 tag principal
//UID: 9F:84:58:28
//UID String: 9f:84:58:28 tag ceo
//UID: F6:D0:7D:05
// UID String: f6:d0:7d:05 tag cars
