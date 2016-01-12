/**
******************************************************************************
* @file    uvis25.h
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   temperature and humidity control demo.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __UVIS25_H
#define __UVIS25_H

/* Includes ------------------------------------------------------------------*/

#include "mico_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UVIS25_I2C_PORT
#define UVIS25_I2C_PORT      MICO_I2C_NONE
#endif
  
#define UVIS25_WHO_AM_I                        0x0F
#define UVIS25_CTRL_REG1                       0x20 
#define UVIS25_CTRL_REG2                       0x21
#define UVIS25_CTRL_REG3                       0x22
#define UVIS25_INT_CFG                         0x23
#define UVIS25_INT_SOURCE                      0x24
#define UVIS25_THS_UV                          0x25
#define UVIS25_STATUS_REG                      0x27
#define UVIS25_UV_OUT_REG                      0x28  
  
  
OSStatus uvis25_sensor_init(void);
OSStatus uvis25_Read_Data(float *uv_index);
OSStatus uvis25_sensor_deinit(void);

#endif  /* __UVIS25_H */
