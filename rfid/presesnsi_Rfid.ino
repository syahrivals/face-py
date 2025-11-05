/*
 * ESP32 Attendance System dengan RFID RC522 Dan FACE RECOGNITION OV2640
 * 
 * Wiring:
 * RFID RC522:
 * - SDA (SS)  -> GPIO 5
 * - SCK       -> GPIO 18
 * - MOSI      -> GPIO 23
 * - MISO      -> GPIO 19
 * - IRQ       -> Not connected
 * - GND       -> GND
 * - RST       -> GPIO 22
 * - 3.3V      -> 3.3V
 * 
 * Buzzer:
 * - Positif   -> GPIO 2
 * - Negatif   -> GND
 * 
 * LED Indikator (Opsional):
 * - LED Hijau -> GPIO 25 (Success)
 * - LED Merah -> GPIO 26 (Error)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>

// Pin definitions
#define SS_PIN 5
#define RST_PIN 22
#define BUZZER_PIN 2

// WiFi credentials
const char* ssid = "4737";
const char* password = "1sampai6";

// Server configuration
const char* serverURL = "http://10.110.171.150:8080/api/attendance/tap";
const String deviceId = "ESP32_001"; // Unique device identifier


// RFID setup
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Variables
unsigned long lastCardReadTime = 0;
const unsigned long CARD_READ_DELAY = 2000; // 2 seconds delay between reads
String lastCardUID = "";

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialize SPI and RFID
  SPI.begin(18, 19, 23, SS_PIN);
  delay(50);
  mfrc522.PCD_Init();
  delay(50);
  
  // Initialize WiFi
  setupWiFi();
  
  Serial.println("ESP32 Attendance System Ready!");
  Serial.println("Place RFID card near reader...");
  
  // Startup beep
  playStartupBeep();
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    setupWiFi();
    return;
  }
  
  // Check for RFID card
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String cardUID = getCardUID();
    
    // Prevent multiple reads of same card within delay period
    if (millis() - lastCardReadTime > CARD_READ_DELAY || cardUID != lastCardUID) {
      Serial.println("Card detected: " + cardUID);
      
      // Send to server
      sendAttendanceData(cardUID);
      
      lastCardReadTime = millis();
      lastCardUID = cardUID;
    }
    
    // Stop reading
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
  
  delay(100);
}

void setupWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

  } else {
    Serial.println();
    Serial.println("WiFi connection failed!");

  }
}

String getCardUID() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void sendAttendanceData(String rfidUID) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000); // 5 detik
    // Create JSON payload
    DynamicJsonDocument doc(1024);
    doc["rfid_uid"] = rfidUID;
    doc["device_id"] = deviceId;
    doc["timestamp"] = millis();
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    Serial.println("Sending data: " + jsonString);
    
    int httpResponseCode = http.POST(jsonString);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("HTTP Response: " + response);
      
      // Parse response
      DynamicJsonDocument responseDoc(1024);
      deserializeJson(responseDoc, response);
      
      bool success = responseDoc["success"];
      String message = responseDoc["message"];
      String buzzerType = responseDoc["buzzer"];
      
      Serial.println("Message: " + message);
      
      // Handle buzzer and LED feedback
      handleFeedback(success, buzzerType);
      
    } else {
      Serial.println("HTTP Error: " + String(httpResponseCode));
      playErrorBeep();

    }
    
    http.end();
  } else {
    Serial.println("WiFi not connected");
    playErrorBeep();

  }
}

void handleFeedback(bool success, String buzzerType) {
  if (buzzerType == "success") {
    playSuccessBeep();

  } else if (buzzerType == "warning") {
    playWarningBeep();

  } else if (buzzerType == "error") {
    playErrorBeep();

  }
}

// Buzzer functions
void playSuccessBeep() {
  // Double short beep for success
  tone(BUZZER_PIN, 1000, 200);
  delay(250);
  tone(BUZZER_PIN, 1200, 200);
  delay(300);
}

void playWarningBeep() {
  // Single long beep for warning
  tone(BUZZER_PIN, 800, 500);
  delay(600);
}

void playErrorBeep() {
  // Triple short beep for error
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 500, 150);
    delay(200);
  }
}

void playStartupBeep() {
  // Ascending tone for startup
  tone(BUZZER_PIN, 800, 200);
  delay(250);
  tone(BUZZER_PIN, 1000, 200);
  delay(250);
  tone(BUZZER_PIN, 1200, 300);
  delay(400);
}

// LED functions
void blinkLED(int pin, int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(200);
    digitalWrite(pin, LOW);
    delay(200);
  }
}

// Utility function untuk debug
void printWiFiStatus() {
  Serial.print("WiFi Status: ");
  Serial.println(WiFi.status());
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Signal Strength: ");
  Serial.println(WiFi.RSSI());
}