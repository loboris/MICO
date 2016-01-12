/**
******************************************************************************
* @file    udp_broadcast.c 
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

#define udp_broadcast_log(M, ...) custom_log("UDP", M, ##__VA_ARGS__)

#define LOCAL_UDP_PORT 20000
#define REMOTE_UDP_PORT 20001

char* data = "UDP broadcast data";

/*create udp socket*/
void udp_broadcast_thread(void *arg)
{
  UNUSED_PARAMETER( arg );

  OSStatus err;
  struct sockaddr_t addr;
  int udp_fd = -1 ;
  
  /*Establish a UDP port to receive any data sent to this port*/
  udp_fd = socket( AF_INET, SOCK_DGRM, IPPROTO_UDP );
  require_action( IsValidSocket( udp_fd ), exit, err = kNoResourcesErr );
  
  addr.s_ip = INADDR_ANY;
  addr.s_port = LOCAL_UDP_PORT;
  err = bind(udp_fd, &addr, sizeof(addr));
  require_noerr( err, exit );

  udp_broadcast_log("Start UDP broadcast mode, local port: %d, remote port: %d", LOCAL_UDP_PORT, REMOTE_UDP_PORT);

  while(1)
  {
    udp_broadcast_log( "broadcast now!" );

    addr.s_ip = INADDR_BROADCAST;
    addr.s_port = REMOTE_UDP_PORT;
    /*the receiver should bind at port=20000*/
    sendto( udp_fd, data, strlen(data), 0, &addr, sizeof(addr) );

    mico_thread_sleep(2);
  }
  
exit:
  if( err != kNoErr )
    udp_broadcast_log("UDP thread exit with err: %d", err);
  mico_rtos_delete_thread(NULL);
}

int application_start( void )
{
  OSStatus err = kNoErr;
  
  /* Start MiCO system functions according to mico_config.h */
  err = mico_system_init( mico_system_context_init( 0 ) );
  require_noerr( err, exit ); 

  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "udp_broadcast", udp_broadcast_thread, 0x800, NULL );
  require_noerr_string( err, exit, "ERROR: Unable to start the UDP thread." );
  
exit:
  if( err != kNoErr )
    udp_broadcast_log("Thread exit with err: %d", err);
  mico_rtos_delete_thread( NULL );
  return err;
}

