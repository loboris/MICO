/**
 * net.c
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"

#include "platform.h"
//#include "MICODefine.h"
#include "system.h"
#include "mico_socket.h"
#include "mico_wlan.h"
#include "mico_system.h"
#include "SocketUtils.h"
#include "mico_rtos.h"
//#include "HTTPUtils.h"

#define TCP IPPROTO_TCP
#define UDP IPPROTO_UDP
#define SOCKET_SERVER 0
#define SOCKET_CLIENT 1
#define INVALID_HANDLE -1

#define MAX_SVR_SOCKET 4
#define MAX_SVRCLT_SOCKET 5
#define MAX_CLT_SOCKET 4

extern mico_queue_t os_queue;
extern void luaWdgReload( void );

static mico_mutex_t  net_mut = NULL; // Used to synchronize the net thread

static bool log = false;
#define net_log(M, ...) if (log == true) printf(M, ##__VA_ARGS__)

enum _req_actions{
  NO_ACTION = 0,
  REQ_ACTION_GOTIP,
  REQ_ACTION_GETIP,
  REQ_ACTION_SEND,
  REQ_ACTION_RECEIVED,
  REQ_ACTION_CONNECTED,
  REQ_ACTION_DISCONNECT,
  REQ_ACTION_DISCONNECTFREE,
  REQ_ACTION_LISTEN,
  REQ_ACTION_BIND
};

enum {
  SOCKET_TYPE_SERVER = 1,
  SOCKET_TYPE_SVRCLT,
  SOCKET_TYPE_CLIENT,
};

enum _socket_state{
  SOCKET_STATE_NOTCONNECTED = 0,
  SOCKET_STATE_CONNECTED,
  SOCKET_STATE_READY,
  SOCKET_STATE_LISTEN,
  SOCKET_STATE_CLOSED,
};

// for server-client
typedef struct {
  int client;              //socket type
  uint8_t clientFlag;      //sent or disconnect
  uint8_t state;           //socket connection state
  struct sockaddr_t addr;  //ip and port 
}_lsvrCltsocket_t;

//for server
typedef struct {
  int socket;              //socket type
  int port;                //port
  uint8_t type;            //TCP or UDP
  int accept_cb;
  int receive_cb;
  int sent_cb;
  int disconnect_cb;
  uint8_t clientFlag;      //disconnect
  uint8_t state;           //socket connection state
  _lsvrCltsocket_t *psvrCltsocket[MAX_SVRCLT_SOCKET];
}svrsockt_t;
svrsockt_t *psvrsockt[MAX_SVR_SOCKET];

//for client
typedef struct _lcltsocket{
  int socket;              // TCP or UDP socket
  uint8_t type;            // type: TCP or UDP
  uint8_t http;            // use http protocol
  uint16_t wait_tmo;
  //uint8_t ssl;             // use ssl (not working!)
  //mico_ssl_t client_ssl;
  struct sockaddr_t addr;  //ip and port for tcp or udp use
  int local_port;
  int connect_cb;
  int dnsfound_cb;
  int receive_cb;
  int sent_cb;
  int disconnect_cb;
  uint8_t clientFlag;      //sent or disconnect or got ip
  uint8_t state;           //socket connection state
  char *pDomain4Dns;
} cltsockt_t;
cltsockt_t *pcltsockt[MAX_CLT_SOCKET];


#define MAX_RECV_LEN 1024
static lua_State *gL = NULL;
static char* recvBuf = NULL;
static char* sendBuf = NULL;
static int send_len = 0;
static bool net_thread_is_started = false;
static int socket_connected = -1;
static int max_recvlen = 10*1024;
static int data_sent = 0;
static int data_received = 0;
static bool wifiConnected = false;

//==============================================================================


// socketHandle: socket
// type: return value,SOCKET_TYPE_SERVER or SOCKET_TYPE_SVRCLT or SOCKET_TYPE_CLIENT
// out1: return value
// out2: return value
//-------------------------------------------------------------------------
static bool getsocketIndex(int socketHandle, int *type,int *out1,int *out2)
{
  *out1 = -1;
  *out2 = -1;
  *type = 0;
  
  int k, m;
  for (k=0; k<MAX_SVR_SOCKET; k++) {
    if (psvrsockt[k] == NULL) continue;
    if (psvrsockt[k]->socket == socketHandle) {
      *type = SOCKET_TYPE_SERVER;
      *out1 = k;
      return true;
    }
    for (m=0; m<MAX_SVRCLT_SOCKET; m++) {
      if (psvrsockt[k]->psvrCltsocket[m] == NULL) continue;
      if (psvrsockt[k]->psvrCltsocket[m]->client == socketHandle) {
        *type = SOCKET_TYPE_SVRCLT;
        *out1 = k;
        *out2 = m;
        return true;
      }
    }
  }
  
  for (k=0; k<MAX_CLT_SOCKET; k++) {
    if (pcltsockt[k] == NULL) continue;
    if (pcltsockt[k]->socket == socketHandle) {
      *type = SOCKET_TYPE_CLIENT;
      *out1 = k;
      return true;
    }
  }
  return false;
}

//-------------------------------------------------------
static void closeSocket(int socketHandle, uint8_t dofree)
{
  // ** server or serverclient
  int k=0, m=0;
  for (k=0; k<MAX_SVR_SOCKET; k++) {
    if (psvrsockt[k] == NULL) continue;
    
    if ((psvrsockt[k]->socket == socketHandle) || (dofree == 97)) {
      //close all serverClient
      //close server socket
      //unref cb function
      //free memery
      for (m=0; m<MAX_SVRCLT_SOCKET; m++) {
        if (psvrsockt[k]->psvrCltsocket[m] == NULL) continue;
        
        if ((psvrsockt[k]->type == TCP) && (psvrsockt[k]->psvrCltsocket[m]->client != INVALID_HANDLE))
          close(psvrsockt[k]->psvrCltsocket[m]->client);
        free(psvrsockt[k]->psvrCltsocket[m]);
        psvrsockt[k]->psvrCltsocket[m] = NULL;
      }
      if (gL != NULL) {
        if (psvrsockt[k]->accept_cb != LUA_NOREF)
          luaL_unref(gL, LUA_REGISTRYINDEX, psvrsockt[k]->accept_cb);
        psvrsockt[k]->accept_cb = LUA_NOREF;
        if (psvrsockt[k]->receive_cb != LUA_NOREF)
          luaL_unref(gL, LUA_REGISTRYINDEX, psvrsockt[k]->receive_cb);
        psvrsockt[k]->receive_cb = LUA_NOREF;
        if (psvrsockt[k]->sent_cb != LUA_NOREF)
          luaL_unref(gL, LUA_REGISTRYINDEX, psvrsockt[k]->sent_cb);
        psvrsockt[k]->sent_cb = LUA_NOREF;
        /*
        if (psvrsockt[k]->disconnect_cb != LUA_NOREF)
          luaL_unref(gL, LUA_REGISTRYINDEX, psvrsockt[k]->disconnect_cb);
        psvrsockt[k]->disconnect_cb = LUA_NOREF;
        */
      }
      
      if (dofree == 97) {
        if (psvrsockt[k]->socket != INVALID_HANDLE) close(psvrsockt[k]->socket);
      }
      else close(socketHandle);
      free(psvrsockt[k]);
      psvrsockt[k] = NULL;
      return ;
    }
    
    for (m=0; m<MAX_SVRCLT_SOCKET; m++) {
      if (psvrsockt[k]->psvrCltsocket[m] == NULL) continue;
      
      if (psvrsockt[k]->psvrCltsocket[m]->client == socketHandle) {
        //close serverClient
        //free memery
        if (psvrsockt[k]->type == TCP) close(socketHandle);
        free(psvrsockt[k]->psvrCltsocket[m]);
        psvrsockt[k]->psvrCltsocket[m] = NULL;
        return ;
      }
    }
  }
  // ** client
  for (k=0; k<MAX_CLT_SOCKET; k++) {
    if (pcltsockt[k] == NULL) continue;
    
    if ((pcltsockt[k]->socket == socketHandle) || (dofree == 97)) {
      //close client socket
      //unref cb function
      //free memery
      if ((gL != NULL) && (dofree > 0)) {
        if (pcltsockt[k]->connect_cb != LUA_NOREF)
          luaL_unref(gL, LUA_REGISTRYINDEX, pcltsockt[k]->connect_cb);
        pcltsockt[k]->connect_cb = LUA_NOREF;
        if (pcltsockt[k]->dnsfound_cb != LUA_NOREF)
          luaL_unref(gL, LUA_REGISTRYINDEX, pcltsockt[k]->dnsfound_cb);
        pcltsockt[k]->dnsfound_cb = LUA_NOREF;
        if (pcltsockt[k]->receive_cb != LUA_NOREF)
          luaL_unref(gL, LUA_REGISTRYINDEX, pcltsockt[k]->receive_cb);
        pcltsockt[k]->receive_cb = LUA_NOREF;
        if (pcltsockt[k]->sent_cb != LUA_NOREF)
          luaL_unref(gL, LUA_REGISTRYINDEX, pcltsockt[k]->sent_cb);
        pcltsockt[k]->sent_cb = LUA_NOREF;
        /*
        if (pcltsockt[k]->disconnect_cb != LUA_NOREF)
          luaL_unref(gL, LUA_REGISTRYINDEX, pcltsockt[k]->disconnect_cb);
        pcltsockt[k]->disconnect_cb = LUA_NOREF;
        */
      }
      
      pcltsockt[k]->clientFlag = NO_ACTION;
      if (dofree == 97) {
        if (pcltsockt[k]->socket != INVALID_HANDLE) close(psvrsockt[k]->socket);
      }
      else close(socketHandle);
      pcltsockt[k]->state = SOCKET_STATE_CLOSED;
      if (dofree > 0) {
        free(pcltsockt[k]->pDomain4Dns);
        pcltsockt[k]->pDomain4Dns = NULL;
        free(pcltsockt[k]);
        pcltsockt[k] = NULL;
      }
      return ;
    }
  }
}

