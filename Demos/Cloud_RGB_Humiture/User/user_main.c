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
#include "MICOAppDefine.h"
#include "MicoFogCloud.h"

#include "json_c/json.h"
#include "lcd/oled.h"
#include "rgb_led/hsb2rgb_led.h"

/* User defined debug log functions
 * Add your own tag like: 'USER', the tag will be added at the beginning of a log
 * in MICO debug uart, when you call this function.
 */
#define user_log(M, ...) custom_log("USER", M, ##__VA_ARGS__)
#define user_log_trace() custom_log_trace("USER")

// DTH11 termperature && humidity data to display on OLED
extern uint8_t dht11_temperature;
extern uint8_t dht11_humidity;

/* rgb led control variants, use hsb color.
* h -- hues
* s -- saturation
* b -- brightness
*/
bool rgbled_switch = false;
int rgbled_hues = 0;
int rgbled_saturation = 0;
int rgbled_brightness = 0;

// device on/off status
volatile bool device_switch = true;  // device state true: on , false: off
volatile bool device_switch_changed = false;  // device state changed flag

volatile bool rgbled_changed = false;  // rgb led state changed flag

static mico_thread_t user_downstrem_thread_handle = NULL;
static mico_thread_t user_upstream_thread_handle = NULL;
  
extern void user_downstream_thread(void* arg);
extern void user_upstream_thread(void* arg);


/* user main function, called by AppFramework after system init done && wifi
 * station on in user_main thread.
 */
OSStatus user_main( app_context_t * const app_context )
{
  user_log_trace();
  OSStatus err = kUnknownErr;
  // one line on OLED, 16 chars max, we just update line2~4 for the first line is not need to change
  char oled_show_line[OLED_DISPLAY_MAX_CHAR_PER_ROW+1] = {'\0'};

  require(app_context, exit);
  
  hsb2rgb_led_init();   // init rgb led
  hsb2rgb_led_close();  // close rgb led
 
  // start the downstream thread to handle user command
  err = mico_rtos_create_thread(&user_downstrem_thread_handle, MICO_APPLICATION_PRIORITY, "user_downstream", 
                                user_downstream_thread, STACK_SIZE_USER_DOWNSTREAM_THREAD, 
                                app_context );
  require_noerr_action( err, exit, user_log("ERROR: create user_downstream thread failed!") );
    
  // start the upstream thread to upload temperature && humidity to user
  err = mico_rtos_create_thread(&user_upstream_thread_handle, MICO_APPLICATION_PRIORITY, "user_upstream", 
                                  user_upstream_thread, STACK_SIZE_USER_UPSTREAM_THREAD, 
                                  app_context );
  require_noerr_action( err, exit, user_log("ERROR: create user_uptream thread failed!") );
  
  // user_main loop, update oled display every 1s
  while(1){
    mico_thread_msleep(500);
    
    // device on/off change
    if(device_switch_changed){
      device_switch_changed = false;
      
      if(!device_switch){  // suspend upstream thread to stop working
        mico_rtos_suspend_thread(&user_upstream_thread_handle);
        hsb2rgb_led_close();
        OLED_Display_Off();
      }
      else{
        mico_rtos_resume_thread(&user_upstream_thread_handle);  // resume upstream thread to work again
        OLED_Display_On();  // resume OLED 
      }
    }
    
    // device on, update OLED && rgbled
    if(device_switch){
      // update OLED line 2~4
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, "Demo H/T && LED  ");  // line 2
      
      memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
      snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "T: %2dC         ", dht11_temperature);
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, (uint8_t*)oled_show_line);
      
      memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
      snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "H: %2d%%        ", dht11_humidity);
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, (uint8_t*)oled_show_line);
      
      // rgb led operation
      if(rgbled_changed){
        rgbled_changed = false;
        if(rgbled_switch){
          hsb2rgb_led_open(rgbled_hues, rgbled_saturation, rgbled_brightness);  // open rgb led
        }else{
          hsb2rgb_led_close();  // close rgb led
        }
      }
    }
  }

exit:
  if(kNoErr != err){
    user_log("ERROR: user_main thread exit with err=%d", err);
  }
  mico_rtos_delete_thread(NULL);  // delete current thread
  return err;
}
