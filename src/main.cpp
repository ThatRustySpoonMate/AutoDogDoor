#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include "main.h"
#include "NetworkCredentials.h"

/* Define Global Vars */

/* Configuration variables */
int SCAN_DURATION = 1; //In seconds
uint32_t SCAN_INTERVAL = 1; // Time between scans in ms
int RSSI_INC_THRESHOLD = 5; // If RSSI increases by this amount between pings, door should be opened
int RSSI_DOOR_OVERRIDE = -70; // Threshold for RSSI to determine if door should be opened
uint32_t door_open_time = 10; // Time in seconds that door should be open for

/* Temporal variables */
int previousRSSI = 0; // Allow for us to track if RSSI is increasing (Stronger signal = Ellie closer)
int currentRSSI = 0;
uint8_t door_status = door_closed; // 0x00 = closed, 0x01 = open
uint32_t time_of_door_open = 0; // Time in seconds that door was opened
uint8_t lockout_engaged = 0;
uint32_t consecutiveFalsePings = 0;

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
void unlock_door();
void lock_door();
//void IRAM_ATTR button_pressed();

/* Define class for BLE */
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

  //setCpuFrequencyMhz(80);
  //Serial.print("CPU: "); Serial.print(getCpuFrequencyMhz()); Serial.println("MHz");
  //Serial.print("APB: "); Serial.print(getApbFrequency()); Serial.println("Hz");

  // Determine which method of lockout we compile for
  #if LOCKOUT_OP_MODE_WIFI
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
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
  #else
  xTaskCreatePinnedToCore(
      handle_door_lock, /* Function to implement the task */
      "HDL", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &door_lockout_task,  /* Task handle. */
      0); /* Core where the task should run */
  #endif

}

void loop() {

  BLEScanResults foundDevices = pBLEScan->start(SCAN_DURATION, false);
  
  if(pinged) { // Ellie beacon was located
    Serial.print("Previous RSSI: "); Serial.println(previousRSSI);
    Serial.print("Current RSSI: "); Serial.println(currentRSSI);

    if(currentRSSI > RSSI_DOOR_OVERRIDE) {
      //Serial.println("Ellie is close enough to open the door!");
      if(lockout_engaged == 0) {
        unlock_door();
      } 
    } else if(previousRSSI != 0 and currentRSSI - previousRSSI > RSSI_INC_THRESHOLD) {
      //Serial.println("Ellie is getting closer!");
      if(lockout_engaged == 0) {
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




void handle_webserver( void * parameter ) {
  for(;;) {
    WiFiClient client = server.available();   // Listen for incoming clients

    if (client) {                             // If a new client connects,
    currentTime = millis();
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
            if (header.indexOf("GET /26/on") >= 0) {
              Serial.println("Turning Lockout on");
              lockout_engaged = 1;
            } else if (header.indexOf("GET /26/off") >= 0) {
              Serial.println("Turning Lockout off");
              lockout_engaged = 0; //*((bool*) parameter)
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
            
            
            // If the output26State is off, it displays the ON button       
            if (lockout_engaged==0) {
              // Display current state, and ON/OFF buttons for Lockout  
              client.println("<p>Lockout state: OFF </p>");
              client.println("<p><a href=\"/26/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              // Display current state, and ON/OFF buttons for Lockout  
              client.println("<p>Lockout state: ON </p>");
              client.println("<p><a href=\"/26/off\"><button class=\"button button2\">OFF</button></a></p>");
            } 
               
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
      lock_door();
      lockout_engaged = 1;
    } else {
      // Lockout switch is open (Unlocked)
      lockout_engaged = 0;

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
