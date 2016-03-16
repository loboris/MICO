/**
******************************************************************************
* @file    BootloaderEntrance.c 
* @author  William Xu
* @version V2.0.0
* @date    05-Oct-2014
* @brief   MICO bootloader main entrance.
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


#include "mico.h"
#include "platform.h"
#include "platformInternal.h"
#include "platform_config.h"
#include "bootloader.h"
#include <spiffs.h>
#include <spiffs_nucleus.h>

#define boot_log(M, ...) custom_log("BOOT", M, ##__VA_ARGS__)
#define boot_log_trace() custom_log_trace("BOOT")

extern void Main_Menu(void);
//extern OSStatus update(void);

#define LOG_BLOCK_SIZE_K  16
#define LOG_PAGE_SIZE     128
#define LOG_BLOCK_SIZE    LOG_BLOCK_SIZE_K*1024
#define PHYS_ERASE_BLOCK  LOG_BLOCK_SIZE
#define FILE_NOT_OPENED   0
static u8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
static u8_t spiffs_fds[32*4];

spiffs fs;
volatile int file_fd = FILE_NOT_OPENED;

//------------------------------------------------------------
static s32_t lspiffs_read(u32_t addr, u32_t size, u8_t *dst) {
  u32_t adr = addr;
  MicoFlashRead(MICO_PARTITION_LUA, &adr,dst,size);
  return SPIFFS_OK;
}

//-------------------------------------------------------------
static s32_t lspiffs_write(u32_t addr, u32_t size, u8_t *src) {
  return SPIFFS_OK;
}

//--------------------------------------------------
static s32_t lspiffs_erase(u32_t addr, u32_t size) {
  return SPIFFS_OK;
} 

//---------------
void _mount(void)
{
  mico_logic_partition_t* part;
  spiffs_config cfg;    

  part = MicoFlashGetInfo( MICO_PARTITION_LUA );
      
  cfg.phys_size = part->partition_length;
  cfg.phys_addr = 0;                        // start at beginning of the LUA partition
  cfg.log_block_size = LOG_BLOCK_SIZE;      // logical block size
  cfg.phys_erase_block = PHYS_ERASE_BLOCK;  // logical erase block size
  cfg.log_page_size = LOG_PAGE_SIZE;        // logical page size
  
  cfg.hal_read_f = lspiffs_read;
  cfg.hal_write_f = lspiffs_write;
  cfg.hal_erase_f = lspiffs_erase;

  int res = SPIFFS_mount(&fs,
    &cfg,
    spiffs_work_buf,
    spiffs_fds,
    sizeof(spiffs_fds),
    NULL,
    0,
    NULL);
}


char menu[] =
"\r\n"
"MICO bootloader for %s, %s\r\nHARDWARE_REVISION: %s\r\n"
"+ command --------------------+ function --------------+\r\n"
"| 0:BOOTUPDATE    <-r> <-f>   | Update bootloader      |\r\n"
"| 1:FWUPDATE      <-r> <-f>   | Update application     |\r\n"
"| 2:DRIVERUPDATE  <-r> <-f>   | Update RF driver       |\r\n"
"| 3:PARAUPDATE    <-r> <-e>   | Update LUA settings    |\r\n"
"| 4:FLASHUPDATE               | Update flash content   |\r\n"
"|   <-dev dv|-i|-s> <-e> <-r> |                        |\r\n"
"|   <-start addr><-end addr>  |                        |\r\n"
"| 5:MEMORYMAP                 | List flash memory map  |\r\n"
"| 6:BOOT                      | Excute application     |\r\n"
"| 7:REBOOT                    | Reboot                 |\r\n"
"| 8:ERASELUAFLASH             | Erase Lua SPIFlash FS  |\r\n"
"| 9:FILELIST                  | List files on LUA FS   |\r\n"
"+-----------------------------+------------------------+\r\n"
"| (C) COPYRIGHT 2015 MXCHIP Corporation                |\r\n"
"|     Modified by LoBo 03/2016                         |\r\n"
"+------------------------------------------------------+\r\n"
"| Notes: use ymodem protocol for file upload/download) |\r\n"
"|                                                      |\r\n"
"|  -e   Erase only         -r  Read&upload from flash  |\r\n"
"|                          -f  Program flash from file |\r\n"
"|  -i   Internal flash     -s  SPI flash               |\r\n"
"|  -end Flash end address  -start Flash start address  |\r\n"
"|  -dev Flash device number: 0=internal, 1=spi         |\r\n"
"|                                                      |\r\n"
"| Example:                                             |\r\n"
"|   Enter '1' or 'FWUPDATE'                            |\r\n"
"|         to update application in embedded flash      |\r\n"
"|   Enter '1 -f' or 'FWUPDATE -f'                      |\r\n"
"|         to update application from file              |\r\n"
"|   Enter '4 -r -s -start 0x00040000 -end 0x001fffff'  |\r\n"
"|         to upload dump of Lua file system            |\r\n"
"+------------------------------------------------------+\r\n";


#ifdef MICO_ENABLE_STDIO_TO_BOOT
extern int stdio_break_in(void);
#endif

static void enable_protection( void )
{
  mico_partition_t i;
  mico_logic_partition_t *partition;

  for( i = MICO_PARTITION_BOOTLOADER; i < MICO_PARTITION_MAX; i++ ){
    partition = MicoFlashGetInfo( i );
    if( PAR_OPT_WRITE_DIS == ( partition->partition_options & PAR_OPT_WRITE_MASK )  )
      MicoFlashEnableSecurity( i, 0x0, MicoFlashGetInfo(i)->partition_length );
  }
}

WEAK bool MicoShouldEnterBootloader( void )
{
  return false;
}

/*
WEAK bool MicoShouldEnterMFGMode( void )
{
  return false;
}

WEAK bool MicoShouldEnterATEMode( void )
{
  return false;
}
*/
void bootloader_start_app( uint32_t app_addr )
{
  enable_protection( );
  startApplication( app_addr );
}


int main(void)
{
  //mico_logic_partition_t *partition;
  
  init_clocks();
  init_memory();
  init_architecture();
  init_platform_bootloader();

  mico_set_bootload_ver();
  
  //update();

  enable_protection();

#ifdef MICO_ENABLE_STDIO_TO_BOOT
  if (stdio_break_in() == 1)
    goto BOOT;
#endif
  
  if( MicoShouldEnterBootloader() == false )
    bootloader_start_app( (MicoFlashGetInfo(MICO_PARTITION_APPLICATION))->partition_start_addr );
  /*else if( MicoShouldEnterMFGMode() == true )
    bootloader_start_app( (MicoFlashGetInfo(MICO_PARTITION_APPLICATION))->partition_start_addr );
  else if( MicoShouldEnterATEMode() ){
    partition = MicoFlashGetInfo( MICO_PARTITION_ATE );
    if (partition->partition_owner != MICO_FLASH_NONE) {
      bootloader_start_app( partition->partition_start_addr );
    }
  }*/

#ifdef MICO_ENABLE_STDIO_TO_BOOT
BOOT:
#endif
  
  _mount();
  printf ( menu, MODEL, Bootloader_REVISION, HARDWARE_REVISION );

  while(1){                             
    Main_Menu ();
  }
}


