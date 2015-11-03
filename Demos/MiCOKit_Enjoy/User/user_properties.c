/**
******************************************************************************
* @file    user_properties.c 
* @author  Eshen Wang
* @version V1.0.0
* @date    18-Mar-2015
* @brief   device properties data.
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
#include "properties.h"
#include "user_properties.h"

#include "micokit_ext.h"
#include "user_uart.h"

#define properties_user_log(M, ...) custom_log("DEV_PROPERTIES_USER", M, ##__VA_ARGS__)
#define properties_user_log_trace() custom_log_trace("DEV_PROPERTIES_USER")


/*******************************************************************************
*                               VARIABLES
*******************************************************************************/
// length of system data type (fixed)
uint32_t bool_len = sizeof(bool);
uint32_t int_len = sizeof(int);
uint32_t float_len = sizeof(float);

bool property_event = true;    // all event value is true.

/*------------------------------------------------------------------------------
 * user context
 * context.config: user property data, stored in flash extra param area
 * context.status: user property status
 *----------------------------------------------------------------------------*/
user_context_t g_user_context = {
  .config.dev_name = DEFAULT_DEVICE_NAME,
  .config.dev_name_len = 11,
//  .config.dev_manufacturer = DEFAULT_MANUFACTURER,
//  .config.dev_manufacturer_len = 6,
//  .status.user_config_need_update = false,
  .status.oled_keep_s = 0,    // oled display keep time(ms), 0 means no keep
  
  .config.rgb_led_sw = false,
  .config.rgb_led_hues = 0,
  .config.rgb_led_saturation = 100,
  .config.rgb_led_brightness = 50,
};


/*******************************************************************************
 * DESCRIPTION: get/set/notify_check function defined for each property.
 * NOTE:        a property must has get/set function to read/write property, and
 *              must has notify_check function, if property need notify.
 *              get: get && return hardware status, reutrn 0 if succeed.
 *              set: set hardware, return 0 if operation succeed.
 *              notify_check: check property changes, returned changed value && set notify.
 * RETURN:
 *              = 0: get/set success;
 *              = 1: returned by notify_check function, property need to update;
 *              < 0: operation failed.
 ******************************************************************************/

/*------------------------------------------------------------------------------
 *                              MODULE: dev_info 
 *----------------------------------------------------------------------------*/

// (COMMON FUNCTION) string property read function
int string_get(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  int ret = 0;
  
  if((NULL == prop) || (NULL == val)){
    return -1;
  }
  
  // no hardware operation, just return ok.
  
  properties_user_log("string_get: val=%s, val_len=%d.", (char*)val, *val_len);
  
  return ret;  // return 0, succeed.
}

// (COMMON FUNCTION) string property write function
int string_set(struct mico_prop_t *prop, void *arg, void *val, uint32_t val_len)
{
  int ret = 0;
  
  if(NULL == prop){
    return -1;
  }
  
  // no hardware operation, but string length must ok.
  if(val_len > prop->maxStringLen){
    return -1;
  }
  
  properties_user_log("string_set: val=%s, val_len=%d.", (char*)prop->value, *(prop->value_len));
  
  return ret;  // return 0, succeed.
}

/*------------------------------------------------------------------------------
 *                              MODULE: RGB LED
 *----------------------------------------------------------------------------*/

// swtich set function
int rgb_led_sw_set(struct mico_prop_t *prop, void *arg, void *val, uint32_t val_len)
{
  int ret = 0;
  bool set_sw_state = *((bool*)val);
  
  user_context_t *uct = (user_context_t*)arg;
  //properties_user_log("rgb_led_sw_set: val=%d, val_len=%d.", *((bool*)val), val_len);
  //properties_user_log("h=%d, s=%d, b=%d", uct->config.rgb_led_hues, uct->config.rgb_led_saturation, uct->config.rgb_led_brightness);
  
  // control hardware
  if(set_sw_state){
    properties_user_log("Open LED.");
    hsb2rgb_led_open((float)uct->config.rgb_led_hues, (float)uct->config.rgb_led_saturation, (float)uct->config.rgb_led_brightness);
  }
  else{
    properties_user_log("Close LED.");
    hsb2rgb_led_close();
  }
  
  return ret;  // return 0, succeed.
}

