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

#include "mico_platform.h"
#include "platform.h"
#include "platform_config.h"
#include "platform_peripheral.h"
#include "platform_config.h"
#include "PlatformLogging.h"
#include "spi_flash_platform_interface.h"
#include "wlan_platform_common.h"
#include "CheckSumUtils.h"

#ifdef USE_MiCOKit_EXT
#include "micokit_ext.h"
#endif

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

/******************************************************
*               Variables Definitions
******************************************************/

const platform_gpio_t platform_gpio_pins[] =
{
  /* Common GPIOs for internal use */
  [STDIO_UART_RX]                     = { GPIOA,  3 },  
  [STDIO_UART_TX]                     = { GPIOA,  2 },  
  [FLASH_PIN_SPI_CS  ]                = { GPIOA, 15 },
  [FLASH_PIN_SPI_CLK ]                = { GPIOB,  3 },
  [FLASH_PIN_SPI_MOSI]                = { GPIOA,  7 },
  [FLASH_PIN_SPI_MISO]                = { GPIOB,  4 },

  /* GPIOs for external use */
  [MICO_GPIO_2]                       = { GPIOB,  2 },  // WiFiMCU bootloader enter
  [MICO_GPIO_8]                       = { GPIOA , 2 },  // [TX0]  (USB2SER)
  [MICO_GPIO_9]                       = { GPIOA,  1 },  // [D1]   PWM  ADC
  [MICO_GPIO_12]                      = { GPIOA,  3 },  // [RX0]  (USB2SER)
  [MICO_GPIO_14]                      = { GPIOA,  0 },  // [EN]
  [MICO_GPIO_16]                      = { GPIOC, 13 },  // [D2]
  [MICO_GPIO_17]                      = { GPIOB, 10 },  // [D3]   PWM  I2C_SCL
  [MICO_GPIO_18]                      = { GPIOB,  9 },  // [D11]  PWM  I2C_SDA
  [MICO_GPIO_19]                      = { GPIOB, 12 },  // [D4]
  [MICO_GPIO_27]                      = { GPIOA, 12 },  // [D7]   SPI5_MISO
  [MICO_GPIO_29]                      = { GPIOA, 10 },  // [D8]   SPI5_MOSI  RX1  PWM
  [MICO_GPIO_30]                      = { GPIOB,  6 },  // [D9]   TX1  PWM
  [MICO_GPIO_31]                      = { GPIOB,  8 },  // [D10]  PWM
  [MICO_GPIO_33]                      = { GPIOB, 13 },  // [D12]  PWM
  [MICO_GPIO_34]                      = { GPIOA,  5 },  // [D13]  PWM  ADC
  [MICO_GPIO_35]                      = { GPIOA, 11 },  // [D14]  PWM
  [MICO_GPIO_36]                      = { GPIOB,  1 },  // [D15]  PWM  ADC
  [MICO_GPIO_37]                      = { GPIOB,  0 },  // [D16]  SPI5_SCK
  [MICO_GPIO_38]                      = { GPIOA,  4 },  // [D17]  BLUE_LED  ADC
  
  [MICO_GPIO_25]                      = { GPIOA, 14 },  // [D5]   SW_CLK 
  [MICO_GPIO_26]                      = { GPIOA, 13 },  // [D6]   SW_DIO
};

