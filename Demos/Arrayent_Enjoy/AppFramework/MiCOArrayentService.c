/**
******************************************************************************
* @file    MiCOArrayentService.c 
* @author  Eshen Wang
* @version V1.0.0
* @date    17-Mar-2015
* @brief   This file contains the implementations of cloud service interfaces 
*          for MICO.
operation
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
#include "aca.h"
#include "MiCOArrayentService.h"


#define arrayent_log(M, ...) custom_log("MiCO_ARRAYENT", M, ##__VA_ARGS__)
#define arrayent_log_trace() custom_log_trace("MiCO_ARRAYENT")


/*******************************************************************************
 *                                  DEFINES
 ******************************************************************************/


/*******************************************************************************
 *                                  VARIABLES
 ******************************************************************************/
static mico_semaphore_t  _wifiConnected_sem = NULL;

/*******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************/
/* cloud status delegate functions, overwite by user in MiCOConfigDelegate.c */
WEAK void mico_cloud_connected_delegate(app_context_t * const inContext)
{
  return;
}

WEAK void mico_cloud_disconnected_delegate(app_context_t * const inContext)
{
  return;
}

void acaNotify_WifiStatusHandler(WiFiEvent event, app_context_t * const inContext)
{
  arrayent_log_trace();
  (void)inContext;
  switch (event) {
  case NOTIFY_STATION_UP:
    arrayent_log("wifi station up.");
    inContext->appStatus.isWifiConnected = true;
    if(NULL != _wifiConnected_sem){
      mico_rtos_set_semaphore(&_wifiConnected_sem);
    }
    break;
  case NOTIFY_STATION_DOWN:
    arrayent_log("wifi station down.");
    inContext->appStatus.isWifiConnected = false;
    inContext->appStatus.arrayentStatus.isCloudConnected = false;
    break;
  case NOTIFY_AP_UP:
    break;
  case NOTIFY_AP_DOWN:
    break;
  default:
    break;
  }
  return;
}

//Arrayent configuration
static OSStatus configureArrayent(void *inContext) 
{
  OSStatus err = kUnknownErr;
  arrayent_config_t arrayent_config;
  arrayent_return_e arrayent_ret = ARRAYENT_FAILURE;
  app_context_t *appContext = inContext;
  
  require_action( inContext, exit, err = kParamErr );
  
  if(ArrayentSetConfigDefaults(&arrayent_config) < 0) {
    arrayent_log("Arrayent Set Config Defaults failed. \r\n");
    err = kNotPreparedErr;
    goto exit ;
  }
  
  arrayent_config.product_id = appContext->appConfig->arrayentConfig.productId;
  arrayent_config.product_aes_key = appContext->appConfig->arrayentConfig.product_aes_key;
  arrayent_config.load_balancer_domain_names[0] = LOAD_BALANCER1;
  arrayent_config.load_balancer_domain_names[1] = LOAD_BALANCER2;
  arrayent_config.load_balancer_domain_names[2] = LOAD_BALANCER3;
  arrayent_config.load_balancer_udp_port = ACA_LOAD_BALANCER_UDP_PORT;
  arrayent_config.load_balancer_tcp_port = ACA_LOAD_BALANCER_TCP_PORT;
  //arrayent_config.device_name = Context->flashContentInRam.appConfig.uuid;
  //arrayent_config.device_password = Context->flashContentInRam.appConfig.aca_config.pwd;
  arrayent_config.device_name = appContext->appConfig->arrayentConfig.deviceId;
  arrayent_config.device_password = appContext->appConfig->arrayentConfig.devicePasswd;
  arrayent_config.device_aes_key = appContext->appConfig->arrayentConfig.device_aes_key;
  arrayent_config.aca_thread_priority = ACA_THREAD_PRIORITY;
  arrayent_config.aca_stack_size = ACA_STACK_SIZE;
  
  arrayent_ret = ArrayentConfigure(&arrayent_config);
  if(ARRAYENT_SUCCESS != arrayent_ret){
    err = kNotPreparedErr;
    goto exit;
  }
  
  return kNoErr;
  
exit:
  arrayent_log("ERROR: configureArrayent exit, err=%d.", err);
  return err;
}

