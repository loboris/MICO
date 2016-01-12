/**
  ******************************************************************************
  * @file    micokit_ext_def.h
  * @author  Eshen Wang
  * @version V1.0.0
  * @date    20-May-2015
  * @brief   micokit extension board peripherals pin defines.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, MXCHIP Inc. SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2014 MXCHIP Inc.</center></h2>
  ******************************************************************************
  */ 
#pragma once

#ifndef __MICOKIT_STMEMS_DEF_H_
#define __MICOKIT_STMEMS_DEF_H_

//-------------------------- MicoKit-EXT board pin define ----------------------

#define SSD1106_USE_I2C
#define OLED_I2C_PORT       (Arduino_I2C)


#define P9813_PIN_CIN       (Arduino_D9)
#define P9813_PIN_DIN       (Arduino_D8)

#define DC_MOTOR            (MICO_GPIO_21)

#define MICOKIT_STMEMS_KEY1                (Arduino_D5)
#define MICOKIT_STMEMS_KEY2                (Arduino_D6)

#define HTS221_I2C_PORT              (Arduino_I2C)
#define LPS25HB_I2C_PORT             (Arduino_I2C)
#define UVIS25_I2C_PORT              (Arduino_I2C)
#define LSM9DS1_I2C_PORT             (Arduino_I2C)

#define LIGHT_SENSOR_ADC             (Arduino_A2)

#endif  // __MICOKIT_EXT_DEF_H_
