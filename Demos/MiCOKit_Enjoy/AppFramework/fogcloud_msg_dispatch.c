/**
******************************************************************************
* @file    fogcloud_msg_dispatch.c 
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

#include "mico.h"
#include "json_c/json.h"
#include "properties.h"
#include "MicoFogCloud.h"
#include "fogcloud_msg_dispatch.h"

#define msg_dispatch_log(M, ...) custom_log("MSG_DISPATCH", M, ##__VA_ARGS__)
#define msg_dispatch_log_trace() custom_log_trace("MSG_DISPATCH")


// notify data
typedef struct _mico_notify_thread_data_t{
  app_context_t* context;
  struct mico_service_t* p_service_table;
  uint32_t notify_interval;
}mico_notify_thread_data_t;

static mico_notify_thread_data_t g_notify_thread_data;


// handle cloud msg here, for example: send to USART or echo to cloud
OSStatus mico_fogcloud_msg_dispatch(app_context_t* context, struct mico_service_t  service_table[],
                                mico_fogcloud_msg_t *cloud_msg, int* ret_status)
{
  msg_dispatch_log_trace();
  OSStatus err = kNoErr;
  
  char* recv_sub_topic_ptr = NULL;
  int recv_sub_topic_len = 0;  
  json_object *recv_json_object = NULL;
  char* response_sub_topic = NULL;
  json_object *response_json_obj = NULL;
  const char *response_json_string = NULL;
  
  json_object *response_services_obj = NULL;
  json_object *response_prop_obj = NULL;
  json_object *response_err_obj = NULL;
  
  *ret_status = MSG_PROP_UNPROCESSED;
  
  if((NULL == context) || (NULL == cloud_msg->topic) || (0 == cloud_msg->topic_len) ) {
    return kParamErr;
  }
  
  // strip "<device_id>/in" to get response sub-topic, then response to:  "<device_id>/out/<sub-topic>"
  recv_sub_topic_len = (int)cloud_msg->topic_len - (strlen(context->appConfig->fogcloudConfig.deviceId) + strlen(FOGCLOUD_MSG_TOPIC_IN));
  if(recv_sub_topic_len <= 0){  // unsupported topic
    msg_dispatch_log("ERROR: Message from unsupported topic: %.*s \t data[%d]: %s, ignored.", 
                     cloud_msg->topic_len, cloud_msg->topic,
                     cloud_msg->data_len, cloud_msg->data);
    err = kUnsupportedErr;
    goto exit;
  }
  recv_sub_topic_ptr = (char*)(cloud_msg->topic) + strlen(context->appConfig->fogcloudConfig.deviceId) + strlen(FOGCLOUD_MSG_TOPIC_IN);
  
  response_sub_topic = (char*)malloc(recv_sub_topic_len);   // response to where msg come from, remove leading '/'
  if(NULL == response_sub_topic){
    msg_dispatch_log("malloc reponse topic memory err!");
    err = kNoMemoryErr;
    goto exit;
  }
  memset(response_sub_topic, '\0', recv_sub_topic_len);
  strncpy(response_sub_topic, recv_sub_topic_ptr + 1, recv_sub_topic_len-1);  // remove leading '/' as send sub-topic
  //msg_dispatch_log("recv_sub_topic[%d]=[%.*s]", recv_sub_topic_len, recv_sub_topic_len, recv_sub_topic_ptr);  
  //msg_dispatch_log("response_sub_topic[%d]=[%s]", strlen(response_sub_topic), response_sub_topic);  
  
  // parse sub topic string
  if( 0 == strncmp((char*)FOGCLOUD_MSG_TOPIC_IN_READ, recv_sub_topic_ptr, strlen((char*)FOGCLOUD_MSG_TOPIC_IN_READ)) ){
    // from /read
    msg_dispatch_log("Recv read cmd: %.*s, data[%d]: %s",
                     recv_sub_topic_len, recv_sub_topic_ptr, cloud_msg->data_len, cloud_msg->data);
   
    // parse input json data
    recv_json_object = json_tokener_parse((const char*)(cloud_msg->data));
    if (NULL == recv_json_object){  // input json format error, response to sub-topic "err" with error code
      response_err_obj = json_object_new_object();
      require( response_err_obj, exit );
      json_object_object_add(response_err_obj, MICO_PROP_KEY_RESP_STATUS, json_object_new_int(MICO_PROP_CODE_DATA_FORMAT_ERR));
      
      response_json_string = json_object_to_json_string(response_err_obj);
      err = MiCOFogCloudMsgSend(context, FOGCLOUD_MSG_TOPIC_OUT_ERROR, 
                                (unsigned char*)response_json_string, strlen(response_json_string));
      goto exit;
    }
    //msg_dispatch_log("Recv read object=%s", json_object_to_json_string(recv_json_object));
        
    // read properties, return "services/read/err" sub-obj in response_json_obj
    response_json_obj = mico_read_properties(service_table, recv_json_object);
    
    // send reponse for read data
    if(NULL == response_json_obj){  // read failed
      msg_dispatch_log("ERROR: read properties error!");
      json_object_object_add(response_err_obj, MICO_PROP_KEY_RESP_STATUS, json_object_new_int(MICO_PROP_CODE_READ_FAILED));
      response_json_string = json_object_to_json_string(response_err_obj);
      err = MiCOFogCloudMsgSend(context, FOGCLOUD_MSG_TOPIC_OUT_ERROR, 
                                  (unsigned char*)response_json_string, strlen(response_json_string));
    }
    else {
      // send err code
      response_err_obj = json_object_object_get(response_json_obj, MICO_PROP_KEY_RESP_ERROR);
      if( (NULL !=  response_err_obj) && (NULL != json_object_get_object(response_err_obj)->head) ){
        response_json_string = json_object_to_json_string(response_err_obj);
        err = MiCOFogCloudMsgSend(context, FOGCLOUD_MSG_TOPIC_OUT_ERROR, 
                                  (unsigned char*)response_json_string, strlen(response_json_string));
      }
      
      // send services info msg
      response_services_obj = json_object_object_get(response_json_obj, MICO_PROP_KEY_RESP_SERVICES);
      if(NULL != response_services_obj){
        json_object_object_del(response_json_obj, MICO_PROP_KEY_RESP_ERROR);  // remove "err" sub-obj
        response_json_string = json_object_to_json_string(response_json_obj);
        err = MiCOFogCloudMsgSend(context, response_sub_topic, 
                                  (unsigned char*)response_json_string, strlen(response_json_string));
      }
      
      // send read ok value
      response_prop_obj = json_object_object_get(response_json_obj, MICO_PROP_KEY_RESP_READ);
      if( (NULL !=  response_prop_obj) && (NULL != json_object_get_object(response_prop_obj)->head) ){
        response_json_string = json_object_to_json_string(response_prop_obj);
        err = MiCOFogCloudMsgSend(context, response_sub_topic, 
                                  (unsigned char*)response_json_string, strlen(response_json_string));
      }
      
      // return process result
      *ret_status = MSG_PROP_READ;
    } 
  }
  else if( 0 == strncmp((char*)FOGCLOUD_MSG_TOPIC_IN_WRITE, recv_sub_topic_ptr, strlen((char*)FOGCLOUD_MSG_TOPIC_IN_WRITE)) ){
    // from /write
    msg_dispatch_log("Recv write cmd: %.*s, data[%d]: %s",
                     recv_sub_topic_len, recv_sub_topic_ptr,
                     cloud_msg->data_len, cloud_msg->data);
    
    // parse input json data
    recv_json_object = json_tokener_parse((const char*)(cloud_msg->data));
    if (NULL == recv_json_object){  // input json format error, response to sub-topic "err" with error code
      response_err_obj = json_object_new_object();
      require( response_err_obj, exit );
      json_object_object_add(response_err_obj, MICO_PROP_KEY_RESP_STATUS, json_object_new_int(MICO_PROP_CODE_DATA_FORMAT_ERR));
      
      response_json_string = json_object_to_json_string(response_err_obj);
      err = MiCOFogCloudMsgSend(context, FOGCLOUD_MSG_TOPIC_OUT_ERROR, 
                                (unsigned char*)response_json_string, strlen(response_json_string));
      goto exit;
    }
    //msg_dispatch_log("Recv write object=%s", json_object_to_json_string(recv_json_object));
        
    // write properties, return "write/err" sub-obj in response_json_obj
    response_json_obj = mico_write_properties(service_table, recv_json_object);
    
    // send reponse for write data
    if(NULL == response_json_obj){  // write failed
      msg_dispatch_log("ERROR: write properties error!");
      json_object_object_add(response_err_obj, MICO_PROP_KEY_RESP_STATUS, json_object_new_int(MICO_PROP_CODE_WRITE_FAILED));
      response_json_string = json_object_to_json_string(response_err_obj);
      err = MiCOFogCloudMsgSend(context, FOGCLOUD_MSG_TOPIC_OUT_ERROR, 
                                  (unsigned char*)response_json_string, strlen(response_json_string));
    }
    else {
      // send err code
      response_err_obj = json_object_object_get(response_json_obj, MICO_PROP_KEY_RESP_ERROR);
      if( (NULL !=  response_err_obj) && (NULL != json_object_get_object(response_err_obj)->head) ){
        response_json_string = json_object_to_json_string(response_err_obj);
        err = MiCOFogCloudMsgSend(context, FOGCLOUD_MSG_TOPIC_OUT_ERROR, 
                                  (unsigned char*)response_json_string, strlen(response_json_string));
      }
      // send write ok value
      response_prop_obj = json_object_object_get(response_json_obj, MICO_PROP_KEY_RESP_WRITE);
      if( (NULL !=  response_prop_obj) && (NULL != json_object_get_object(response_prop_obj)->head) ){
        response_json_string = json_object_to_json_string(response_prop_obj);
        err = MiCOFogCloudMsgSend(context, response_sub_topic, 
                                  (unsigned char*)response_json_string, strlen(response_json_string));
      }
      
      // return process result
      *ret_status = MSG_PROP_WROTE;
    }
  }
  else{
    // unknown topic, ignore msg
    err = kUnsupportedErr;
    msg_dispatch_log("ERROR: Message from unknown topic: %.*s \t data[%d]: %s, ignored.", 
                     recv_sub_topic_len, cloud_msg->topic,
                     cloud_msg->data_len, cloud_msg->data);
  }
  
exit:
  if(NULL != recv_json_object){
    json_object_put(recv_json_object);
    recv_json_object = NULL;
  }
  if(NULL != response_json_obj){
    json_object_put(response_json_obj);
    response_json_obj = NULL;
  }
  if(NULL != response_sub_topic){
    free(response_sub_topic);
    response_sub_topic = NULL;
  }
  return err;
}


OSStatus  _properties_notify(app_context_t * const inContext, struct mico_service_t service_table[])
{
  OSStatus err = kUnknownErr;
  json_object *notify_obj = NULL;
  const char *notify_json_string = NULL;
  
  require_action(inContext, exit, err = kParamErr);
  
  notify_obj = json_object_new_object();
  require_action(notify_obj, exit, err = kParamErr);
  
  // properties update check
  err = mico_properties_notify_check(inContext, service_table, notify_obj);
  require_noerr(err, exit);
  
  // send notify message to cloud
  if( NULL != (json_object_get_object(notify_obj)->head) ){
    notify_json_string = json_object_to_json_string(notify_obj);
    // notify to topic: <device_id>/out/read
    err = MiCOFogCloudMsgSend(inContext, FOGCLOUD_MSG_TOPIC_OUT_NOTIFY, 
                              (unsigned char*)notify_json_string, strlen(notify_json_string));
  }
  else{
    // no update msg
    err = kNoErr;
  }
  
exit:
  if(kNoErr != err){
    msg_dispatch_log("ERROR: _properties_notify error, err = %d", err);
  }
  if(NULL != notify_obj){
    json_object_put(notify_obj);
    notify_obj = NULL;
  }
  return err;
}

// properties notify task
void notify_thread(void* arg)
{
  OSStatus err = kUnknownErr;
  mico_notify_thread_data_t *p_notify_thread_data;
  
  p_notify_thread_data = (mico_notify_thread_data_t*)arg;
  require_action(p_notify_thread_data, exit, err = kParamErr);
  
   // wait semaphore for cloud connection
  //mico_fogcloud_waitfor_connect(p_notify_thread_data->context, MICO_WAIT_FOREVER);  // block to wait fogcloud connect
  //msg_dispatch_log("Cloud connected, do notify task.");
  
  while(1){
    if(p_notify_thread_data->context->appStatus.fogcloudStatus.isCloudConnected){
      err = _properties_notify(p_notify_thread_data->context, p_notify_thread_data->p_service_table);
      if(kNoErr != err){
        msg_dispatch_log("ERROR: properties notify failed! err = %d", err);
      }
    }
    
    mico_thread_msleep(p_notify_thread_data->notify_interval);
  }
  
exit:  
  // never get here only if notify work err && exit.
  msg_dispatch_log("ERROR: notify thread exit err=%d.", err);
  mico_rtos_delete_thread(NULL);
  return;
}

OSStatus mico_start_properties_notify(app_context_t * const inContext, struct mico_service_t service_table[],
                                      uint32_t period_ms, uint32_t stack_size)
{
  msg_dispatch_log_trace();
  OSStatus err = kUnknownErr;
  
  require_action(inContext, exit, err = kParamErr);
  
  g_notify_thread_data.context = inContext;
  g_notify_thread_data.p_service_table = service_table;
  g_notify_thread_data.notify_interval = period_ms;
  
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "prop_notify", 
                                notify_thread, stack_size, 
                                (void*)&g_notify_thread_data );
  
exit:
  return err; 
}
