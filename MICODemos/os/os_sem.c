/**
******************************************************************************
* @file    os_sem.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   MiCO RTOS thread control demo.
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

#define os_sem_log(M, ...) custom_log("OS", M, ##__VA_ARGS__)

static mico_semaphore_t os_sem = NULL;

void release_thread(void *arg)
{
  UNUSED_PARAMETER( arg );

  while(1){
    os_sem_log( "release semaphore" );
    mico_rtos_set_semaphore( &os_sem );
    mico_thread_sleep( 3 );
  }
  
  mico_rtos_delete_thread(NULL);
}

int application_start( void )
{
  OSStatus err = kNoErr;
  os_sem_log( "test binary semaphore" );

  err = mico_rtos_init_semaphore( &os_sem, 1 );//0/1 binary semaphore || 0/N semaphore
  require_noerr( err, exit );

  err = mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "release sem", release_thread, 500, NULL );
  require_noerr( err, exit );

  while(1){
    mico_rtos_get_semaphore( &os_sem, MICO_WAIT_FOREVER );//wait until get semaphore 
    os_sem_log( "get semaphore" );  
  }

exit:
  if( err != kNoErr )
    os_sem_log( "Thread exit with err: %d", err );

  if( os_sem != NULL )
    mico_rtos_deinit_semaphore( &os_sem );

  mico_rtos_delete_thread( NULL );
  return err;
}



