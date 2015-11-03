/**
******************************************************************************
* @file    platform.h
* @author  William Xu
* @version V1.0.0
* @date    05-May-2014
* @brief   This file provides all MICO Peripherals defined for current platform.
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

#include "platform_common_config.h"

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/
  
#define HARDWARE_REVISION   "3161"
#define DEFAULT_NAME        "EMW3161 Module"
#define MODEL               "EMW3161"


   
/******************************************************
 *                   Enumerations
 ******************************************************/

/*
EMW3162 on EMB-380-S platform pin definitions ...
+-------------------------------------------------------------------------+
| Enum ID       |Pin | STM32| Peripheral  |    Board     |   Peripheral   |
|               | #  | Port | Available   |  Connection  |     Alias      |
|---------------+----+------+-------------+--------------+----------------|
| MICO_GPIO_1   | 1  | B  6 | GPIO        |              |                |
|               |    |      | TIM4_CH1    |              |                |
|               |    |      | CAN2_TX     |              |                |
|               |    |      | USART1_TX   |              |                |
|               |    |      | I2C1_SCL    |              | MICO_I2C1_SCL  |
|               |    |      | CAN2_TX     |              |                |
|---------------+----+------+-------------+--------------+----------------|
| MICO_GPIO_2   | 2  | B  7 | GPIO        |              |                |
|               |    |      | I2C1_SCL    |              | MICO_I2C1_SDA  |
|               |    |      | USART1_RX   |              |                |
|               |    |      | TIM4_CH2    |              |                |
|---------------+----+------+-------------+--------------+----------------|
|               | 3  | A  13| SWDIO       |              |                |
|---------------+----+------+-------------+--------------+----------------|
| MICO_GPIO_4   | 4  | C  6 | USART6_TX   |              |                |
|               |    |      | GPIO        |              |                |
|               |    |      | TIM8_CH1    |              |                |
|               |    |      | TIM3_CH1    |              |                |
|               |    |      | I2S2_MCK    |              |                |
|---------------+----+------+-------------+--------------+----------------|
| MICO_GPIO_5   | 5  | H  9 | DCMI_D0     |              |                |
|               |    |      | GPIO        |              |                |
|               |    |      | I2C3_SMBA   |              |                |
|               |    |      | TIM12_CH2   |              |                |
|               |    |      |             |              |                |
|               |    |      |             |              |                |
|---------------+----+------+-------------+--------------+----------------|
| MICO_GPIO_6   | 6  | I  0 | SPI2_NSS    |              | MICO_SPI_1_NSS |
|               |    |      | GPIO        |              |                |
|               |    |      | TIM5_CH4    |              |                |
|               |    |      | I2S2_WS     |              |                |
|---------------+----+------+-------------+--------------+----------------|
| MICO_GPIO_7   | 7  | I  1 | SPI2_SCK    |              | MICO_SPI_1_SCK |
|               |    |      | GPIO        |              |                |
|               |    |      | I2S2_SCK    |              |                |
|---------------+----+------+-------------+--------------+----------------|
| MICO_GPIO_8   | 8  | I  2 | SPI2_MISO   |              | MICO_SPI_1_MISO|
|               |    |      | GPIO        |              |                |
|               |    |      | TIM8_CH4    |              |                |
|---------------+----+------+-------------+--------------+----------------|
| MICO_GPIO_9   | 9  | I  3 | SPI2_MOSI   |              | MICO_SPI_1_MISO|
|               |    |      | GPIO        |              |                |
|               |    |      | 2S2_SD      |              |                |
|               |    |      | TIM8_ETR    |              |                |
|---------------+----+------+-------------+--------------+----------------|
| MICO_GPIO_10  | 10 | E  5 | GPIO        |              | MICO_PWM_1     |
|               |    |      | TIM9_CH1    |              |                |
|---------------+----+------+-------------+--------------+----------------|
| MICO_GPIO_11  | 11 | H 10 | GPIO        |EasyLink_BUTTON |              |
|               |    |      | TIM5_CH1    |              |                |
|---------------+----+------+-------------+--------------+----------------|
| MICO_GPIO_12  | 12 | B 14 | GPIO        |              |                |
|               |    |      | OTG_HS_DM   |              |                |
|               |    |      | TIM12_CH1   |              |                | 
|               |    |      | TIM1_CH2N   |              |                | 
|               |    |      | TIM8_CH2N   |              |                |   
|---------------+----+------+-------------+--------------+----------------|
| MICO_GPIO_13  | 13 | B 15 | GPIO        |              |                |
|               |    |      | TIM1_CH3N   |              |  MICO_PWM_2    |
|               |    |      | TIM12_CH2   |              |                |
|               |    |      | OTG_HS_DP   |              |                |  
|---------------+----+------+-------------+--------------+----------------|
| MICO_GPIO_14  | 14 | C  6 | GPIO        |              |                |
|               |    |      | TIM5_CH2    |              |                |
|---------------+----+------+-------------+--------------+----------------|
|               | 15 | GND  |             |              |                |
|---------------+----+------+-------------+--------------+----------------|
| MICO_GPIO_16  | 16 | E  6 | GPIO        |  RF_LED      |                |
|               |    |      | TIM9_CH2    |              |                |
|---------------+----+------+-------------+--------------+----------------|
|               | 17 |nReset|             |              |                |
|---------------+----+------+-------------+--------------+----------------|
| MICO_GPIO_18  | 18 | A 10 | GPIO        |              |                |
|               |    |      | USART1_RX   |              |                |
|               |    |      | TIM1_CH3    |              |                |
|               |    |      | OTG_FS_ID   |              |                |
+---------------+----+--------------------+--------------+----------------+
| MICO_GPIO_19  | 19 | A  9 | GPIO        |              |                |
|               |    |      | USART1_TX   |              | MICO_PWM_3     |
|               |    |      | TIM1_CH2    |              |                |
|               |    |      | I2C3_SMBA   |              |                |
+---------------+----+--------------------+--------------+----------------+
| MICO_GPIO_20  | 20 | A  1 | GPIO        |              |                |
|               |    |      | USART2_RTS  |              |                |
|               |    |      | TIM5_CH2    |              |                |
|               |    |      | TIM2_CH2    |              |                |
+---------------+----+--------------------+--------------+----------------+
| MICO_GPIO_21  | 21 | A  0 | GPIO        |              |                |
|               |    |      | USART2_CTS  |              |                |
|               |    |      | IM2_CH1_ETR |              |                |
|               |    |      | TIM5_CH1    |              |                |
|               |    |      | TIM8_ETR    |              |                |
+---------------+----+--------------------+--------------+----------------+
| MICO_GPIO_22  | 22 | A  2 | GPIO        |              |                |
|               |    |      | USART1_TX   |STDIO_UART_TX |                |
|               |    |      | TIM5_CH3    |              |                |
+---------------+----+--------------------+--------------+----------------+
| MICO_GPIO_23  | 23 | A  3 | GPIO        |              |                |
|               |    |      | USART2_RX   |STDIO_UART_RX |                |
|               |    |      | TIM9_CH2    |              |                |
|               |    |      | TIM2_CH4    |              |                |
+---------------+----+--------------------+--------------+----------------+
|               | 24 | VCC  |             |              |                |
+---------------+----+--------------------+--------------+----------------+
|               | 25 | GND  |             |              |                |
+---------------+----+--------------------+--------------+----------------+
|               | 26 | NC   |             |              |                |
+---------------+----+--------------------+--------------+----------------+
|               | 27 | BOOT0|             |              |                |
+---------------+----+--------------------+--------------+----------------+
|               | 28 | A 14 | JTCK-SWCLK  |              |                |
+---------------+----+--------------------+--------------+----------------+
| MICO_GPIO_29  | 29 | F  0 | GPIO        |StandBy/WakeUp|                |
|               |    |      | I2C2_SDA    |              |                |
+---------------+----+--------------------+--------------+----------------+
| MICO_GPIO_30  | 30 | F  1 | GPIO        | Status_Sel   |                |
|               |    |      | I2C2_SCL    |              |                |
+---------------+----+--------------------+--------------+----------------+
| MICO_SYS_LED  |    | B  0 | GPIO        |              |                |
+---------------+----+--------------------+--------------+----------------+

Notes
1. These mappings are defined in <MICO-SDK>/Platform/BCM943362WCD4/platform.c
2. STM32F2xx Datasheet  -> http://www.st.com/web/en/resource/technical/document/datasheet/CD00237391.pdf
3. STM32F2xx Ref Manual -> http://www.st.com/web/en/resource/technical/document/reference_manual/CD00225773.pdf
*/