//const platform_pwm_t *platform_pwm_peripherals = NULL;
const platform_pwm_t  platform_pwm_peripherals[] =
{
  [MICO_PWM_1] =
  {
    .tim                          = TIM5,
    .channel                      = 2,
    .tim_peripheral_clock         = RCC_APB1Periph_TIM5,
    .gpio_af                      = GPIO_AF_TIM5,
    .pin                          = &platform_gpio_pins[MICO_GPIO_9],
  },
  [MICO_PWM_2] =
  {
    .tim                          = TIM2,
    .channel                      = 3,
    .tim_peripheral_clock         = RCC_APB1Periph_TIM2,
    .gpio_af                      = GPIO_AF_TIM2,
    .pin                          = &platform_gpio_pins[MICO_GPIO_17],
  },
  [MICO_PWM_3] =
  {
    .tim                          = TIM11,
    .channel                      = 1,
    .tim_peripheral_clock         = RCC_APB2Periph_TIM11,
    .gpio_af                      = GPIO_AF_TIM11,
    .pin                          = &platform_gpio_pins[MICO_GPIO_18],
  },
  [MICO_PWM_4] =
  {
    .tim                          = TIM1,
    .channel                      = 3,
    .tim_peripheral_clock         = RCC_APB2Periph_TIM1,
    .gpio_af                      = GPIO_AF_TIM1,
    .pin                          = &platform_gpio_pins[MICO_GPIO_29],
  },
  [MICO_PWM_5] =
  {
    .tim                          = TIM4,
    .channel                      = 1,
    .tim_peripheral_clock         = RCC_APB1Periph_TIM4,
    .gpio_af                      = GPIO_AF_TIM4,
    .pin                          = &platform_gpio_pins[MICO_GPIO_30],
  },  
  [MICO_PWM_6] =
  {
    .tim                          = TIM10,
    .channel                      = 1,
    .tim_peripheral_clock         = RCC_APB2Periph_TIM10,
    .gpio_af                      = GPIO_AF_TIM10,
    .pin                          = &platform_gpio_pins[MICO_SYS_LED],
  },
  [MICO_PWM_7] =
  {
    .tim                          = TIM1,
    .channel                      = 1,
    .tim_peripheral_clock         = RCC_APB2Periph_TIM1,
    .gpio_af                      = GPIO_AF_TIM1,
    .pin                          = &platform_gpio_pins[MICO_GPIO_33],
  },
  [MICO_PWM_8] =
  {
    .tim                          = TIM2,
    .channel                      = 1,
    .tim_peripheral_clock         = RCC_APB1Periph_TIM2,
    .gpio_af                      = GPIO_AF_TIM2,
    .pin                          = &platform_gpio_pins[MICO_GPIO_34],
  },
  [MICO_PWM_9] =
  {
    .tim                          = TIM1,
    .channel                      = 4,
    .tim_peripheral_clock         = RCC_APB2Periph_TIM1,
    .gpio_af                      = GPIO_AF_TIM1,
    .pin                          = &platform_gpio_pins[MICO_GPIO_35],
  },
  [MICO_PWM_10] =
  {
    .tim                          = TIM3,
    .channel                      = 4,
    .tim_peripheral_clock         = RCC_APB1Periph_TIM3,
    .gpio_af                      = GPIO_AF_TIM3,
    .pin                          = &platform_gpio_pins[MICO_GPIO_36],
  },
  [MICO_PWM_11] =
  {
    .tim                          = TIM3,
    .channel                      = 3,
    .tim_peripheral_clock         = RCC_APB1Periph_TIM3,
    .gpio_af                      = GPIO_AF_TIM3,
    .pin                          = &platform_gpio_pins[MICO_GPIO_37],
  },
  
};

const platform_i2c_t platform_i2c_peripherals[] =
{
  [MICO_I2C_1] =
  {
    .port                         = I2C2,
    .pin_scl                      = &platform_gpio_pins[MICO_GPIO_17],
    .pin_sda                      = &platform_gpio_pins[MICO_GPIO_18],
    .peripheral_clock_reg         = RCC_APB1Periph_I2C2,
    .tx_dma                       = DMA1,
    .tx_dma_peripheral_clock      = RCC_AHB1Periph_DMA1,
    .tx_dma_stream                = DMA1_Stream7,
    .rx_dma_stream                = DMA1_Stream5,
    .tx_dma_stream_id             = 7,
    .rx_dma_stream_id             = 5,
    .tx_dma_channel               = DMA_Channel_1,
    .rx_dma_channel               = DMA_Channel_1,
    .gpio_af_scl                  = GPIO_AF_I2C2,
    .gpio_af_sda                  = GPIO_AF9_I2C2
  },
};

