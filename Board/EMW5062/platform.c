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

#include "MICOPlatform.h"
#include "platform.h"
#include "MicoDriverMapping.h"
#include "platform_common_config.h"
#include "PlatformLogging.h"
#include "gpio.h"

/******************************************************
*                      Macros
******************************************************/

/******************************************************
*                    Constants
******************************************************/

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
extern WEAK void bootloader_start(void);

/******************************************************
*               Variables Definitions
******************************************************/

/* This table maps STM32 pins to GPIO definitions on the schematic
* A full pin definition is provided in <WICED-SDK>/include/platforms/BCM943362WCD4/platform.h
*/

static uint32_t _default_start_time = 0;
static mico_timer_t _button_EL_timer;

const platform_pin_mapping_t gpio_mapping[] =
{
//  /* Common GPIOs for internal use */
//  [MICO_GPIO_WLAN_POWERSAVE_CLOCK]    = {WL_32K_OUT_BANK, WL_32K_OUT_PIN, WL_32K_OUT_BANK_CLK},
//  [WL_GPIO0]                          = {GPIOB, 12,  RCC_AHB1Periph_GPIOB},
  [WL_GPIO1]                            = {GPIOA,  10},
//  [WL_REG]                            = {GPIOC,  1,  RCC_AHB1Periph_GPIOC},
  [WL_RESET]                            = {GPIOB,  20},
  [MICO_SYS_LED]                        = {GPIOB,  2 }, 
  [MICO_RF_LED]                         = {GPIOB,  3 }, 
//  [BOOT_SEL]                          = {GPIOB,  1,  RCC_AHB1Periph_GPIOB}, 
//  [MFG_SEL]                           = {GPIOB,  9,  RCC_AHB1Periph_GPIOB}, 
  [EasyLink_BUTTON]                     = {GPIOA,  5}, 
  [STDIO_UART_RX]                       = {GPIOB,  29},
  [STDIO_UART_TX]                       = {GPIOB,  28},
  [SDIO_INT]                            = {GPIOA,  24},

//  /* GPIOs for external use */
  [APP_UART_RX]                         = {GPIOB, 6},
  [APP_UART_TX]                         = {GPIOB, 7},  
//  [MICO_GPIO_4]  = {GPIOC,  7,  RCC_AHB1Periph_GPIOC},
//  [MICO_GPIO_5]  = {GPIOA,  4,  RCC_AHB1Periph_GPIOA},
//  [MICO_GPIO_6]  = {GPIOA,  4,  RCC_AHB1Periph_GPIOA},
//  [MICO_GPIO_7]  = {GPIOB,  3,  RCC_AHB1Periph_GPIOB},
//  [MICO_GPIO_8]  = {GPIOB , 4,  RCC_AHB1Periph_GPIOB},
//  [MICO_GPIO_9]  = {GPIOB,  5,  RCC_AHB1Periph_GPIOB},
//  [MICO_GPIO_10] = {GPIOB,  8,  RCC_AHB1Periph_GPIOB},
//  [MICO_GPIO_12] = {GPIOC,  2,  RCC_AHB1Periph_GPIOC},
//  [MICO_GPIO_13] = {GPIOB, 14,  RCC_AHB1Periph_GPIOB},
//  [MICO_GPIO_14] = {GPIOC,  6,  RCC_AHB1Periph_GPIOC},
//  [MICO_GPIO_18] = {GPIOA, 15,  RCC_AHB1Periph_GPIOA},
//  [MICO_GPIO_19] = {GPIOB, 11,  RCC_AHB1Periph_GPIOB},
//  [MICO_GPIO_20] = {GPIOA, 12,  RCC_AHB1Periph_GPIOA},
//  [MICO_GPIO_21] = {GPIOA, 11,  RCC_AHB1Periph_GPIOA},
//  [MICO_GPIO_22] = {GPIOA,  9,  RCC_AHB1Periph_GPIOA},
//  [MICO_GPIO_23] = {GPIOA, 10,  RCC_AHB1Periph_GPIOA},
//  [MICO_GPIO_29] = {GPIOA,  0,  RCC_AHB1Periph_GPIOA},  
};

