/**
  ******************************************************************************
  * @file    user_properties.h
  * @author  Eshen Wang
  * @version V1.0.0
  * @date    18-Mar-2015
  * @brief   device properties data.
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

#ifndef __MICO_DEVICE_PROPERTIES_USER_H_
#define __MICO_DEVICE_PROPERTIES_USER_H_

#include "mico.h"

/*******************************************************************************
 *                                 DEFINES
 ******************************************************************************/
#define MAX_DEVICE_NAME_SIZE            16
#define MAX_USER_UART_BUF_SIZE          32 // for OLED display
 
//#define USE_COMMON_MODULE   // add a common module for demo

/*******************************************************************************
 *                               USER CONTEXT
 ******************************************************************************/

// user module config params in flash
typedef struct _user_config_t {
  // dev_info
  char dev_name[MAX_DEVICE_NAME_SIZE+1];
  uint32_t dev_name_len; 
  
  // rgb led
  bool rgb_led_sw;
  int rgb_led_hues;
  int rgb_led_saturation;
  int rgb_led_brightness;
  
  // DC Motor
  int  dc_motor_switch;   // 0: off;  others: on, for later use
}user_config_t;

// user module status
typedef struct _user_status_t {
//  bool user_config_need_update;               // if set, user context config need to write back to flash.
  int32_t oled_keep_s;   // oled keep current display info time(s)
  
  //  light sensor (ADC1_4)
  int light_sensor_data;
  
  // infrared reflective sensor
  int infrared_reflective_data;
  
  // temperature && humidity
  int temperature;
  int humidity_saved;   // save humidity value when get temperature, because we can not read DHT11 within 1 second.
  int humidity;         // real humidity prop value.
  
  // uart
  char uart_rx_buf[MAX_USER_UART_BUF_SIZE];   // use a buffer to store data received
  uint32_t uart_rx_data_len;                  // uart data len received

#ifdef USE_COMMON_MODULE  
  // common module values(support 3 values)
  int common_module_value1;
  int common_module_value2;
  int common_module_value3;
#endif
}user_status_t;

// user context
typedef struct _user_context_t {
  user_config_t config;                       // config params in flash
  mico_mutex_t config_mutex;                  // mutex for write flash
  
  user_status_t status;                       // running status
}user_context_t;

#endif  // __MICO_DEVICE_PROPERTIES_USER_H_
