/**
******************************************************************************
* @file    menu.c 
* @author  William Xu, modified by LoBo
* @version V2.0.0
* @date    05-Oct-2014 ~ 08-Mar-2016
* @brief   his file provides the software which contains the main menu routine.
*          The main menu gives the options of:
*             - downloading a new binary file, 
*             - uploading internal flash memory,
*             - executing the binary file already loaded 
******************************************************************************
*
*  The MIT License
*  Copyright (c) 2014 MXCHIP Inc.
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy 
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights 
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is furnished
*  to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in
*  all copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
*  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR 
*  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include "mico.h"
#include "ymodem.h"
#include "platform_config.h"
#include "platformInternal.h"
#include "StringUtils.h"
#include "bootloader.h"
#include <ctype.h>                    
#include <spiffs.h>
#include <spiffs_nucleus.h>

extern spiffs fs;
extern volatile int file_fd;
#define FILE_NOT_OPENED 0

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define CMD_STRING_SIZE       128
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern platform_flash_t platform_flash_peripherals[];

uint8_t tab_1024[1024] =
{
  0
};

char FileName[FILE_NAME_LENGTH];
char ERROR_STR [] = "\n\r*** ERROR: %s\n\r";    /* ERROR message string in code   */

extern char menu[];
extern void getline (char *line, int n);          /* input line               */
extern void startApplication( uint32_t app_addr );

/* Private function prototypes -----------------------------------------------*/
void SerialDownload(mico_flash_t flash, uint32_t flashdestination, int32_t maxRecvSize);
void SerialUpload(mico_flash_t flash, uint32_t flashdestination, char * fileName, int32_t maxRecvSize);

/* Private functions ---------------------------------------------------------*/
/**
* @brief  Analyse a command parameter
* @param  commandBody: command string address
* @param  para: The para we are looking for
* @param  paraBody: A pointer to the buffer to receive the para body.
* @param  paraBodyLength: The length, in bytes, of the buffer pointed to by the paraBody parameter.
* @retval the actual length of the paraBody received, -1 means failed to find this paras 
*/
int findCommandPara(char *commandBody, char *para, char *paraBody, int paraBodyLength)
{
  int i = 0;
  int k, j;
  int retval = -1;
  char para_in_ram[100];
  strncpy(para_in_ram, para, 100);
  
  for (i = 0; para_in_ram[i] != 0; i++)  {                /* convert to upper characters */
    para_in_ram[i] = toupper(para_in_ram[i]);
  }
  
  i = 0;
  while(commandBody[i] != 0) {
    if(commandBody[i] == '-' ){
      for(j=i+1, k=0; *(para_in_ram+k)!=0x0; j++, k++ ){
        if(commandBody[j] != *(para_in_ram+k)){
          break;
        } 
      }
      
      if(*(para+k)!=0x0 || (commandBody[j]!=' '&& commandBody[j]!=0x0)){   /* para not found!             */
        i++;
        continue;
      }
      
      retval = 0;
      for (k = j+1; commandBody[k] == ' '; k++);      /* skip blanks                 */
      for(j = 0; commandBody[k] != ' ' && commandBody[k] != 0 && commandBody[k] != '-'; j++, k++){   /* para body found!             */
        if(paraBody) paraBody[j] = commandBody[k];
        retval ++;
        if( retval == paraBodyLength) goto exit;
      }
      goto exit;
    }
    i++;
  }
  
exit:
  if(paraBody) paraBody[retval] = 0x0;
  return retval;
}


/**
* @brief  Download a file via serial port
* @param  None
* @retval None
*/
void SerialDownload(mico_flash_t flash, uint32_t flashdestination, int32_t maxRecvSize)
{
  char Number[10] = "          ";
  int32_t Size = 0;
  
  printf("Waiting for the file to be sent ... (press 'a' to abort)\n\r");
  Size = Ymodem_Receive( &tab_1024[0], flash, flashdestination, maxRecvSize );
  if (Size > 0)
  {
    printf("\n\n\r Successfully!\n\r\r\n Name: %s", FileName);
    
    Int2Str((uint8_t *)Number, Size);
    printf("\n\r Size: %s Bytes\r\n", Number);
  }
  else if (Size == -1)
  {
    printf("\n\n\rImage size is higher than memory!\n\r");
  }
  else if (Size == -2)
  {
    printf("\n\n\rVerification failed!\n\r");
  }
  else if (Size == -3)
  {
    printf("\r\n\nAborted.\n\r");
  }
  else
  {
    printf("\n\rReceive failed!\n\r");
  }
}

