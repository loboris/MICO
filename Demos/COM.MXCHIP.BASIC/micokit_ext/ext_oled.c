/**
******************************************************************************
* @file    ext_oled.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   OLED control demo.
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
#define ext_oled_log(M, ...) custom_log("EXT", M, ##__VA_ARGS__)
int application_start( void )
{
  char time[16]={0};
  ext_oled_log("OLED control demo!");
  /*Init Organic Light-Emitting Diode*/
  OLED_Init();
  /*Starting position display string at the first row*/
  OLED_ShowString(0, 0, "MXCHIP Inc.");
  /*Starting position display string at the second row*/
  OLED_ShowString(0, 2, "MiCO run time:");
  while(1)
  {
    memset(time, 0, sizeof(time));
    /*Gets time in miiliseconds since MiCO RTOS start*/
    sprintf(time, "%d ms", mico_get_time());
    /*Starting position display time at the third row*/
    OLED_ShowString(0, 4, (uint8_t *)time);
    mico_thread_sleep(1);
  }
  //OLED_Clear();
  return 1;
}


