/**
******************************************************************************
* @file    http_client.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   http client demo to download ota bin
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
#include "HTTPUtils.h"
#include "SocketUtils.h"
#include "StringUtils.h"

#define http_client_log(M, ...) custom_log("HTTP", M, ##__VA_ARGS__)

static OSStatus onReceivedData(struct _HTTPHeader_t * httpHeader, 
                               uint32_t pos, 
                               uint8_t *data, 
                               size_t len, 
                               void * userContext);

/*http://webservice.webxml.com.cn/WebServices/TrainTimeWebService.asmx/getStationAndTimeByTrainCode?TrainCode=d11&UserID=*/
static char *ap_ssid = "Xiaomi.Router";
static char *ap_key  = "stm32f215";
static char *http_get_url = "WebServices/TrainTimeWebService.asmx/getStationAndTimeByTrainCode?TrainCode=d11&UserId=";
static char *http_post_url = "WebServices/TrainTimeWebService.asmx/getStationAndTimeByTrainCode";
static char *str_args="TrainCode=d11&UserID=";
static char *http_host = "webservice.webxml.com.cn";
static int  http_port = 80;
static mico_semaphore_t wait_sem;
void start_http_get( );
char *g_data=NULL;
/*
GET /WebServices/TrainTimeWebService.asmx/getStationAndTimeByTrainCode?TrainCode=string&UserID=string HTTP/1.1
Host: webservice.webxml.com.cn

*/
const char http_get_format[]=
{
"GET /%s HTTP/1.1\r\n\
 Host: %s\r\n\r\n"
};
/*
POST /WebServices/TrainTimeWebService.asmx/getStationAndTimeByTrainCode HTTP/1.1
Host: webservice.webxml.com.cn
Content-Type: application/x-www-form-urlencoded
Content-Length: length

TrainCode=string&UserID=string
*/
const char http_post_format[]=
{
"POST /%s HTTP/1.1\r\n\
Host: %s\r\n\
Content-Type: application/x-www-form-urlencoded\r\n\
Content-Length:%s\r\n\r\n\
%s\r\n"
};

void micoNotify_ConnectFailedHandler(OSStatus err, void* const inContext)
{
    http_client_log("Wlan Connection Err %d", err);
}
void micoNotify_WifiStatusHandler(WiFiEvent status, void* const inContext)
{
  switch (status) {
    case NOTIFY_STATION_UP:
        http_client_log("Station up,connected wifi");
        mico_rtos_set_semaphore(&wait_sem);
        break;
    case NOTIFY_STATION_DOWN:
        http_client_log("Station down,unconnected wifi");
        break;
  }
}

static void connect_ap( void )
{  
  network_InitTypeDef_adv_st wNetConfigAdv={0};
  strcpy((char*)wNetConfigAdv.ap_info.ssid, ap_ssid);
  strcpy((char*)wNetConfigAdv.key, ap_key);
  wNetConfigAdv.key_len = strlen(ap_key);
  wNetConfigAdv.ap_info.security = SECURITY_TYPE_AUTO;
  wNetConfigAdv.ap_info.channel = 0; //Auto
  wNetConfigAdv.dhcpMode = DHCP_Client;
  wNetConfigAdv.wifi_retry_interval = 100;
  micoWlanStartAdv(&wNetConfigAdv);
  http_client_log("connecting to %s...", ap_ssid);
}

int application_start( void )
{
  OSStatus err = kNoErr;
  http_client_log("this is http demo");
  MicoInit( );/*TCPIP,RF driver init */
  
  /*Register user function for MiCO nitification: WiFi status changed */
  err = mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED, (void *)micoNotify_WifiStatusHandler, NULL );
  require_noerr( err, EXIT ); 
  
  err = mico_system_notify_register( mico_notify_WIFI_CONNECT_FAILED, (void *)micoNotify_ConnectFailedHandler, NULL );
  require_noerr( err, EXIT );
  
  err = mico_rtos_init_semaphore(&wait_sem, 1);
  require_noerr( err, EXIT );
  
  connect_ap( );
  /*wait here forever until wifi is connected*/
  mico_rtos_get_semaphore(&wait_sem, MICO_WAIT_FOREVER);

  start_http_get();
  return err;
