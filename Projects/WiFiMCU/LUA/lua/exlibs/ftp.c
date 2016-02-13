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
extern spiffs fs;
extern volatile int file_fd;
extern int mode2flag(char *mode);
extern void luaWdgReload( void );

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
static bool ftp_thread_is_started = false;
static uint8_t data_done = 1;
static uint8_t cmd_done = 1;
static uint8_t status = FTP_NOT_CONNECTED;
static char *recvBuf = NULL;
static uint16_t recvLen = 0;
static char *recvDataBuf = NULL;
static char *sendDataBuf = NULL;
static uint16_t recvDataLen = 0;
static uint16_t max_recv_datalen = 1024;
static int file_status = 0;
static int file_size = 0;
static uint8_t list_type = 0;
static uint8_t send_type = 0;
static uint8_t recv_type = 0;


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
static void closeCmdSocket(uint8_t unreg)
{
  if (ftpCmdSocket == NULL) return;

  //unref cb functions
  if (gL != NULL) {
    if (unreg) {
      if (ftpCmdSocket->disconnect_cb != LUA_NOREF)
        luaL_unref(gL, LUA_REGISTRYINDEX, ftpCmdSocket->disconnect_cb);
    }
    ftpCmdSocket->disconnect_cb = LUA_NOREF;
    
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
    recvDataBuf = malloc(max_recv_datalen+4);
    if (recvDataBuf == NULL) {
      free(ftpDataSocket);
      ftpDataSocket = NULL;
      return -3;
    }
    memset(recvDataBuf, 0, max_recv_datalen);
  }
  recvDataLen = 0;
  status &= ~FTP_DATACONNECTED;
  return 0;
}