void arrayent_main_thread(void *arg)
{
  OSStatus err = kUnknownErr;
  arrayent_net_status_t aca_net_status;
  arrayent_return_e ret;
  app_context_t* inContext = (app_context_t *)arg;
  
  require_action( inContext, exit, err = kParamErr );
  
  err = mico_rtos_init_semaphore(&_wifiConnected_sem, 1);
  require_noerr_action(err, exit, 
                       arrayent_log("ERROR: mico_rtos_init_semaphore (_wifiConnected_sem) failed!") );
  
  /* wait for wifi station on */
  while(!inContext->appStatus.isWifiConnected){
    arrayent_log("Waiting for Wi-Fi network...");
    mico_rtos_get_semaphore(&_wifiConnected_sem, 20000);
    mico_thread_msleep(500);
  }
  
  /* start Arrayent service */
  arrayent_log("Initializing Arrayent daemon...");
  err = configureArrayent(inContext);
  require_noerr_action(err, exit, 
                       arrayent_log("ERROR: configureArrayent failed!") );
  
  ret = ArrayentInit();
  if(ARRAYENT_SUCCESS != ret) {
    arrayent_log("Failed to initialize Arrayent daemon.%d", ret);
    err = kNotInitializedErr;
    goto exit;
  }
  arrayent_log("Arrayent daemon successfully initialized!");
  
  //Wait for Arrayent to finish connecting to the server
  do {
    ret = ArrayentNetStatus(&aca_net_status);//get connecting status to server
    if(ARRAYENT_SUCCESS != ret) {
      arrayent_log("Failed to connect to Arrayent network[0].");
    }
    arrayent_log("Waiting for good network status[0]");
    mico_thread_sleep(1);
  } while(!(aca_net_status.connected_to_server));//after few time connected server success.
  
  arrayent_log("Connected to Arrayent network!\r\n");
  inContext->appStatus.arrayentStatus.isCloudConnected = true;
  mico_cloud_connected_delegate(inContext);  // cloud connect(e.g: led blink)
  
  /* service loop */
  while(1){ 
    if(false == inContext->appStatus.isWifiConnected){  //sure wifi connected.
      mico_rtos_get_semaphore(&_wifiConnected_sem, 3000);
    }
    else{  // sure cloud connected.
      ret = ArrayentNetStatus(&aca_net_status);  // get connecting status to server
      if(ARRAYENT_SUCCESS == ret) {
        if(aca_net_status.connected_to_server){
          if(false == inContext->appStatus.arrayentStatus.isCloudConnected){  // connect recover
            arrayent_log("Arrayent network recover!\r\n");
            inContext->appStatus.arrayentStatus.isCloudConnected = true;
            mico_cloud_connected_delegate(inContext);
          }
        }
        else{
          if(true == inContext->appStatus.arrayentStatus.isCloudConnected){  // connect lost
            arrayent_log("Arrayent network lost!\r\n");
            inContext->appStatus.arrayentStatus.isCloudConnected = false;
            mico_cloud_disconnected_delegate(inContext);
          }
        }
      }else{
        arrayent_log("Failed to connect to Arrayent network[1]");
      }
    }
    
    mico_thread_sleep(1);  // free cpu if nothing to do
  }
  
exit:
  arrayent_log("ERROR: arrayent_main_thread exit err=%d.", err);
  mico_rtos_delete_thread(NULL);
  return;
}


/*******************************************************************************
 *                          service interfaces
 ******************************************************************************/
// reset default value
void MiCOArrayentRestoreDefault(arrayentcloud_config_t* const arrayent_config)
{
//  arrayent_return_e aca_ret = ARRAYENT_SUCCESS;

  if(NULL == arrayent_config){
    return;
  }
  
  arrayent_log("MiCOArrayentRestoreDefault");
  
//  // arrayent service reset
//  aca_ret = ArrayentReset();
//  if( ARRAYENT_SUCCESS != aca_ret){
//    arrayent_log("ERROR: ArrayentReset failed, err=%d.", aca_ret);
//  }
  
  // reset all MiCOArrayent config params
  memset((void*)arrayent_config, 
         0, sizeof(arrayentcloud_config_t));
  
  arrayent_config->productId = PPRODUCT_ID;
  sprintf(arrayent_config->product_aes_key, PRODUCT_AES_KEY);
  sprintf(arrayent_config->deviceId, DEVICE_NAME);
  sprintf(arrayent_config->devicePasswd, DEVICE_PASSWORD);
  sprintf(arrayent_config->device_aes_key, DEVICE_AES_KEY);
}

