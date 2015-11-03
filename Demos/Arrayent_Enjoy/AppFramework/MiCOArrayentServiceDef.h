/**
******************************************************************************
* @file    MiCOArrayentServiceDef.h 
* @author  Eshen Wang
* @version V1.0.0
* @date    26-Aug-2015
* @brief   This header contains the defines of arrayent service. 
  operation
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

#ifndef __MICO_ARRAYENT_DEF_H_
#define __MICO_ARRAYENT_DEF_H_


/*******************************************************************************
 *                                DEFINES
 ******************************************************************************/
#define ACA_MAX_SIZE_DEVICE_NAME        32
#define ACA_MAX_SIZE_DEVICE_PASSWD      16

#define ACA_MAX_SIZE_PRODUCT_AES_KEY    64
#define ACA_MAX_SIZE_DEVICE_AES_KEY     64
    
#define ACA_STACK_SIZE_ARRAYENT_MAIN_THREAD    0x400  

// Device credentials and server addresses
#define PPRODUCT_ID  		       1101
#define DEVICE_NAME                    "AXA00000706"
#define DEVICE_PASSWORD                "7B"

#define LOAD_BALANCER1 		       "axlp01lb01.cn.pub.arrayent.com"
#define LOAD_BALANCER2 		       "axlp01lb02.cn.pub.arrayent.com"
#define LOAD_BALANCER3 		       "axlp01lb03.cn.pub.arrayent.com"

#define DEVICE_AES_KEY                 "6C794338547CD09531B5D1B18EE3C54D"
#define PRODUCT_AES_KEY                "7EFF77215F8BFB963AD45D571D2BF8BC"

#define ACA_LOAD_BALANCER_UDP_PORT      80
#define ACA_LOAD_BALANCER_TCP_PORT      80
#define ACA_THREAD_PRIORITY             7
#define ACA_STACK_SIZE                  2048

/*******************************************************************************
 *                               STRUCTURES
 ******************************************************************************/

/* device configurations stored in flash */
typedef struct _arrayent_config_t
{
  uint32_t          productId;                                 // product id
  char              product_aes_key[ACA_MAX_SIZE_PRODUCT_AES_KEY];
    
  char              deviceId[ACA_MAX_SIZE_DEVICE_NAME];        // device name
  char              devicePasswd[ACA_MAX_SIZE_DEVICE_PASSWD];  // device password
  char              device_aes_key[ACA_MAX_SIZE_DEVICE_AES_KEY];
} arrayentcloud_config_t;

/* device status */
typedef struct _arrayent_status_t
{
  bool              isCloudConnected;   // cloud service connect status
} arrayentcloud_status_t;

typedef struct _arrayent_context_t {
  arrayentcloud_config_t config_info;        // arrayent config info
  arrayentcloud_status_t status;             // arrayent running status
} arrayent_context_t;

#endif  // __MICO_ARRAYENT_DEF_H_
