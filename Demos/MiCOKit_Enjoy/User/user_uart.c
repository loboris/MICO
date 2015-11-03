/**
******************************************************************************
* @file    uart.h 
* @author  Eshen Wang
* @version V1.0.0
* @date    17-Mar-2015
* @brief   This file contains the implementations of uart interfaces for user. 
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

#include <stdio.h>

#include "mico.h"
#include "user_uart.h"

#define user_uart_log(M, ...) custom_log("USER_UART", M, ##__VA_ARGS__)
#define user_uart_log_trace() custom_log_trace("USER_UART")

volatile ring_buffer_t  rx_buffer;
volatile uint8_t        rx_data[USER_UART_BUFFER_LENGTH];

/*******************************************************************************
* INTERFACES
******************************************************************************/

OSStatus user_uartInit(void)
{
 // OSStatus err = kUnknownErr;
  mico_uart_config_t uart_config;
  
  //USART init
  uart_config.baud_rate    = 115200;
  uart_config.data_width   = DATA_WIDTH_8BIT;
  uart_config.parity       = NO_PARITY;
  uart_config.stop_bits    = STOP_BITS_1;
  uart_config.flow_control = FLOW_CONTROL_DISABLED;
  uart_config.flags = UART_WAKEUP_DISABLE;
  ring_buffer_init  ( (ring_buffer_t *)&rx_buffer, (uint8_t *)rx_data, USER_UART_BUFFER_LENGTH );
  
  MicoUartInitialize( USER_UART, &uart_config, (ring_buffer_t *)&rx_buffer );
  
  return kNoErr;
}

OSStatus user_uartSend(unsigned char *inBuf, unsigned int inBufLen)
{
  OSStatus err = kUnknownErr;

  if( (NULL == inBuf) || ( 0 == inBufLen) ){
    err = kParamErr;
    user_uart_log("ERROR: user_uartSend input params error!");
    goto exit;
  }
  
  user_uart_log("KIT => MCU:[%d]=%.*s", inBufLen, inBufLen, inBuf);
  
  err = MicoUartSend(USER_UART, inBuf, inBufLen);
  require_noerr_action( err, exit, user_uart_log("ERROR: send to USART error! err=%d", err) );
  return kNoErr;
  
exit:
  return err;
}

uint32_t user_uartRecv(unsigned char *outBuf, unsigned int getLen)
{
  unsigned int data_len = 0;

  if( (NULL == outBuf) || (0 == getLen) ){
    user_uart_log("ERROR: user_uartRecv input params error!");
    return 0;
  }
  
  if( MicoUartRecv( USER_UART, outBuf, getLen, USER_UART_RECV_TIMEOUT) == kNoErr){
    data_len = getLen;
  }
  else{
    data_len = MicoUartGetLengthInBuffer( USER_UART );
    if(data_len){
      MicoUartRecv(USER_UART, outBuf, data_len, USER_UART_RECV_TIMEOUT);
    }
    else{
      data_len = 0;
    }
  }
  
  return data_len;
}
