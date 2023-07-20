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

typedef enum {
  door_closed = 0x00,
  door_open = 0x01
} DOORSTATUS;

// Define Global Vars
int SCAN_DURATION = 2; //In seconds
int16_t RSSI_THRESHOLD = -80; // Threshold for RSSI value to determine if device is in range
BLEScan* pBLEScan;
uint32_t SCAN_INTERVAL = 50; // Time between scans in ms
uint32_t door_open_time = 10; // Time in seconds that door should be open for

// Allow for us to track if RSSI is increasing (Stronger signal = Ellie closer)
int previousRSSI = 0;
int currentRSSI = 0;
int RSSI_INC_THRESHOLD = 5; // Threshold for RSSI increase to determine if door should be opened
int RSSI_DOOR_OVERRIDE = -70; // Threshold for RSSI to determine if door should be opened

uint8_t door_status = 0x00; // 0x00 = closed, 0x01 = open
uint32_t time_of_door_open = 0; // Time in seconds that door was opened
bool lockout_engaged = false;

TaskHandle_t door_lockout_task;

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
void handle_door_lock( void * parameter );
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
      Serial.println("Ellie is close enough to open the door!");
      if(lockout_engaged == false) {
        unlock_door();
      } 
    } else if(previousRSSI != 0 and currentRSSI - previousRSSI > RSSI_INC_THRESHOLD) {
      Serial.println("Ellie is getting closer!");
      if(lockout_engaged == false) {
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


// Run on separate core
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