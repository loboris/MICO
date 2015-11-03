/**
******************************************************************************
* @file    platform.c 
* @author  William Xu
* @version V1.0.0
* @date    05-May-2014
* @brief   This file provides all MICO Peripherals mapping table and platform
*          specific funcgtions.
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

#include "stdio.h"
#include "string.h"

#include "mico_platform.h"
#include "platform.h"
#include "platform_peripheral.h"
#include "platform_config.h"
#include "PlatformLogging.h"
#include "wlan_platform_common.h"
#include "sd_card.h"
#include "nvm.h"


/******************************************************
*                      Macros
******************************************************/

/******************************************************
*                    Constants
******************************************************/
//keep consist with miscfg.h definition
#define UPGRADE_NVM_ADDR        (176)//boot upgrade information at NVRAM address
#define UPGRADE_ERRNO_NOERR   (-1) //just initialization after boot up
#define UPGRADE_ERRNO_ENOENT  (-2) //no such file open by number
#define UPGRADE_ERRNO_EIO   (-5) //read/write error
#define UPGRADE_ERRNO_E2BIG   (-7) //too big than flash capacity
#define UPGRADE_ERRNO_EBADF   (-9) //no need to upgrade
#define UPGRADE_ERRNO_EFAULT  (-14) //address fault
#define UPGRADE_ERRNO_EBUSY   (-16) //flash lock fail
#define UPGRADE_ERRNO_ENODEV  (-19) //no upgrade device found
#define UPGRADE_ERRNO_ENODATA (-61) //flash is empty
#define UPGRADE_ERRNO_EPROTOTYPE (-91) //bad head format("MVO\x12")
#define UPGRADE_ERRNO_ELIBBAD (-80) //CRC error
#define UPGRADE_ERRNO_USBDEV  (-81) //no upgrade USB device
#define UPGRADE_ERRNO_SDDEV   (-82) //no upgrade SD device
#define UPGRADE_ERRNO_USBFILE (-83) //no upgrade file found in USB device
#define UPGRADE_ERRNO_SDFILE  (-84) //no upgrade file found in SD device
#define UPGRADE_ERRNO_NOBALL  (-85) //no upgrade ball in USB & SD device
#define UPGRADE_ERRNO_CODEMGC (-86) //wrong code magic number
#define UPGRADE_ERRNO_CODBUFDAT (-87) //code successful but data fail,because of constant data offset setting

#define UPGRADE_SUCC_MAGIC    (0x57F9B3C8) //just a successful magic
#define UPGRADE_REQT_MAGIC    (0x9ab4d18e) //just a request magic
#define UPGRADE_ERRNO_LASTCLR (0x581f9831) //just a clear magic

#define UDISK_PORT_NUM		        2		// USB PORT

/******************************************************
*                   Enumerations
******************************************************/

/******************************************************
*                 Type Definitions
******************************************************/

/******************************************************
*                    Structures
******************************************************/

/******************************************************
*               Function Declarations
******************************************************/
extern WEAK void PlatformEasyLinkButtonClickedCallback(void);
extern WEAK void PlatformStandbyButtonClickedCallback(void);
extern WEAK void PlatformEasyLinkButtonLongPressedCallback(void);

/******************************************************
*               Variables Definitions
******************************************************/

static uint32_t _default_start_time = 0;
static mico_timer_t _button_EL_timer;

const platform_gpio_t platform_gpio_pins[] =
{
//  /* Common GPIOs for internal use */
    //  /* GPIOs for external use */
    [MICO_GPIO_1]                         = {GPIOB, 27},
    [MICO_GPIO_2]                         = {GPIOB, 24},
    [MICO_GPIO_3]                         = {GPIOC, 14}, //swdio

    [MICO_GPIO_7]                         = {GPIOB, 20},
    [MICO_GPIO_8]                         = {GPIOB, 8},
    [MICO_GPIO_9]                         = {GPIOB, 7}, // debug uart tx
    [MICO_GPIO_10]                        = {GPIOB, 6}, // debug uart rx
    [MICO_GPIO_11]                        = {GPIOA, 25}, // easylink
    [MICO_GPIO_12]                        = {GPIOA, 23}, // USB DM
    [MICO_GPIO_13]                        = {GPIOA, 22}, // USB DP
    [MICO_GPIO_16]                        = {GPIOB, 26}, // BOOT
    [MICO_GPIO_17]                        = {GPIOA, 10}, // RESET
    [MICO_GPIO_20]                        = {GPIOB, 31}, // user uart rts
    [MICO_GPIO_21]                        = {GPIOC, 0},  // user uart cts
    [MICO_GPIO_22]                        = {GPIOB, 28}, // user uart tx
    [MICO_GPIO_23]                        = {GPIOB, 29}, // user uart rx
    [MICO_GPIO_26]                        = {GPIOC, 11},
    [MICO_GPIO_27]                        = {GPIOC, 12},
    [MICO_GPIO_28]                        = {GPIOC, 13}, // SWCLK
    [MICO_GPIO_30]                        = {GPIOB, 25}, // status
};