// swtich get function
int rgb_led_sw_get(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  int ret = 0;
  
  // no hardware status to read, just return prop->value
  *((bool*)val) = *((bool*)prop->value);  
  *val_len = *(prop->value_len);
  
  properties_user_log("rgb_led_sw_get: val=%d, val_len=%d.", *((bool*)val), *((uint32_t*)val_len) );
  
  return ret;  // return 0, succeed.
}

// hues set function
int rgb_led_hues_set(struct mico_prop_t *prop, void *arg, void *val, uint32_t val_len)
{
  int ret = 0;
  int hues = *((int*)val);
  user_context_t *uct = (user_context_t*)arg;
  
  properties_user_log("rgb_led_hues_set: val=%d, val_len=%d.", *((int*)val), val_len);
  
  // control hardware
  if(uct->config.rgb_led_sw){
    hsb2rgb_led_open((float)hues, (float)uct->config.rgb_led_saturation, (float)uct->config.rgb_led_brightness);
  }
  else{
    hsb2rgb_led_close();
  }
  
  return ret;  // return 0, succeed.
}

// hues get function
int rgb_led_hues_get(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  int ret = 0;
  
  // get hardware status, no hardware status to read, just return prop->value
  *((int*)val) = *((int*)prop->value);  
  *val_len = *(prop->value_len);
  
  properties_user_log("rgb_led_hues_get: val=%d, val_len=%d.", *((int*)val), *(uint32_t*)val_len );
  
  return ret;  // return 0, succeed.
}

// saturation set function
int rgb_led_saturation_set(struct mico_prop_t *prop, void *arg, void *val, uint32_t val_len)
{
  int ret = 0;
  int saturation = *((int*)val);
  user_context_t *uct = (user_context_t*)arg;
  
  properties_user_log("rgb_led_saturation_set: val=%d, val_len=%d.", *((int*)val), val_len);
  
  // control hardware
  if(uct->config.rgb_led_sw){
    hsb2rgb_led_open((float)uct->config.rgb_led_hues, (float)saturation, (float)uct->config.rgb_led_brightness);
  }
  else{
    hsb2rgb_led_close();
  }
  
  return ret;  // return 0, succeed.
}

// saturation get function
int rgb_led_saturation_get(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  int ret = 0;
  
  // get hardware status, no hardware status to read, just return prop->value
  *((int*)val) = *((int*)prop->value);  
  *val_len = *(prop->value_len);
  
  properties_user_log("rgb_led_saturation_get: val=%d, val_len=%d.", *((int*)val), *(uint32_t*)val_len );
  
  return ret;  // return 0, succeed.
}

// brightness set function
int rgb_led_brightness_set(struct mico_prop_t *prop, void *arg, void *val, uint32_t val_len)
{
  int ret = 0;
  int brightness = *((int*)val);
  user_context_t *uct = (user_context_t*)arg;
  
  properties_user_log("rgb_led_brightness_set: val=%d, val_len=%d.", *((int*)val), val_len);
  
  // control hardware
  if(uct->config.rgb_led_sw){
    hsb2rgb_led_open((float)uct->config.rgb_led_hues, (float)uct->config.rgb_led_saturation, (float)brightness);
  }
  else{
    hsb2rgb_led_close();
  }
  
  return ret;  // return 0, succeed.
}

// brightness get function
int rgb_led_brightness_get(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  int ret = 0;
  
  // get hardware status, no hardware status to read, just return prop->value
  *((int*)val) = *((int*)prop->value);  
  *val_len = *(prop->value_len);
  
  properties_user_log("rgb_led_brightness_get: val=%d, val_len=%d.", *((int*)val), *(uint32_t*)val_len );
  
  return ret;  // return 0, succeed.
}