platform_i2c_driver_t platform_i2c_drivers[MICO_I2C_MAX];

const platform_uart_t platform_uart_peripherals[] =
{
  [MICO_UART_1] =
  {
    .port                         = USART2,
    .pin_tx                       = &platform_gpio_pins[STDIO_UART_TX],
    .pin_rx                       = &platform_gpio_pins[STDIO_UART_RX],
    .pin_cts                      = NULL,
    .pin_rts                      = NULL,
    .tx_dma_config =
    {
      .controller                 = DMA1,
      .stream                     = DMA1_Stream6,
      .channel                    = DMA_Channel_4,
      .irq_vector                 = DMA1_Stream6_IRQn,
      .complete_flags             = DMA_HISR_TCIF6,
      .error_flags                = ( DMA_HISR_TEIF6 | DMA_HISR_FEIF6 ),
    },
    .rx_dma_config =
    {
      .controller                 = DMA1,
      .stream                     = DMA1_Stream5,
      .channel                    = DMA_Channel_4,
      .irq_vector                 = DMA1_Stream5_IRQn,
      .complete_flags             = DMA_HISR_TCIF5,
      .error_flags                = ( DMA_HISR_TEIF5 | DMA_HISR_FEIF5 | DMA_HISR_DMEIF5 ),
    },
  },
  [MICO_UART_2] =
  {
    .port                         = USART1,
    .pin_tx                       = &platform_gpio_pins[MICO_GPIO_30],
    .pin_rx                       = &platform_gpio_pins[MICO_GPIO_29],
    .pin_cts                      = NULL,
    .pin_rts                      = NULL,
    .tx_dma_config =
    {
      .controller                 = DMA2,
      .stream                     = DMA2_Stream7,
      .channel                    = DMA_Channel_4,
      .irq_vector                 = DMA2_Stream7_IRQn,
      .complete_flags             = DMA_HISR_TCIF7,
      .error_flags                = ( DMA_HISR_TEIF7 | DMA_HISR_FEIF7 ),
    },
    .rx_dma_config =
    {
      .controller                 = DMA2,
      .stream                     = DMA2_Stream2,
      .channel                    = DMA_Channel_4,
      .irq_vector                 = DMA2_Stream2_IRQn,
      .complete_flags             = DMA_LISR_TCIF2,
      .error_flags                = ( DMA_LISR_TEIF2 | DMA_LISR_FEIF2 | DMA_LISR_DMEIF2 ),
    },
  },
};
platform_uart_driver_t platform_uart_drivers[MICO_UART_MAX];

/* NOTE: SPI peripherals SPI4, SPI5 and SPI6 might have issues with RX DMA
 *       See https://github.com/MXCHIP-EMW/WICED-for-EMW/pull/13
 *       and https://github.com/MXCHIP-EMW/WICED-for-EMW/issues/9
 */
