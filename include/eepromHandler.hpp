#include <EEPROM.h>

#define WIFI_SSID_EEP_ADDR 0x0000
#define WIFI_PWD_EEP_ADDR 0x0064
#define DOG_NAME_EEP_ADDR 0x00c8

void writeStringToEEPROM(uint32_t addrOffset, const char *string);
void readStringFromEEPROM(uint32_t addrOffset, char *string);
String readStringFromEEPROM(uint32_t addrOffset);
void writeStringToEEPROM(uint32_t addrOffset, const String &str);
uint8_t checkForEEPROMData(uint32_t address);
void clearEEPROM(uint16_t addr_l = 0, uint16_t addr_h = 4096);
void initEEPROM(uint16_t size);