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

#ifndef __MICO_SITEWHERE_H_
#define __MICO_SITEWHERE_H_

#include "mico.h"
#include "MiCOAppDefine.h"


/*******************************************************************************
 *                                DEFINES
 ******************************************************************************/


/*******************************************************************************
 *                            USER INTERFACES
 ******************************************************************************/

/*----------------------------------- init -----------------------------------*/
// init FogCloud
OSStatus MiCOStartSiteWhereService(app_context_t* const inContext);

#endif  // __MICO_SITEWHERE_H_
