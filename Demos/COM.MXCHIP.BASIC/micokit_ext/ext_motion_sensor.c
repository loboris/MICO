/**
******************************************************************************
* @file    ext_motion_sensor.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   motion sensor control demo.
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

#define ext_motion_sensor_log(M, ...) custom_log("EXT", M, ##__VA_ARGS__)

int application_start( void )
{
  OSStatus err = kNoErr;
  motion_data_t *motion_data;
  
  motion_data = (motion_data_t *)malloc(sizeof(motion_data_t));
  memset(motion_data, 0x0, sizeof(motion_data_t));
  
  err = motion_sensor_init();
  require_noerr( err, exit );
  
  while(1)
  {
     mico_thread_sleep(1); 
     err = motion_sensor_readout(motion_data);
     require_noerr( err, exit );
     
     ext_motion_sensor_log("accel x %d, y %d z %d", motion_data->accel_data.accel_datax,
                                                    motion_data->accel_data.accel_datay,
                                                    motion_data->accel_data.accel_dataz);
     
     ext_motion_sensor_log("gyro  x %d, y %d z %d", motion_data->gyro_data.gyro_datax,
                                                    motion_data->gyro_data.gyro_datay,
                                                    motion_data->gyro_data.gyro_dataz);
     
     ext_motion_sensor_log("mag   x %d, y %d z %d", motion_data->mag_data.mag_datax,
                                                    motion_data->mag_data.mag_datay,
                                                    motion_data->mag_data.mag_dataz);
  }
  
exit:
  return err;
}


