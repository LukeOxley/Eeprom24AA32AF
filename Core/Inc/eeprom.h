/*
 * eeprom.h
 *
 *  Created on: Dec 6, 2020
 *      Author: Luke Oxley
 */

#ifndef EEPROM_H_
#define EEPROM_H_

#include "main.h"

//#define DEVICE_ADDR 0x50 //Before Bit Shifting

//writing
#define WRITE_ENABLE 0x00
#define WRITE_TIMEOUT 1000

#define PAGE_SIZE 32

//reading
#define READ_ENABLE 0x01

#define NAME_SIZE 3

//Header management

struct HeaderNode
{
  char name[NAME_SIZE];
  uint8_t version;
  uint16_t size;
  uint16_t eAddress;
  void* ptr;
  struct HeaderNode* next;
};

#define HEADER_SIZE 8 //bytes per header

#define MAX_HEADER_COUNT 20 //8*20 160/4000 number of entries worth of space allocated

#define OVERWRITE_LOC 7
#define OVERWRITE_MASK 0b01111111
#define MAX_VERSION 127

//#define EEPROM_SIZE 4000 //bytes

//macros
#define SET_ADDRESS(address, write_en) (address << 1) | write_en

void esetAddress(uint16_t addr);
uint8_t eread(uint16_t addr);
void edownload(uint16_t from_addr, void* to_addr, uint16_t size);
void ewrite(uint16_t addr, uint8_t val);
void eupload(void* from_addr, uint16_t to_addr, uint16_t size);
void edump(UART_HandleTypeDef huart);
void eWipe();
struct HeaderNode* eAddToList();
struct HeaderNode* findHeader(char name[]);
void addHeaderEntry(struct HeaderNode* newHeader);
void updateHeaderEntry(struct HeaderNode* header);
void eLinkStruct(void* ptr, uint16_t size, char name[], uint8_t version, uint8_t overwrite);
void loadHeaders();
void sortHeaders();
uint16_t eSpaceAvailable(uint16_t address);
uint16_t emalloc(uint16_t size);
void eInitialize(I2C_HandleTypeDef* i2c, uint16_t eepromSpace, uint8_t address);
void cleanHeaders();
void removeFromList(char name[]);
void deleteHeader(char name[]);
void eSetI2C(I2C_HandleTypeDef* i2c);
uint8_t loadStruct(char name[]);
uint8_t saveStruct(char name[]);
void splitVersion(uint8_t* version, uint8_t* overwrite);
void combineVersion(uint8_t* version, uint8_t* overwrite);

#endif
