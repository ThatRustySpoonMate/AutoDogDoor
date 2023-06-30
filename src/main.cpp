#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Define Global Vars
int SCAN_DURATION = 2; //In seconds
int16_t RSSI_THRESHOLD = -80; // Threshold for RSSI value to determine if device is in range
BLEScan* pBLEScan;
uint32_t SCAN_INTERVAL = 50; // Time between scans in ms

// Allow for us to track if RSSI is increasing (Stronger signal = Ellie closer)
int previousRSSI = 0;
int currentRSSI = 0;
int RSSI_INC_THRESHOLD = 5; // Threshold for RSSI increase to determine if door should be opened
int RSSI_DOOR_OVERRIDE = -75; // Threshold for RSSI increase to determine if door should be opened

// Flags when ellie beacon is found
bool pinged = false;
uint32_t consecutiveFalsePings = 0;

// Define classes
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if(advertisedDevice.getName() == "Ellie") {
        currentRSSI = advertisedDevice.getRSSI();
        pinged = true;
        consecutiveFalsePings = 0;
      }
    }
};

// Declare Functions
void unlock_door();


// put function declarations here:

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Ellie Door running.");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value
}

void loop() {
  // put your main code here, to run repeatedly:
  BLEScanResults foundDevices = pBLEScan->start(SCAN_DURATION, false);
  
  if(pinged) { // Ellie beacon was located
    Serial.print("Previous RSSI: "); Serial.println(previousRSSI);
    Serial.print("Current RSSI: "); Serial.println(currentRSSI);

    if(currentRSSI > RSSI_DOOR_OVERRIDE) {
      Serial.println("Ellie is close enough to open the door!");
      // unlock_door() 
    } else if(previousRSSI != 0 and currentRSSI - previousRSSI > RSSI_INC_THRESHOLD) {
      Serial.println("Ellie is getting closer!");
      // unlock_door() 
    } else if(previousRSSI != 0 and currentRSSI - previousRSSI < 0) {
      Serial.println("Ellie is getting further away!");
    } else if(previousRSSI != 0){
      Serial.println("Ellie is the same distance away!");
    }

    previousRSSI = currentRSSI;
    pinged = false;
  } else {
    consecutiveFalsePings++;
  }

  if(consecutiveFalsePings > 10) { // 10 scans in a row did not find ellie
    previousRSSI = 0;
  }

  pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
  delay(SCAN_INTERVAL);
}


void unlock_door(){
  Serial.println("Unlocking door!");

  return;
}