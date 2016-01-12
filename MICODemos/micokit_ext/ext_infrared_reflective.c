/**
******************************************************************************
* @file    ext_infrared_refective.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   infrared refective control demo.
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
#include "micokit_ext.h"

#define ext_infrared_refective_log(M, ...) custom_log("EXT", M, ##__VA_ARGS__)

int application_start( void )
{
  OSStatus err = kNoErr;
  uint16_t infrared_reflective_data = 0;
  
  /*init infrared refective*/ 
  err = infrared_reflective_init();
  require_noerr_action( err, exit, ext_infrared_refective_log("ERROR: Unable to Init infrared refective") );
  
  while(1)
  {
    err = infrared_reflective_read(&infrared_reflective_data);
    require_noerr_action( err, exit, ext_infrared_refective_log("ERROR: Can't infrared refectiver read data") );
    ext_infrared_refective_log("infrared reflective date: %d", infrared_reflective_data);
    mico_thread_sleep(1);
  }
exit:
  return err; 
}



