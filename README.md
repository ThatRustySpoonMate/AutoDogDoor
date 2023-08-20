# AutoDogDoor

## Parts list:

 - ESP32 Development Board (WROOM-32)
   ![image](https://github.com/ThatRustySpoonMate/AutoDogDoor/assets/51020883/ab7f99fa-5d4b-4be5-beed-736737ce4eec)

 - BLE Beacon (I chose this one: https://www.aliexpress.com/item/1005005060806181.html?spm=a2g0o.order_list.order_list_main.17.6a7e1802zjQ3Iw)
   ![image](https://github.com/ThatRustySpoonMate/AutoDogDoor/assets/51020883/92abeeef-df28-4aa8-88ea-13081c13d427)
   
 - LM2596 DC-DC Step down converter
   ![image](https://github.com/ThatRustySpoonMate/AutoDogDoor/assets/51020883/4f993893-507a-4cd0-b1cc-4a27577d3a77)

 - 1 channel 12v Relay with 3.3v logic level trigger
   ![image](https://github.com/ThatRustySpoonMate/AutoDogDoor/assets/51020883/d26c6a88-2001-4c2b-ada8-e7630f57c659)

 - 12v Cabinet Lock solenoid (Normally Closed)
   ![image](https://github.com/ThatRustySpoonMate/AutoDogDoor/assets/51020883/233f53d1-fea9-4d91-b114-982af194a9af)

 - Male+Female XT90 connector pair (Or any of your favourite connectors)
   ![image](https://github.com/ThatRustySpoonMate/AutoDogDoor/assets/51020883/259d6a01-ff3e-4bba-afb4-1e1525d47ad4)

## Wiring Guide
 - ESP32 Pin 4 to Relay
 - ESP32 GND to Relay
 - ESP32 3v3 To Relay
 - ESP32 Pin 17 to switch
 - ESP32 GND to other side of switch
