/*
 * eeprom.h
 *
 *  Created on: Dec 6, 2020
 *      Author: Luke Oxley
 */

#ifndef EEPROM_H_
#define EEPROM_H_

#include "main.h"

extern I2C_HandleTypeDef hi2c3;

#define DEVICE_ADDR 0x50 //Before Bit Shifting

//writing
#define WRITE_ENABLE 0x00
#define WRITE_TIMEOUT 1000

#define PAGE_SIZE 32

//reading
#define READ_ENABLE 0x01

#define NAME_SIZE 3

struct MemNode
{
  char name[NAME_SIZE];
  uint8_t version;
  uint16_t eAddress; //loc in eeprom
  uint16_t size;
  void* ptr;
  struct MemNode* next;
};

struct MemNode* memHead; //first node

//macros
#define SET_ADDRESS(address, write_en) (address << 1) | write_en

uint8_t eread(uint16_t addr);
void ewrite(uint16_t addr, uint8_t val);
void edownload(uint16_t from_addr, void* to_addr, uint16_t size);
void eupload(void* from_addr, uint16_t to_addr, uint16_t size);
void edump(UART_HandleTypeDef huart);
void eWipe();
void includeStruct(void* ptr, uint16_t size, char name[], uint8_t version);

#endif
