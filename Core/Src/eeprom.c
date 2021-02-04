#include "eeprom.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

uint8_t g_write_data[4];
HAL_StatusTypeDef ret;

struct HeaderNode *headerFirst; //first node
struct HeaderNode *headerLast;  //last node

uint8_t g_numStructs; //number of entries in header

I2C_HandleTypeDef *i2c01;
uint16_t g_eeprom_size;
uint8_t g_device_addr;

void setAddress(uint16_t addr);
uint8_t readByte(uint16_t addr);
void downloadChunk(uint16_t from_addr, void *to_addr, uint16_t size);
void uploadByte(uint16_t addr, uint8_t val);
void uploadChunk(void *from_addr, uint16_t to_addr, uint16_t size);
struct HeaderNode *eAddToList();
struct HeaderNode *eFindHeader(char name[]);
void addHeaderEntry(struct HeaderNode *newHeader);
void updateHeaderEntry(struct HeaderNode *header);
void sortHeaders();
uint16_t spaceAvailable(uint16_t address);
uint16_t eepromMalloc(uint16_t size);
void removeHeaderFromList(char name[]);
void deleteHeader(char name[]);
void splitVersion(uint8_t *version, uint8_t *overwrite);
void combineVersion(uint8_t *version, uint8_t *overwrite);
void errorFound(enum EEPROM_ERROR error);
void loadHeaderEntries();

//sets 'cursor' in eeprom
void setAddress(uint16_t addr)
{
  uint8_t timeout = 0;

  g_write_data[0] = addr >> 8;
  g_write_data[1] = addr & 0xFF;
  HAL_Delay(10);
  ret = HAL_I2C_Master_Transmit(i2c01, SET_ADDRESS(g_device_addr, WRITE_ENABLE), g_write_data, 2, HAL_MAX_DELAY);

  if (ret != HAL_OK)
  {
    errorFound(COM_ERROR);
  }

  for (timeout = 0; i2c01->State != HAL_I2C_STATE_READY && timeout < WRITE_TIMEOUT; timeout++)
  {
    //Wait for the send to stop
  }

  if (timeout > WRITE_TIMEOUT)
  {
    errorFound(COM_TIMEOUT);
  }
}

//reads single byte
uint8_t readByte(uint16_t addr)
{
  uint8_t value;

  setAddress(addr);

  ret = HAL_I2C_Master_Receive(i2c01, SET_ADDRESS(g_device_addr, READ_ENABLE), &value, 1, HAL_MAX_DELAY);

  if (ret != HAL_OK)
  {
    errorFound(COM_ERROR);
  }

  HAL_Delay(5);
  return value;
}

//reads chunk of data
void downloadChunk(uint16_t from_addr, void *to_addr, uint16_t size)
{
  setAddress(from_addr);
  HAL_Delay(5);
  ret = HAL_I2C_Master_Receive(i2c01, SET_ADDRESS(g_device_addr, READ_ENABLE), to_addr, size, HAL_MAX_DELAY);

  if (ret != HAL_OK)
  {
    errorFound(COM_ERROR);
  }
}

//writes single byte
void uploadByte(uint16_t addr, uint8_t val)
{
  g_write_data[0] = addr >> 8;
  g_write_data[1] = addr & 0xFF;
  g_write_data[2] = val;

  uint8_t timeout = 0;
  HAL_Delay(5); //Was not working without a delay...
  ret = HAL_I2C_Master_Transmit(i2c01, SET_ADDRESS(g_device_addr, WRITE_ENABLE), g_write_data, 3, HAL_MAX_DELAY);

  if (ret != HAL_OK)
  {
    errorFound(COM_ERROR);
  }
  for (timeout = 0; i2c01->State != HAL_I2C_STATE_READY && timeout < WRITE_TIMEOUT; timeout++)
  {
    //Wait for the send to stop
  }
  if (timeout > WRITE_TIMEOUT)
  {
    errorFound(COM_TIMEOUT);
  }
}

//uploads chunk ignoring page breaks
void eUploadRaw(void *from_addr, uint16_t to_addr, uint16_t size)
{
  //convert type to allow copying
  uint8_t *from = from_addr;

  uint8_t *buff = malloc(size + 2);
  //TODO: remove buffer (have two transmit calls)
  //insert memory address
  buff[0] = to_addr >> 8;
  buff[1] = to_addr & 0xFF;

  //copy memory from from_addr to buffer
  for (uint16_t i = 0; i < size; i++)
  {
    buff[i + 2] = from[i];
  }

  HAL_Delay(5);
  ret = HAL_I2C_Master_Transmit(i2c01, SET_ADDRESS(g_device_addr, WRITE_ENABLE), buff, size + 2, HAL_MAX_DELAY);

  if (ret != HAL_OK)
  {
    errorFound(COM_ERROR);
  }

  uint8_t timeout = 0;
  for (timeout = 0; i2c01->State != HAL_I2C_STATE_READY && timeout < WRITE_TIMEOUT; timeout++)
  {
    //Wait for the send to stop
  }

  if (timeout > WRITE_TIMEOUT)
  {
    errorFound(COM_TIMEOUT);
  }

  free(buff); //TODO: don't forget this either
}