const platform_adc_t *platform_adc_peripherals = NULL;

const platform_pwm_t *platform_pwm_peripherals = NULL;

const platform_spi_t *platform_spi_peripherals = NULL;

platform_spi_driver_t *platform_spi_drivers = NULL;

const platform_spi_slave_driver_t *platform_spi_slave_drivers = NULL;

const platform_uart_t platform_uart_peripherals[] =
{
  [MICO_UART_DEBUG] =
  {
    .uart                            = FUART,
    .pin_tx                          = &platform_gpio_pins[STDIO_UART_TX],
    .pin_rx                          = &platform_gpio_pins[STDIO_UART_RX],
    .pin_cts                         = NULL,
    .pin_rts                         = NULL,
  },
  [MICO_UART_DATA] =
  {
    .uart                            = BUART,
    .pin_tx                          = &platform_gpio_pins[APP_UART_TX],
    .pin_rx                          = &platform_gpio_pins[APP_UART_RX],
    .pin_cts                         = NULL,
    .pin_rts                         = NULL,
  },
};

platform_uart_driver_t platform_uart_drivers[MICO_UART_MAX];


const platform_i2c_t *platform_i2c_peripherals = NULL;


/* Wi-Fi control pins. Used by platform/MCU/wlan_platform_common.c */
const platform_gpio_t wifi_control_pins[] =
{
  [WIFI_PIN_POWER        ]  = { GPIOB, 5 },
};

/* Wi-Fi SDIO bus pins. Used by platform/MCU/STM32F2xx/EMW1062_driver/wlan_SDIO.c */
const platform_gpio_t wifi_sdio_pins[] =
{
  [WIFI_PIN_SDIO_IRQ    ] = { GPIOA, 24 },
  [WIFI_PIN_SDIO_CLK    ] = { GPIOA, 20 },
  [WIFI_PIN_SDIO_CMD    ] = { GPIOA, 21 },
  [WIFI_PIN_SDIO_D0     ] = { GPIOB, 19 },
};

/* Flash memory devices */
const platform_flash_t platform_flash_peripherals[] =
{
  [MICO_FLASH_SPI] =
  {
    .flash_type                   = FLASH_TYPE_SPI,
    .flash_start_addr             = 0x000000,
    .flash_length                 = 0x200000,
    .flash_protect_opt            = FLASH_HALF_PROTECT,
  }
};

platform_flash_driver_t platform_flash_drivers[MICO_FLASH_MAX];

/* Logic partition on flash devices */
const mico_logic_partition_t mico_partitions[] =
{
  [MICO_PARTITION_BOOTLOADER] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "Bootloader",
    .partition_start_addr      = 0x0,
    .partition_length          = 0xA000,    //40k bytes + 4k bytes empty space
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
  },
  [MICO_PARTITION_APPLICATION] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "Application",
    .partition_start_addr      = 0xB000,
    .partition_length          = 0xC0000,   //768k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
  },
  [MICO_PARTITION_ATE] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "ATE",
    .partition_start_addr      = 0xCB000,
    .partition_length          = 0x50000,  //320k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
  },
  [MICO_PARTITION_RF_FIRMWARE] =
  {
    .partition_owner           = MICO_FLASH_NONE,
    .partition_description     = "RF Firmware",
    .partition_start_addr      = 0x0,
    .partition_length          = 0x0, 
    .partition_options         = PAR_OPT_READ_DIS | PAR_OPT_WRITE_DIS,
  },
  [MICO_PARTITION_OTA_TEMP] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "OTAStorage",
    .partition_start_addr      = 0x11B000,
    .partition_length          = 0xC0000, //768k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
  },
  [MICO_PARTITION_PARAMETER_1] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "PARAMETER1",
    .partition_start_addr      = 0x1DB000,
    .partition_length          = 0x1000, // 4k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
  },
  [MICO_PARTITION_PARAMETER_2] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "PARAMETER2",
    .partition_start_addr      = 0x1DC000,
    .partition_length          = 0x1000, //4k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
  }
};



