/**
******************************************************************************
* @file    acaApp.c 
* @author  Walker Wang
* @version V1.0.0
* @date    09-April-2015
* @brief   Connect the arrayent cloud, recieved data and send out to UART. 
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
#include <string.h>
#include "MICOAppDefine.h"
#include "SocketUtils.h"
#include "micokit_ext.h"
#include "DHT11\DHT11.h"
#include "aca.h"

#define STACK_SIZE_UART_RECV_THREAD           0x2A0

// Device credentials and server addresses
#define DEVICE_NAME      "AXA00000703"
#define DEVICE_PASSWORD   "9G"
#define PPRODUCT_ID  		1101
#define LOAD_BALANCER1 		"axlp01lb01.cn.pub.arrayent.com"
#define LOAD_BALANCER2 		"axlp01lb02.cn.pub.arrayent.com"
#define LOAD_BALANCER3 		"axlp01lb03.cn.pub.arrayent.com"

#define DEVICE_AES_KEY      "6C794338547CD09531B5D1B18EE3C54D"
#define PRODUCT_AES_KEY     "7EFF77215F8BFB963AD45D571D2BF8BC"

#define aca_version_major ACA_VERSION_MAJOR
#define aca_version_minor ACA_VERSION_MINOR
#define aca_version_revision ACA_VERSION_REVISION
#define aca_version_subrevision ACA_VERSION_SUBREVISION


#define client_log(M, ...) custom_log("ACA app", M, ##__VA_ARGS__)
#define client_log_trace() custom_log_trace("ACA app")

#define UART_INTERFACE_VERSION "0.0.1"

static bool _wifiConnected = false;
static bool _connection_lost = true;
//static bool _response = false;
static mico_semaphore_t  _wifiConnected_sem = NULL;
uint16_t infrared_reflective_data = 0,light_sensor_data = 0;
uint8_t dht11_temp_data = 0,dht11_hum_data = 0;

#define ArrThreadSleep(msec)  mico_thread_msleep(msec) // assumes 1 milli per tick

/** Structure for registering uart commands */
struct uart_command {
  /** The name of the CLI command */
  const char *name;
  /** The function that should be invoked for this command. */
  void (*function) (char *property, mico_Context_t * const inContext);
};

uint8_t checkCmd(char* property, mico_Context_t *Context);
void refresh_command(char *property, mico_Context_t * const inContext);
void setCode_command(char *property, mico_Context_t * const inContext);
void setPwd_command(char *property, mico_Context_t * const inContext);
void setLightState_command(char *property, mico_Context_t * const inContext);
void setDCState_command(char *property, mico_Context_t * const inContext);
void setRGBState_command(char *property, mico_Context_t * const inContext);
void update_property(void *inContext);
void update_dht11_sensor(void);
void update_light_sensor(void);
void update_infrared_sensor(void);
void except_handle(void *inContext);
void itoa(int num,char *str);




static const struct uart_command uart_cmds[] = {
  {"config_refresh", refresh_command},
  {"config_arr_code",  setCode_command},
  {"config_arr_pwd",  setPwd_command},
  {"light", setLightState_command},
  {"DC",setDCState_command},
  {"RGB",setRGBState_command}
};

void notify_WifiStatusHandler(int event, mico_Context_t * const inContext)
{
  client_log_trace();
  (void)inContext;
  switch (event) {
  case NOTIFY_STATION_UP:
    _wifiConnected = true;
    mico_rtos_set_semaphore(&_wifiConnected_sem);
    break;
  case NOTIFY_STATION_DOWN:
    _wifiConnected = false;
    _connection_lost = true;
    break;
  default:
    break;
  }
  return;
}


void ArrayentConfigRestoreDefault(void *inContext){
  
  //mico_Context_t *Context = inContext;
  
  //strcpy(Context->flashContentInRam.appConfig.aca_config.code, DEVICE_NAME);
  //strcpy(Context->flashContentInRam.appConfig.aca_config.pwd, DEVICE_PASSWORD);
}


//Arrayent configuration
static void configureArrayent(void *inContext) 
{
  arrayent_config_t arrayent_config;
  //mico_Context_t *Context = inContext;
  
  if(ArrayentSetConfigDefaults(&arrayent_config) < 0) {
    client_log("Arrayent Set Config Defaults failed. \r\n");
    return;
  }
  
  arrayent_config.product_id = PPRODUCT_ID;
  arrayent_config.product_aes_key = PRODUCT_AES_KEY;
  arrayent_config.load_balancer_domain_names[0] = LOAD_BALANCER1;
  arrayent_config.load_balancer_domain_names[1] = LOAD_BALANCER2;
  arrayent_config.load_balancer_domain_names[2] = LOAD_BALANCER3;
  arrayent_config.load_balancer_udp_port = 80;
  arrayent_config.load_balancer_tcp_port = 80;
  //arrayent_config.device_name = Context->flashContentInRam.appConfig.uuid;
  //arrayent_config.device_password = Context->flashContentInRam.appConfig.aca_config.pwd;
  arrayent_config.device_name=DEVICE_NAME;
  arrayent_config.device_password=DEVICE_PASSWORD;
  arrayent_config.device_aes_key = DEVICE_AES_KEY;
  arrayent_config.aca_thread_priority = 7;
  arrayent_config.aca_stack_size = 2048;
  
  ArrayentConfigure(&arrayent_config);
}