//breaks data into chunks to prevent crossing page boundary
void uploadChunk(void *from_addr, uint16_t to_addr, uint16_t size)
{
  uint16_t next_boundary = (to_addr / PAGE_SIZE + 1) * PAGE_SIZE;
  uint16_t current_addr = to_addr;
  uint16_t end_loc = to_addr + (size - 1);
  uint8_t *from = from_addr;
  uint8_t chunkSize; //number of bytes copying from mem

  do
  {
    //send from current to boundary or end loc, whichever is less
    if (end_loc - current_addr < next_boundary - current_addr)
    {
      chunkSize = end_loc - current_addr + 1;
    }
    else
    {
      chunkSize = next_boundary - current_addr;
    }

    eUploadRaw(from + (current_addr - to_addr), current_addr, chunkSize);
    HAL_Delay(5);
    current_addr += chunkSize;
    next_boundary = (current_addr / PAGE_SIZE + 1) * PAGE_SIZE;
  } while (current_addr < end_loc);
}

//transfers all values to given huart
void eepromDump(UART_HandleTypeDef huart)
{
  uint8_t MSG[PAGE_SIZE + 1] = {0};
  for (uint16_t i = 0; i < g_eeprom_size; i += PAGE_SIZE)
  {
    downloadChunk(i, MSG, PAGE_SIZE);
    HAL_UART_Transmit(&huart, MSG, sizeof(MSG) - 1, 100);
    HAL_Delay(10);
  }
}

//Sets all addresses to 0
void eepromWipe()
{
  uint8_t data[PAGE_SIZE] = {0};

  for (uint16_t i = 0; i < g_eeprom_size; i += PAGE_SIZE)
  {
    uploadChunk(data, i, 32);
  }
}

//adds header node, returns pointer
struct HeaderNode *eAddToList()
{
  struct HeaderNode *newNode = malloc(sizeof(struct HeaderNode));

  newNode->next = NULL;

  //For first node in list
  if (NULL == headerFirst)
  {
    headerFirst = newNode;
  }
  else
  {
    headerLast->next = newNode;
  }

  headerLast = newNode;

  return newNode;
}

//returns null if none
struct HeaderNode *eFindHeader(char name[])
{

  struct HeaderNode *currentHeader = headerFirst;

  //search through headers until name match
  for (int i = 0; i < g_numStructs; i++)
  {

    if (strncmp(name, currentHeader->name, NAME_SIZE) == 0)
    {
      return currentHeader;
    }

    currentHeader = currentHeader->next;
  }

  return NULL;
}

//adds Header to eeprom
void addHeaderEntry(struct HeaderNode *newHeader)
{

  g_numStructs += 1;
  uploadByte(0, g_numStructs); //increment struct num by 1

  if (g_numStructs > MAX_HEADER_COUNT)
  {
    errorFound(MAX_HEADER);
  }

  newHeader->address_on_eeprom = eepromMalloc(newHeader->size);

  uploadChunk(newHeader, (g_numStructs - 1) * HEADER_SIZE + 1, HEADER_SIZE);

  sortHeaders(); //added new item, put it in place
}

//finds location of header in eeprom and updates it
void updateHeaderEntry(struct HeaderNode *header)
{
  //somehow find where its located
  //current process is slower due to searching through actual eeprom mem
  char nameFound[NAME_SIZE];

  //converting allows for pointer addition
  uint8_t *headerLoc = header;

  for (int i = 0; i < g_numStructs; i++)
  {
    downloadChunk(i * HEADER_SIZE + 1, &nameFound, NAME_SIZE);
    if (strncmp(nameFound, header->name, NAME_SIZE) == 0)
    {
      //found the correct header to update
      uploadChunk(headerLoc + NAME_SIZE, i * HEADER_SIZE + 1 + NAME_SIZE, HEADER_SIZE - NAME_SIZE);
      return;
    }
  }
  //header not found, should never reach this point...
}

