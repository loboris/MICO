/**
  ******************************************************************************
  * @file    MICOConfigDelegate.c 
  * @author  William Xu
  * @version V1.0.0
  * @date    05-May-2014
  * @brief   This file provide delegate functons from Easylink function and FTC
  *          server. 
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
#include "MICO.h"
#include "Platform_config.h"

#include "MICOAppDefine.h"
#include "SppProtocol.h"  
#include "StringUtils.h"        


#define config_delegate_log(M, ...) custom_log("Config Delegate", M, ##__VA_ARGS__)
#define config_delegate_log_trace() custom_log_trace("Config Delegate")

USED void mico_system_delegate_config_success( mico_config_source_t source )
{
  config_delegate_log( "Configed by %d", source );
}

USED void config_server_delegate_report( json_object *app_menu, mico_Context_t *in_context )
{
  config_delegate_log_trace();
  OSStatus err = kNoErr;
  application_config_t *appConfig = mico_system_context_get_user_data(in_context);
  
  // SPP protocol remote server connection enable
  err = config_server_create_bool_cell(app_menu, "Connect SPP Server",   appConfig->remoteServerEnable,   "RW");
  require_noerr(err, exit);

  //Seerver address cell
  err = config_server_create_string_cell(app_menu, "SPP Server",           appConfig->remoteServerDomain,   "RW", NULL);
  require_noerr(err, exit);

  //Seerver port cell
  err = config_server_create_number_cell(app_menu, "SPP Server Port",      appConfig->remoteServerPort,   "RW", NULL);
  require_noerr(err, exit);

  /*UART Baurdrate cell*/
  json_object *selectArray;
  selectArray = json_object_new_array();
  require( selectArray, exit );
  json_object_array_add(selectArray, json_object_new_int(9600));
  json_object_array_add(selectArray, json_object_new_int(19200));
  json_object_array_add(selectArray, json_object_new_int(38400));
  json_object_array_add(selectArray, json_object_new_int(57600));
  json_object_array_add(selectArray, json_object_new_int(115200));
  err = config_server_create_number_cell(app_menu, "Baurdrate", 115200, "RW", selectArray);
  require_noerr(err, exit);
  
exit:
  return;
}

USED void config_server_delegate_recv( const char *key, json_object *value, bool *need_reboot, mico_Context_t *in_context )
{
  config_delegate_log_trace();
  application_config_t *appConfig = mico_system_context_get_user_data(in_context);

  if(!strcmp(key, "Connect SPP Server")){
    appConfig->remoteServerEnable = json_object_get_boolean(value);
    *need_reboot = true;
  }else if(!strcmp(key, "SPP Server")){
    strncpy(appConfig->remoteServerDomain, json_object_get_string(value), 64);
    *need_reboot = true;
  }else if(!strcmp(key, "SPP Server Port")){
    appConfig->remoteServerPort = json_object_get_int(value);
    *need_reboot = true;
  }else if(!strcmp(key, "Baurdrate")){
    appConfig->USART_BaudRate = json_object_get_int(value);
    *need_reboot = true;
  }
}
