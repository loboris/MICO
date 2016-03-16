/**
 * ftp.c
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"

#include "platform.h"
#include "system.h"
#include "mico_socket.h"
#include "mico_wlan.h"
#include "mico_system.h"
#include "SocketUtils.h"
#include "mico_rtos.h"
#include <spiffs.h>

#define INVALID_HANDLE -1
#define MAX_CLT_SOCKET 2
#define FILE_NOT_OPENED 0

extern mico_queue_t os_queue;
extern void luaWdgReload( void );
extern spiffs fs;
extern uint8_t checkFileName(int len, const char* name, char* newname, uint8_t addcurrdir, uint8_t exists);
extern uint8_t fileExists(char* name);

static mico_mutex_t  ftp_mut = NULL; // Used to synchronize the ftp thread

#define FILE_NOT_OPENED 0
static volatile spiffs_file file_fd = FILE_NOT_OPENED;

static bool log = false;
#define ftp_log(M, ...) if (log == true) printf(M, ##__VA_ARGS__)

// ftp socket actions
enum _req_actions{
  NO_ACTION = 0,
  REQ_ACTION_DISCONNECT,
  REQ_ACTION_QUIT,
  REQ_ACTION_LOGGED,
  REQ_ACTION_SENT,
  REQ_ACTION_RECEIVED,
  REQ_ACTION_LIST_RECEIVED,
  REQ_ACTION_SEND,
  REQ_ACTION_DOSEND,
  REQ_ACTION_RECV,
  REQ_ACTION_DORECV,
  REQ_ACTION_CHDIR,
  REQ_ACTION_LIST,
  REQ_ACTION_DOLIST,
};

// ftp state
#define FTP_NOT_CONNECTED  0
#define FTP_CONNECTED      1
#define FTP_DATACONNECTED  2
#define FTP_LOGGED         4
#define FTP_LISTING        8
#define FTP_RECEIVING     16
#define FTP_SENDING       32

// send type
#define SEND_OVERWRITTE  0
#define SEND_APPEND      1
#define SEND_STRING      2

// recv type
#define RECV_TOFILE      0
#define RECV_TOSTRING    1

#define MIN_RECVDATABUF_LEN 1024

//ftp Cmd socket
typedef struct {
  int socket;             // TCP socket
  struct sockaddr_t addr; //ip and port for tcp or udp use
  int logon_cb;
  int disconnect_cb;
  int sent_cb;
  int received_cb;
  int list_cb;
  uint8_t clientFlag;     // action
  uint8_t clientLastFlag; // last action
} ftpCmdSocket_t;

//ftp Data socket
typedef struct {
  int socket;             // TCP socket
  struct sockaddr_t addr; //ip and port for tcp or udp use
} ftpDataSocket_t;

#define MAX_FTP_TIMEOUT 60000 * 5
#define MAX_RECV_LEN 256

// for FTP we need 2 sockets
static ftpCmdSocket_t *ftpCmdSocket;
static ftpDataSocket_t *ftpDataSocket;

static lua_State *gL = NULL;
static char *pDomain4Dns = NULL;
static char *ftpuser = NULL;
static char *ftppass = NULL;
static char *ftpfile = NULL;
static char *ftpresponse = NULL;
static char *recvBuf = NULL;
static char *recvDataBuf = NULL;
static char *sendDataBuf = NULL;

static bool ftp_thread_is_started = false;
static uint8_t data_done = 1;
static uint8_t cmd_done = 1;
static uint8_t status = FTP_NOT_CONNECTED;
static uint16_t recvLen = 0;
static uint16_t recvDataLen = 0;
static int sendDataLen = 0;
static uint16_t max_recv_datalen = MIN_RECVDATABUF_LEN*2;
static uint16_t recvDataBufLen = MIN_RECVDATABUF_LEN;
static int file_status = 0;
static int file_size = 0;
static uint8_t list_type = 0;
static uint8_t send_type = 0;
static uint8_t recv_type = 0;
static uint8_t data_ready = 0;
static uint8_t data_get = 0;


//-------------------------------------------------------
static void _micoNotify_FTPClientConnectedHandler(int fd)
{
  if (ftpCmdSocket != NULL) {
    if (ftpCmdSocket->socket == fd) {
      // ** command socket connected
      status |= FTP_CONNECTED;
      ftp_log("[FTP cmd] Socket connected\r\n");
      return;
    }
  }
  if (ftpDataSocket != NULL) {
    if (ftpDataSocket->socket == fd) {
      // ** data socket connected
      status |= FTP_DATACONNECTED;
      ftp_log("[FTP dta] Socket connected\r\n");
      return;
    }
  }
}

//---------------------------------------
static void closeCmdSocket( void )
{
  if (ftpCmdSocket == NULL) return;

  //unref cb functions
  if (gL != NULL) {
    /*if (ftpCmdSocket->disconnect_cb != LUA_NOREF)
      luaL_unref(gL, LUA_REGISTRYINDEX, ftpCmdSocket->disconnect_cb);
    ftpCmdSocket->disconnect_cb = LUA_NOREF;*/
    
    if (ftpCmdSocket->logon_cb != LUA_NOREF)
      luaL_unref(gL, LUA_REGISTRYINDEX, ftpCmdSocket->logon_cb);
    ftpCmdSocket->logon_cb = LUA_NOREF;
    
    if (ftpCmdSocket->list_cb != LUA_NOREF)
      luaL_unref(gL, LUA_REGISTRYINDEX, ftpCmdSocket->list_cb);
    ftpCmdSocket->list_cb = LUA_NOREF;
    
    if (ftpCmdSocket->received_cb != LUA_NOREF)
      luaL_unref(gL, LUA_REGISTRYINDEX, ftpCmdSocket->received_cb);
    ftpCmdSocket->received_cb = LUA_NOREF;
    
    if (ftpCmdSocket->sent_cb != LUA_NOREF)
      luaL_unref(gL, LUA_REGISTRYINDEX, ftpCmdSocket->sent_cb);
    ftpCmdSocket->sent_cb = LUA_NOREF;
  }
  
  ftpCmdSocket->clientFlag = NO_ACTION;

  //close client socket
  close(ftpCmdSocket->socket);
  
  //free memory
  free(ftpCmdSocket);
  ftpCmdSocket = NULL;
  
  status = FTP_NOT_CONNECTED;
  
  ftp_log("[FTP cmd] Socket closed\r\n");
}

//-------------------------------
static void closeDataSocket(void)
{
  if (ftpDataSocket == NULL) return;

  //close client socket
  close(ftpDataSocket->socket);
  
  //free memory
  free(ftpDataSocket);
  ftpDataSocket = NULL;
  
  status &= ~(FTP_DATACONNECTED | FTP_RECEIVING | FTP_LISTING | FTP_SENDING);
  
  ftp_log("[FTP dta] Socket closed\r\n");
}

//--------------------------------
static int _openDataSocket( void )
{
  closeDataSocket();
  
  ftpDataSocket = (ftpDataSocket_t*)malloc(sizeof(ftpDataSocket_t));
  if (ftpDataSocket == NULL) {
    ftp_log("[FTP dta] Memory allocation failed\r\n" );
    return -1;
  }

  int skt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (skt < 0) {
    ftp_log("[FTP dta] Open data socket failed\r\n" );
    free(ftpDataSocket);
    ftpDataSocket = NULL;
    return -2;
  }
  ftpDataSocket->socket = skt;
  ftpDataSocket->addr.s_port = 0;
  ftpDataSocket->addr.s_ip = 0;

  if (recvDataBuf == NULL) {
    // create recv data buffer
    recvDataBuf = malloc(MIN_RECVDATABUF_LEN+4);
    recvDataBufLen = MIN_RECVDATABUF_LEN;
    if (recvDataBuf == NULL) {
      free(ftpDataSocket);
      ftpDataSocket = NULL;
      return -3;
    }
    memset(recvDataBuf, 0, MIN_RECVDATABUF_LEN+4);
  }
  recvDataLen = 0;
  status &= ~FTP_DATACONNECTED;
  return 0;
}