void acaUartInterface_thread(void *inContext)
{
  client_log_trace();
  OSStatus err = kUnknownErr;
  arrayent_net_status_t arg;
  arrayent_return_e ret;
  //int8_t retry = 0;
  uint16_t len = 0;
  static char property[150];
  
  mico_rtos_init_semaphore(&_wifiConnected_sem, 1);//初始化信号量
  /* Regisist notifications */
  err = mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED, (void *)notify_WifiStatusHandler, (void *)inContext);//???
  require_noerr( err, exit );
  while(_wifiConnected == false){//after few time connected wifi success.???
    mico_rtos_get_semaphore(&_wifiConnected_sem, 200000);
    client_log("Waiting for Wi-Fi network...");
    mico_thread_sleep(1);
  }
  user_modules_init();
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, "Arrayent        ");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, "  Connecting... ");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, "                ");
  configureArrayent(inContext);
  
  //Initialize Arrayent stack
  client_log("Initializing Arrayent daemon...");
  ret = ArrayentInit();
  if(ret != ARRAYENT_SUCCESS) {
    client_log("Failed to initialize Arrayent daemon.%d", ret);
    return;
  }
  client_log("Arrayent daemon successfully initialized!");
  //Wait for Arrayent to finish connecting to the server
  do {
    ret = ArrayentNetStatus(&arg);//get connecting status to server
    if(ret != ARRAYENT_SUCCESS) {
      client_log("Failed to connect to Arrayent network.");
    }
    client_log("Waiting for good network status.");
    mico_thread_sleep(1);
  } while(!(arg.connected_to_server));//after few time connected server success.
  
  client_log("Connected to Arrayent network!\r\n");
  _connection_lost = false;
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, "Connected       ");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, "    Arrayent!   ");
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, "                ");
  
  ArrayentSetProperty("DC","0");
  mico_thread_msleep(200);
  ArrayentSetProperty("RGB","off");
  
  err = mico_rtos_create_thread(NULL, MICO_DEFAULT_LIBRARY_PRIORITY, "update", update_property, STACK_SIZE_UART_RECV_THREAD, (void*)inContext);
  require_noerr_action( err, exit, client_log("ERROR: Unable to start the update_property thread.") );
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "except", except_handle, STACK_SIZE_UART_RECV_THREAD, (void*)inContext);
  require_noerr_action( err, exit, client_log("ERROR: Unable to start the except_handle thread.") );
  while(1) {
    //sure wifi connected.
    if(_wifiConnected == false){
      mico_rtos_get_semaphore(&_wifiConnected_sem, 200000);
      continue;
    }
    //sure server connected.
    if(_connection_lost && _wifiConnected){
      do {
        ret = ArrayentNetStatus(&arg);//get connecting status to server
        if(ret != ARRAYENT_SUCCESS) {
          client_log("Failed to connect to Arrayent network.");
        }
        client_log("Waiting for good network status.");
        mico_thread_sleep(1);
      } while(!(arg.connected_to_server));
      _connection_lost = false;
    }
    
    len = sizeof(property);
    if(ARRAYENT_SUCCESS==ArrayentRecvProperty(property,&len,10000)){//receive property data from server
      client_log("property = %s\n\r",property);
      if(0!=checkCmd(property, inContext)){//parse cmd success
        //_response = false;
        //retry = 1;
        client_log("%s", property);
      }
    }
  }
  
exit:
  client_log("Exit: ACA App exit with err = %d", err);
  mico_rtos_delete_thread(NULL);
  return;
}


uint8_t checkCmd(char* property, mico_Context_t *Context){
  
  int i = 0;
  int nameLen=0;
  //int len = 0;
  
  //len = sizeof(uart_cmds)/sizeof(uart_cmds[0]);//len = 4
  for(i=0;i < sizeof(uart_cmds)/sizeof(uart_cmds[0]); i++){
    
    nameLen = strlen(uart_cmds[i].name);
    //client_log("property[%d]: %c result=%d\r\n", nameLen, property[nameLen],strncmp(uart_cmds[i].name, property, nameLen) );
    if(0==strncmp(uart_cmds[i].name, property, nameLen)
       &&property[nameLen] == ' ' ){//compare command and run.
         uart_cmds[i].function(property, Context);
         return 0;
       }
    
  }
  
  return -1;
  
}

