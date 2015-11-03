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
#include "stm32f4xx_platform.h"
#include "platform_common_config.h"
#include "PlatformLogging.h"

/******************************************************
*                      Macros
******************************************************/

#ifdef __GNUC__
#define WEAK __attribute__ ((weak))
#elif defined ( __IAR_SYSTEMS_ICC__ )
#define WEAK __weak
#endif /* ifdef __GNUC__ */

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
#ifdef SDIO_1_BIT
  /* Common GPIOs for internal use */
  [WL_GPIO1]                          = {GPIOA,  8,  RCC_AHB1Periph_GPIOA},
#else
  [WL_GPIO1]                          = {GPIOB,  6,  RCC_AHB1Periph_GPIOB},
#endif
  [MICO_SYS_LED]                      = {GPIOB,  12, RCC_AHB1Periph_GPIOB}, 
  [MICO_RF_LED]                       = {GPIOB,  13, RCC_AHB1Periph_GPIOB},
  [BOOT_SEL]                          = {GPIOB,  2,  RCC_AHB1Periph_GPIOB}, 
  [MFG_SEL]                           = {GPIOA,  4,  RCC_AHB1Periph_GPIOA}, 
  [EasyLink_BUTTON]                   = {GPIOB,  14, RCC_AHB1Periph_GPIOA}, 
  [STDIO_UART_RX]                     = {GPIOB,  3,  RCC_AHB1Periph_GPIOB},  
  [STDIO_UART_TX]                     = {GPIOA,  15,  RCC_AHB1Periph_GPIOA},  

  /* GPIOs for external use */
  [MICO_GPIO_2]  = {GPIOA, 11,  RCC_AHB1Periph_GPIOA},
  [MICO_GPIO_4]  = {GPIOA,  7,  RCC_AHB1Periph_GPIOA},
  [MICO_GPIO_8]  = {GPIOA,  2,  RCC_AHB1Periph_GPIOA},
  [MICO_GPIO_9]  = {GPIOA,  1,  RCC_AHB1Periph_GPIOA},
  [MICO_GPIO_12] = {GPIOA,  3,  RCC_AHB1Periph_GPIOA},
  [MICO_GPIO_16] = {GPIOC, 13,  RCC_AHB1Periph_GPIOC},
  [MICO_GPIO_17] = {GPIOB, 10,  RCC_AHB1Periph_GPIOB},
  [MICO_GPIO_18] = {GPIOB,  9,  RCC_AHB1Periph_GPIOB},
  //[MICO_GPIO_19] = {GPIOB, 12,  RCC_AHB1Periph_GPIOB},
  [MICO_GPIO_27] = {GPIOA, 12,  RCC_AHB1Periph_GPIOA},  
  [MICO_GPIO_29] = {GPIOA, 10,  RCC_AHB1Periph_GPIOA},
  [MICO_GPIO_30] = {GPIOB,  6,  RCC_AHB1Periph_GPIOB},
  [MICO_GPIO_31] = {GPIOB,  8,  RCC_AHB1Periph_GPIOB},
  //[MICO_GPIO_33] = {GPIOB, 13,  RCC_AHB1Periph_GPIOB},
  [MICO_GPIO_34] = {GPIOA,  5,  RCC_AHB1Periph_GPIOA},
  [MICO_GPIO_35] = {GPIOA, 10,  RCC_AHB1Periph_GPIOA},
  [MICO_GPIO_36] = {GPIOB,  1,  RCC_AHB1Periph_GPIOB},
  [MICO_GPIO_37] = {GPIOB,  0,  RCC_AHB1Periph_GPIOB},
  [MICO_GPIO_38] = {GPIOA,  4,  RCC_AHB1Periph_GPIOA},
};

/*
* Possible compile time inputs:
* - Set which ADC peripheral to use for each ADC. All on one ADC allows sequential conversion on all inputs. All on separate ADCs allows concurrent conversion.
*/
/* TODO : These need fixing */
const platform_adc_mapping_t adc_mapping[] =
{
  [MICO_ADC_1] = NULL,
 // [MICO_ADC_1] = {ADC1, ADC_Channel_1, RCC_APB2Periph_ADC1, 1, (platform_pin_mapping_t*)&gpio_mapping[MICO_GPIO_2]},
 // [MICO_ADC_2] = {ADC1, ADC_Channel_2, RCC_APB2Periph_ADC1, 1, (platform_pin_mapping_t*)&gpio_mapping[MICO_GPIO_4]},
 // [MICO_ADC_3] = {ADC1, ADC_Channel_3, RCC_APB2Periph_ADC1, 1, (platform_pin_mapping_t*)&gpio_mapping[MICO_GPIO_5]},
};