/******************************************************
*           Interrupt Handler Definitions
******************************************************/

MICO_RTOS_DEFINE_ISR( FuartInterrupt )
{
  platform_uart_irq( &platform_uart_drivers[MICO_UART_DEBUG] );
}

MICO_RTOS_DEFINE_ISR( BuartInterrupt )
{
  platform_uart_irq( &platform_uart_drivers[MICO_UART_DATA] );
}

/******************************************************
*               Function Definitions
******************************************************/

static void _button_EL_irq_handler( void* arg )
{
  (void)(arg);
  int interval = -1;
  
  if ( MicoGpioInputGet( (mico_gpio_t)EasyLink_BUTTON ) == 0 ) {
    _default_start_time = mico_get_time()+1;
    mico_start_timer(&_button_EL_timer);
    MicoGpioEnableIRQ( (mico_gpio_t)EasyLink_BUTTON, IRQ_TRIGGER_RISING_EDGE, _button_EL_irq_handler, NULL );
  } else {
    interval = mico_get_time() + 1 - _default_start_time;
    if ( (_default_start_time != 0) && interval > 50 && interval < RestoreDefault_TimeOut){
      /* EasyLink button clicked once */
      PlatformEasyLinkButtonClickedCallback();
      MicoGpioEnableIRQ( (mico_gpio_t)EasyLink_BUTTON, IRQ_TRIGGER_FALLING_EDGE, _button_EL_irq_handler, NULL );
   }
   mico_stop_timer(&_button_EL_timer);
   _default_start_time = 0;
  }
}

static void _button_EL_Timeout_handler( void* arg )
{
  (void)(arg);
  _default_start_time = 0;
  MicoGpioEnableIRQ( (mico_gpio_t)EasyLink_BUTTON, IRQ_TRIGGER_FALLING_EDGE, _button_EL_irq_handler, NULL );
  if( MicoGpioInputGet( (mico_gpio_t)EasyLink_BUTTON ) == 0){
    PlatformEasyLinkButtonLongPressedCallback();
  }
  mico_stop_timer(&_button_EL_timer);
}

void init_platform( void )
{
  MicoGpioInitialize( (mico_gpio_t)MICO_SYS_LED, OUTPUT_PUSH_PULL );
  MicoSysLed(false);
  MicoGpioInitialize( (mico_gpio_t)MICO_RF_LED, OUTPUT_PUSH_PULL );
  MicoRfLed(false);
  MicoGpioInitialize( BOOT_SEL, INPUT_PULL_UP );
  MicoGpioInitialize( MFG_SEL, INPUT_PULL_UP );
  //  Initialise EasyLink buttons
  MicoGpioInitialize( (mico_gpio_t)EasyLink_BUTTON, INPUT_PULL_UP );
  mico_init_timer(&_button_EL_timer, RestoreDefault_TimeOut, _button_EL_Timeout_handler, NULL);
  MicoGpioEnableIRQ( (mico_gpio_t)EasyLink_BUTTON, IRQ_TRIGGER_FALLING_EDGE, _button_EL_irq_handler, NULL );

}

#ifdef BOOTLOADER
#include "host_stor.h"
#include "fat_file.h" 
#include "host_hcd.h"
#include "dir.h"

static bool HardwareInit(DEV_ID DevId);
static FOLDER	 RootFolder;
static void FileBrowse(FS_CONTEXT* FsContext);
static bool UpgradeFileFound = false;

static const mico_uart_config_t fuart_config =
{
  .baud_rate    = STDIO_UART_BAUDRATE,
  .data_width   = DATA_WIDTH_8BIT,
  .parity       = NO_PARITY,
  .stop_bits    = STOP_BITS_1,
  .flow_control = FLOW_CONTROL_DISABLED,
  .flags        = 0,
};
static volatile uint8_t rx_data[100];
static volatile ring_buffer_t rx_buffer;


