#include <EEPROM.h>

#define WIFI_SSID_EEP_ADDR 0x0000
#define WIFI_PWD_EEP_ADDR 0x0100

void writeStringToEEPROM(uint32_t addrOffset, const char *string);
void readStringFromEEPROM(uint32_t addrOffset, char *string);
String readStringFromEEPROM(uint32_t addrOffset);
void writeStringToEEPROM(uint32_t addrOffset, const String &str);
uint8_t checkForEEPROMData();