/**
******************************************************************************
* @file    mdns.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   mDNS protocol demonstration!
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
#include "platform_config.h"
#include "StringUtils.h"

#define mdns_log(M, ...) custom_log("mDNS", M, ##__VA_ARGS__)

#define PROTOCOL            "com.mxchip.basic"
#define BONJOURNANE         "MiCOKit"
#define BONJOUR_SERVICE     "_easylink._tcp.local."
#define LOCAL_PORT          8080

void my_bonjour_server( WiFi_Interface interface )
{
  char *temp_txt= (char*)malloc(500);
  char *temp_txt2;
  IPStatusTypedef para;
  mdns_init_t init;
  
  micoWlanGetIPStatus(&para, interface);

  init.service_name = BONJOUR_SERVICE; /*"_easylink._tcp.local."*/

  /*format*/
  /*   name#xxxxxx.local.  */
  snprintf( temp_txt, 100, "%s#%c%c%c%c%c%c.local.", BONJOURNANE, 
                                                     para.mac[6],   para.mac[7], \
                                                     para.mac[8],   para.mac[9], \
                                                     para.mac[10],  para.mac[11]  );
  init.host_name = (char*)__strdup(temp_txt);

  /*   name#xxxxxx.   */
  snprintf( temp_txt, 100, "%s#%c%c%c%c%c%c",        BONJOURNANE, 
                                                     para.mac[6],   para.mac[7], \
                                                     para.mac[8],   para.mac[9], \
                                                     para.mac[10],  para.mac[11]   );
  init.instance_name = (char*)__strdup(temp_txt);

  init.service_port = LOCAL_PORT;

  //take some data
  /*for example,modify here*/
  temp_txt2 = __strdup_trans_dot(para.mac);
  sprintf(temp_txt, "MAC=%s.", temp_txt2);
  free(temp_txt2);

  temp_txt2 = __strdup_trans_dot(FIRMWARE_REVISION);
  sprintf(temp_txt, "%sFirmware Rev=%s.", temp_txt, temp_txt2);
  free(temp_txt2);
  
  temp_txt2 = __strdup_trans_dot(HARDWARE_REVISION);
  sprintf(temp_txt, "%sHardware Rev=%s.", temp_txt, temp_txt2);
  free(temp_txt2);

  temp_txt2 = __strdup_trans_dot(MicoGetVer());
  sprintf(temp_txt, "%sMICO OS Rev=%s.", temp_txt, temp_txt2);
  free(temp_txt2);

  temp_txt2 = __strdup_trans_dot(MODEL);
  sprintf(temp_txt, "%sModel=%s.", temp_txt, temp_txt2);
  free(temp_txt2);

  temp_txt2 = __strdup_trans_dot(PROTOCOL);
  sprintf(temp_txt, "%sProtocol=%s.", temp_txt, temp_txt2);
  free(temp_txt2);

  temp_txt2 = __strdup_trans_dot(MANUFACTURER);
  sprintf(temp_txt, "%sManufacturer=%s.", temp_txt, temp_txt2);
  free(temp_txt2);
  
  /*printf*/
  mdns_log("TXT RECORD=%s",temp_txt);
  
  /*
  TXT RECORD=MAC=c89346918152.
  Firmware Rev=MICO_BASE_1_0.
  Hardware Rev=MK3288_1.
  MICO OS Rev=10880002/.032.
  Model=MiCOKit-3288.
  Protocol=com/.mxchip/.basic.
  Manufacturer=MXCHIP Inc/..
  */
  /*take some data in txt_record*/
  init.txt_record = (char*)__strdup(temp_txt);

  mdns_add_record(init, interface, 1500 );

  /*free memeoy*/
  free(init.host_name);
  free(init.instance_name);
  free(init.txt_record);
 
  if(temp_txt) 
    free(temp_txt);
}

int application_start( void )
{
  OSStatus err = kNoErr;

  /* Start MiCO system functions according to mico_config.h */
  err = mico_system_init( mico_system_context_init( 0 ) );
  require_noerr( err, exit ); 
  
  /* Register mDNS record */
  my_bonjour_server( Station );

exit:
  if( err != kNoErr )
    mdns_log("Thread exit with err: %d", err);
  mico_rtos_delete_thread( NULL );
  return err;
}