//-----------------------------
static void _ftp_deinit( void )
{
  ftp_log("[FTP ini] Deinit start\r\n" );
  closeDataSocket();
  free(ftpDataSocket);
  ftpDataSocket = NULL;
  
  closeCmdSocket();
  free(ftpCmdSocket);
  ftpCmdSocket = NULL;
  
  free(pDomain4Dns);
  pDomain4Dns=NULL;
  free(ftpuser);
  ftpuser=NULL;
  free(ftppass);
  ftppass=NULL;
  free(ftpfile);
  ftpfile=NULL;
  free(recvBuf);
  recvBuf=NULL;
  free(recvDataBuf);
  recvDataBuf=NULL;
  free(sendDataBuf);
  sendDataBuf=NULL;
  free(ftpresponse);
  ftpresponse = NULL;

  data_done = 1;
  cmd_done = 1;
  recvLen = 0;
  recvDataLen = 0;
  max_recv_datalen = MIN_RECVDATABUF_LEN*2;
  file_status = 0;
  file_size = 0;
  list_type = 0;
  send_type = 0;
  recv_type = 0;

  gL = NULL;
  status = FTP_NOT_CONNECTED;
  ftp_log("[FTP ini] Deinit end\r\n" );
}

//-----------------------------------------
static int _send(const char *data, int len)
{
  if (ftpCmdSocket == NULL) return 0;
  
  int s = 0;
  s = send(ftpCmdSocket->socket ,data, len, 0);
  if (s == len ) return 1;
  else return 0;
}

//---------------------------------------------
static int _sendData(const char *data, int len)
{
  if (ftpDataSocket == NULL) return 0;
  
  int s = 0;
  s = send(ftpDataSocket->socket ,data, len, 0);
  if (s == len ) return 1;
  else return 0;
}

//-----------------------
uint8_t _sendString(void)
{
  if (sendDataBuf == NULL) {
    ftp_log("\r[FTP dta] Buffer error\r\n");
    file_status = -2;
    return 1;
  }
  if (file_status >= file_size) return 1;

  int len = 1020;
  if ((file_size - file_status) < len) len = file_size - file_status;
  
  if (!_sendData(sendDataBuf+file_status, len)) {
    ftp_log("\r[FTP dta] Error sending data\r\n");
    file_status = -3;
    return 1;
  }
  else {
    file_status += len;
    ftp_log("\r[FTP dta] %.*f %%\r\n", 1, (float)(((float)file_status/(float)file_size)*100.0));
    if (file_status >= file_size) return 1;
    else return 0;
  }
}

// Leave only the last line in buffer
//---------------------------------
static void _checkCmdResponse(void)
{
  uint8_t n;
  char* firstLin;
  uint16_t i;
  
  do {
    n = 0;
    firstLin = NULL;
    for (i=0; i<recvLen; i++) {
      if (*(recvBuf+i) == '\n') {
        n++;
        if (n == 1) firstLin = recvBuf+i+1;
        if (n > 1) break;
      }
    }
    if (n > 1) {
      recvLen = (recvBuf+recvLen)-firstLin;
      memmove(recvBuf, firstLin, recvLen);
      *(recvBuf+recvLen) = '\0';
    }
  } while (n > 1);
}

//------------------------------------
static void _setResponse(uint16_t cmd)
{
  for (uint16_t i = (recvLen-1); i>0; i--) {
    if ((*(recvBuf+i) == '\n') || (*(recvBuf+i) == '\r')) {
      *(recvBuf+i) = '\0';
    }
    else break;
  }
  ftp_log("[FTP cmd] [%d][%s]\r\n", cmd, recvBuf);
  free(ftpresponse);
  ftpresponse = NULL;
  ftpresponse = (char*)malloc(strlen(recvBuf)+1);
  if (ftpresponse != NULL) {
    strcpy(ftpresponse, recvBuf);
  }
  cmd_done = 1;
}

// Analyze FTP server response and take some action
// Server response is in recvBuf
//--------------------------
static void response( void )
{
  if (!(status & FTP_CONNECTED)) return;
  if (recvBuf == NULL) return;

  uint16_t cmd = 0;
  
  if (*(recvBuf+3) != ' ') {
    _setResponse(cmd);
    return;
  }
  *(recvBuf+3) = '\0';
  cmd = atoi(recvBuf);
  if (cmd == 0) {
    _setResponse(cmd);
    return;
  }
  recvLen -= 4;
  memmove(recvBuf, (recvBuf+4), recvLen);
  *(recvBuf+recvLen) = '\0';
  
  
  if (cmd == 220) {
    ftp_log("[FTP cmd] Send user: %s\r\n",ftpuser);
    char tmps[100];
    sprintf(tmps,"USER %s\r\n",ftpuser);
    _send(&tmps[0], strlen(ftpuser)+7);
  }
  else if (cmd == 331) {
    ftp_log("[FTP cmd] Send pass: %s\r\n",ftppass);
    char tmps[100];
    sprintf(tmps,"PASS %s\r\n", ftppass);
    _send(&tmps[0], strlen(ftppass)+7);
  }
  else if (cmd == 230) {
    if (ftppass != NULL) {
      free(ftppass);
      ftppass = NULL;
    }
    if (ftpuser != NULL) {
      free(ftpuser);
      ftpuser = NULL;
    }
    _send("TYPE I\r\n", 8);

    ftp_log("[FTP cmd] Login OK.\r\n");
    status |= FTP_LOGGED;
    ftpCmdSocket->clientFlag = REQ_ACTION_LOGGED;
  }
  else if (cmd == 530) {
    ftp_log("[FTP cmd] Login authentication failed\r\n");
    ftpCmdSocket->clientFlag = REQ_ACTION_QUIT;
  }
  else if (cmd == 221) {
    ftp_log("[FTP cmd] Request Logout\r\n");
    ftpCmdSocket->clientFlag = REQ_ACTION_DISCONNECT;
  }
  else if (cmd == 226) {
    // Closing data connection
    _setResponse(cmd);
  }
  else if (cmd == 250) {
    // Requested file action okay, completed
    _setResponse(cmd);
  }
  else if (cmd == 257) {
    // Pathname
    _setResponse(cmd);
    if (ftpresponse != NULL) {
      uint16_t i;
      for (i = 0; i<strlen(ftpresponse); i++) {
        if (*(recvBuf+i) == '"') break;
      }
      if (i<strlen(ftpresponse)) {
        memmove(ftpresponse, ftpresponse+i+1, strlen(ftpresponse)-i);
        for (i=strlen(ftpresponse)-1; i>0; i--) {
          if (*(ftpresponse+i) == '"') *(ftpresponse+i) = '\0';
        }
      }
    }
  }
  else if ((cmd == 150) || (cmd == 125)) {
    // mark: Accepted data connection
    if ((ftpfile != NULL) && (ftpDataSocket != NULL) &&
        (ftpCmdSocket->clientLastFlag == REQ_ACTION_DOSEND)) {
      status |= FTP_SENDING;
      if ((send_type & SEND_STRING)) {
        ftp_log("[FTP dta] Sending string to %s (%d)\r\n", ftpfile, file_size);
      }
      else {
        ftp_log("[FTP dta] Sending file %s (%d)\r\n", ftpfile, file_size);
      }
    }
    else {
      //if ((status & FTP_RECEIVING) == 0) _setResponse(cmd);
      _setResponse(cmd);
    }
  }
  else if (cmd == 227) {
    // entering passive mod
    char *tStr = strtok(recvBuf, "(,");
    uint8_t array_pasv[6];
    for ( int i = 0; i < 6; i++) {
      tStr = strtok(NULL, "(,");
      if (tStr != NULL) {
        array_pasv[i] = atoi(tStr);
      }
      else {
        array_pasv[i] = 0;
      }
    }
    uint32_t dataPort, dataServ;
    dataPort = (uint32_t)((array_pasv[4] << 8) + (array_pasv[5] & 255));
    dataServ = (uint32_t)((array_pasv[0] << 24) + (array_pasv[1] << 16) + (array_pasv[2] << 8) + (array_pasv[3] & 255));

    if ((dataServ != 0) && (dataPort != 0)) {
      // connect data socket
      if (_openDataSocket() >= 0) {
        ftpDataSocket->addr.s_ip = dataServ;
        ftpDataSocket->addr.s_port = dataPort;

        char ip[17]; memset(ip, 0x00, 17);
        inet_ntoa(ip, ftpDataSocket->addr.s_ip);
        ftp_log("[FTP dta] Opening data connection to: %s:%d\r\n", ip, ftpDataSocket->addr.s_port);

        struct sockaddr_t *paddr = &(ftpDataSocket->addr);
        int slen = sizeof(ftpDataSocket->addr);
      
        //_micoNotify will be called if connected
        int stat = connect(ftpDataSocket->socket, paddr, slen);
        if (stat < 0) {
          ftp_log("[FTP dta] Data connection error: %d\r\n", stat);
          ftpCmdSocket->clientFlag = NO_ACTION;
          ftpCmdSocket->clientLastFlag = NO_ACTION;
          data_done = 1;
          file_status = -9;
          closeDataSocket();
        }
        else {
          // Data socket connected, initialize action
          if (ftpCmdSocket->clientLastFlag == REQ_ACTION_LIST) {
            ftpCmdSocket->clientFlag = REQ_ACTION_DOLIST;
            ftpCmdSocket->clientLastFlag = ftpCmdSocket->clientFlag;
          }
          else if (ftpCmdSocket->clientLastFlag == REQ_ACTION_RECV) {
            ftpCmdSocket->clientFlag = REQ_ACTION_DORECV;
            ftpCmdSocket->clientLastFlag = ftpCmdSocket->clientFlag;
          }
          else if (ftpCmdSocket->clientLastFlag == REQ_ACTION_SEND) {
            ftpCmdSocket->clientFlag = REQ_ACTION_DOSEND;
            ftpCmdSocket->clientLastFlag = ftpCmdSocket->clientFlag;
          }
        }
      }
    }
    else {
      ftp_log("[FTP dta] Wrong pasv address:port received!\r\n");
      ftpCmdSocket->clientFlag = REQ_ACTION_DISCONNECT;
    }
  }
  else _setResponse(cmd);
}

