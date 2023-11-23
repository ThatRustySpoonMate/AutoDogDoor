#include "eepromHandler.hpp"

void writeStringToEEPROM(uint32_t addrOffset, const char *string){

    uint8_t strLen = sizeof(string); // Get length of character array

    EEPROM.write(addrOffset, strLen); // First write the length of the character array that follows

    // Write each character of the character array to an incremental address
    for (uint16_t i = 0; i < strLen; i++) {
        EEPROM.write(addrOffset + 1 + i, string[i]); 
    }

    EEPROM.commit();

    return;
}

void readStringFromEEPROM(uint32_t addrOffset, char *string){

    uint8_t newStrLen = EEPROM.read(addrOffset); // Get length of character array

    // Read each char of the character array and store it
    for (uint16_t i = 0; i < newStrLen; i++) {
        string[i] = EEPROM.read(addrOffset + 1 + i);
    }

    string[newStrLen] = '\0'; // Null terminate character array

    return;
}

// ** Overloads for the String data type included in arduino framework ** //

void writeStringToEEPROM(uint32_t addrOffset, const String &str){

    uint8_t strLen = str.length(); // Get length of character array

    EEPROM.write(addrOffset, strLen); // First write the length of the character array that follows


    // Write each character of the character array to an incremental address
    for (uint16_t i = 0; i < strLen; i++) {
        EEPROM.write(addrOffset + 1 + i, str[i]); 
    }


    EEPROM.commit();

    return;
}

String readStringFromEEPROM(uint32_t addrOffset){

    uint8_t newStrLen = EEPROM.read(addrOffset); // Get length of character array

    char data[newStrLen + 1];

    // Read each char of the character array and store it
    for (uint16_t i = 0; i < newStrLen; i++) {
        data[i] = EEPROM.read(addrOffset + 1 + i);
    }

    data[newStrLen] = '\0'; // Null terminate character array

    return String(data);
}

uint8_t checkForEEPROMData(){
    return EEPROM.read(WIFI_SSID_EEP_ADDR);
}