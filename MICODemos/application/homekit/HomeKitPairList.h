/**
  ******************************************************************************
  * @file    HomeKitPairList.h 
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
#pragma once

#include "Common.h"
#include "platform.h"
#include "platform_config.h"

#define MaxControllerNameLen  64
#define MaxPairRecord         16

/*Pair Info flash content*/
typedef struct _pair_t {
  char             controllerName[MaxControllerNameLen];
  uint8_t          controllerLTPK[32];
  int              permission;
} _pair_t;

typedef struct _pair_list_in_flash_t {
  _pair_t          pairInfo[MaxPairRecord];
} pair_list_in_flash_t;

/* Malloc a memory and */
//OSStatus HKReadPairList(pair_list_in_flash_t **pPairList);

uint32_t HKPairInfoCount(void);

OSStatus HKPairInfoClear(void);

OSStatus HKPairInfoInsert(char controllerIdentifier[64], uint8_t controllerLTPK[32], bool admin);

OSStatus HKPairInfoFindByName(char controllerIdentifier[64], uint8_t foundControllerLTPK[32], bool *isAdmin );

OSStatus HKPairInfoFindByIndex(uint32_t index, char controllerIdentifier[64], uint8_t foundControllerLTPK[32], bool *isAdmin );

OSStatus HKPairInfoRemove(char controllerIdentifier[64]);