//-------------------------------------------------------
static void _micoNotify_TCPClientConnectedHandler(int fd)
{
  socket_connected = fd;
}

//---------------------------------------------------------------
static void _queueDisconnect(int socket, int cbref, uint8_t type)
{
  if (cbref == LUA_NOREF) return;
  
  queue_msg_t msg;

  msg.L = gL;
  msg.source = type;
  msg.para1 = socket;
  msg.para3 = NULL;
  msg.para4 = NULL;
  msg.para2 = cbref;
  mico_rtos_push_to_queue( &os_queue, &msg, 0);
}

//------------------------------------------------------
static void _queueAction(int socket, int cbref, int len)
{
  if (cbref == LUA_NOREF) return;
  
  queue_msg_t msg;
  char ss[8];
  memset(ss,0x00,8);

  msg.L = gL;
  msg.source = onNet;
  msg.para1 = socket;
  msg.para3 = NULL;
  if (len > -10) {
    sprintf(ss, "%d", len);
    msg.para3 = (uint8_t*)malloc(strlen(ss)+1);
    if (msg.para3 != NULL) strcpy((char*)msg.para3, ss);
  }
  msg.para4 = NULL;
  msg.para2 = cbref;
  mico_rtos_push_to_queue( &os_queue, &msg, 0);
}

//------------------------------------------------------------------
static void _queueIPport(int socket, int cbref, int ip, int port)
{
  if (cbref == LUA_NOREF) return;
  
  queue_msg_t msg;
  char ss[8];
  memset(ss,0x00,8);

  msg.L = gL;
  msg.source = onNet;
  msg.para1 = socket;
  msg.para3 = NULL;
  msg.para4 = NULL;

  // IP address
  char sip[17];
  memset(sip, 0x00, 17);
  inet_ntoa(sip, ip);
  msg.para3 = (uint8_t*)malloc(strlen(sip)+1);
  if (msg.para3 != NULL) strcpy((char*)msg.para3, sip);
  // port
  if (port >= 0) {
    memset(sip, 0x00, 17);
    sprintf(sip, "%d", port);
    msg.para4 = (uint8_t*)malloc(strlen(sip)+1);
    if (msg.para4 != NULL) strcpy((char*)msg.para4, sip);
  }
  msg.para2 = cbref;
  mico_rtos_push_to_queue( &os_queue, &msg, 0);
}

//----------------------------------------------------------------------------------------
static void _queueReceive(int socket, int cbref, int len, uint8_t http, uint16_t wait_tmo)
{
  uint8_t* hdrbuf = NULL;
  
  if (http > 0) {
    if (strstr(recvBuf, "HTTP") == recvBuf) {
      int recv_len = len;
      char* hdrend = strstr(recvBuf, "\r\n\r\n");
      if (hdrend != NULL) {
        int hdrlen = (hdrend - recvBuf);
        //net_log("[NET clt] HTTP Header len: %d\r\n", hdrlen );
        hdrbuf = (uint8_t*)malloc(hdrlen+1);
        if (hdrbuf != NULL) {
          memcpy(hdrbuf, recvBuf, hdrlen);
          hdrbuf[hdrlen] = 0x00;
        }
        recv_len -= (hdrlen + 4);
        memmove(recvBuf, (recvBuf + hdrlen + 4), recv_len);
        recvBuf[recv_len] = 0x00;
        //net_log("[NET clt] HTTP Data len: %d\r\n", recv_len );
      }
    }
  }

  if (cbref == LUA_NOREF) {
    if (wait_tmo > 0) {
      free(hdrbuf);
      // leave recvBuf
    }
    else {
      free(recvBuf);
      recvBuf = NULL;
      free(hdrbuf);
    }
    return;
  }
  
  queue_msg_t msg;
  int recv_len = len;
  
  msg.L = gL;
  msg.source = onNet;
  msg.para1 = socket;
  msg.para3 = NULL;
  msg.para4 = NULL;
  msg.para2 = cbref;

  if (http > 0) msg.para4 = hdrbuf;
  else msg.para4 = NULL;
  msg.para3 = (uint8_t*)malloc(recv_len+1);
  if (msg.para3 != NULL) {
    memcpy((char*)msg.para3, recvBuf, recv_len);
    msg.para3[recv_len] = 0x00;
  }
  mico_rtos_push_to_queue( &os_queue, &msg, 0);

  if (wait_tmo == 0) {
    free(recvBuf);
    recvBuf = NULL;
  }
}

//----------------------------------------------------------------------------------------
static int _receive(int socket, uint8_t type, struct sockaddr_t * clientaddr, uint8_t* res)
{
  fd_set readset;
  struct timeval_t t_val;
  int recv_len = 0;
  int total_len = 0;
  int slen = sizeof(struct sockaddr_t);

  *res = 1;
  free(recvBuf);
  recvBuf = NULL;
  recvBuf = calloc(MAX_RECV_LEN, sizeof(char));
  if (recvBuf == NULL) {
    net_log("\r\n[NET trd] Error allocating receive buffer.\r\n" );
    return total_len;
  }
  
  int buf_size = MAX_RECV_LEN;  
  int n = 1;  
  while (n > 0) {
    if (type == TCP) {
      recv_len = recv(socket, recvBuf+total_len, (buf_size-total_len-1), 0);
    }
    else {
      recv_len = recvfrom(socket, recvBuf+total_len, (buf_size-total_len-1),
                          0, clientaddr, &slen);
    }
    if (recv_len <= 0) break;

    // success, wait next
    total_len += recv_len;
    
    FD_ZERO(&readset);
    FD_SET(socket, &readset);
    t_val.tv_sec = 0;
    t_val.tv_usec = 10*1000;
    n = select(socket+1, &readset, NULL, NULL, &t_val);
    if (n > 0) {
      if (total_len > (buf_size / 2)) {
        // we have more data than the size of the recvBuf, increase the size
        if ((buf_size+512) > max_recvlen) {
          n = 0;
          *res = 0;
        }
        else {
          buf_size += 512;
          recvBuf = realloc(recvBuf, buf_size);
          if (recvBuf == NULL) {
            net_log("\r\n[NET trd] Error reallocating receive buffer.\r\n" );
            total_len = 0;
            n = 0;
          }
        }
      }
    }
  }
  return total_len;
}

//------------------------------------------------------------------
static int _send(int socket, uint8_t type, struct sockaddr_t *paddr)
{
  fd_set wrset;
  struct timeval_t t_val;
  size_t sendlen = 0;
  
  if ((sendBuf == NULL) || (send_len < 1)) {
    free(sendBuf);
    sendBuf = NULL;
    send_len = 0;
    return -1;
  }

  int sent = 0;
  int total_sent = 0;
  while (send_len > 0) {
    if (send_len > 1024) sendlen = 1024;
    else sendlen = send_len;
    
    if (type == TCP) { // TCP socket
      sent = send(socket, (sendBuf+sent), sendlen, 0);
    }
    else { // UDP socket
      char sip[17];
      memset(sip, 0x00, 17);
      inet_ntoa(sip, paddr->s_ip);
      net_log("[NET udp] UDP skt %d, Send to %s:%d\r\n", socket, sip, paddr->s_port);
      sent = sendto(socket, (sendBuf+sent), sendlen, 0, paddr, sizeof(struct sockaddr_t));
    }
    if (sent != sendlen) {
      net_log("[NET snd] Error: %d<>%d\r\n", sent, sendlen);
      sent = -2;
      break;
    }
    total_sent += sent;
    // success, wait next
    send_len -= sendlen;
    if (send_len > 0) {
      FD_ZERO(&wrset);
      FD_SET(socket, &wrset);
      // check socket state
      t_val.tv_sec = 0;
      t_val.tv_usec = 10*1000;
      if (select(socket+1, NULL, &wrset, NULL, &t_val) <= 0) {
        sent = -3;
        break;
      }
    }
  }
  if (sent > 0) sent = total_sent;
  free(sendBuf);
  sendBuf = NULL;
  send_len = 0;
  return sent;
}  

