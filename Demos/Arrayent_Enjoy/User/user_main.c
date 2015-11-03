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

#include "MiCO.h"
#include "micokit_ext.h"
#include "MiCOArrayentService.h"
#include "aca.h"
#include "DHT11/DHT11.h"

/* User defined debug log functions
 * Add your own tag like: 'USER', the tag will be added at the beginning of a log
 * in MICO debug uart, when you call this function.
 */
#define user_log(M, ...) custom_log("USER", M, ##__VA_ARGS__)
#define user_log_trace() custom_log_trace("USER")

uint16_t infrared_reflective_data = 0,light_sensor_data = 0;
uint8_t dht11_temp_data = 0,dht11_hum_data = 0;

/** Structure for registering uart commands */
struct uart_command {
  /** The name of the CLI command */
  const char *name;
  /** The function that should be invoked for this command. */
  void (*function) (char *property, app_context_t * const inContext);
};

int8_t checkCmd(char* property, app_context_t *Context);
//void refresh_command(char *property, app_context_t * const inContext);
//void setCode_command(char *property, app_context_t * const inContext);
//void setPwd_command(char *property, app_context_t * const inContext);
void setLightState_command(char *property, app_context_t * const inContext);
void setDCState_command(char *property, app_context_t * const inContext);
void setRGBState_command(char *property, app_context_t * const inContext);
void property_update(void *inContext);
void property_recv(void *inContext);
void update_dht11_sensor(void);
void update_light_sensor(void);
void update_infrared_sensor(void);
void itoa(int num,char *str);

static const struct uart_command uart_cmds[] = {
//  {"config_refresh", refresh_command},
//  {"config_arr_code",  setCode_command},
//  {"config_arr_pwd",  setPwd_command},
//  {"light", setLightState_command},
  {"DC",setDCState_command},
  {"RGB",setRGBState_command}
};

/* display on OLED */
void system_state_display( app_context_t * const app_context)
{
  // update 2~4 lines on OLED
  char oled_show_line[OLED_DISPLAY_MAX_CHAR_PER_ROW+1] = {'\0'};
  
    if(!app_context->appStatus.isWifiConnected){
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)"Wi-Fi           ");
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, (uint8_t*)"  Connecting... ");
      memset(oled_show_line, '\0', OLED_DISPLAY_MAX_CHAR_PER_ROW+1);
      snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%16s", 
               app_context->mico_context->flashContentInRam.micoSystemConfig.ssid);
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, (uint8_t*)oled_show_line);
    }
    else if(!app_context->appStatus.arrayentStatus.isCloudConnected){
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)"Arrayent        ");
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
               dht11_temp_data, dht11_hum_data);
      OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, (uint8_t*)oled_show_line);
    }
}

/* user main function, called by AppFramework.
 */
OSStatus user_main( app_context_t * const app_context )
{
  user_log_trace();
  OSStatus err = kUnknownErr;
  
  user_log("User main thread start...");
  
  // start prop get/set thread
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "prop_recv", property_recv, 
                                STACK_SIZE_PROP_RECV_THREAD, (void*)app_context);
  require_noerr_action( err, exit, user_log("ERROR: Unable to start the prop_recv thread.") );
  
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "prop_update", property_update, 
                                STACK_SIZE_PROP_UPDATE_THREAD, (void*)app_context);
  require_noerr_action( err, exit, user_log("ERROR: Unable to start the prop_update thread.") );

  /* main loop for user display */
  while(1){
    // system work state show on OLED
    system_state_display(app_context);
    mico_thread_sleep(1);
  }
  
exit:
  user_log("ERROR: user_main exit with err=%d", err);
  return err;
}

int8_t checkCmd(char* property, app_context_t *Context){
  
  int i = 0;
  int nameLen = 0;
  
  for(i=0;i < sizeof(uart_cmds)/sizeof(uart_cmds[0]); i++){
    
    nameLen = strlen(uart_cmds[i].name);
    //user_log("property[%d]: %c result=%d\r\n", nameLen, property[nameLen],strncmp(uart_cmds[i].name, property, nameLen) );
    if(0 == strncmp(uart_cmds[i].name, property, nameLen)
       &&property[nameLen] == ' ' ){//compare command and run.
         uart_cmds[i].function(property, Context);
         return 0;
       }
  }
  
  return -1;
}

//void refresh_command(char *property, app_context_t * const inContext){
//  //ArrayentSetProperty("config_arr_code",inContext->flashContentInRam.appConfig.aca_config.code);
//  //ArrayentSetProperty("config_arr_pwd",inContext->flashContentInRam.appConfig.aca_config.pwd);
//}

