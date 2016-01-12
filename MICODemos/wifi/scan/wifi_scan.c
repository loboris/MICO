/**
******************************************************************************
* @file    wifi_scan.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   scan wifihot
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

#define wifi_scan_log(M, ...) custom_log("WIFI", M, ##__VA_ARGS__)

static void micoNotify_ApListCallback(ScanResult *pApList)
{
  int i=0;
  wifi_scan_log("got %d AP", pApList->ApNum);
  for(i=0; i<pApList->ApNum; i++)
  {
    wifi_scan_log("ap%d: name = %s  | strenth=%d",  
                  i,pApList->ApList[i].ssid, pApList->ApList[i].ApPower);

  }
}

int application_start( void )
{
  /* Start MiCO system functions according to mico_config.h*/
  mico_system_init( mico_system_context_init( 0 ) );
  
  /* Register user function when wlan scan is completed */
  mico_system_notify_register( mico_notify_WIFI_SCAN_COMPLETED, (void *)micoNotify_ApListCallback, NULL );
  
  wifi_scan_log("start scan mode, please wait...");
  micoWlanStartScan( );
  
  mico_rtos_delete_thread( NULL );
  return kNoErr;
}



