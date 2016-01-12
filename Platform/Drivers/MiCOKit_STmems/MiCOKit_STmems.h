/**
  ******************************************************************************
  * @file    micokit_ext.h
  * @author  Eshen Wang
  * @version V1.0.0
  * @date    8-May-2015
  * @brief   micokit extension board peripherals operations..
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

#ifndef __MICOKIT_STMEMS_H_
#define __MICOKIT_STMEMS_H_

#include "Common.h"

//------------------------- MiCOKit STmems board modules drivers ------------------
#include "rgb_led/P9813/rgb_led.h"
#include "display/VGM128064/oled.h"
#include "motor/dc_motor/dc_motor.h"
#include "keypad/gpio_button/button.h"

#include "sensor/HTS221/hts221.h"
#include "sensor/LPS25HB/lps25hb.h"
#include "sensor/UVIS25/uvis25.h"
#include "sensor/LSM9DS1/lsm9ds1.h"
#include "sensor/light_adc/light_sensor.h"

//--------------------------- MiCOKit STmems board info ---------------------------
#define MICOKIT_STMEMS_MANUFACTURER    "MXCHIP"
#define MICOKIT_STMEMS_NAME            "MiCOKit-STmems"

#define MFG_TEST_MAX_MODULE_NUM      8

OSStatus micokit_STmems_init(void);    // MicoKit-EXT board init

#endif  // __MICOKIT_STMEMS_H_