const platform_spi_t platform_spi_peripherals[] =
{
  [MICO_SPI_1]  =
  {
    .port                         = SPI1,
    .gpio_af                      = GPIO_AF_SPI1,
    .peripheral_clock_reg         = RCC_APB2Periph_SPI1,
    .peripheral_clock_func        = RCC_APB2PeriphClockCmd,
    .pin_mosi                     = &platform_gpio_pins[FLASH_PIN_SPI_MOSI],
    .pin_miso                     = &platform_gpio_pins[FLASH_PIN_SPI_MISO],
    .pin_clock                    = &platform_gpio_pins[FLASH_PIN_SPI_CLK],
    .tx_dma =
    {
      .controller                 = DMA2,
      .stream                     = DMA2_Stream5,
      .channel                    = DMA_Channel_3,
      .irq_vector                 = DMA2_Stream5_IRQn,
      .complete_flags             = DMA_HISR_TCIF5,
      .error_flags                = ( DMA_HISR_TEIF5 | DMA_HISR_FEIF5 ),
    },
    .rx_dma =
    {
      .controller                 = DMA2,
      .stream                     = DMA2_Stream0,
      .channel                    = DMA_Channel_3,
      .irq_vector                 = DMA2_Stream0_IRQn,
      .complete_flags             = DMA_LISR_TCIF0,
      .error_flags                = ( DMA_LISR_TEIF0 | DMA_LISR_FEIF0 | DMA_LISR_DMEIF0 ),
    },
  },
  /*[MICO_SPI_4]  =
  {
    .port                         = SPI4,
    .gpio_af                      = GPIO_AF6_SPI4,
    .peripheral_clock_reg         = RCC_APB2Periph_SPI4,
    .peripheral_clock_func        = RCC_APB2PeriphClockCmd,
    .pin_mosi                     = &platform_gpio_pins[MICO_GPIO_9],
    .pin_miso                     = &platform_gpio_pins[MICO_GPIO_35],
    .pin_clock                    = &platform_gpio_pins[MICO_GPIO_33],
    .tx_dma =
    {
      .controller                 = DMA2,
      .stream                     = DMA2_Stream1,
      .channel                    = DMA_Channel_4,
      .irq_vector                 = DMA2_Stream1_IRQn,
      .complete_flags             = DMA_LISR_TCIF1,
      .error_flags                = ( DMA_LISR_TEIF1 | DMA_LISR_FEIF1 ),
    },
    .rx_dma =
    {
      .controller                 = DMA2,
      .stream                     = DMA2_Stream4,
      .channel                    = DMA_Channel_4,
      .irq_vector                 = DMA2_Stream4_IRQn,
      .complete_flags             = DMA_HISR_TCIF4,
      .error_flags                = ( DMA_HISR_TEIF4 | DMA_HISR_FEIF4 | DMA_HISR_DMEIF4 ),
    },
  },*/
  [MICO_SPI_5]  =
  {
    .port                         = SPI5,
    .gpio_af                      = GPIO_AF6_SPI5,
    .peripheral_clock_reg         = RCC_APB2Periph_SPI5,
    .peripheral_clock_func        = RCC_APB2PeriphClockCmd,
    .pin_mosi                     = &platform_gpio_pins[MICO_GPIO_29],
    .pin_miso                     = &platform_gpio_pins[MICO_GPIO_27],
    .pin_clock                    = &platform_gpio_pins[MICO_GPIO_37],
    .tx_dma =
    {
      .controller                 = DMA2,
      .stream                     = DMA2_Stream6,
      .channel                    = DMA_Channel_7,
      .irq_vector                 = DMA2_Stream6_IRQn,
      .complete_flags             = DMA_HISR_TCIF6,
      .error_flags                = ( DMA_HISR_TEIF6 | DMA_HISR_FEIF6 ),
    },
    .rx_dma =
    {
      .controller                 = DMA2,
      .stream                     = DMA2_Stream5,
      .channel                    = DMA_Channel_7,
      .irq_vector                 = DMA2_Stream5_IRQn,
      .complete_flags             = DMA_HISR_TCIF5,
      .error_flags                = ( DMA_HISR_TEIF5 | DMA_HISR_FEIF5 | DMA_HISR_DMEIF5 ),
    },
  }
};

platform_spi_driver_t platform_spi_drivers[MICO_SPI_MAX];

/* Flash memory devices */
const platform_flash_t platform_flash_peripherals[] =
{
  [MICO_FLASH_EMBEDDED] =
  {
    .flash_type                   = FLASH_TYPE_EMBEDDED,
    .flash_start_addr             = 0x08000000,
    .flash_length                 = 0x80000,
  },
  [MICO_FLASH_SPI] =
  {
    .flash_type                   = FLASH_TYPE_SPI,
    .flash_start_addr             = 0x000000,
    .flash_length                 = 0x200000,
  },
};