/*------------------------------------------------------------------------------
 *                            MODULE: light sensor
 *----------------------------------------------------------------------------*/

// get function: get light sensor data 
int light_sensor_data_get(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  int ret = 0;
  uint16_t light_sensor_data = 0;
  
  // get light sensor data
  ret = light_sensor_read(&light_sensor_data);
  if(0 == ret){  // get data succeed
    // unit conversion (test)
    light_sensor_data = 4095 - light_sensor_data;
    *((uint16_t*)val) = light_sensor_data;
    *val_len = sizeof(light_sensor_data);
  }
  
  return ret;
}

// notify check function: check changes for light sensor data
int notify_check_light_sensor_data(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  int ret = 0;
  uint16_t light_sensor_data = 0;
  uint32_t light_sensor_data_len = 0;
  
  // get adc data
  ret = prop->get(prop, arg, &light_sensor_data, &light_sensor_data_len);
  if(0 != ret){
    return -1;   // get value error
  }
  
  // update check (diff get_data and prop->value)
  //if(light_sensor_data != *((uint16_t*)(prop->value))){  // changed
  if( (((int)light_sensor_data - *((int*)(prop->value))) >= 50) || 
     ((*((int*)(prop->value)) - (int)light_sensor_data) >= 50) ){  // abs >=10
    properties_user_log("light_sensor_data changed: %d -> %d", *((int*)prop->value), (int)light_sensor_data);   
    // return new value to update prop value && len
    *((int*)val) = (int)light_sensor_data;  
    *val_len = light_sensor_data_len;
    ret = 1;  // value changed, need to send notify message
  }
  else{
    ret = 0;  // not changed, not need to notify
  }
  
  return ret;
}

/*------------------------------------------------------------------------------
 *                     MODULE: infrared reflective sensor
 *----------------------------------------------------------------------------*/

// get function: get infrared reflective data
int infrared_reflective_data_get(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  int ret = 0;
  uint16_t infrared_reflective_data = 0;
  
  // get infrared sensor data
  ret = infrared_reflective_read(&infrared_reflective_data);
  if(0 == ret){  // get data succeed
    // uint conversion (test)
    infrared_reflective_data = 4095 - infrared_reflective_data;
    *((uint16_t*)val) = infrared_reflective_data;
    *val_len = sizeof(infrared_reflective_data); 
  }
  
  return ret;
}

// notify check function for infrared reflective data
int notify_check_infrared_reflective_data(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  int ret = 0;
  uint16_t infrared_reflective_data = 0;
  uint32_t infrared_reflective_data_len = 0;
  
  // get adc data
  ret = prop->get(prop, arg, &infrared_reflective_data, &infrared_reflective_data_len);
  if(0 != ret){
    return -1;   // get value error
  }
  
  // update check (diff get_data and prop->value)
  //if(infrared_reflective_data != *((uint16_t*)(prop->value))){  // changed
  if( (((int)infrared_reflective_data - *((int*)(prop->value))) >= 50) || 
     ((*((int*)(prop->value)) - (int)infrared_reflective_data) >= 50) ){  // abs >=10
    properties_user_log("infrared_reflective_data changed: %d -> %d",
                        *((int*)prop->value), (int)infrared_reflective_data);   
    // return new value to update prop value && len
    *((int*)val) = (int)infrared_reflective_data;  
    *val_len = infrared_reflective_data_len;
    ret = 1;  // value changed, need to send notify message
  }
  else{
    ret = 0;  // not changed, not need to notify
  }
  
  return ret;
}

/*------------------------------------------------------------------------------
 *                                MODULE: UART 
 *-----------------------------------------------------------------------------*/

