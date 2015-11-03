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
#include "user_config.h"
#include "MicoFogCloud.h"

#ifdef USE_MiCOKit_EXT
  #include "micokit_ext.h"
#endif

#include "json_c/json.h"
#include "temp_hum_sensor\DHT11\DHT11.h"
#include "lcd\oled.h"

/* User defined debug log functions
 * Add your own tag like: 'USER', the tag will be added at the beginning of a log
 * in MICO debug uart, when you call this function.
 */
#define user_log(M, ...) custom_log("USER", M, ##__VA_ARGS__)
#define user_log_trace() custom_log_trace("USER")


/* user main function, called by AppFramework after system init done && wifi
 * station on in user_main thread.
 */
OSStatus user_main( app_context_t * const app_context )
{
  user_log_trace();
  OSStatus err = kUnknownErr;
  json_object *send_json_object = NULL;
  const char *upload_data = NULL;
  
  uint8_t ret = 0;
  uint8_t dht11_temperature = 0;
  uint8_t dht11_humidity = 0;
  
  // update 2~4 lines on OLED
  char oled_show_line[OLED_DISPLAY_MAX_CHAR_PER_ROW+1] = {'\0'};
    
  require(app_context, exit);
  
  // init humiture sensor DHT11
  ret = DHT11_Init();
  if(0 != ret){  // init error
    err = kNoResourcesErr;
    user_log("DHT11 init failed!");
    goto exit;
  }
  else{
    err = kNoErr;
  }
  
  while(1){
    mico_thread_sleep(2);  // data acquisition && upload every 2 seconds
    
    // check fogcloud connect status
    if(!app_context->appStatus.fogcloudStatus.isCloudConnected){
      continue;
    }
    
    // fogcloud connected, do data acquisition
    ret = DHT11_Read_Data(&dht11_temperature, &dht11_humidity);
    if(0 != ret){
      err = kReadErr;
    }
    else{
      err = kNoErr;
      
      // temperature/humidity display on OLED, each line 16 chars max
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, "Demo Read H/T   ");  // clean line2
      
      memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
      snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "T: %2dC         ", dht11_temperature);
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, (uint8_t*)oled_show_line);
      
      memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
      snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "H: %2d%%        ", dht11_humidity);
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, (uint8_t*)oled_show_line);
      
      // create json object && upload to cloud
      send_json_object = json_object_new_object();
      if(NULL == send_json_object){
        user_log("create json object error!");
        err = kNoMemoryErr;
      }
      else{
        // create json object && data string to upload
        json_object_object_add(send_json_object, "dht11_temperature", json_object_new_int(dht11_temperature)); 
        json_object_object_add(send_json_object, "dht11_humidity", json_object_new_int(dht11_humidity)); 
        upload_data = json_object_to_json_string(send_json_object);
        if(NULL == upload_data){
          user_log("create upload data string error!");
          err = kNoMemoryErr;
        }
        else{
          // upload data string to fogcloud, the seconde param(NULL) means send to defalut topic: '<device_id>/out'
          MiCOFogCloudMsgSend(app_context, NULL, (unsigned char*)upload_data, strlen(upload_data));
          user_log("upload data success!\r\ntopic=%s/out\tdht11_temperature=%d, dht11_humidity=%d", 
                   app_context->appConfig->fogcloudConfig.deviceId,
                   dht11_temperature, dht11_humidity);
          err = kNoErr;
        }
        
        // free json object memory
        json_object_put(send_json_object);
        send_json_object = NULL;
      }
    }  
  }

exit:
  user_log("ERROR: user_main exit with err=%d", err);
  return err;
}