/**
* @brief  Upload a file via serial port.
* @param  None
* @retval None
*/
void SerialUpload(mico_flash_t flash, uint32_t flashdestination, char * fileName, int32_t maxRecvSize)
{
  uint8_t status = 0;
  uint8_t key;
  
  printf("Select Receive File\n\r");
  MicoUartRecv( STDIO_UART, &key, 1, MICO_NEVER_TIMEOUT );
  
  if (key == CRC16)
  {
    /* Transmit the flash image through ymodem protocol */
    status = Ymodem_Transmit(flash, flashdestination, (uint8_t *)fileName, maxRecvSize);
    
    if (status != 0)
    {
      printf("\n\rError while Transmitting\n\r");
    }
    else
    {
      printf("\n\rSuccessfully\n\r");
    }
  }
}

/**
* @brief  Flash application from file
* @param  None
* @retval None
*/
void FlashFile(mico_flash_t flash, uint32_t flashdestination, char * fileName, int32_t maxSize)
{
  int32_t Size = 0;
  
  printf("\r\nFlashing from file\n\r");

  if (SPIFFS_mounted(&fs) == false) {
    printf("FS not mounted!\r\n");
    return;
  }

  if (FILE_NOT_OPENED != file_fd) {
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
  }
  
  file_fd = SPIFFS_open(&fs, fileName, SPIFFS_RDONLY, 0);
  if (file_fd < FILE_NOT_OPENED) {
    file_fd = FILE_NOT_OPENED;
    printf("File '%s' not found!\r\n", fileName);
    return;
  }

  spiffs_stat s;
  SPIFFS_fstat(&fs, file_fd, &s);
  
  Size = s.size;
  if (Size > maxSize) {
    printf("\r\nFile size (%d) too bigg for partition (%d)!\r\n", Size, maxSize);
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
    return;
  }

  printf("Erasing flash partition ... ");  
  if (platform_flash_erase(&platform_flash_peripherals[flash], flashdestination, flashdestination+maxSize-1) != 0) {
    printf("error!\r\n");
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
    return;
  }
  printf("done.\r\n");
  
  uint32_t read = 0;
  uint32_t tot_read = 0;
  uint32_t addr = flashdestination;
  do {
    read = SPIFFS_read(&fs, (spiffs_file)file_fd, &tab_1024[0], 1024);
    if (read > 0) {
      tot_read += read;
      if ((addr-flashdestination+1024) >= maxSize) {
        printf("\r\nPartition size exceded!\r\n");
        break;
      }
      if (platform_flash_write(&platform_flash_peripherals[flash], &addr, &tab_1024[0], (uint32_t)read)  == 0) {
        printf("\rProgress: %d of %d", tot_read, Size);
      }
      else {
        printf("\r\nError writing to Flash!\r\n");
        break;
      }
      if (read < 1024) break;
    }
  } while (read > 0);

  printf("\r\nFinished.");
  SPIFFS_close(&fs,file_fd);
  file_fd = FILE_NOT_OPENED;
}

//==========================
static int file_list( void )
{
  if (SPIFFS_mounted(&fs) == false) {
    printf("FS not mounted!\r\n");
    return -1;
  }

  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;
  uint8_t len = 0;

  SPIFFS_opendir(&fs, "/", &d);
  while ((pe = SPIFFS_readdir(&d, pe))) {
    uint8_t nlen = strlen((char*)pe->name);
    if (nlen > len) len = nlen;
  }

  if (len > 0) {
    d.block = 0;
    d.entry = 0;
    if (len < 4) len = 4;
    memset(&e, 0x00, sizeof(e));
    pe = &e;
    SPIFFS_opendir(&fs, "/", &d);
    printf("\r\n %*s  %s\r\n", len, "Name", "Size");
    printf(" %*s  %s\r\n", len, "----", "----");
    while ((pe = SPIFFS_readdir(&d, pe))) {
      if (pe->name[strlen((char *)pe->name)-1] == '/') {
        printf(" %*s  <DIR>\r\n", len, pe->name);
      }
      else {
        printf(" %*s  %i\r\n", len, pe->name, pe->size);
      }
    }
  }
  else printf("\r\nNo files!\r\n");
  SPIFFS_closedir(&d);
  
  return 0;
}


