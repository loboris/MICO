/**
******************************************************************************
* @file    RemoteTcpClient.c
* @author  William Xu
* @version V1.0.0
* @date    05-May-2014
* @brief   Create a TCP client thread, and connect to a remote server.
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
#include "SocketUtils.h"
#include "MicoAES.h"
#include "MiCOAppDefine.h"
#include "libemqtt.h"
#include "custom.h"
#include "sitewhere.h"
#include "micokit_ext.h"

#define client_log(M, ...) custom_log("Local client", M, ##__VA_ARGS__)
#define client_log_trace() custom_log_trace("Local client")

#define CLOUD_RETRY  3

/** MQTT client name */

extern char* clientName;

/** Unique hardware id for this device */
extern char hardwareId[HARDWARE_ID_SIZE];

/** Device specification token for hardware configuration */
extern char* specificationToken;

/** Outbound MQTT topic */
extern char* outbound;

extern  char Command1[COMMAND1_SIZE];
/** Inbound system command topic */
extern char System1[SYSTEM1_SIZE];

/** Message buffer */
extern uint8_t buffer[300];

extern void mqtt_client(void);
extern void handleSpecificationCommand(byte* payload, unsigned int length);
extern void handleSystemCommand(byte* payload, unsigned int length);
extern void handleTestEvents(ArduinoCustom_testData testEvents, char* originator);

uint8_t connected_to_ethernet=0;
extern uint8_t MQTT_REGISTERED;
extern bool registered;

extern mqtt_broker_handle_t broker_mqtt;

int send_packet(void* socket_info, const void* buf, unsigned int count)
{
  return send((int)socket_info, buf, count, 0);
}

OSStatus sppWlanCommandProcess(unsigned char *inBuf, int *inBufLen, int inSocketFd, mico_Context_t * const inContext)
{
  client_log_trace();
  (void)inSocketFd;
  (void)inContext;
  OSStatus err = kUnknownErr;
  
  uint8_t *packet_buffer;
  
  packet_buffer = inBuf;
  
  client_log("Packet Header: 0x%x...\n", packet_buffer[0]);
  if(MQTTParseMessageType(packet_buffer) == MQTT_MSG_PUBLISH)
  {
    uint8_t topic[255], msg[1000];
    uint16_t len;
    len = mqtt_parse_pub_topic(packet_buffer, topic);
    topic[len] = '\0'; // for printf
    len = mqtt_parse_publish_msg(packet_buffer, msg);
    msg[len] = '\0';
    
    
    if (strcmp((char*)topic, System1) == 0)
    {
      handleSystemCommand(msg, len);
    }
    else if (strcmp((char*)topic, Command1) == 0)
    {
      handleSpecificationCommand(msg, len);
    }
  }
  
  *inBufLen = 0;
  return err;
}

void sitewhere_main_thread(void *inContext)
{
  client_log_trace();
  OSStatus err = kUnknownErr;
  int len;
  app_context_t *app_context = inContext;
  struct sockaddr_t addr;
  fd_set readfds;
  char ipstr[16];
  struct timeval_t t;
  int remoteTcpClient_fd = -1;
  uint8_t *inDataBuffer = NULL;
  ArduinoCustom_testData testEvents;
  
  uint16_t infrared_data = 0;
  uint16_t light_data = 0;
  int32_t temperature = 0;
  uint32_t humidity = 0;
  
  memset(buffer,0,300);
  
  inDataBuffer = malloc(1024);
  require_action(inDataBuffer, exit, err = kNoMemoryErr);
  
  while(1) {
    if(remoteTcpClient_fd == -1 ) {
      client_log("wifi check...");
      if(app_context->appStatus.isWifiConnected == false){
        sleep(1);
        goto Continue;
      }
      
      client_log("sitewhere connecting...");
      err = gethostbyname("sitewhere.chinacloudapp.cn", (uint8_t *)ipstr, 16);
      require_noerr(err, ReConnWithDelay);
      
      remoteTcpClient_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      
      addr.s_ip = inet_addr(ipstr);
      addr.s_port = 1883;
      
      err = connect(remoteTcpClient_fd, &addr, sizeof(addr));
      require_noerr_quiet(err, ReConnWithDelay);
      client_log("Remote server connected at port: %d, fd: %d",  1883,
                 remoteTcpClient_fd);
      broker_mqtt.socket_info =(void *)remoteTcpClient_fd;
      broker_mqtt.send = send_packet;
      
      mqtt_client();
      
      connected_to_ethernet =1;
      app_context->appStatus.isCloudConnected = true;
    }
    else{
      MicoGpioOutputTrigger(MICO_RF_LED);
      
      FD_ZERO(&readfds);
      FD_SET(remoteTcpClient_fd, &readfds);
      t.tv_sec = 2;
      t.tv_usec = 0;
      select(1, &readfds, NULL, NULL, &t);
      
      /*recv wlan data using remote client fd*/
      if(MQTT_REGISTERED==1)
      {
        if (FD_ISSET(remoteTcpClient_fd, &readfds)) {
          len = recv(remoteTcpClient_fd, inDataBuffer, 1024, 0);
          if(len <= 0) {
            client_log("Remote client closed, fd: %d", remoteTcpClient_fd);
            connected_to_ethernet =0;
            goto ReConnWithDelay;
          }
          client_log("recv: %d", len);
          sppWlanCommandProcess(inDataBuffer, &len, remoteTcpClient_fd, app_context->mico_context);
        }
        else{ // no message received, just upload test data every 2s.
          if(registered){
            // alert
            if (len = sw_alert(hardwareId, clientName, "is alive", NULL, buffer, sizeof(buffer), NULL)) {
              mqtt_publish(&broker_mqtt,outbound,(char*)buffer,len,0);
              client_log("Sent alert.");
            }
            // location
            if (len = sw_location(hardwareId, 31.00f, 121.00f, 0.0f, NULL, buffer, sizeof(buffer), NULL)) {
              mqtt_publish(&broker_mqtt,outbound,(char*)buffer,len,0);
              client_log("Sent location.");
            }
            
            // upload test data
            temp_hum_sensor_read(&temperature, &humidity);
            infrared_reflective_read(&infrared_data);
            light_sensor_read(&light_data);
            if((temperature > 0) && (humidity > 0)){
              app_context->appStatus.user_context.status.temperature = temperature;
              app_context->appStatus.user_context.status.humidity = humidity;
            }
            app_context->appStatus.user_context.status.light_sensor_data = light_data;
            app_context->appStatus.user_context.status.infrared_reflective_data = infrared_data;
            
            testEvents.temperature = app_context->appStatus.user_context.status.temperature;
            testEvents.humidity = app_context->appStatus.user_context.status.humidity;
            testEvents.infrared = app_context->appStatus.user_context.status.infrared_reflective_data;
            testEvents.light = app_context->appStatus.user_context.status.light_sensor_data;
            handleTestEvents(testEvents, NULL);
            client_log("Sent test data.");
          }
        }
      }
      
    Continue:
      continue;
      
    ReConnWithDelay:
      
      app_context->appStatus.isCloudConnected = false;
      if(remoteTcpClient_fd != -1){
        SocketClose(&remoteTcpClient_fd);
      }
      sleep(CLOUD_RETRY);
    }
  }
  
exit:
  if(inDataBuffer) free(inDataBuffer);
  client_log("Exit: local client exit with err = %d", err);
  mico_rtos_delete_thread(NULL);
  return;
}

