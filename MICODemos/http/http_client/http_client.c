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
static void onClearData( struct _HTTPHeader_t * inHeader, void * inUserContext );


static mico_semaphore_t wait_sem = NULL;

typedef struct _http_context_t{
  char *content;
  uint64_t content_length;
} http_context_t;

void simple_http_get( char* host, char* query );
void simple_https_get( char* host, char* query );

#define SIMPLE_GET_REQUEST \
    "GET / HTTP/1.1\r\n" \
    "Host: www.baidu.com\r\n" \
    "Connection: close\r\n" \
    "\r\n"

static void micoNotify_WifiStatusHandler( WiFiEvent status, void* const inContext )
{
  UNUSED_PARAMETER(inContext);
  switch (status) {
  case NOTIFY_STATION_UP:
    mico_rtos_set_semaphore( &wait_sem );
    break;
  case NOTIFY_STATION_DOWN:
    break;
  }
}

int application_start( void )
{
  OSStatus err = kNoErr;
  
  mico_rtos_init_semaphore( &wait_sem,1 );
  
  /*Register user function for MiCO nitification: WiFi status changed */
  err = mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED, (void *)micoNotify_WifiStatusHandler, NULL );
  require_noerr( err, exit ); 
  
  /* Start MiCO system functions according to mico_config.h */
  err = mico_system_init( mico_system_context_init( 0 ) );
  require_noerr( err, exit );
  
  /* Wait for wlan connection*/
  mico_rtos_get_semaphore( &wait_sem, MICO_WAIT_FOREVER );
  http_client_log( "wifi connected successful" );

  /* Read http data from server */
  simple_http_get( "www.baidu.com", SIMPLE_GET_REQUEST );
  simple_https_get( "www.baidu.com", SIMPLE_GET_REQUEST );

exit:
  mico_rtos_delete_thread( NULL );
  return err;
}


void simple_http_get( char* host, char* query )
{
  OSStatus err;
  int client_fd = -1;
  fd_set readfds;
  char ipstr[16];
  struct sockaddr_t addr;
  HTTPHeader_t *httpHeader = NULL;
  http_context_t context = { NULL, 0 };

  err = gethostbyname(host, (uint8_t *)ipstr, 16);
  require_noerr(err, exit);
  http_client_log("HTTP server address: host:%s, ip: %s", host, ipstr);

  /*HTTPHeaderCreateWithCallback set some callback functions */
  httpHeader = HTTPHeaderCreateWithCallback( 1024, onReceivedData, onClearData, &context );
  require_action( httpHeader, exit, err = kNoMemoryErr );
  
  client_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  addr.s_ip = inet_addr( ipstr );
  addr.s_port = 80;
  err = connect( client_fd, &addr, sizeof(addr) ); 
  require_noerr_string( err, exit, "connect http server failed" );

  /* Send HTTP Request */
  send( client_fd, query, strlen(query), 0 );

  FD_ZERO( &readfds );
  FD_SET( client_fd, &readfds );
  
  select( client_fd + 1, &readfds, NULL, NULL, NULL );
  if( FD_ISSET( client_fd, &readfds ) )
  {
    /*parse header*/
    err = SocketReadHTTPHeader( client_fd, httpHeader );
    switch ( err )
    {
      case kNoErr:
        PrintHTTPHeader( httpHeader );
        err = SocketReadHTTPBody( client_fd, httpHeader );/*get body data*/
        require_noerr( err, exit );
        /*get data and print*/
        http_client_log( "Content Data: %s", context.content );
        break;
      case EWOULDBLOCK:
      case kNoSpaceErr:
      case kConnectionErr:
      default:
        http_client_log("ERROR: HTTP Header parse error: %d", err);
        break;
    }
  }

exit:
  http_client_log( "Exit: Client exit with err = %d, fd:%d", err, client_fd );
  SocketClose( &client_fd );
  HTTPHeaderDestory( &httpHeader );
}