// get function: recv uart data
int uart_data_recv(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  int ret = 0;  
  uint32_t recv_len = 0;
  char string_display_on_oled[MAX_USER_UART_BUF_SIZE+1] = {'\0'};
  
  if((NULL == val) || (NULL == val_len) || (NULL == prop) || (NULL == arg)){
    return -1;
  }
  
  user_context_t *uct = (user_context_t*)arg;
  
  // recv data from uart
  memset(val, '\0', prop->maxStringLen);
  recv_len = user_uartRecv((unsigned char*)val, prop->maxStringLen);
  if(recv_len > 0){
    *val_len = recv_len;
    ret = 0;
    properties_user_log("uart_data_recv: [%d][%*.s]", *val_len, *val_len, (char*)val);
    
    // display string on OLED (max_len=32bytes)
    properties_user_log("Show uart recv msg on OLED: [%d][%*.s]", *val_len, *val_len, (char*)val);
    OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)"To Cloud:       ");
    memset(string_display_on_oled, ' ', MAX_USER_UART_BUF_SIZE);  // clean display char
    memcpy(string_display_on_oled, (uint8_t*)val, *val_len);
    string_display_on_oled[MAX_USER_UART_BUF_SIZE] = '\0';  // display end
    OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, (uint8_t*)string_display_on_oled);
    ret = 0;  // set always ok
    uct->status.oled_keep_s = 3;   // display 3s  
  }
  else{
    *val_len = 0;
    ret = -1;
  }
  
  return ret;
}

// set function: send data to uart && OLED
int uart_data_send(struct mico_prop_t *prop, void *arg, void *val, uint32_t val_len)
{
  int ret = 0;
  OSStatus err = kUnknownErr;
  uint32_t send_len = 0;
  char string_display_on_oled[MAX_USER_UART_BUF_SIZE+3] = {'\0'};  // add CR/LF && '\0' to the end for display on uart
  
  if((NULL == val) || (NULL == val_len) || (NULL == prop) || (NULL == arg)){
    return -1;
  }
  
  user_context_t *uct = (user_context_t*)arg;
  
  // display string on OLED (max_len=32bytes)
  send_len = ((val_len > prop->maxStringLen) ? prop->maxStringLen : val_len);
  properties_user_log("Show cloud msg on OLED: [%d][%*.s]", send_len, send_len, (char*)val);
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_2, (uint8_t*)"From Cloud:     ");
  memset(string_display_on_oled, ' ', MAX_USER_UART_BUF_SIZE);  // clean display char
  memcpy(string_display_on_oled, (uint8_t*)val, send_len);
  string_display_on_oled[MAX_USER_UART_BUF_SIZE] = '\0';  // display end
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, (uint8_t*)string_display_on_oled);
  ret = 0;  // set always ok
  uct->status.oled_keep_s = 3;   // display 3s
  
  // send data to uart, add CR/LF to the end for display.
  string_display_on_oled[send_len] = '\r';
  string_display_on_oled[send_len+1] = '\n';  // add CR LF for display on uart
  string_display_on_oled[send_len+2] = '\0';  // string end
  
  err = user_uartSend((unsigned char*)string_display_on_oled, send_len+2);
  if(kNoErr == err){
    properties_user_log("uart_data_send: [%d][%s]", send_len+2, (char*)string_display_on_oled);
    ret = 0;  // set ok
  }
  else{
    properties_user_log("ERROR: uart_data_send, err=%d.", err);
    ret = -1; // set failed
  }
  
  return ret;
}

// notify_check function: check uart data recv
int uart_data_recv_check(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  int ret = 0;
  
  // get adc data
  ret = prop->get(prop, arg, val, val_len);
  if(0 != ret){
    return -1;   // get value error
  }
  
  // need notify (uart data recieved)
  ret = 1;
  
  return ret;
}

/*------------------------------------------------------------------------------
 *                                DC Motor
 *----------------------------------------------------------------------------*/

