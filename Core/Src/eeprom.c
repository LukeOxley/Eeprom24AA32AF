#include "eeprom.h"
#include <stdlib.h>
#include <string.h>

uint8_t g_write_data[4];
HAL_StatusTypeDef ret;


void euploadRaw(void* from_addr, uint16_t to_addr, uint16_t size);

void esetAddress(uint16_t addr)
{
  uint8_t timeout = 0;

  g_write_data[0] = addr >> 8;
  g_write_data[1] = addr & 0xF;

  ret = HAL_I2C_Master_Transmit(&hi2c3, SET_ADDRESS(DEVICE_ADDR, WRITE_ENABLE), g_write_data, 2, HAL_MAX_DELAY);

  for(timeout = 0; hi2c3.State != HAL_I2C_STATE_READY && timeout < WRITE_TIMEOUT; timeout++)
  {
    //Wait for the send to stop
  }
}

//reads single byte
uint8_t eread(uint16_t addr)
{
  uint8_t value=6;

  esetAddress(addr);

  ret = HAL_I2C_Master_Receive(&hi2c3, SET_ADDRESS(DEVICE_ADDR, READ_ENABLE), &value, 1, HAL_MAX_DELAY);

  HAL_Delay(20);
  return value;
}

//reads chunk of data
void edownload(uint16_t from_addr, void* to_addr, uint16_t size)
{
  esetAddress(from_addr);

  ret = HAL_I2C_Master_Receive(&hi2c3, SET_ADDRESS(DEVICE_ADDR, READ_ENABLE), to_addr, size, HAL_MAX_DELAY);

  HAL_Delay(20);
}

//writes single byte
void ewrite(uint16_t addr, uint8_t val)
{
  g_write_data[0] = addr >> 8;
  g_write_data[1] = addr & 0xF;
  g_write_data[2] = val;

  uint8_t timeout = 0;

  ret = HAL_I2C_Master_Transmit(&hi2c3, SET_ADDRESS(DEVICE_ADDR, WRITE_ENABLE), g_write_data, 3, HAL_MAX_DELAY);

  if(ret != HAL_OK)
  {
     HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
  }//else
 // {
  //   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
 // }
  for(timeout = 0; hi2c3.State != HAL_I2C_STATE_READY && timeout < WRITE_TIMEOUT; timeout++)
      {
        //Wait for the send to stop
      }
  if(timeout > WRITE_TIMEOUT)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
  }

}

//uploads chunk ignoring page breaks
void euploadRaw(void* from_addr, uint16_t to_addr, uint16_t size)
{
  //convert type to allow copying
  uint8_t* from = from_addr;

  uint8_t* buff = malloc(size + 2);

  //insert memory address
  buff[0] = to_addr >> 8;
  buff[1] = to_addr & 0xF;

  //copy memory from from_addr to buffer
  for(uint16_t i = 0; i < size; i++)
  {
    buff[i+2] = from[i];
  }

  uint8_t timeout = 0;

  ret = HAL_I2C_Master_Transmit(&hi2c3, SET_ADDRESS(DEVICE_ADDR, WRITE_ENABLE), buff, size + 2, HAL_MAX_DELAY);

  if(ret != HAL_OK)
  {
     HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
  }

  for(timeout = 0; hi2c3.State != HAL_I2C_STATE_READY && timeout < WRITE_TIMEOUT; timeout++)
  {
    //Wait for the send to stop
  }

  if(timeout > WRITE_TIMEOUT)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
  }

  free(buff);
}

//breaks data into chunks to prevent crossing page boundary
void eupload(void* from_addr, uint16_t to_addr, uint16_t size)
{
  uint16_t next_boundary = (to_addr / PAGE_SIZE + 1) * PAGE_SIZE;
  uint16_t current_addr = to_addr;
  uint16_t end_loc = to_addr + (size - 1);
  uint8_t* from = from_addr;
  uint8_t chunkSize; //number of bytes copying from mem

  do
  {
    //send from current to boundary or end loc, whichever is less
    if(end_loc - current_addr < next_boundary - current_addr)
    {
      chunkSize = end_loc - current_addr + 1;
    }else
    {
      chunkSize = next_boundary - current_addr;
    }

    euploadRaw(from + (current_addr - to_addr) ,current_addr , chunkSize);
    HAL_Delay(20);
    current_addr += chunkSize;
    next_boundary = (current_addr / PAGE_SIZE + 1) * PAGE_SIZE;
  }while(current_addr < end_loc);

}

//transfers all values to given huart
void edump(UART_HandleTypeDef huart)
{
  uint8_t MSG[33] = {'\0'};
  for(uint16_t i = 0; i < 4000; i+=32)
  {
    edownload(i, MSG, 32);
    HAL_UART_Transmit(&huart, MSG, sizeof(MSG)-1, 100);
    //HAL_Delay(10);
  }
}

//Sets all addresses to 0
void eWipe()
{
  uint8_t data[32] = {0};

  for(uint16_t i = 0; i < 4000; i+=32)
  {
    eupload(data, i, 32);
  }
}

void includeStruct(void* ptr, uint16_t size, char name[], uint8_t version)
{
  struct MemNode* newNode;
  struct MemNode* nodeEnd;
  newNode = malloc(sizeof(struct MemNode));
  nodeEnd = memHead;

  //Find correct node
  if(memHead == NULL)
  {
    memHead = newNode;
  }else
  {
    //finds last node in sequence
    while(nodeEnd->next != NULL)
    {
      nodeEnd = nodeEnd->next;
    }
    nodeEnd->next = newNode;
  }

  //Assign values in node
  newNode->ptr = ptr;
  newNode->size = size;
  strcpy(newNode->name, name);
  newNode->version = version;

  //TODO maybe make functions for linked list traversing
  //TODO find address for eeprom (flow chart of version checking)
  //TODO function to search header file by name for existing #yeet
}