void simple_https_get( char* host, char* query )
{
  OSStatus err;
  int client_fd = -1;
  int ssl_errno = 0;
  mico_ssl_t client_ssl = NULL;
  fd_set readfds;
  char ipstr[16];
  struct sockaddr_t addr;
  HTTPHeader_t *httpHeader = NULL;
  http_context_t context = { NULL, 0 };

  err = gethostbyname(host, (uint8_t *)ipstr, 16);
  require_noerr(err, exit);
  http_client_log("HTTP server address: host:%s, ip: %s", host, ipstr);

  /*HTTPHeaderCreateWithCallback set some callback functions */
  httpHeader = HTTPHeaderCreateWithCallback( 1024, onReceivedData, onClearData, &context );
  require_action( httpHeader, exit, err = kNoMemoryErr );
  
  client_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  addr.s_ip = inet_addr( ipstr );
  addr.s_port = 443;
  err = connect( client_fd, &addr, sizeof(addr) ); 
  require_noerr_string( err, exit, "connect http server failed" );

  client_ssl = ssl_connect( client_fd, 0, NULL, &ssl_errno );
  require_string( client_ssl != NULL, exit, "ERROR: ssl disconnect" );

  /* Send HTTP Request */
  ssl_send( client_ssl, query, strlen(query) );

  FD_ZERO( &readfds );
  FD_SET( client_fd, &readfds );
  
  select( client_fd + 1, &readfds, NULL, NULL, NULL );
  if( FD_ISSET( client_fd, &readfds ) )
  {
    /*parse header*/
    err = SocketReadHTTPSHeader( client_ssl, httpHeader );
    switch ( err )
    {
      case kNoErr:
        PrintHTTPHeader( httpHeader );
        err = SocketReadHTTPSBody( client_ssl, httpHeader );/*get body data*/
        require_noerr( err, exit );
        /*get data and print*/
        http_client_log( "Content Data: %s", context.content );
        break;
      case EWOULDBLOCK:
      case kNoSpaceErr:
      case kConnectionErr:
      default:
        http_client_log("ERROR: HTTP Header parse error: %d", err);
        break;
    }
  }

exit:
  http_client_log( "Exit: Client exit with err = %d, fd:%d", err, client_fd );
  if( client_ssl ) ssl_close( client_ssl );
  SocketClose( &client_fd );
  HTTPHeaderDestory( &httpHeader );
}

/*one request may receive multi reply*/
static OSStatus onReceivedData( struct _HTTPHeader_t * inHeader, uint32_t inPos, uint8_t * inData, size_t inLen, void * inUserContext )
{
  OSStatus err = kNoErr;
  http_context_t *context = inUserContext;
  if( inHeader->chunkedData == false){ //Extra data with a content length value
    if( inPos == 0 ){
      context->content = calloc( inHeader->contentLength + 1, sizeof(uint8_t) );
      require_action( context->content, exit, err = kNoMemoryErr );
      context->content_length = inHeader->contentLength;
      
    }
    memcpy( context->content + inPos, inData, inLen );
  }else{ //extra data use a chunked data protocol
    http_client_log("This is a chunked data, %d", inLen);
    if( inPos == 0 ){
      context->content = calloc( inHeader->contentLength + 1, sizeof(uint8_t) );
      require_action( context->content, exit, err = kNoMemoryErr );
      context->content_length = inHeader->contentLength;
    }else{
      context->content_length += inLen;
      context->content = realloc(context->content, context->content_length + 1);
      require_action( context->content, exit, err = kNoMemoryErr );
    }
    memcpy( context->content + inPos, inData, inLen );
  }
  
exit:
  return err;
}

/* Called when HTTPHeaderClear is called */
static void onClearData( struct _HTTPHeader_t * inHeader, void * inUserContext )
{
  UNUSED_PARAMETER( inHeader );
  http_context_t *context = inUserContext;
  if( context->content ) {
    free( context->content );
    context->content = NULL;
  }
}



