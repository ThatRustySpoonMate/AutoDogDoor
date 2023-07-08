#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Define private macros
#define RELAY_PIN 4 // Pin for relay
#define LOCKOUT_SWITCH_PIN 17 // Pin for switch
#define LOCKOUT_SWITCH_LOCKED 0 // Switch is closed
#define LOCKOUT_SWITCH_UNLOCKED 1 // Switch is open

// Define Global Vars
int SCAN_DURATION = 2; //In seconds
int16_t RSSI_THRESHOLD = -80; // Threshold for RSSI value to determine if device is in range
BLEScan* pBLEScan;
uint32_t SCAN_INTERVAL = 50; // Time between scans in ms

// Allow for us to track if RSSI is increasing (Stronger signal = Ellie closer)
int previousRSSI = 0;
int currentRSSI = 0;
int RSSI_INC_THRESHOLD = 5; // Threshold for RSSI increase to determine if door should be opened
int RSSI_DOOR_OVERRIDE = -70; // Threshold for RSSI to determine if door should be opened

uint8_t door_status = 0x00; // 0x00 = closed, 0x01 = open

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
void lock_door();
//void IRAM_ATTR button_pressed();


// put function declarations here:

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Ellie Door running.");

  // Set up relay pin
  pinMode(RELAY_PIN, OUTPUT);
  // Set up button pin
  pinMode(LOCKOUT_SWITCH_PIN, INPUT_PULLUP);

  //attachInterrupt(BUTTON_PIN, button_pressed, HIGH);

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value

  setCpuFrequencyMhz(80);
  Serial.print("CPU: "); Serial.print(getCpuFrequencyMhz()); Serial.println("MHz");
  Serial.print("APB: "); Serial.print(getApbFrequency()); Serial.println("Hz");

}

void loop() {
  BLEScanResults foundDevices = pBLEScan->start(SCAN_DURATION, false);

  if(digitalRead(LOCKOUT_SWITCH_PIN) == LOCKOUT_SWITCH_LOCKED) {
    lock_door();
  }
  
  if(pinged) { // Ellie beacon was located
    Serial.print("Previous RSSI: "); Serial.println(previousRSSI);
    Serial.print("Current RSSI: "); Serial.println(currentRSSI);

    if(currentRSSI > RSSI_DOOR_OVERRIDE) {
      Serial.println("Ellie is close enough to open the door!");
      if(digitalRead(LOCKOUT_SWITCH_PIN) == LOCKOUT_SWITCH_UNLOCKED) {
        unlock_door();
      } 
    } else if(previousRSSI != 0 and currentRSSI - previousRSSI > RSSI_INC_THRESHOLD) {
      Serial.println("Ellie is getting closer!");
      if(digitalRead(LOCKOUT_SWITCH_PIN) == LOCKOUT_SWITCH_UNLOCKED) {
        unlock_door();
      }
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
  
  //digitalWrite(RELAY_PIN, HIGH);
  //delay(500);
  //digitalWrite(RELAY_PIN, LOW);

  /*
  if(door_status == 0x00) {
    // Open door
    Serial.println("Unlocking door!");
    door_status = 0x01;
  } */

  // Set timer for 10 seconds ?
  
  return;
}

void lock_door() {
  Serial.println("Locking door!");

  return;
}

/** Interrupt approach
void IRAM_ATTR button_pressed() {
  Serial.println("Button Pressed!");
} 
*/