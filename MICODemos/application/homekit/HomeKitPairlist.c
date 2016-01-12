/**
  ******************************************************************************
  * @file    HomeKitPairList.c 
  * @author  William Xu
  * @version V1.0.0
  * @date    05-May-2014
  * @brief   This file provide operations on nonvolatile memory.
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

#include "mico.h"
#include "mico_app_define.h"
#include "HomeKit.h"
#include "HomeKitPairlist.h"

extern app_context_t* app_context;

uint32_t HKPairInfoCount(void)
{
  pair_list_in_flash_t        *pairList = &app_context->appConfig->pairList;
  uint32_t i, count = 0;
  
  /* Looking for controller pair record */
  for(i=0; i < MaxPairRecord; i++){
    if(pairList->pairInfo[i].controllerName[0] != 0x0)
      count++;
  }
  return count;
}

OSStatus HKPairInfoInsert(char controllerIdentifier[64], uint8_t controllerLTPK[32], bool admin)
{
  OSStatus err = kNoErr;
  pair_list_in_flash_t        *pairList = &app_context->appConfig->pairList;
  uint32_t i;
  
  /* Looking for controller pair record */
  for(i=0; i < MaxPairRecord; i++){
    if(strncmp(pairList->pairInfo[i].controllerName, controllerIdentifier, 64)==0)
      break;
  }

  /* This is a new record, find a empty slot */
  if(i == MaxPairRecord){
    for(i=0; i < MaxPairRecord; i++){
      if(pairList->pairInfo[i].controllerName[0] == 0x0)
      break;
    }
  }
  
  /* No empty slot for new record */
  require_action(i < MaxPairRecord, exit, err = kNoSpaceErr);

  /* Write pair info to flash */
  strcpy(pairList->pairInfo[i].controllerName, controllerIdentifier);
  memcpy(pairList->pairInfo[i].controllerLTPK, controllerLTPK, 32);
  if(admin)
    pairList->pairInfo[i].permission = pairList->pairInfo[i].permission|0x00000001;
  else
    pairList->pairInfo[i].permission = pairList->pairInfo[i].permission&0xFFFFFFFE;

  mico_system_context_update( mico_system_context_get() );

exit: 
  return err;
}

OSStatus HKPairInfoFindByName(char controllerIdentifier[64], uint8_t foundControllerLTPK[32], bool *isAdmin )
{
  OSStatus err = kNotFoundErr;
  uint32_t i;

  pair_list_in_flash_t *pairList = &app_context->appConfig->pairList;

  for(i=0; i < MaxPairRecord; i++){
    if(strncmp(pairList->pairInfo[i].controllerName, controllerIdentifier, 64) == 0){
      if( foundControllerLTPK != NULL)
        memcpy(foundControllerLTPK, pairList->pairInfo[i].controllerLTPK, 32);
      if( isAdmin != NULL )
        *isAdmin = pairList->pairInfo[i].permission&0x1;
      err = kNoErr;
      break;
    }
  }
  return err;
}

OSStatus HKPairInfoFindByIndex(uint32_t index, char controllerIdentifier[64], uint8_t foundControllerLTPK[32], bool *isAdmin )
{
  OSStatus err = kNoErr;
  uint32_t i, count;

  pair_list_in_flash_t *pairList = &app_context->appConfig->pairList;

  /* Find a record by index, should skip empty record */
  for(i = 0, count = 0; i < MaxPairRecord; i++){
    if(pairList->pairInfo[i].controllerName[0] != 0x0){
      if( count == index )
        break;
      else
        count++;
    }
  }

  require_action(i < MaxPairRecord, exit, err = kNotFoundErr);

  if( controllerIdentifier != NULL)
    strncpy(controllerIdentifier, pairList->pairInfo[i].controllerName, 64);  
  if( foundControllerLTPK != NULL)
    memcpy(foundControllerLTPK, pairList->pairInfo[i].controllerLTPK, 32);
  if( isAdmin != NULL )
    *isAdmin = pairList->pairInfo[i].permission&0x1;


exit:
  return err;
}

OSStatus HKPairInfoRemove(char * name)
{
  uint32_t i;
  OSStatus err = kNotFoundErr;

  pair_list_in_flash_t *pairList = &app_context->appConfig->pairList;

  for(i=0; i < MaxPairRecord; i++){
    if(strncmp(pairList->pairInfo[i].controllerName, name, 64) == 0){
      pairList->pairInfo[i].controllerName[0] = 0x0; //Clear the controller name record
      err = mico_system_context_update( mico_system_context_get() );
      break;
    }
  }

  return err;
}