// get function: get dc motor switch value 
int dc_motor_switch_get(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  *val_len = int_len;
  
  if(MicoGpioInputGet( (mico_gpio_t)DC_MOTOR ) == 0){
    *(int*)val = 0;
  }
  else{
     *(int*)val = 1;
  }
  
  return 0;  // get ok
}

// set function: set dc motor switch value 
int dc_motor_switch_set(struct mico_prop_t *prop, void *arg, void *val, uint32_t val_len)
{
  int value = 0;
  
  value = *((int*)val);
  dc_motor_set(value);

  return 0;  // set ok
}

/*------------------------------------------------------------------------------
 *                          Temperature && Humidity
 *----------------------------------------------------------------------------*/

// get function: get temperature value 
int temperature_get(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  OSStatus err = kUnknownErr;
//  uint8_t ret = 0;
  
  int32_t temp_data = 0;
  uint32_t hum_data = 0;
  
  user_context_t *uct = (user_context_t*)arg;
  
  *val_len = int_len;
  
//  ret = DHT11_Read_Data(&temp_data, &hum_data);
//  if((0 != ret) || (0 == temp_data)){
//    properties_user_log("ERROR: DHT11_Read_Data error.");
//    return -1;
//  }
//  else{
//     *((int*)val) = (int)temp_data;
//     //NOTE: we also save humidity value here, because we could not read humidity after temperature within 1s.
//     uct->status.humidity_saved = hum_data;
//  }
  
  err = temp_hum_sensor_read(&temp_data, &hum_data);
  if((kNoErr != err) || ((0 == temp_data) && (0 == hum_data))){
    //properties_user_log("ERROR: temp_hum_sensor_read error, err=%d.", err);
    return -1;
  }
  else{
     *((int*)val) = (int)temp_data;
     //NOTE: we also save humidity value here, because we could not read humidity after temperature within 1s.
     uct->status.humidity_saved = hum_data;
  }
  
  return 0;  // get ok
}

// notify check function for temperature data changes
int notify_check_temperature(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  int ret = 0;
  int temperature_data = 0;
  uint32_t temperature_data_len = 0;
  
  // get adc data
  ret = prop->get(prop, arg, &temperature_data, &temperature_data_len);
  if(0 != ret){
    return -1;   // get value error
  }
  
  // update check (diff get_data and prop->value)
  if(temperature_data != *((int*)(prop->value))){  // changed
//  if( (((int)temperature_data - *((int*)(prop->value))) >= 5) || 
//     ((*((int*)(prop->value)) - (int)temperature_data) >= 5) ){  // abs >=5
    properties_user_log("temperature changed: %d -> %d",
                        *((int*)prop->value), temperature_data);
    
    // return new value to update prop value && len
    *((int*)val) = temperature_data;
    *val_len = temperature_data_len;
    ret = 1;  // value changed, need to send notify message
  }
  else{
    ret = 0;  // not changed, not need to notify
  }
  
  return ret;
}

// get function: get humidity value 
int humidity_get(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
//  uint8_t ret = 0;
//  uint8_t temp_data = 0;
//  uint8_t hum_data = 0;
  
  user_context_t *uct = (user_context_t*)arg;
  
  *val_len = int_len;
  
 /* NOTE: here because DHT11 sample rate < 1HZ, so we could not read humidity after read tepmerature within 1s,
    so we just use the humidity value saved when got temperatue, see function: temperature_get

  ret = DHT11_Read_Data(&temp_data, &hum_data);
  if(0 != ret){
    properties_user_log("ERROR: DHT11_Read_Data error!");
    return -1;
  }
  else{
     *((int*)val) = (int)hum_data;
  }
  
  */
  
   *((int*)val) = uct->status.humidity_saved;
  
  return 0;  // get ok
}

