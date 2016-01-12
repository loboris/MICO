/**
******************************************************************************
* @file    tcp_server.c 
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
#include "SocketUtils.h"

#define tcp_server_log(M, ...) custom_log("TCP", M, ##__VA_ARGS__)

#define SERVER_PORT 20000 /*set up a tcp server,port at 20000*/

void micoNotify_WifiStatusHandler(WiFiEvent event, void* const inContext)
{
  IPStatusTypedef para;
  switch (event) {
  case NOTIFY_STATION_UP:
    micoWlanGetIPStatus(&para, Station);
    tcp_server_log("Server established at ip: %s port: %d",para.ip, SERVER_PORT);
    break;
  case NOTIFY_STATION_DOWN:
    break;
  }
}

void tcp_client_thread(void *arg)
{
  OSStatus err = kNoErr;
  int fd = (int)arg;
  int len = 0;
  fd_set readfds;
  char *buf = NULL; 
  struct timeval_t t;
  
  buf=(char*)malloc(1024);
  require_action(buf, exit, err = kNoMemoryErr); 
  
  t.tv_sec = 5;
  t.tv_usec = 0;
  
  while(1)
  {
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    
    require_action( select(1, &readfds, NULL, NULL, &t) >= 0, exit, err = kConnectionErr );
    
    if( FD_ISSET( fd, &readfds ) ) /*one client has data*/
    {
      len = recv( fd, buf, 1024, 0 );
      require_action( len >= 0, exit, err = kConnectionErr );
      
      if( len == 0){
        tcp_server_log( "TCP Client is disconnected, fd: %d", fd );
        goto exit;
      }  
      
      tcp_server_log("fd: %d, recv data %d from client", fd, len);
      len = send( fd, buf, len, 0 );
      tcp_server_log("fd: %d, send data %d to client", fd, len);
    }
  }
exit:
  if( err != kNoErr ) tcp_server_log( "TCP client thread exit with err: %d", err );
  if( buf != NULL ) free( buf );
  SocketClose( &fd );
  mico_rtos_delete_thread( NULL );
}

/* TCP server listener thread */
void tcp_server_thread( void *arg )
{
  OSStatus err = kNoErr;
  struct sockaddr_t server_addr,client_addr;
  socklen_t sockaddr_t_size = sizeof( client_addr );
  char  client_ip_str[16];
  int tcp_listen_fd = -1, client_fd = -1;
  fd_set readfds;
  
  tcp_listen_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  require_action(IsValidSocket( tcp_listen_fd ), exit, err = kNoResourcesErr );
  
  server_addr.s_ip =  INADDR_ANY;  /* Accept conenction request on all network interface */
  server_addr.s_port = SERVER_PORT;/* Server listen on port: 20000 */
  err = bind( tcp_listen_fd, &server_addr, sizeof( server_addr ) );
  require_noerr( err, exit );
  
  err = listen( tcp_listen_fd, 0);
  require_noerr( err, exit );
  
  while(1)
  {
    FD_ZERO( &readfds );
    FD_SET( tcp_listen_fd, &readfds );
    
    require( select(1, &readfds, NULL, NULL, NULL) >= 0, exit );
    
    if(FD_ISSET(tcp_listen_fd, &readfds)){
      client_fd = accept( tcp_listen_fd, &client_addr, &sockaddr_t_size );
      if( IsValidSocket( client_fd ) ) {
        inet_ntoa( client_ip_str, client_addr.s_ip );
        tcp_server_log( "TCP Client %s:%d connected, fd: %d", client_ip_str, client_addr.s_port, client_fd );
        if ( kNoErr !=  mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "TCP Clients", tcp_client_thread, 0x800, (void *)client_fd ) )
          SocketClose( &client_fd );      
      }
    }
  }
exit:
  if( err != kNoErr ) tcp_server_log( "Server listerner thread exit with err: %d", err );
  SocketClose( &tcp_listen_fd );
  mico_rtos_delete_thread(NULL );
}


int application_start( void )
{
  OSStatus err = kNoErr;
  
  /*Register user function for MiCO nitification: WiFi status changed */
  err = mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED, (void *)micoNotify_WifiStatusHandler, NULL );
  require_noerr( err, exit ); 
  
  /* Start MiCO system functions according to mico_config.h */
  err = mico_system_init( mico_system_context_init( 0 ) );
  require_noerr( err, exit ); 
  
  /* Start TCP server listener thread*/
  err = mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "TCP_server", tcp_server_thread, 0x800, NULL );
  require_noerr_string( err, exit, "ERROR: Unable to start the tcp server thread." );
  
exit:
  mico_rtos_delete_thread( NULL );
  return err;
}

