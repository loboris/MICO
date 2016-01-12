/**
******************************************************************************
* @file    http_server.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   First MiCO application to setup http server!
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
#include "app_httpd.h"

#define http_server_log(format, ...)  custom_log("httpserver", format, ##__VA_ARGS__)

int application_start( void )
{
  /* Start MiCO system functions according to mico_config.h*/
  mico_system_init( mico_system_context_init( 0 ) );
  
  /* Output on debug serial port */
  http_server_log( "http server Demo!" );
  
  /* start http server thread */
  app_httpd_start();
  
  mico_rtos_delete_thread( NULL );
  return kNoErr;
}


