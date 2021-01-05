#include "eeprom.h"
#include <stdlib.h>
#include <string.h>

uint8_t g_write_data[4];
HAL_StatusTypeDef ret;

struct HeaderNode* headerFirst; //first node
struct HeaderNode* headerLast; //last node

uint8_t g_numStructs; //number of entries in header

I2C_HandleTypeDef i2c01;
uint16_t eepromSize;
uint8_t device_addr;

//sets 'cursor' in eeprom
void esetAddress(uint16_t addr)
{
  uint8_t timeout = 0;

  g_write_data[0] = addr >> 8;
  g_write_data[1] = addr & 0xFF;

  ret = HAL_I2C_Master_Transmit(&i2c01, SET_ADDRESS(device_addr, WRITE_ENABLE), g_write_data, 2, HAL_MAX_DELAY);

  for(timeout = 0; i2c01.State != HAL_I2C_STATE_READY && timeout < WRITE_TIMEOUT; timeout++)
  {
    //Wait for the send to stop
  }
}

//reads single byte
uint8_t eread(uint16_t addr)
{
  uint8_t value=6;

  esetAddress(addr);

  ret = HAL_I2C_Master_Receive(&i2c01, SET_ADDRESS(device_addr, READ_ENABLE), &value, 1, HAL_MAX_DELAY);

  HAL_Delay(20);
  return value;
}

//reads chunk of data
void edownload(uint16_t from_addr, void* to_addr, uint16_t size)
{
  esetAddress(from_addr);
  HAL_Delay(10);
  ret = HAL_I2C_Master_Receive(&i2c01, SET_ADDRESS(device_addr, READ_ENABLE), to_addr, size, HAL_MAX_DELAY);

  HAL_Delay(20);
}