//links struct ptr with a header from eeprom, overwrite protect active high
void eepromLinkStruct(void *ptr, uint16_t size, char name[], uint8_t version, uint8_t overwrite_protection)
{
  struct HeaderNode *node = NULL;

  node = eFindHeader(name);

  if (version > MAX_VERSION)
    version = MAX_VERSION;

  uint8_t overwrite_previous;
  //if node found, extract overwrite bit from version
  if (node != NULL)
    splitVersion(&(node->version), &overwrite_previous);

  if (overwrite_protection != 0)
    overwrite_protection = 1;

  if (node == NULL)
  {
    //struct not in eeprom in any form
    node = eAddToList();
    strcpy(node->name, name);

    combineVersion(&version, &overwrite_protection);
    node->version = version;
    node->size = size;

    node->ptr_to_data = ptr; //link

    addHeaderEntry(node); //update eAddress too
    uploadChunk(node->ptr_to_data, node->address_on_eeprom, node->size);
  }
  else if (node->size != size || node->version != version)
  {
    //overwrite and header change

    if (spaceAvailable(node->address_on_eeprom) < node->size)
    {
      //can't place struct here, move
      node->address_on_eeprom = eepromMalloc(node->size);

      //change of address, sort headers
      sortHeaders();
    }

    combineVersion(&version, &overwrite_protection);
    node->version = version;
    node->size = size;

    node->ptr_to_data = ptr; //link

    updateHeaderEntry(node);
    uploadChunk(node->ptr_to_data, node->address_on_eeprom, node->size);
  }
  else if (overwrite_previous != overwrite_protection)
  {
    combineVersion(&version, &overwrite_protection);
    node->version = version;
    node->ptr_to_data = ptr; //link
    updateHeaderEntry(node);
  }
  else
  {
    //struct info matches that in eeprom
    node->ptr_to_data = ptr; //link
  }
}

//populate linked list with header info from eeprom
void loadHeaderEntries()
{

  for (int i = 0; i < g_numStructs; i++)
  {
    struct HeaderNode *header = malloc(sizeof(struct HeaderNode));
    header->next = NULL;
    header->ptr_to_data = NULL;

    if (headerFirst == NULL)
    {
      headerFirst = header;
    }
    else
    {
      headerLast->next = header;
    }

    headerLast = header;

    downloadChunk(i * HEADER_SIZE + 1, header, HEADER_SIZE);
  }
}

//sort headers by increasing eaddress
void sortHeaders()
{
  struct HeaderNode *prev;
  struct HeaderNode *current;
  struct HeaderNode *future;

  uint8_t swapped = 0;

  if (g_numStructs > 1)
  {

    do
    {
      prev = NULL;
      current = headerFirst;
      swapped = 0;

      for (int i = 0; i < g_numStructs - 1; i++) //(current->next != NULL)
      {

        future = current->next;

        if (current->address_on_eeprom > future->address_on_eeprom)
        {
          swapped = 1;
          if (prev == NULL)
          {
            current->next = future->next;
            future->next = current;
            headerFirst = future;

            prev = headerFirst;
          }
          else
          {
            prev->next = future;
            current->next = future->next;
            future->next = current;

            prev = prev->next;
            //current=prev->next;
          }
        }
        else
        {
          prev = current;
          current = current->next;
        }
      }

      if (current != NULL)
      {
        headerLast = current;
      }

    } while (swapped);
  }
}

//returns available space to use at an address
uint16_t spaceAvailable(uint16_t address)
{
  if (address > g_eeprom_size)
  {
    return 0;
  }

  //find header with first address greater than eAddress
  struct HeaderNode *curr = headerFirst;
  while (curr != NULL)
  {
    if (curr->address_on_eeprom > address)
    {
      return curr->address_on_eeprom - address;
    }

    curr = curr->next;
  }
  //no headers with address after said address
  return g_eeprom_size - address;
}

/*returns eeprom address with space for set size
null if not available, relies on the fact that
the linked list is sorted by increasing
eaddress*/
uint16_t eepromMalloc(uint16_t size)
{
  if (g_numStructs > 1)
  {
    struct HeaderNode *current = headerFirst;

    //check between end of headers and first node
    if (headerFirst->address_on_eeprom - (MAX_HEADER_COUNT * HEADER_SIZE + 1) >= size)
    {
      return MAX_HEADER_COUNT * HEADER_SIZE + 1;
    }

    //check between individual nodes
    while (current->next != NULL)
    {
      if (current->next->address_on_eeprom - (current->address_on_eeprom + current->size) >= size)
      {
        return current->address_on_eeprom + current->size;
      }
      current = current->next;
    }
    //reached last entry, check is space between last and end of eeprom

    if (g_eeprom_size - current->address_on_eeprom + current->size >= size)
    {
      return current->address_on_eeprom + current->size;
    }

    errorFound(MAX_MEM);
    return NULL; //no space available
  }
  else
  {
    return MAX_HEADER_COUNT * HEADER_SIZE + 1;
  }
}

