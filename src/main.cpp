#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include "main.hpp"
#include "eepromHandler.hpp"

/* Define Global Vars */

/* Configuration variables - not const as they I may add support for editing them via webpage */
int SCAN_DURATION = 1; //In seconds
uint32_t SCAN_INTERVAL = 1; // Time between scans in ms
int RSSI_INC_THRESHOLD = 5; // If RSSI increases by this amount between pings, door should be opened
int RSSI_DOOR_OVERRIDE = -78; // Threshold for RSSI to determine if door should be opened
uint32_t door_open_time = 10; // Time in seconds that door should be open for

/* Temporal variables */
int previousRSSI = 0; // Allow for us to track if RSSI is increasing (Stronger signal = Ellie closer)
int currentRSSI = 0;
DOORSTATUS door_status = door_closed; // 0x00 = closed, 0x01 = open
DOORLOCKSTATE doorLockState = door_unlocked;  
uint32_t time_of_door_open = 0; // Time in seconds that door was opened
uint32_t consecutiveFalsePings = 0;
uint32_t unlock_cycles = 0; // Number of times this has been unlocked
unsigned long uptime_hours = 0; // System uptime
unsigned long uptime_days = 0; // System uptime
float coreTemp;
char BLEDogName[100]; // Name registered in dog's BLE beacon - could make this an array of strings and check each one
String BLEDogNameTemp;

// BLE Object
BLEScan* pBLEScan;


TaskHandle_t door_lockout_task;

// Flags when ellie beacon is found
bool pinged = false;


/* WIFI Variables */
TaskHandle_t webserver_task;
WiFiServer server(80); // Create server on port 80
String header; // To store HTTP request

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;


/* Declare Functions */
void handle_door_lock( void * parameter );
void handle_webserver( void * parameter );
void open_door();
void close_door();
void unlock_door(DOORLOCKSTATE dls);
void lock_door(DOORLOCKSTATE dls);
void get_core_temp();

