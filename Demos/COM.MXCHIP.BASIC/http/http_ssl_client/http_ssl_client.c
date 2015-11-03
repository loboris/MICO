/**
******************************************************************************
* @file    http_ssl_client.c 
* @author  QQ DING
* @version V1.0.0
* @date    1-September-2015
* @brief   This demo main function is to open to use SSL encrypted page
******************************************************************************
*
*  The MIT License
*  Copyright (c) 2014 MXCHIP Inc.
*
*  Permission is hereby granted, free of charge, to any person obtaining a 
copy 
*  of this software and associated documentation files (the "Software"), to 
deal
*  in the Software without restriction, including without limitation the 
rights 
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is 
furnished
*  to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in
*  all copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY,
*  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
OR 
*  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE.
******************************************************************************
*/


#include "MICO.h"

#define https_client_log(M, ...) custom_log("HTTP", M, ##__VA_ARGS__)

#define https_host  "persons.shgjj.com"
#define https_port  443
static char *ap_ssid = "ssid";
static char *ap_key  = "1234567890";
static mico_semaphore_t wait_sem;

char httpRequest[]=
"GET /SsoLogin?url=https://persons.shgjj.com/MainServlet?ID=1 HTTP/1.1\r\n\
Host: persons.shgjj.com\r\n\
Cache-Control: no-cache\r\n\r\n";

void micoNotify_ConnectFailedHandler(OSStatus err, void* const inContext)
{
  https_client_log("Wlan Connection Err %d", err);
}

void micoNotify_WifiStatusHandler(WiFiEvent event, void* const inContext)
{
  switch (event) {
      case NOTIFY_STATION_UP:
      https_client_log("connected wlan success");
      /*release semaphore*/
      mico_rtos_set_semaphore(&wait_sem);
      break;
  case NOTIFY_STATION_DOWN:
      https_client_log("Station down");
      break;
  }
}

/*connect wifi*/
static void connect_ap( void )
{  
  network_InitTypeDef_adv_st wNetConfigAdv;
  memset(&wNetConfigAdv, 0x0, sizeof(network_InitTypeDef_adv_st));
  strcpy((char*)wNetConfigAdv.ap_info.ssid, ap_ssid);/*ap's ssid*/
  strcpy((char*)wNetConfigAdv.key, ap_key);/*ap's key*/
  wNetConfigAdv.key_len = strlen(ap_key);
  wNetConfigAdv.ap_info.security = SECURITY_TYPE_AUTO;
  wNetConfigAdv.ap_info.channel = 0; /*Auto*/
  wNetConfigAdv.dhcpMode = DHCP_Client;
  wNetConfigAdv.wifi_retry_interval = 100;
  micoWlanStartAdv(&wNetConfigAdv);
}

void ssl_channel_close(void)
{
  
}

void https_client_thread(void *inContext)
{
  OSStatus err;
  int errno = 0;
  struct sockaddr_t addr;
  struct timeval_t timeout;
  static char *ssl_active = NULL;
  char ipstr[16];
  fd_set readfd;
  int https_fd = -1 , len, nfound;
  char *buf=(char*)malloc(1024);
  require_action(buf, EXIT_THREAD, err = kNoMemoryErr);/*alloc failed*/
  
  err = gethostbyname(https_host, (uint8_t *)ipstr, 16);
  require_noerr(err, EXIT_THREAD);
  
  https_client_log("server address: host:%s, ip: %s", https_host, ipstr);
  
  https_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  
  addr.s_ip = inet_addr(ipstr);
  addr.s_port = https_port;
  https_client_log("Connecting to server: ip=%s  port=%d!", ipstr, addr.s_port);
  
  err = connect(https_fd, &addr, sizeof(addr));
  require_noerr(err, EXIT_THREAD);
  
  ssl_active = ssl_connect(https_fd, 0, 0, &errno);
  require_noerr_action( NULL == ssl_active, EXIT_THREAD, https_client_log("ERROR: ssl disconnect") );
  
  https_client_log("ssl connect, send HTTP POST, ssl_active %p, errno %d", ssl_active, errno);
  
  ssl_send(ssl_active, httpRequest, strlen(httpRequest));
  
  FD_ZERO(&readfd);
  FD_SET(https_fd, &readfd);
  
  timeout.tv_sec = 60;
  timeout.tv_usec = 0;

  while(1)
  {
    nfound=select(1, &readfd, NULL, NULL, &timeout);
    if(nfound==0)/*nothing happened,printf dot...*/
    {
      MicoUartSend( STDIO_UART, ".", 1 ); 
      continue;
    }
    /*recv wlan data*/
    if (FD_ISSET( https_fd, &readfd )) /*read what,write back*/
    {
       memset(buf, 0, 1024);
       len=ssl_recv(ssl_active, buf, 1024);
       if( len <= 0)
       {
           https_client_log("socket disconnected");
           break;/*error*/
       }
       https_client_log("rec from server,len=%d,buf=%s", len, buf);
    }
  }
  
EXIT_THREAD:
  
  free(buf);
  if(https_fd != -1)
    close(https_fd);
  if(ssl_active != NULL)
    ssl_close(ssl_active);
  mico_rtos_delete_thread(NULL);

}


int application_start( void )
{
  OSStatus err = kNoErr;
  https_client_log("http_ssl_client demo");
  MicoInit( );/*important,init tcpip,rf driver and so on...*/
  
   /*Register user function for MiCO nitification: WiFi status changed */
  err = mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED, (void *)micoNotify_WifiStatusHandler, NULL);
  require_noerr( err, EXIT ); 
  
  err = mico_system_notify_register( mico_notify_WIFI_CONNECT_FAILED, (void *)micoNotify_ConnectFailedHandler, NULL);
  require_noerr( err, EXIT );
  
  err = mico_rtos_init_semaphore(&wait_sem, 1);
  require_noerr( err, EXIT ); 
  
  connect_ap( );/*connect wifi*/
  https_client_log("connecting to %s...", ap_ssid);
  mico_rtos_get_semaphore(&wait_sem, MICO_WAIT_FOREVER);/*wait forever until connected wifi*/
    
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "HTTPs_client", https_client_thread, 0x2500, NULL );
  require_noerr_action( err, EXIT, https_client_log("ERROR: Unable to start the https client thread.") );
  
  return err;

EXIT:
  https_client_log("ERROR, err: %d", err);
  return err;
}





















