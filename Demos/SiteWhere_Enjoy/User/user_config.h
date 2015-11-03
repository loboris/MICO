/**
  ******************************************************************************
  * @file    user_config.h 
  * @author  Eshen Wang
  * @version V1.0.0
  * @date    14-May2015
  * @brief   User configuration header file.
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

#ifndef __USER_CONFIG_H_
#define __USER_CONFIG_H_

#include "mico_config.h"

/*******************************************************************************
 *                             APP INFO
 ******************************************************************************/


/*******************************************************************************
 *                             CONNECTION
 ******************************************************************************/

/* MICO cloud service */
#define MICO_CLOUD_TYPE               CLOUD_SITEWHERE

/* Firmware update check
 * If need firmware update function, comment this out
 */
#define DISABLE_FOGCLOUD_OTA_CHECK


/*******************************************************************************
 *                             RESOURCES
 ******************************************************************************/

/* stack size of the user_main thread */
#define STACK_SIZE_USER_MAIN_THREAD    0x800

/* User provided configurations seed
 * If user configuration(params in flash) is changed, update this number to
 * indicate the bootloader to clean params in flash next time restart.
 */
#define CONFIGURATION_VERSION          0x00000002


/*******************************************************************************
 *                               USER CONTEXT
 ******************************************************************************/
#define MAX_DEVICE_NAME_SIZE            16
#define MAX_USER_UART_BUF_SIZE          32 // for OLED display
 
// user module config params
typedef struct _user_config_t {
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
  int32_t oled_keep_s;   // oled keep current display info time(s)
  
  //  light sensor (ADC1_4)
  int light_sensor_data;
  // infrared reflective sensor
  int infrared_reflective_data;
  
  // temperature && humidity
  int temperature;
  int humidity;         // real humidity prop value.
}user_status_t;

// user context
typedef struct _user_context_t {
  user_config_t config;                       // config params in flash
  
  user_status_t status;                       // running status
}user_context_t;

#endif  // __USER_CONFIG_H_