//void setCode_command(char *property, app_context_t * const Context)
//{
//}

//void setPwd_command(char *property, app_context_t * const Context)
//{
//}

//void setLightState_command(char *property, app_context_t * const inContext)
//{
//  if(0==strcmp(property, "light 0")){
//    MicoSysLed(false);
//  }else if(0==strcmp(property, "light 1")){
//    MicoSysLed(true);
//  }
//}


void setDCState_command(char *property, app_context_t * const inContext)
{
  if(0==strcmp(property, "DC 1")){  // dc motor on
    dc_motor_set(1);
  }else if(0==strcmp(property, "DC 0")){
    dc_motor_set(0);
  } 
}


void setRGBState_command(char *property, app_context_t * const inContext){
  if(0==strcmp(property, "RGB red")){
    hsb2rgb_led_open(0, 100, 50);
  }else if(0==strcmp(property, "RGB green")){
    hsb2rgb_led_open(120, 100, 50);
  }
  else if(0==strcmp(property, "RGB blue")){
    hsb2rgb_led_open(240, 100, 50);
  }
  else if(0==strcmp(property, "RGB off")){
    hsb2rgb_led_open(0, 0, 0);
  }
}

void property_update(void *inContext)
{
  int num = 0;
  app_context_t * app_context = (app_context_t*)inContext;
  
  while(1)
  {
    mico_thread_sleep(1);
    if(!app_context->appStatus.arrayentStatus.isCloudConnected){
      continue;
    }
    
    switch(num)
    {
    case 0:
      update_dht11_sensor();
      break;
    case 1:
      update_light_sensor();
      break;
    case 2:
      update_infrared_sensor();
      break;
    }
    num++;
    if(num == 3)num = 0;
  }
}

void property_recv(void *inContext)
{
  uint16_t len = 0;
  char property[150];
  
  app_context_t * app_context = (app_context_t*)inContext;
  
  while(1)
  {  
    if(!app_context->appStatus.arrayentStatus.isCloudConnected){
      mico_thread_sleep(1);
      continue;
    }

    len = sizeof(property);
    memset(property, 0, len);
    if(ARRAYENT_SUCCESS == ArrayentRecvProperty(property,&len,10000)){//receive property data from server
      user_log("recv property=%s\n\r",property);
      if(0 != checkCmd(property, inContext)){//parse cmd success
        user_log("checkCmd error: property=%s", property);
      }
    }
  }
}

void update_dht11_sensor(void)
{
  uint8_t dht11_ret = 1;
  char temp_value[10];
  char hum_value[10];
  
  dht11_ret = DHT11_Read_Data(&dht11_temp_data, &dht11_hum_data);
  if(0 == dht11_ret)
  {
    itoa(dht11_temp_data,temp_value);
    itoa(dht11_hum_data,hum_value);
    if(ARRAYENT_SUCCESS!=ArrayentSetProperty("temp",temp_value))
    {
      user_log("service have rejected!\n\r");
    }
    mico_thread_msleep(200);
    if(ARRAYENT_SUCCESS!=ArrayentSetProperty("hsv",hum_value))
    {
      user_log("service have rejected!\n\r");
    }
    
  }
}


void update_light_sensor(void)
{
  int light_ret = 0;
  char light_value[10];
  
  light_ret = light_sensor_read(&light_sensor_data);
  if(0 == light_ret)
  {
    itoa(light_sensor_data,light_value);
    if(ARRAYENT_SUCCESS!=ArrayentSetProperty("light",light_value))
    {
      user_log("service have rejected!\n\r");
    }
    
  }
}


void update_infrared_sensor(void)
{
  int infrared_ret = 0;
  char infrared_value[10];
  
  infrared_ret = infrared_reflective_read(&infrared_reflective_data);
  if(0 == infrared_ret)
  { 
    itoa(infrared_reflective_data,infrared_value);
    if(ARRAYENT_SUCCESS!=ArrayentSetProperty("infrared",infrared_value))
    {
      user_log("service have rejected!\n\r");
    }
    
  }
  
}

void itoa(int num,char *str)
{
  int i = 0,j = 0,sign = 0;
  char buff[10];
  
  sign = num;
  if(sign < 0)sign = -sign;
  do{
    str[i++] = num%10 + '0';
    num = num/10;
  }while(num > 0);
  if(sign < 0)str[i++] = '-';
  str[i] = '\0';
  strcpy(buff,str);
  for(j=0;j<i;j++)
  {
    str[j] = buff[i-1-j];
  }
}