//void _timer_net_handle( lua_State* gL )
//{
// ======= NET thread handler =========
static void _thread_net(void*inContext)
{
  (void)inContext;
  //step 0
  fd_set readset;
  struct timeval_t t_val;
  t_val.tv_sec = 0;
  t_val.tv_usec = 10*1000;
  int k = 0, m = 0;
  uint32_t timeout = mico_get_time();
  LinkStatusTypeDef wifi_link;
  
  if (gL == NULL) goto terminate;

  //net_log("\r\n[NET trd] Net thread started.\r\n" );

  socket_connected = -1;
  // ===========================================================================
  // Main Thread loop
  while (1) {
    mico_rtos_unlock_mutex(&net_mut);
    mico_thread_msleep(5);
    mico_rtos_lock_mutex(&net_mut);
    
    // Check if any socket is active
    int n = 0;
    for (k=0; k<MAX_SVR_SOCKET; k++) {
      if (psvrsockt[k] != NULL) {
        n++;
        break;
      }
    }
    for (k=0; k<MAX_CLT_SOCKET; k++) {
      if (pcltsockt[k] != NULL) {
        n++;
        break;
      }
    }
    //-------------------------------------------------------
    if (n == 0) { // no active sockets
      if ((mico_get_time() - timeout) > 2000) {
        mico_rtos_unlock_mutex(&net_mut);
        gL = NULL;
        goto terminate;
      }
      else continue;
    }
    //-------------------------------------------------------

    // *** Check if wifi is connected

    micoWlanGetLinkStatus( &wifi_link );
    if ( wifi_link.is_connected == false ) {
      wifiConnected = false;
      closeSocket(0, 97); // Close all sockets
      continue;
    }
    wifiConnected = true;
    
    // ===== Check if some socket is connected =================================
    if (socket_connected >= 0) {
      //net_log("[NET nfy] Connection on socket #%d\r\n", socket_connected);
      int type = -1;
      k = -1; m = -1;
      if (getsocketIndex(socket_connected, &type, &k, &m) == false) {
        net_log("[NET nfy] Socket index not found.\r\n");
      }
      else {
        //net_log("[NET nfy] Type #%d, index %d\r\n", type, k);
        if (type == SOCKET_TYPE_CLIENT) {
          if (pcltsockt[k] != NULL) {
            if (pcltsockt[k]->type == TCP) {
              pcltsockt[k]->clientFlag = REQ_ACTION_CONNECTED;
            }
          }
        }
      }
      socket_connected = -1;
    }
    
    uint32_t timeout = mico_get_time();
    // ===== step 1, check socket actions required =============================
    // ** Check server sockets
    for (k=0; k<MAX_SVR_SOCKET; k++) {
      if (psvrsockt[k] == NULL) continue;

      if (psvrsockt[k]->socket != INVALID_HANDLE ) {
        // * Check server client sockets
        for (m=0; m<MAX_SVRCLT_SOCKET; m++) {
          if (psvrsockt[k]->psvrCltsocket[m] == NULL) continue;
          
          if (psvrsockt[k]->psvrCltsocket[m]->client != INVALID_HANDLE) {
            // REQ_ACTION_SEND
            if (psvrsockt[k]->psvrCltsocket[m]->clientFlag == REQ_ACTION_SEND) {
              psvrsockt[k]->psvrCltsocket[m]->clientFlag = NO_ACTION;
              
              if (psvrsockt[k]->type == TCP) {
                data_sent = _send(psvrsockt[k]->psvrCltsocket[m]->client, psvrsockt[k]->type,
                                  &(psvrsockt[k]->psvrCltsocket[m]->addr));
              }
              else {
                data_sent = _send(psvrsockt[k]->socket, psvrsockt[k]->type,
                                  &(psvrsockt[k]->psvrCltsocket[m]->addr));
              }
              if (data_sent < 0) {
                //psvrsockt[k]->psvrCltsocket[m]->clientFlag = REQ_ACTION_DISCONNECT; 
                net_log("[NET scl] Error sending data: %d\r\n", data_sent);
              }
              _queueAction(psvrsockt[k]->psvrCltsocket[m]->client, psvrsockt[k]->sent_cb, data_sent);
            }
            //REQ_ACTION_DISCONNECT
            else if (psvrsockt[k]->psvrCltsocket[m]->clientFlag == REQ_ACTION_DISCONNECT) {
              psvrsockt[k]->psvrCltsocket[m]->clientFlag = NO_ACTION;
    
              _queueDisconnect(psvrsockt[k]->psvrCltsocket[m]->client, psvrsockt[k]->disconnect_cb, (onNet | needUNREF));
              closeSocket(psvrsockt[k]->psvrCltsocket[m]->client, 1);
            }
          }
        } //end for(m=0...
        //REQ_ACTION_DISCONNECT
        if (psvrsockt[k]->clientFlag == REQ_ACTION_DISCONNECT) {
          psvrsockt[k]->clientFlag = NO_ACTION;
          _queueDisconnect(psvrsockt[k]->socket, psvrsockt[k]->disconnect_cb, (onNet | needUNREF));
          closeSocket(psvrsockt[k]->socket, 1);
        }
        // REQ_ACTION_BIND
        else if (psvrsockt[k]->clientFlag == REQ_ACTION_BIND) {
          psvrsockt[k]->clientFlag = NO_ACTION;
          
          struct sockaddr_t addr;
          addr.s_ip = INADDR_ANY;
          addr.s_port = psvrsockt[k]->port;
          if (bind(psvrsockt[k]->socket, &addr, sizeof(struct sockaddr_t)) < 0) {
            net_log("[NET svr] Bind error.\r\n");
          }
          else {
            if (psvrsockt[k]->type == TCP) psvrsockt[k]->clientFlag = REQ_ACTION_LISTEN;
            else if (psvrsockt[k]->type == UDP) psvrsockt[k]->state = SOCKET_STATE_READY;
          }
        }
        //REQ_ACTION_LISTEN
        else if (psvrsockt[k]->clientFlag == REQ_ACTION_LISTEN) {
          psvrsockt[k]->clientFlag = NO_ACTION;
          if (listen(psvrsockt[k]->socket, 0) == 0) psvrsockt[k]->state = SOCKET_STATE_LISTEN;
          else {
            net_log("[NET svr] Listen command failed.\r\n");
          }
        }
        
      } //end if(psvr...
    }
    // ** Check client sockets
    for (k=0; k<MAX_CLT_SOCKET; k++) {
      if (pcltsockt[k] == NULL) continue;
      if (pcltsockt[k]->socket == INVALID_HANDLE) continue;

      // REQ_ACTION_SEND
      if (pcltsockt[k]->clientFlag == REQ_ACTION_SEND) {
        pcltsockt[k]->clientFlag = NO_ACTION;
        if ((sendBuf == NULL) || (send_len < 1)) continue;
        
        data_sent = _send(pcltsockt[k]->socket, pcltsockt[k]->type, &(pcltsockt[k]->addr));
        if (data_sent < 0) {
          pcltsockt[k]->clientFlag = REQ_ACTION_DISCONNECT; 
          net_log("[NET clt] Error sending data: %d\r\n", data_sent);
        }
        _queueAction(pcltsockt[k]->socket, pcltsockt[k]->sent_cb, data_sent);
      }
      // REQ_ACTION_DISCONNECT
      else if ((pcltsockt[k]->clientFlag == REQ_ACTION_DISCONNECT) ||
               (pcltsockt[k]->clientFlag == REQ_ACTION_DISCONNECTFREE)) {
        pcltsockt[k]->state = SOCKET_STATE_NOTCONNECTED;

        /*if (pcltsockt[k]->ssl == 1) {
          ssl_close(pcltsockt[k]->client_ssl);
        }*/
        if (pcltsockt[k]->clientFlag == REQ_ACTION_DISCONNECTFREE) {
          pcltsockt[k]->clientFlag = NO_ACTION;
          _queueDisconnect(pcltsockt[k]->socket, pcltsockt[k]->disconnect_cb, (onNet | needUNREF));
          closeSocket(pcltsockt[k]->socket, 1);
        }
        else {
          pcltsockt[k]->clientFlag = NO_ACTION;
          _queueDisconnect(pcltsockt[k]->socket, pcltsockt[k]->disconnect_cb, onNet);
          closeSocket(pcltsockt[k]->socket, 0);
        }
      }
      // REQ_ACTION_CONNECTED
      else if (pcltsockt[k]->clientFlag == REQ_ACTION_CONNECTED) {
        pcltsockt[k]->clientFlag = NO_ACTION;
        pcltsockt[k]->state = SOCKET_STATE_CONNECTED;
        _queueAction(pcltsockt[k]->socket, pcltsockt[k]->connect_cb, -99);
        /*if (pcltsockt[k]->ssl == 1) {
          ssl_version_set(TLS_V1_2_MODE);
          int ssl_errno = 0;
          pcltsockt[k]->client_ssl = ssl_connect(pcltsockt[k]->socket, 0, NULL, &ssl_errno);
          if (pcltsockt[k]->client_ssl == NULL) {
            pcltsockt[k]->ssl = 0;
            pcltsockt[k]->clientFlag = REQ_ACTION_DISCONNECT;
            net_log("[NET clt] SSL error %d\r\n", ssl_errno);
          }
          else {
            net_log("[NET clt] Using SSL connection: %d\r\n", k);
          }
        }*/
      }
      // REQ_ACTION_GETIP
      else if (pcltsockt[k]->clientFlag == REQ_ACTION_GETIP) {
        if (pcltsockt[k]->pDomain4Dns != NULL) {
          char pIPstr[16] = {0};
          if (gethostbyname((char *)pcltsockt[k]->pDomain4Dns, (uint8_t *)pIPstr, 16) == kNoErr) {
            //free(pcltsockt[k]->pDomain4Dns);
            //pcltsockt[k]->pDomain4Dns = NULL;
            pcltsockt[k]->addr.s_ip = inet_addr(pIPstr);
            pcltsockt[k]->clientFlag = REQ_ACTION_GOTIP;
          }
          else {
            pcltsockt[k]->addr.s_ip = 0;
            net_log("[NET clt] GetIP error [%s]\r\n", pcltsockt[k]->pDomain4Dns);
            pcltsockt[k]->clientFlag = NO_ACTION;
          }
        }
        else pcltsockt[k]->clientFlag = NO_ACTION;
      }
      // REQ_ACTION_GOTIP
      else if (pcltsockt[k]->clientFlag == REQ_ACTION_GOTIP) {
        pcltsockt[k]->clientFlag = NO_ACTION;
        
        pcltsockt[k]->state = SOCKET_STATE_READY;
        _queueIPport(pcltsockt[k]->socket, pcltsockt[k]->dnsfound_cb, pcltsockt[k]->addr.s_ip, -1); 

        //auto connect tcp socket
        if (pcltsockt[k]->type == TCP) {
          struct sockaddr_t *paddr = &(pcltsockt[k]->addr);
          int slen = sizeof(struct sockaddr_t);
          //_micoNotify will be called if connected
          if (connect(pcltsockt[k]->socket, paddr, slen) < 0) {
            net_log("[NET clt] Connection error.\r\n");
          }
        }
      }
      // REQ_ACTION_BIND
      else if (pcltsockt[k]->clientFlag == REQ_ACTION_BIND) {
        pcltsockt[k]->clientFlag = NO_ACTION;
        
        if (pcltsockt[k]->local_port > 0) {
          struct sockaddr_t addr;
          addr.s_ip = INADDR_ANY;
          addr.s_port = pcltsockt[k]->local_port;
          if (bind(pcltsockt[k]->socket, &addr, sizeof(struct sockaddr_t)) < 0) {
            net_log("[NET clt] Bind error.\r\n");
          }
          pcltsockt[k]->clientFlag = REQ_ACTION_GETIP;
        }
      }
    }
    
    // ===== step 2, Check receive/disconnect events =============================
    // ** select all server&client socket to monitor **
    int maxfd = -1;
    FD_ZERO(&readset);
    // == Server sockets
    for (k=0; k<MAX_SVR_SOCKET; k++) {
      if (psvrsockt[k] == NULL) continue;
      if (psvrsockt[k]->socket == INVALID_HANDLE ) continue;

      // = server socket (accept)
      if (psvrsockt[k]->socket > maxfd) maxfd = psvrsockt[k]->socket;
      FD_SET(psvrsockt[k]->socket, &readset);

      // = server client sockets (recieve or disconnect)
      for (m=0; m<MAX_SVRCLT_SOCKET; m++) {
        if (psvrsockt[k]->psvrCltsocket[m] == NULL) continue;
        if ((psvrsockt[k]->type == UDP) || (psvrsockt[k]->psvrCltsocket[m]->client == INVALID_HANDLE)) continue;

        if (psvrsockt[k]->psvrCltsocket[m]->client > maxfd) 
          maxfd = psvrsockt[k]->psvrCltsocket[m]->client;
        FD_SET(psvrsockt[k]->psvrCltsocket[m]->client, &readset);
      }
    }
    // == Client sockets
    for (k=0; k<MAX_CLT_SOCKET; k++) {
      if(pcltsockt[k] ==NULL) continue;
      
      // = client socket (recieve or disconnect)
      if (pcltsockt[k]->socket != INVALID_HANDLE) {
        if (pcltsockt[k]->socket > maxfd) maxfd = pcltsockt[k]->socket;
        FD_SET(pcltsockt[k]->socket, &readset);
      }
    }
    if (maxfd < 0) continue; // nothing to check
    
    // === Check event ===
    mico_rtos_unlock_mutex(&net_mut);
    t_val.tv_sec = 0;
    t_val.tv_usec = 10*1000;
    if (select(maxfd+1, &readset, NULL, NULL, &t_val) <= 0) continue; // no event
    mico_rtos_lock_mutex(&net_mut);
    
    // **** Analyze server sockets events ***
    for (k=0; k<MAX_SVR_SOCKET; k++) {
      if (psvrsockt[k] == NULL) continue;
      if (psvrsockt[k]->socket == INVALID_HANDLE ) continue;
      
      if (psvrsockt[k]->type == TCP) {
        // == Server TCP socket (accept) ==
        if (FD_ISSET(psvrsockt[k]->socket, &readset)) {
          struct sockaddr_t clientaddr;
          int len = sizeof(struct sockaddr_t);
          // accept connection
          int clientTmp = accept(psvrsockt[k]->socket, &clientaddr, &len);
          if (clientTmp >= 0) {
            // get a new index, create new psvrCltsocket
            int mi = 0;
            for (mi=0; mi<MAX_SVRCLT_SOCKET; mi++) {
               if (psvrsockt[k]->psvrCltsocket[mi] == NULL) break; // found free socket
             }
            if (mi == MAX_SVRCLT_SOCKET) {
              net_log("[NET srv] Max Client Number is reached");
              continue;
            }
            psvrsockt[k]->psvrCltsocket[mi] = (_lsvrCltsocket_t*)malloc(sizeof(_lsvrCltsocket_t));
            if (psvrsockt[k]->psvrCltsocket[mi] == NULL) {
              net_log("[NET srv] Client memory allocation failed");
              continue;
            }
            //net_log("[NET srv] Accepted client connection: %d.\r\n", clientTmp);
            psvrsockt[k]->psvrCltsocket[mi]->client= clientTmp;
            psvrsockt[k]->psvrCltsocket[mi]->addr.s_ip= clientaddr.s_ip;
            psvrsockt[k]->psvrCltsocket[mi]->addr.s_port= clientaddr.s_port;
            psvrsockt[k]->psvrCltsocket[mi]->clientFlag = NO_ACTION;
            psvrsockt[k]->psvrCltsocket[mi]->state = SOCKET_STATE_CONNECTED;
            // call accept cb function
            _queueIPport(clientTmp, psvrsockt[k]->accept_cb, clientaddr.s_ip, clientaddr.s_port); 
          }
          else {
            net_log("[NET srv] Accept Error.\r\n" );
          }
        }
      }
      else if (psvrsockt[k]->type == UDP) {
        if (FD_ISSET(psvrsockt[k]->socket, &readset)) {
          // == Server UDP socket (recvfrom) ==
          struct sockaddr_t clientaddr;
          uint8_t stat = 1;
          char sip[17];
          memset(sip, 0x00, 17);

          int received = _receive(psvrsockt[k]->socket, UDP, &clientaddr, &stat);
          if (received <= 0) {
            psvrsockt[k]->clientFlag = REQ_ACTION_DISCONNECT;
            continue;
          }
          recvBuf[received] = 0x00;
          int mi = 0;
          // == check if we already have the server client socket with the same client address
          for (mi=0; mi<MAX_SVRCLT_SOCKET; mi++) {
            if ((psvrsockt[k]->psvrCltsocket[mi] !=NULL) &&
                (psvrsockt[k]->psvrCltsocket[mi]->addr.s_ip==clientaddr.s_ip)) {
              // use existing socket, but set the port
              psvrsockt[k]->psvrCltsocket[mi]->addr.s_port = clientaddr.s_port;
              goto doUdpRecieve;
            }
          }
          // there is none, create new one
          for (mi=0; mi<MAX_SVRCLT_SOCKET; mi++) {
            if(psvrsockt[k]->psvrCltsocket[mi]==NULL) break;
          }
          if (mi == MAX_SVRCLT_SOCKET) {
            //if reach max, return to 0
            mi = 0;
            closeSocket(psvrsockt[k]->psvrCltsocket[mi]->client, 1);
          }
          psvrsockt[k]->psvrCltsocket[mi] = (_lsvrCltsocket_t*)malloc(sizeof(_lsvrCltsocket_t));
          if (psvrsockt[k]->psvrCltsocket[mi] == NULL) {
            net_log("[NET srv] Client memory allocation failed");
            continue;
          }
          psvrsockt[k]->psvrCltsocket[mi]->client = 32767 - mi;
          psvrsockt[k]->psvrCltsocket[mi]->addr.s_ip = clientaddr.s_ip;
          psvrsockt[k]->psvrCltsocket[mi]->addr.s_port = clientaddr.s_port;
          psvrsockt[k]->psvrCltsocket[mi]->clientFlag = NO_ACTION;
            
          doUdpRecieve:
            inet_ntoa(sip, clientaddr.s_ip);
            net_log("[NET udp] UDP Received from %s:%d\r\n", sip, clientaddr.s_port);
            data_received = received;
            _queueReceive(psvrsockt[k]->psvrCltsocket[mi]->client, psvrsockt[k]->receive_cb, received, 0, 0);
         } //if(FD_ISSET...
       }
      
      // ** Analyze server client sockets events **
      for (m=0; m<MAX_SVRCLT_SOCKET; m++) {
        if (psvrsockt[k]->psvrCltsocket[m] == NULL) continue;
        if ((psvrsockt[k]->type == UDP) || (psvrsockt[k]->psvrCltsocket[m]->client == INVALID_HANDLE)) continue;
        
        if (FD_ISSET(psvrsockt[k]->psvrCltsocket[m]->client, &readset)) {
          //tcp read  recv
          uint8_t stat = 1;
          int received = _receive(psvrsockt[k]->psvrCltsocket[m]->client, TCP, NULL, &stat);
          if (received <= 0) {
            psvrsockt[k]->psvrCltsocket[m]->clientFlag = REQ_ACTION_DISCONNECT;
            continue;
          }
          // success call recieve_cb
          recvBuf[received] = 0x00;
          data_received = received;
          _queueReceive(psvrsockt[k]->psvrCltsocket[m]->client, psvrsockt[k]->receive_cb, received, 0, 0);
        }
      }
    }

    // *** Analyze client sockets events **
    for (k=0; k<MAX_CLT_SOCKET; k++) {
      if (pcltsockt[k] == NULL) continue;
      if (pcltsockt[k]->socket == INVALID_HANDLE) continue;

      if (FD_ISSET(pcltsockt[k]->socket, &readset)) {
        if (pcltsockt[k]->type == TCP) {
          // == TCP client: recieve or disconnect ==
          uint8_t stat = 1;
          int received = 0;
          /*if (pcltsockt[k]->ssl == 1) {
            received = ssl_recv(pcltsockt[k]->client_ssl, recvBuf, MAX_RECV_LEN-1);
          }
          else {*/
            received = _receive(pcltsockt[k]->socket, TCP, NULL, &stat);
          //}
          if ((recvBuf == NULL) || (received <= 0)) {
            //net_log("[NET clt] TCP Disconnect skt: %d\r\n", k);
            pcltsockt[k]->clientFlag = REQ_ACTION_DISCONNECT;
            continue;
          }
          // success, call recieve_cb
          recvBuf[received] = 0x00;
          data_received = received;
          _queueReceive(pcltsockt[k]->socket, pcltsockt[k]->receive_cb,
                        received, pcltsockt[k]->http, pcltsockt[k]->wait_tmo); 

          if (stat) pcltsockt[k]->clientFlag = REQ_ACTION_DISCONNECT;
        }
        else if (pcltsockt[k]->type == UDP) {
          // == UDP client: recieve or disconnect ==
          struct sockaddr_t clientaddr;
          uint8_t stat = 1;
          int received = _receive(pcltsockt[k]->socket, UDP, &clientaddr, &stat);
          if (received <= 0) {
            //net_log("[NET clt] UDP Disconnect skt: %d\r\n", k);
            pcltsockt[k]->clientFlag = REQ_ACTION_DISCONNECT;
            continue;
          }
          // success, call recieve_cb
          recvBuf[received]=0x00;
          data_received = received;
          _queueReceive(pcltsockt[k]->socket, pcltsockt[k]->receive_cb,
                        received, 0, pcltsockt[k]->wait_tmo);
        }
      }
    }
  } // while(1)

terminate:
  free(recvBuf);
  recvBuf = NULL;
  free(sendBuf);
  sendBuf = NULL;
  net_thread_is_started = false;
  //net_log("\r\n[NET trd] Net thread stopped.\r\n" );
  mico_rtos_delete_thread( NULL );
}