// ======= FTP thread handler =========
static void _thread_ftp(void*inContext)
{
  (void)inContext;

  fd_set readset, wrset;
  struct timeval_t t_val;
  int k;
  uint32_t timeout = 0;
  uint8_t recv_tmo = 0;
  queue_msg_t msg;

  t_val.tv_sec = 0;
  t_val.tv_usec = 5000;

  mico_rtos_lock_mutex(&ftp_mut);
  timeout = mico_get_time();
  ftp_log("\r\n[FTP trd] FTP THREAD STARTED\r\n");
  
  // *** First we have to get IP address and connect the cmd socket
  char pIPstr[16]={0};
  int err;
  k = 3;
  do {
    err = gethostbyname((char *)pDomain4Dns, (uint8_t *)pIPstr, 16);
    if (err != kNoErr) {
      k--;
      mico_thread_msleep(10);
    }
  }while ((err != kNoErr) && (k > 0));
  
  if ((err == kNoErr) && (ftpCmdSocket != NULL)) {
    ftpCmdSocket->addr.s_ip = inet_addr(pIPstr);
    ftpCmdSocket->clientFlag = NO_ACTION;

    free(pDomain4Dns);
    pDomain4Dns=NULL;

    // Connect socket!
    char ip[17];
    memset(ip, 0x00, 17);
    inet_ntoa(ip, ftpCmdSocket->addr.s_ip);
    ftp_log("[FTP cmd] Got IP: %s, connecting...\r\n", ip);

    struct sockaddr_t *paddr = &(ftpCmdSocket->addr);
    int slen = sizeof(ftpCmdSocket->addr);
    // _micoNotify will be called if connected
    connect(ftpCmdSocket->socket, paddr, slen);
  }
  else {
    ftp_log("[FTP dns] Get IP error\r\n");
    goto terminate;
  }

  if (recvBuf == NULL) {
    recvBuf = malloc(MAX_RECV_LEN+4);
    memset(recvBuf, 0, MAX_RECV_LEN+4);
  }
  recvLen = 0;

  // ===========================================================================
  // Main Thread loop
  while (1) {
    mico_rtos_unlock_mutex(&ftp_mut);
    mico_thread_msleep(5);
    mico_rtos_lock_mutex(&ftp_mut);
        
    if (ftpCmdSocket == NULL) goto terminate;

    if ( ((mico_get_time() - timeout) > MAX_FTP_TIMEOUT) ||
         (((mico_get_time() - timeout) > 8000) && (!(status & FTP_LOGGED))) ) {
      // ** TIMEOUT **
      ftp_log("[FTP trd] Timeout\r\n");
      timeout = mico_get_time();
      
      if ((status & FTP_LOGGED))
        ftpCmdSocket->clientFlag = REQ_ACTION_QUIT;
      else
        ftpCmdSocket->clientFlag = REQ_ACTION_DISCONNECT;
    }

    // ========== stage #1, Check cmd socket action requests ===================
    
    //REQ_ACTION_DISCONNECT
    if (ftpCmdSocket->clientFlag == REQ_ACTION_DISCONNECT) {
      ftpCmdSocket->clientFlag = NO_ACTION;
      
      closeDataSocket();
      
      ftp_log("[FTP cmd] Socket disconnected\r\n");
      if (ftpCmdSocket->disconnect_cb != LUA_NOREF) {
        msg.L = gL;
        msg.source = onFTP | needUNREF;
        msg.para1 = 0;
        msg.para3 = NULL;
        msg.para4 = NULL;
        msg.para2 = ftpCmdSocket->disconnect_cb;
        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
      closeCmdSocket();
      continue;
    }
    //REQ_ACTION_QUIT
    if (ftpCmdSocket->clientFlag == REQ_ACTION_QUIT) {
      if ((status & FTP_LOGGED)) {
        ftpCmdSocket->clientFlag = NO_ACTION;
        ftp_log("[FTP cmd] Quit command\r\n");
        if (_send("QUIT\r\n", 6) <= 0) {
          ftpCmdSocket->clientFlag = REQ_ACTION_DISCONNECT;
        }
      }
      else ftpCmdSocket->clientFlag = REQ_ACTION_DISCONNECT;
      continue;
    }

    if (!(status & FTP_CONNECTED)) continue;
    //--------------------------------------

    //REQ_ACTION_LIST, REQ_ACTION_RECV, REQ_ACTION_SEND
    else if ((ftpCmdSocket->clientFlag == REQ_ACTION_LIST) ||
             (ftpCmdSocket->clientFlag == REQ_ACTION_RECV) ||
             (ftpCmdSocket->clientFlag == REQ_ACTION_SEND)) {
      ftpCmdSocket->clientLastFlag = ftpCmdSocket->clientFlag;
      ftpCmdSocket->clientFlag = NO_ACTION;

      _send("PASV\r\n", 6);
    }
    //REQ_ACTION_DOLIST
    else if (ftpCmdSocket->clientFlag == REQ_ACTION_DOLIST) {
      ftpCmdSocket->clientFlag = NO_ACTION;

      int res;
      if (ftpfile != NULL) {
        char tmps[100];
        if (list_type == 1) sprintf(tmps,"NLST %s\r\n", ftpfile);
        else sprintf(tmps,"LIST %s\r\n", ftpfile);
        res = _send(&tmps[0], strlen(ftpfile)+7);
        free(ftpfile);
        ftpfile = NULL;
      }
      else {
        if (list_type == 1) res = _send("NLST\r\n", 6);
        else res = _send("LIST\r\n", 6);
      }
      if (res > 0) status |= FTP_LISTING;
      else {
        ftp_log("[FTP cmd] LIST command failed.\r\n");
      }
    }
    //REQ_ACTION_DORECV
    else if (ftpCmdSocket->clientFlag == REQ_ACTION_DORECV) {
      ftpCmdSocket->clientFlag = NO_ACTION;

      char tmps[100];
      sprintf(tmps,"RETR %s\r\n", ftpfile);
      int res = _send(&tmps[0], strlen(ftpfile)+7);
      if (res > 0) {
        status |= FTP_RECEIVING;
        file_status = 0;
      }
      else {
        ftp_log("[FTP cmd] RETR command failed.\r\n");
        file_status = -4;
      }
    }
    //REQ_ACTION_DOSEND
    else if (ftpCmdSocket->clientFlag == REQ_ACTION_DOSEND) {
      ftpCmdSocket->clientFlag = NO_ACTION;

      char tmps[100];
      if ((send_type & SEND_APPEND)) sprintf(tmps,"APPE %s\r\n", ftpfile);
      else sprintf(tmps,"STOR %s\r\n", ftpfile);
      int res = _send(&tmps[0], strlen(ftpfile)+7);
      if (res > 0) {
        file_status = 0;
      }
      else {
        ftp_log("[FTP cmd] STOR/APPE command failed.\r\n");
        file_status = -4;
      }
    }
    //REQ_ACTION_CHDIR
    else if (ftpCmdSocket->clientFlag == REQ_ACTION_CHDIR) {
      ftpCmdSocket->clientFlag = NO_ACTION;

      if (ftpfile != NULL) {
        char tmps[100];
        sprintf(tmps,"CWD %s\r\n", ftpfile);
        _send(&tmps[0], strlen(ftpfile)+6);
        free(ftpfile);
        ftpfile = NULL;
      }
      else {
        _send("PWD\r\n", 5);
      }
    }
    //REQ_ACTION_LOGGED
    else if (ftpCmdSocket->clientFlag == REQ_ACTION_LOGGED) {
      ftpCmdSocket->clientFlag=NO_ACTION;
      if (ftpCmdSocket->logon_cb != LUA_NOREF) {
        msg.L = gL;
        msg.source = onFTP;
        msg.para1 = 1;
        msg.para3 = NULL;
        msg.para4 = NULL;
        msg.para2 = ftpCmdSocket->logon_cb;
        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
    }
    //REQ_ACTION_LIST_RECEIVED
    else if (ftpCmdSocket->clientFlag == REQ_ACTION_LIST_RECEIVED) {
      ftpCmdSocket->clientFlag=NO_ACTION;
      if (ftpCmdSocket->list_cb != LUA_NOREF) {
        msg.L = gL;
        msg.source = onFTP;
        msg.para1 = recvDataLen;
        msg.para2 = ftpCmdSocket->list_cb;
        msg.para3 = (uint8_t*)malloc(recvDataLen+4);
        msg.para4 = NULL;
        if (msg.para3 != NULL) memcpy((char*)msg.para3, recvDataBuf, recvDataLen+1);

        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
    }
    //REQ_ACTION_RECEIVED
    else if (ftpCmdSocket->clientFlag == REQ_ACTION_RECEIVED) {
      ftpCmdSocket->clientFlag=NO_ACTION;
      if (ftpCmdSocket->received_cb != LUA_NOREF) {
        msg.L = gL;
        msg.source = onFTP;
        msg.para1 = file_status;
        msg.para3 = NULL;
        msg.para4 = NULL;
        if ((recv_type == RECV_TOSTRING) && (file_status > 0)) {
          msg.para3 = (uint8_t*)malloc(recvDataLen+4);
          if (msg.para3 != NULL) memcpy((char*)msg.para3, recvDataBuf, recvDataLen+1);
        }
        msg.para2 = ftpCmdSocket->received_cb;

        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
    }
    //REQ_ACTION_SENT
    else if (ftpCmdSocket->clientFlag == REQ_ACTION_SENT) {
      ftpCmdSocket->clientFlag=NO_ACTION;
      if (ftpCmdSocket->sent_cb != LUA_NOREF) {
        msg.L = gL;
        msg.source = onFTP;
        msg.para1 = file_status;
        msg.para3 = NULL;
        msg.para4 = NULL;
        msg.para2 = ftpCmdSocket->sent_cb;

        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
    }

    // ========== stage 2, check data send =====================================
    if ((ftpDataSocket != NULL) && ((status & FTP_SENDING))) {
      FD_ZERO(&wrset);
      FD_SET(ftpDataSocket->socket, &wrset);
      // check socket state
      t_val.tv_sec = 0;
      t_val.tv_usec = 5000;
      mico_rtos_unlock_mutex(&ftp_mut);
      select(ftpDataSocket->socket+1, NULL, &wrset, NULL, &t_val);
      mico_rtos_lock_mutex(&ftp_mut);
      
      if (FD_ISSET(ftpDataSocket->socket, &wrset)) {
        int err;
        if ((send_type & SEND_STRING)) { // sending from string
          err = _sendString();
        }
        else { // sending from file
          if (data_get == 1) {
            data_get = 2; // get data from file from main thread
            mico_rtos_unlock_mutex(&ftp_mut);
            mico_thread_msleep(5);
            mico_rtos_lock_mutex(&ftp_mut);
            
            if (data_get == 1) {
              err = 0;
              // got data to send
              if (!_sendData(recvDataBuf, sendDataLen)) {
                ftp_log("[FTP dta] Error sending data\r\n");
                file_status = -3;
                data_get = 3; // abort
              }
            }
            else err = -1;
          }
          else err = -1;
        }
        
        if (err != 0) {
          closeDataSocket();
          data_done = 1;
          status &= ~FTP_SENDING;
          ftpCmdSocket->clientFlag = REQ_ACTION_SENT;
        }
        continue;
      }
    }

    // ========== stage 3, check receive&disconnect ============================
    FD_ZERO(&readset);
    // ** select sockets to monitor for receive or disconnect
    int maxfd = ftpCmdSocket->socket;
    FD_SET(ftpCmdSocket->socket, &readset);
    if ( (ftpDataSocket != NULL) && ((status & FTP_DATACONNECTED)) ) {
      // check also the data socket
      if (ftpDataSocket->socket > maxfd) maxfd = ftpDataSocket->socket;
      FD_SET(ftpDataSocket->socket, &readset);
    }
    // ** check sockets state
    t_val.tv_sec = 0;
    t_val.tv_usec = 5000;
    mico_rtos_unlock_mutex(&ftp_mut);
    select(maxfd+1, &readset, NULL, NULL, &t_val);
    mico_rtos_lock_mutex(&ftp_mut);

    // ** Check COMMAND socket
    if (FD_ISSET(ftpCmdSocket->socket, &readset)) {
      // read received data to buffer
      int rcv_len;
      if ((MAX_RECV_LEN-recvLen) > 1) {
        rcv_len = recv(ftpCmdSocket->socket, (recvBuf+recvLen), MAX_RECV_LEN-recvLen-1, 0);
      }
      else { // buffer full, ignore received bytes
        char tmpb[16];
        rcv_len = recv(ftpCmdSocket->socket, &tmpb[0], 16, 0);
      }
      
      if (rcv_len <= 0) { // failed
        ftp_log("\r\n[FTP cmd] Disconnect!\r\n");
        ftpCmdSocket->clientFlag = REQ_ACTION_DISCONNECT;
        continue;
      }
      
      recvLen += rcv_len;
      _checkCmdResponse(); // leave only the last line in buffer
      timeout = mico_get_time();
      recv_tmo = 0;
    }

    // ** Check DATA socket
    if ( (ftpDataSocket != NULL) && ((status & FTP_DATACONNECTED)) ) {
      if (FD_ISSET(ftpDataSocket->socket, &readset)) {
        // read received data to buffer
        // recvDataLen contains the total length of received data
        int rcv_len = -1;
        if (recvDataBuf != NULL) {
          rcv_len = recv(ftpDataSocket->socket, (recvDataBuf+recvDataLen), recvDataBufLen-recvDataLen-1, 0);
        }

        if (rcv_len <= 0) { // failed
          // == Check if Data socket was receiving ==
          if (!(status & (FTP_RECEIVING | FTP_LISTING))) {
            // Finish all operattions on data socket
            data_done = 1;
            file_status = -9;
            // Close data socket
            closeDataSocket();
          }
          continue;
        }
        else {
          if ((status & FTP_RECEIVING) && (recv_type == RECV_TOFILE)) {
            // === write received data to file ===
            recvDataLen = rcv_len;
            data_ready = 1;
            uint32_t tmo = mico_get_time();
            while (data_ready > 0) { // wait until block is written to fs in main thread
              if ((mico_get_time() - tmo) > 1000) break;
              mico_rtos_unlock_mutex(&ftp_mut);
              mico_thread_msleep(5);
              mico_rtos_lock_mutex(&ftp_mut);
            }
            recvDataLen = 0;
          }
          else if ((status & FTP_LISTING) || ((status & FTP_RECEIVING) && (recv_type == RECV_TOSTRING))) {
            // receiving to string (buffer)
            if ((recvDataLen + rcv_len) < (recvDataBufLen-128)) recvDataLen += rcv_len;
            else {
              // make room for more data
              if ((recvDataBufLen+256) <= max_recv_datalen) {
                recvDataBuf = realloc(recvDataBuf, (recvDataBufLen+256+4));
                if (recvDataBuf == NULL) {
                  // create recv data buffer
                  recvDataBuf = malloc(MIN_RECVDATABUF_LEN+4);
                  recvDataBufLen = MIN_RECVDATABUF_LEN;
                  memset(recvDataBuf, 0, MIN_RECVDATABUF_LEN+4);
                  ftp_log("\r\n[FTP cmd] Data buffer reallocation error!\r\n");
                  data_done = 1;
                  file_status = -9;
                  recvDataLen = 0;
                  // Close data socket
                  closeDataSocket();
                }
                else {
                  recvDataBufLen += 256;
                  recvDataLen += rcv_len;
                }
              }
              else recvDataLen = recvDataBufLen-16;
            }
            if (recvDataBuf != NULL) *(recvDataBuf + recvDataLen) = '\0';
          }
          recv_tmo = 0;
        }
        timeout = mico_get_time();
      }
    }

    // ===== nothing received ==================================================
    recv_tmo++;
    if (recv_tmo < 10) continue;
    
    recv_tmo = 0;

    // == Check if Data socket was receiving ==
    if ((status & (FTP_RECEIVING | FTP_LISTING))) {
      // Finish all operattions on data socket
      if ((status & FTP_RECEIVING) && (recv_type == RECV_TOFILE)) {
        data_ready = 2;
        uint32_t tmo = mico_get_time();
        while (data_ready > 0) { // inform the main thread of transfer end
          if ((mico_get_time() - tmo) > 1000) break;
          mico_rtos_unlock_mutex(&ftp_mut);
          mico_thread_msleep(5);
          mico_rtos_lock_mutex(&ftp_mut);
        }
        recvDataLen = 0;
      }
      else {
        if ((status & FTP_RECEIVING) && (recv_type == RECV_TOSTRING)) {
          file_status = recvDataLen;
        }
        ftp_log("[FTP dta] Data received (%d)\r\n", recvDataLen);
      }
      
      if ((status & FTP_LISTING)) {
        status &= ~FTP_LISTING;
        ftpCmdSocket->clientFlag = REQ_ACTION_LIST_RECEIVED;
      }
      if ((status & FTP_RECEIVING)) {
        status &= ~FTP_RECEIVING;
        ftpCmdSocket->clientFlag = REQ_ACTION_RECEIVED;
      }

      // Close data socket
      closeDataSocket();
      data_done = 1;
    }

    // == Check if something was received from Command socket ==
    if (recvLen > 0) {
      // == Analize response ===
      response();
      recvLen = 0;
      memset(recvBuf, 0, MAX_RECV_LEN);
    }
  } // while

terminate:
  _ftp_deinit();

  ftp_thread_is_started = false;
  mico_rtos_unlock_mutex(&ftp_mut);
  ftp_log("\r\n[FTP trd] FTP THREAD TERMINATED\r\n");

  mico_rtos_delete_thread( NULL );
}
// ==================================

//-----------------------------
int _stopThread( uint8_t wait )
{
  int res = 0;
  
  mico_rtos_lock_mutex(&ftp_mut);

  if ( (ftp_thread_is_started) && (ftpCmdSocket != NULL) ) {
    ftpCmdSocket->clientFlag = REQ_ACTION_QUIT;
    
    if (wait == 1) {
      // wait max 10 sec for disconnect
      uint32_t tmo = mico_get_time();
      while (ftp_thread_is_started) {
        if ((mico_get_time() - tmo) > 10000) break;
        mico_rtos_unlock_mutex(&ftp_mut);
        mico_thread_msleep(100);
        mico_rtos_lock_mutex(&ftp_mut);
        luaWdgReload();
      }
      if (ftp_thread_is_started) res = -1;
    }
  }

  mico_rtos_unlock_mutex(&ftp_mut);
  return res;
}

//stat = ftp.new(serv,port,user,pass [,buflen])
//=================================
static int lftp_new( lua_State* L )
{

  size_t dlen=0, ulen=0, plen=0;
  int port, socketHandle, err;
  uint32_t opt;
  const char *user;
  const char *pass;
  
  if (ftp_mut == NULL) {
    mico_rtos_init_mutex(&ftp_mut);
  }

  err = _stopThread(1);
  if (err == -1) {
    ftp_log("[FTP usr] Thread still running!\r\n" );
    lua_pushinteger(L, -9);
    return 1;
  }
  
  mico_rtos_lock_mutex(&ftp_mut);

  _ftp_deinit();
  
  const char *domain = luaL_checklstring( L, 1, &dlen );
  if (dlen>128 || domain == NULL) {
    ftp_log("[FTP usr] Domain needed\r\n" );
    lua_pushinteger(L, -1);
    goto exit;
  }
  port = luaL_checkinteger( L, 2 );
  user = luaL_checklstring( L, 3, &ulen );
  if (ulen>128 || user == NULL) {
    ftp_log("[FTP usr] User needed\r\n" );
    lua_pushinteger(L, -2);
    goto exit;
  }
  pass = luaL_checklstring( L, 4, &plen );
  if (plen>128 || pass == NULL)  {
    ftp_log("[FTP usr] Pass needed\r\n" );
    lua_pushinteger(L, -3);
    goto exit;
  }

  max_recv_datalen = MIN_RECVDATABUF_LEN*2;
  if (lua_gettop(L) >= 5) {
    int maxdl = luaL_checkinteger(L, 5);
    if ((maxdl >= MIN_RECVDATABUF_LEN) && (maxdl <= (MIN_RECVDATABUF_LEN*10))) max_recv_datalen = (uint16_t)maxdl;
  }

  // allocate buffers
  pDomain4Dns=(char*)malloc(dlen+1);
  ftpuser=(char*)malloc(ulen+1);
  ftppass=(char*)malloc(plen+1);
  if ((pDomain4Dns==NULL) || (ftpuser==NULL) || (ftppass==NULL)) {
    _ftp_deinit();    
    ftp_log("[FTP usr] Memory allocation failed\r\n" );
    lua_pushinteger(L, -4);
    goto exit;
  }
  strcpy(pDomain4Dns,domain);
  strcpy(ftpuser,user);
  strcpy(ftppass,pass);

  socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socketHandle < 0) {
    _ftp_deinit();    
    ftp_log("[FTP usr] Open CMD socket failed\r\n" );
    lua_pushinteger(L, -5);
    goto exit;
  }
  
  ftpCmdSocket = NULL;
  ftpDataSocket = NULL;
  
  ftpCmdSocket = (ftpCmdSocket_t*)malloc(sizeof(ftpCmdSocket_t));
  if (ftpCmdSocket == NULL) {
    _ftp_deinit();    
    ftp_log("[FTP usr] Memory allocation failed\r\n" );
    lua_pushinteger(L, -6);
    goto exit;
  }

  ftpCmdSocket->socket = socketHandle;
  ftpCmdSocket->disconnect_cb = LUA_NOREF;
  ftpCmdSocket->logon_cb = LUA_NOREF;
  ftpCmdSocket->list_cb = LUA_NOREF;
  ftpCmdSocket->received_cb = LUA_NOREF;
  ftpCmdSocket->sent_cb = LUA_NOREF;
  ftpCmdSocket->clientFlag = NO_ACTION;
  ftpCmdSocket->clientLastFlag = NO_ACTION;
  ftpCmdSocket->addr.s_port = port;
  
  status = FTP_NOT_CONNECTED;
  opt=0;
  err = setsockopt(socketHandle,IPPROTO_IP,SO_BLOCKMODE,&opt,4); // non block mode
  if (err < 0) {
    _ftp_deinit();    
    ftp_log("[FTP usr] Set socket options failed\r\n" );
    lua_pushinteger(L, -7);
    goto exit;
  }

  gL = L;
  ftp_log("[FTP usr] FTP client configured.\r\n" );
  lua_pushinteger(L, 0);

exit:
  mico_rtos_unlock_mutex(&ftp_mut);
  return 1;
}

//stat = ftp.start()
//===================================
static int lftp_start( lua_State* L )
{
  uint32_t tmo;
  LinkStatusTypeDef wifi_link;
  
  int err = micoWlanGetLinkStatus( &wifi_link );

  if ( wifi_link.is_connected == false ) {
    ftp_log("[FTP usr] WiFi NOT CONNECTED!\r\n" );
    lua_pushinteger(L, -1);
    return 1;
  }
  
  mico_rtos_lock_mutex(&ftp_mut);

  if ( (gL == NULL) || (ftpCmdSocket == NULL) ) {
    ftp_log("[FTP usr] Execute ftp.new first!\r\n" );
    lua_pushinteger(L, -2);
    goto exit;
  }
  
  if (ftp_thread_is_started) {
    ftp_log("[FTP usr] Thread already started, execute net.stop() first!\r\n" );
    lua_pushinteger(L, -3);
    goto exit;
  }
  
  mico_system_notify_register( mico_notify_TCP_CLIENT_CONNECTED, (void *)_micoNotify_FTPClientConnectedHandler, NULL );

  // all setup, start the ftp thread
  if (!ftp_thread_is_started) {
    if (mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY-1, "Ftp_Thread", _thread_ftp, 1024, NULL) != kNoErr) {
      _ftp_deinit();    
      ftp_log("[FTP usr] Create thread failed\r\n" );
      lua_pushinteger(L, -4);
      goto exit;
    }
    else ftp_thread_is_started = true;
  } 

  if (ftpCmdSocket->logon_cb != LUA_NOREF) {
    lua_pushinteger(L, 0);
    goto exit;
  }
  
  // wait max 10 sec for login
  tmo = mico_get_time();
  while ( (ftp_thread_is_started) && !(status & FTP_LOGGED) ) {
    if ((mico_get_time() - tmo) > 10000) break;
    mico_rtos_unlock_mutex(&ftp_mut);
    mico_thread_msleep(100);
    mico_rtos_lock_mutex(&ftp_mut);
    luaWdgReload();
  }
  if (!(status & FTP_LOGGED)) lua_pushinteger(L, -4);
  else lua_pushinteger(L, 0);

exit:
  mico_rtos_unlock_mutex(&ftp_mut);
  return 1;
}

//ftp.stop()
//===================================
static int lftp_stop( lua_State* L )
{
  int res = 1;
  
  if (ftpCmdSocket->disconnect_cb != LUA_NOREF) res = _stopThread(0);
  else res = _stopThread(1);
  lua_pushinteger(L, res);
  return 1;
}

//ftp.debug(dbg)
//===================================
static int lftp_debug( lua_State* L )
{
  int dbg = luaL_checkinteger(L, 1);
  
  if (dbg == 1) log = true;
  else log = false;
  
  return 0;
}

//ftp.on("onEvent",function_cb)
//================================
static int lftp_on( lua_State* L )
{
  const char *method;
  uint8_t reg_func = 0;
  
  mico_rtos_lock_mutex(&ftp_mut);
  
  if ( (gL == NULL) || (ftpCmdSocket == NULL) ) {
    ftp_log("[FTP usr] Execute ftp.new first!" );
    lua_pushinteger(L, -1);
    goto exit;
  }

  size_t sl;
  method = luaL_checklstring( L, 1, &sl );
  if (method == NULL) {
    ftp_log("[FTP usr] Method string needed" );
    lua_pushinteger(L, -3);
    goto exit;
  }
  
  if ((lua_type(L, 2) == LUA_TFUNCTION) || (lua_type(L, 2) == LUA_TLIGHTFUNCTION)) {
    lua_pushvalue(L, 2);
    reg_func = 1;
  }
  
  if ((strcmp(method,"login") == 0) && (sl == strlen("login"))) {
    if (ftpCmdSocket->logon_cb != LUA_NOREF)
      luaL_unref(gL,LUA_REGISTRYINDEX, ftpCmdSocket->logon_cb);
    if (reg_func == 1) ftpCmdSocket->logon_cb = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else if ((strcmp(method,"disconnect") == 0) && (sl == strlen("disconnect"))) {
    if (ftpCmdSocket->disconnect_cb != LUA_NOREF)
      luaL_unref(gL,LUA_REGISTRYINDEX, ftpCmdSocket->disconnect_cb);
    if (reg_func == 1) ftpCmdSocket->disconnect_cb = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else if ((strcmp(method,"receive") == 0) && (sl == strlen("receive"))) {
    if (ftpCmdSocket->received_cb != LUA_NOREF)
      luaL_unref(gL,LUA_REGISTRYINDEX, ftpCmdSocket->received_cb);
    if (reg_func == 1) ftpCmdSocket->received_cb = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else if ((strcmp(method,"send") == 0) && (sl == strlen("send"))) {
    if (ftpCmdSocket->sent_cb != LUA_NOREF)
      luaL_unref(gL,LUA_REGISTRYINDEX, ftpCmdSocket->sent_cb);
    if (reg_func == 1) ftpCmdSocket->sent_cb = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else if ((strcmp(method,"list") == 0) && (sl == strlen("list"))) {
    if (ftpCmdSocket->list_cb != LUA_NOREF)
      luaL_unref(gL,LUA_REGISTRYINDEX, ftpCmdSocket->list_cb);
    if (reg_func == 1) ftpCmdSocket->list_cb = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else {
    ftp_log("[FTP usr] Unknown CB function: %s", method);
    lua_pushinteger(L, -5);
  }

  lua_pushinteger(L, 0);

exit:
  mico_rtos_unlock_mutex(&ftp_mut);
  return 1;
}

//-----------------------------------------------
static int _getFName(lua_State* L, uint8_t index)
{
  free(ftpfile);
  ftpfile = NULL;
  if (lua_gettop(L) < index) return -3; // no file arg exists
  
  size_t len = 0;
  const char *fname = luaL_checklstring( L, index, &len );
  if ((len < 1) || (len >= SPIFFS_OBJ_NAME_LEN) || (fname == NULL)) {
    ftp_log("[FTP fil] Bad file name\r\n" );
    return -1;
  }

  ftpfile = (char*)malloc(SPIFFS_OBJ_NAME_LEN);
  if (ftpfile == NULL) {
    ftp_log("[FTP fil] Memory allocation failed\r\n" );
    return -2;
  }
  strcpy(ftpfile, fname);
  return 0;
}

// open local file for sending or receiving
//-------------------------
int _openFile(uint8_t mode)
{
  
  // open the file
  if (ftpfile == NULL) {
    ftp_log("[FTP fil] File name not allocated\r\n");
    return -1;
  }

  if (FILE_NOT_OPENED != file_fd) {
    SPIFFS_close(&fs, file_fd);
    file_fd = FILE_NOT_OPENED;
  }

  uint8_t check = 1;
  int mde = SPIFFS_RDONLY;
  char fullname[SPIFFS_OBJ_NAME_LEN] = {0};
  uint8_t res = 0;
  
  if (mode == 1) {
    check = 0;
    mde = SPIFFS_WRONLY|SPIFFS_CREAT|SPIFFS_TRUNC;
  }
  res = checkFileName(strlen(ftpfile), ftpfile, fullname, 1, check);
  if ((mode == 0) && (res != 2)) {
      ftp_log("[FTP fil] File '%s' not found\r\n", fullname);
      return -2;
  }
  if (res == 0) {
    ftp_log("[FTP fil] Bad file name: '%s'\r\n", fullname);
    return -2;
  }
  
  if ((mode == 1) && (fileExists(fullname))) {
    ftp_log("[FTP fil] Deleting existing file '%s'\r\n", fullname);
    SPIFFS_remove(&fs, fullname);
  }

  file_fd = SPIFFS_open(&fs, fullname, mde, 0);
  if (file_fd <= FILE_NOT_OPENED) {
    ftp_log("[FTP fil] Error opening file '%s'\r\n", fullname);
    return -3;
  }

  ftp_log("[FTP fil] Opened local file '%s' for ", fullname);
  if (mode == 0) {
    ftp_log("reading\r\n"); 
    // reading, get file size
    spiffs_stat s;
    SPIFFS_fstat(&fs, file_fd, &s);
    ftp_log("[FTP fil] File size: %d\r\n", s.size);
    if (s.size == 0) {
      if (FILE_NOT_OPENED != file_fd) SPIFFS_close(&fs, file_fd);
      return -4;
    }
    else return s.size;
  }
  else {
    ftp_log("writing\r\n"); 
  }
  return 0;
}

//-------------------------------
uint8_t _waitDataSocketFree(void)
{
  uint32_t tmo = mico_get_time();
  while (ftpDataSocket != NULL) {
    if ((mico_get_time() - tmo) > 5000) break;
    mico_rtos_unlock_mutex(&ftp_mut);
    mico_thread_msleep(50);
    mico_rtos_lock_mutex(&ftp_mut);
    luaWdgReload();
  }
  if (ftpDataSocket != NULL) {
    closeDataSocket();
    ftp_log("[FTP usr] Data socket not free!\r\n" );
    return 0;
  }
  return 1;
}

//ftp.list(ltype, otype [,remotedir])
//===================================
static int lftp_list( lua_State* L )
{
  int err = 0;
  uint16_t dptr = 0;
  uint8_t n = 0;
  int nlin = 0;
  char buf[255] = {0};
  uint32_t tmo;

  uint8_t ltype = luaL_checkinteger(L, 1);
  uint8_t otype = luaL_checkinteger(L, 2);
  if (otype == 1) ltype = 1;
  _getFName(L, 3); // get remote dir/file spec
  
  mico_rtos_lock_mutex(&ftp_mut);

  if ((gL == NULL) || (ftpCmdSocket == NULL) || (!(status & FTP_LOGGED))) {
    ftp_log("[FTP usr] Login first\r\n" );
    err = -1;
    goto exit;
  }

  if (!_waitDataSocketFree()) {
    err = -2;
    goto exit;
  }
  
  list_type = ltype;
  data_done = 0;
  ftpCmdSocket->clientFlag = REQ_ACTION_LIST;
  
  if (ftpCmdSocket->list_cb != LUA_NOREF) {
    goto exit;
  }
  
  // no cb function, wait until List received (max 10 sec)
  tmo = mico_get_time();
  while (data_done == 0) {
    if ((mico_get_time() - tmo) > 10000) break;
    mico_rtos_unlock_mutex(&ftp_mut);
    mico_thread_msleep(60);
    mico_rtos_lock_mutex(&ftp_mut);
    luaWdgReload();
  }
  _waitDataSocketFree();

  if (data_done == 0) {
    ftp_log("[FTP usr] Timeout: list not received\r\n" );
    err = -3;
    goto exit;
  }

  if ((recvDataBuf == NULL) || (recvDataLen == 0)) {
    ftp_log("[FTP usr] List not received\r\n" );
    err = -4;
    goto exit;
  }
  
  if (otype != 1) {
    printf("===================\r\n");
    printf("FTP directory list:");
    if (recvDataLen >= (recvDataBufLen-16)) {
      printf(" (buffer full)");
    }
    printf("\r\n");
  }
  else {
    lua_newtable( L );
  }

  nlin = 0;
  while (dptr < recvDataLen) {
    if (*(recvDataBuf+dptr) == '\0') break;
    if ((*(recvDataBuf+dptr) == '\n') || (*(recvDataBuf+dptr) == '\r') || (n >= 254)) {
      // EOL, print line
      if (n > 0) {
        nlin++;
        if (otype != 1) printf("%s\r\n", &buf[0]);
        else {
          lua_pushstring( L, &buf[0] );
          lua_rawseti(L,-2,nlin);
        }
        n = 0;
      }
    }
    if (*(recvDataBuf+dptr) >= ' ') buf[n++] = *(recvDataBuf+dptr);
    buf[n] = '\0';
    dptr++;
  }
  if (n > 0) { // last line
    nlin++;
    if (otype != 1) printf("%s\r\n", &buf[0]);
    else {
      lua_pushstring( L, &buf[0] );
      lua_rawseti(L,-2, nlin);
    }
  }
  if (otype != 1) {
    printf("===================\r\n");
    lua_pushinteger(L, nlin);
    mico_rtos_unlock_mutex(&ftp_mut);
    return 1;
  }
  else {
    ftp_log("[FTP usr] List received to table\r\n" );
    lua_pushinteger(L, nlin);
    mico_rtos_unlock_mutex(&ftp_mut);
    return 2;
  }
  
exit:
  if (otype == 1) {
    lua_newtable( L );
    lua_pushinteger(L, err);
    mico_rtos_unlock_mutex(&ftp_mut);
    return 2;
  }
  lua_pushinteger(L, err);
  mico_rtos_unlock_mutex(&ftp_mut);
  return 1;
}

//ftp.chdir([dir])
//===================================
static int lftp_chdir( lua_State* L )
{
  uint32_t tmo;
  
  mico_rtos_lock_mutex(&ftp_mut);
  
  if ( (gL == NULL) || (ftpCmdSocket == NULL) || (!(status & FTP_LOGGED)) ) {
    ftp_log("[FTP usr] Login first\r\n" );
    lua_pushinteger(L, -1);
    goto exit;
  }
  _getFName(L, 1); // get remote dir

  free(ftpresponse);
  ftpresponse = NULL;
  cmd_done = 0;
  ftpCmdSocket->clientFlag = REQ_ACTION_CHDIR;

  tmo = mico_get_time();
  while (cmd_done == 0) {
    if ((mico_get_time() - tmo) > 5000) break;
    mico_rtos_unlock_mutex(&ftp_mut);
    mico_thread_msleep(60);
    mico_rtos_lock_mutex(&ftp_mut);
    luaWdgReload();
  }
  if (cmd_done == 0) {
    ftp_log("[FTP usr] Timeout\r\n" );
    lua_pushinteger(L, -2);
    goto exit;
  }

  lua_pushinteger(L, 0);
  if (ftpresponse != NULL) {
    lua_pushstring(L, ftpresponse);
    free(ftpresponse);
    ftpresponse = NULL;
  }
  else lua_pushstring(L, "?");
  mico_rtos_unlock_mutex(&ftp_mut);
  return 2;

exit:
  mico_rtos_unlock_mutex(&ftp_mut);
  return 1;
  
}

//ftp.recv(file [,tostr])
//===================================
static int lftp_recv( lua_State* L )
{
  int max_fsize = 32 * 1024;

  mico_rtos_lock_mutex(&ftp_mut);

  if ( (gL == NULL) || (ftpCmdSocket == NULL) || (!(status & FTP_LOGGED)) ) {
    ftp_log("[FTP usr] Login first\r\n" );
    lua_pushinteger(L, -11);
    goto exit;
  }

  if (!_waitDataSocketFree()) {
    lua_pushinteger(L, -15);
    goto exit;
  }
  if (_getFName(L, 1) < 0) {
    lua_pushinteger(L, -12);
    goto exit;
  }
  
  recv_type = RECV_TOFILE;
  if (lua_gettop(L) >= 2) {
    int tos = luaL_checkinteger(L, 2);
    if (tos == 1) recv_type = RECV_TOSTRING;
  }

  if (recv_type == RECV_TOFILE) {
    // get max file size
    uint32_t total, used;
    SPIFFS_info(&fs, &total, &used);
    if ((total > 2000000) || (used > 2000000) || (used > total)) {
      ftp_log("[FTP fil] File system error\r\n" );
      lua_pushinteger(L, -16);
      goto exit;
    }
    max_fsize = total - used - (10 *1024);
    if (max_fsize < 10*1024) {
      ftp_log("[FTP fil] No space left on fs!\r\n" );
      lua_pushinteger(L, -17);
      goto exit;
    }
    if (max_fsize > (512*1024)) max_fsize = 512*1024;
    ftp_log("[FTP fil] Max file size: %d\r\n", max_fsize);
    // open the file
    if (_openFile(1) < 0) {
      lua_pushinteger(L, -13);
      goto exit;
    }
  }
  
  data_ready = 0;
  data_done = 0;
  ftpCmdSocket->clientFlag = REQ_ACTION_RECV;
  
  if (recv_type == RECV_TOFILE) {
    // ** receiving to file,
    //    we have to handle saving to file from THIS thread
    uint32_t tmo = mico_get_time();
    while ((mico_get_time() - tmo) < 4000) {
      mico_rtos_unlock_mutex(&ftp_mut);
      mico_thread_msleep(5);
      mico_rtos_lock_mutex(&ftp_mut);

      if (data_ready == 1) {
        // some data received, write to file
        if (!(status & FTP_RECEIVING)) file_status = -4;
        else if (file_fd == FILE_NOT_OPENED) file_status =  -2;
        else if ((recvDataBuf != NULL) && (recvDataLen > 0)) {
          if ((file_status + recvDataLen) < max_fsize) {
            // OK to write
            int writen = SPIFFS_write(&fs, file_fd, (char*)recvDataBuf, recvDataLen);
            mico_thread_msleep(5);
            
            if (writen != recvDataLen) { //failed
              SPIFFS_fflush(&fs, file_fd);
              SPIFFS_close(&fs, (spiffs_file)file_fd);
              file_fd = FILE_NOT_OPENED;
              ftp_log("\r\n[FTP dta] Write to file failed (%d)\r\n", writen);
              file_status = -3;
            }
            else {
              SPIFFS_fflush(&fs, file_fd);
              file_status += recvDataLen;
              ftp_log("\r[FTP dta] Received %d byte(s)", file_status);
            }
          }
          else {
            // file too large, no space left to write
            file_status += recvDataLen;
            ftp_log("\r[FTP dta] Max size exceded: %d byte(s)", file_status);
          }
        }
        data_ready = 0;
        tmo = mico_get_time();
      }
      else if (data_ready == 2) {
        data_ready = 0;
        break;
      }
      luaWdgReload();
    }
    if (file_fd != FILE_NOT_OPENED) {
      SPIFFS_fflush(&fs, file_fd);
      SPIFFS_close(&fs, file_fd);
      file_fd = FILE_NOT_OPENED;
    }
    free(ftpfile);
    ftpfile = NULL;
    if (file_status >= max_fsize) {
      ftp_log("\r\n[FTP dta] File too big, truncated\r\n");
    }
    else {
      ftp_log("\r\n[FTP dta] Data file closed\r\n");
    }
    lua_pushinteger(L, file_status);
    _waitDataSocketFree();
  }
  else if ((recv_type == RECV_TOSTRING) && (ftpCmdSocket->received_cb == LUA_NOREF)) {
    // no cb function & receive to string,
    // wait until file received (max 10 sec)
    uint32_t tmo = mico_get_time();
    while (!data_done) {
      if ((mico_get_time() - tmo) > 10000) break;
      mico_rtos_unlock_mutex(&ftp_mut);
      mico_thread_msleep(60);
      mico_rtos_lock_mutex(&ftp_mut);
      luaWdgReload();
    }
    _waitDataSocketFree();
    if (!data_done) {
      ftp_log("[FTP usr] Timeout: file not received\r\n" );
      lua_pushinteger(L, -14);
    }
    else {
      ftp_log("[FTP usr] File received\r\n" );
      lua_pushinteger(L, file_status);
      lua_pushstring(L, recvDataBuf);
      mico_rtos_unlock_mutex(&ftp_mut);
      return 2;
    }
  }
  else lua_pushinteger(L, 0);

exit:
  mico_rtos_unlock_mutex(&ftp_mut);
  return 1;
}

//ftp.send(localfile [,append])
//==================================
static int lftp_send( lua_State* L )
{
  mico_rtos_lock_mutex(&ftp_mut);
  uint32_t tmo;
  
  if ( (gL == NULL) || (ftpCmdSocket == NULL) || (!(status & FTP_LOGGED)) ) {
    ftp_log("[FTP usr] Login first\r\n" );
    lua_pushinteger(L, -11);
    goto exit;
  }
  if (lua_gettop(L) >= 2) {
    send_type = (uint8_t)luaL_checkinteger(L, 2);
    if (send_type != SEND_APPEND) send_type = SEND_OVERWRITTE;
  }
  else send_type = SEND_OVERWRITTE;
  
  if (!_waitDataSocketFree()) {
    lua_pushinteger(L, -15);
    goto exit;
  }
  if (_getFName(L, 1) < 0) {
    lua_pushinteger(L, -12);
    goto exit;
  }
  file_size = _openFile(0);
  if ( file_size < 0) {
    file_size = 0;
    lua_pushinteger(L, -13);
    goto exit;
  }

  data_done = 0;
  data_get = 1;
  ftpCmdSocket->clientFlag = REQ_ACTION_SEND;
  
  // ** sending from file,
  //    we have to handle saving to file from THIS thread
  tmo = mico_get_time();
  while ((mico_get_time() - tmo) < 4000) {
    mico_rtos_unlock_mutex(&ftp_mut);
    mico_thread_msleep(5);
    mico_rtos_lock_mutex(&ftp_mut);
    if (data_get == 2) {
      data_get = 0;
      if (ftpfile == NULL) {
        ftp_log("\r[FTP dta] Send file not valid\r\n");
        file_status = -1;
        break;
      }
      else if (recvDataBuf == NULL) {
        ftp_log("\r[FTP dta] Buffer error\r\n");
        file_status = -2;
        break;
      }
      else if (file_status >= file_size) {
        break;
      }
      else {
        uint16_t len = MIN_RECVDATABUF_LEN-4;
        if (recvDataBufLen < len) len = recvDataBufLen-4;
        sendDataLen = SPIFFS_read(&fs, file_fd, (char*)recvDataBuf, len);
        if (sendDataLen > 0) {
          file_status += sendDataLen;
          ftp_log("\r[FTP dta] %.*f %%", 1, (float)(((float)file_status/(float)file_size)*100.0));
          data_get = 1; // send data
          tmo = mico_get_time();
        }
        else if (sendDataLen < 0) {
          ftp_log("\r[FTP dta] Error reading from file (%d)\r\n", sendDataLen);
          file_status = -4;
          break;
        }
        else break;
      }
    }
    else if (data_get == 3) {
      // abort, error sending
      break;
    }
    luaWdgReload();
  }
  data_get = 0;
  if ((mico_get_time() - tmo) > 4000) {
    ftp_log("\r[FTP dta] Timeout while sending data\r\n");
  }
  // Close file
  if (file_fd != FILE_NOT_OPENED) {
    SPIFFS_fflush(&fs, file_fd);
    SPIFFS_close(&fs, file_fd);
    file_fd = FILE_NOT_OPENED;
  }
  free(ftpfile);
  ftpfile = NULL;
  ftp_log("\r\n[FTP dta] Data file closed\r\n");
  lua_pushinteger(L, file_status);
  _waitDataSocketFree();

exit:
  mico_rtos_unlock_mutex(&ftp_mut);
  return 1;
}

//ftp.sendstring(remotefile, str [,append])
//========================================
static int lftp_sendstring( lua_State* L )
{
  mico_rtos_lock_mutex(&ftp_mut);
  
  if ((gL == NULL) || (ftpCmdSocket == NULL) || ((status & FTP_LOGGED) == 0)) {
    ftp_log("[FTP usr] Login first\r\n" );
    lua_pushinteger(L, -11);
    goto exit;
  }

  if (lua_gettop(L) >= 3) {
    send_type = (uint8_t)luaL_checkinteger(L, 3);
    if (send_type != SEND_APPEND) send_type = SEND_OVERWRITTE;
  }
  else send_type = SEND_OVERWRITTE;
  send_type |= SEND_STRING;

  if (_getFName(L, 1) < 0) {
    lua_pushinteger(L, -12);
    goto exit;
  }
  
  size_t len;
  sendDataBuf = (char*)luaL_checklstring( L, 2, &len );
  if ((sendDataBuf == NULL) || (len <= 0)) {
    ftp_log("[FTP fil] Bad string\r\n");
    lua_pushinteger(L, -14);
  }
  else {
    if (!_waitDataSocketFree()) {
      lua_pushinteger(L, -15);
      goto exit;
    }
    file_size = len;
    data_done = 0;
    ftpCmdSocket->clientFlag = REQ_ACTION_SEND;
  
    uint32_t tmo = mico_get_time();
    while (!data_done) {
      if ((mico_get_time() - tmo) > 10000) break;
      mico_rtos_unlock_mutex(&ftp_mut);
      mico_thread_msleep(60);
      mico_rtos_lock_mutex(&ftp_mut);
      luaWdgReload();
    }
    _waitDataSocketFree();
    if (!data_done) {
      ftp_log("[FTP usr] Timeout: string not sent\r\n" );
      lua_pushinteger(L, -15);
    }
    else {
      ftp_log("[FTP usr] String sent\r\n" );
      lua_pushinteger(L, file_status);
    }
    sendDataBuf = NULL;
  }

exit:
  mico_rtos_unlock_mutex(&ftp_mut);
  return 1;
}


#define MIN_OPT_LEVEL   2
#include "lrodefs.h"
const LUA_REG_TYPE ftp_map[] =
{
  {LSTRKEY("new"), LFUNCVAL(lftp_new)},
  {LSTRKEY("start"), LFUNCVAL(lftp_start)},
  {LSTRKEY("stop"), LFUNCVAL(lftp_stop)},
  {LSTRKEY("on"), LFUNCVAL(lftp_on)},
  {LSTRKEY("chdir"), LFUNCVAL(lftp_chdir)},
  {LSTRKEY("recv"), LFUNCVAL(lftp_recv)},
  {LSTRKEY("send"), LFUNCVAL(lftp_send)},
  {LSTRKEY("sendstring"), LFUNCVAL(lftp_sendstring)},
  {LSTRKEY("list"), LFUNCVAL(lftp_list)},
  {LSTRKEY("debug"), LFUNCVAL(lftp_debug)},
#if LUA_OPTIMIZE_MEMORY > 0
#endif        
  {LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_ftp(lua_State *L)
{

  mico_rtos_init_mutex(&ftp_mut);

#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else  
  luaL_register( L, EXLIB_NET, ftp_map );
 
  return 1;
#endif
}
