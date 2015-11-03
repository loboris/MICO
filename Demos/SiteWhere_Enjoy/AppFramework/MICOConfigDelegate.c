/**
  ******************************************************************************
  * @file    MICOConfigDelegate.c 
  * @author  William Xu
  * @version V1.0.0
  * @date    05-May-2014
  * @brief   This file provide delegate functons from Easylink function and
  *          fogcloud data function. 
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
#include "StringUtils.h"

#ifdef USE_MiCOKit_EXT
#include "micokit_ext.h"   // extension board operation by user.
#endif

#define SYS_LED_TRIGGER_INTERVAL  100 
#define SYS_LED_TRIGGER_INTERVAL_AFTER_EASYLINK  500 
#define RF_LED_TRIGGER_INTERVAL_AFTER_CLOUD_CONNECTED  1000 
#define SYS_LED_TRIGGER_OTA_INTERVAL    50 
  
#define config_delegate_log(M, ...) custom_log("Config Delegate", M, ##__VA_ARGS__)
#define config_delegate_log_trace() custom_log_trace("Config Delegate")

static mico_timer_t _Led_EL_timer;
static void _led_EL_Timeout_handler( void* arg );


void OTAWillStart( mico_Context_t * const inContext )
{
  //config_delegate_log_trace();
  (void)(inContext); 
#ifdef USE_MiCOKit_EXT
  char oled_show_line[OLED_DISPLAY_MAX_CHAR_PER_ROW+1] = {'\0'};
#endif

  /*Led trigger*/
  mico_init_timer(&_Led_EL_timer, SYS_LED_TRIGGER_OTA_INTERVAL, _led_EL_Timeout_handler, NULL);
  mico_start_timer(&_Led_EL_timer);
  
#ifdef USE_MiCOKit_EXT
  memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
  snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%s", (uint8_t*)"Firmware update ");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)oled_show_line);
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, "  Donwloading...");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, "  will reboot...");
#endif
  
  return;
}

void OTAFailed( mico_Context_t * const inContext )
{
  //config_delegate_log_trace();
  (void)(inContext); 
#ifdef USE_MiCOKit_EXT
  char oled_show_line[OLED_DISPLAY_MAX_CHAR_PER_ROW+1] = {'\0'};
#endif

  // led stop
  mico_stop_timer(&_Led_EL_timer);
  mico_deinit_timer( &_Led_EL_timer );
  MicoSysLed(true);
  
#ifdef USE_MiCOKit_EXT
  memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
  snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%s", (uint8_t*)"OTA...          ");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)oled_show_line);
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, "  Failed!       ");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, "                ");
#endif
  
  return;
}

void OTASuccess( mico_Context_t * const inContext )
{
  //config_delegate_log_trace();
  (void)(inContext); 
#ifdef USE_MiCOKit_EXT
  char oled_show_line[OLED_DISPLAY_MAX_CHAR_PER_ROW+1] = {'\0'};
#endif
  
  // led stop
  mico_stop_timer(&_Led_EL_timer);
  mico_deinit_timer( &_Led_EL_timer );
  MicoSysLed(true);

#ifdef USE_MiCOKit_EXT
  memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
  snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%s", (uint8_t*)"OTA...          ");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)oled_show_line);
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, "  Success!      ");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, "    reboot...   ");
#endif
  return;
}

void set_RF_LED_cloud_connected( app_context_t * const inContext )
{
  config_delegate_log_trace();
  (void)(inContext); 
  /*Led trigger*/
   MicoGpioOutputTrigger((mico_gpio_t)MICO_RF_LED);
  
  return;
}
 
void set_RF_LED_cloud_disconnected( app_context_t * const inContext )
{
  config_delegate_log_trace();
  
  if(inContext->appStatus.isWifiConnected){
    MicoRfLed(true);
  }
  else{
    MicoRfLed(false);
  }
  
  return;
}

static void _led_EL_Timeout_handler( void* arg )
{
  (void)(arg);
  MicoGpioOutputTrigger((mico_gpio_t)MICO_SYS_LED);
}

void mico_system_delegate_config_will_start( void )
{
  config_delegate_log_trace();
#ifdef USE_MiCOKit_EXT
  char oled_show_line[OLED_DISPLAY_MAX_CHAR_PER_ROW+1] = {'\0'};
#endif
  
  mico_Context_t* mico_context = mico_system_context_get();
  
  /*Led trigger*/
  mico_init_timer(&_Led_EL_timer, SYS_LED_TRIGGER_INTERVAL, _led_EL_Timeout_handler, NULL);
  mico_start_timer(&_Led_EL_timer);
#ifdef USE_MiCOKit_EXT
  memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
  snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%s", (uint8_t*)"Config...       ");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)oled_show_line);
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, "  Awaiting      ");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, "    ssid/key    ");
#endif
  
  return;
}