void refresh_command(char *property, mico_Context_t * const inContext){
  //ArrayentSetProperty("config_arr_code",inContext->flashContentInRam.appConfig.aca_config.code);
  //ArrayentSetProperty("config_arr_pwd",inContext->flashContentInRam.appConfig.aca_config.pwd);
}

void setCode_command(char *property, mico_Context_t * const Context){
  
/*  OSStatus err = kNoErr;
  uint32_t paraStartAddress, paraEndAddress;
    
  paraStartAddress = PARA_START_ADDRESS;
  paraEndAddress = PARA_END_ADDRESS;
  
  char* cmd=strtok(property, " ");
  char* value=strtok(NULL, " ");
  
  if(value){
  //strcpy(Context->flashContentInRam.appConfig.aca_config.code, value);
  err = MicoFlashInitialize(MICO_FLASH_FOR_PARA);
  require_noerr(err, exit);
  err = MicoFlashErase(MICO_FLASH_FOR_PARA, paraStartAddress, paraEndAddress);
  require_noerr(err, exit);
  err = MicoFlashWrite(MICO_FLASH_FOR_PARA, &paraStartAddress, (void *)Context, sizeof(flash_content_t));
  require_noerr(err, exit);
  err = MicoFlashFinalize(MICO_FLASH_FOR_PARA);
  require_noerr(err, exit);
}
  
  exit:
  */
}

void setPwd_command(char *property, mico_Context_t * const Context){
/*  OSStatus err = kNoErr;
  uint32_t paraStartAddress, paraEndAddress;
  
  paraStartAddress = PARA_START_ADDRESS;
  paraEndAddress = PARA_END_ADDRESS;
  
  char* cmd=strtok(property, " ");
  char* value=strtok(NULL, " ");
  
  if(value){
  //strcpy(Context->flashContentInRam.appConfig.aca_config.pwd, value);
  err = MicoFlashInitialize(MICO_FLASH_FOR_PARA);
  require_noerr(err, exit);
  err = MicoFlashErase(MICO_FLASH_FOR_PARA, paraStartAddress, paraEndAddress);
  require_noerr(err, exit);
  err = MicoFlashWrite(MICO_FLASH_FOR_PARA, &paraStartAddress, (void *)Context, sizeof(flash_content_t));
  require_noerr(err, exit);
  err = MicoFlashFinalize(MICO_FLASH_FOR_PARA);
  require_noerr(err, exit);
}
  
  exit:
  */
  
}

void setLightState_command(char *property, mico_Context_t * const inContext)
{
  if(0==strcmp(property, "light 0")){
    MicoGpioOutputLow( (mico_gpio_t)MICO_SYS_LED );
  }else if(0==strcmp(property, "light 1")){
    MicoGpioOutputHigh( (mico_gpio_t)MICO_SYS_LED );
  }
}


void setDCState_command(char *property, mico_Context_t * const inContext)
{
  if(0==strcmp(property, "DC 1")){//直流电机转动
    dc_motor_set(1);
  }else if(0==strcmp(property, "DC 0")){
    dc_motor_set(0);
  } 
}


void setRGBState_command(char *property, mico_Context_t * const inContext){
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

void update_property(void *inContext){
  int num = 0;
  while(1)
  {
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
      printf("service have rejected!\n\r");
    }
    mico_thread_msleep(200);
    if(ARRAYENT_SUCCESS!=ArrayentSetProperty("hsv",hum_value))
    {
      printf("service have rejected!\n\r");
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
      printf("service have rejected!\n\r");
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
      printf("service have rejected!\n\r");
    }
    
  }
  
}

void except_handle(void *inContext)
{
  char str[20];
  while(1)
  {
    if(dht11_temp_data >= 32)
    {
      hsb2rgb_led_open(0, 100, 50);
      mico_thread_msleep(500);
      hsb2rgb_led_open(0, 0, 0);
      mico_thread_msleep(500);
    }
    sprintf(str, "T:%3.1fC H:%3.1f%%",(float)dht11_temp_data, (float)dht11_hum_data);
    OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_4, (uint8_t*)str);
    memset(str,0,20);
    sprintf(str, "infrared: %d",infrared_reflective_data);
    OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, (uint8_t*)str);
    memset(str,0,20);
    sprintf(str, "light:    %d",light_sensor_data);
    OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)str);
  }
}


void itoa(int num,char *str)
{
  int i = 0,j = 0,sign = 0;
  char buff[10];
  
  sign = num;
  if(sign < 0)sign = -sign;//如果是负数，变为整正数
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



