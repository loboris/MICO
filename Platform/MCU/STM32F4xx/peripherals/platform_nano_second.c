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

/******************************************************
*                    Constants
******************************************************/

const uint32_t NR_NS_PER_SECOND     = 1*1000*1000*1000;

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
#define CYCLE_COUNTING_INIT() \
    do \
    { \
        /* enable DWT hardware and cycle counting */ \
        CoreDebug->DEMCR = CoreDebug->DEMCR | CoreDebug_DEMCR_TRCENA_Msk; \
        /* reset a counter */ \
        DWT->CYCCNT = 0;  \
        /* enable the counter */ \
        DWT->CTRL = (DWT->CTRL | DWT_CTRL_CYCCNTENA_Msk) ; \
    } \
    while(0)

#define absolute_value(a) ( (a) < 0 ) ? -(a) : (a)
          
/******************************************************
*               Variables Definitions
******************************************************/

static uint32_t nsclock_nsec =0;
static uint32_t nsclock_sec =0;
static uint32_t prev_cycles = 0;
static uint32_t ns_divisor = 0;

/******************************************************
*               Function Declarations
******************************************************/

/******************************************************
*               Function Definitions
******************************************************/

uint64_t platform_get_nanosecond_clock_value(void)
{
    uint64_t nanos;
    uint32_t cycles;
    uint32_t diff;
    cycles = DWT->CYCCNT;

    /* add values to the ns part of the time which can be divided by ns_divisor */
    /* every time such value is added we will increment our clock by 1 / (MCU_CLOCK_HZ / ns_divisor).
     * For example, for MCU_CLOCK_HZ of 120MHz, ns_divisor of 3, the granularity is 25ns = 1 / (120MHz/3) or 1 / (40MHz).
     * Note that the cycle counter is running on the CPU frequency.
     */
    /* Values will be a multiple of ns_divisor (e.g. 1*ns_divisor, 2*ns_divisor, 3*ns_divisor, ...).
     * Roll-over taken care of by subtraction
     */
    diff = cycles - prev_cycles;
    {
        const uint32_t ns_per_unit = NR_NS_PER_SECOND / (MCU_CLOCK_HZ / ns_divisor);
        nsclock_nsec += ( (uint64_t)( diff / ns_divisor ) * ns_per_unit);
    }

    /* when ns counter rolls over, add one second */
    if( nsclock_nsec >= NR_NS_PER_SECOND )
    {
        /* Accumulate seconds portion of nanoseconds. */
        nsclock_sec += (uint32_t)(nsclock_nsec / NR_NS_PER_SECOND);
        /* Store remaining nanoseconds. */
        nsclock_nsec = nsclock_nsec - (nsclock_nsec / NR_NS_PER_SECOND) * NR_NS_PER_SECOND;
    }
    /* Subtract off unaccounted for cycles, so that they are accounted next time. */
    prev_cycles = cycles - (diff % ns_divisor);

    nanos = nsclock_sec;
    nanos *= NR_NS_PER_SECOND;
    nanos += nsclock_nsec;
    return nanos;
}


void platform_deinit_nanosecond_clock(void)
{
    nsclock_nsec = 0;
    nsclock_sec = 0;
}

void platform_reset_nanosecond_clock(void)
{
    nsclock_nsec = 0;
    nsclock_sec = 0;
}

void platform_init_nanosecond_clock(void)
{
    CYCLE_COUNTING_INIT();
    nsclock_nsec = 0;
    nsclock_sec = 0;
    ns_divisor = 0;
    /* Calculate a divisor that will produce an
     * integer nanosecond value for the CPU
     * clock frequency.
     */
    while (NR_NS_PER_SECOND % (MCU_CLOCK_HZ / ++ns_divisor) != 0);

}


void platform_nanosecond_delay( uint64_t delayns )
{
  uint64_t currentns = 0;
  
  platform_init_nanosecond_clock();
  
  do
  {
    currentns = platform_get_nanosecond_clock_value();
  }
  while(currentns < delayns);
  
}