// notify check function for humidity data changes
int notify_check_humidity(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  int ret = 0;
  int humidity_data = 0;
  uint32_t humidity_data_len = 0;
  
  // get adc data
  ret = prop->get(prop, arg, &humidity_data, &humidity_data_len);
  if(0 != ret){
    return -1;   // get value error
  }
  
  // update check (diff get_data and prop->value)
  if(humidity_data != *((int*)(prop->value))){  // changed
//  if( (((int)humidity_data - *((int*)(prop->value))) >= 50) || 
//     ((*((int*)(prop->value)) - (int)humidity_data) >= 50) ){  // abs >=50
    properties_user_log("humidity changed: %d -> %d",
                        *((int*)prop->value), (int)humidity_data);
    
    // return new value to update prop value && len
    *((int*)val) = (int)humidity_data;  
    *val_len = humidity_data_len;
    ret = 1;  // value changed, need to send notify message
  }
  else{
    ret = 0;  // not changed, not need to notify
  }
  
  return ret;
}

#ifdef USE_COMMON_MODULE
/*------------------------------------------------------------------------------
 *                               Common module
 *----------------------------------------------------------------------------*/

// get function: get value 
int common_module_value_get(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  *val_len = int_len;
  
  // get current status from your hardware and return to val
  *(int*)val = *((int*)prop->value);  // here just return current prop value
  
  return 0;  // get ok
}

// set function: set value 
int common_module_value_set(struct mico_prop_t *prop, void *arg, void *val, uint32_t val_len)
{
  int value = 0;
  
  // use value(param: val) operate your hardware here and return result
  value = *((int*)val);
  UNUSED_PARAMETER(value);
  
  return 0;  // set ok
}
#endif  // USE_COMMON_MODULE


/*******************************************************************************
 * service_table: list all serivices && properties for the device
 ******************************************************************************/