//------------------------------------
static void _ftp_deinit(uint8_t unreg)
{
  closeCmdSocket(unreg);
  free(ftpCmdSocket);
  ftpCmdSocket = NULL;
  closeDataSocket();
  free(ftpDataSocket);
  ftpDataSocket = NULL;
  
  free( pDomain4Dns);
  pDomain4Dns=NULL;
  free( ftpuser);
  ftpuser=NULL;
  free( ftppass);
  ftppass=NULL;
  free( ftpfile);
  ftpfile=NULL;
  free( recvBuf);
  recvBuf=NULL;
  free( recvDataBuf);
  recvDataBuf=NULL;
  free(ftpresponse);
  ftpresponse = NULL;
  if ((gL != NULL) & (unreg == 0)) ftp_log("[FTP end] FTP SESSION CLOSED.\r\n");
  status = FTP_NOT_CONNECTED;
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

//----------------------------------
static void _saveData(uint8_t close)
{
  if (!(status & FTP_RECEIVING)) {
    ftp_log("[FTP dta] Not receiving!\r\n");
    file_status = -4;
    return;
  }
  if ((ftpfile == NULL) || (file_fd == FILE_NOT_OPENED)) {
    ftp_log("[FTP dta] Receive file not opened\r\n");
    file_status = -2;
    return;
  }

  if (close == 0) {  
    if (SPIFFS_write(&fs, (spiffs_file)file_fd, (char*)recvDataBuf, recvDataLen) < 0)
    { //failed
      SPIFFS_fflush(&fs,file_fd);
      SPIFFS_close(&fs,file_fd);
      file_fd = FILE_NOT_OPENED;
      ftp_log("\r\n[FTP dta] Write to local file failed\r\n");
      file_status = -3;
    }
    else {
      ftp_log("\r[FTP dta] received: %d", recvDataLen);
      file_status += recvDataLen;
    }
  }
  else { // close file;
    SPIFFS_fflush(&fs,file_fd);
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
    free(ftpfile);
    ftpfile = NULL;
    if (close == 2) {
      ftp_log("\r\n[FTP dta] Data ERROR, file closed\r\n");
      file_status = -1;
    }
    else {
      ftp_log("\r\n[FTP dta] Data file closed\r\n");
    }
  }
}

//---------------------
uint8_t _sendFile(void)
{
  if ((ftpfile == NULL) || (file_fd == FILE_NOT_OPENED)) {
    ftp_log("[FTP dta] Send file not opened\r\n");
    file_status = -1;
    return 1;
  }
  if (recvDataBuf == NULL) {
    ftp_log("[FTP dta] Buffer error\r\n");
    file_status = -2;
    return 1;
  }
  if (file_status >= file_size) return 1;

  uint16_t len = 1020;
  int rdlen;
  if (max_recv_datalen < len) len = max_recv_datalen-4;
  rdlen = SPIFFS_read(&fs, (spiffs_file)file_fd, (char*)recvDataBuf, len);
  if (rdlen > 0) {
    if (!_sendData(recvDataBuf, rdlen)) {
      ftp_log("[FTP dta] Error sending data\r\n");
      file_status = -3;
      return 1;
    }
    else {
      file_status += rdlen;
      ftp_log("\r[FTP dta] %.*f %%", 1, (float)(((float)file_status/(float)file_size)*100.0));
      if (file_status >= file_size) return 1;
      else return 0;
    }
  }
  else if (rdlen <= 0) {
    ftp_log("[FTP dta] Error reading from file (%d)\r\n", rdlen);
    file_status = -4;
    return 1;
  }
  else return 1;
}

//-----------------------
uint8_t _sendString(void)
{
  if (sendDataBuf == NULL) {
    ftp_log("[FTP dta] Buffer error\r\n");
    file_status = -2;
    return 1;
  }
  if (file_status >= file_size) return 1;

  int len = 1020;
  if ((file_size - file_status) < len) len = file_size - file_status;
  
  if (!_sendData(sendDataBuf+file_status, len)) {
    ftp_log("[FTP dta] Error sending data\r\n");
    file_status = -3;
    return 1;
  }
  else {
    file_status += len;
    ftp_log("\r[FTP dta] %.*f %%", 1, (float)(((float)file_status/(float)file_size)*100.0));
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
    if ((ftpfile != NULL) && ftpDataSocket != NULL) {
      status |= FTP_SENDING;
      if ((send_type & SEND_STRING)) {
        ftp_log("[FTP dta] Sending string to %s (%d)\r\n", ftpfile, file_size);
      }
      else {
        ftp_log("[FTP dta] Sending file %s (%d)\r\n", ftpfile, file_size);
      }
    }
    else _setResponse(cmd);
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
            ftpCmdSocket->clientLastFlag = NO_ACTION;
            ftpCmdSocket->clientFlag = REQ_ACTION_DOLIST;
          }
          else if (ftpCmdSocket->clientLastFlag == REQ_ACTION_RECV) {
            ftpCmdSocket->clientLastFlag = NO_ACTION;
            ftpCmdSocket->clientFlag = REQ_ACTION_DORECV;
          }
          else if (ftpCmdSocket->clientLastFlag == REQ_ACTION_SEND) {
            ftpCmdSocket->clientLastFlag = NO_ACTION;
            ftpCmdSocket->clientFlag = REQ_ACTION_DOSEND;
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

  t_val.tv_sec = 0;
  t_val.tv_usec = 5000;

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
      mico_thread_msleep(50);
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
    mico_thread_msleep(5);
        
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
        queue_msg_t msg;
        msg.L = gL;
        msg.source = onFTP;
        msg.para1 = 0;
        msg.para2 = ftpCmdSocket->disconnect_cb;
        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
      closeCmdSocket(0);
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
        queue_msg_t msg;
        msg.L = gL;
        msg.source = onFTP;
        msg.para1 = 1;
        msg.para3 = NULL;
        msg.para2 = ftpCmdSocket->logon_cb;
        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
    }
    //REQ_ACTION_LIST_RECEIVED
    else if (ftpCmdSocket->clientFlag == REQ_ACTION_LIST_RECEIVED) {
      ftpCmdSocket->clientFlag=NO_ACTION;
      if (ftpCmdSocket->list_cb != LUA_NOREF) {
        queue_msg_t msg;
        msg.L = gL;
        msg.source = onFTP;
        msg.para1 = recvDataLen;
        msg.para2 = ftpCmdSocket->list_cb;
        msg.para3 = (uint8_t*)malloc(recvDataLen+4);
        if (msg.para3 != NULL) memcpy((char*)msg.para3, recvDataBuf, recvDataLen);

        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
    }
    //REQ_ACTION_RECEIVED
    else if (ftpCmdSocket->clientFlag == REQ_ACTION_RECEIVED) {
      ftpCmdSocket->clientFlag=NO_ACTION;
      if (ftpCmdSocket->received_cb != LUA_NOREF) {
        queue_msg_t msg;
        msg.L = gL;
        msg.source = onFTP;
        msg.para1 = file_status;
        msg.para3 = NULL;
        if (recv_type == RECV_TOSTRING) {
          msg.para3 = (uint8_t*)malloc(recvDataLen+4);
          if (msg.para3 != NULL) memcpy((char*)msg.para3, recvDataBuf, recvDataLen);
        }
        msg.para2 = ftpCmdSocket->received_cb;

        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
    }
    //REQ_ACTION_SENT
    else if (ftpCmdSocket->clientFlag == REQ_ACTION_SENT) {
      ftpCmdSocket->clientFlag=NO_ACTION;
      if (ftpCmdSocket->sent_cb != LUA_NOREF) {
        queue_msg_t msg;
        msg.L = gL;
        msg.source = onFTP;
        msg.para1 = file_status;
        msg.para3 = NULL;
        msg.para2 = ftpCmdSocket->sent_cb;

        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
    }

    // ========== stage 2, check data send =====================================
    if ((ftpDataSocket != NULL) && ((status & FTP_SENDING))) {
      FD_ZERO(&wrset);
      FD_SET(ftpDataSocket->socket, &wrset);
      // check socket state
      select(ftpDataSocket->socket+1, NULL, &wrset, NULL, &t_val);
      
      if (FD_ISSET(ftpDataSocket->socket, &wrset)) {
        int err;
        if ((send_type & SEND_STRING)) err = _sendString();
        else err = _sendFile();
        if (err != 0) {
          closeDataSocket();
          // close file;
          if (file_fd != FILE_NOT_OPENED) {
            SPIFFS_close(&fs,file_fd);
            file_fd = FILE_NOT_OPENED;
            ftp_log("\r\n[FTP dta] Data file closed\r\n");
          }
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
      if (ftpDataSocket->socket > maxfd) maxfd = ftpDataSocket->socket;
      FD_SET(ftpDataSocket->socket, &readset);
    }
    // ** check sockets state
    select(maxfd+1, &readset, NULL, NULL, &t_val);

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
        int rcv_len = recv(ftpDataSocket->socket, (recvDataBuf+recvDataLen), max_recv_datalen-recvDataLen-1, 0);

        if (rcv_len <= 0) { // failed
          if (!(status & (FTP_RECEIVING | FTP_LISTING))) {
            ftp_log("\r\n[FTP dta] Disconnect!\r\n");
            closeDataSocket();
            file_status = -9;
            data_done = 1;
          }
          continue;
        }
        else {
          if ((status & FTP_RECEIVING) && (recv_type == RECV_TOFILE)) {
            // === write received data to file ===
            recvDataLen = rcv_len;
            _saveData(0);
            recvDataLen = 0;
          }
          else if ((status & FTP_LISTING) || ((status & FTP_RECEIVING) && (recv_type == RECV_TOSTRING))) {
            if ((recvDataLen + rcv_len) < (max_recv_datalen-16)) recvDataLen += rcv_len;
            else recvDataLen = max_recv_datalen-16;
            *(recvDataBuf + recvDataLen) = '\0';
          }
        }
        timeout = mico_get_time();
        recv_tmo = 0;
      }
    }

    // ===== nothing received ==================================================
    recv_tmo++;
    if (recv_tmo < 10) continue;
    
    recv_tmo = 0;

    // == Check if something was received from Command socket ==
    if (recvLen > 0) {
      // == Analize response ===
      response();
      recvLen = 0;
      memset(recvBuf, 0, MAX_RECV_LEN);
    }

    // == Check if Data socket was receiving ==
    if ((status & (FTP_RECEIVING | FTP_LISTING))) {
      // Finish all operattions on data socket
      if ((status & FTP_RECEIVING) && (recv_type == RECV_TOFILE)) {
        _saveData(1);
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
  } // while

terminate:
  _ftp_deinit(0);
  ftp_log("\r\n[FTP trd] FTP THREAD TERMINATED\r\n");
  ftp_thread_is_started = false;
  mico_rtos_delete_thread( NULL );
}
// ==================================


//stat = ftp.new(serv,port,user,pass [,buflen])
//=================================
static int lftp_new( lua_State* L )
{

  size_t dlen=0, ulen=0, plen=0;
  _ftp_deinit(1);
  gL = NULL;
  
  const char *domain = luaL_checklstring( L, 1, &dlen );
  if (dlen>128 || domain == NULL) {
    ftp_log("[FTP usr] Domain needed\r\n" );
    lua_pushinteger(L, -1);
    return 1;
  }
  int port = luaL_checkinteger( L, 2 );
  const char *user = luaL_checklstring( L, 3, &ulen );
  if (ulen>128 || user == NULL) {
    ftp_log("[FTP usr] Eser needed\r\n" );
    lua_pushinteger(L, -2);
    return 1;
  }
  const char *pass = luaL_checklstring( L, 4, &plen );
  if (plen>128 || pass == NULL)  {
    ftp_log("[FTP usr] Pass needed\r\n" );
    lua_pushinteger(L, -3);
    return 1;
  }
  max_recv_datalen = 1024;
  if (lua_gettop(L) >= 5) {
    int maxdl = luaL_checkinteger(L, 5);
    if ((maxdl >= 512) && (maxdl <= 4096)) max_recv_datalen = (uint16_t)maxdl;
  }

  // allocate buffers
  pDomain4Dns=(char*)malloc(dlen+1);
  ftpuser=(char*)malloc(ulen+1);
  ftppass=(char*)malloc(plen+1);
  if ((pDomain4Dns==NULL) || (ftpuser==NULL) || (ftppass==NULL)) {
    _ftp_deinit(0);    
    ftp_log("[FTP usr] Memory allocation failed\r\n" );
    lua_pushinteger(L, -4);
    return 1;
  }
  strcpy(pDomain4Dns,domain);
  strcpy(ftpuser,user);
  strcpy(ftppass,pass);

  int socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socketHandle < 0) {
    _ftp_deinit(0);    
    ftp_log("[FTP usr] Open CMD socket failed\r\n" );
    lua_pushinteger(L, -5);
    return 1;
  }
  
  ftpCmdSocket = NULL;
  ftpDataSocket = NULL;
  
  ftpCmdSocket = (ftpCmdSocket_t*)malloc(sizeof(ftpCmdSocket_t));
  if (ftpCmdSocket == NULL) {
    _ftp_deinit(0);    
    ftp_log("[FTP usr] Memory allocation failed\r\n" );
    lua_pushinteger(L, -6);
    return 1;
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
  uint32_t opt=0;
  int err = setsockopt(socketHandle,IPPROTO_IP,SO_BLOCKMODE,&opt,4); // non block mode
  if (err < 0) {
    _ftp_deinit(0);    
    ftp_log("[FTP usr] Set socket options failed\r\n" );
    lua_pushinteger(L, -7);
    return 1;
  }

  gL = L;
  ftp_log("[FTP usr] FTP client configured.\r\n" );
  lua_pushinteger(L, 0);
  return 1;
}

//stat = ftp.start()
//===================================
static int lftp_start( lua_State* L )
{
  LinkStatusTypeDef wifi_link;
  int err = micoWlanGetLinkStatus( &wifi_link );

  if ( wifi_link.is_connected == false ) {
    ftp_log("[FTP usr] WiFi NOT CONNECTED!\r\n" );
    lua_pushinteger(L, -1);
    return 1;
  }
  
  if ( (gL == NULL) || (ftpCmdSocket == NULL) ) {
    ftp_log("[FTP usr] Execute ftp.new first!\r\n" );
    lua_pushinteger(L, -2);
    return 1;
  }
  if (ftp_thread_is_started) {
    ftp_log("[FTP usr] Already started!\r\n" );
    lua_pushinteger(L, -3);
    return 1;
  }
  
  mico_system_notify_register( mico_notify_TCP_CLIENT_CONNECTED, (void *)_micoNotify_FTPClientConnectedHandler, NULL );

  // all setup, start the ftp thread
  if (!ftp_thread_is_started) {
    if (mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY-1, "Ftp_Thread", _thread_ftp, 1024, NULL) != kNoErr) {
      _ftp_deinit(0);    
      ftp_log("[FTP usr] Create thread failed\r\n" );
      lua_pushinteger(L, -4);
      return 1;
    }
    else ftp_thread_is_started = true;
  } 

  if (ftpCmdSocket->logon_cb != LUA_NOREF) {
    lua_pushinteger(L, 0);
    return 1;
  }
  
  // wait max 10 sec for login
  uint32_t tmo = mico_get_time();
  while ( (ftp_thread_is_started) && !(status & FTP_LOGGED) ) {
    if ((mico_get_time() - tmo) > 10000) break;
    mico_thread_msleep(100);
    luaWdgReload();
  }
  if (!(status & FTP_LOGGED)) lua_pushinteger(L, -4);
  else lua_pushinteger(L, 0);
  return 1;
}

//ftp.stop()
//===================================
static int lftp_stop( lua_State* L )
{
  if ( (ftp_thread_is_started) && (ftpCmdSocket != NULL) ) {
    ftpCmdSocket->clientFlag = REQ_ACTION_QUIT;

    if (ftpCmdSocket->disconnect_cb != LUA_NOREF) {
      lua_pushinteger(L, 0);
      return 1;
    }
    // wait max 10 sec for disconnect
    uint32_t tmo = mico_get_time();
    while (ftp_thread_is_started) {
      if ((mico_get_time() - tmo) > 10000) break;
      mico_thread_msleep(100);
      luaWdgReload();
    }
    if (!ftp_thread_is_started) {
      _ftp_deinit(0);    
      lua_pushinteger(L, 0);
    }
    else lua_pushinteger(L, -1);
  }
  else lua_pushinteger(L, 0);
  return 1;
}

//ftp.debuf(dbg)
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
  if ( (gL == NULL) || (ftpCmdSocket == NULL) ) {
    ftp_log("[FTP usr] Execute ftp.new first!" );
    lua_pushinteger(L, -1);
    return 1;
  }
  if (ftp_thread_is_started) {
    ftp_log("[FTP usr] Use before FTP is started!" );
    lua_pushinteger(L, -2);
    return 1;
  }

  size_t sl;
  const char *method = luaL_checklstring( L, 1, &sl );
  if (method == NULL) {
    ftp_log("[FTP usr] Method string needed" );
    lua_pushinteger(L, -3);
    return 1;
  }
  
  if ((lua_type(L, 2) == LUA_TFUNCTION) || (lua_type(L, 2) == LUA_TLIGHTFUNCTION)) {
    lua_pushvalue(L, 2);
  }
  else {
    ftp_log("[FTP usr] CB function needed" );
    lua_pushinteger(L, -4);
    return 1;
  }
  
  if ((strcmp(method,"login") == 0) && (sl == strlen("login"))) {
    if (ftpCmdSocket->logon_cb != LUA_NOREF)
      luaL_unref(gL,LUA_REGISTRYINDEX, ftpCmdSocket->logon_cb);
    ftpCmdSocket->logon_cb = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else if ((strcmp(method,"disconnect") == 0) && (sl == strlen("disconnect"))) {
    if (ftpCmdSocket->disconnect_cb != LUA_NOREF)
      luaL_unref(gL,LUA_REGISTRYINDEX, ftpCmdSocket->disconnect_cb);
    ftpCmdSocket->disconnect_cb = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else if ((strcmp(method,"receive") == 0) && (sl == strlen("receive"))) {
    if (ftpCmdSocket->received_cb != LUA_NOREF)
      luaL_unref(gL,LUA_REGISTRYINDEX, ftpCmdSocket->received_cb);
    ftpCmdSocket->received_cb = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else if ((strcmp(method,"send") == 0) && (sl == strlen("send"))) {
    if (ftpCmdSocket->sent_cb != LUA_NOREF)
      luaL_unref(gL,LUA_REGISTRYINDEX, ftpCmdSocket->sent_cb);
    ftpCmdSocket->sent_cb = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else if ((strcmp(method,"list") == 0) && (sl == strlen("list"))) {
    if (ftpCmdSocket->list_cb != LUA_NOREF)
      luaL_unref(gL,LUA_REGISTRYINDEX, ftpCmdSocket->list_cb);
    ftpCmdSocket->list_cb = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  else {
    ftp_log("[FTP usr] Unknown CB function: %s", method);
    lua_pushinteger(L, -5);
  }

  lua_pushinteger(L, 0);
  return 1;
}

//------------------------------
static int _openFile(char *mode)
{
  // open the file
  if (ftpfile == NULL) {
    ftp_log("[FTP fil] Ftpfile not assigned\r\n" );
    return -1;
  }
  ftp_log("[FTP fil] Opening local file: %s\r\n", ftpfile );
  if (FILE_NOT_OPENED != file_fd) {
    ftp_log("[FTP fil] Closing file first\r\n" );
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
  }
  file_fd = SPIFFS_open(&fs, (char*)ftpfile, mode2flag(mode), 0);
  if (file_fd <= FILE_NOT_OPENED) {
    ftp_log("[FTP fil] Error opening local file: %d\r\n", file_fd );
    file_fd = FILE_NOT_OPENED;
    return -2;
  }
  return 0;
}

//-----------------------------------------------
static int _getFName(lua_State* L, uint8_t index)
{
  free(ftpfile);
  ftpfile = NULL;
  if (lua_gettop(L) < index) return -3;
  
  size_t len = 0;
  const char *fname = luaL_checklstring( L, index, &len );
  if (len > SPIFFS_OBJ_NAME_LEN || fname == NULL) {
    ftp_log("[FTP fil] Bad name or length > %d\r\n", SPIFFS_OBJ_NAME_LEN);
    return -1;
  }
  ftpfile=(char*)malloc(len+1);
  if (ftpfile==NULL) {
    ftp_log("[FTP fil] Memory allocation failed\r\n" );
    return -2;
  }
  strcpy(ftpfile,fname);
  return 0;
}

//ftp.list(ltype, otype [,dir])
//===================================
static int lftp_list( lua_State* L )
{
  int err = 0;
  uint16_t dptr = 0;
  uint8_t n = 0;
  uint8_t i = 0;
  int nlin = 0;
  char buf[255] = {0};
  uint32_t tmo;

  uint8_t ltype = luaL_checkinteger(L, 1);
  uint8_t otype = luaL_checkinteger(L, 2);
  if (otype == 1) ltype = 1;
  err = _getFName(L, 3);
  err = 0;
  
  if ((gL == NULL) || (ftpCmdSocket == NULL) || (!(status & FTP_LOGGED))) {
    ftp_log("[FTP usr] Login first\r\n" );
    err = -1;
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
    mico_thread_msleep(60);
    luaWdgReload();
  }
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
    if (recvDataLen == (max_recv_datalen-16)) {
      printf(" (buffer full)");
    }
    printf("\r\n");
  }
  else {
    lua_newtable( L );
  }

  while (dptr < recvDataLen) {
    if (*(recvDataBuf+dptr) == '\0') break;
    if ((*(recvDataBuf+dptr) == '\n') || (*(recvDataBuf+dptr) == '\r') || (n >= 254)) {
      // EOL, print line
      if (n > 0) {
        nlin++;
        if (otype != 1) printf("%s\r\n", &buf[0]);
        else {
          lua_pushstring( L, &buf[0] );
          lua_rawseti(L,-2,i++);
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
      lua_rawseti(L,-2,i++);
    }
  }
  if (otype != 1) {
    printf("===================\r\n");
    lua_pushinteger(L, nlin);
    return 1;
  }
  else {
    ftp_log("[FTP usr] List received to table\r\n" );
    lua_pushinteger(L, nlin);
    return 2;
  }
  
exit:
  if (otype == 1) {
    lua_newtable( L );
    lua_pushinteger(L, err);
    return 2;
  }
  lua_pushinteger(L, err);
  return 1;
}

//ftp.chdir([dir])
//===================================
static int lftp_chdir( lua_State* L )
{
  if ( (gL == NULL) || (ftpCmdSocket == NULL) || (!(status & FTP_LOGGED)) ) {
    ftp_log("[FTP usr] Login first\r\n" );
    lua_pushinteger(L, -1);
    return 1;
  }
  _getFName(L, 1);

  free(ftpresponse);
  ftpresponse = NULL;
  cmd_done = 0;
  uint32_t tmo = mico_get_time();
  ftpCmdSocket->clientFlag = REQ_ACTION_CHDIR;

  while (cmd_done == 0) {
    if ((mico_get_time() - tmo) > 4000) break;
    mico_thread_msleep(60);
    luaWdgReload();
  }
  if (cmd_done == 0) {
    ftp_log("[FTP usr] Timeout\r\n" );
    lua_pushinteger(L, -2);
    return 1;
  }

  lua_pushinteger(L, 0);
  if (ftpresponse != NULL) {
    lua_pushstring(L, ftpresponse);
    free(ftpresponse);
    ftpresponse = NULL;
  }
  else lua_pushstring(L, "?");
  return 2;
}

//ftp.recv(file [,tostr])
//===================================
static int lftp_recv( lua_State* L )
{
  if ( (gL == NULL) || (ftpCmdSocket == NULL) || (!(status & FTP_LOGGED)) ) {
    ftp_log("[FTP usr] Login first\r\n" );
    lua_pushinteger(L, -11);
    return 1;
  }

  if (_getFName(L, 1) < 0) {
    ftp_log("[FTP fil] File name missing\r\n" );
    lua_pushinteger(L, -12);
    return 1;
  }
  if (_openFile("w") < 0) {
    lua_pushinteger(L, -13);
    return 1;
  }
  
  recv_type = RECV_TOFILE;
  if (lua_gettop(L) >= 2) {
    int tos = luaL_checkinteger(L, 2);
    if (tos == 1) recv_type = RECV_TOSTRING;
  }
  data_done = 0;
  ftpCmdSocket->clientFlag = REQ_ACTION_RECV;
  
  if (ftpCmdSocket->received_cb == LUA_NOREF) {
    // no cb function, wait until file received (max 10 sec)
    uint32_t tmo = mico_get_time();
    while (!data_done) {
      if ((mico_get_time() - tmo) > 10000) break;
      mico_thread_msleep(60);
      luaWdgReload();
    }
    if (!data_done) {
      ftp_log("[FTP usr] Timeout: file not received\r\n" );
      lua_pushinteger(L, -14);
    }
    else {
      ftp_log("[FTP usr] File received\r\n" );
      lua_pushinteger(L, file_status);
      if (recv_type == RECV_TOSTRING) {
        lua_pushstring(L, recvDataBuf);
        return 2;
      }
    }
  }
  else lua_pushinteger(L, 0);
  return 1;
}

//ftp.send(file [,append])
//==================================
static int lftp_send( lua_State* L )
{
  if ( (gL == NULL) || (ftpCmdSocket == NULL) || (!(status & FTP_LOGGED)) ) {
    ftp_log("[FTP usr] Login first\r\n" );
    lua_pushinteger(L, -11);
    return 1;
  }
  if (lua_gettop(L) >= 2) {
    send_type = (uint8_t)luaL_checkinteger(L, 2);
    if (send_type != SEND_APPEND) send_type = SEND_OVERWRITTE;
  }
  else send_type = SEND_OVERWRITTE;
  
  if (_getFName(L, 1) < 0) {
    ftp_log("[FTP fil] File name missing\r\n" );
    lua_pushinteger(L, -12);
    return 1;
  }
  if (_openFile("r") < 0) {
    lua_pushinteger(L, -13);
    return 1;
  }

  spiffs_stat s;
  // Get file size
  SPIFFS_fstat(&fs, file_fd, &s);
  file_size = s.size;
  
  data_done = 0;
  ftpCmdSocket->clientFlag = REQ_ACTION_SEND;
  
  if (ftpCmdSocket->sent_cb == LUA_NOREF) {
    // no cb function, wait until file received (max 10 sec)
    uint32_t tmo = mico_get_time();
    while (!data_done) {
      if ((mico_get_time() - tmo) > 10000) break;
      mico_thread_msleep(60);
      luaWdgReload();
    }
    if (!data_done) {
      ftp_log("[FTP usr] Timeout: file not sent\r\n" );
      lua_pushinteger(L, -14);
    }
    else {
      ftp_log("[FTP usr] File sent\r\n" );
      lua_pushinteger(L, file_status);
    }
  }
  else lua_pushinteger(L, 0);
  return 1;
}

//ftp.sendstring(file, str [,append])
//========================================
static int lftp_sendstring( lua_State* L )
{
  if ((gL != NULL) && (ftpCmdSocket != NULL)) {
    if ((status & FTP_LOGGED)) {
      if (lua_gettop(L) >= 3) {
        send_type = (uint8_t)luaL_checkinteger(L, 3);
        if (send_type != SEND_APPEND) send_type = SEND_OVERWRITTE;
      }
      else send_type = SEND_OVERWRITTE;
      send_type |= SEND_STRING;

      if (_getFName(L, 1) < 0) {
        ftp_log("[FTP fil] File name missing\r\n" );
        lua_pushinteger(L, -13);
        return 1;
      }
      
      size_t len;
      sendDataBuf = (char*)luaL_checklstring( L, 2, &len );
      if (sendDataBuf == NULL) {
        ftp_log("[FTP fil] Bad string\r\n");
        lua_pushinteger(L, -14);
      }
      else {
        file_size = len;
        data_done = 0;
        ftpCmdSocket->clientFlag = REQ_ACTION_SEND;
      
        uint32_t tmo = mico_get_time();
        while (!data_done) {
          if ((mico_get_time() - tmo) > 10000) break;
          mico_thread_msleep(60);
          luaWdgReload();
        }
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
    }
    else {
      ftp_log("[FTP usr] Not logged\r\n" );
      lua_pushinteger(L, -12);
    }
  }
  else {
    ftp_log("[FTP usr] Login first\r\n" );
    lua_pushinteger(L, -11);
  }
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

#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else  
  luaL_register( L, EXLIB_NET, ftp_map );
 
  return 1;
#endif
}
