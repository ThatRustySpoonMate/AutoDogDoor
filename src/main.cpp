#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "main.h"

/* Define Global Vars */

/* Configuration variables */
int SCAN_DURATION = 1; //In seconds
uint32_t SCAN_INTERVAL = 1; // Time between scans in seconds
int RSSI_INC_THRESHOLD = 5; // If RSSI increases by this amount between pings, door should be opened
int RSSI_DOOR_OVERRIDE = -70; // Threshold for RSSI to determine if door should be opened
uint32_t door_open_time = 10; // Time in seconds that door should be open for

/* Temporal variables */
int previousRSSI = 0; // Allow for us to track if RSSI is increasing (Stronger signal = Ellie closer)
int currentRSSI = 0;
uint8_t door_status = door_closed; // 0x00 = closed, 0x01 = open
uint32_t time_of_door_open = 0; // Time in seconds that door was opened
bool lockout_engaged = false;
uint32_t consecutiveFalsePings = 0;

BLEScan* pBLEScan;
TaskHandle_t door_lockout_task;

// Flags when ellie beacon is found
bool pinged = false;

/* Declare Functions */
void handle_door_lock( void * parameter );
void unlock_door();
void lock_door();
//void IRAM_ATTR button_pressed();

/* Define classes */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if(advertisedDevice.getName() == "Ellie") {
        currentRSSI = advertisedDevice.getRSSI();
        pinged = true;
        consecutiveFalsePings = 0;
      }
    }
};


/* Define Functions */
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

  xTaskCreatePinnedToCore(
      handle_door_lock, /* Function to implement the task */
      "HDL", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &door_lockout_task,  /* Task handle. */
      0); /* Core where the task should run */

}

void loop() {

  BLEScanResults foundDevices = pBLEScan->start(SCAN_DURATION, false);
  
  if(pinged) { // Ellie beacon was located
    Serial.print("Previous RSSI: "); Serial.println(previousRSSI);
    Serial.print("Current RSSI: "); Serial.println(currentRSSI);

    if(currentRSSI > RSSI_DOOR_OVERRIDE) {
      //Serial.println("Ellie is close enough to open the door!");
      if(lockout_engaged == false) {
        unlock_door();
      } 
    } else if(previousRSSI != 0 and currentRSSI - previousRSSI > RSSI_INC_THRESHOLD) {
      //Serial.println("Ellie is getting closer!");
      if(lockout_engaged == false) {
        unlock_door();
      }
    } else if(previousRSSI != 0 and currentRSSI - previousRSSI < 0) {
      //Serial.println("Ellie is getting further away!");
      asm("nop");
    } else if(previousRSSI != 0){
      asm("nop");
      //Serial.println("Ellie is the same distance away!");
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


/* Runs on separate core
  * Handles locking and unlocking the door based on lockout switch
  * position and door open time
*/
void handle_door_lock( void * parameter ) {
  for(;;) {
    // Lock door if lockout switch is closed (Locked)
    if(digitalRead(LOCKOUT_SWITCH_PIN) == LOCKOUT_SWITCH_LOCKED) {
      lock_door();
      lockout_engaged = true;
    } else {
      // Lockout switch is open (Unlocked)
      lockout_engaged = false;

      // Check if door has been open for too long
      if(door_status == door_open && (millis() / 1000) - time_of_door_open > door_open_time) {
        lock_door();
      }
    }
  }
}


void unlock_door(){

  if(door_status == door_closed) {
    // Open door
    //Serial.println("Unlocking door!");
    digitalWrite(RELAY_PIN, HIGH);
    time_of_door_open = millis() / 1000;
    door_status = door_open;
  } 
  
  return;
}

void lock_door() {
  //Serial.println("Locking door!");

  digitalWrite(RELAY_PIN, LOW);
  time_of_door_open = 0;
  door_status = door_closed;

  return;
}


/** Interrupt approach
void IRAM_ATTR button_pressed() {
  Serial.println("Button Pressed!");
} 
*/