/*
* Possible compile time inputs:
* - Set which ADC peripheral to use for each ADC. All on one ADC allows sequential conversion on all inputs. All on separate ADCs allows concurrent conversion.
*/
/* TODO : These need fixing */
const platform_adc_mapping_t adc_mapping[] =
{
  [MICO_ADC_1] = {1},
  [MICO_ADC_2] = {1},
  [MICO_ADC_3] = {1},
};


/* PWM mappings */
const platform_pwm_mapping_t pwm_mappings[] =
{
#if ( MICO_WLAN_POWERSAVE_CLOCK_SOURCE == MICO_WLAN_POWERSAVE_CLOCK_IS_PWM )
  /* Extended PWM for internal use */
  [MICO_PWM_WLAN_POWERSAVE_CLOCK] = {TIM1, 4, RCC_APB2Periph_TIM1, GPIO_AF_TIM1, (platform_pin_mapping_t*)&gpio_mapping[MICO_GPIO_WLAN_POWERSAVE_CLOCK] }, /* or TIM2/Ch2                       */
#endif
  
  [MICO_PWM_1]  = {1},    /* or TIM10/Ch1                       */
  [MICO_PWM_2]  = {1}, /* or TIM1/Ch2N                       */
  [MICO_PWM_3]  = {1},    
  /* TODO: fill in the other options here ... */
};

const platform_spi_mapping_t spi_mapping[] =
{
  [MICO_SPI_1]  =
  {
    1
  }
};

const platform_uart_mapping_t uart_mapping[] =
{
[MICO_UART_1] =
  {
    .uart                            = FUART,
    .pin_tx                          = &gpio_mapping[APP_UART_TX],
    .pin_rx                          = &gpio_mapping[APP_UART_RX],
    .pin_cts                         = NULL,
    .pin_rts                         = NULL,
  },
  [MICO_UART_2] =
  {
    .uart                            = BUART,
    .pin_tx                          = &gpio_mapping[STDIO_UART_TX],
    .pin_rx                          = &gpio_mapping[STDIO_UART_RX],
    .pin_cts                         = NULL,
    .pin_rts                         = NULL,
  },
};

const platform_i2c_mapping_t i2c_mapping[] =
{
  [MICO_I2C_1] =
  {
    1,
  },
};

/******************************************************
*               Function Definitions
******************************************************/

static void _button_EL_irq_handler( void* arg )
{
  (void)(arg);
  int interval = -1;

  mico_start_timer(&_button_EL_timer);
  
  if ( MicoGpioInputGet( (mico_gpio_t)EasyLink_BUTTON ) == 0 ) {
    _default_start_time = mico_get_time()+1;
    mico_start_timer(&_button_EL_timer);
    MicoGpioEnableIRQ( (mico_gpio_t)EasyLink_BUTTON, IRQ_TRIGGER_RISING_EDGE, _button_EL_irq_handler, NULL );
  } else {
    interval = mico_get_time() + 1 - _default_start_time;
    if ( (_default_start_time != 0) && interval > 50 && interval < RestoreDefault_TimeOut){
      /* EasyLink button clicked once */
      PlatformEasyLinkButtonClickedCallback();
      //platform_log("PlatformEasyLinkButtonClickedCallback!");
      MicoGpioOutputLow( (mico_gpio_t)MICO_RF_LED );
      MicoGpioEnableIRQ( (mico_gpio_t)EasyLink_BUTTON, IRQ_TRIGGER_FALLING_EDGE, _button_EL_irq_handler, NULL );
   }
   mico_stop_timer(&_button_EL_timer);
   _default_start_time = 0;
  }
}

//static void _button_STANDBY_irq_handler( void* arg )
//{
//  (void)(arg);
//  PlatformStandbyButtonClickedCallback();
//}

static void _button_EL_Timeout_handler( void* arg )
{
  (void)(arg);
  _default_start_time = 0;
  MicoGpioEnableIRQ( (mico_gpio_t)EasyLink_BUTTON, IRQ_TRIGGER_FALLING_EDGE, _button_EL_irq_handler, NULL );
  if( MicoGpioInputGet( (mico_gpio_t)EasyLink_BUTTON ) == 0){
    //platform_log("PlatformEasyLinkButtonLongPressedCallback!");
    PlatformEasyLinkButtonLongPressedCallback();
  }
  mico_stop_timer(&_button_EL_timer);
}

