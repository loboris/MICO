/**
******************************************************************************
* @file    ext_environmental_sensor.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   environmental sensor control demo.
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
#include "temp_hum_sensor\BME280\bme280_user.h"

#define ext_environmental_sensor_log(M, ...) custom_log("EXT", M, ##__VA_ARGS__)

int application_start( void )
{
  OSStatus err = kNoErr;
  int32_t bme280_temp = 0;
  uint32_t bme280_hum = 0;
  uint32_t bme280_press = 0;
  
  err = bme280_sensor_init();
  require_noerr_action( err, exit, ext_environmental_sensor_log("ERROR: Unable to Init BME280") );
  
  while(1)
  {
     /* BME280 Read Data Delay must >= 1s*/
     mico_thread_sleep(1); 
     
     err = bme280_data_readout(&bme280_temp, &bme280_press, &bme280_hum);
     require_noerr_action( err, exit, ext_environmental_sensor_log("ERROR: Can't Read Data") );
     ext_environmental_sensor_log("BME280  T: %3.1fC  H: %3.1f%%  P: %5.2fkPa", 
                      (float)bme280_temp/100, (float)bme280_hum/1024, (float)bme280_press/1000);  
  }
  
exit:
  return err;
}


