/**
******************************************************************************
* @file    user_main.c 
* @author  Eshen Wang
* @version V1.0.0
* @date    14-May-2015
* @brief   user main functons in user_main thread.
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
#include "MicoFogCloud.h"
#include "json_c/json.h"
#include "rgb_led/hsb2rgb_led.h"


/* User defined debug log functions
 * Add your own tag like: 'USER_DOWNSTREAM', the tag will be added at the beginning of a log
 * in MICO debug uart, when you call this function.
 */
#define user_log(M, ...) custom_log("USER_DOWNSTREAM", M, ##__VA_ARGS__)
#define user_log_trace() custom_log_trace("USER_DOWNSTREAM")


// device state on/off
extern volatile bool  device_switch;  // device state true: on , false: off
extern volatile bool device_switch_changed;  // device state changed flag

extern bool rgbled_switch;
extern int rgbled_hues;
extern int rgbled_saturation;
extern int rgbled_brightness;

extern volatile bool rgbled_changed;  // rgb led state changed flag

/* Handle user message from cloud
 * Receive msg from cloud && do hardware operation, like rgbled
 */
void user_downstream_thread(void* arg)
{
  user_log_trace();
  OSStatus err = kUnknownErr;
  app_context_t *app_context = (app_context_t *)arg;
  fogcloud_msg_t *recv_msg = NULL;
  json_object *recv_json_object = NULL;
  
  bool device_switch_tmp = true;
    
  require(app_context, exit);
  
  /* thread loop to handle cloud message */
  while(1){
    mico_thread_msleep(200);
        
    // check fogcloud connect status
    if(!app_context->appStatus.fogcloudStatus.isCloudConnected){
      continue;
    }
    
    /* get a msg pointer, points to the memory of a msg: 
     * msg data format: recv_msg->data = <topic><data>
     */
    err = MiCOFogCloudMsgRecv(app_context, &recv_msg, 100);
    if(kNoErr == err){
      // debug log in MICO dubug uart
      user_log("Cloud => Module: topic[%d]=[%.*s]\tdata[%d]=[%.*s]", 
               recv_msg->topic_len, recv_msg->topic_len, recv_msg->data, 
               recv_msg->data_len, recv_msg->data_len, recv_msg->data + recv_msg->topic_len);
      
      // parse json data from the msg, get led control value
      recv_json_object = json_tokener_parse((const char*)(recv_msg->data + recv_msg->topic_len));
      if (NULL != recv_json_object){
        json_object_object_foreach(recv_json_object, key, val) {
          if(!strcmp(key, "rgbled_switch")){
            rgbled_switch = json_object_get_boolean(val);
            rgbled_changed = true;
          }
          else if(!strcmp(key, "rgbled_hues")){
            rgbled_hues = json_object_get_int(val);
            rgbled_changed = true;
          }
          else if(!strcmp(key, "rgbled_saturation")){
            rgbled_saturation = json_object_get_int(val);
            rgbled_changed = true;
          }
          else if(!strcmp(key, "rgbled_brightness")){
            rgbled_brightness = json_object_get_int(val);
            rgbled_changed = true;
          }
          else if(!strcmp(key, "device_switch")){
            device_switch_tmp = json_object_get_boolean(val);
            if(device_switch_tmp != device_switch){  // device on/off state changed
              device_switch_changed = true;
              device_switch = device_switch_tmp;
            }
          }
          else{}
        }
        
        // free memory of json object
        json_object_put(recv_json_object);
        recv_json_object = NULL;
      }
      
      // NOTE: must free msg memory after been used.
      if(NULL != recv_msg){
        free(recv_msg);
        recv_msg = NULL;
      }
    }
  }

exit:
  user_log("ERROR: user_downstream_thread exit with err=%d", err);
}