platform_flash_driver_t platform_flash_drivers[MICO_FLASH_MAX];

/* Logic partition on flash devices */
const mico_logic_partition_t mico_partitions[] =
{
  [MICO_PARTITION_BOOTLOADER] =
  {
    .partition_owner           = MICO_FLASH_EMBEDDED,
    .partition_description     = "Bootloader",
    .partition_start_addr      = 0x08000000,
    .partition_length          =     0x8000,    //32k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
  },
  [MICO_PARTITION_APPLICATION] =
  {
    .partition_owner           = MICO_FLASH_EMBEDDED,
    .partition_description     = "Application",
    .partition_start_addr      = 0x0800C000,
    .partition_length          =    0x74000,   //464k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
  },
  [MICO_PARTITION_RF_FIRMWARE] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "RF Firmware",
    .partition_start_addr      = 0x2000,
    .partition_length          = 0x3E000,  //248k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
  },
  [MICO_PARTITION_OTA_TEMP] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "OTA Storage",
    .partition_start_addr      = 0x40000,
    //.partition_length          = 0x70000, //448k bytes
    .partition_length          = 0x4000, //16k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
  },
  [MICO_PARTITION_LUA] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "LUA Storage",
    //.partition_start_addr      = 0xB0000,
    //.partition_length          = 0x150000, //1344k bytes
    .partition_start_addr      = 0x44000,
    .partition_length          = 0x1BC000, //1776k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
  },
  [MICO_PARTITION_PARAMETER_1] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "PARAMETER1",
    .partition_start_addr      = 0x0,
    .partition_length          = 0x1000, // 4k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
  },
  [MICO_PARTITION_PARAMETER_2] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "PARAMETER2",
    .partition_start_addr      = 0x1000,
    .partition_length          = 0x1000, //4k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
  },
  [MICO_PARTITION_ATE] =
  {
    .partition_owner           = MICO_FLASH_NONE,
  }
};


#if defined ( USE_MICO_SPI_FLASH )
const mico_spi_device_t mico_spi_flash =
{
  .port        = MICO_SPI_1,
  .chip_select = FLASH_PIN_SPI_CS,
  .speed       = 40000000,
  .mode        = (SPI_CLOCK_RISING_EDGE | SPI_CLOCK_IDLE_HIGH | SPI_USE_DMA | SPI_MSB_FIRST ),
  .bits        = 8
};
#endif

const platform_adc_t platform_adc_peripherals[] =
{
  [MICO_ADC_1] = { ADC1, ADC_Channel_1, RCC_APB2Periph_ADC1, 1, (platform_gpio_t*)&platform_gpio_pins[MICO_GPIO_9] },
  [MICO_ADC_2] = { ADC1, ADC_Channel_5, RCC_APB2Periph_ADC1, 1, (platform_gpio_t*)&platform_gpio_pins[MICO_GPIO_34] },
  [MICO_ADC_3] = { ADC1, ADC_Channel_9, RCC_APB2Periph_ADC1, 1, (platform_gpio_t*)&platform_gpio_pins[MICO_GPIO_36] },
  [MICO_ADC_4] = { ADC1, ADC_Channel_8, RCC_APB2Periph_ADC1, 1, (platform_gpio_t*)&platform_gpio_pins[MICO_GPIO_37] },
  [MICO_ADC_5] = { ADC1, ADC_Channel_4, RCC_APB2Periph_ADC1, 1, (platform_gpio_t*)&platform_gpio_pins[MICO_GPIO_38] },
  [MICO_ADC_6] = { ADC1, ADC_Channel_16, RCC_APB2Periph_ADC1, 1, NULL },
  [MICO_ADC_7] = { ADC1, ADC_Channel_17, RCC_APB2Periph_ADC1, 1, NULL },
};

