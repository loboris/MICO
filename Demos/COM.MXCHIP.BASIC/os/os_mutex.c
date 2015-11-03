/**
******************************************************************************
* @file    os_mutex.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   MiCO RTOS mutex demo.
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

#include "MiCO.h" 

#define os_mutex_log(M, ...) custom_log("OS", M, ##__VA_ARGS__)

//#define USE_MUTEX   //Uncommon this macro to see the different result

#ifdef USE_MUTEX
#define MUTEX_LOCK()  mico_rtos_lock_mutex(&mutex)
#define MUTEX_UNLOCK()  mico_rtos_unlock_mutex(&mutex)
#else
#define MUTEX_LOCK() 
#define MUTEX_UNLOCK()
#endif

int g_tickets = 100;//remain tickets
mico_mutex_t  mutex = NULL;

char *p_name1 = "t1";
char *p_name2 = "t2";
char *p_name3 = "t3";
char *p_name4 = "t4";

void run( void *arg )
{
  char *name = (char *)arg;
  char debug[20];

  while(1)
  {
    MUTEX_LOCK();
    if( g_tickets <= 0 ){
      MUTEX_UNLOCK();
      goto exit;
    }

    g_tickets--;
    sprintf( debug, "Thread %s, %d\r\n", name, g_tickets );
    MicoUartSend(STDIO_UART, debug, strlen(debug) );

    MUTEX_UNLOCK();
  } 

exit:
  os_mutex_log( "thread: %s exit now", name );
  mico_rtos_delete_thread(NULL);
}

int application_start( void )
{
  OSStatus err = kNoErr;

  /* Create a mutex*/
  err = mico_rtos_init_mutex( &mutex);
  require_noerr(err, exit);

  /* Create threads */
  err = mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY+1, "t1", run, 0x800, p_name1 );
  require_noerr(err, exit);

  err = mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY+1, "t2", run, 0x800, p_name2 );
  require_noerr(err, exit);

  err = mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY+1, "t3", run, 0x800, p_name3 );
  require_noerr(err, exit);

  err = mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY+1, "t4", run, 0x800, p_name4 );
  require_noerr(err, exit);

exit:
  if( err != kNoErr )
    os_mutex_log( "Thread exit with err: %d", err );

  mico_rtos_delete_thread(NULL);
  return err;  
}


