
#ifndef _ALINK_VENDOR_MICO_H_
#define _ALINK_VENDOR_MICO_H_

#include "alink_export.h"

#define FIRMWARE_REVISION       "ALINK_AOS_5088_WH_E6@"
#define FIRMWARE_REVISION_NUM   1

//#define DEGUB_ON_UART6
//#define SANDBOX
///* device info */
#define DEV_NAME "EQ810T50"
#define DEV_MODEL "AOSMITH_LIVING_ELECTRICWATERHEATER_EQ810T50"
#define DEV_MANUFACTURE "AOSMITH"
#ifdef SANDBOX
  #define ALINK_KEY "llMic4nYygKku8HdENb8"
  #define ALINK_SECRET "agP6KVGKaK7dNJRfd2ZWTmdsr9oAl81wNbBLeVjm"
#else
  #define ALINK_KEY "CF2ABGLICzabesrwirOp"
  #define ALINK_SECRET "2UNUFuc0FNs3nqsX2KRNFNETVHWsgikwN62GVONt"
#endif

#define DEV_TYPE "ELECTRICWATERHEATER"
#define DEV_CATEGORY "LIVING"
#define DEV_SN "12345678"

#define DEV_VERSION "1.0.0"

#define ALINK_VER "1.0"
#define ALINK_RPC "2.0"
#define ALINK_LANG "en"

int mico_start_alink(void);


#endif


