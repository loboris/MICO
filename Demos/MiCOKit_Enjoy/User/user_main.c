/**
  ******************************************************************************
  * @file    user_main.c 
  * @author  Eshen Wang
  * @version V1.0.0
  * @date    17-Mar-2015
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
#include "fogcloud_msg_dispatch.h"

#include "user_properties.h"
#include "micokit_ext.h"
#include "user_uart.h"

#define user_log(M, ...) custom_log("USER", M, ##__VA_ARGS__)
#define user_log_trace() custom_log_trace("USER")


/* properties defined in user_properties.c by user
 */
// device services &&¡¡properties table
extern struct mico_service_t  service_table[];
// user context
extern user_context_t g_user_context;


//---------------------------- User work function ------------------------------

// delete device from cloud when KEY1(ext-board) long pressed.
void user_key1_long_pressed_callback(void)
{
  MiCOFogCloudNeedResetDevice();  // set device need reset flag
  return;
}

/* FogCloud msg handle thread */
void user_cloud_msg_handle_thread(void* arg)
{
  OSStatus err = kUnknownErr;
  fogcloud_msg_t *recv_msg = NULL;
  mico_fogcloud_msg_t mico_fog_msg;
  int retCode = MSG_PROP_UNPROCESSED;
  app_context_t *app_context = (app_context_t *)arg;
  require_action(app_context, exit, err=kParamErr);
  
  while(1){
    mico_thread_msleep(100);  // in case of while (1)
    
    err = MiCOFogCloudMsgRecv(app_context, &recv_msg, 0);
    if(kNoErr == err){
      user_log("Msg recv: topic[%d]=[%.*s]\tdata[%d]=[%.*s]", 
               recv_msg->topic_len, recv_msg->topic_len, recv_msg->data, 
               recv_msg->data_len, recv_msg->data_len, recv_msg->data + recv_msg->topic_len);
      // msg structure format transfer
      mico_fog_msg.topic = (const char*)(recv_msg->data);
      mico_fog_msg.topic_len = recv_msg->topic_len;
      mico_fog_msg.data = recv_msg->data + recv_msg->topic_len;
      mico_fog_msg.data_len = recv_msg->data_len;
      err = mico_fogcloud_msg_dispatch(app_context, service_table, &mico_fog_msg, &retCode);    
      if(kNoErr != err){
        user_log("ERROR: mico_cloudmsg_dispatch error, err=%d", err);
      }
      else{
      }
  
      // NOTE: msg memory must be free after been used.
      if(NULL != recv_msg){
        free(recv_msg);
        recv_msg = NULL;
      }
    }
  }
  
exit:
  user_log("ERROR: user_cloud_msg_handle_thread exit with err=%d", err);
  return;
}

OSStatus start_fog_msg_handler(app_context_t *app_context)
{
  user_log_trace();
  OSStatus err = kNoErr;
  require_action(app_context, exit, err = kParamErr);
  
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "user_msg_handler", 
                                user_cloud_msg_handle_thread, STACK_SIZE_USER_MSG_HANDLER_THREAD, 
                                app_context );
exit:
  return err;
}

//--- show actions for user, such as OLED, RGB_LED, DC_Motor
void system_state_display( app_context_t * const app_context, user_context_t *user_context)
{
  // update 2~4 lines on OLED
  char oled_show_line[OLED_DISPLAY_MAX_CHAR_PER_ROW+1] = {'\0'};
  
  if(app_context->appStatus.fogcloudStatus.isOTAInProgress){
    return;  // ota is in progress, the oled && system led will be holding
  }
  
  if(user_context->status.oled_keep_s >= 1){
    user_context->status.oled_keep_s--;  // keep display current info
  }
  else{
    if(!app_context->appStatus.isWifiConnected){
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)"Wi-Fi           ");
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, (uint8_t*)"  Connecting... ");
      memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
      snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%16s", 
               app_context->mico_context->flashContentInRam.micoSystemConfig.ssid);
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, (uint8_t*)oled_show_line);
    }
    else if(!app_context->appStatus.fogcloudStatus.isCloudConnected){
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)"FogCloud        ");
      if(app_context->appConfig->fogcloudConfig.isActivated){
        OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, (uint8_t*)"  Connecting... ");
        OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, (uint8_t*)"                ");
      }
      else{
        OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, (uint8_t*)"  Awaiting      ");
        OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, (uint8_t*)"    activation..");
      }
    }
    else{
      snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "MAC:%c%c%c%c%c%c%c%c%c%c%c%c",
               app_context->mico_context->micoStatus.mac[0], app_context->mico_context->micoStatus.mac[1], 
               app_context->mico_context->micoStatus.mac[3], app_context->mico_context->micoStatus.mac[4], 
               app_context->mico_context->micoStatus.mac[6], app_context->mico_context->micoStatus.mac[7],
               app_context->mico_context->micoStatus.mac[9], app_context->mico_context->micoStatus.mac[10],
               app_context->mico_context->micoStatus.mac[12], app_context->mico_context->micoStatus.mac[13],
               app_context->mico_context->micoStatus.mac[15], app_context->mico_context->micoStatus.mac[16]);
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)oled_show_line);
      snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%-16s", app_context->mico_context->micoStatus.localIp);
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, (uint8_t*)oled_show_line);
      // temperature/humidity display on OLED
      memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
      snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "T: %2dC  H: %2d%%  ", 
               user_context->status.temperature, user_context->status.humidity);
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, (uint8_t*)oled_show_line);
    }
  }
}

/* user main function, called by AppFramework.
 */
OSStatus user_main( app_context_t * const app_context )
{
  user_log_trace();
  OSStatus err = kUnknownErr;
  
  user_log("User main task start...");
  
  err = user_uartInit();
  require_noerr_action( err, exit, user_log("ERROR: user_uartInit err = %d.", err) );
 
#if (MICO_CLOUD_TYPE != CLOUD_DISABLED)
  /* start fogcloud msg handle task */
  err = start_fog_msg_handler(app_context);
  require_noerr_action( err, exit, user_log("ERROR: start_fog_msg_handler err = %d.", err) );

  /* start properties notify task(upload data) */
  err = mico_start_properties_notify(app_context, service_table, 
                                     MICO_PROPERTIES_NOTIFY_INTERVAL_MS, 
                                     STACK_SIZE_NOTIFY_THREAD);
  require_noerr_action( err, exit, user_log("ERROR: mico_start_properties_notify err = %d.", err) );
#endif
  
  /* main loop for user display */
  while(1){
    // check every 1 seconds
    mico_thread_sleep(1);
    
    // system work state show on OLED
    system_state_display(app_context, &g_user_context);
  }
  
exit:
  user_log("ERROR: user_main exit with err=%d", err);
  return err;
}
