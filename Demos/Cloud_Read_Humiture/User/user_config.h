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
// wes' product
  #define PRODUCT_ID                   "df7a9706"
  #define PRODUCT_KEY                  "9c748c29-e60c-4227-8d5e-243aa78fae58"
#elif  MICOKIT_3165
  #define PRODUCT_ID                   "5b8820c0"
  #define PRODUCT_KEY                  "333865e4-6859-48d4-acc0-29fb5de0fc02"
#elif MICOKIT_G55
  #define PRODUCT_ID                   "04e026f2"
  #define PRODUCT_KEY                  "efd5c3d1-f43b-4d6e-b6c3-19a84b83d01d"
#elif MICOKIT_LPC5410X
  #define PRODUCT_ID                   "a49a5cb8"
  #define PRODUCT_KEY                  "9b8e0cf8-da63-47fa-9144-ac2d65106b63"
#else

#endif

/*******************************************************************************
 *                             FOGCLOUD
 ******************************************************************************/

/* MICO cloud service */
#define MICO_CLOUD_TYPE               CLOUD_FOGCLOUD

/* Firmware update check
 * If need to check new firmware on server after wifi on, comment out this macro
 */
#define DISABLE_FOGCLOUD_OTA_CHECK


/*******************************************************************************
 *                             USER RESOURCES
 ******************************************************************************/

/* stack size of the user_main thread */
#define STACK_SIZE_USER_MAIN_THREAD    0x800

/* User provided configurations seed
 * If user configuration(params in flash) is changed, update this number to
 * indicate the bootloader to clean params in flash next time restart.
 */
#define CONFIGURATION_VERSION          0x00000002


#endif  // __USER_CONFIG_H_

