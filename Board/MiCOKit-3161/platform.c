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
#include "stm32f2xx_platform.h"
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
  /* Common GPIOs for internal use */
  [MICO_GPIO_WLAN_POWERSAVE_CLOCK]    = {WL_32K_OUT_BANK, WL_32K_OUT_PIN, WL_32K_OUT_BANK_CLK},
  [WL_GPIO0]                          = {GPIOB,  0,  RCC_AHB1Periph_GPIOB},
  [WL_GPIO1]                          = {GPIOB,  1,  RCC_AHB1Periph_GPIOB},
  [WL_REG]                            = {GPIOB,  2,  RCC_AHB1Periph_GPIOB},
  [WL_RESET]                          = {GPIOB,  5,  RCC_AHB1Periph_GPIOB},
  [MICO_SYS_LED]                      = {GPIOI,  9,  RCC_AHB1Periph_GPIOI}, 
  [MICO_RF_LED]                       = {GPIOF,  1,  RCC_AHB1Periph_GPIOF}, //MICO_GPIO_16
  [BOOT_SEL]                          = {GPIOE,  6,  RCC_AHB1Periph_GPIOE}, //MICO_GPIO_16
  [MFG_SEL]                           = {GPIOF,  1,  RCC_AHB1Periph_GPIOF}, //MICO_GPIO_30
  [EasyLink_BUTTON]                   = {GPIOH,  10,  RCC_AHB1Periph_GPIOH}, //MICO_GPIO_11

  /* GPIOs for external use */
  [MICO_GPIO_1]  = {GPIOB,  6,  RCC_AHB1Periph_GPIOB},
  [MICO_GPIO_2]  = {GPIOB,  7,  RCC_AHB1Periph_GPIOB},
  [MICO_GPIO_4]  = {GPIOC,  6,  RCC_AHB1Periph_GPIOC},
  [MICO_GPIO_5]  = {GPIOH,  9,  RCC_AHB1Periph_GPIOH},
  [MICO_GPIO_6]  = {GPIOI,  0,  RCC_AHB1Periph_GPIOI},
  [MICO_GPIO_7]  = {GPIOI,  1,  RCC_AHB1Periph_GPIOI},
  [MICO_GPIO_8]  = {GPIOI,  2,  RCC_AHB1Periph_GPIOI},
  [MICO_GPIO_9]  = {GPIOI,  3,  RCC_AHB1Periph_GPIOI},
  [MICO_GPIO_10] = {GPIOE,  5,  RCC_AHB1Periph_GPIOE},
  [MICO_GPIO_12] = {GPIOB, 14,  RCC_AHB1Periph_GPIOB},
  [MICO_GPIO_13] = {GPIOB, 15,  RCC_AHB1Periph_GPIOB},
  [MICO_GPIO_14] = {GPIOC,  6,  RCC_AHB1Periph_GPIOC},
  [MICO_GPIO_18] = {GPIOA, 10,  RCC_AHB1Periph_GPIOA},
  [MICO_GPIO_19] = {GPIOA,  9,  RCC_AHB1Periph_GPIOA},
  [MICO_GPIO_20] = {GPIOA,  1,  RCC_AHB1Periph_GPIOA},
  [MICO_GPIO_21] = {GPIOA,  0,  RCC_AHB1Periph_GPIOA},
  [MICO_GPIO_22] = {GPIOA,  2,  RCC_AHB1Periph_GPIOA},
  [MICO_GPIO_23] = {GPIOA,  3,  RCC_AHB1Periph_GPIOA},
  [MICO_GPIO_29] = {GPIOF,  0,  RCC_AHB1Periph_GPIOF},  
};

/*
* Possible compile time inputs:
* - Set which ADC peripheral to use for each ADC. All on one ADC allows sequential conversion on all inputs. All on separate ADCs allows concurrent conversion.
*/
/* TODO : These need fixing */
const platform_adc_mapping_t adc_mapping[] =
{
  [MICO_ADC_1] = NULL,
};


/* PWM mappings */
const platform_pwm_mapping_t pwm_mappings[] =
{
#if ( MICO_WLAN_POWERSAVE_CLOCK_SOURCE == MICO_WLAN_POWERSAVE_CLOCK_IS_PWM )
  /* Extended PWM for internal use */
  [MICO_PWM_WLAN_POWERSAVE_CLOCK] = {TIM1, 4, RCC_APB2Periph_TIM1, GPIO_AF_TIM1, (platform_pin_mapping_t*)&gpio_mapping[MICO_GPIO_WLAN_POWERSAVE_CLOCK] }, /* or TIM2/Ch2                       */
#endif
  
  [MICO_PWM_1]  = {TIM9, 1, RCC_APB2Periph_TIM9, GPIO_AF_TIM9, (platform_pin_mapping_t*)&gpio_mapping[MICO_GPIO_10]},                        
  [MICO_PWM_2]  = {TIM12, 2, RCC_APB1Periph_TIM12, GPIO_AF_TIM12, (platform_pin_mapping_t*)&gpio_mapping[MICO_GPIO_13]}, 
  [MICO_PWM_3]  = {TIM1, 2, RCC_APB2Periph_TIM1, GPIO_AF_TIM1, (platform_pin_mapping_t*)&gpio_mapping[MICO_GPIO_19]},    
  /* TODO: fill in the other options here ... */
};