//writes single byte
void ewrite(uint16_t addr, uint8_t val)
{
  g_write_data[0] = addr >> 8;
  g_write_data[1] = addr & 0xFF;
  g_write_data[2] = val;


  uint8_t timeout = 0;
  HAL_Delay(5);
  ret = HAL_I2C_Master_Transmit(&i2c01, SET_ADDRESS(device_addr, WRITE_ENABLE), g_write_data, 3, HAL_MAX_DELAY);

  if(ret != HAL_OK)
  {
     HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
  }
  for(timeout = 0; i2c01.State != HAL_I2C_STATE_READY && timeout < WRITE_TIMEOUT; timeout++)
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
  buff[1] = to_addr & 0xFF;

  //copy memory from from_addr to buffer
  for(uint16_t i = 0; i < size; i++)
  {
    buff[i+2] = from[i];
  }

  uint8_t timeout = 0;
  HAL_Delay(5);
  ret = HAL_I2C_Master_Transmit(&i2c01, SET_ADDRESS(device_addr, WRITE_ENABLE), buff, size + 2, HAL_MAX_DELAY);

  if(ret != HAL_OK)
  {
     HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
  }

  for(timeout = 0; i2c01.State != HAL_I2C_STATE_READY && timeout < WRITE_TIMEOUT; timeout++)
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
  for(uint16_t i = 0; i < eepromSize; i+=32)
  {
    edownload(i, MSG, 32);
    HAL_UART_Transmit(&huart, MSG, sizeof(MSG)-1, 100);
    HAL_Delay(10);
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

//adds header node, returns pointer
struct HeaderNode* eAddToList()
{
  struct HeaderNode *newNode = malloc(sizeof(struct HeaderNode));

  newNode->next = NULL;

  //For first node in list
  if(NULL == headerFirst)
  {
    headerFirst = newNode;
  }else{
    headerLast->next = newNode;
  }

  headerLast = newNode;

  return newNode;
}

//returns null if none
struct HeaderNode* findHeader(char name[])
{

  struct HeaderNode* currentHeader = headerFirst;

  //search through headers until name match
  for(int i = 0; i < g_numStructs; i++)
  {

    if(strncmp(name, currentHeader->name, NAME_SIZE) == 0)
    {
      return currentHeader;
    }

    currentHeader = currentHeader->next;
  }

  return NULL;
}

//adds Header to eeprom
void addHeaderEntry(struct HeaderNode* newHeader)
{

  g_numStructs += 1;
  ewrite(0, g_numStructs); //increment struct num by 1

  if(g_numStructs > MAX_HEADER_COUNT)
  {
    //TODO max struct count reached D:
  }

  newHeader->eAddress = emalloc(newHeader->size);

  eupload(newHeader, (g_numStructs - 1) * HEADER_SIZE + 1, HEADER_SIZE);

  sortHeaders(); //added new item, put it in place

}

//finds location of header in eeprom and updates it
void updateHeaderEntry(struct HeaderNode* header)
{
  //somehow find where its located
  //current process is slower due to searching through actual eeprom mem
  char nameFound[NAME_SIZE];

  //converting allows for pointer addition
  uint8_t* headerLoc = header;

  for(int i = 0; i < g_numStructs; i++)
  {
    edownload(i * HEADER_SIZE + 1, &nameFound, NAME_SIZE);
    if(strncmp(nameFound, header->name, NAME_SIZE) == 0)
    {
      //found the correct header to update
      eupload(headerLoc + NAME_SIZE, i * HEADER_SIZE + 1 + NAME_SIZE, HEADER_SIZE - NAME_SIZE);
      return;
    }
  }
  //header not found, should never reach this point...
}

//links struct ptr with a header from eeprom
void eLinkStruct(void* ptr, uint16_t size, char name[], uint8_t version)
{
  struct HeaderNode* node = NULL;

  node = findHeader(name);

  if(node == NULL)
  {
    //struct not in eeprom in any form
    node = eAddToList();
    strcpy(node->name, name);
    node->version = version;
    node->size = size;

    node->ptr = ptr; //link

    addHeaderEntry(node); //update eAddress too
    eupload(node->ptr, node->eAddress, node->size);

  }else if(node->size != size || node->version != version)
  {
    //overwrite and header change

    if(eSpaceAvailable(node->eAddress) < node->size)
    {
      //can't place struct here, move
      node->eAddress = emalloc(node->size);

      //change of address, sort headers
      sortHeaders();
    }

    node->version = version;
    node->size = size;

    node->ptr = ptr; //link

    updateHeaderEntry(node);
    eupload(node->ptr, node->eAddress, node->size);

  }else{
    //struct info matches that in eeprom
    node->ptr = ptr; //link
  }

}

//populate linked list with header info from eeprom
void loadHeaders()
{

  for(int i = 0; i < g_numStructs; i++)
  {
      struct HeaderNode *header = malloc(sizeof(struct HeaderNode));
      header->next = NULL;
      header->ptr = NULL;

      if(headerFirst == NULL)
      {
        headerFirst = header;
      }else
      {
        headerLast->next = header;
      }

      headerLast = header;

      edownload(i * HEADER_SIZE + 1, header, HEADER_SIZE);

  }
}

//sort headers by increasing eaddress
void sortHeaders()
{
  struct HeaderNode* prev;
  struct HeaderNode* current;
  struct HeaderNode* future;

  uint8_t swapped = 0;

  if(g_numStructs > 1)
  {

    do
    {
      prev = NULL;
      current = headerFirst;
      swapped = 0;

      while(current->next != NULL)
      {

        future = current->next;

        if(current->eAddress > future->eAddress)
        {
          swapped = 1;
          if(prev == NULL)
          {
            current->next = future->next;
            future->next = current;
            headerFirst = future;

            prev = headerFirst;
          }else
          {
            prev->next = future;
            current->next = future->next;
            future->next = current;

            prev=prev->next;
            current=prev->next;
          }
        }else
        {
          prev = current;
          current = current->next;
        }

      }

      if(current->next != NULL)
      {
        headerLast = current->next;
      }

    }while(swapped);
  }
}

//returns available space to use at an address
uint16_t eSpaceAvailable(uint16_t address)
{
  if(address > eepromSize){
    return 0;
  }

  //find header with first address greater than eAddress
  struct HeaderNode* curr = headerFirst;
  while(curr != NULL)
  {
    if(curr->eAddress > address)
    {
      return curr->eAddress - address;
    }

    curr = curr->next;
  }
  //no headers with address after said address
  return eepromSize - address;

}

//returns eeprom address with space for set size
//null if not available
uint16_t emalloc(uint16_t size)
{
  if(g_numStructs > 1)
  {
    struct HeaderNode* current = headerFirst;

    //doesn't check between end of header and first (want to leave room for more headers)
    while(current->next != NULL)
    {
      if(current->next->eAddress - (current->eAddress + current->size) >= size)
      {
        return current->eAddress + current->size;
      }
      current = current->next;
    }
    //reached last entry, check is space between last and end of eeprom

    if(eepromSize - current->eAddress + current->size >= size)
    {
      return current->eAddress + current->size;
    }

    return NULL;//no space available
  }else
  {
    //no headers in mem
    return MAX_HEADER_COUNT * HEADER_SIZE + 1;
  }


}

//loads current header info
void eInitialize(I2C_HandleTypeDef i2c, uint16_t eepromSpace, uint8_t address)
{
  i2c01 = i2c;
  eepromSize = eepromSpace;
  device_addr = address;


  g_numStructs = eread(0);

  loadHeaders();
  sortHeaders();

}


//TODO cleanHeaders (if any not being used, maybe erase them
//overwrite with last header entry :) -> think through, be careful - sinc num header entries, will have a value in the linked list with a null ptr

//TODO look at cushioning between data