typedef enum
{
    MICO_GPIO_1 = MICO_COMMON_GPIO_MAX,
    MICO_GPIO_2,
    //MICO_GPIO_3,
    MICO_GPIO_4,
    MICO_GPIO_5,
    MICO_GPIO_6,
    MICO_GPIO_7,
    MICO_GPIO_8,
    MICO_GPIO_9,
    MICO_GPIO_10,
    //MICO_GPIO_11,
    MICO_GPIO_12,
    MICO_GPIO_13,
    MICO_GPIO_14,
    //MICO_GPIO_15,
    //MICO_GPIO_16,
    //MICO_GPIO_17,
    MICO_GPIO_18,
    MICO_GPIO_19,
    MICO_GPIO_20,
    MICO_GPIO_21,
    MICO_GPIO_22,
    MICO_GPIO_23,
    //MICO_GPIO_24,
    //MICO_GPIO_25,
    //MICO_GPIO_26,
    //MICO_GPIO_27,
    //MICO_GPIO_28,
    MICO_GPIO_29,
    //MICO_GPIO_30,

    MICO_GPIO_MAX, /* Denotes the total number of GPIO port aliases. Not a valid GPIO alias */
} mico_gpio_t;

typedef enum
{
    MICO_SPI_1,
    MICO_SPI_MAX, /* Denotes the total number of SPI port aliases. Not a valid SPI alias */
} mico_spi_t;