//removes header from linked list
void removeHeaderFromList(char name[])
{

  //remove from list
  struct HeaderNode *currentNode = headerFirst;
  struct HeaderNode *bufferNode;

  if (strncmp(headerFirst->name, name, NAME_SIZE) == 0)
  {
    //first in list is match
    bufferNode = headerFirst;
    headerFirst = headerFirst->next;
    free(bufferNode);
  }
  else if (strncmp(headerLast->name, name, NAME_SIZE) == 0)
  {
    //last in list is match

    //get second to last node
    for (int i = 0; i < g_numStructs - 2; i++)
    {
      currentNode = currentNode->next;
    }

    headerLast = currentNode;
    free(headerLast->next);
    headerLast->next = NULL;
  }
  else
  {
    for (int i = 0; i < g_numStructs - 2; i++)
    {
      if (strncmp(currentNode->next->name, name, NAME_SIZE) == 0)
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

  removeHeaderFromList(name);

  //copy last header info into header_buffer
  downloadChunk((g_numStructs - 1) * HEADER_SIZE + 1, header_buffer, HEADER_SIZE);

  //find unused header pos and overwrite
  for (int i = 0; i < g_numStructs; i++)
  {
    downloadChunk(i * HEADER_SIZE + 1, &name_buffer, NAME_SIZE);
    if (strncmp(name_buffer, name, NAME_SIZE) == 0)
    {
      //found the correct header to update
      uploadChunk(header_buffer, i * HEADER_SIZE + 1, HEADER_SIZE);
      i = g_numStructs;
    }
  }

  //decrement num headers
  g_numStructs -= 1;
  uploadByte(0, g_numStructs);
}

//removes unused headers (those without a linked pointer) from eeprom
void eepromCleanHeaders()
{

  struct HeaderNode *currentNode = headerFirst;
  struct HeaderNode *nextNode = headerFirst->next;

  uint8_t currentNum = g_numStructs;

  for (int i = 0; i < currentNum; i++)
  {
    //must save next node before deletion
    //since current node will be removed
    nextNode = currentNode->next;

    if (currentNode->ptr_to_data == NULL)
    {

      //unused header is currentNode, check for overwrite
      if (!(currentNode->version >> OVERWRITE_LOC))
      {
        deleteHeader(currentNode->name);
      }
    }

    currentNode = nextNode;
  }
}

//loads current header info
void eepromInitialize(I2C_HandleTypeDef *i2c, uint16_t eepromSpace, uint8_t address)
{
  i2c01 = i2c;
  g_eeprom_size = eepromSpace;
  g_device_addr = address;

  g_numStructs = readByte(0);

  loadHeaderEntries();
  sortHeaders();

  //eWipe();
}

//loads struct from mem, returns 1 if unknown struct
uint8_t eepromLoadStruct(char name[])
{
  struct HeaderNode *current = headerFirst;

  for (int i = 0; i < g_numStructs; i++)
  {
    if (strncmp(name, current->name, NAME_SIZE) == 0)
    {
      //found desired node
      downloadChunk(current->address_on_eeprom, current->ptr_to_data, current->size);
      return 0;
    }

    current = current->next;
  }

  return 1;
}

//saves struct to mem, returns 1 if unknown struct
uint8_t eepromSaveStruct(char name[])
{
  struct HeaderNode *current = headerFirst;

  for (int i = 0; i < g_numStructs; i++)
  {
    if (strncmp(name, current->name, NAME_SIZE) == 0)
    {
      //found desired node
      uploadChunk(current->ptr_to_data, current->address_on_eeprom, current->size);
      return 0;
    }

    current = current->next;
  }

  return 1;
}

//splits version into overwrite and version
void splitVersion(uint8_t *version, uint8_t *overwrite)
{
  *overwrite = *version >> OVERWRITE_LOC;
  *version = *version & OVERWRITE_MASK;
}

//combines overwrite with version
void combineVersion(uint8_t *version, uint8_t *overwrite)
{
  *version = *version | (*overwrite << OVERWRITE_LOC);
}

void errorFound(enum EEPROM_ERROR error)
{
  switch (error)
  {
  case COM_TIMEOUT:
  case COM_ERROR:
  case MAX_HEADER:
  case MAX_MEM:
    while (1)
      ;
    break;
  }
}
