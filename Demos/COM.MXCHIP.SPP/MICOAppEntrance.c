/**
  ******************************************************************************
  * @file    MICOAppEntrance.c 
  * @author  William Xu
  * @version V1.0.0
  * @date    05-May-2014
  * @brief   Mico application entrance, addd user application functons and threads.
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

#include "mico.h"

#include "StringUtils.h"
#include "SppProtocol.h"
#include "cfunctions.h"
#include "cppfunctions.h"
#include "MICOAPPDefine.h"

#define app_log(M, ...) custom_log("APP", M, ##__VA_ARGS__)
#define app_log_trace() custom_log_trace("APP")

volatile ring_buffer_t  rx_buffer;
volatile uint8_t        rx_data[UART_BUFFER_LENGTH];

extern void localTcpServer_thread(void *inContext);

extern void remoteTcpClient_thread(void *inContext);

extern void uartRecv_thread(void *inContext);

extern OSStatus MICOStartBonjourService ( WiFi_Interface interface, app_context_t * const inContext );

/* MICO system callback: Restore default configuration provided by application */
void appRestoreDefault_callback(void * const user_config_data, uint32_t size)
{
  UNUSED_PARAMETER(size);
  application_config_t* appConfig = user_config_data;
  appConfig->configDataVer = CONFIGURATION_VERSION;
  appConfig->localServerPort = LOCAL_PORT;
  appConfig->localServerEnable = true;
  appConfig->USART_BaudRate = 115200;
  appConfig->remoteServerEnable = true;
  sprintf(appConfig->remoteServerDomain, DEAFULT_REMOTE_SERVER);
  appConfig->remoteServerPort = DEFAULT_REMOTE_SERVER_PORT;
}

int application_start(void)
{
  app_log_trace();
  OSStatus err = kNoErr;
  mico_uart_config_t uart_config;
  app_context_t* app_context;
  mico_Context_t* mico_context;

  /* Create application context */
  app_context = ( app_context_t *)calloc(1, sizeof(app_context_t) );
  require_action( app_context, exit, err = kNoMemoryErr );

  /* Create mico system context and read application's config data from flash */
  mico_context = mico_system_context_init( sizeof( application_config_t) );
  app_context->appConfig = mico_system_context_get_user_data( mico_context );

  /* mico system initialize */
  err = mico_system_init( mico_context );
  require_noerr( err, exit );

  /* Bonjour for service searching */
  MICOStartBonjourService( Station, app_context );

  /* Protocol initialize */
  sppProtocolInit( app_context );

  /*UART receive thread*/
  uart_config.baud_rate    = app_context->appConfig->USART_BaudRate;
  uart_config.data_width   = DATA_WIDTH_8BIT;
  uart_config.parity       = NO_PARITY;
  uart_config.stop_bits    = STOP_BITS_1;
  uart_config.flow_control = FLOW_CONTROL_DISABLED;
  if(mico_context->flashContentInRam.micoSystemConfig.mcuPowerSaveEnable == true)
    uart_config.flags = UART_WAKEUP_ENABLE;
  else
    uart_config.flags = UART_WAKEUP_DISABLE;
  ring_buffer_init  ( (ring_buffer_t *)&rx_buffer, (uint8_t *)rx_data, UART_BUFFER_LENGTH );
  MicoUartInitialize( UART_FOR_APP, &uart_config, (ring_buffer_t *)&rx_buffer );
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "UART Recv", uartRecv_thread, STACK_SIZE_UART_RECV_THREAD, (void*)app_context );
  require_noerr_action( err, exit, app_log("ERROR: Unable to start the uart recv thread.") );

 /* Local TCP server thread */
 if(app_context->appConfig->localServerEnable == true){
   err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Local Server", localTcpServer_thread, STACK_SIZE_LOCAL_TCP_SERVER_THREAD, (void*)app_context );
   require_noerr_action( err, exit, app_log("ERROR: Unable to start the local server thread.") );
 }

  /* Remote TCP client thread */
 if(app_context->appConfig->remoteServerEnable == true){
   err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Remote Client", remoteTcpClient_thread, STACK_SIZE_REMOTE_TCP_CLIENT_THREAD, (void*)app_context );
   require_noerr_action( err, exit, app_log("ERROR: Unable to start the remote client thread.") );
 }

exit:
  mico_rtos_delete_thread(NULL);
  return err;
}