EXIT:
  http_client_log("ERROR, err: %d", err);
  return err;
}


void start_http_get( )
{
  OSStatus err;
  int client_fd = -1;
  fd_set readfds;
  char ipstr[16];
  struct sockaddr_t addr;
  char content_length[20]={0};
  char *httpRequest = (char*)malloc(256);

  /*HTTPHeaderCreateWithCallback set some callback functions */
  HTTPHeader_t *httpHeader = HTTPHeaderCreateWithCallback(onReceivedData, NULL, NULL);
  require_action( httpHeader, EXIT, err = kNoMemoryErr );
  
  /*get free memory*/
  http_client_log("Free memory has %d bytes", MicoGetMemoryInfo()->free_memory) ; 
  err = gethostbyname(http_host, (uint8_t *)ipstr, 16);
  http_client_log("server address: host:%s, ip: %s", http_host, ipstr);
  
  client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  addr.s_ip = inet_addr(ipstr);
  addr.s_port = http_port;/*80*/
  err = connect(client_fd, &addr, sizeof(addr)); 
  require_noerr_action(err, EXIT, http_client_log("connect http server failed"));
  
  /*build my request command,get method*/
  sprintf(httpRequest, http_get_format, http_get_url, http_host);/*format URL and HOST*/
  
   /*build my request command,post method*/
  //Int2Str((uint8_t*)content_length, strlen(str_args));
  //sprintf(httpRequest, http_post_format, 
   //                    http_post_url,
     //                  http_host,
       //                content_length,
         //              str_args);/*format URL,HOST,length,args*/
  
  printf("*******\r\n");
  printf("%s\r\n",httpRequest);
  printf("*******\r\n");
  write(client_fd,(uint8_t *)httpRequest, strlen(httpRequest));/*write socket*/
  free(httpRequest);

  FD_ZERO(&readfds);
  FD_SET(client_fd, &readfds);
  while(1){
     http_client_log("waiting...");
     select(1, &readfds, NULL, NULL, NULL);
     if(FD_ISSET(client_fd,&readfds))
     {
        /*parse header*/
        err = SocketReadHTTPHeader( client_fd, httpHeader );
        switch ( err )
        {
            case kNoErr:
              //PrintHTTPHeader(httpHeader);
              err = SocketReadHTTPBody( client_fd, httpHeader );/*get body data*/
              require_noerr(err, EXIT);
              /*get data and print*/
              printf("*******\r\n");
              printf("%s\r\n",g_data);
              printf("*******\r\n");
              free(g_data);
              g_data=NULL;
              goto EXIT;
            case EWOULDBLOCK:
            case kNoSpaceErr:
            case kConnectionErr:
            default:
              http_client_log("ERROR: HTTP Header parse error: %d", err);
              goto EXIT;
        }
    }
  }

EXIT:
  http_client_log("Exit: Client exit with err = %d, fd:%d", err, client_fd);
  close(client_fd);
  if(httpHeader) {
    HTTPHeaderClear( httpHeader );
    free(httpHeader);
  }
  mico_rtos_delete_thread(NULL);
}

/*one request may receive multi reply*/
static OSStatus onReceivedData(struct _HTTPHeader_t * inHeader, uint32_t inPos, uint8_t * inData, size_t inLen, void * inUserContext )
{
  if(inPos==0)
  {
       g_data=(char *)malloc(3500);
       memset(g_data,0,3500);
       memcpy(g_data,inData,inLen);
  }
  else
  {
      memcpy(g_data+inPos,inData,inLen);
  }
  return kNoErr;
}
