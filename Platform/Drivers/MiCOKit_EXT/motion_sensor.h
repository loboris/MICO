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

#ifndef __MOTION_SENSOR_H_
#define __MOTION_SENSOR_H_

#include "Common.h"

typedef struct _accel_data_t {
  int16_t      accel_datax;
  int16_t      accel_datay;
  int16_t      accel_dataz;  
} accel_data_t;

typedef struct _gyro_data_t {
  int16_t      gyro_datax;
  int16_t      gyro_datay;
  int16_t      gyro_dataz;
} gyro_data_t;

typedef struct _mag_data_t {
  int16_t      mag_datax;
  int16_t      mag_datay;
  int16_t      mag_dataz;
} mag_data_t;

typedef struct _motion_data_t {
  accel_data_t   accel_data;
  gyro_data_t    gyro_data;
  mag_data_t     mag_data;
} motion_data_t;

OSStatus motion_sensor_init(void);
OSStatus motion_sensor_readout(motion_data_t *motion_data);
OSStatus motion_sensor_deinit(void);

#endif  // __TEMP_HUM_SENSOR_H_



