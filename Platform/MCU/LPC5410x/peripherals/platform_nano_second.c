/**
******************************************************************************
* @file    platform_nano_second.c 
* @author  William Xu
* @version V1.0.0
* @date    05-May-2014
* @brief   This file provide time delay function using nano second.
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

#include "platform_peripheral.h"
#include "chip.h"

/******************************************************
*                    Constants
******************************************************/

/******************************************************
*                   Enumerations
******************************************************/

/******************************************************
*                 Type Definitions
******************************************************/

/******************************************************
*                    Structures
******************************************************/

/******************************************************
 *                      Macros
 ******************************************************/
          
/******************************************************
*               Variables Definitions
******************************************************/


/******************************************************
*               Function Declarations
******************************************************/

/******************************************************
*               Function Definitions
******************************************************/


void platform_init_nanosecond_clock(void)
{
  Chip_TIMER_Init(LPC_TIMER4);  
  Chip_TIMER_PrescaleSet(LPC_TIMER4,0);  
  Chip_TIMER_Reset(LPC_TIMER4);
  Chip_TIMER_Enable(LPC_TIMER4);
}


void platform_nanosecond_delay( uint64_t delayns )//max 1s
{
  uint32_t currentns = 0;
  uint32_t delay10ns = ((delayns*96)/1000);
  if(delay10ns>100000000)
    delay10ns=100000000;
  
  platform_init_nanosecond_clock();
  
  do
  {
    currentns = Chip_TIMER_ReadCount(LPC_TIMER4);
  }
  while(currentns < delay10ns);
  
  Chip_TIMER_Disable(LPC_TIMER4);
  
}