static void debug_uart_send(char*str)
{
    MicoUartSend(MICO_UART_DEBUG, str, strlen(str));
}

void init_platform_bootloader( void )
{
  uint32_t BootNvmInfo;
  OSStatus err;
  char str[8];
  
  MicoGpioInitialize( BOOT_SEL, INPUT_PULL_UP );
  MicoGpioInitialize( MFG_SEL, INPUT_PULL_UP );
  MicoGpioInitialize( EasyLink_BUTTON, INPUT_PULL_UP );

  /* Check USB-HOST is inserted */
  err = MicoGpioInitialize( (mico_gpio_t)USB_DETECT, INPUT_PULL_DOWN );
  require_noerr(err, exit);
  mico_thread_msleep_no_os(2);

  MicoUartInitialize(MICO_UART_DEBUG, &fuart_config,  (ring_buffer_t*)&rx_buffer);
  if (MicoGpioInputGet( (mico_gpio_t)USB_DETECT ) != true) {
    debug_uart_send("**ASSERT** USB device is not inserted\r\n" );
    goto exit;
  }

  ClkModuleEn( USB_CLK_EN );
  //platform_log("USB device inserted");
  if( HardwareInit(DEV_ID_USB) ){
    FolderOpenByNum(&RootFolder, NULL, 1);
    FileBrowse(RootFolder.FsContext);
  }

  /* Check last firmware update is success or not. */
  NvmRead(UPGRADE_NVM_ADDR, (uint8_t*)&BootNvmInfo, 4);

  if(false == UpgradeFileFound)
  {
    if(BootNvmInfo == UPGRADE_SUCC_MAGIC)
    {
      /*
       * boot up check for the last time
       */
      debug_uart_send("[UPGRADE]:upgrade successful completely\r\n");
    }
    else if(BootNvmInfo == (uint32_t)UPGRADE_ERRNO_NOERR)
    {
      debug_uart_send("[UPGRADE]:no upgrade, boot normallly\r\n");
    }
    else if(BootNvmInfo == (uint32_t)UPGRADE_ERRNO_CODBUFDAT)
    {
      debug_uart_send("[UPGRADE]:upgrade successful partly, data fail\r\n");
    }
    else
    {
      sprintf(str, "%d", (int32_t)BootNvmInfo);
      debug_uart_send("[UPGRADE]:upgrade error, errno = ");
      debug_uart_send(str);
      debug_uart_send("\r\n");
    }
  }
  else
  {
    if(BootNvmInfo == (uint32_t)UPGRADE_ERRNO_NOERR)
    {
      debug_uart_send("[UPGRADE]:found upgrade ball, prepare to boot upgrade\r\n");
      BootNvmInfo = UPGRADE_REQT_MAGIC;
      NvmWrite(UPGRADE_NVM_ADDR, (uint8_t*)&BootNvmInfo, 4);
            //if you want PORRESET to reset GPIO only,uncomment it
            //GpioPorSysReset(GPIO_RSTSRC_PORREST);
      NVIC_SystemReset();
      while(1);;;
    }
    else if(BootNvmInfo == UPGRADE_SUCC_MAGIC)
    {
      BootNvmInfo = (uint32_t)UPGRADE_ERRNO_NOERR;
      NvmWrite(UPGRADE_NVM_ADDR, (uint8_t*)&BootNvmInfo, 4);
      debug_uart_send("[UPGRADE]:found upgrade ball file for the last time, re-plugin/out, if you want to upgrade again\r\n");
    }
    else
    {
      sprintf(str, "%d", (int32_t)BootNvmInfo);
      debug_uart_send("[UPGRADE]:upgrade error, errno = ");
      debug_uart_send(str);
      debug_uart_send("\r\n");
      if( BootNvmInfo == (uint32_t)UPGRADE_ERRNO_EBADF ) {
        debug_uart_send("[UPGRADE]:Same file, no need to update\r\n");
        goto exit;
      }
      BootNvmInfo = (uint32_t)UPGRADE_ERRNO_NOERR;
      NvmWrite(UPGRADE_NVM_ADDR, (uint8_t*)&BootNvmInfo, 4);
      BootNvmInfo = UPGRADE_REQT_MAGIC;
      NvmWrite(UPGRADE_NVM_ADDR, (uint8_t*)&BootNvmInfo, 4);
            //if you want PORRESET to reset GPIO only,uncomment it
            //GpioPorSysReset(GPIO_RSTSRC_PORREST);
      NVIC_SystemReset();
    }
  }
exit:
  return;
}

