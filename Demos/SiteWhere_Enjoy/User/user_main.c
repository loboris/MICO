/**
******************************************************************************
* @file    user_main.c 
* @author  Eshen Wang
* @version V1.0.0
* @date    14-May-2015
* @brief   user main tasks in user_main thread.
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
#include "MiCOAppDefine.h"

#include "json_c/json.h"
#include "rgb_led/hsb2rgb_led.h"
#include "lcd/oled.h"

/* User defined debug log functions
 * Add your own tag like: 'USER', the tag will be added at the beginning of a log
 * in MICO debug uart, when you call this function.
 */
#define user_log(M, ...) custom_log("USER", M, ##__VA_ARGS__)
#define user_log_trace() custom_log_trace("USER")


//--- show actions for user, such as OLED, RGB_LED, DC_Motor
void system_state_display( app_context_t * const app_context)
{
  // update 2~4 lines on OLED
  char oled_show_line[OLED_DISPLAY_MAX_CHAR_PER_ROW+1] = {'\0'};
  
  if(app_context->appStatus.user_context.status.oled_keep_s >= 1){
    app_context->appStatus.user_context.status.oled_keep_s--;  // keep display current info
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
    else if(!app_context->appStatus.isCloudConnected){
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)"Cloud           ");
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, (uint8_t*)"  Connecting... ");
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, (uint8_t*)"                ");
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
               app_context->appStatus.user_context.status.temperature, 
               app_context->appStatus.user_context.status.humidity);
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, (uint8_t*)oled_show_line);
    }
  }
}

/* user main function, called by AppFramework after system init done && wifi
 * station on in user_main thread.
 */
OSStatus user_main( app_context_t * const app_context )
{
  user_log_trace();
  OSStatus err = kUnknownErr;
  require(app_context, exit);
  
  hsb2rgb_led_init();  // rgb led init
  
  while(1){
    mico_thread_sleep(1);
    
    // system work state show on OLED
    system_state_display(app_context);
  }

exit:
  user_log("ERROR: user_main exit with err=%d", err);
  return err;
}
