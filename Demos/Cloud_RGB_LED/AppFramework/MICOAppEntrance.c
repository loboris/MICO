/**
  ******************************************************************************
  * @file    MICOAppEntrance.c 
  * @author  William Xu
  * @version V1.0.0
  * @date    05-May-2014
  * @brief   MiCO application entrance, addd user application functons and threads.
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
#include "MiCOFogCloud.h"

//#ifdef USE_MiCOKit_EXT
//#include "micokit_ext.h"
//extern bool MicoExtShouldEnterTestMode(void);
//#endif

#define app_log(M, ...) custom_log("APP", M, ##__VA_ARGS__)
#define app_log_trace() custom_log_trace("APP")

extern OSStatus MICOStartBonjourService( WiFi_Interface interface, app_context_t * const inContext );

app_context_t* app_context_global = NULL;

// for station connect ap error state(no station down event when AP down)
static void appNotify_ConnectFailedHandler(OSStatus err, mico_Context_t * const inContext)
{
  system_log_trace();
  (void)inContext;
  app_log("Wlan Connection Err %d", err);
  if(NULL != app_context_global){
    app_context_global->appStatus.isWifiConnected = false;
    MicoRfLed(false);
  }
}

/* default user_main callback function, this must be override by user. */
WEAK OSStatus user_main( app_context_t * const mico_context )
{
  //app_log("ERROR: user_main undefined!");
  return kNotHandledErr;
}

/* user main thread created by MICO APP thread */
void user_main_thread(void* arg)
{
  OSStatus err = kUnknownErr;
  app_context_t *app_context = (app_context_t *)arg;
  
  // loop in user mian function && must not return
  err = user_main(app_context);
  
  // never get here only if user work error.
  app_log("ERROR: user_main thread exit err=%d, system will reboot...", err);
  
  err = mico_system_power_perform(app_context->mico_context, eState_Software_Reset);
  UNUSED_PARAMETER(err);
  
  mico_rtos_delete_thread(NULL);   
}

OSStatus startUserMainThread(app_context_t *app_context)
{
  app_log_trace();
  OSStatus err = kNoErr;
  require_action(app_context, exit, err = kParamErr);
  
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "user_main", 
                                user_main_thread, STACK_SIZE_USER_MAIN_THREAD, 
                                app_context );
exit:
  return err;
}

/* MICO system callback: Restore default configuration provided by application */
void appRestoreDefault_callback(void * const user_config_data, uint32_t size)
{
  UNUSED_PARAMETER(size);
  application_config_t* appConfig = user_config_data;
  
  appConfig->configDataVer = CONFIGURATION_VERSION;
  appConfig->bonjourServicePort = BONJOUR_SERVICE_PORT;
  
  // restore fogcloud config
  MiCOFogCloudRestoreDefault(&(appConfig->fogcloudConfig));
}

int application_start(void)
{
  app_log_trace();
  OSStatus err = kNoErr;
  app_context_t* app_context;
  mico_Context_t* mico_context;
  LinkStatusTypeDef wifi_link_status;

  /* Create application context */
  app_context = ( app_context_t *)calloc(1, sizeof(app_context_t) );
  require_action( app_context, exit, err = kNoMemoryErr );

  /* Create mico system context and read application's config data from flash */
  mico_context = mico_system_context_init( sizeof( application_config_t) );
  require_action(mico_context, exit, err = kNoResourcesErr);
  app_context->appConfig = mico_system_context_get_user_data( mico_context );
  app_context->mico_context = mico_context;
  app_context_global = app_context;
  
  /* user params restore check */
  if(app_context->appConfig->configDataVer != CONFIGURATION_VERSION){
    err = mico_system_context_restore(mico_context);
    require_noerr( err, exit );
  }

  /* mico system initialize */
  err = mico_system_init( mico_context );
  require_noerr( err, exit );
  MicoSysLed(true);
  
  // fix for AP down problem
  err = mico_system_notify_register( mico_notify_WIFI_CONNECT_FAILED,
                                    (void *)appNotify_ConnectFailedHandler, app_context->mico_context );
  require_noerr_action(err, exit, 
                       app_log("ERROR: MICOAddNotification (mico_notify_WIFI_CONNECT_FAILED) failed!") );
  
//#ifdef USE_MiCOKit_EXT
//  /* user test mode to test MiCOKit-EXT board */
//  if(MicoExtShouldEnterTestMode()){
//    app_log("Enter ext-board test mode by key2.");
//    micokit_ext_mfg_test(mico_context);
//  }
//#endif
  
  // block here if no wifi configuration.
  while(1){
    if( mico_context->flashContentInRam.micoSystemConfig.configured == wLanUnConfigured ||
       mico_context->flashContentInRam.micoSystemConfig.configured == unConfigured){
         mico_thread_msleep(100);
       }
    else{
      break;
    }
  }

  /* Bonjour for service searching */
  MICOStartBonjourService( Station, app_context );
    
  /* check wifi link status */
  do{
    err = micoWlanGetLinkStatus(&wifi_link_status);
    if(kNoErr != err){
      mico_thread_sleep(3);
    }
  }while(kNoErr != err);
  
  if(1 ==  wifi_link_status.is_connected){
    app_context->appStatus.isWifiConnected = true;
    MicoRfLed(true);
  }
  else{
    app_context->appStatus.isWifiConnected = false;
    MicoRfLed(false);
  }
  
  /* start cloud service */
#if (MICO_CLOUD_TYPE == CLOUD_FOGCLOUD)
  app_log("MICO CloudService: FogCloud.");
  err = MiCOStartFogCloudService( app_context );
  require_noerr_action( err, exit, app_log("ERROR: Unable to start FogCloud service.") );
#elif (MICO_CLOUD_TYPE == CLOUD_ALINK)
  app_log("MICO CloudService: Alink.");
#elif (MICO_CLOUD_TYPE == CLOUD_DISABLED)
  app_log("MICO CloudService: disabled.");
#else
  #error "MICO cloud service type is not defined"?
#endif
  
  /* start user thread */
  err = startUserMainThread( app_context );
  require_noerr_action( err, exit, app_log("ERROR: start user_main thread failed!") );

exit:
  mico_rtos_delete_thread(NULL);
  return err;
}
