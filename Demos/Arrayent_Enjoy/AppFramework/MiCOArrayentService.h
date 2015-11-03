/**
******************************************************************************
* @file    MiCOArrayentService.h 
* @author  Eshen Wang
* @version V1.0.0
* @date    26-Aug-2015
* @brief   This header contains the arrayent service interfaces for MiCO.
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

#ifndef __MICO_ARRYENT_H_
#define __MICO_ARRYENT_H_

#include "MiCO.h"
#include "MiCOAppDefine.h"
#include "MiCOArrayentServiceDef.h"
#include "aca.h"


/*******************************************************************************
 *                                DEFINES
 ******************************************************************************/


/*******************************************************************************
 *                            USER INTERFACES
 ******************************************************************************/

/*----------------------------------- init -----------------------------------*/
// init Arrayent
OSStatus MiCOStartArrayentService(app_context_t* const inContext);
// service sleep, can be re-configure
OSStatus MiCOSuspendArrayentService(app_context_t* const inContext, arrayent_sleep_level_e level);
// service wake up
OSStatus MiCOResumeArrayentService(app_context_t* const inContext);

// restore default configurations for Arrayent
void MiCOArrayentRestoreDefault(arrayentcloud_config_t* const arrayent_config);

/*--------------------------- send && recv porperties ---------------------------*/
// Module <=> Cloud
OSStatus MiCOArrayentPropSend(app_context_t* const context, char* property, char* value);
OSStatus MiCOArrayentPropRecv(app_context_t* const context, char* data, uint16_t* len, uint32_t timeout);

/*------------------------------ device control ------------------------------*/
//OTA
//OSStatus MiCOArrayentFirmwareUpdate(app_context_t* const context,
//                                    MVDOTARequestData_t OTAData);

#endif  // __MICO_ARRYENT_H_