/* PWM mappings */
const platform_pwm_mapping_t pwm_mappings[] =
{
#if ( MICO_WLAN_POWERSAVE_CLOCK_SOURCE == MICO_WLAN_POWERSAVE_CLOCK_IS_PWM )
  /* Extended PWM for internal use */
  [MICO_PWM_WLAN_POWERSAVE_CLOCK] = {TIM1, 1, RCC_APB2Periph_TIM1, GPIO_AF_TIM1, (platform_pin_mapping_t*)&gpio_mapping[MICO_GPIO_WLAN_POWERSAVE_CLOCK] }, /* or TIM2/Ch2                       */
#endif
  //[MICO_PWM_1] = NULL,
  [MICO_PWM_G]  = {TIM2, 2, RCC_APB1Periph_TIM2, GPIO_AF_TIM2, (platform_pin_mapping_t*)&gpio_mapping[MICO_GPIO_9]},    /* or TIM10/Ch1                       */
  [MICO_PWM_B]  = {TIM2, 1, RCC_APB1Periph_TIM2, GPIO_AF_TIM2, (platform_pin_mapping_t*)&gpio_mapping[MICO_GPIO_34]}, /* or TIM1/Ch2N                       */
  [MICO_PWM_R]  = {TIM3, 2, RCC_APB1Periph_TIM3, GPIO_AF_TIM3, (platform_pin_mapping_t*)&gpio_mapping[MICO_GPIO_4]},    
  /* TODO: fill in the other options here ... */
};

const platform_spi_mapping_t spi_mapping[] =
{
  [MICO_SPI_1]  = NULL,
  // [MICO_SPI_1]  =
  // {
  //   .spi_regs              = SPI1,
  //   .gpio_af               = GPIO_AF_SPI1,
  //   .peripheral_clock_reg  = RCC_APB2Periph_SPI1,
  //   .peripheral_clock_func = RCC_APB2PeriphClockCmd,
  //   .pin_mosi              = &gpio_mapping[MICO_GPIO_8],
  //   .pin_miso              = &gpio_mapping[MICO_GPIO_7],
  //   .pin_clock             = &gpio_mapping[MICO_GPIO_6],
  //   .tx_dma_stream         = DMA2_Stream5,
  //   .rx_dma_stream         = DMA2_Stream0,
  //   .tx_dma_channel        = DMA_Channel_3,
  //   .rx_dma_channel        = DMA_Channel_3,
  //   .tx_dma_stream_number  = 5,
  //   .rx_dma_stream_number  = 0
  // }
};

const platform_uart_mapping_t uart_mapping[] =
{
  [MICO_UART_1] =
  {
    .usart                        = USART1,
    .gpio_af                      = GPIO_AF_USART1,
    .pin_tx                       = &gpio_mapping[STDIO_UART_TX],
    .pin_rx                       = &gpio_mapping[STDIO_UART_RX],
    .pin_cts                      = NULL,
    .pin_rts                      = NULL,
    .usart_peripheral_clock       = RCC_APB2Periph_USART1,
    .usart_peripheral_clock_func  = RCC_APB2PeriphClockCmd,
    .usart_irq                    = USART1_IRQn,
    .tx_dma                       = DMA2,
    .tx_dma_stream                = DMA2_Stream7,
    .tx_dma_stream_number         = 7,
    .tx_dma_channel               = DMA_Channel_4,
    .tx_dma_peripheral_clock      = RCC_AHB1Periph_DMA2,
    .tx_dma_peripheral_clock_func = RCC_AHB1PeriphClockCmd,
    .tx_dma_irq                   = DMA2_Stream7_IRQn,
    .rx_dma                       = DMA2,
    .rx_dma_stream                = DMA2_Stream2,
    .rx_dma_stream_number         = 2,
    .rx_dma_channel               = DMA_Channel_4,
    .rx_dma_peripheral_clock      = RCC_AHB1Periph_DMA2,
    .rx_dma_peripheral_clock_func = RCC_AHB1PeriphClockCmd,
    .rx_dma_irq                   = DMA2_Stream2_IRQn,
  },
  [MICO_UART_2] =
  {
    .usart                        = USART2,
    .gpio_af                      = GPIO_AF_USART6,
    .pin_tx                       = &gpio_mapping[MICO_GPIO_1],
    .pin_rx                       = &gpio_mapping[MICO_GPIO_0],
    .pin_cts                      = NULL,
    .pin_rts                      = NULL,
    .usart_peripheral_clock       = RCC_APB2Periph_USART6,
    .usart_peripheral_clock_func  = RCC_APB2PeriphClockCmd,
    .usart_irq                    = USART6_IRQn,
    .tx_dma                       = DMA2,
    .tx_dma_stream                = DMA2_Stream6,
    .tx_dma_channel               = DMA_Channel_5,
    .tx_dma_peripheral_clock      = RCC_AHB1Periph_DMA2,
    .tx_dma_peripheral_clock_func = RCC_AHB1PeriphClockCmd,
    .tx_dma_irq                   = DMA2_Stream6_IRQn,
    .rx_dma                       = DMA2,
    .rx_dma_stream                = DMA2_Stream1,
    .rx_dma_channel               = DMA_Channel_5,
    .rx_dma_peripheral_clock      = RCC_AHB1Periph_DMA2,
    .rx_dma_peripheral_clock_func = RCC_AHB1PeriphClockCmd,
    .rx_dma_irq                   = DMA2_Stream1_IRQn,
  },
};