//---------------------------------------------------
static int _getOpts(lua_State* L, uint8_t n, int skt)
{
  if (!lua_istable(L, n)) return 1;
  int res = 1;
  
  lua_getfield(L, n, "lport");
  if (!lua_isnil(L, -1)) {  // found?
    if( lua_isstring(L, -1) ) {
      int lport = luaL_checkinteger( L, -1 );
      if ((lport < 2) || (lport > 65535)) {
        net_log("Local port range error\r\n" );
        return -6;
      }
      pcltsockt[skt]->local_port = lport;
      res = 2;
    }
    else {
      net_log("Local port arg error\r\n" );
      return -6;
    }
  }

  lua_getfield(L, n, "http");
  if (!lua_isnil(L, -1)) {  // found?
    if( lua_isstring(L, -1) ) {
      pcltsockt[skt]->http = 1;
      int http = luaL_checkinteger( L, -1 );
      if (http == 2) pcltsockt[skt]->http = 2;
    }
  }
  lua_getfield(L, n, "wait");
  if (!lua_isnil(L, -1)) {  // found?
    if( lua_isstring(L, -1) ) {
      int wait = luaL_checkinteger( L, -1 );
      if ((wait > 0) && (wait < 60)) pcltsockt[skt]->wait_tmo = wait * 1000;
      else pcltsockt[skt]->wait_tmo = 0;
    }
  }
  return res;
}

