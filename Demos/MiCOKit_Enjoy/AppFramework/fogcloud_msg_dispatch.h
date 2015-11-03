/**
  ******************************************************************************
  * @file    msg_dispatch.h
  * @author  Eshen Wang
  * @version V1.0.0
  * @date    18-Mar-2015
  * @brief   fogclud msg dispatch.
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

#ifndef __MICO_FOGCLOUD_MSG_DISPATCH_H_
#define __MICO_FOGCLOUD_MSG_DISPATCH_H_

#include "mico.h"
#include "MiCOAppDefine.h"
#include "properties.h"


/*******************************************************************************
 *                                DEFINES
 ******************************************************************************/
// recv topic
#define FOGCLOUD_MSG_TOPIC_IN            "/in"
#define FOGCLOUD_MSG_TOPIC_OUT           "/out"

#define FOGCLOUD_MSG_TOPIC_IN_READ       "/read"
#define FOGCLOUD_MSG_TOPIC_IN_WRITE      "/write"

#define FOGCLOUD_MSG_TOPIC_OUT_NOTIFY    "read"
#define FOGCLOUD_MSG_TOPIC_OUT_ERROR     "err"

// msg handle result
#define MSG_PROP_UNPROCESSED             0
#define MSG_PROP_READ                    1
#define MSG_PROP_WROTE                   2

/*******************************************************************************
 *                                STRUCTURES
 ******************************************************************************/
// msg data
typedef struct _mico_fogcloud_msg_t{
  const char *topic;
  unsigned int topic_len;
  unsigned char *data;
  unsigned int data_len;
}mico_fogcloud_msg_t;


/*******************************************************************************
 *                                FUNCTIONS
 ******************************************************************************/
// handle cloud msg here, for example: send to USART or echo to cloud
OSStatus mico_fogcloud_msg_dispatch(app_context_t* context, struct mico_service_t service_table[],
                                    mico_fogcloud_msg_t *cloud_msg, int* ret_status);

// property notify
OSStatus mico_start_properties_notify(app_context_t * const inContext, struct mico_service_t service_table[],
                                      uint32_t period_ms, uint32_t statck_size);

#endif // __MICO_FOGCLOUD_MSG_DISPATCH_H_