void mico_system_delegate_config_will_stop( void )
{
  config_delegate_log_trace();
#ifdef USE_MiCOKit_EXT
  char oled_show_line[OLED_DISPLAY_MAX_CHAR_PER_ROW+1] = {'\0'};
#endif

  mico_stop_timer(&_Led_EL_timer);
  mico_deinit_timer( &_Led_EL_timer );
  MicoSysLed(false);
  
#ifdef USE_MiCOKit_EXT
  memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
  snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%s", (uint8_t*)"Config          ");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)oled_show_line);
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, "    Stop.       ");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, "                ");
#endif
  
  return;
}

void mico_system_delegate_config_recv_ssid ( char *ssid, char *key )
{
  config_delegate_log_trace();
#ifdef USE_MiCOKit_EXT
  char oled_show_line[OLED_DISPLAY_MAX_CHAR_PER_ROW+1] = {'\0'};
#endif

  mico_stop_timer(&_Led_EL_timer);
  mico_deinit_timer( &_Led_EL_timer );
  mico_init_timer(&_Led_EL_timer, SYS_LED_TRIGGER_INTERVAL_AFTER_EASYLINK, _led_EL_Timeout_handler, NULL);
  mico_start_timer(&_Led_EL_timer);
  
#ifdef USE_MiCOKit_EXT
  memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
  snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%s", (uint8_t*)"Got ssid/key    ");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)oled_show_line);
  memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
  snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%16s", ssid);
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, (uint8_t*)oled_show_line);
  memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
  snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%16s", key);
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, (uint8_t*)oled_show_line);
#endif
}

void mico_system_delegate_config_success( mico_config_source_t source )
{
  config_delegate_log_trace();
#ifdef USE_MiCOKit_EXT
  char oled_show_line[OLED_DISPLAY_MAX_CHAR_PER_ROW+1] = {'\0'};
#endif
  
  config_delegate_log( "Wi-Fi configed by: %d", source );

  MicoSysLed(true);  
  
#ifdef USE_MiCOKit_EXT
  memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
  if(CONFIG_BY_AIRKISS == source){
    snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%s", (uint8_t*)"Airkiss success ");
  }else if(CONFIG_BY_SOFT_AP == source){
    snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%s", (uint8_t*)"SoftAP success  ");
  }else if(CONFIG_BY_WAC == source){
    snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%s", (uint8_t*)"WAC success     ");
  }else if( (CONFIG_BY_EASYLINK_V2 == source) || (CONFIG_BY_EASYLINK_PLUS == source) || 
           (CONFIG_BY_EASYLINK_MINUS == source) ){
    snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%s", (uint8_t*)"Easylink success");
  }else{
    snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%s", (uint8_t*)"Unknown         ");
  }
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)oled_show_line);
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, (uint8_t*)"                ");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, (uint8_t*)"                ");
#endif
  
  return;
}

USED void config_server_delegate_report( json_object *app_menu, mico_Context_t *in_context )
{
  config_delegate_log_trace();
  OSStatus err = kNoErr;
  application_config_t *appConfig = mico_system_context_get_user_data(in_context);
  
  // confifg data version number
  err = config_server_create_number_cell(app_menu, "ConfigDataVer", appConfig->configDataVer, "RW", NULL);
  require_noerr(err, exit);
  // bonjourt service port
  err = config_server_create_number_cell(app_menu, "BonjourPort", appConfig->bonjourServicePort, "RW", NULL);
  require_noerr(err, exit);
  
exit:
  return;
}

USED void config_server_delegate_recv( const char *key, json_object *value, bool *need_reboot, mico_Context_t *in_context )
{
  config_delegate_log_trace();
  application_config_t *appConfig = mico_system_context_get_user_data(in_context);

  if(!strcmp(key, "ConfigDataVer")){
    appConfig->configDataVer = json_object_get_int(value);
    *need_reboot = true;
  }else if(!strcmp(key, "BonjourPort")){
    appConfig->bonjourServicePort = json_object_get_int(value);
    *need_reboot = true;
  }else{
  }
}