bool watchdog_check_last_reset( void )
{
//  if ( RCC->CSR & RCC_CSR_WDGRSTF )
//  {
//    /* Clear the flag and return */
//    RCC->CSR |= RCC_CSR_RMVF;
//    return true;
//  }
//  
  return false;
}

OSStatus mico_platform_init( void )
{
#ifdef DEBUG
  #if defined(__CC_ARM)
    platform_log("Build by Keil");
  #elif defined (__IAR_SYSTEMS_ICC__)
    platform_log("Build by IAR");
  #endif
#endif
  
 platform_log( "Mico platform initialised" );
 if ( true == watchdog_check_last_reset() )
 {
   platform_log( "WARNING: Watchdog reset occured previously. Please see watchdog.c for debugging instructions." );
 }
  
  return kNoErr;
}

void init_platform( void )
{

  MicoGpioInitialize( (mico_gpio_t)MICO_SYS_LED, OUTPUT_OPEN_DRAIN_NO_PULL );
  MicoSysLed(false);
  MicoGpioInitialize( (mico_gpio_t)MICO_RF_LED, OUTPUT_OPEN_DRAIN_NO_PULL );
  MicoRfLed(false);
  
  //  Initialise EasyLink buttons
  //MicoGpioInitialize( (mico_gpio_t)EasyLink_BUTTON, INPUT_PULL_UP );
  //mico_init_timer(&_button_EL_timer, RestoreDefault_TimeOut, _button_EL_Timeout_handler, NULL);
  //MicoGpioEnableIRQ( (mico_gpio_t)EasyLink_BUTTON, IRQ_TRIGGER_FALLING_EDGE, _button_EL_irq_handler, NULL );
//  
//  //  Initialise Standby/wakeup switcher
//  MicoGpioInitialize( Standby_SEL, INPUT_PULL_UP );
//  MicoGpioEnableIRQ( Standby_SEL , IRQ_TRIGGER_FALLING_EDGE, _button_STANDBY_irq_handler, NULL);

}

void init_platform_bootloader( void )
{
//  MicoGpioInitialize( (mico_gpio_t)MICO_SYS_LED, OUTPUT_PUSH_PULL );
//  MicoGpioOutputLow( (mico_gpio_t)MICO_SYS_LED );
//  MicoGpioInitialize( (mico_gpio_t)MICO_RF_LED, OUTPUT_OPEN_DRAIN_NO_PULL );
//  MicoGpioOutputHigh( (mico_gpio_t)MICO_RF_LED );
//  
//  MicoGpioInitialize((mico_gpio_t)BOOT_SEL, INPUT_PULL_UP);
//  MicoGpioInitialize((mico_gpio_t)MFG_SEL, INPUT_HIGH_IMPEDANCE);
}


void host_platform_reset_wifi( bool reset_asserted )
{
  if ( reset_asserted == true )
  {
    MicoGpioOutputLow( (mico_gpio_t)WL_RESET );  
  }
  else
  {
    MicoGpioOutputHigh( (mico_gpio_t)WL_RESET ); 
  }
}

void host_platform_power_wifi( bool power_enabled )
{
  if ( power_enabled == true )
  {
    MicoGpioOutputLow( (mico_gpio_t)WL_REG );  
  }
  else
  {
    MicoGpioOutputHigh( (mico_gpio_t)WL_REG ); 
  }
}

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
//  if(MicoGpioInputGet((mico_gpio_t)BOOT_SEL)==false && MicoGpioInputGet((mico_gpio_t)MFG_SEL)==false)
//    return true;
//  else
    return false;
}

bool MicoShouldEnterBootloader(void)
{
//  if(MicoGpioInputGet((mico_gpio_t)BOOT_SEL)==false && MicoGpioInputGet((mico_gpio_t)MFG_SEL)==true)
//    return true;
//  else
    return true;
}