OSStatus MiCOStartArrayentService(app_context_t* const inContext)
{
  OSStatus err = kUnknownErr;
  
  require_action( inContext, exit, err = kParamErr );
  
  //init MiCOArrayent status
  inContext->appStatus.arrayentStatus.isCloudConnected = false;
  
  err = mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED, (void *)acaNotify_WifiStatusHandler, inContext);
  require_noerr_action(err, exit, 
                       arrayent_log("ERROR: mico_system_notify_register (mico_notify_WIFI_STATUS_CHANGED) failed!") );
  
  // start MiCOArrayent main thread
  err = mico_rtos_create_thread(NULL, MICO_DEFAULT_LIBRARY_PRIORITY, "arrayent_main", 
                                arrayent_main_thread, ACA_STACK_SIZE_ARRAYENT_MAIN_THREAD, 
                                inContext );
exit:
  return err;
}

// service sleep, can be re-configure
OSStatus MiCOSuspendArrayentService(app_context_t* const inContext, arrayent_sleep_level_e level)
{
  OSStatus err = kUnknownErr;
  arrayent_return_e aca_ret = ARRAYENT_FAILURE;
  
  require_action( inContext, exit, err = kParamErr );
  arrayent_log("MiCOSuspendArrayentService");

  aca_ret = ArrayentSleep(level);
  if(ARRAYENT_SUCCESS != aca_ret){
    arrayent_log("ERROR: ArrayentSleep: err=%d", aca_ret);
    err = kResponseErr;
  }
  else{
    arrayent_log("Arrayent sleep.");
    err = kNoErr;
  }
  
exit:
  return err;
}

// service wake up
OSStatus MiCOResumeArrayentService(app_context_t* const inContext)
{
  OSStatus err = kUnknownErr;
  arrayent_return_e aca_ret = ARRAYENT_FAILURE;
  
  require_action( inContext, exit, err = kParamErr );
  arrayent_log("MiCOResumeArrayentService");
  
  aca_ret = ArrayentWake();
  if(ARRAYENT_SUCCESS != aca_ret){
    arrayent_log("ERROR: ArrayentWake: err=%d", aca_ret);
    err = kResponseErr;
  }
  else{
    arrayent_log("Arrayent awake.");
    err = kNoErr;
  }
  
exit:
  return err;
}


/*******************************************************************************
*                             working state
*******************************************************************************/
// cloud connect state
bool MiCOArrayentIsConnect(app_context_t* const context)
{
  if(NULL == context){
    arrayent_log("ERROR: MiCOArrayentIsConnect param error!");
    return false;
  }
  return context->appStatus.arrayentStatus.isCloudConnected;
}

/*******************************************************************************
*                           property get/set
******************************************************************************/
// Module => Cloud
OSStatus MiCOArrayentPropSend(app_context_t* const context, char* property, char* value)
{
  arrayent_log_trace();
  OSStatus err = kUnknownErr;
  arrayent_return_e ret;
  
  if( (NULL == context) || (NULL == property) || (NULL == value) ){
    arrayent_log("ERROR: MiCOArrayentPropSend params error!");
    return kParamErr;
  }
  
  ret = ArrayentSetProperty(property,value);
  if(ARRAYENT_SUCCESS != ret){
    err = kWriteErr;
    arrayent_log("ERROR: ArrayentSetProperty error! err=%d", ret);
  }
  else{
    err = kNoErr;
  }

  return err;
}

// Cloud => Module
OSStatus MiCOArrayentPropRecv(app_context_t* const context, char* data, uint16_t* len, uint32_t timeout)
{
  arrayent_log_trace();
  OSStatus err = kUnknownErr;
  
  if( (NULL == context) || (NULL == data) || (NULL == len) ){
    arrayent_log("ERROR: MiCOArrayentPropSend params error!");
    mico_thread_sleep(timeout);
    return kParamErr;
  }
  
  //receive property data from server
  if(ARRAYENT_SUCCESS == ArrayentRecvProperty(data,len,timeout)){
    err = kNoErr;
    arrayent_log("recv property = %s\n\r",data);
  }
  else{
    err = kReadErr;
  }
  
  return err;
}

/*******************************************************************************
*                             device management
******************************************************************************/
//OTA
//OSStatus MiCOArrayentFirmwareUpdate(app_context_t* const context,
//                                    MVDOTARequestData_t OTAData)
//{
//  OSStatus err = kUnknownErr;
//  app_context_t *inContext = context;
//  
//  // firmware update
//  // ...
//  
//  return kNoErr;
//}
