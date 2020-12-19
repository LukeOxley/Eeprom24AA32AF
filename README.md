# Eeprom24AA32AF
EEPROM memory management for a STM Nucleo Board

Designed to save values of structs for data tracking. Each struct is stored with a corresponding name, version, and size. If a struct is added with a same name, but different version, the existing struct in memory will be overwritten. If the new struct is too large for the memory allocated, more space will be made to fit the larger struct.