/* Define class for BLE */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if(advertisedDevice.getName() == BLEDogName) {
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
  Serial.println("Ellie Door starting.");

  // Set up relay pin
  pinMode(RELAY_PIN, OUTPUT);
  // Set up button pin
  pinMode(LOCKOUT_SWITCH_PIN, INPUT_PULLUP);

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value

  setCpuFrequencyMhz(240);
  Serial.print("CPU: "); Serial.print(getCpuFrequencyMhz()); Serial.println("MHz");
  Serial.print("APB: "); Serial.print(getApbFrequency()); Serial.println("Hz");

  initEEPROM(512);
  

  // Ensure we have name of dog to start with
  bool requestNewName = false;
  if(checkForEEPROMData(DOG_NAME_EEP_ADDR) == 0x00) {
    requestNewName = true;
    // Dog name not stored in EEPROM
    Serial.println("---Unable to find BLE Beacon name in storage---");

  } else { // Located BLE Dog name
    uint32_t time_of_BLEName_prompt = millis();
    BLEDogNameTemp = readStringFromEEPROM(DOG_NAME_EEP_ADDR);

    Serial.print("Found BLE Name '"); Serial.print(BLEDogNameTemp); Serial.println("' - Would you like to change this? (y/N)");

    // Wait 5s for user input
    while(time_of_BLEName_prompt + 5000 > millis()) {
      if( Serial.available() > 0) {
        uint8_t conf = Serial.read();

        if(conf == 'y') {
          requestNewName = true;
          break;
        } else if(conf == 'n') {
          requestNewName = false;
          break;
        }
      }
    }
  }

  if(requestNewName) {
    Serial.println("Please enter the name of the dog's BLE Beacon as it appears in bluetooth: ");

    while(Serial.available() == 0) {
      delay(1);
    }

    // Read the SSID into the variable
    BLEDogNameTemp = Serial.readStringUntil('\n');//.c_str();

    writeStringToEEPROM(DOG_NAME_EEP_ADDR, BLEDogNameTemp);
  }

  memcpy(BLEDogName, BLEDogNameTemp.c_str(), strlen(BLEDogNameTemp.c_str()) -1 ); // Copy string to c-str and remove trailing charater 


  // Wifi vars
  String ssid = "";
  String password = "";

  char ssid_processed[100];
  char password_processed[100];


  uint32_t time_of_wifi_prompt = millis();
  bool forceWifiCredentials = false;

  Serial.println("Would you like to enter new WIFI Credentials? (y/N)");
  // Wait 5s for user input
  while(time_of_wifi_prompt + 5000 > millis()) {
    if( Serial.available() > 0) {
      uint8_t conf = Serial.read();

      if(conf == 'y') {
        forceWifiCredentials = true;
        break;
      } else if(conf == 'n') {
        break;
      }
    }
  }

  
  // Attempt to read SSID from EEPROM
  if(checkForEEPROMData(WIFI_SSID_EEP_ADDR) == 0x00 || forceWifiCredentials) {

    Serial.println("---Unable to find WIFI credentials in storage---");

    // Get WIFI SSID from user
    Serial.println("Please enter your WiFi SSID: ");
    // Wait for data
    while(Serial.available() == 0) {
      delay(1);
    }

    // Read the SSID into the variable
    ssid = Serial.readStringUntil('\n');

    // Get WIFI password from user
    Serial.println("Please enter your WiFI Password: ");
    // Wait for data
    while(Serial.available() == 0) {
      delay(1);
    }

    // Read the Password into the variable
    password = Serial.readStringUntil('\n');

    // Wifi.begin requires c-strings, create them from string objects
    ssid.toCharArray(ssid_processed, ssid.length());
    password.toCharArray(password_processed, password.length());
  } else {
    // Retrieve SSID and Password from EEPROM
    ssid = readStringFromEEPROM(WIFI_SSID_EEP_ADDR);
    password = readStringFromEEPROM(WIFI_PWD_EEP_ADDR);
    Serial.println("---Located WIFI credentials in storage---");

    // Wifi.begin requires c-strings, create them from string objects
    ssid.toCharArray(ssid_processed, ssid.length());
    password.toCharArray(password_processed, password.length());
  }

  Serial.print("Connecting to ");
  Serial.println( ssid );

  WiFi.begin(ssid_processed, password_processed);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Store uname and pwd to EEPROM
  writeStringToEEPROM(WIFI_SSID_EEP_ADDR, ssid);
  writeStringToEEPROM(WIFI_PWD_EEP_ADDR, password);

  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();

  xTaskCreatePinnedToCore(
      handle_webserver, /* Function to implement the task */
      "hdl_ws", /* Name of the task */
      5000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &webserver_task,  /* Task handle. */
      0); /* Core where the task should run */

  xTaskCreatePinnedToCore(
      handle_door_lock, /* Function to implement the task */
      "hdl_sw", /* Name of the task */
      5000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &door_lockout_task,  /* Task handle. */
      0); /* Core where the task should run */


  Serial.println("Startup has finished.");
}


