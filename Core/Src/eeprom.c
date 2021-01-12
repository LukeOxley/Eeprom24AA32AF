#include "eeprom.h"
#include <stdlib.h>
#include <string.h>

uint8_t g_write_data[4];
HAL_StatusTypeDef ret;

struct HeaderNode* headerFirst; //first node
struct HeaderNode* headerLast; //last node

uint8_t g_numStructs; //number of entries in header

I2C_HandleTypeDef* i2c01;
uint16_t eepromSize;
uint8_t device_addr;

//sets 'cursor' in eeprom
void esetAddress(uint16_t addr)
{
  uint8_t timeout = 0;

  g_write_data[0] = addr >> 8;
  g_write_data[1] = addr & 0xFF;

  ret = HAL_I2C_Master_Transmit(i2c01, SET_ADDRESS(device_addr, WRITE_ENABLE), g_write_data, 2, HAL_MAX_DELAY);

  for(timeout = 0; i2c01->State != HAL_I2C_STATE_READY && timeout < WRITE_TIMEOUT; timeout++)
  {
    //Wait for the send to stop
  }
}

//reads single byte
uint8_t eread(uint16_t addr)
{
  uint8_t value=6;

  esetAddress(addr);

  ret = HAL_I2C_Master_Receive(i2c01, SET_ADDRESS(device_addr, READ_ENABLE), &value, 1, HAL_MAX_DELAY);

  HAL_Delay(20);
  return value;
}

