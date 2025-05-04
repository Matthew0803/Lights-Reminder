#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

// Wi-Fi Credentials
const char* ssid = "WIFI_NAME";
const char* password = "WIFI_PASSWORD";

// Telegram Bot
#define BOT_TOKEN "BOT_TOKEN"
#define CHAT_ID "CHAT_ID"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// BLE Settings
const char* TARGET_MAC = "TARGET_MAC";
const int RSSI_THRESHOLD = -65;
bool phoneInRange = false;
BLEScan* pBLEScan;

// Timing Controls
unsigned long lastBLEScan = 0;
const long BLE_INTERVAL = 5000;  // 5 seconds between BLE scans
unsigned long lastMsgCheck = 0;
const long MSG_CHECK_INTERVAL = 1000;  // 1 second between status checks

// Pins
const int photoPin = 34;

// Light Threshold
int lightThreshold = 2000;

void setup() {
  Serial.begin(115200);
  connectToWiFi();
  // Initialize BLE only once
  BLEDevice::init("ESP32 Light Monitor");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);  // Lower scan frequency
  pBLEScan->setWindow(99);     // Reduce active scanning time
}

void loop() {
  unsigned long currentMillis = millis();

  // Non-blocking BLE scan
  if (currentMillis - lastBLEScan >= BLE_INTERVAL) {
    checkBLEProximity();
    lastBLEScan = currentMillis;
  }

  // Non-blocking status check
  if (currentMillis - lastMsgCheck >= MSG_CHECK_INTERVAL) {
    checkLightStatus();
    lastMsgCheck = currentMillis;
  }
}

void checkBLEProximity() {
  int strongestRSSI = -100;
  BLEScanResults* foundDevices = pBLEScan->start(2);

  for (int i = 0; i < foundDevices->getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices->getDevice(i);
    // Check for Apple devices (Company ID 0x004C)
    if (device.haveManufacturerData()) {
      String data = device.getManufacturerData();
      if (data.length() >= 2) {
        uint16_t company_id = (data[1] << 8) | data[0];
        if (company_id == 0x004C) {      
          if (device.getRSSI() > strongestRSSI) {
            strongestRSSI = device.getRSSI();
          }
        }
      }
    }
  }
  
  phoneInRange = (strongestRSSI > RSSI_THRESHOLD);
  Serial.print("Strongest iPhone RSSI: ");
  Serial.println(strongestRSSI);
  pBLEScan->clearResults();
}

void checkLightStatus() {
  int lightValue = analogRead(photoPin);
  
  if(!phoneInRange && lightValue > lightThreshold) {
    // Stop BLE before sending message
    BLEDevice::deinit();
    delay(3000);
    bool success = bot.sendMessage(CHAT_ID, "Lights are ON, and you're away!", "");
    Serial.println(success ? "Message sent" : "Message failed");
    
    // Restart BLE after message
    BLEDevice::init("ESP32 Light Monitor");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
  }
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
}
