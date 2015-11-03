/**
  ******************************************************************************
  * @file    user_config.h 
  * @author  Eshen Wang
  * @version V1.0.0
  * @date    14-May2015
  * @brief   User configuration file.
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

/*------------------------------ product -------------------------------------*/
#ifdef MICOKIT_3288
// wes' product, replace it with your own product
  #define PRODUCT_ID                   "d3dc7a21"
  #define PRODUCT_KEY                  "5d26608d-4b79-460d-b8a7-3fc5b105be3b"
#elif  MICOKIT_3165
  #define PRODUCT_ID                   "57f737bd"
  #define PRODUCT_KEY                  "e0b0939d-35c3-42f3-9b83-79deb4f5d8be"
#elif MICOKIT_G55
  #define PRODUCT_ID                   "a95f5997"
  #define PRODUCT_KEY                  "f4a35f56-0265-45c9-8ebd-9078fb1f477b"
#elif MICOKIT_LPC5410X
  #define PRODUCT_ID                   "1c7290f8"
  #define PRODUCT_KEY                  "5aee31e9-8fd3-4389-8644-42be6332f4af"
#else

#endif


/*******************************************************************************
 *                             CONNECTING
 ******************************************************************************/

/* MICO cloud service */
#define MICO_CLOUD_TYPE               CLOUD_FOGCLOUD

/* Firmware update check
 * If not need to check new firmware on server after wifi on, comment out this macro
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
#define CONFIGURATION_VERSION          0x00000001


#endif  // __USER_CONFIG_H_