//-----------------------------------
static int _preparePost(lua_State* L)
{
  if ((!lua_istable(L, 3)) && (!lua_isstring(L, 3))) {
    net_log("POST data arg missing\r\n" );
    return -1;
  }
  
  int data_len = 0;
  int buf_len = 512;
  char* post_data = NULL;
  char bndry[32];

  if (lua_istable(L, 3)) {
    post_data = malloc(512);
    if (post_data == NULL) {
      net_log("POST memory allocation error\r\n" );
      return -1;
    }
    memset(bndry,0x00,20);
    int randn = rand();
    sprintf(bndry, "_____%d_____", randn);
    int bndry_len = strlen(bndry);

    // table is in the stack at index 3
    lua_pushnil(L);  // first key
    while (lua_next(L, 3) != 0) {
      // uses 'key' (at index -2) and 'value' (at index -1)
      if ((lua_isstring(L, -1)) && (lua_isstring(L, -2))) {
        size_t klen = 0;
        size_t vlen = 0;
        const char* key = lua_tolstring(L, -2, &klen);
        const char* value = lua_tolstring(L, -1, &vlen);
        if ((klen > 0) && (vlen > 0)) {
          if (post_data != NULL) {    
            if (((bndry_len*2)+strlen(key)+strlen(value)+51) > buf_len) {
              post_data = realloc(post_data, (data_len-buf_len+128));
              if (post_data == NULL) {
                net_log("POST memory reallocation error\r\n" );
              }
            }
            if (post_data != NULL) {    
              sprintf(post_data+data_len,
                      "--%s\r\nContent-Disposition: form-data; name=%s\r\n\r\n%s\r\n--%s\r\n\r\n",
                      bndry, key, value, bndry);
              data_len = strlen(post_data);
            }
          }
        }
      }
      //'Content-Disposition: form-data; name=' + par[0] + '\r\n\r\n'
      // removes 'value'; keeps 'key' for next iteration
      lua_pop(L, 1);
    }
  }
  else {
    size_t jlen = 0;
    const char *jdata = NULL;
    jdata = luaL_checklstring( L, 3, &jlen );
    if ((jlen < 3) || (jlen > 500) || (jdata == NULL)) {
      net_log("Error preparing POST data (json)\r\n" );
      return -1;
    }
    post_data = malloc(jlen+128);
    if (post_data == NULL) {
      net_log("POST memory allocation error\r\n" );
      return -1;
    }
    sprintf(post_data,
            "Content-Type: application/json\r\nContent-Length: %d\r\n\r\n%s",
            jlen, jdata);
    data_len = strlen(post_data);
  }
  
  if (post_data == NULL) {
    net_log("Error preparing POST data\r\n" );
    return -1;
  }
  sendBuf = realloc(sendBuf, (send_len+strlen(post_data)+128));
  if (sendBuf == NULL) {
    net_log("SEND memory reallocation error\r\n" );
    free(post_data);
    return -1;
  }
  if (lua_istable(L, 3)) {
    sprintf(sendBuf+send_len, 
            "Content-Type: multipart/form-data; boundary=%s\r\nContent-Length: %d\r\n\r\n%s",
            bndry, strlen(post_data), post_data);
  }
  else sprintf(sendBuf+send_len, "%s", post_data);
  send_len = strlen(sendBuf);
  free(post_data);
  return 0;
}

// socket=net.new(net.TCP/UDP,net.SERVER/net.CLIENT)
//=================================
static int lnet_new( lua_State* L )
{
  int protocolType = luaL_checkinteger( L, 1 );
  int socketType = luaL_checkinteger( L, 2);
  int socketHandle = INVALID_HANDLE;
  int err = 0;

  if (!net_thread_is_started) {
    LinkStatusTypeDef wifi_link;
    err = micoWlanGetLinkStatus( &wifi_link );
    if (err == 0) wifiConnected = wifi_link.is_connected;
  }

  if (!wifiConnected) {
    net_log("WiFi not active\r\n" );
    err = -9;
    wifiConnected = false;
    goto errexit1;
  }

  if ( (protocolType != TCP) && (protocolType != UDP) ) {
    net_log("Wrong arg type, net.TCP or net.UDP is needed\r\n" );
    err = -1;
    goto errexit1;
  }
  
  if ( (socketType != SOCKET_SERVER) && (socketType !=SOCKET_CLIENT) ) {
    net_log("Wrong arg type, net.SERVER or net.CLIENT is needed\r\n" );
    err = -2;
    goto errexit1;
  }
    
  if (net_mut == NULL) {
    mico_rtos_init_mutex(&net_mut);
  }

  if (protocolType == TCP) socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  else socketHandle = socket(AF_INET, SOCK_DGRM, IPPROTO_UDP);

  if (socketHandle < 0) {
    net_log("Error obtaining socket handle.\r\n" );
    err = -3;
    goto errexit;
  }
  
  if (gL == NULL) gL = L;

  mico_rtos_lock_mutex(&net_mut);
  if (socketType == SOCKET_SERVER) { // ** server socker
    int k = 0;
    for(k = 0; k<MAX_SVR_SOCKET; k++){
      if (psvrsockt[k] == NULL) break;
    }
    if (k == MAX_SVR_SOCKET) {
      net_log("Max SOCKET Number is reached\r\n" );
      err = -4;
      goto errexit;
    }
    
    psvrsockt[k] = (svrsockt_t*)malloc(sizeof(svrsockt_t));
    if (psvrsockt[k] == NULL) {
      net_log("Memory allocation failed\r\n" );
      err = -5;
      goto errexit;
    }
    psvrsockt[k]->socket = socketHandle;
    psvrsockt[k]->port = INVALID_HANDLE;
    psvrsockt[k]->type = protocolType;
    psvrsockt[k]->accept_cb = LUA_NOREF;
    psvrsockt[k]->receive_cb = LUA_NOREF;
    psvrsockt[k]->sent_cb = LUA_NOREF;
    psvrsockt[k]->disconnect_cb = LUA_NOREF;
    psvrsockt[k]->clientFlag = NO_ACTION;
    psvrsockt[k]->state = SOCKET_STATE_NOTCONNECTED;
    for (int m=0; m<MAX_SVRCLT_SOCKET; m++) {
      psvrsockt[k]->psvrCltsocket[m] = NULL;
    }
  }
  else { // ** client socket
    int k=0;
    for (k=0; k<MAX_CLT_SOCKET; k++) {
      if (pcltsockt[k] == NULL) break;
    }
    if (k == MAX_CLT_SOCKET) {
      net_log("Max SOCKET Number is reached\r\n" );
      err = -6;
      goto errexit;
    }
    pcltsockt[k] = (cltsockt_t*)malloc(sizeof(cltsockt_t));
    if (pcltsockt[k] == NULL) {
      net_log("Memory allocation failed\r\n" );
      err = -7;
      goto errexit;
    }
    pcltsockt[k]->socket = socketHandle;
    pcltsockt[k]->type = protocolType;
    pcltsockt[k]->connect_cb = LUA_NOREF;
    pcltsockt[k]->dnsfound_cb = LUA_NOREF;
    pcltsockt[k]->receive_cb = LUA_NOREF;
    pcltsockt[k]->sent_cb = LUA_NOREF;
    pcltsockt[k]->disconnect_cb = LUA_NOREF;
    pcltsockt[k]->clientFlag = NO_ACTION;
    pcltsockt[k]->state = SOCKET_STATE_NOTCONNECTED;
    pcltsockt[k]->pDomain4Dns = NULL;
    pcltsockt[k]->local_port = -1;
    pcltsockt[k]->http = 0;
    //pcltsockt[k]->ssl = 0;
    //pcltsockt[k]->client_ssl = NULL;
  }
  mico_rtos_unlock_mutex(&net_mut);

  // ** start Net thread if not started
  if (!net_thread_is_started) {
    if (mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY-1, "Net_Thread", _thread_net, 1024, NULL) != kNoErr) {
      net_log("Net thread not started.\r\n" );
      closeSocket(socketHandle, 1);
      err = -8;
      goto errexit1;
    }
    else net_thread_is_started = true;
  } 
  
  lua_pushinteger(L, socketHandle);
  return 1;

