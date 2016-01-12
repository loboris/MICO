/**
******************************************************************************
* @file    wifi_station.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   First MiCO application to say hello world!
******************************************************************************
*
*  The MIT License
*  Copyright (c) 2014 MXCHIP Inc.
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy 
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights 
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is furnished
*  to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in
*  all copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
*  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR 
*  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************
*/

#include "MICO.h"

#define wifi_station_log(M, ...) custom_log("WIFI", M, ##__VA_ARGS__)

static void micoNotify_ConnectFailedHandler(OSStatus err, void* inContext)
{
  wifi_station_log("join Wlan failed Err: %d", err);
}

static void micoNotify_WifiStatusHandler(WiFiEvent event,  void* inContext)
{
  switch (event) 
  {
  case NOTIFY_STATION_UP:
    wifi_station_log("Station up");
    break;
  case NOTIFY_STATION_DOWN:
    wifi_station_log("Station down");
    break;
  default:
    break;
  }
}
int application_start( void )
{
  OSStatus err = kNoErr;
  network_InitTypeDef_adv_st  wNetConfigAdv={0};
 
  MicoInit( );
  
  /* Register user function when wlan connection status is changed */
  err = mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED, (void *)micoNotify_WifiStatusHandler, NULL );
  require_noerr( err, exit ); 
  
  /* Register user function when wlan connection is faile in one attempt */
  err = mico_system_notify_register( mico_notify_WIFI_CONNECT_FAILED, (void *)micoNotify_ConnectFailedHandler, NULL );
  require_noerr( err, exit );
  
  /* Initialize wlan parameters */
  strcpy((char*)wNetConfigAdv.ap_info.ssid, "Xiaomi.Router");   /* wlan ssid string */
  strcpy((char*)wNetConfigAdv.key, "stm32f215");                /* wlan key string or hex data in WEP mode */
  wNetConfigAdv.key_len = strlen(ap_key);                       /* wlan key length */
  wNetConfigAdv.ap_info.security = SECURITY_TYPE_AUTO;          /* wlan security mode */
  wNetConfigAdv.ap_info.channel = 0;                            /* Select channel automatically */
  wNetConfigAdv.dhcpMode = DHCP_Client;                         /* Fetch Ip address from DHCP server */
  wNetConfigAdv.wifi_retry_interval = 100;                      /* Retry interval after a failure connection */
  
  /* Connect Now! */
  wifi_station_log("connecting to %s...", wNetConfigAdv.ap_info.ssid);
  micoWlanStartAdv(&wNetConfigAdv);
  
exit:
  mico_rtos_delete_thread(NULL);
  return err;
}