/* Wi-Fi control pins. Used by platform/MCU/wlan_platform_common.c
* SDIO: EMW1062_PIN_BOOTSTRAP[1:0] = b'00
* gSPI: EMW1062_PIN_BOOTSTRAP[1:0] = b'01
*/
const platform_gpio_t wifi_control_pins[] =
{
  [WIFI_PIN_RESET]           = { GPIOB, 14 },
};

/* Wi-Fi SDIO bus pins. Used by platform/MCU/STM32F2xx/EMW1062_driver/wlan_SDIO.c */
const platform_gpio_t wifi_sdio_pins[] =
{
  [WIFI_PIN_SDIO_OOB_IRQ] = { GPIOA,  0 },
  [WIFI_PIN_SDIO_CLK    ] = { GPIOB, 15 },
  [WIFI_PIN_SDIO_CMD    ] = { GPIOA,  6 },
  [WIFI_PIN_SDIO_D0     ] = { GPIOB,  7 },
  [WIFI_PIN_SDIO_D1     ] = { GPIOA,  8 },
  [WIFI_PIN_SDIO_D2     ] = { GPIOA,  9 },
  [WIFI_PIN_SDIO_D3     ] = { GPIOB,  5 },
};


/******************************************************
*           Interrupt Handler Definitions
******************************************************/

MICO_RTOS_DEFINE_ISR( USART1_IRQHandler )
{
  platform_uart_irq( &platform_uart_drivers[MICO_UART_2] );
}

MICO_RTOS_DEFINE_ISR( USART2_IRQHandler )
{
  platform_uart_irq( &platform_uart_drivers[MICO_UART_1] );
}

MICO_RTOS_DEFINE_ISR( DMA1_Stream6_IRQHandler )
{
  platform_uart_tx_dma_irq( &platform_uart_drivers[MICO_UART_1] );
}

MICO_RTOS_DEFINE_ISR( DMA2_Stream7_IRQHandler )
{
  platform_uart_tx_dma_irq( &platform_uart_drivers[MICO_UART_2] );
}

MICO_RTOS_DEFINE_ISR( DMA1_Stream5_IRQHandler )
{
  platform_uart_rx_dma_irq( &platform_uart_drivers[MICO_UART_1] );
}

MICO_RTOS_DEFINE_ISR( DMA2_Stream2_IRQHandler )
{
  platform_uart_rx_dma_irq( &platform_uart_drivers[MICO_UART_2] );
}


/******************************************************
*               Function Definitions
******************************************************/

void platform_init_peripheral_irq_priorities( void )
{
  /* Interrupt priority setup. Called by MICO/platform/MCU/STM32F2xx/platform_init.c */
  NVIC_SetPriority( RTC_WKUP_IRQn    ,  1 ); /* RTC Wake-up event   */
  NVIC_SetPriority( SDIO_IRQn        ,  2 ); /* WLAN SDIO           */
  NVIC_SetPriority( DMA2_Stream3_IRQn,  3 ); /* WLAN SDIO DMA       */
  //NVIC_SetPriority( DMA1_Stream3_IRQn,  3 ); /* WLAN SPI DMA        */
  NVIC_SetPriority( USART1_IRQn      ,  6 ); /* MICO_UART_1         */
  NVIC_SetPriority( USART2_IRQn      ,  6 ); /* MICO_UART_2         */
  NVIC_SetPriority( DMA1_Stream6_IRQn,  7 ); /* MICO_UART_1 TX DMA  */
  NVIC_SetPriority( DMA1_Stream5_IRQn,  7 ); /* MICO_UART_1 RX DMA  */
  NVIC_SetPriority( DMA2_Stream7_IRQn,  7 ); /* MICO_UART_2 TX DMA  */
  NVIC_SetPriority( DMA2_Stream2_IRQn,  7 ); /* MICO_UART_2 RX DMA  */
  NVIC_SetPriority( EXTI0_IRQn       , 14 ); /* GPIO                */
  NVIC_SetPriority( EXTI1_IRQn       , 14 ); /* GPIO                */
  NVIC_SetPriority( EXTI2_IRQn       , 14 ); /* GPIO                */
  NVIC_SetPriority( EXTI3_IRQn       , 14 ); /* GPIO                */
  NVIC_SetPriority( EXTI4_IRQn       , 14 ); /* GPIO                */
  NVIC_SetPriority( EXTI9_5_IRQn     , 14 ); /* GPIO                */
  NVIC_SetPriority( EXTI15_10_IRQn   , 14 ); /* GPIO                */
}


