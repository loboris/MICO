/**
******************************************************************************
* @file    os_queue.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   MiCO RTOS queue demo.
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

#define os_queue_log(M, ...) custom_log("OS", M, ##__VA_ARGS__)

static mico_queue_t os_queue = NULL;
typedef struct _msg
{
  int value;
} msg_t;

void receiver_thread(void *arg)
{
  UNUSED_PARAMETER( arg );

  OSStatus err;
  msg_t received ={ 0 };
  
  while(1)
  {
    /*Wait until queue has data*/
    err = mico_rtos_pop_from_queue( &os_queue, &received, MICO_WAIT_FOREVER);
    require_noerr( err, exit );

    os_queue_log( "Received data from queue:value = %d",received.value );
  }

exit:
  if( err != kNoErr )
    os_queue_log( "Receiver exit with err: %d", err );

  mico_rtos_delete_thread( NULL );
}

void sender_thread(void *arg)
{
  UNUSED_PARAMETER( arg );
  OSStatus err = kNoErr;

  msg_t my_message = { 0 };

  while(1)
  {
    my_message.value++;
    mico_rtos_push_to_queue( &os_queue, &my_message, MICO_WAIT_FOREVER );
    require_noerr( err, exit );

    os_queue_log( "send data to queue" );
    mico_thread_sleep( 1 );
  }

exit:
  if( err != kNoErr )
    os_queue_log( "Sender exit with err: %d", err );

  mico_rtos_delete_thread( NULL );
}

int application_start( void )
{
  OSStatus err = kNoErr;

  err = mico_rtos_init_queue( &os_queue, "queue", sizeof(msg_t), 3 );
  require_noerr( err, exit );

  err = mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "receiver", receiver_thread, 500, NULL );
  require_noerr( err, exit );

  err = mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "sender", sender_thread, 500, NULL );
  require_noerr( err, exit );
  
exit:
  if( err != kNoErr )
    os_queue_log( "Thread exit with err: %d", err );

  mico_rtos_delete_thread( NULL );

  return err;
}