/**
* @brief  Display the Main Menu on HyperTerminal
* @param  None
* @retval None
*/
void Main_Menu(void)
{
  char cmdbuf [CMD_STRING_SIZE] = {0}, cmdname[15] = {0};     /* command input buffer        */
  int i, j;                                       /* index for command buffer    */
  char startAddressStr[10], endAddressStr[10], flash_dev_str[4];
  int32_t startAddress, endAddress;
  bool inputFlashArea = false;
  mico_logic_partition_t *partition;
  mico_flash_t flash_dev;
  OSStatus err = kNoErr;
  
  while (1)  {                                    /* loop forever                */
    printf ("\n\rWiFiMCU> ");
    getline (&cmdbuf[0], sizeof (cmdbuf));        /* input command line          */
    
    for (i = 0; cmdbuf[i] == ' '; i++);           /* skip blanks on head         */
    for (; cmdbuf[i] != 0; i++)  {                /* convert to upper characters */
      cmdbuf[i] = toupper(cmdbuf[i]);
    }
    
    for (i = 0; cmdbuf[i] == ' '; i++);        /* skip blanks on head         */
    for(j=0; cmdbuf[i] != ' '&&cmdbuf[i] != 0; i++,j++)  {         /* find command name       */
      cmdname[j] = cmdbuf[i];
    }
    cmdname[j] = '\0';
    
    /***************** Command "0" or "BOOTUPDATE": Update the application  *************************/
    if(strcmp(cmdname, "BOOTUPDATE") == 0 || strcmp(cmdname, "0") == 0) {
      partition = MicoFlashGetInfo( MICO_PARTITION_BOOTLOADER );
      if (findCommandPara(cmdbuf, "r", NULL, 0) != -1){
        printf ("\n\rRead Bootloader...\n\r");
        SerialUpload( partition->partition_owner, partition->partition_start_addr, "BootLoaderImage.bin", partition->partition_length );
        continue;
      }
      if (findCommandPara(cmdbuf, "f", NULL, 0) != -1) {
        err = MicoFlashDisableSecurity( MICO_PARTITION_BOOTLOADER, 0x0, partition->partition_length );
        require_noerr( err, exit);
        FlashFile( partition->partition_owner, partition->partition_start_addr, "Bootloader.bin", partition->partition_length ); 							   	
        continue;
      }
      printf ("\n\rUpdating Bootloader...\n\r");
      err = MicoFlashDisableSecurity( MICO_PARTITION_BOOTLOADER, 0x0, partition->partition_length );
      require_noerr( err, exit);

      SerialDownload( partition->partition_owner, partition->partition_start_addr, partition->partition_length );
    }
    
    /***************** Command "1" or "FWUPDATE": Update the MICO application  *************************/
    else if(strcmp(cmdname, "FWUPDATE") == 0 || strcmp(cmdname, "1") == 0)	{
      partition = MicoFlashGetInfo( MICO_PARTITION_APPLICATION );
      if (findCommandPara(cmdbuf, "r", NULL, 0) != -1){
        printf ("\n\rRead application...\n\r");
        SerialUpload( partition->partition_owner, partition->partition_start_addr, "ApplicationImage.bin", partition->partition_length );
        continue;
      }
      if (findCommandPara(cmdbuf, "f", NULL, 0) != -1){
        err = MicoFlashDisableSecurity( MICO_PARTITION_APPLICATION, 0x0, partition->partition_length );
        require_noerr( err, exit);
        FlashFile( partition->partition_owner, partition->partition_start_addr, "WiFiMCU_Lua.bin", partition->partition_length ); 							   	
        continue;
      }
      printf ("\n\rUpdating application...\n\r");
      err = MicoFlashDisableSecurity( MICO_PARTITION_APPLICATION, 0x0, partition->partition_length );
      require_noerr( err, exit);
      SerialDownload( partition->partition_owner, partition->partition_start_addr, partition->partition_length ); 							   	
    }
    
    /***************** Command "2" or "DRIVERUPDATE": Update the RF driver  *************************/
    else if(strcmp(cmdname, "DRIVERUPDATE") == 0 || strcmp(cmdname, "2") == 0) {
      partition = MicoFlashGetInfo( MICO_PARTITION_RF_FIRMWARE );
      if( partition == NULL ){
        printf ("\n\rNo flash memory for RF firmware, exiting...\n\r");
        continue;
      }
      
      if (findCommandPara(cmdbuf, "r", NULL, 0) != -1) {
        printf ("\n\rRead RF firmware...\n\r");
        SerialUpload( partition->partition_owner, partition->partition_start_addr, "RFDriverImage.bin", partition->partition_length );
        continue;
      }
      if (findCommandPara(cmdbuf, "f", NULL, 0) != -1) {
        err = MicoFlashDisableSecurity( MICO_PARTITION_RF_FIRMWARE, 0x0, partition->partition_length );
        require_noerr( err, exit);
        FlashFile( partition->partition_owner, partition->partition_start_addr, "RFDriver.bin", partition->partition_length ); 							   	
        continue;
      }
      printf ("\n\rUpdating RF driver...\n\r");
      err = MicoFlashDisableSecurity( MICO_PARTITION_RF_FIRMWARE, 0x0, partition->partition_length );
      require_noerr( err, exit);
      SerialDownload( partition->partition_owner, partition->partition_start_addr, partition->partition_length );    
    }
    
    /***************** Command "3" or "PARAUPDATE": Update the application  *************************/
    else if(strcmp(cmdname, "PARAUPDATE") == 0 || strcmp(cmdname, "3") == 0)  {
      partition = MicoFlashGetInfo( MICO_PARTITION_PARAMETER_1 );

      if (findCommandPara(cmdbuf, "e", NULL, 0) != -1){
        printf ("\n\rErasing settings...\n\r");

        err = MicoFlashDisableSecurity( MICO_PARTITION_PARAMETER_1, 0x0, partition->partition_length );
        require_noerr( err, exit);
        MicoFlashErase( MICO_PARTITION_PARAMETER_1, 0x0, partition->partition_length );
        continue;
      }
      if (findCommandPara(cmdbuf, "r", NULL, 0) != -1){
        printf ("\n\rRead settings...\n\r");
        SerialUpload( partition->partition_owner, partition->partition_start_addr, "DriverImage.bin", partition->partition_length );
        continue;
      }
      printf ("\n\rUpdating settings...\n\r");
      err = MicoFlashDisableSecurity( MICO_PARTITION_PARAMETER_1, 0x0, partition->partition_length );
      require_noerr( err, exit);
      SerialDownload( partition->partition_owner, partition->partition_start_addr, partition->partition_length );                        
    }
    
    /***************** Command "8" or "ERASELUAFLASH": : Erase Lua SPI FLASH FS  *********************/
    else if(strcmp(cmdname, "ERASELUAFLASH") == 0 || strcmp(cmdname, "8") == 0) {
      partition = MicoFlashGetInfo( MICO_PARTITION_LUA );
      printf ("\n\rErasing Lua SPI FLASH FS content...\n\r");
      MicoFlashErase( MICO_PARTITION_LUA, 0x0, partition->partition_length );
    }
    
    /***************** Command "9" or "FILELIST": : List files on LUA FS  *********************/
    else if(strcmp(cmdname, "FILELIST") == 0 || strcmp(cmdname, "9") == 0) {
      file_list();
    }
    
    /***************** Command "4" or "FLASHUPDATE": : Update the Flash  *************************/
    else if(strcmp(cmdname, "FLASHUPDATE") == 0 || strcmp(cmdname, "4") == 0) {
      // WiFiMCU Studio compatibility
      if (findCommandPara(cmdbuf, "i", NULL, 0) != -1){
        flash_dev = MICO_FLASH_EMBEDDED;
      }else if(findCommandPara(cmdbuf, "s", NULL, 200) != -1){
        flash_dev = MICO_FLASH_SPI;
      }else{
        if (findCommandPara(cmdbuf, "dev", flash_dev_str, 1) == -1  ){
          printf ("\n\rUnkown target type! Exiting...\n\r");
          continue;
        }
        
        if(Str2Int((uint8_t *)flash_dev_str, (int32_t *)&flash_dev)==0){ 
          printf ("\n\rDevice Number Err! Exiting...\n\r");
          continue;
        }
        if( flash_dev >= MICO_FLASH_MAX ){
          printf ("\n\rDevice Err! Exiting...\n\r");
          continue;
        }
      }
      
      inputFlashArea = false;
      
      if (findCommandPara(cmdbuf, "start", startAddressStr, 10) != -1){
        if(Str2Int((uint8_t *)startAddressStr, &startAddress)==0){      //Found Flash start address
          printf ("\n\rIllegal start address.\n\r");
          continue;
        }else{
          if (findCommandPara(cmdbuf, "end", endAddressStr, 10) != -1){ //Found Flash end address
            if(Str2Int((uint8_t *)endAddressStr, &endAddress)==0){
              printf ("\n\rIllegal end address.\n\r");
              continue;
            }else{
              inputFlashArea = true;
            }
          }else{
            printf ("\n\rFlash end address not found.\n\r");
            continue;
          }
        }
      }
      
      if(endAddress<startAddress && inputFlashArea == true) {
        printf ("\n\rIllegal address.\n\r");
        continue;
      }
      
      if(inputFlashArea != true){
        startAddress = platform_flash_peripherals[ flash_dev ].flash_start_addr ;
        endAddress = platform_flash_peripherals[ flash_dev ].flash_start_addr 
          + platform_flash_peripherals[ flash_dev ].flash_length - 1;
      }
      
      if (findCommandPara(cmdbuf, "e", NULL, 0) != -1){
        printf ("\n\rErasing dev%d content From 0x%x to 0x%x\n\r", flash_dev, startAddress, endAddress);
        platform_flash_init( &platform_flash_peripherals[ flash_dev ] );
        platform_flash_disable_protect( &platform_flash_peripherals[ flash_dev ], startAddress, endAddress );
        platform_flash_erase( &platform_flash_peripherals[ flash_dev ], startAddress, endAddress );
        continue;
      }
      
      if (findCommandPara(cmdbuf, "r", NULL, 0) != -1){
        printf ("\n\rRead dev%d content From 0x%x to 0x%x\n\r", flash_dev, startAddress, endAddress);
        SerialUpload(flash_dev, startAddress, "FlashImage.bin", endAddress-startAddress+1);
        continue;
      }
      
      printf ("\n\rUpdating dev%d content From 0x%x to 0x%x\n\r", flash_dev, startAddress, endAddress);
      platform_flash_disable_protect( &platform_flash_peripherals[ flash_dev ], startAddress, endAddress );
      SerialDownload(flash_dev, startAddress, endAddress-startAddress+1);                           
    }
    
    
    /***************** Command: MEMORYMAP *************************/
    else if(strcmp(cmdname, "MEMORYMAP") == 0 || strcmp(cmdname, "5") == 0)  {
      printf("\r");
      printf( "+--------------------------------------------------+\r\n");
      printf( "| Description |  Device  |    Start   |   Length   |\r\n");
      printf( "+-------------+----------+------------+------------+\r\n");
      for( i = 0; i <= MICO_PARTITION_PARAMETER_2; i++ ){
        partition = MicoFlashGetInfo( (mico_partition_t)i );
        if (partition->partition_owner == MICO_FLASH_NONE)
            continue;
        printf( "| %11s |  Dev: %d  | 0x%08x | 0x%08x |\r\n", partition->partition_description, partition->partition_owner, 
               partition->partition_start_addr, partition->partition_length);
      }
      printf( "+--------------------------------------------------+\r\n");
    }
    /***************** Command: Excute the application *************************/
    else if(strcmp(cmdname, "BOOT") == 0 || strcmp(cmdname, "6") == 0)	{
      printf ("\n\rBooting.......\n\r");
      partition = MicoFlashGetInfo( MICO_PARTITION_APPLICATION );
      bootloader_start_app( partition->partition_start_addr );
    }
    
    /***************** Command: Reboot *************************/
    else if(strcmp(cmdname, "REBOOT") == 0 || strcmp(cmdname, "7") == 0)  {
      printf ("\n\rReBooting.......\n\r");
      MicoSystemReboot();
      break;                              
    }
    
    else if(strcmp(cmdname, "HELP") == 0 || strcmp(cmdname, "?") == 0)	{
        printf ( menu, MODEL, Bootloader_REVISION, HARDWARE_REVISION );  /* display command menu        */
      break;
    }
    
    else if(strcmp(cmdname, "") == 0 )	{                         
      break;
    }
    else{
      printf (ERROR_STR, "UNKNOWN COMMAND");
      break;
    }

exit:
    continue;
  }
}