//reads chunk of data
void edownload(uint16_t from_addr, void* to_addr, uint16_t size)
{
  esetAddress(from_addr);
  HAL_Delay(10);
  ret = HAL_I2C_Master_Receive(i2c01, SET_ADDRESS(device_addr, READ_ENABLE), to_addr, size, HAL_MAX_DELAY);

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
  ret = HAL_I2C_Master_Transmit(i2c01, SET_ADDRESS(device_addr, WRITE_ENABLE), g_write_data, 3, HAL_MAX_DELAY);

  if(ret != HAL_OK)
  {
     HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
  }
  for(timeout = 0; i2c01->State != HAL_I2C_STATE_READY && timeout < WRITE_TIMEOUT; timeout++)
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

  HAL_Delay(5);
  ret = HAL_I2C_Master_Transmit(i2c01, SET_ADDRESS(device_addr, WRITE_ENABLE), buff, size + 2, HAL_MAX_DELAY);

  if(ret != HAL_OK)
  {
     HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
  }

  uint8_t timeout = 0;
  for(timeout = 0; i2c01->State != HAL_I2C_STATE_READY && timeout < WRITE_TIMEOUT; timeout++)
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

//links struct ptr with a header from eeprom, overwrite protect active high
void eLinkStruct(void* ptr, uint16_t size, char name[], uint8_t version, uint8_t overwrite_protection)
{
  struct HeaderNode* node = NULL;

  node = findHeader(name);

  if(version > MAX_VERSION) version = MAX_VERSION;

  uint8_t overwrite_previous;
  //if node found, extract overwrite bit from version
  if(node != NULL) splitVersion(&(node->version), &overwrite_previous);

  if(node == NULL)
  {
    //struct not in eeprom in any form
    node = eAddToList();
    strcpy(node->name, name);

    combineVersion(&version, &overwrite_protection);
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

    combineVersion(&version, &overwrite_protection);
    node->version = version;
    node->size = size;

    node->ptr = ptr; //link

    updateHeaderEntry(node);
    eupload(node->ptr, node->eAddress, node->size);

  }else if(overwrite_previous != overwrite_protection)
  {
    combineVersion(&version, &overwrite_protection);
    node->version = version;
    node->ptr = ptr; //link
    updateHeaderEntry(node);

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

      for(int i = 0; i < g_numStructs - 1; i++)//(current->next != NULL)
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
            //current=prev->next;
          }
        }else
        {
          prev = current;
          current = current->next;
        }

      }

      if(current != NULL)
      {
        headerLast = current;
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

/*returns eeprom address with space for set size
null if not available, relies on the fact that
the linked list is sorted by increasing
eaddress*/
uint16_t emalloc(uint16_t size)
{
  if(g_numStructs > 1)
  {
    struct HeaderNode* current = headerFirst;

    //check between end of headers and first node
    if(headerFirst->eAddress - (MAX_HEADER_COUNT * HEADER_SIZE + 1) >= size)
    {
      return MAX_HEADER_COUNT * HEADER_SIZE + 1;
    }

    //check between individual nodes
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
    return MAX_HEADER_COUNT * HEADER_SIZE + 1;
  }


}

//removes header from linked list
void removeFromList(char name[])
{

  //remove from list
  struct HeaderNode* currentNode = headerFirst;
  struct HeaderNode* bufferNode;

  if(strncmp(headerFirst->name, name, NAME_SIZE) == 0)
  {
    //first in list is match
    bufferNode = headerFirst;
    headerFirst = headerFirst->next;
    free(bufferNode);
  }else if(strncmp(headerLast->name, name, NAME_SIZE) == 0)
  {
    //last in list is match

    //get second to last node
    for(int i = 0; i < g_numStructs - 2; i++)
    {
      currentNode = currentNode->next;
    }

    headerLast = currentNode;
    free(headerLast->next);
    headerLast->next = NULL;

  }else
  {
    for(int i = 0; i < g_numStructs - 2; i++)
    {
      if(strncmp(currentNode->next->name, name, NAME_SIZE) == 0)
      {
        free(currentNode->next);
        currentNode->next = currentNode->next->next; //skip over deleted node
        i = g_numStructs;
      }

      currentNode = currentNode->next;
    }
  }

}

//everything necessary to remove an unused header
void deleteHeader(char name[])
{
  uint8_t header_buffer[HEADER_SIZE]; //stores last header entry in eeprom
  char name_buffer[NAME_SIZE];

  removeFromList(name);

  //copy last header info into header_buffer
  edownload((g_numStructs - 1) * HEADER_SIZE + 1, header_buffer, HEADER_SIZE);

  //find unused header pos and overwrite
  for(int i = 0; i < g_numStructs; i++)
  {
    edownload(i * HEADER_SIZE + 1, &name_buffer, NAME_SIZE);
    if(strncmp(name_buffer, name, NAME_SIZE) == 0)
    {
      //found the correct header to update
      eupload(header_buffer, i * HEADER_SIZE + 1, HEADER_SIZE);
      i = g_numStructs;
    }
  }

  //decrement num headers
  g_numStructs -= 1;
  ewrite(0, g_numStructs);

}

//removes unused headers (those without a linked pointer) from eeprom
void cleanHeaders()
{

  struct HeaderNode* currentNode = headerFirst;
  struct HeaderNode* nextNode = headerFirst->next;
  char name_buffer[NAME_SIZE];
  uint8_t header_buffer[HEADER_SIZE];

  uint8_t currentNum = g_numStructs;

  for(int i = 0; i < currentNum; i++)
  {
    //must save next node before deletion
    //since current node will be removed
    nextNode = currentNode->next;

    if(currentNode->ptr == NULL)
    {

      //unused header is currentNode, check for overwrite
      if(!(currentNode->version >> OVERWRITE_LOC)){
        deleteHeader(currentNode->name);
      }

    }

    currentNode = nextNode;
  }

}

//loads current header info
void eInitialize(I2C_HandleTypeDef* i2c, uint16_t eepromSpace, uint8_t address)
{
  i2c01 = i2c;
  eepromSize = eepromSpace;
  device_addr = address;


  g_numStructs = eread(0);

  loadHeaders();
  sortHeaders();

  //eWipe();

}

//loads struct from mem, returns 1 if unknown struct
uint8_t loadStruct(char name[])
{
  struct HeaderNode* current = headerFirst;

  for(int i = 0; i < g_numStructs; i++)
  {
    if(strncmp(name, current->name, NAME_SIZE) == 0)
    {
      //found desired node
      edownload(current->eAddress, current->ptr, current->size);
      return 0;
    }

    current = current->next;
  }

  return 1;
}

//saves struct to mem, returns 1 if unknown struct
uint8_t saveStruct(char name[])
{
  struct HeaderNode* current = headerFirst;

  for(int i = 0; i < g_numStructs; i++)
  {
    if(strncmp(name, current->name, NAME_SIZE) == 0)
    {
      //found desired node
      eupload(current->ptr, current->eAddress, current->size);
      return 0;
    }

    current = current->next;
  }

  return 1;
}

//splits version into overwrite and version
void splitVersion(uint8_t* version, uint8_t* overwrite)
{
  *overwrite = *version >> OVERWRITE_LOC;
  *version = *version & OVERWRITE_MASK;
}

//combines overwrite with version
void combineVersion(uint8_t* version, uint8_t* overwrite)
{
  *version = *version | (*overwrite << OVERWRITE_LOC);
}
//TODO cleanHeaders (if any not being used, maybe erase them
//overwrite with last header entry :) -> think through, be careful - sinc num header entries, will have a value in the linked list with a null ptr

//TODO look at cushioning between data

