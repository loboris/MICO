/**
******************************************************************************
* @file    ext_ambient_light_sensor.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   ambient light sensor control demo.
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

#define ext_ambient_light_sensor_log(M, ...) custom_log("EXT", M, ##__VA_ARGS__)

int application_start( void )
{
  OSStatus err = kNoErr;
  uint16_t apds9930_Prox = 0;
  uint16_t apds9930_Lux = 0;
  
  err = apds9930_sensor_init();
  require_noerr_action( err, exit, ext_ambient_light_sensor_log("ERROR: Unable to Init APDS9930") );
  
  while(1)
  {
     mico_thread_sleep(1); 
     err = apds9930_data_readout(&apds9930_Prox, &apds9930_Lux);
     require_noerr_action( err, exit, ext_ambient_light_sensor_log("ERROR: Can't Read Data") );
     ext_ambient_light_sensor_log("APDS9930  Prox: %.1fmm  Lux: %d", 
                                  (float)(10239-apds9930_Prox)/100, apds9930_Lux);  
  }
  
exit:
  return err;
}