void loop() {

  get_core_temp();

  BLEScanResults foundDevices = pBLEScan->start(SCAN_DURATION, false);
  
  if(pinged) { // Ellie beacon was located
    Serial.print("Previous RSSI: "); Serial.println(previousRSSI);
    Serial.print("Current RSSI: "); Serial.println(currentRSSI);

    if(currentRSSI > RSSI_DOOR_OVERRIDE) {
      //Serial.println("Ellie is close enough to open the door!");
      open_door();
      
    } else if(previousRSSI != 0 and currentRSSI - previousRSSI > RSSI_INC_THRESHOLD) {
      //Serial.println("Ellie is getting closer!");
      //open_door();
      asm("nop");
      
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




void handle_webserver( void * parameter ) {
  for(;;) {
    WiFiClient client = server.available();   // Listen for incoming clients

    if (client) {                             // If a new client connects,
    currentTime = millis();
    uptime_days = currentTime / 1000 / 60 / 60 / 24; // Days
    uptime_hours = (currentTime / 1000 / 60 / 60) - (uptime_days * 24); // Hours
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs on and off
            if (header.indexOf("GET /lock/on") >= 0) {
              Serial.println("Turning Lockout on");
              lock_door(door_locked_wifi);
              close_door();
            } else if (header.indexOf("GET /lock/off") >= 0) {
              Serial.println("Turning Lockout off");
              unlock_door(door_locked_wifi); //*((bool*) parameter)
            }
            // RSSI (sensitivity) controls
            if (header.indexOf("GET /rssi/inc") >= 0) {
              RSSI_DOOR_OVERRIDE++;
            } else if (header.indexOf("GET /rssi/dec") >= 0) {
              RSSI_DOOR_OVERRIDE--;
            }

            if (header.indexOf("GET /door/open") >= 0) {
              open_door();
            }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>Ellie Door Control</h1>");

            client.println("<h2>Door Control</h2>");
            
            // If the outputlockState is off, it displays the ON button       
            if (doorLockState==door_unlocked) {
              // Display current state, and ON/OFF buttons for Lockout  
              client.println("<p>Lockout state: UNLOCKED </p>");
              client.println("<a href=\"/lock/on\"><button class=\"button\">LOCK</button></a>");
              // Opening the door
              client.println("<a href=\"/door/open\"><button class=\"button\">OPEN</button></a>");
            } else if (doorLockState==door_locked_switch) {
              // Display current state, and ON/OFF buttons for Lockout  
              client.println("<p>Lockout state: LOCKED (By Switch) </p>");
              client.println("<a href=\"/lock/on\"><button class=\"button\">LOCK</button></a>");
              // Opening the door
              client.println("<a href=\"/door/open\"><button class=\"button\" disabled>OPEN</button></a>");
            } else {
              // Display current state, and ON/OFF buttons for Lockout  
              client.println("<p>Lockout state: LOCKED </p>");
              client.println("<a href=\"/lock/off\"><button class=\"button\">UNLOCK</button></a>");
              // Opening the door
              client.println("<a href=\"/door/open\"><button class=\"button button2\" disabled>OPEN</button></a>");
            } 

            

            // RSSI
            client.println("<h2>Sensitivity</h2>");
            client.println("<p>RSSI Threshold: " + String(RSSI_DOOR_OVERRIDE) + "</p>");
            client.println("<a href=\"/rssi/inc\"><button class=\"button\">+1</button></a>");
            client.println("<a href=\"/rssi/dec\"><button class=\"button\">-1</button></a>");
            client.println("<p>Most recent RSSI: " + String(currentRSSI) + "</p>");

            client.println("<h2>Stats</h2>");
            client.println("<p>Uptime: " + String(uptime_days) + " days " + String(uptime_hours) + " hours </p>");
            client.println("<p>Unlock cycles: " + String(unlock_cycles) + "</p>");
            client.println("<p>Core temp:  " + String(coreTemp) + "c</p>");
               
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }

  delay(5000);

  }


}

/* Runs on separate core
  * Handles locking and unlocking the door based on lockout switch
  * position and door open time
*/
void handle_door_lock( void * parameter ) {
  for(;;) {
    // Lock door if lockout switch is closed (Locked)
    if(digitalRead(LOCKOUT_SWITCH_PIN) == LOCKOUT_SWITCH_LOCKED) {
      close_door();
      lock_door(door_locked_switch);
    } else {
      // Lockout switch is open (Unlocked)
      unlock_door(door_locked_switch);

      // Check if door has been open for too long
      if(door_status == door_open && (millis() / 1000) - time_of_door_open > door_open_time) {
        close_door();
      }
    }

    delay(200);
    
  }
}


// Allows ellie to go through the door
void open_door(){

  if(door_status == door_closed && doorLockState == door_unlocked) {
    // Open door
    digitalWrite(RELAY_PIN, HIGH);
    time_of_door_open = millis() / 1000;
    door_status = door_open;
    unlock_cycles++;
  } 
  
  return;
}

// Stops ellie from going through the door
void close_door() {

  digitalWrite(RELAY_PIN, LOW);
  time_of_door_open = 0;
  door_status = door_closed;

  return;
}

void lock_door(DOORLOCKSTATE dls) {
  // Handle flagging the source of the door lock
  if(doorLockState == door_unlocked) {
    doorLockState = dls;
  } else if (doorLockState != dls) {
    doorLockState = door_locked_both;
  }

  return;
}

// This func will return if already unlocked
void unlock_door(DOORLOCKSTATE dls) {
  // Handling clearing of locks
  if(doorLockState == dls) {
    doorLockState = door_unlocked;
  } else if(doorLockState == door_locked_both) {
    if(dls == door_locked_switch) {
      doorLockState = door_locked_wifi;
    } else {
      doorLockState = door_locked_switch;
    }
  }
  return;
}

void get_core_temp() {
  static float tempCoreTemp;

  tempCoreTemp = temperatureRead();
  if(tempCoreTemp < 53.3 || tempCoreTemp > 53.4) {
    coreTemp = tempCoreTemp;
  }

  return;
}