const platform_i2c_mapping_t i2c_mapping[] =
{
  [MICO_I2C_1] = NULL,
  // [MICO_I2C_1] =
  // {
  //   .i2c = I2C1,
  //   .pin_scl                 = &gpio_mapping[MICO_GPIO_1],
  //   .pin_sda                 = &gpio_mapping[MICO_GPIO_2],
  //   .peripheral_clock_reg    = RCC_APB1Periph_I2C1,
  //   .tx_dma                  = DMA1,
  //   .tx_dma_peripheral_clock = RCC_AHB1Periph_DMA1,
  //   .tx_dma_stream           = DMA1_Stream7,
  //   .rx_dma_stream           = DMA1_Stream5,
  //   .tx_dma_stream_id        = 7,
  //   .rx_dma_stream_id        = 5,
  //   .tx_dma_channel          = DMA_Channel_1,
  //   .rx_dma_channel          = DMA_Channel_1,
  //   .gpio_af                 = GPIO_AF_I2C1
  // },
};

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
  } else {
    interval = mico_get_time() + 1 - _default_start_time;
    if ( (_default_start_time != 0) && interval > 50 && interval < RestoreDefault_TimeOut){
      /* EasyLink button clicked once */
      PlatformEasyLinkButtonClickedCallback();
    }
    mico_stop_timer(&_button_EL_timer);
    _default_start_time = 0;
  }
}

static void _button_STANDBY_irq_handler( void* arg )
{
  (void)(arg);
  PlatformStandbyButtonClickedCallback();
}

static void _button_EL_Timeout_handler( void* arg )
{
  (void)(arg);
  _default_start_time = 0;
  PlatformEasyLinkButtonLongPressedCallback();
}

bool watchdog_check_last_reset( void )
{
  if ( RCC->CSR & RCC_CSR_WDGRSTF )
  {
    /* Clear the flag and return */
    RCC->CSR |= RCC_CSR_RMVF;
    return true;
  }
  
  return false;
}

OSStatus mico_platform_init( void )
{
  platform_log( "Platform initialised" );
  
  if ( true == watchdog_check_last_reset() )
  {
    platform_log( "WARNING: Watchdog reset occured previously. Please see watchdog.c for debugging instructions." );
  }
  
  return kNoErr;
}

void init_platform( void )
{
   MicoGpioInitialize( (mico_gpio_t)MICO_SYS_LED, OUTPUT_PUSH_PULL );
   MicoGpioOutputLow( (mico_gpio_t)MICO_SYS_LED );
   MicoGpioInitialize( (mico_gpio_t)MICO_RF_LED, OUTPUT_OPEN_DRAIN_NO_PULL );
   MicoGpioOutputHigh( (mico_gpio_t)MICO_RF_LED );
  
   //  Initialise EasyLink buttons
   MicoGpioInitialize( (mico_gpio_t)EasyLink_BUTTON, INPUT_PULL_UP );
   mico_init_timer(&_button_EL_timer, RestoreDefault_TimeOut, _button_EL_Timeout_handler, NULL);
   MicoGpioEnableIRQ( (mico_gpio_t)EasyLink_BUTTON, IRQ_TRIGGER_BOTH_EDGES, _button_EL_irq_handler, NULL );
  
   //  Initialise Standby/wakeup switcher
   MicoGpioInitialize( (mico_gpio_t)Standby_SEL, INPUT_PULL_UP );
   MicoGpioEnableIRQ( (mico_gpio_t)Standby_SEL , IRQ_TRIGGER_FALLING_EDGE, _button_STANDBY_irq_handler, NULL);

   MicoFlashInitialize( MICO_SPI_FLASH );
}

void init_platform_bootloader( void )
{
  MicoGpioInitialize( (mico_gpio_t)MICO_SYS_LED, OUTPUT_PUSH_PULL );
  MicoGpioOutputLow( (mico_gpio_t)MICO_SYS_LED );
  MicoGpioInitialize( (mico_gpio_t)MICO_RF_LED, OUTPUT_OPEN_DRAIN_NO_PULL );
  MicoGpioOutputHigh( (mico_gpio_t)MICO_RF_LED );
  
  MicoGpioInitialize((mico_gpio_t)BOOT_SEL, INPUT_PULL_UP);
  MicoGpioInitialize((mico_gpio_t)MFG_SEL, INPUT_HIGH_IMPEDANCE);
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

void host_platform_power_wifi_init(void)
{
    MicoGpioInitialize((mico_gpio_t)WL_REG, OUTPUT_PUSH_PULL);
    MicoGpioOutputLow( (mico_gpio_t)WL_REG );
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
  return false;
  
  if(MicoGpioInputGet((mico_gpio_t)BOOT_SEL)==false && MicoGpioInputGet((mico_gpio_t)MFG_SEL)==false)
    return true;
  else
    return false;
}

bool MicoShouldEnterBootloader(void)
{
  if(MicoGpioInputGet((mico_gpio_t)BOOT_SEL)==false /*&& MicoGpioInputGet((mico_gpio_t)MFG_SEL)==true*/)
    return true;
  else
    return false;
}

