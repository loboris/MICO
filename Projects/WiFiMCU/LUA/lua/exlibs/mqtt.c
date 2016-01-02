
 /* mqtt.c
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"
#include "user_config.h"
   
#include "platform.h"
#include "mico_platform.h"
#include "platform_peripheral.h"

#include "MiCO.h" 
#include "MQTTClient.h"

#define MQTT_CMD_TIMEOUT 5000  // 5s
#define MQTT_YIELD_TMIE  1000  // 1s
#define MAX_MQTT_NUM     3

typedef struct {
  Client  c;
  Network n;
  ssl_opts ssl_settings;
  MQTTPacket_connectData connectData;
  
  char     *pServer;
  int      port;
  bool     reqStart;
  bool     reqClose;
  bool     reqSubscribe[MAX_MESSAGE_HANDLERS];
  bool     requnSubscribe[MAX_MESSAGE_HANDLERS];
  bool     reqPublish;
  bool     req_goto_disconect;
  uint32_t keepaliveTick;
  int      qos;
  char     *pTopic[MAX_MESSAGE_HANDLERS];
  char     *pPubTopic;
  char     *pData;
  size_t   pDataLen;
  int      cb_ref_connect;
  int      cb_ref_offline;
  int      cb_ref_message;
  uint8_t  conn_retry;
}mqtt_t;

extern mico_queue_t os_queue;
static mqtt_t *pmqtt[MAX_MQTT_NUM];
static bool mqtt_debug             = false;
static bool mqtt_thread_is_started = false;
static lua_State *gL               = NULL;
static uint8_t max_conn_retry      = 10;

#define mqtt_log(M, ...) if (mqtt_debug == true) printf(M, ##__VA_ARGS__)
//#define mqtt_log(M, ...) printf(M, ##__VA_ARGS__)
//#define mqtt_log(M, ...)

//-----------------------------------------
static void messageArrived(MessageData* md)
{
  char* topic; 
  int   tlen;
  
  //mqtt_log("[mqtt>>] FreeMem=%d\r\n",MicoGetMemoryInfo()->free_memory);
  tlen = md->topicName->lenstring.len;
  topic = (char*)malloc(tlen+1);
  memcpy(topic,md->topicName->lenstring.data,tlen);
  *(topic+tlen) = '\0';
  
  MQTTMessage* message = md->message;
  mqtt_log("[mqtt: ] messageArrived: [topic: %s] [len=%d] [%.*s]\r\n", topic,
           (int)message->payloadlen,
           (int)message->payloadlen, (char*)message->payload);
  
  for (int i=0;i<MAX_MQTT_NUM;i++)
  {
    if (pmqtt[i] == NULL) continue;
    if (pmqtt[i]->cb_ref_message == LUA_NOREF) continue;
    
    for (int idx=0; idx<MAX_MESSAGE_HANDLERS;idx++) {
      if (pmqtt[i]->pTopic[idx] != NULL) {
        if (strcmp(pmqtt[i]->pTopic[idx], topic) == 0) {
          if (mico_rtos_is_queue_full(&os_queue)) {
            mqtt_log("[mqtt:%d] %d LUA Queue full!\r\n", i, idx);
          }
          else {
            // Queue messageArrived callback function
            //----------------------------------------------------------------------
            queue_msg_t msg;
            msg.L = gL;
            msg.source = onMQTTmsg;

            msg.para3 = (uint8_t*)malloc(tlen+1);
            msg.para4 = (uint8_t*)malloc((int)message->payloadlen);
            memcpy(msg.para3, (uint8_t*)topic, tlen);
            *(msg.para3+tlen) = '\0';
            memcpy(msg.para4, (uint8_t*)message->payload, (int)message->payloadlen);
            
            msg.para1 = (tlen << 16) + (int)message->payloadlen;
            msg.para2 = pmqtt[i]->cb_ref_message;
            mico_rtos_push_to_queue( &os_queue, &msg,0);
            //----------------------------------------------------------------------
          }
        }
      }
    }
  }
  free(topic);
  //mqtt_log("[mqtt>>] FreeMem=%d\r\n",MicoGetMemoryInfo()->free_memory);
}

//---------------------------
static void closeMqtt(int id)
{
  if (pmqtt[id]==NULL) return;
  
  uint8_t idx;
  
  //mqtt_log("[mqtt>>] FreeMem=%d\r\n",MicoGetMemoryInfo()->free_memory);
  mqtt_log("[mqtt:%d] Closing...\r\n",id);
  pmqtt[id]->reqClose = false;
  
  if (pmqtt[id]->pServer != NULL) {
    if (pmqtt[id]->c.isconnected) {
      mqtt_log("         Disconnecting...\r\n");
      MQTTDisconnect(&(pmqtt[id]->c));
    }
    pmqtt[id]->n.disconnect(&(pmqtt[id]->n));
    
    if (MQTT_SUCCESS != MQTTClientDeinit(&(pmqtt[id]->c))) {
      mqtt_log("         Client Deinit failed!\r\n");
    }
  }

  // unregister callbacks  
  if(pmqtt[id]->cb_ref_connect != LUA_NOREF)
    luaL_unref(gL, LUA_REGISTRYINDEX, pmqtt[id]->cb_ref_connect); 
  pmqtt[id]->cb_ref_connect = LUA_NOREF;

  if(pmqtt[id]->cb_ref_offline != LUA_NOREF)
    luaL_unref(gL, LUA_REGISTRYINDEX, pmqtt[id]->cb_ref_offline); 
  pmqtt[id]->cb_ref_offline = LUA_NOREF;

  if(pmqtt[id]->cb_ref_message != LUA_NOREF)
    luaL_unref(gL, LUA_REGISTRYINDEX, pmqtt[id]->cb_ref_message); 
  pmqtt[id]->cb_ref_message = LUA_NOREF;

  // free strings
  if (pmqtt[id]->pServer != NULL) free(pmqtt[id]->pServer);
  for (idx=0;idx<MAX_MESSAGE_HANDLERS;idx++) {
    if (pmqtt[id]->pTopic[idx] != NULL) free(pmqtt[id]->pTopic[idx]);
  }
  if (pmqtt[id]->pPubTopic != NULL) free(pmqtt[id]->pPubTopic);
  if (pmqtt[id]->pData != NULL) free(pmqtt[id]->pData);
  if (pmqtt[id]->connectData.clientID.cstring != NULL) free(pmqtt[id]->connectData.clientID.cstring);
  if (pmqtt[id]->connectData.username.cstring != NULL) free(pmqtt[id]->connectData.username.cstring);
  if (pmqtt[id]->connectData.password.cstring != NULL) free(pmqtt[id]->connectData.password.cstring);
  
  free(pmqtt[id]);
  pmqtt[id] = NULL;
  mqtt_log("[mqtt:%d] Closed.\r\n",id);
  //mqtt_log("[mqtt>>] FreeMem=%d\r\n",MicoGetMemoryInfo()->free_memory);
}

//--------------------------------------
static void _thread_mqtt(void*inContext)
{
  (void)inContext;
 
  int rc = -1;
  fd_set readfds;
  struct timeval_t t = {0, MQTT_YIELD_TMIE*1000};
  int i=0;
  int maxSck=0;
  uint8_t idx;
  
  mqtt_log("[mqtt: ] MQTT Thread started.\r\n");
  
  while (1) {
    mico_thread_sleep(5);
      
    // --- Check if any active client left ---
    for (i=0; i<MAX_MQTT_NUM; i++) {
      if (pmqtt[i] !=NULL) break;
    }
    //if (i == MAX_MQTT_NUM) return;
    if (i == MAX_MQTT_NUM) goto terminate;

    // --- Check close request ---
    for (i=0;i<MAX_MQTT_NUM;i++) {
      if ((pmqtt[i] != NULL) && (pmqtt[i]->reqClose)) {
        closeMqtt(i);
      }
    }
    
    // --- Check start request ---
    for (i=0;i<MAX_MQTT_NUM;i++)
    {
      if ((pmqtt[i] != NULL) && (pmqtt[i]->reqStart == true))
      { // client requires start
        //mqtt_log("[mqtt>>] FreeMem=%d\r\n",MicoGetMemoryInfo()->free_memory);
        
        if (!pmqtt[i]->c.isconnected) {
          mqtt_log("[mqtt:%d] Creating connection [%s:%d]\r\n",i,pmqtt[i]->pServer,pmqtt[i]->port);
          rc = NewNetwork(&(pmqtt[i]->n), pmqtt[i]->pServer, pmqtt[i]->port, pmqtt[i]->ssl_settings);
          if (rc < 0) {
            mqtt_log("[mqtt:%d] Network connection ERROR=%d.\r\n",i, rc);
            pmqtt[i]->conn_retry++;
            if (pmqtt[i]->conn_retry > max_conn_retry) {
              mqtt_log("         Reconnect failed after %d retries\r\n", max_conn_retry);
              pmqtt[i]->reqStart = false;
              if (pmqtt[i]->cb_ref_offline != LUA_NOREF) {
                //-----------------------------------------------------------
                queue_msg_t msg;
                msg.L = gL;
                msg.source = onMQTT;
                msg.para1 = i | 0x00020000;
                msg.para2 = pmqtt[i]->cb_ref_offline;
                mico_rtos_push_to_queue( &os_queue, &msg, 0);
                //-----------------------------------------------------------
              }
            }
          }
          else {
            mqtt_log("[mqtt:%d] Network connection OK!\r\n",i);
            rc = MQTTClientInit(&(pmqtt[i]->c), &(pmqtt[i]->n), MQTT_CMD_TIMEOUT);
            if (MQTT_SUCCESS != rc) {
              mqtt_log("[mqtt:%d] Client init ERROR=%d.\r\n",i, rc);
              pmqtt[i]->conn_retry++;
              if (pmqtt[i]->conn_retry > max_conn_retry) {
                mqtt_log("         Reconnect failed after %d retries\r\n", max_conn_retry);
                pmqtt[i]->reqStart = false;
                if (pmqtt[i]->cb_ref_offline != LUA_NOREF) {
                  //-----------------------------------------------------------
                  queue_msg_t msg;
                  msg.L = gL;
                  msg.source = onMQTT;
                  msg.para1 = i | 0x00020000;
                  msg.para2 = pmqtt[i]->cb_ref_offline;
                  mico_rtos_push_to_queue( &os_queue, &msg, 0);
                  //-----------------------------------------------------------
                }
              }
            }
            else {
              mqtt_log("[mqtt:%d] Client init OK!\r\n",i);
              if ((pmqtt[i]->connectData.username.cstring != NULL) && (pmqtt[i]->connectData.password.cstring != NULL)) {
                mqtt_log("         Client connecting [user: %s, pass: %s]\r\n", pmqtt[i]->connectData.username.cstring,pmqtt[i]->connectData.password.cstring);
              }
              else {
                mqtt_log("         Client connecting []\r\n");
              }
              rc = MQTTConnect(&(pmqtt[i]->c), &(pmqtt[i]->connectData));
              if (MQTT_SUCCESS == rc) {
                pmqtt[i]->reqStart = false;
                pmqtt[i]->conn_retry = 0;
                
                // resubscribe if the client was subscribet to some topics
                for (idx=0;idx<MAX_MESSAGE_HANDLERS;idx++) {
                  if (pmqtt[i]->pTopic[idx] != NULL) {
                    rc = MQTTSubscribe(&(pmqtt[i]->c), pmqtt[i]->pTopic[idx], (enum QoS)(pmqtt[i]->qos), messageArrived);
                    if (MQTT_SUCCESS == rc) {
                      mqtt_log("         Client subscribed to topic [%s]\r\n", pmqtt[i]->pTopic[idx]);
                    }
                    else {
                      free(pmqtt[i]->pTopic[idx]);
                      pmqtt[i]->pTopic[idx] = NULL;
                      mqtt_log("         Client subscribe ERROR=%d\r\n", rc);
                    }
                  }
                }
                
                mqtt_log("         MQTT client connect OK!\r\n");
                if (pmqtt[i]->cb_ref_connect  != LUA_NOREF) {
                  //-----------------------------------------------------------
                  queue_msg_t msg;
                  msg.L = gL;
                  msg.source = onMQTT;
                  msg.para1 = i;
                  msg.para2 = pmqtt[i]->cb_ref_connect;
                  mico_rtos_push_to_queue( &os_queue, &msg,0);
                  //-----------------------------------------------------------
                }
              }
              else {
                mqtt_log("         Client connect ERROR=%d\r\n", rc);
                pmqtt[i]->conn_retry++;
                if (pmqtt[i]->conn_retry > max_conn_retry) {
                  mqtt_log("         Reconnect failed after %d retries\r\n", max_conn_retry);
                  pmqtt[i]->reqStart = false;
                  if (pmqtt[i]->cb_ref_offline != LUA_NOREF) {
                    //-----------------------------------------------------------
                    queue_msg_t msg;
                    msg.L = gL;
                    msg.source = onMQTT;
                    msg.para1 = i | 0x00020000;
                    msg.para2 = pmqtt[i]->cb_ref_offline;
                    mico_rtos_push_to_queue( &os_queue, &msg, 0);
                    //-----------------------------------------------------------
                  }
                }
              }
            }
          }
        }
        else {
          mqtt_log("[mqtt:%d] Connect request, but connected!\r\n",i);
        }
        //mqtt_log("[mqtt>>] FreeMem=%d\r\n",MicoGetMemoryInfo()->free_memory);
      }
    }
    
    // --- Check subscribe or unSubcribe request ---
    for (i=0;i<MAX_MQTT_NUM;i++)
    {
      if (pmqtt[i] == NULL) continue;
      // -- Subscribe --
      for (idx=0; idx<MAX_MESSAGE_HANDLERS; idx++) {
        if (pmqtt[i]->reqSubscribe[idx] && (pmqtt[i]->pTopic[idx] !=NULL)) {
          if (pmqtt[i]->c.isconnected) {
            //mqtt_log("[mqtt>>] FreeMem=%d\r\n",MicoGetMemoryInfo()->free_memory);
            rc = MQTTSubscribe(&(pmqtt[i]->c), pmqtt[i]->pTopic[idx], (enum QoS)(pmqtt[i]->qos), messageArrived);
            if (MQTT_SUCCESS == rc) {
              mqtt_log("[mqtt:%d] Client subscribed to topic [%s]\r\n", i, pmqtt[i]->pTopic[idx]);
            }
            else {
              pmqtt[i]->req_goto_disconect=true;
              free(pmqtt[i]->pTopic[idx]);
              pmqtt[i]->pTopic[idx] = NULL;
              mqtt_log("[mqtt:%d] Client subscribe ERROR=%d\r\n", i, rc);
            }
            //mqtt_log("[mqtt>>] FreeMem=%d\r\n",MicoGetMemoryInfo()->free_memory);
          }
          else {
            mqtt_log("[mqtt:%d] subscribe request, but not connected\r\n", i);
            pmqtt[i]->req_goto_disconect=true;
          }
        }
        pmqtt[i]->reqSubscribe[idx] = false;
      }
      
      // -- unSubscribe --
      for (idx=0; idx<MAX_MESSAGE_HANDLERS; idx++) {
        if (pmqtt[i]->requnSubscribe[idx] && (pmqtt[i]->pTopic[idx] !=NULL)) {
          if (pmqtt[i]->c.isconnected) {
            //mqtt_log("[mqtt>>] FreeMem=%d\r\n",MicoGetMemoryInfo()->free_memory);
            rc = MQTTUnsubscribe(&(pmqtt[i]->c), pmqtt[i]->pTopic[idx]);
            if(MQTT_SUCCESS == rc) {
              mqtt_log("[mqtt:%d] Client unsubscribed from topic [%s]\r\n", i, pmqtt[i]->pTopic[idx]);
            }
            else {
              pmqtt[i]->req_goto_disconect=true;
              mqtt_log("[mqtt:%d] Client unsubscribe ERROR=%d\r\n", i, rc);
            }
            free(pmqtt[i]->pTopic[idx]);
            pmqtt[i]->pTopic[idx] = NULL;
            //mqtt_log("[mqtt>>] FreeMem=%d\r\n",MicoGetMemoryInfo()->free_memory);
          }
          else {
            mqtt_log("[mqtt:%d] unsubscribe request, but not connected\r\n", i);
            pmqtt[i]->req_goto_disconect=true;
          }
        }
        pmqtt[i]->requnSubscribe[idx] = false;
      }
    }
    
    // --- Check publish request ---
    for (i=0;i<MAX_MQTT_NUM;i++)
    {
      if (pmqtt[i] == NULL) continue;
      if (pmqtt[i]->reqPublish)
      {
        if (pmqtt[i]->c.isconnected) {
          if ((pmqtt[i]->pPubTopic != NULL) && pmqtt[i]->pDataLen > 0) {
            //mqtt_log("[mqtt>>] FreeMem=%d\r\n",MicoGetMemoryInfo()->free_memory);
            MQTTMessage publishData =  MQTTMessage_publishData_initializer;
            publishData.qos = (enum QoS)(pmqtt[i]->qos);
            publishData.payload = (void*)pmqtt[i]->pData;
            publishData.payloadlen = pmqtt[i]->pDataLen;
            rc = MQTTPublish(&(pmqtt[i]->c), pmqtt[i]->pPubTopic, &publishData);
            if(MQTT_SUCCESS == rc) {
              mqtt_log("[mqtt:%d] Client published to topic [%s]\r\n", i, pmqtt[i]->pPubTopic);
            }
            else {
              mqtt_log("[mqtt:%d] Client publish ERROR=%d\r\n", i, rc);
              pmqtt[i]->req_goto_disconect=true;
            }
            free(pmqtt[i]->pData);
            pmqtt[i]->pData = NULL;
            free(pmqtt[i]->pPubTopic);
            pmqtt[i]->pPubTopic = NULL;
            //mqtt_log("[mqtt>>] FreeMem=%d\r\n",MicoGetMemoryInfo()->free_memory);
          }
          else {
            mqtt_log("[mqtt:%d] No publish data!\r\n", i);
          }
        }
        else {
          mqtt_log("[mqtt:%d] Client publish request, but not connected\r\n", i);
        }
        pmqtt[i]->reqPublish = false;
      }
    }
    
    // --- Check connected clients ---
    for (i=0;i<MAX_MQTT_NUM;i++) {
      if ((pmqtt[i] != NULL) && (pmqtt[i]->c.isconnected)) break;
    }
    //if (i == MAX_MQTT_NUM) return; // no connected clients
    if (i == MAX_MQTT_NUM) continue; // no connected clients
    
    // --- there is at least 1 active&connected client ---
    FD_ZERO(&readfds);
    for (i=0;i<MAX_MQTT_NUM;i++)
    {
      if (pmqtt[i] == NULL) continue;
      if (!pmqtt[i]->c.isconnected) continue;
      
      FD_SET(pmqtt[i]->c.ipstack->my_socket, &readfds);
      if (pmqtt[i]->c.ipstack->my_socket > maxSck) 
           maxSck = pmqtt[i]->c.ipstack->my_socket;
    }
    
    select(maxSck+1, &readfds, NULL, NULL, &t);
    
    for (i=0;i<MAX_MQTT_NUM;i++)
    {
      if(pmqtt[i] == NULL) continue;
      if (!pmqtt[i]->c.isconnected) continue;
      
      if (FD_ISSET(pmqtt[i]->c.ipstack->my_socket, &readfds )) {
        rc = MQTTYield(&(pmqtt[i]->c), (int)MQTT_YIELD_TMIE);
        if (MQTT_SUCCESS != rc) {
          mqtt_log("[mqtt:%d] Yield ERROR\r\n", i);
          pmqtt[i]->req_goto_disconect = true;
        }
      }
      else{ // if MQTTYield was not called periodical, we need to check ping msg to keep alive.
        if ((mico_get_time() - pmqtt[i]->keepaliveTick) > (pmqtt[i]->connectData.keepAliveInterval*1000))
          {
            pmqtt[i]->keepaliveTick = mico_get_time();
            rc = keepalive(&(pmqtt[i]->c));
            if (MQTT_SUCCESS != rc) {
              mqtt_log("[mqtt:%d] KeepaliveTick ERROR\r\n", i);
              pmqtt[i]->req_goto_disconect=true;
            }
          }
        }
    }
    
    //check disconnect;
    for (i=0; i<MAX_MQTT_NUM; i++) {
      if ((pmqtt[i] != NULL) && (pmqtt[i]->req_goto_disconect == true)) {
        pmqtt[i]->req_goto_disconect = false;

        mqtt_log("[mqtt:%d] Client will reconnect...\r\n", i);

        if (pmqtt[i]->pServer != NULL) {
          if (pmqtt[i]->c.isconnected) {
            mqtt_log("         Disconnecting...\r\n");
            MQTTDisconnect(&(pmqtt[i]->c));
          }
          pmqtt[i]->n.disconnect((&(pmqtt[i]->n)));
          
          if (MQTT_SUCCESS != MQTTClientDeinit(&(pmqtt[i]->c))) {
            mqtt_log("         Client Deinit failed!\r\n");
          }
        }
        
        if (pmqtt[i]->cb_ref_offline  != LUA_NOREF) {
          //-----------------------------------------------------------
          queue_msg_t msg;
          msg.L = gL;
          msg.source = onMQTT;
          msg.para1 = i | 0x00010000;
          msg.para2 = pmqtt[i]->cb_ref_offline;
          mico_rtos_push_to_queue( &os_queue, &msg,0);
          //-----------------------------------------------------------
        }
        pmqtt[i]->reqStart = true;
      }
    }
  } // while
terminate:
  mqtt_log("[mqtt: ] MQTT Thread terminated.\r\n");
  mqtt_thread_is_started = false;
  mico_rtos_delete_thread( NULL );
}


//==================================
static int lmqtt_ver( lua_State* L )
{
  uint32_t mqtt_lib_version = 0;
  char str[32] = {0};
  
  mqtt_lib_version = MQTTClientLibVersion();
  sprintf(str,"%d.%d.%d",0xFF & (mqtt_lib_version >> 16), 
                         0xFF & (mqtt_lib_version >> 8), 
                         0xFF &  mqtt_lib_version);
  lua_pushstring(L,str);
  return 1;  
}

//mqttClt = mqtt.new(clientid,user,pass[,keepalive])
//==================================
static int lmqtt_new( lua_State* L )
{
  size_t sl_cid = 0;
  size_t sl_user = 0;
  size_t sl_pass = 0;
  
  char const *clientid = luaL_checklstring( L, 1, &sl_cid );
  if ((clientid == NULL) || (strlen(clientid)==0)) {
    l_message(NULL, "clientid: wrong arg type");
    lua_pushinteger(L, -1);
    return 1;
  }
  
  char const *user = luaL_checklstring( L, 2, &sl_user );
  char const *pass = luaL_checklstring( L, 3, &sl_pass );
  
  if (strlen(user)==0) user=NULL;
  if (strlen(pass)==0) pass=NULL;
  
  unsigned keepalive = 60;
  if (lua_gettop(L) >= 4) {
    keepalive = luaL_checkinteger( L, 4 );
    if (keepalive < 30) keepalive = 30;
    if (keepalive > 300) keepalive = 300;
  }

  uint8_t idx;
  int k = 0;
  for (k=0; k<MAX_MQTT_NUM; k++){
    if (pmqtt[k] == NULL) break;
  }
  if (k == MAX_MQTT_NUM) {
    l_message(NULL, "Max MQTT Number is reached");
    lua_pushinteger(L, -2);
    return 1;
  }
  
  pmqtt[k] = (mqtt_t*)malloc(sizeof(mqtt_t));
  if (pmqtt[k] == NULL) {
    l_message(NULL, "memery allocation failed");
    lua_pushinteger(L, -3);
    return 1;
  }

  if (!mqtt_thread_is_started) {
    if (mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY-1, "Mqtt_Thread", _thread_mqtt, 1024, NULL) != kNoErr) {
      l_message(NULL, "Create thread failed" );
      lua_pushinteger(L, -4);
      return 1;
    }
    else mqtt_thread_is_started = true;
  } 
  
  //mqtt_log("[mqtt>>] FreeMem=%d\r\n",MicoGetMemoryInfo()->free_memory);
  MQTTPacket_connectData cd = MQTTPacket_connectData_initializer;
  memcpy(&(pmqtt[k]->connectData), &cd, sizeof(MQTTPacket_connectData));
  
  pmqtt[k]->connectData.willFlag = 0;
  pmqtt[k]->connectData.MQTTVersion = 3;
  
  if (pmqtt[k]->connectData.clientID.cstring != NULL) free(pmqtt[k]->connectData.clientID.cstring);
  pmqtt[k]->connectData.clientID.cstring = (char*)malloc(sl_cid+1);
  memcpy(pmqtt[k]->connectData.clientID.cstring, clientid, sl_cid);
  *(pmqtt[k]->connectData.clientID.cstring+sl_cid) = '\0';
  pmqtt[k]->connectData.clientID.lenstring.data = pmqtt[k]->connectData.clientID.cstring;
  pmqtt[k]->connectData.clientID.lenstring.len = sl_cid;
  
  if (user != NULL) {
    if (pmqtt[k]->connectData.username.cstring != NULL) free(pmqtt[k]->connectData.username.cstring);
    pmqtt[k]->connectData.username.cstring = (char*)malloc(sl_user+1);
    memcpy(pmqtt[k]->connectData.username.cstring, user, sl_user);
    *(pmqtt[k]->connectData.username.cstring+sl_user) = '\0';
    pmqtt[k]->connectData.username.lenstring.data = pmqtt[k]->connectData.username.cstring;
    pmqtt[k]->connectData.username.lenstring.len = sl_user;
  }
  
  if (pass != NULL) {
    if (pmqtt[k]->connectData.password.cstring != NULL) free(pmqtt[k]->connectData.password.cstring);
    pmqtt[k]->connectData.password.cstring = (char*)malloc(sl_pass+1);
    memcpy(pmqtt[k]->connectData.password.cstring, pass, sl_pass);
    *(pmqtt[k]->connectData.password.cstring+sl_pass) = '\0';
    pmqtt[k]->connectData.password.lenstring.data = pmqtt[k]->connectData.password.cstring;
    pmqtt[k]->connectData.password.lenstring.len = sl_pass;
  }

  pmqtt[k]->connectData.keepAliveInterval = keepalive;
  pmqtt[k]->connectData.cleansession = 1;
  pmqtt[k]->ssl_settings.ssl_enable = false;
  
  pmqtt[k]->reqStart = false;
  pmqtt[k]->reqClose = false;
  pmqtt[k]->cb_ref_connect = LUA_NOREF;
  pmqtt[k]->cb_ref_offline = LUA_NOREF;
  pmqtt[k]->cb_ref_message = LUA_NOREF;
  pmqtt[k]->qos = QOS0;
  pmqtt[k]->pServer = NULL;
  for (idx=0;idx<MAX_MESSAGE_HANDLERS;idx++) {
    pmqtt[k]->pTopic[idx] = NULL;
    pmqtt[k]->reqSubscribe[idx] = false;
    pmqtt[k]->requnSubscribe[idx] = false;
  }
  pmqtt[k]->pPubTopic = NULL;
  pmqtt[k]->pData = NULL;
  pmqtt[k]->reqPublish = false;
  pmqtt[k]->req_goto_disconect = false;
  pmqtt[k]->keepaliveTick = 0;
  pmqtt[k]->conn_retry = 0;
    
  memset(&(pmqtt[k]->c), 0, sizeof(pmqtt[k]->c));
  memset(&(pmqtt[k]->n), 0, sizeof(pmqtt[k]->n));
  mqtt_log("[mqtt:%d] Init: OK.\r\n", k);
  //mqtt_log("[mqtt>>] FreeMem=%d\r\n",MicoGetMemoryInfo()->free_memory);
  gL = L;

  lua_pushinteger(L, k);
  return 1;
}

//mqtt.start(mqttClt, server, port)
//====================================
static int lmqtt_start( lua_State* L )
{
  int mqttClt;
  mqttClt = luaL_checkinteger( L, 1);
  
  if ((mqttClt < 0) || (mqttClt >= MAX_MQTT_NUM))
    return luaL_error( L, "mqttClt arg is wrong!" );
  if (pmqtt[mqttClt] == NULL) {
    l_message(NULL, "Client not initialized");
    lua_pushinteger(L, -1);
    return 1;
  }
  
  if (pmqtt[mqttClt]->c.isconnected) {
    l_message(NULL, "Client already connected");
    lua_pushinteger(L, -2);
    return 1;
  }
  
  size_t sl=0;
  char const *server = luaL_checklstring( L, 2, &sl );
  if (server == NULL) return luaL_error( L, "server: wrong arg type" );

  unsigned port = luaL_checkinteger( L, 3);

  uint8_t idx;
  for (idx=0; idx<MAX_MQTT_NUM; idx++) {
    if (pmqtt[idx] != NULL) {
      if ((pmqtt[idx]->pServer != NULL) && (strcmp(pmqtt[idx]->pServer, server) == 0)) {
        if (idx != mqttClt) {
          l_message(NULL, "Another Client already connected to the same server");
          lua_pushinteger(L, -3);
          return 1;
        }
      }
    }
  }

  if (pmqtt[mqttClt]->pServer != NULL) free(pmqtt[mqttClt]->pServer);
  pmqtt[mqttClt]->pServer = (char*)malloc(sl+1);
  memcpy(pmqtt[mqttClt]->pServer, server, sl);
  *(pmqtt[mqttClt]->pServer+sl) = '\0';
  
  pmqtt[mqttClt]->port = port;
  
  pmqtt[mqttClt]->reqStart = true;

  lua_pushinteger(L, 0);
  return 1;
}

//mqtt.isactive(mqttClt)
//=======================================
static int lmqtt_isactive( lua_State* L )
{
  int mqttClt;

  mqttClt = luaL_checkinteger( L, 1);
  
  if ((mqttClt < 0) || (mqttClt >= MAX_MQTT_NUM))
    return luaL_error( L, "mqttClt arg is wrong!" );

  if (pmqtt[mqttClt] != NULL) {
    lua_pushinteger(L, 1);
  }
  else {
    lua_pushinteger(L, 0);
  }
  return 1;
}

//mqtt.isconnected(mqttClt)
//==========================================
static int lmqtt_isconnected( lua_State* L )
{
  int mqttClt;

  mqttClt = luaL_checkinteger( L, 1);
  
  if ((mqttClt < 0) || (mqttClt >= MAX_MQTT_NUM))
    return luaL_error( L, "mqttClt arg is wrong!" );

  if (pmqtt[mqttClt] == NULL) {
    lua_pushinteger(L, 0);
    return 1;
  }
  if (pmqtt[mqttClt]->c.isconnected) {
    lua_pushinteger(L, 1);
  }
  else {
    lua_pushinteger(L, 0);
  }
  return 1;
}

//mqtt.status(mqttClt)
//=====================================
static int lmqtt_status( lua_State* L )
{
  int mqttClt;
  uint8_t n = 0;

  mqttClt = luaL_checkinteger( L, 1);
  
  if ((mqttClt < 0) || (mqttClt >= MAX_MQTT_NUM))
    return luaL_error( L, "mqttClt arg is wrong!" );

  if (pmqtt[mqttClt] != NULL) {
    mqtt_log("  Client #%d: Initialized\r\n", mqttClt);
    lua_pushinteger(L, 1);
  }
  else {
    mqtt_log("  Client #%d: Not initialized\r\n", mqttClt);
    lua_pushinteger(L, 0);
    lua_pushinteger(L, 0);
    lua_pushinteger(L, 0);
    return 3;
  }
  
  mqtt_log("   clientID: %s\r\n",pmqtt[mqttClt]->connectData.clientID.cstring);
  if (pmqtt[mqttClt]->connectData.username.cstring != NULL)
    mqtt_log("   username: %s\r\n",pmqtt[mqttClt]->connectData.username.cstring);
  if (pmqtt[mqttClt]->connectData.password.cstring != NULL)
    mqtt_log("   password: %s\r\n\r\n",pmqtt[mqttClt]->connectData.password.cstring);
  if (pmqtt[mqttClt]->cb_ref_message != LUA_NOREF) {
    mqtt_log(" Message cb: assigned\r\n");
  }
  else {
    mqtt_log(" Message cb: not assigned\r\n");
  }
  if (pmqtt[mqttClt]->cb_ref_connect != LUA_NOREF) {
    mqtt_log(" Connect cb: assigned\r\n");
  }
  else {
    mqtt_log(" Connect cb: not assigned\r\n");
  }
  if (pmqtt[mqttClt]->cb_ref_offline != LUA_NOREF) {
    mqtt_log(" Offline cb: assigned\r\n");
  }
  else {
    mqtt_log(" Offline cb: not assigned\r\n\r\n");
  }
  
  mqtt_log("  Connected: ");
  if (pmqtt[mqttClt]->c.isconnected) {
    mqtt_log("yes\r\n");
    lua_pushinteger(L, 1);
  }
  else {
    mqtt_log("no\r\n");
    lua_pushinteger(L, 0);
    lua_pushinteger(L, 0);
    return 3;
  }
  mqtt_log("     Server: %s\r\n",pmqtt[mqttClt]->pServer);
  mqtt_log("       port: %d\r\n\r\n",pmqtt[mqttClt]->port);
  
  int i;
  mqtt_log(" Subscribed: \r\n");
  for (i=0;i<MAX_MESSAGE_HANDLERS;i++) {
    if ( pmqtt[mqttClt]->c.messageHandlers[i].topicFilter != NULL ) {
      mqtt_log("   Topic #%d: %s\r\n",i,pmqtt[mqttClt]->c.messageHandlers[i].topicFilter);
      n++;
    }
  }
  lua_pushinteger(L, n);

  return 3;
}

//mqtt.close(mqttClt)
//====================================
static int lmqtt_close( lua_State* L )
{
  int mqttClt = luaL_checkinteger( L, 1);
  
  if ((mqttClt < 0) || (mqttClt >= MAX_MQTT_NUM))
    return luaL_error( L, "mqttClt arg is wrong!" );
  if (pmqtt[mqttClt] == NULL) {
    l_message(NULL, "Client not initialized");
    lua_pushinteger(L, -1);
    return 1;
  }
  
  pmqtt[mqttClt]->reqClose = true;

  lua_pushinteger(L, 0);
  return 1;
}

//mqtt.closeall()
//=======================================
static int lmqtt_closeall( lua_State* L )
{
  uint8_t i;

  for (i=0;i<MAX_MQTT_NUM;i++) {
    if (pmqtt[i] != NULL) {
      pmqtt[i]->reqClose = true;
    }
  }
  return 0;
}

//mqtt.subscribe(mqttClt,topic,QoS[,cb_messagearrived(topic,message)])
//========================================
static int lmqtt_subscribe( lua_State* L )
{
  int mqttClt = luaL_checkinteger( L, 1);

  if ((mqttClt < 0) || (mqttClt >= MAX_MQTT_NUM))
    return luaL_error( L, "mqttClt arg is wrong!" );
  if (pmqtt[mqttClt] == NULL) {
    l_message(NULL, "Client not initialized");
    lua_pushinteger(L, -1);
    return 1;
  }

  if (pmqtt[mqttClt]->c.isconnected == 0) {
    l_message(NULL, "Client not connected");
    lua_pushinteger(L, -2);
    return 1;
  }

  size_t sl = 0;
  char const *topic = luaL_checklstring( L, 2, &sl );
  if (topic == NULL) {
    l_message(NULL, "topic: wrong arg type");
    lua_pushinteger(L, -3);
    return 1;
  }
  
  unsigned qos=luaL_checkinteger( L, 3);
  if (!(qos == QOS0 || qos == QOS1 ||qos == QOS2)) {
    l_message(NULL, "QoS: wrong arg type");
    lua_pushinteger(L, -4);
    return 1;
  }

  uint8_t idx;
  for (idx=0;idx<MAX_MESSAGE_HANDLERS;idx++) {
    if ((pmqtt[mqttClt]->pTopic[idx] != NULL) && (strcmp(pmqtt[mqttClt]->pTopic[idx], topic) == 0)) {
      l_message(NULL, "Topic already subscribed");
      lua_pushinteger(L, -5);
      return 1;
    }
  }

  for (idx=0;idx<MAX_MESSAGE_HANDLERS;idx++) {
    if (pmqtt[mqttClt]->pTopic[idx] == NULL) break;
  }
  if (idx == MAX_MESSAGE_HANDLERS) {
    l_message(NULL, "Max number of topics subscribed");
    lua_pushinteger(L, -6);
    return 1;
  }
  
  if (lua_type(L, 4) == LUA_TFUNCTION || lua_type(L, 4) == LUA_TLIGHTFUNCTION) {
    // registry callback function
    lua_pushvalue(L, 4);
    if (pmqtt[mqttClt]->cb_ref_message != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, pmqtt[mqttClt]->cb_ref_message);
    }
    pmqtt[mqttClt]->cb_ref_message = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  if (pmqtt[mqttClt]->pTopic[idx] != NULL) free(pmqtt[mqttClt]->pTopic[idx]);
  pmqtt[mqttClt]->pTopic[idx] = (char*)malloc(sl+1);
  memcpy(pmqtt[mqttClt]->pTopic[idx], topic, sl);
  *(pmqtt[mqttClt]->pTopic[idx]+sl) = '\0';

  pmqtt[mqttClt]->qos = qos;
  pmqtt[mqttClt]->reqSubscribe[idx] = true;

  lua_pushinteger(L, 0);
  return 1;
}

//mqtt.issubscribed(mqttClt,topic))
//===========================================
static int lmqtt_issubscribed( lua_State* L )
{
  int mqttClt = luaL_checkinteger( L, 1);

  if ((mqttClt < 0) || (mqttClt >= MAX_MQTT_NUM))
    return luaL_error( L, "mqttClt arg is wrong!" );
  if (pmqtt[mqttClt] == NULL) {
    lua_pushinteger(L, 0);
    return 1;
  }

  if (pmqtt[mqttClt]->c.isconnected == 0) {
    lua_pushinteger(L, 0);
    return 1;
  }

  size_t sl = 0;
  char const *topic = luaL_checklstring( L, 2, &sl );
  if (topic == NULL) {
    lua_pushinteger(L, 0);
    return 1;
  }
  
  uint8_t idx;
  for (idx=0;idx<MAX_MESSAGE_HANDLERS;idx++) {
    if ((pmqtt[mqttClt]->pTopic[idx] != NULL) && (strcmp(pmqtt[mqttClt]->pTopic[idx], topic) == 0)) {
      lua_pushinteger(L, 1);
      return 1;
    }
  }

  lua_pushinteger(L, 0);
  return 1;
}

//mqtt.unsubscribe(mqttClt,topic)
//==========================================
static int lmqtt_unsubscribe( lua_State* L )
{
  int mqttClt = luaL_checkinteger( L, 1);

  if ((mqttClt < 0) || (mqttClt >= MAX_MQTT_NUM))
    return luaL_error( L, "mqttClt arg is wrong!" );
  if (pmqtt[mqttClt] == NULL) {
    l_message(NULL, "Client not initialized");
    lua_pushinteger(L, -1);
    return 1;
  }
  
  if (pmqtt[mqttClt]->c.isconnected == 0) {
    l_message(NULL, "Client not connected");
    lua_pushinteger(L, -2);
    return 1;
  }

  size_t sl=0;
  char const *topic = luaL_checklstring( L, 2, &sl );
  if (topic == NULL) {
    l_message(NULL, "topic: wrong arg type");
    lua_pushinteger(L, -3);
    return 1;
  }
  
  uint8_t idx;
  for (idx=0;idx<MAX_MESSAGE_HANDLERS;idx++) {
    if ((pmqtt[mqttClt]->pTopic[idx] != NULL) && (strcmp(pmqtt[mqttClt]->pTopic[idx], topic) == 0)) break;
  }
  if (idx == MAX_MESSAGE_HANDLERS) {
    l_message(NULL, "Topics not subscribed");
    lua_pushinteger(L, -4);
    return 1;
  }
  
  pmqtt[mqttClt]->requnSubscribe[idx] = true;

  lua_pushinteger(L, 0);
  return 1;
}

//mqtt.publish(mqttClt,topic,QoS, data)
//======================================
static int lmqtt_publish( lua_State* L )
{
  int mqttClt = luaL_checkinteger( L, 1);

  if ((mqttClt < 0) || (mqttClt >= MAX_MQTT_NUM))
    return luaL_error( L, "mqttClt arg is wrong!" );
  if (pmqtt[mqttClt] == NULL) {
    l_message(NULL, "Client not initialized");
    lua_pushinteger(L, -1);
    return 1;
  }
  
  if (pmqtt[mqttClt]->c.isconnected == 0) {
    l_message(NULL, "Client not connected");
    lua_pushinteger(L, -2);
    return 1;
  }

  size_t slt = 0;
  char const *topic = luaL_checklstring( L, 2, &slt );
  if (topic == NULL) {
    l_message(NULL, "topic: wrong arg type");
    lua_pushinteger(L, -3);
    return 1;
  }
  
  unsigned qos=luaL_checkinteger( L, 3);
  if (!(qos == QOS0 || qos == QOS1 ||qos == QOS2)) {
    l_message(NULL, "QoS: wrong arg type");
    lua_pushinteger(L, -4);
    return 1;
  }
    
  size_t sld = 0;
  char const *data = luaL_checklstring( L, 4, &sld );
  if (data == NULL) {
    l_message(NULL, "data: wrong arg type");
    lua_pushinteger(L, -5);
    return 1;
  }
  
  pmqtt[mqttClt]->qos = qos;

  if (pmqtt[mqttClt]->pPubTopic != NULL) free(pmqtt[mqttClt]->pPubTopic);
  pmqtt[mqttClt]->pPubTopic = (char*)malloc(slt+1);
  memcpy(pmqtt[mqttClt]->pPubTopic, topic, slt);
  *(pmqtt[mqttClt]->pPubTopic+slt) = '\0';

  if (pmqtt[mqttClt]->pData != NULL) free(pmqtt[mqttClt]->pData);
  pmqtt[mqttClt]->pData = (char*)malloc(sld+1);
  memcpy(pmqtt[mqttClt]->pData, data, sld);
  *(pmqtt[mqttClt]->pData+sld) = '\0';

  pmqtt[mqttClt]->pDataLen = sld;
  pmqtt[mqttClt]->reqPublish = true;

  lua_pushinteger(L, 0);
  return 1;
}

//mqtt.on(mqttClt,'connect',function(clt))
//mqtt.on(mqttClt,'offline',function(clt))
//mqtt.on(mqttClt,'message',cb_messagearrived(topic,message))
//  different topics with same callback
//=================================
static int lmqtt_on( lua_State* L )
{
  int mqttClt = luaL_checkinteger( L, 1);

  if ((pmqtt[mqttClt] == NULL) || (mqttClt < 0) || (mqttClt >= MAX_MQTT_NUM))
    return luaL_error( L, "mqttClt arg is wrong!" );
  
  size_t sl=0;
  char const *method = luaL_checklstring( L, 2, &sl );
  if (method == NULL) return luaL_error( L, "wrong arg type" );
  
  if (lua_type(L, 3) == LUA_TFUNCTION || lua_type(L, 3) == LUA_TLIGHTFUNCTION)
    lua_pushvalue(L, 3);
  else
    return luaL_error( L, "callback function needed" );
  
  if ((strcmp(method,"connect") == 0) && (sl == strlen("connect")))
   {
     if (pmqtt[mqttClt]->cb_ref_connect != LUA_NOREF) {
       luaL_unref(L, LUA_REGISTRYINDEX, pmqtt[mqttClt]->cb_ref_connect);
     }
     pmqtt[mqttClt]->cb_ref_connect = luaL_ref(L, LUA_REGISTRYINDEX);
   }
  else if ((strcmp(method,"message") == 0) && (sl == strlen("message")))
  {
    if (pmqtt[mqttClt]->cb_ref_message != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, pmqtt[mqttClt]->cb_ref_message);
    }
    pmqtt[mqttClt]->cb_ref_message = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else if ((strcmp(method,"offline") == 0) && (sl==strlen("offline")))
  {
    if (pmqtt[mqttClt]->cb_ref_offline != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, pmqtt[mqttClt]->cb_ref_offline);
    }
    pmqtt[mqttClt]->cb_ref_offline = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else {
    //mico_start_timer(&_timer_mqtt);
    return luaL_error( L, "wrong method" );
  }
  
  return 0;
}

//mqtt.debug(flag)
//==========================================
static int lmqtt_debug( lua_State* L )
{
  unsigned flag = luaL_checkinteger( L, 1);
  
  if (flag == 0) mqtt_debug = false;
  else mqtt_debug = true;
  
  return 0;
}

//mqtt.setretry(n)
//=======================================
static int lmqtt_setretry( lua_State* L )
{
  int rtr = luaL_checkinteger( L, 1);
  
  if (rtr < 1) rtr = 1;
  if (rtr > 100) rtr = 100;
  max_conn_retry = rtr;
  
  return 0;
}

/*
mqttClt = mqtt.new(clientid,keepalive,user,pass)
mqtt.start(mqttClt, server, port)
mqtt.on(mqttClt,'connect',function())
mqtt.on(mqttClt,'offline',function())
//different topics with same callback
mqtt.on(mqttClt,'message',cb_messagearrived(topic,message))
mqtt.close(mqttClt)
mqtt.publish(mqttClt,topic,QoS, data)
mqtt.subscribe(mqttClt,topic,QoS,cb_messagearrived(topic,message))
mqtt.unsubscribe(mqttClt,topic)
*/

