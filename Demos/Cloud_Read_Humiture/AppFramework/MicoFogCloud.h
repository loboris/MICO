/**
******************************************************************************
* @file    MiCOFogCloud.h 
* @author  Eshen Wang
* @version V1.0.0
* @date    17-Mar-2015
* @brief   This header contains the cloud service interfaces 
*          for MICO. 
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

#ifndef __MICO_FOGCLOUD_H_
#define __MICO_FOGCLOUD_H_

#include "mico.h"
#include "MiCOAppDefine.h"
#include "MiCOFogCloudDef.h"
#include "FogCloudUtils.h"


/*******************************************************************************
 *                                DEFINES
 ******************************************************************************/


/*******************************************************************************
 *                            USER INTERFACES
 ******************************************************************************/

/*----------------------------------- init -----------------------------------*/
// init FogCloud
OSStatus MiCOStartFogCloudService(app_context_t* const inContext);
// restore default config for FogCloud
void MiCOFogCloudRestoreDefault(fogcloud_config_t* const fog_config);


/*-------------------------- get MiCOFogCloud state --------------------------*/
// device activate state
bool MiCOFogCloudIsActivated(app_context_t* const context);
// cloud connect state
bool MiCOFogCloudIsConnect(app_context_t* const context);


/*--------------------------- send && recv message ---------------------------*/
// Module <=> Cloud
OSStatus MiCOFogCloudMsgSend(app_context_t* const context, const char* topic,
                             unsigned char *inBuf, unsigned int inBufLen);

OSStatus MiCOFogCloudMsgRecv(app_context_t* const context, fogcloud_msg_t **msg, 
                             uint32_t timeout_ms);

/*------------------------------ device control ------------------------------*/
//activate
OSStatus MiCOFogCloudActivate(app_context_t* const context, 
                              MVDActivateRequestData_t activateData);
//authorize
OSStatus MiCOFogCloudAuthorize(app_context_t* const context,
                               MVDAuthorizeRequestData_t authorizeData);
//reset device info on cloud
OSStatus MiCOFogCloudResetCloudDevInfo(app_context_t* const context,
                                       MVDResetRequestData_t devResetData);
// just set need cloud reset flag, device will reset itself from cloud.
void MiCOFogCloudNeedResetDevice(void);

//OTA
OSStatus MiCOFogCloudFirmwareUpdate(app_context_t* const context,
                                    MVDOTARequestData_t OTAData);
//get device state info(activate/connect)
OSStatus MiCOFogCloudGetState(app_context_t* const context,
                              MVDGetStateRequestData_t getStateRequestData,
                              void* outDevState);

#endif  // __MICO_FOGCLOUD_H_
