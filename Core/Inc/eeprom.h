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

//errors
enum EEPROM_ERROR{COM_TIMEOUT, COM_ERROR};

//macros
#define SET_ADDRESS(address, write_en) (address << 1) | write_en

void eSetAddress(uint16_t addr);
uint8_t eRead(uint16_t addr);
void eDownload(uint16_t from_addr, void* to_addr, uint16_t size);
void eWrite(uint16_t addr, uint8_t val);
void eUpload(void* from_addr, uint16_t to_addr, uint16_t size);
void eDump(UART_HandleTypeDef huart);
void eWipe();
struct HeaderNode* eAddToList();
struct HeaderNode* eFindHeader(char name[]);
void eAddHeaderEntry(struct HeaderNode* newHeader);
void eUpdateHeaderEntry(struct HeaderNode* header);
void eLinkStruct(void* ptr, uint16_t size, char name[], uint8_t version, uint8_t overwrite);
void eLoadHeaders();
void eSortHeaders();
uint16_t eSpaceAvailable(uint16_t address);
uint16_t eMalloc(uint16_t size);
void eInitialize(I2C_HandleTypeDef* i2c, uint16_t eepromSpace, uint8_t address);
void eCleanHeaders();
void eRemoveFromList(char name[]);
void eDeleteHeader(char name[]);
void eSetI2C(I2C_HandleTypeDef* i2c);
uint8_t eLoadStruct(char name[]);
uint8_t eSaveStruct(char name[]);
void eSplitVersion(uint8_t* version, uint8_t* overwrite);
void eCombineVersion(uint8_t* version, uint8_t* overwrite);
void eErrorFound(enum EEPROM_ERROR error);

#endif