errexit:
  mico_rtos_unlock_mutex(&net_mut);
errexit1:
  lua_pushinteger(L, err);
  return 1;
}

//net.send(socket,"data",[post_data],[options])
//==================================
static int lnet_send( lua_State* L )
{
  int socketHandle = luaL_checkinteger( L, 1 );
  int type=0, k=0, m=0;
  int err = 0;
  size_t len=0;
  const char *data = NULL;
  uint8_t oldhttp = 0xFF;
  uint16_t oldwait = 0xFFFF;
  int cbsend = LUA_NOREF;
  int cbrecv = LUA_NOREF;
  
  mico_rtos_lock_mutex(&net_mut);
  if (false == getsocketIndex(socketHandle,&type,&k,&m)) {
    err = -1;
    net_log("Socket is not valid\r\n" );
    goto exit1;
  }
  if (!wifiConnected) {
    net_log("WiFi not active\r\n" );
    err = -9;
    wifiConnected = false;
    goto exit1;
  }
  if (type == SOCKET_TYPE_SERVER) {
    err = -2;
    net_log("Can't send on server socket\r\n" );
    goto exit1;
  }
  if (pcltsockt[k]->state == SOCKET_STATE_CLOSED) {
    err = -3;
    net_log("Socket not connected\r\n" );
    goto exit1;
  }
  if (sendBuf != NULL) {
    err = -4;
    net_log("Previous data not sent\r\n" );
    goto exit1;
  }
  
  data = luaL_checklstring( L, 2, &len );
  if ((len < 1) || (len > 975) || (data == NULL)) {
    err = -5;
    net_log("Data length must be < 976\r\n" );
    goto exit1;
  }

  if (!net_thread_is_started) {
    err = -6;
    net_log("Net thread not started!\r\n" );
    goto exit1;
  }

  free(sendBuf);
  sendBuf = NULL;

  if (type == SOCKET_TYPE_CLIENT) {
    oldhttp = pcltsockt[k]->http;
    oldwait = pcltsockt[k]->wait_tmo;
    if (strstr(data, "POST ") == data) err = _getOpts(L, 4, k);
    else err = _getOpts(L, 3, k);
    err = 0;
  }
  
  if ((type != SOCKET_TYPE_CLIENT) || (pcltsockt[k]->type != TCP) || (pcltsockt[k]->http == 0)) {
    sendBuf = (char*)malloc(len+1);
    if (sendBuf == NULL) {
      err = -7;
      net_log("Memory allocation error.\r\n" );
      goto exit;
    }
    memcpy(sendBuf, data, len);
    sendBuf[len] = 0x00;
    send_len = len;
  }
  
  if (type == SOCKET_TYPE_SVRCLT) {
    // ** tcp or udp server client socket
    data_sent = -99;
    psvrsockt[k]->psvrCltsocket[m]->clientFlag = REQ_ACTION_SEND;
  }
  else {
    // ** tcp or udp client socket
    // save socket options
    // get additional options
    
    if ((pcltsockt[k]->type == TCP) && (pcltsockt[k]->http > 0)) {
      sendBuf = (char*)malloc(len+128);
      if (sendBuf == NULL) {
        err = -7;
        net_log("Memory allocation error.\r\n" );
        goto exit;
      }
      memcpy(sendBuf, data, len);
      if (pcltsockt[k]->http == 2) sprintf(sendBuf+len, " HTTP/1.0\r\nConnection: close\r\nHost: %s\r\n", pcltsockt[k]->pDomain4Dns);
      else sprintf(sendBuf+len, " HTTP/1.1\r\nConnection: close\r\nHost: %s\r\n", pcltsockt[k]->pDomain4Dns);
      send_len = strlen(sendBuf);
      
      if (strstr(sendBuf, "GET ") == sendBuf) {
        sprintf(sendBuf+send_len, "\r\n");
        send_len = strlen(sendBuf);
      }
      else if (strstr(sendBuf, "POST ") == sendBuf) {
        if (_preparePost(L) < 0) {
          free(sendBuf);
          sendBuf = NULL;
          err = -8;
          goto exit;
        }
      }
      else {
        free(sendBuf);
        sendBuf = NULL;
        err = -7;
        net_log("HTTP request must start wiht 'GET ' or 'POST '\r\n" );
        goto exit;
      }
    }
    
    data_sent = -99;
    data_received = -99;
    pcltsockt[k]->clientFlag = REQ_ACTION_SEND;
    
    if (pcltsockt[k]->wait_tmo > 0) {
      // save cb functions and disable then
      cbsend = pcltsockt[k]->sent_cb;
      pcltsockt[k]->sent_cb = LUA_NOREF;
      cbrecv = pcltsockt[k]->receive_cb;
      pcltsockt[k]->receive_cb = LUA_NOREF;

      // *** wait until sent ***
      int tmo = mico_get_time();
      while (data_sent == -99) {
        if ((mico_get_time() - tmo) > pcltsockt[k]->wait_tmo) break;
        mico_rtos_unlock_mutex(&net_mut);
        mico_thread_msleep(50);
        mico_rtos_lock_mutex(&net_mut);
        luaWdgReload();
      }
      if (data_sent <= 0) {
        net_log("Data not sent (err: %d)\r\n", data_sent);
        err = -10;
        goto exit;
      }

      tmo = mico_get_time();
      while (data_received == -99) {
        if ((mico_get_time() - tmo) > pcltsockt[k]->wait_tmo) break;
        mico_rtos_unlock_mutex(&net_mut);
        mico_thread_msleep(50);
        mico_rtos_lock_mutex(&net_mut);
        luaWdgReload();
      }
      if (data_received <= 0) {
        net_log("No data received (err: %d)\r\n", data_received);
        lua_pushinteger(L, -11);
        lua_pushnil(L);
      }
      else {
        lua_pushinteger(L, 0);
        lua_pushstring(L, recvBuf);
        free(recvBuf);
        recvBuf = NULL;
      }
      if (oldhttp != 0xFF) pcltsockt[k]->http = oldhttp;
      if (oldwait != 0xFFFF) pcltsockt[k]->wait_tmo = oldwait;
      if (cbsend != LUA_NOREF) pcltsockt[k]->sent_cb = cbsend;
      if (cbrecv != LUA_NOREF) pcltsockt[k]->receive_cb = cbrecv;

      mico_rtos_unlock_mutex(&net_mut);
      return 2;
    }
  }
  err = 0;
  
exit:
  if (type == SOCKET_TYPE_CLIENT) {
    if (oldhttp != 0xFF) pcltsockt[k]->http = oldhttp;
    if (oldwait != 0xFFFF) pcltsockt[k]->wait_tmo = oldwait;
    if (cbsend != LUA_NOREF) pcltsockt[k]->sent_cb = cbsend;
    if (cbrecv != LUA_NOREF) pcltsockt[k]->receive_cb = cbrecv;
  }
exit1:  
  mico_rtos_unlock_mutex(&net_mut);
  lua_pushinteger(L, err);
  return 1;
}


