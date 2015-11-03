/**
******************************************************************************
* @file    bma2x2_user.h
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   bma2x2 sensor control demo.
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
#ifndef __BMA2x2_USER_H_
#define __BMA2x2_USER_H_

#include "mico_platform.h"

#define s16 int16_t
#define u32 uint32_t

#ifdef USE_MiCOKit_EXT
  #include "micokit_ext_def.h"
  #define BMA2x2_I2C_DEVICE      Arduino_I2C
#else
  #define BMA2x2_I2C_DEVICE      MICO_I2C_NONE
#endif

OSStatus bma2x2_sensor_init(void);
OSStatus bma2x2_data_readout(s16 *v_accel_x_s16, s16 *v_accel_y_s16, s16 *v_accel_z_s16);
OSStatus bma2x2_sensor_deinit(void);

#endif