/**
******************************************************************************
* @file    MiCOFogCloud.c 
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

#include "mico.h"
#include "MiCOSiteWhere.h"
#include "custom.h"
#include "StringUtils.h"

#define sitewhere_log(M, ...) custom_log("MiCO SiteWhere", M, ##__VA_ARGS__)
#define sitewhere_log_trace() custom_log_trace("MiCO SiteWhere")


/*******************************************************************************
 *                                  DEFINES
 ******************************************************************************/
#define STACK_SIZE_SITEWHERE_MAIN_THREAD    0xC00


/*******************************************************************************
 *                                  VARIABLES
 ******************************************************************************/


/*******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************/

void swNotify_WifiStatusHandler(WiFiEvent event, app_context_t * const inContext)
{
  (void)inContext;
  switch (event) {
  case NOTIFY_STATION_UP:
    inContext->appStatus.isWifiConnected = true;
    MicoRfLed(true);
    break;
  case NOTIFY_STATION_DOWN:
    inContext->appStatus.isWifiConnected = false;
    MicoRfLed(false);
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

extern void sitewhere_main_thread(void *arg);
extern void mqtt_loop_thread(void *inContext);

extern char* clientName;

/** Unique hardware id for this device */
extern char hardwareId[HARDWARE_ID_SIZE];

/** Device specification token for hardware configuration */
extern char* specificationToken;

/** Inbound custom command topic */
extern char Command1[COMMAND1_SIZE];

/** Inbound system command topic */
extern char System1[SYSTEM1_SIZE];

/*******************************************************************************
 *                        FogCloud  interfaces init
 ******************************************************************************/

OSStatus MiCOStartSiteWhereService(app_context_t* const inContext)
{
  sitewhere_log_trace();
  OSStatus err = kUnknownErr;
  char *bonjour_txt_record = NULL;
  char *bonjour_txt_field = NULL;
  
  // update wifi status
  err = mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED, (void *)swNotify_WifiStatusHandler, inContext);
  require_noerr_action(err, exit, 
                       sitewhere_log("ERROR: mico_system_notify_register (mico_notify_WIFI_STATUS_CHANGED) failed!") );
  
  // create uinque hardeareID like: MODEL-HARDWARE_REVISION-MAC
  memset(hardwareId, '\0', sizeof(hardwareId));
  sprintf(hardwareId, "Enjoy_%s-%s-%c%c%c%c%c%c",MODEL, HARDWARE_REVISION, 
          mico_system_context_get()->micoStatus.mac[9],
          mico_system_context_get()->micoStatus.mac[10],
          mico_system_context_get()->micoStatus.mac[12],
          mico_system_context_get()->micoStatus.mac[13],
          mico_system_context_get()->micoStatus.mac[15],
          mico_system_context_get()->micoStatus.mac[16]
            );
  sitewhere_log("hardwareId=[%s]", hardwareId);
  // create system topic like: SiteWhere/commands/<hardwareid>
  memset(System1, '\0', sizeof(System1));
  sprintf(System1, "%s/%s", "SiteWhere/system", hardwareId);
  sitewhere_log("System1=[%s]", System1);
  // create command topic like: SiteWhere/<system/hardwareid>
  memset(Command1, '\0', sizeof(Command1));
  sprintf(Command1, "%s/%s", "SiteWhere/commands", hardwareId);
  sitewhere_log("Command1=[%s]", Command1);
  // specificationToken
  sitewhere_log("specificationToken=[%s]", specificationToken);
  
  //------------------------------------------------------------------------
  // update hardwareid in mdns text record
  sitewhere_log("update bonjour txt record.");
  bonjour_txt_record = malloc(550);
  require_action(bonjour_txt_record, exit, err = kNoMemoryErr);
  
  bonjour_txt_field = __strdup_trans_dot(inContext->mico_context->micoStatus.mac);
  sprintf(bonjour_txt_record, "MAC=%s.", bonjour_txt_field);
  free(bonjour_txt_field);
  
  bonjour_txt_field = __strdup_trans_dot(hardwareId);
  sprintf(bonjour_txt_record, "%sHardware ID=%s.", bonjour_txt_record, bonjour_txt_field);
  free(bonjour_txt_field);
  
  bonjour_txt_field = __strdup_trans_dot(FIRMWARE_REVISION);
  sprintf(bonjour_txt_record, "%sFirmware Rev=%s.", bonjour_txt_record, bonjour_txt_field);
  free(bonjour_txt_field);
  
  bonjour_txt_field = __strdup_trans_dot(HARDWARE_REVISION);
  sprintf(bonjour_txt_record, "%sHardware Rev=%s.", bonjour_txt_record, bonjour_txt_field);
  free(bonjour_txt_field);
  
  bonjour_txt_field = __strdup_trans_dot(MicoGetVer());
  sprintf(bonjour_txt_record, "%sMICO OS Rev=%s.", bonjour_txt_record, bonjour_txt_field);
  free(bonjour_txt_field);
  
  bonjour_txt_field = __strdup_trans_dot(MODEL);
  sprintf(bonjour_txt_record, "%sModel=%s.", bonjour_txt_record, bonjour_txt_field);
  free(bonjour_txt_field);
  
  bonjour_txt_field = __strdup_trans_dot(PROTOCOL);
  sprintf(bonjour_txt_record, "%sProtocol=%s.", bonjour_txt_record, bonjour_txt_field);
  free(bonjour_txt_field);
  
  bonjour_txt_field = __strdup_trans_dot(MANUFACTURER);
  sprintf(bonjour_txt_record, "%sManufacturer=%s.", bonjour_txt_record, bonjour_txt_field);
  free(bonjour_txt_field);
  
  sprintf(bonjour_txt_record, "%sSeed=%u.", bonjour_txt_record, inContext->mico_context->flashContentInRam.micoSystemConfig.seed);
  
  mdns_update_txt_record(BONJOUR_SERVICE, Station, bonjour_txt_record);
  if(NULL != bonjour_txt_record) free(bonjour_txt_record);
  //------------------------------------------------------------------------
  
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "sitewhere", 
                                sitewhere_main_thread, STACK_SIZE_SITEWHERE_MAIN_THREAD, 
                                inContext );
  require_noerr_action( err, exit, sitewhere_log("ERROR: Unable to start sitewhere thread.") );
  
  //err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "MQTT Client", 
  //                              mqtt_loop_thread, 0xC00, (void*)inContext->mico_context );
  //require_noerr_action( err, exit, sitewhere_log("ERROR: Unable to start the MQTT client thread.") );
  
exit:
  return err;
}