const platform_spi_mapping_t spi_mapping[] =
{
  [MICO_SPI_1]  =
  {
    .spi_regs              = SPI2,
    .gpio_af               = GPIO_AF_SPI2,
    .peripheral_clock_reg  = RCC_APB1Periph_SPI2,
    .peripheral_clock_func = RCC_APB1PeriphClockCmd,
    .pin_mosi              = &gpio_mapping[MICO_GPIO_8],
    .pin_miso              = &gpio_mapping[MICO_GPIO_7],
    .pin_clock             = &gpio_mapping[MICO_GPIO_6],
    .tx_dma_stream         = DMA1_Stream4,
    .rx_dma_stream         = DMA1_Stream3,
    .tx_dma_channel        = DMA_Channel_0,
    .rx_dma_channel        = DMA_Channel_0,
    .tx_dma_stream_number  = 4,
    .rx_dma_stream_number  = 3
  }
};

const platform_uart_mapping_t uart_mapping[] =
{
  [MICO_UART_1] =
  {
    .usart                        = USART2,
    .gpio_af                      = GPIO_AF_USART2,
    .pin_tx                       = &gpio_mapping[MICO_GPIO_22],
    .pin_rx                       = &gpio_mapping[MICO_GPIO_23],
    .pin_cts                      = &gpio_mapping[MICO_GPIO_21],
    .pin_rts                      = &gpio_mapping[MICO_GPIO_20],
    .usart_peripheral_clock       = RCC_APB1Periph_USART2,
    .usart_peripheral_clock_func  = RCC_APB1PeriphClockCmd,
    .usart_irq                    = USART2_IRQn,
    .tx_dma                       = DMA1,
    .tx_dma_stream                = DMA1_Stream6,
    .tx_dma_stream_number         = 6,
    .tx_dma_channel               = DMA_Channel_4,
    .tx_dma_peripheral_clock      = RCC_AHB1Periph_DMA1,
    .tx_dma_peripheral_clock_func = RCC_AHB1PeriphClockCmd,
    .tx_dma_irq                   = DMA1_Stream6_IRQn,
    .rx_dma                       = DMA1,
    .rx_dma_stream                = DMA1_Stream5,
    .rx_dma_stream_number         = 5,
    .rx_dma_channel               = DMA_Channel_4,
    .rx_dma_peripheral_clock      = RCC_AHB1Periph_DMA1,
    .rx_dma_peripheral_clock_func = RCC_AHB1PeriphClockCmd,
    .rx_dma_irq                   = DMA1_Stream5_IRQn,
  },
  [MICO_UART_2] =
  {
    .usart                        = USART2,
    .gpio_af                      = GPIO_AF_USART2,
    .pin_tx                       = &gpio_mapping[MICO_GPIO_22],
    .pin_rx                       = &gpio_mapping[MICO_GPIO_23],
    .pin_cts                      = NULL,
    .pin_rts                      = NULL,
    .usart_peripheral_clock       = RCC_APB1Periph_USART2,
    .usart_peripheral_clock_func  = RCC_APB1PeriphClockCmd,
    .usart_irq                    = USART2_IRQn,
    .tx_dma                       = DMA1,
    .tx_dma_stream                = DMA1_Stream6,
    .tx_dma_stream_number         = 6,
    .tx_dma_channel               = DMA_Channel_4,
    .tx_dma_peripheral_clock      = RCC_AHB1Periph_DMA1,
    .tx_dma_peripheral_clock_func = RCC_AHB1PeriphClockCmd,
    .tx_dma_irq                   = DMA1_Stream6_IRQn,
    .rx_dma                       = DMA1,
    .rx_dma_stream                = DMA1_Stream5,
    .rx_dma_stream_number         = 5,
    .rx_dma_channel               = DMA_Channel_4,
    .rx_dma_peripheral_clock      = RCC_AHB1Periph_DMA1,
    .rx_dma_peripheral_clock_func = RCC_AHB1PeriphClockCmd,
    .rx_dma_irq                   = DMA1_Stream5_IRQn,
  },
};

const platform_i2c_mapping_t i2c_mapping[] =
{
  [MICO_I2C_1] =
  {
    .i2c = I2C1,
    .pin_scl                 = &gpio_mapping[MICO_GPIO_1],
    .pin_sda                 = &gpio_mapping[MICO_GPIO_2],
    .peripheral_clock_reg    = RCC_APB1Periph_I2C1,
    .tx_dma                  = DMA1,
    .tx_dma_peripheral_clock = RCC_AHB1Periph_DMA1,
    .tx_dma_stream           = DMA1_Stream7,
    .rx_dma_stream           = DMA1_Stream5,
    .tx_dma_stream_id        = 7,
    .rx_dma_stream_id        = 5,
    .tx_dma_channel          = DMA_Channel_1,
    .rx_dma_channel          = DMA_Channel_1,
    .gpio_af                 = GPIO_AF_I2C1
  },
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
  MicoGpioInitialize( Standby_SEL, INPUT_PULL_UP );
  MicoGpioEnableIRQ( Standby_SEL , IRQ_TRIGGER_FALLING_EDGE, _button_STANDBY_irq_handler, NULL);
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
        MicoGpioOutputHigh( (mico_gpio_t)MICO_SYS_LED );
    } else {
        MicoGpioOutputLow( (mico_gpio_t)MICO_SYS_LED );
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
  if(MicoGpioInputGet((mico_gpio_t)MFG_SEL)==false && MicoGpioInputGet((mico_gpio_t)MFG_SEL)==false)
    return true;
  else
    return false;
}

bool MicoShouldEnterBootloader(void)
{
  if(MicoGpioInputGet((mico_gpio_t)BOOT_SEL)==false && MicoGpioInputGet((mico_gpio_t)MFG_SEL)==true)
    return true;
  else
    return false;
}