//net.start(socket,port)
//net.start(socket,port,"domain",[options])
//===================================
static int lnet_start( lua_State* L )
{
  int socketHandle = luaL_checkinteger( L, 1 );
  int port = luaL_checkinteger( L, 2 );
  int type=0, k=0, m=0, err=0;
  
  mico_rtos_lock_mutex(&net_mut);
  if ((type == SOCKET_TYPE_SVRCLT) || (false == getsocketIndex(socketHandle,&type,&k,&m))) {
    net_log("Socket is not valid\r\n" );
    err = -1;
    goto errexit;
  }

  if (!wifiConnected) {
    net_log("WiFi not active\r\n" );
    err = -9;
    wifiConnected = false;
    goto errexit;
  }

  if (type == SOCKET_TYPE_SERVER) {
    // ** server socket
    uint32_t opt=0;
    err = setsockopt(socketHandle, 0, SO_BLOCKMODE, &opt, 4); //non block
    if (err < 0) {
      net_log("Set socket options failed\r\n" );
      err = -2;
      goto errexit;
     }
    //opt = MAX_CLIENT_NUM;
    //setsockopt(socketHandle,0,TCP_MAX_CONN_NUM,&opt,1); //max client num

    psvrsockt[k]->port = port;
    psvrsockt[k]->clientFlag = REQ_ACTION_BIND;
  }
  else {
    // ** client socket
    free(pcltsockt[k]->pDomain4Dns);
    pcltsockt[k]->pDomain4Dns = NULL;
    //pcltsockt[k]->ssl = 0;
    //pcltsockt[k]->client_ssl = NULL;
    
    if (pcltsockt[k]->state == SOCKET_STATE_CLOSED) {
      // reopen the socket
      int socketHandle = INVALID_HANDLE;
      if (pcltsockt[k]->type == TCP) socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      else socketHandle = socket(AF_INET, SOCK_DGRM, IPPROTO_UDP);

      if (socketHandle < 0) {
        net_log("Error obtaining socker handle.\r\n" );
        err = -3;
        goto errexit;
      }
      pcltsockt[k]->socket = socketHandle;
    }
    pcltsockt[k]->state = SOCKET_STATE_NOTCONNECTED;
    
    size_t len = 0;
    const char *domain = luaL_checklstring( L, 3, &len );
    if ((len < 3) || (len > 128) || (domain == NULL)) {
      net_log("Domain needed\r\n" );
      err = -4;
      goto errexit;
    }

    pcltsockt[k]->pDomain4Dns = (char*)malloc(len+1);
    if (pcltsockt[k]->pDomain4Dns == NULL) {
      net_log("Memory allocation failed\r\n" );
      err = -5;
      goto errexit;
    }
    strcpy(pcltsockt[k]->pDomain4Dns, domain);
    
    pcltsockt[k]->wait_tmo = 0;
    pcltsockt[k]->http = 0;
    
    // get additional options
    err = _getOpts(L, 4, k);
    if (err < 0) goto errexit;

    if (err == 2) pcltsockt[k]->clientFlag = REQ_ACTION_BIND; // local port was givven
    else pcltsockt[k]->clientFlag = REQ_ACTION_GETIP;
    err = 0;
    
    pcltsockt[k]->addr.s_port = port;
    
    uint32_t opt=0;
    err = setsockopt(pcltsockt[k]->socket, 0, SO_BLOCKMODE, &opt, 4); //non block
    if (err < 0) {
      net_log("Set socket options failed\r\n" );
      err = -7;
      goto errexit;
     }
    
    if (pcltsockt[k]->type == TCP) {
      mico_system_notify_register( mico_notify_TCP_CLIENT_CONNECTED,
                                  (void *)_micoNotify_TCPClientConnectedHandler, NULL );
    }
    
    if (pcltsockt[k]->wait_tmo > 0) {
      int cbconn = LUA_NOREF;
      if (pcltsockt[k]->type == TCP) {
        // save connect cb function and disable it
        cbconn = pcltsockt[k]->connect_cb;
        pcltsockt[k]->connect_cb = LUA_NOREF;
      }

      int tmo = mico_get_time();
      uint8_t sstate = 0;
      if (pcltsockt[k]->type == TCP) sstate = SOCKET_STATE_CONNECTED;
      else sstate = SOCKET_STATE_READY;
      
      while (pcltsockt[k]->state != sstate) {
        if ((mico_get_time() - tmo) > pcltsockt[k]->wait_tmo) break;
        mico_rtos_unlock_mutex(&net_mut);
        mico_thread_msleep(50);
        mico_rtos_lock_mutex(&net_mut);
        luaWdgReload();
      }
      if (pcltsockt[k]->state != sstate) {
        if (pcltsockt[k]->type == TCP) {
          net_log("Socket not connected!\r\n" );
        }
        else {
          net_log("Socket not ready!\r\n" );
        }
        err = -10;
        if ((pcltsockt[k]->type == TCP) && (cbconn == LUA_NOREF)) {
          // restore connect cb function
          pcltsockt[k]->connect_cb = cbconn;
        }
        goto errexit;
      }
      if ((pcltsockt[k]->type == TCP) && (cbconn == LUA_NOREF)) {
        // restore connect cb function
        pcltsockt[k]->connect_cb = cbconn;
      }
    }
  }
  err = 0;

errexit:
  mico_rtos_unlock_mutex(&net_mut);
  lua_pushinteger(L, err);
  return 1;
}

//net.close(socket)
//===================================
static int lnet_close( lua_State* L )
{
  int socketHandle = luaL_checkinteger( L, 1 );
  int type=0,k=0,m=0;
  
  mico_rtos_lock_mutex(&net_mut);
  if (getsocketIndex(socketHandle,&type,&k,&m) == false) {
    mico_rtos_unlock_mutex(&net_mut);
    net_log("Socket is not valid\r\n" );
    lua_pushinteger(L, -1);
    return 1;
  }

  if (net_thread_is_started) {
    if (type == SOCKET_TYPE_SERVER) psvrsockt[k]->clientFlag = REQ_ACTION_DISCONNECT;
    else if (type == SOCKET_TYPE_CLIENT) pcltsockt[k]->clientFlag = REQ_ACTION_DISCONNECTFREE;
    else if (type == SOCKET_TYPE_SVRCLT) psvrsockt[k]->psvrCltsocket[m]->clientFlag = REQ_ACTION_DISCONNECT;
  }
  else {
    if (type == SOCKET_TYPE_SERVER) {
      _queueDisconnect(psvrsockt[k]->socket, psvrsockt[k]->disconnect_cb, (onNet | needUNREF));
      closeSocket(psvrsockt[k]->socket, 1);
    }
    else if (type == SOCKET_TYPE_CLIENT) {
      _queueDisconnect(pcltsockt[k]->socket, pcltsockt[k]->disconnect_cb, (onNet | needUNREF));
      closeSocket(pcltsockt[k]->socket, 1);
    }
    else if (type == SOCKET_TYPE_SVRCLT) closeSocket(psvrsockt[k]->psvrCltsocket[m]->client, 1);
  }

  mico_rtos_unlock_mutex(&net_mut);
  lua_pushinteger(L, 0);
  return 1;
}

//==server==
//net.on(socket,"accept",accept_cb)         //(sktclt,ip,port)
//net.on(socket,"receive",receive_cb)       //(sktclt,data)
//net.on(socket,"sent",sent_cb)             //(sktclt)
//net.on(socket,"disconnect",disconnect_cb) //(sktclt)
//==client==
//net.on(socket,"dnsfound",dnsfound_cb)     //(socket,ip)
//net.on(socket,"connect",connect_cb)       //(socket)
//net.on(socket,"receive",receive_cb)       //(socket,data)
//net.on(socket,"sent",sent_cb)             //(socket)
//net.on(socket,"disconnect",disconnect_cb) //(socket)
//================================
static int lnet_on( lua_State* L )
{
  int socketHandle = luaL_checkinteger( L, 1 );
  int type=0, k=0, m=0;
  int err = 0;
  size_t sl;
  const char *method = NULL;
  
  mico_rtos_lock_mutex(&net_mut);
  if(false == getsocketIndex(socketHandle,&type,&k,&m)) {
    err = -1;
    net_log("Socket is not valid\r\n" );
    goto exit;
  }
  if(type==SOCKET_TYPE_SVRCLT) {
    err = -2;
    net_log("CB function for server client is not possible" );
    goto exit;
  }
  
  method = luaL_checklstring( L, 2, &sl );
  if (method == NULL) {
    err = -3;
    net_log("Method string expected" );
    goto exit;
  }
  
  if ((lua_type(L, 3) == LUA_TFUNCTION) || (lua_type(L, 3) == LUA_TLIGHTFUNCTION))
    lua_pushvalue(L, 3);
  else {
    err = -4;
    net_log("Function arg expected" );
    goto exit;
  }
  
  if (type == SOCKET_TYPE_SERVER) {
    // ** server socket
    if ((psvrsockt[k]->type == TCP) && (strcmp(method,"accept") == 0) && (sl == strlen("accept"))) {
      if (psvrsockt[k]->accept_cb != LUA_NOREF)
        luaL_unref(gL,LUA_REGISTRYINDEX,psvrsockt[k]->accept_cb);
      psvrsockt[k]->accept_cb = luaL_ref(gL, LUA_REGISTRYINDEX);
    }
    else if ((strcmp(method,"receive") == 0) && (sl == strlen("receive"))) {
      if (psvrsockt[k]->receive_cb != LUA_NOREF)
        luaL_unref(gL,LUA_REGISTRYINDEX,psvrsockt[k]->receive_cb);
      psvrsockt[k]->receive_cb = luaL_ref(gL, LUA_REGISTRYINDEX);
    }
    else if ((strcmp(method,"sent") == 0) && (sl == strlen("sent"))) {
      if (psvrsockt[k]->sent_cb != LUA_NOREF)
        luaL_unref(gL,LUA_REGISTRYINDEX,psvrsockt[k]->sent_cb);
      psvrsockt[k]->sent_cb = luaL_ref(gL, LUA_REGISTRYINDEX);
    }
    else if ((strcmp(method,"disconnect") == 0) && (sl == strlen("disconnect"))) {
      if (psvrsockt[k]->disconnect_cb != LUA_NOREF)
        luaL_unref(gL,LUA_REGISTRYINDEX,psvrsockt[k]->disconnect_cb);
      psvrsockt[k]->disconnect_cb = luaL_ref(gL, LUA_REGISTRYINDEX);
    }
    else {
      lua_pop(L, 1);
      err = -5;
      net_log("Unknown method\r\n" );
      goto exit;
    }
  }
  else {
    // client socket
    if ((strcmp(method,"dnsfound") == 0) && (sl == strlen("dnsfound"))) {
      if (pcltsockt[k]->dnsfound_cb != LUA_NOREF)
        luaL_unref(gL,LUA_REGISTRYINDEX, pcltsockt[k]->dnsfound_cb);
      pcltsockt[k]->dnsfound_cb = luaL_ref(gL, LUA_REGISTRYINDEX);
    }
    else if ((pcltsockt[k]->type == TCP) && (strcmp(method,"connect") == 0) && (sl == strlen("connect"))) {
      if (pcltsockt[k]->connect_cb != LUA_NOREF)
        luaL_unref(gL,LUA_REGISTRYINDEX, pcltsockt[k]->connect_cb);
      pcltsockt[k]->connect_cb = luaL_ref(gL, LUA_REGISTRYINDEX);
    }
    else if ((strcmp(method,"receive") == 0) && (sl == strlen("receive"))) {
      if (pcltsockt[k]->receive_cb != LUA_NOREF)
        luaL_unref(gL,LUA_REGISTRYINDEX, pcltsockt[k]->receive_cb);
      pcltsockt[k]->receive_cb = luaL_ref(gL, LUA_REGISTRYINDEX);
    }
    else if ((strcmp(method,"sent") == 0) && (sl == strlen("sent"))) {
      if (pcltsockt[k]->sent_cb != LUA_NOREF)
        luaL_unref(gL,LUA_REGISTRYINDEX, pcltsockt[k]->sent_cb);
      pcltsockt[k]->sent_cb = luaL_ref(gL, LUA_REGISTRYINDEX);
    }
    else if ((strcmp(method,"disconnect") == 0) && (sl == strlen("disconnect"))) {
      if (pcltsockt[k]->disconnect_cb != LUA_NOREF)
        luaL_unref(gL,LUA_REGISTRYINDEX, pcltsockt[k]->disconnect_cb);
      pcltsockt[k]->disconnect_cb = luaL_ref(gL, LUA_REGISTRYINDEX);
    }
    else {
      lua_pop(L, 1);
      err = -5;
      net_log("Unknown method\r\n" );
      goto exit;
    }
  }
  err = 0;
  
exit:
  mico_rtos_unlock_mutex(&net_mut);
  lua_pushinteger(L, err);
  return 1;
}

