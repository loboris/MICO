/**
******************************************************************************
* @file    ext_rgb_led.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   rgb led control demo.
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

#define ext_rgb_led_log(M, ...) custom_log("EXT", M, ##__VA_ARGS__)

#define COLOR_MODE RGB_MODE

#define RGB_MODE 0
#define HSB_MODE 1

#if (COLOR_MODE == RGB_MODE)
int application_start( void )
{
  ext_rgb_led_log("rgb led conrtol demo(RGB_MODE)");
  /*init RGB LED(P9813)*/
  rgb_led_init();
  while(1)
  {
    /*open red led,#FF0000*/
    rgb_led_open(255, 0, 0);
    mico_thread_sleep(1);
    /*open green led #00FF00*/
    rgb_led_open(0, 255, 0);
    mico_thread_sleep(1); 
    /*open blue led,#0000FF*/
    rgb_led_open(0, 0, 255);
    mico_thread_sleep(1);
  }
}
#else
int application_start( void )
{
  ext_rgb_led_log("rgb led conrtol demo(HBS_MODE)");
  /*init RGB LED(P9813)*/
  rgb_led_init();
  while(1)
  {
    /*open red led*/
    hsb2rgb_led_open(0, 100, 100);
    mico_thread_sleep(1);
    /*open green led*/
    hsb2rgb_led_open(120, 100, 100);
    mico_thread_sleep(1);
    /*open blue led*/
    hsb2rgb_led_open(240, 100, 100);
    mico_thread_sleep(1);
  }
}
#endif