#define MIN_OPT_LEVEL  2
#include "lrodefs.h"
const LUA_REG_TYPE mqtt_map[] =
{
  { LSTRKEY( "ver" ), LFUNCVAL( lmqtt_ver )},
  { LSTRKEY( "new" ), LFUNCVAL( lmqtt_new )},
  { LSTRKEY( "start" ), LFUNCVAL( lmqtt_start )},
  { LSTRKEY( "status" ), LFUNCVAL( lmqtt_status )},
  { LSTRKEY( "isactive" ), LFUNCVAL( lmqtt_isactive )},
  { LSTRKEY( "isconnected" ), LFUNCVAL( lmqtt_isconnected )},
  { LSTRKEY( "close" ), LFUNCVAL( lmqtt_close )},
  { LSTRKEY( "closeall" ), LFUNCVAL( lmqtt_closeall )},
  { LSTRKEY( "subscribe" ), LFUNCVAL( lmqtt_subscribe )},
  { LSTRKEY( "issubscribed" ), LFUNCVAL( lmqtt_issubscribed )},
  { LSTRKEY( "unsubscribe" ), LFUNCVAL( lmqtt_unsubscribe )},
  { LSTRKEY( "publish" ), LFUNCVAL( lmqtt_publish )},
  { LSTRKEY( "on" ), LFUNCVAL( lmqtt_on )},
  { LSTRKEY( "debug" ), LFUNCVAL( lmqtt_debug )},
  { LSTRKEY( "setretry" ), LFUNCVAL( lmqtt_setretry )},
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY( "QOS0" ), LNUMVAL( QOS0 ) },
  { LSTRKEY( "QOS1" ), LNUMVAL( QOS1 ) },
  { LSTRKEY( "QOS2" ), LNUMVAL( QOS2 ) },
  { LSTRKEY( "MAX_CLIENT" ), LNUMVAL( MAX_MQTT_NUM ) },
  { LSTRKEY( "MAX_TOPIC" ), LNUMVAL( MAX_MESSAGE_HANDLERS ) },
#endif        
  {LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_mqtt(lua_State *L)
{
    for(int i=0;i<MAX_MQTT_NUM;i++)
    {
      pmqtt[i]=NULL;
    }
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else  
    luaL_register( L, EXLIB_MQTT, mqtt_map );
    MOD_REG_NUMBER( L, "QOS0", QOS0);
    MOD_REG_NUMBER( L, "QOS1", QOS1);
    MOD_REG_NUMBER( L, "QOS2", QOS2);
    MOD_REG_NUMBER( L, "MAX_CLIENT", MAX_MQTT_NUM);
    MOD_REG_NUMBER( L, "MAX_TOPIC", MAX_MESSAGE_HANDLERS);
    return 1;
#endif
}