//--------------------------------
static void _printState(int state)
{
  if (state == SOCKET_STATE_NOTCONNECTED) {
    net_log("Not connected\r\n" );
  }
  else if (state == SOCKET_STATE_CONNECTED) {
    net_log("Connected\r\n" );
  }
  else if (state == SOCKET_STATE_READY) {
    net_log("Ready\r\n" );
  }
  else if (state == SOCKET_STATE_LISTEN) {
    net_log("Listening\r\n" );
  }
  else if (state == SOCKET_STATE_CLOSED) {
    net_log("Closed\r\n" );
  }
  else {
    net_log("Socket is not valid\r\n" );
  }
}

//state = net.state([clientSocket])
//===================================
static int lnet_state( lua_State* L )
{
  int socketHandle = -1;
  if (lua_gettop(L) >= 1) socketHandle = luaL_checkinteger( L, 1 );
  int type=0, k=0, m=0;
  int err = 0;
  
  mico_rtos_lock_mutex(&net_mut);
  if (socketHandle >= 0) {
    if (false == getsocketIndex(socketHandle,&type,&k,&m)) {
      err = -1;
      net_log("Socket is not valid\r\n" );
      goto exit;
    }

    err = -1;
    if (type == SOCKET_TYPE_SERVER) {
      if (psvrsockt[k] != NULL) err = psvrsockt[k]->state;
    }
    else if (type == SOCKET_TYPE_SVRCLT) {
      if ((psvrsockt[k] != NULL) && (psvrsockt[k]->psvrCltsocket[m] != NULL)) {
        err = psvrsockt[k]->psvrCltsocket[m]->state;
      }
    }
    else if (type == SOCKET_TYPE_CLIENT) {
      if (pcltsockt[k] != NULL) err = pcltsockt[k]->state;
    }
    _printState(err);
  }
  else {
    int nclt = 0;
    int nsvr = 0;
    int nsvrclt = 0;
    for (k=0; k<MAX_CLT_SOCKET; k++) {
      if (pcltsockt[k] != NULL) nclt++;
    }
    for (k=0; k<MAX_SVR_SOCKET; k++) {
      if (psvrsockt[k] != NULL) {
        nsvr++;
        for (m=0; m<MAX_SVRCLT_SOCKET; m++) {
          if (psvrsockt[k]->psvrCltsocket[m] != NULL) nsvrclt++;
        }
      }
    }
    net_log("SVR: %d, SVRClt: %d, CLT: %d\r\n", nsvr, nsvrclt, nclt);
    mico_rtos_unlock_mutex(&net_mut);
    lua_pushinteger(L, nsvr);
    lua_pushinteger(L, nsvrclt);
    lua_pushinteger(L, nclt);
    return 3;
  }

exit:
  mico_rtos_unlock_mutex(&net_mut);
  lua_pushinteger(L, err);
  return 1;
}
  
//ip,port = net.getip(clientSocket)
//===================================
static int lnet_getip( lua_State* L )
{
  int socketHandle = luaL_checkinteger( L, 1 );
  int type=0, k=0, m=0;
  int err = 0;
  
  mico_rtos_lock_mutex(&net_mut);
  if (false == getsocketIndex(socketHandle,&type,&k,&m)) {
    err = -1;
    net_log("Socket is not valid\r\n" );
    goto exit;
  }    
  if (!wifiConnected) {
    net_log("WiFi not active\r\n" );
    err = -9;
    wifiConnected = false;
    goto exit;
  }
  if (type == SOCKET_TYPE_SERVER) {
    err = -2;
    net_log("Can't get IP of server socket" );
    goto exit;
  }
  if (pcltsockt[k]->state == SOCKET_STATE_CLOSED) {
    err = -3;
    net_log("Socket not connected\r\n" );
    goto exit;
  }
  
  if (type == SOCKET_TYPE_SVRCLT) {
    // * Server client socket
    if ((psvrsockt[k] != NULL) && (psvrsockt[k]->psvrCltsocket[m] != NULL)) {
      char ip_address[17];
      memset(ip_address,0x00,17);
      inet_ntoa(ip_address, psvrsockt[k]->psvrCltsocket[m]->addr.s_ip);
      lua_pushstring(L, ip_address);
      lua_pushinteger(L, psvrsockt[k]->psvrCltsocket[m]->addr.s_port);
    }
    else {
      err = -1;
      net_log("Socket is not valid\r\n" );
      goto exit;
    }
  }
  else if (type == SOCKET_TYPE_CLIENT) {
    // * Client socket
    if (pcltsockt[k] != NULL) {
      char ip_address[17];
      memset(ip_address,0x00,17);
      inet_ntoa(ip_address, pcltsockt[k]->addr.s_ip);
      lua_pushstring(L, ip_address);
      lua_pushinteger(L, pcltsockt[k]->addr.s_port);
    }    
    else {
      err = -1;
      net_log("Socket is not valid\r\n" );
      goto exit;
    }
  }
  else {
    err = -1;
    net_log("Socket is not valid\r\n" );
    goto exit;
  }
  mico_rtos_unlock_mutex(&net_mut);
  return 2;

exit:
  mico_rtos_unlock_mutex(&net_mut);
  lua_pushinteger(L, err);
  return 1;
}

//net.debug(dbg)
//===================================
static int lnet_debug( lua_State* L )
{
  int dbg = luaL_checkinteger(L, 1);
  
  if (dbg == 1) log = true;
  else log = false;
  
  lua_pushboolean(L, log);
  return 1;
}



#define MIN_OPT_LEVEL   2
#include "lrodefs.h"
const LUA_REG_TYPE net_map[] =
{
  {LSTRKEY("new"), LFUNCVAL(lnet_new)},
  {LSTRKEY("start"), LFUNCVAL(lnet_start)},
  {LSTRKEY("on"), LFUNCVAL(lnet_on)},
  {LSTRKEY("send"), LFUNCVAL(lnet_send)},
  {LSTRKEY("close"), LFUNCVAL(lnet_close)},
  {LSTRKEY("getip"), LFUNCVAL(lnet_getip)},
  {LSTRKEY("status"), LFUNCVAL(lnet_state)},
  {LSTRKEY("debug"), LFUNCVAL(lnet_debug)},
#if LUA_OPTIMIZE_MEMORY > 0
   { LSTRKEY( "TCP" ), LNUMVAL( TCP ) },
   { LSTRKEY( "UDP" ), LNUMVAL( UDP ) },
   { LSTRKEY( "SERVER" ), LNUMVAL( SOCKET_SERVER ) },
   { LSTRKEY( "CLIENT" ), LNUMVAL( SOCKET_CLIENT ) },
#endif        
  {LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_net(lua_State *L)
{
  int i=0;
  for(i=0;i<MAX_SVR_SOCKET;i++)
  {
    psvrsockt[i] = NULL;
    for(int j=0;j<MAX_SVRCLT_SOCKET;j++)
      psvrsockt[i]->psvrCltsocket[j]=NULL;
  }
  for(i=0;i<MAX_CLT_SOCKET;i++)
    pcltsockt[i] = NULL;
    
  mico_rtos_init_mutex(&net_mut);
  set_tcp_keepalive(3, 60);
  
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else  
  luaL_register( L, EXLIB_NET, net_map );
 
  MOD_REG_NUMBER( L, "TCP", TCP );
  MOD_REG_NUMBER( L, "UDP", UDP );
  MOD_REG_NUMBER( L, "SERVER", SOCKET_SERVER);
  MOD_REG_NUMBER( L, "CLIENT", SOCKET_CLIENT);
  return 1;
#endif
}