const struct mico_service_t  service_table[] = {
  [0] = {
    .type = "public.map.service.dev_info",    // service 1: dev_info (uuid)
    .properties = {
      [0] = {
        .type = "public.map.property.name",  // device name uuid
        .value = &(g_user_context.config.dev_name),
        .value_len = &(g_user_context.config.dev_name_len),
        .format = MICO_PROP_TYPE_STRING,
        .perms = (MICO_PROP_PERMS_RO | MICO_PROP_PERMS_WO),
        .get = string_get,                  // get string func to get device name
        .set = string_set,                  // set sring func to change device name
        .notify_check = NULL,               // not notifiable
        .arg = &(g_user_context.config.dev_name),            // get/set string pointer (device name)
        .event = NULL,                      // not notifiable
        .hasMeta = false,                   // no max/min/step
        .maxStringLen = MAX_DEVICE_NAME_SIZE,  // max length of device name string
      },
      [1] = {NULL}                          // end flag
    }
  },
  [1] = {
    .type = "public.map.service.rgb_led",       // service 2: rgb led (uuid)
    .properties = {
      [0] = {
        .type = "public.map.property.switch",  // led switch uuid
        .value = &(g_user_context.config.rgb_led_sw),
        .value_len = &bool_len,                // bool type len
        .format = MICO_PROP_TYPE_BOOL,
        .perms = (MICO_PROP_PERMS_RO | MICO_PROP_PERMS_WO),
        .get = rgb_led_sw_get,              // get led switch status function
        .set = rgb_led_sw_set,              // set led switch status function
        .notify_check = NULL,               // not notifiable
        .arg = &g_user_context,               // user context
        .event = NULL,
        .hasMeta = false,
      },
      [1] = {
        .type = "public.map.property.hues",  // led hues
        .value = &(g_user_context.config.rgb_led_hues),
        .value_len = &int_len,               // int type len
        .format = MICO_PROP_TYPE_INT,
        .perms = (MICO_PROP_PERMS_RO | MICO_PROP_PERMS_WO),
        .get = rgb_led_hues_get,
        .set = rgb_led_hues_set,
        .notify_check = NULL,               // not notifiable
        .arg = &g_user_context,               // user context
        .event = NULL,
        .hasMeta = true,
        .maxValue.intValue = 360,
        .minValue.intValue = 0,
        .minStep.intValue = 1
      },
      [2] = {
        .type = "public.map.property.saturation",  // led saturation
        .value = &(g_user_context.config.rgb_led_saturation),
        .value_len = &int_len,
        .format = MICO_PROP_TYPE_INT,
        .perms = (MICO_PROP_PERMS_RO | MICO_PROP_PERMS_WO),
        .get = rgb_led_saturation_get,
        .set = rgb_led_saturation_set,
        .notify_check = NULL,                     // not notifiable
        .arg = &g_user_context,                     // user context
        .event = NULL,
        .hasMeta = true,
        .maxValue.intValue = 100,
        .minValue.intValue = 0,
        .minStep.intValue = 1
      },
      [3] = {
        .type = "public.map.property.brightness",  // led brightness
        .value = &(g_user_context.config.rgb_led_brightness),
        .value_len = &int_len,
        .format = MICO_PROP_TYPE_INT,
        .perms = (MICO_PROP_PERMS_RO | MICO_PROP_PERMS_WO),
        .get = rgb_led_brightness_get,
        .set = rgb_led_brightness_set,
        .arg = &g_user_context,                      // user context
        .event = NULL,
        .hasMeta = true,
        .maxValue.intValue = 100,
        .minValue.intValue = 0,
        .minStep.intValue = 1,
      },
      [4] = {NULL}
    }
  },
  [2] = {
    .type = "public.map.service.light_sensor",         //  service 3: light sensor (ADC)
    .properties = {
      [0] = {
        .type = "public.map.property.value", 
        .value = &(g_user_context.status.light_sensor_data),  // light_sensor_data value
        .value_len = &int_len,
        .format = MICO_PROP_TYPE_INT,
        .perms = (MICO_PROP_PERMS_RO | MICO_PROP_PERMS_EV),
        .get = light_sensor_data_get,
        .set = NULL,
        .notify_check = notify_check_light_sensor_data,  // check notify for adc data
        .arg = &g_user_context,                          // user context
        .event = &(property_event),
        .hasMeta = false
      },
      [1] = {NULL}
    }
  },
  [3] = {
    .type = "public.map.service.uart",          //  service 3: uart (uuid)
    .properties = {
      [0] = {
        .type = "public.map.property.message",
        .value = &(g_user_context.status.uart_rx_buf),   // uart message buffer
        .value_len = &(g_user_context.status.uart_rx_data_len),
        .format = MICO_PROP_TYPE_STRING,
        .perms = ( MICO_PROP_PERMS_WO | MICO_PROP_PERMS_EV ),
        .get = uart_data_recv,
        .set = uart_data_send,
        .notify_check = uart_data_recv_check,   // check recv data
        .arg = &g_user_context,
        .event = &(property_event),
        .hasMeta = false,
        .maxStringLen = MAX_USER_UART_BUF_SIZE
      },
      [1] = {NULL}
    }
  },
  [4] = {
    .type = "public.map.service.motor",       // service 5: dc motor (uuid)
    .properties = {
      [0] = {
        .type = "public.map.property.value",  // dc motor switch value
        .value = &(g_user_context.config.dc_motor_switch),
        .value_len = &int_len,                // int type len
        .format = MICO_PROP_TYPE_INT,
        .perms = (MICO_PROP_PERMS_RO | MICO_PROP_PERMS_WO),
        .get = dc_motor_switch_get,              // get switch status function
        .set = dc_motor_switch_set,              // set switch status function
        .notify_check = NULL,                    // not notifiable
        .arg = &g_user_context,                    // user context
        .event = NULL,
        .hasMeta = false
      },
      [1] = {NULL}
    }
  },
  [5] = {
    .type = "public.map.service.infrared",    //  service 4: infrared sensor (ADC)
    .properties = {
      [0] = {
        .type = "public.map.property.value",
        .value = &(g_user_context.status.infrared_reflective_data),  // infrared value
        .value_len = &int_len,
        .format = MICO_PROP_TYPE_INT,
        .perms = (MICO_PROP_PERMS_RO | MICO_PROP_PERMS_EV),
        .get = infrared_reflective_data_get,
        .set = NULL,
        .notify_check = notify_check_infrared_reflective_data,  // check notify for infrared data
        .arg = &g_user_context,                   // user context
        .event = &(property_event),             // event flag
        .hasMeta = false
      },
      [1] = {NULL}
    }
  },
  [6] = {
    .type = "public.map.service.temperature",     // service 6: temperature (uuid)
    .properties = {
      [0] = {
        .type = "public.map.property.value",  // temperature value
        .value = &(g_user_context.status.temperature),
        .value_len = &int_len,                // int type len
        .format = MICO_PROP_TYPE_INT,
        .perms = (MICO_PROP_PERMS_RO | MICO_PROP_PERMS_EV),
        .get = temperature_get,              // get temperature function
        .set = NULL,
        .notify_check = notify_check_temperature,
        .arg = &g_user_context,                // user context
        .event = &property_event,
        .hasMeta = false
      },
      [1] = {NULL}
    }
  },
  [7] = {
    .type = "public.map.service.humidity",     // service 7: humidity (uuid)
    .properties = {
      [0] = {
        .type = "public.map.property.value",  // humidity value
        .value = &(g_user_context.status.humidity),
        .value_len = &int_len,                // int type len
        .format = MICO_PROP_TYPE_INT,
        .perms = (MICO_PROP_PERMS_RO | MICO_PROP_PERMS_EV),
        .get = humidity_get,              // get humidity function
        .set = NULL,
        .notify_check = notify_check_humidity,
        .arg = &g_user_context,                // user context
        .event = &property_event,
        .hasMeta = false
      },
      [1] = {NULL}
    }
  },
#ifdef USE_COMMON_MODULE
    [8] = {
    .type = "public.map.service.common",     // service 8: common module
    .properties = {
      [0] = {
        .type = "public.map.property.value",  // common value1
        .value = &(g_user_context.status.common_module_value1),
        .value_len = &int_len,                // int type len
        .format = MICO_PROP_TYPE_INT,
        .perms = (MICO_PROP_PERMS_RO | MICO_PROP_PERMS_WO),
        .get = common_module_value_get,              // get value function
        .set = common_module_value_set,              // set value function
        .notify_check = NULL,
        .arg = &g_user_context,                // user context
        .event = NULL,
        .hasMeta = false
      },
      [1] = {
        .type = "public.map.property.value",  // common value2
        .value = &(g_user_context.status.common_module_value2),
        .value_len = &int_len,                // int type len
        .format = MICO_PROP_TYPE_INT,
        .perms = (MICO_PROP_PERMS_RO | MICO_PROP_PERMS_WO),
        .get = common_module_value_get,              // get value function
        .set = common_module_value_set,              // set value function
        .notify_check = NULL,
        .arg = &g_user_context,                // user context
        .event = NULL,
        .hasMeta = false
      },
      [2] = {
        .type = "public.map.property.value",  // common value3
        .value = &(g_user_context.status.common_module_value3),
        .value_len = &int_len,                // int type len
        .format = MICO_PROP_TYPE_INT,
        .perms = (MICO_PROP_PERMS_RO | MICO_PROP_PERMS_WO),
        .get = common_module_value_get,              // get value function
        .set = common_module_value_set,              // set value function
        .notify_check = NULL,
        .arg = &g_user_context,                // user context
        .event = NULL,
        .hasMeta = false
      },
      [4] = {NULL}
    }
  },
  [9] = {NULL}
#else  // ! USE_COMMON_MODULE
  [8] = {NULL}
#endif
};

/************************************ FILE END ********************************/