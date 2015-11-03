/**
  ******************************************************************************
  * @file    MICOAppDefine.h 
  * @author  William Xu
  * @version V1.0.0
  * @date    05-May-2014
  * @brief   This file create a TCP listener thread, accept every TCP client
  *          connection and create thread for them.
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

#ifndef __MICOAPPDEFINE_H
#define __MICOAPPDEFINE_H

#include "mico.h"
#include "user_config.h"
#include "MiCOFogCloudDef.h"


/*******************************************************************************
 *                            DEFAULT SETTING
 ******************************************************************************/
    
#ifndef APP_INFO
  #define APP_INFO            "MiCOKit Demo based on MICO OS"
#endif

#ifndef FIRMWARE_REVISION
  #define FIRMWARE_REVISION   "MiCOKit_1_0"
#endif

#ifndef MANUFACTURER
  #define MANUFACTURER        "MXCHIP Inc."
#endif

#ifndef SERIAL_NUMBER
  #define SERIAL_NUMBER       "20150530"
#endif

#ifndef PROTOCOL
  #define PROTOCOL            "com.mxchip.micokit"
#endif

/* Wi-Fi configuration mode */
#ifndef MICO_CONFIG_MODE
  #define MICO_CONFIG_MODE     CONFIG_MODE_EASYLINK_WITH_SOFTAP
#endif

/* Define MICO cloud type */
#define CLOUD_DISABLED       (0)
#define CLOUD_FOGCLOUD       (1)
#define CLOUD_ALINK          (2)
    
/* MICO cloud service type */
#ifndef MICO_CLOUD_TYPE
  #define MICO_CLOUD_TYPE    CLOUD_FOGCLOUD
#endif

#ifndef STACK_SIZE_USER_MAIN_THREAD
  #define STACK_SIZE_USER_MAIN_THREAD    0x800
#endif

/*User provided configurations*/
#ifndef CONFIGURATION_VERSION
  #define CONFIGURATION_VERSION          0x00000001 // if default configuration is changed, update this number
#endif

#ifndef BONJOUR_SERVICE_PORT
  #define BONJOUR_SERVICE_PORT           8080
#endif

#ifndef BONJOUR_SERVICE
  #define BONJOUR_SERVICE                "_easylink._tcp.local."
#endif

/* product id/key check */
#ifndef PRODUCT_ID
  #error  "PRODUCT_ID must be set in 'user_config.h'."
#endif

#ifndef PRODUCT_KEY
  #error "PRODUCT_KEY must be set in 'user_config.h'."
#endif

#ifndef DEFAULT_ROM_VERSION
  #error  "DEFAULT_ROM_VERSION must be set in 'user_config.h'."
#endif

#ifndef DEFAULT_DEVICE_NAME
  #error  "DEFAULT_DEVICE_NAME must be set in 'user_config.h'."
#endif


/*******************************************************************************
 *                              APP CONTEXT
 ******************************************************************************/
    
/* Application's configuration stores in flash */
typedef struct
{
  uint32_t          configDataVer;        // config param update number
  uint32_t          bonjourServicePort;   // for bonjour service port

  fogcloud_config_t fogcloudConfig;       // fogcloud settings
} application_config_t;

/* Running status */
typedef struct _current_app_status_t {
  volatile bool     noOTACheckOnSystemStart;  // indacate for not OTA check after configed
  volatile bool     isWifiConnected;      // wifi station connect status
  fogcloud_status_t fogcloudStatus;       // fogcloud status
} current_app_status_t;

typedef struct _app_context_t
{
  /*Flash content*/
  application_config_t*     appConfig;

  /*Running status*/
  current_app_status_t      appStatus;
  
  /*MICO context*/
   mico_Context_t*          mico_context;
} app_context_t;

#endif  // __MICOAPPDEFINE_H