static bool IsCardLink(void)
{
  return false;
}
static bool IsUDiskLink(void)
{
	return UsbHost2IsLink();
}

bool CheckAllDiskLinkFlag(void)
{
    return TRUE;
}

static bool HardwareInit(DEV_ID DevId)
{
	switch(DevId)
	{
		case DEV_ID_SD:
			if(!IsCardLink())
			{
				return FALSE;
			}
			FSDeInit(DEV_ID_SD);
			if(SdCardInit())	
			{
				return FALSE;
			}
			if(!FSInit(DEV_ID_SD))
			{
				return FALSE;
			}
			return TRUE;
		case DEV_ID_USB:
			Usb2SetDetectMode(1, 0);
			UsbSetCurrentPort(UDISK_PORT_NUM);
			if(!IsUDiskLink())
			{
				return FALSE;
			}
			FSDeInit(DEV_ID_SD);
			FSDeInit(DEV_ID_USB);
			if(!HostStorInit())
			{
				return FALSE;
			}
			if(!FSInit(DEV_ID_USB))
			{
				return FALSE;
			}
			return TRUE;
		default:
			break;
	}
	return FALSE;
}
static void FileBrowse(FS_CONTEXT* FsContext)
{	
	uint8_t	EntryType;
	DirSetStartEntry(FsContext, FsContext->gFsInfo.RootStart, 0, TRUE);
	FsContext->gFolderDirStart = FsContext->gFsInfo.RootStart;
	while(1)
	{
		EntryType = DirGetNextEntry(FsContext);
		switch(EntryType)
		{
			case ENTRY_FILE:
//				platform_log("%-.11s  %d年%d月%d日 %d:%d:%d  %d 字节",
//					FsContext->gCurrentEntry->FileName,
//					1980+(FsContext->gCurrentEntry->CreateDate >> 9),
//					(FsContext->gCurrentEntry->CreateDate >> 5) & 0xF,
//					(FsContext->gCurrentEntry->CreateDate) & 0x1F,
//					FsContext->gCurrentEntry->CreateTime >> 11,
//					(FsContext->gCurrentEntry->CreateTime >> 5) & 0x3F,
//					((FsContext->gCurrentEntry->CreateTime << 1) & 0x3F) + (FsContext->gCurrentEntry->CrtTimeTenth / 100),
//					FsContext->gCurrentEntry->Size);
      	if((FsContext->gCurrentEntry->ExtName[0] == 'M') && 
           (FsContext->gCurrentEntry->ExtName[1] == 'V') && 
           (FsContext->gCurrentEntry->ExtName[2] == 'A')){
            UpgradeFileFound = true;
            return;
          }
				break;
			case ENTRY_FOLDER:
				break;
			case ENTRY_END:
        return;
			default:
				break;
		}
	}
}

#endif

void MicoSysLed(bool onoff)
{
    if (onoff) {
        MicoGpioOutputLow( (mico_gpio_t)MICO_SYS_LED );
    } else {
        MicoGpioOutputHigh( (mico_gpio_t)MICO_SYS_LED );
    }
}

void MicoRfLed(bool onoff)
{
    if (onoff) {
        MicoGpioOutputLow( (mico_gpio_t)MICO_RF_LED );
    } else {
        MicoGpioOutputHigh( (mico_gpio_t)MICO_RF_LED );
    }
}

bool MicoShouldEnterMFGMode(void)
{
  if(MicoGpioInputGet((mico_gpio_t)BOOT_SEL)==false && MicoGpioInputGet((mico_gpio_t)MFG_SEL)==false)
    return true;
  else
    return false;
}

bool MicoShouldEnterBootloader(void)
{
  if(MicoGpioInputGet((mico_gpio_t)BOOT_SEL)==false && MicoGpioInputGet((mico_gpio_t)MFG_SEL)==true)
    return true;
  else {
    return false;
  }
}

bool MicoShouldEnterATEMode(void)
{
  if(MicoGpioInputGet((mico_gpio_t)BOOT_SEL)==false && MicoGpioInputGet((mico_gpio_t)EasyLink_BUTTON)==false)
    return true;
  else
    return false;
}

