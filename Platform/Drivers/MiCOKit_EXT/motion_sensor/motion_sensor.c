/**
******************************************************************************
* @file    motion_sensor.c
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
#include "bma2x2/bma2x2_user.h"
#include "bmg160/bmg160_user.h"
#include "bmm050/bmm050_user.h"
#include "motion_sensor.h"

OSStatus motion_sensor_init(void)
{
  OSStatus err = kUnknownErr;
  
  /*low-g acceleration sensor init*/
  err = bma2x2_sensor_init();
  require_noerr( err, exit );
  
  /*triaxial angular rate sensor init*/
  err = bmg160_sensor_init();
  require_noerr( err, exit );
  
  /* triaxial geomagnetic sensor init*/
  err = bmm050_sensor_init();
  require_noerr( err, exit );
  
exit:
  return err;
}

OSStatus motion_sensor_readout(motion_data_t *motion_data)
{
  OSStatus err = kUnknownErr;
  
  /*low-g acceleration sensor data read*/
  err = bma2x2_data_readout(&motion_data->accel_data.accel_datax,
                            &motion_data->accel_data.accel_datay, 
                            &motion_data->accel_data.accel_dataz);
  require_noerr( err, exit );
  
  /*triaxial angular rate sensor data read*/
  err = bmg160_data_readout(&motion_data->gyro_data.gyro_datax,
                            &motion_data->gyro_data.gyro_datay,
                            &motion_data->gyro_data.gyro_dataz);
  require_noerr( err, exit );
  
  /* triaxial geomagnetic sensor data read*/
  err = bmm050_data_readout(&motion_data->mag_data.mag_dataz,
                            &motion_data->mag_data.mag_datay,
                            &motion_data->mag_data.mag_dataz);
  require_noerr( err, exit );

exit:
  return err;
}

OSStatus motion_sensor_deinit(void)
{
  OSStatus err = kUnknownErr;
  
  /*low-g acceleration sensor deinit*/
  err = bma2x2_sensor_deinit();
  require_noerr( err, exit );
  
  /*triaxial angular rate sensor deinit*/
  err = bmg160_sensor_deinit();
  require_noerr( err, exit );
  
  /* triaxial geomagnetic sensor deinit*/
  err = bmm050_sensor_deinit();
  require_noerr( err, exit );
  
exit:
  return err;
}