typedef enum
{
    MICO_I2C_1,
    MICO_I2C_MAX, /* Denotes the total number of I2C port aliases. Not a valid I2C alias */
} mico_i2c_t;

typedef enum
{
    MICO_PWM_1 = MICO_COMMON_PWM_MAX,
    MICO_PWM_2,
    MICO_PWM_3,
    MICO_PWM_MAX, /* Denotes the total number of PWM port aliases. Not a valid PWM alias */
} mico_pwm_t;

typedef enum
{
    MICO_ADC_1,
    MICO_ADC_2,
    MICO_ADC_3,
    MICO_ADC_MAX, /* Denotes the total number of ADC port aliases. Not a valid ADC alias */
} mico_adc_t;

typedef enum
{
    MICO_UART_1,
    MICO_UART_2,
    MICO_UART_MAX, /* Denotes the total number of UART port aliases. Not a valid UART alias */
} mico_uart_t;

typedef enum
{
  MICO_SPI_FLASH,
  MICO_INTERNAL_FLASH,
  MICO_FLASH_MAX,
} mico_flash_t;

#define STM32_UART_1 NULL
#define STM32_UART_2 MICO_UART_1
#define STM32_UART_6 NULL

/* Components connected to external I/Os*/
#define Standby_SEL         (MICO_GPIO_29)

/* I/O connection <-> Peripheral Connections */
#define MICO_I2C_CP         (MICO_I2C_1)

#define RestoreDefault_TimeOut          3000  /**< Restore default and start easylink after 
                                                   press down EasyLink button for 3 seconds. */

#ifdef __cplusplus
} /*extern "C" */
#endif