void init_platform( void )
{
  MicoGpioInitialize((mico_gpio_t)BOOT_SEL, INPUT_PULL_UP);
  MicoGpioInitialize((mico_gpio_t)MFG_SEL, INPUT_PULL_UP);
}

#ifdef BOOTLOADER

#define SizePerRW                   (4096)
static uint8_t data[SizePerRW];

void init_platform_bootloader( void )
{
  OSStatus err = kNoErr;
  mico_logic_partition_t *rf_partition = MicoFlashGetInfo( MICO_PARTITION_RF_FIRMWARE );
  
  MicoGpioInitialize( (mico_gpio_t)MICO_SYS_LED, OUTPUT_PUSH_PULL );
  MicoGpioOutputLow( (mico_gpio_t)MICO_SYS_LED );
  MicoGpioInitialize( (mico_gpio_t)MICO_RF_LED, OUTPUT_OPEN_DRAIN_NO_PULL );
  MicoGpioOutputHigh( (mico_gpio_t)MICO_RF_LED );
  
  MicoGpioInitialize((mico_gpio_t)BOOT_SEL, INPUT_PULL_UP);
  MicoGpioInitialize((mico_gpio_t)MFG_SEL, INPUT_PULL_UP);
  
  /* Specific operations used in EMW3165 production */
#define NEED_RF_DRIVER_COPY_BASE    ((uint32_t)0x08008000)
#define TEMP_RF_DRIVER_BASE         ((uint32_t)0x08040000)
#define TEMP_RF_DRIVER_END          ((uint32_t)0x0807FFFF)
  
  const uint8_t isDriverNeedCopy = *(uint8_t *)(NEED_RF_DRIVER_COPY_BASE);
  const uint32_t totalLength = rf_partition->partition_length;
  const uint8_t crcResult = *(uint8_t *)(TEMP_RF_DRIVER_END);
  uint8_t targetCrcResult = 0;
  
  uint32_t copyLength;
  uint32_t destStartAddress_tmp = rf_partition->partition_start_addr;
  uint32_t sourceStartAddress_tmp = TEMP_RF_DRIVER_BASE;
  uint32_t i;
  
  if ( isDriverNeedCopy != 0x0 )
    return;
  
  platform_log( "Bootloader start to copy RF driver..." );
  /* Copy RF driver to SPI flash */

  err = platform_flash_init( &platform_flash_peripherals[ MICO_FLASH_SPI ] );
  require_noerr(err, exit);
  err = platform_flash_init( &platform_flash_peripherals[ MICO_FLASH_EMBEDDED ] );
  require_noerr(err, exit);
  err = platform_flash_erase( &platform_flash_peripherals[ MICO_FLASH_SPI ], 
    rf_partition->partition_start_addr, rf_partition->partition_start_addr + rf_partition->partition_length - 1 );
  require_noerr(err, exit);
  platform_log( "Time: %d", mico_get_time_no_os() );
  
  for(i = 0; i <= totalLength/SizePerRW; i++){
    if( i == totalLength/SizePerRW ){
      if(totalLength%SizePerRW)
        copyLength = totalLength%SizePerRW;
      else
        break;
    }else{
      copyLength = SizePerRW;
    }
    printf(".");
    err = platform_flash_read( &platform_flash_peripherals[ MICO_FLASH_EMBEDDED ], &sourceStartAddress_tmp, data , copyLength );
    require_noerr( err, exit );
    err = platform_flash_write( &platform_flash_peripherals[ MICO_FLASH_SPI ], &destStartAddress_tmp, data, copyLength );
    require_noerr(err, exit);
  }
  
  printf("\r\n");
  /* Check CRC-8 check-sum */
  platform_log( "Bootloader start to verify RF driver..." );
  sourceStartAddress_tmp = TEMP_RF_DRIVER_BASE;
  destStartAddress_tmp = rf_partition->partition_start_addr;
  
  for(i = 0; i <= totalLength/SizePerRW; i++){
    if( i == totalLength/SizePerRW ){
      if(totalLength%SizePerRW)
        copyLength = totalLength%SizePerRW;
      else
        break;
    }else{
      copyLength = SizePerRW;
    }
    printf(".");
    err = platform_flash_read( &platform_flash_peripherals[ MICO_FLASH_SPI ], &destStartAddress_tmp, data, copyLength );
    require_noerr( err, exit );
    
    targetCrcResult = mico_CRC8_Table(targetCrcResult, data, copyLength);
  }
  
  printf("\r\n");
  //require_string( crcResult == targetCrcResult, exit, "Check-sum error" ); 
  if( crcResult != targetCrcResult ){
    platform_log("Check-sum error");
    while(1);
  }
  /* Clear RF driver from temperary storage */
  platform_log("Bootloader start to clear RF driver temporary storage...");
  
  /* Clear copy tag */
  err = platform_flash_erase( &platform_flash_peripherals[ MICO_FLASH_EMBEDDED ], NEED_RF_DRIVER_COPY_BASE, NEED_RF_DRIVER_COPY_BASE);
  require_noerr(err, exit);
  
exit:
  return;
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

#ifdef USE_MiCOKit_EXT
// add test mode for MiCOKit-EXT board,check Arduino_D5 pin when system startup
bool MicoExtShouldEnterTestMode(void)
{
  if( MicoGpioInputGet((mico_gpio_t)Arduino_D5)==false ){
    return true;
  }
  else{
    return false;
  }
}
#endif

bool MicoShouldEnterMFGMode(void)
{
    return false;
}

bool MicoShouldEnterBootloader(void)
{
  if(MicoGpioInputGet((mico_gpio_t)MICO_GPIO_2)==false)
    return true;
  else
    return false;
}

//doit
unsigned char boot_reason=BOOT_REASON_NONE;

void platform_check_bootreason( void )
{
  boot_reason=0;
  if(RCC_GetFlagStatus(RCC_FLAG_IWDGRST))
  {
    boot_reason=BOOT_REASON_WDG_RST;
  }
  else if(RCC_GetFlagStatus(RCC_FLAG_WWDGRST))
  {
    boot_reason=BOOT_REASON_WWDG_RST;
  }
  else if(RCC_GetFlagStatus(RCC_FLAG_LPWRRST))
  {
    boot_reason=BOOT_REASON_LOWPWR_RST;
  }
  else if(RCC_GetFlagStatus(RCC_FLAG_BORRST))
  {
    boot_reason=BOOT_REASON_BOR_RST;
  }
  else if(RCC_GetFlagStatus(RCC_FLAG_PORRST))
  {//Power-On-Reset
    boot_reason=BOOT_REASON_PWRON_RST;
  }
  else if(RCC_GetFlagStatus(RCC_FLAG_SFTRST))
  {//Software Reset
    boot_reason=BOOT_REASON_SOFT_RST;
  }
  else if(RCC_GetFlagStatus(RCC_FLAG_PINRST))
  {//Always set, test other cases first
    boot_reason=BOOT_REASON_EXPIN_RST;
  }
  RCC_ClearFlag();
}

