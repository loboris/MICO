/**
******************************************************************************
* @file    uart.h 
* @author  Eshen Wang
* @version V1.0.0
* @date    17-Mar-2015
* @brief   This header contains the uart interfaces for user. 
  operation
******************************************************************************
* @attention
*
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
* TIME. AS A RESULT, MXCHIP Inc. SHALL NOT BE HELD LIABLE FOR ANY
* DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
* FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
* CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
* <h2><center>&copy; COPYRIGHT 2014 MXCHIP Inc.</center></h2>
******************************************************************************
*/ 

#ifndef __USER_UART_H_
#define __USER_UART_H_

#include "mico.h"

/*******************************************************************************
 * DEFINES
 ******************************************************************************/
#ifdef USE_MiCOKit_EXT
  #include "micokit_ext_def.h"
  #define USER_UART                     (Arduino_UART)
#else
  #define USER_UART                     (MICO_UART_NONE)
#endif

#define USER_UART_RECV_TIMEOUT              100
#define USER_UART_ONE_PACKAGE_LENGTH        32
#define USER_UART_BUFFER_LENGTH             256
   
#define STACK_SIZE_USART_RECV_THREAD        0x500

/*******************************************************************************
 * INTERFACES
 ******************************************************************************/

OSStatus user_uartInit(void);
OSStatus user_uartSend(unsigned char *inBuf, unsigned int inBufLen);
uint32_t user_uartRecv(unsigned char *outBuf, unsigned int getLen);

#endif  // __USER_UART_H_
