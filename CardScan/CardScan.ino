#include <SPI.h>                 // SPI library for RC522 communication
#include <MFRC522.h>             // RFID RC522 library

#define SS_PIN  D2               // SDA / SS pin of RC522 connected to D2
#define RST_PIN D1               // RST pin of RC522 connected to D1

MFRC522 rfid(SS_PIN, RST_PIN);   // Create RFID object

String uidString = "";           // Variable to store UID as text

void setup() {
  Serial.begin(115200);          // Start Serial Monitor
  SPI.begin();                   // Initialize SPI bus (D5, D6, D7)
  rfid.PCD_Init();               // Initialize RFID reader
  Serial.println("Scan RFID card...");
}

void loop() {

  // Check if a new card is present
  if (!rfid.PICC_IsNewCardPresent())
    return;

  // Try to read the card UID
  if (!rfid.PICC_ReadCardSerial())
    return;

  uidString = "";                // Clear old UID value

  // Read UID bytes and store them into uidString
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10)
      uidString += "0";          // Add leading zero if needed
    uidString += String(rfid.uid.uidByte[i], HEX); // Convert byte to HEX
  }

  uidString.toUpperCase();       // Make UID uppercase (optional)

  // Display UID from the variable
  Serial.print("UID stored = ");
  Serial.println(uidString);

  rfid.PICC_HaltA();             // Stop communication with this card
  rfid.PCD_StopCrypto1();        // Stop encryption

  ///////////////////////////////////////////////////////////////////












  ////////////////////////////////////////////////////////////////////
}
