/**
 * file.c
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"

#include "platform.h"
#include "mico_platform.h"
#include "user_config.h"
#include "CheckSumUtils.h"
#include "StringUtils.h"

#include <spiffs.h>
#include <spiffs_nucleus.h>

extern void luaWdgReload( void );


#define LOG_PAGE_SIZE       256
static u8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
static u8_t spiffs_fds[32*4];
static u8_t spiffs_cache_buf[(LOG_PAGE_SIZE+32)*4];
spiffs fs;
#define FILE_NOT_OPENED 0
static volatile int file_fd = FILE_NOT_OPENED;

#define PACKET_SEQNO_INDEX      (1)
#define PACKET_SEQNO_COMP_INDEX (2)

#define PACKET_HEADER           (3)
#define PACKET_TRAILER          (2)
#define PACKET_OVERHEAD         (PACKET_HEADER + PACKET_TRAILER)
#define PACKET_SIZE             (128)
#define PACKET_1K_SIZE          (1024)

#define FILE_SIZE_LENGTH        (16)

#define SOH                     (0x01)  /* start of 128-byte data packet */
#define STX                     (0x02)  /* start of 1024-byte data packet */
#define EOT                     (0x04)  /* end of transmission */
#define ACK                     (0x06)  /* acknowledge */
#define NAK                     (0x15)  /* negative acknowledge */
#define CA                      (0x18)  /* two of these in succession aborts transfer */
#define CRC16                   (0x43)  /* 'C' == 0x43, request 16-bit CRC */

#define ABORT1                  (0x41)  /* 'A' == 0x41, abort by user */
#define ABORT2                  (0x61)  /* 'a' == 0x61, abort by user */

#define NAK_TIMEOUT             (1000)
#define MAX_ERRORS              (45)




//------------------------------------------------------------
static s32_t lspiffs_read(u32_t addr, u32_t size, u8_t *dst) {
    MicoFlashRead(MICO_PARTITION_LUA, &addr,dst,size);
    return SPIFFS_OK;
  }

//-------------------------------------------------------------
static s32_t lspiffs_write(u32_t addr, u32_t size, u8_t *src) {
    MicoFlashWrite(MICO_PARTITION_LUA,&addr,src,size);
    return SPIFFS_OK;
  }

//--------------------------------------------------
static s32_t lspiffs_erase(u32_t addr, u32_t size) {
    MicoFlashErase(MICO_PARTITION_LUA,addr,addr+size-1);
    luaWdgReload(); //in case wathdog
    return SPIFFS_OK;
  } 

//-----------------------
void lua_spiffs_mount() {
    mico_logic_partition_t* part;
    spiffs_config cfg;    

    part = MicoFlashGetInfo( MICO_PARTITION_LUA );
	
    cfg.phys_size = part->partition_length;
    cfg.phys_addr = part->partition_start_addr; // start spiffs at start of spi flash
    cfg.phys_erase_block = 65536/2;             // according to datasheet
    cfg.log_block_size = 65536;                 // let us not complicate things
    cfg.log_page_size = LOG_PAGE_SIZE;          // as we said */
    
    cfg.hal_read_f = lspiffs_read;
    cfg.hal_write_f = lspiffs_write;
    cfg.hal_erase_f = lspiffs_erase;
    
    //if (part == NULL)
    //    goto exit;
    //MicoFlashInitialize(MICO_FLASH_FOR_LUA);
    
    if(SPIFFS_mounted(&fs)) return;
    
    int res = SPIFFS_mount(&fs,
      &cfg,
      spiffs_work_buf,
      spiffs_fds,
      sizeof(spiffs_fds),
      spiffs_cache_buf,
      sizeof(spiffs_cache_buf),
      0);
}

//--------------------------------
static int mode2flag(char *mode) {
  if(strlen(mode)==1){
  	if(strcmp(mode,"w")==0)
  	  return SPIFFS_WRONLY|SPIFFS_CREAT|SPIFFS_TRUNC;
  	else if(strcmp(mode, "r")==0)
  	  return SPIFFS_RDONLY;
  	else if(strcmp(mode, "a")==0)
  	  return SPIFFS_WRONLY|SPIFFS_CREAT|SPIFFS_APPEND;
  	else
  	  return SPIFFS_RDONLY;
  } else if (strlen(mode)==2){
  	if(strcmp(mode,"r+")==0)
  	  return SPIFFS_RDWR;
  	else if(strcmp(mode, "w+")==0)
  	  return SPIFFS_RDWR|SPIFFS_CREAT|SPIFFS_TRUNC;
  	else if(strcmp(mode, "a+")==0)
  	  return SPIFFS_RDWR|SPIFFS_CREAT|SPIFFS_APPEND;
  	else
  	  return SPIFFS_RDONLY;
  } else {
  	return SPIFFS_RDONLY;
  }
}

//-----------------------------------------------------------
static uint16_t Cal_CRC16(const uint8_t* data, uint32_t size)
{
  CRC16_Context contex;
  uint16_t ret;
  
  CRC16_Init( &contex );
  CRC16_Update( &contex, data, size );
  CRC16_Final( &contex, &ret );
  return ret;
}

/*
//------------------------------------------------------------
static uint8_t CalChecksum(const uint8_t* data, uint32_t size)
{
  uint32_t sum = 0;
  const uint8_t* dataEnd = data+size;

  while(data < dataEnd )
    sum += *data++;

  return (sum & 0xffu);
}
*/

//------------------------------------------------------------------------
static unsigned short crc16(const unsigned char *buf, unsigned long count)
{
  unsigned short crc = 0;
  int i;

  while(count--) {
          crc = crc ^ *buf++ << 8;

          for (i=0; i<8; i++) {
                  if (crc & 0x8000) {
                          crc = crc << 1 ^ 0x1021;
                  } else {
                          crc = crc << 1;
                  }
          }
  }
  return crc;
}


//--------------------------------------------------------
static int32_t Receive_Byte (uint8_t *c, uint32_t timeout)
{
  if (MicoUartRecv( MICO_UART_1, c, 1, timeout ) != kNoErr)
    return -1; // Timeout
  else
    return 0;  // ok
}

//-----------------------------------
static uint32_t Send_Byte (uint8_t c)
{
  MicoUartSend( MICO_UART_1, &c, 1 );
  return 0;
}

//----------------------------
static void send_CA ( void ) {
  Send_Byte(CA);
  Send_Byte(CA);
  mico_thread_msleep(500);
  luaWdgReload();
}


/**
  * @brief  Receive a packet from sender
  * @param  data
  * @param  length
  * @param  timeout
  *     0: end of transmission
  *    -1: abort by sender
  *    >0: packet length
  * @retval 0: normally return
  *        -1: timeout or packet error
  *        -2: crc error
  *         1: abort by user
  */
//------------------------------------------------------------------------------
static int32_t Receive_Packet (uint8_t *data, int32_t *length, uint32_t timeout)
{
  uint16_t i, packet_size;
  uint8_t c;
  //uint16_t tempCRC;
  *length = 0;
  
  luaWdgReload();
  if (Receive_Byte(&c, timeout) != 0)
  {
    luaWdgReload();
    return -1;
  }
  luaWdgReload();
  switch (c)
  {
    case SOH:
      packet_size = PACKET_SIZE;
      break;
    case STX:
      packet_size = PACKET_1K_SIZE;
      break;
    case EOT:
      return 0;
    case CA:
      if ((Receive_Byte(&c, timeout) == 0) && (c == CA))
      {
        *length = -1;
        luaWdgReload();
        return 0;
      }
      else
      {
        luaWdgReload();
        return -1;
      }
    case ABORT1:
    case ABORT2:
      luaWdgReload();
      return 1;
    default:
      luaWdgReload();
      return -1;
  }
  *data = c;
  luaWdgReload();
  for (i = 1; i < (packet_size + PACKET_OVERHEAD); i ++)
  {
    if (Receive_Byte(data + i, 10) != 0)
    {
      luaWdgReload();
      return -1;
    }
  }
  luaWdgReload();
  if (data[PACKET_SEQNO_INDEX] != ((data[PACKET_SEQNO_COMP_INDEX] ^ 0xff) & 0xff))
  {
    return -1;
  }
  if (crc16(&data[PACKET_HEADER], packet_size + PACKET_TRAILER) != 0) {
    return -1;
  }
  *length = packet_size;
  return 0;
}

/**
  * @brief  Receive a file using the ymodem protocol.
  * @param  buf: Address of the first byte.
  * @retval The size of the file.
  */
//---------------------------------------------------------------------------------
static int32_t Ymodem_Receive ( char* FileName, uint32_t maxsize, uint8_t getname )
{
  uint8_t packet_data[PACKET_1K_SIZE + PACKET_OVERHEAD], file_size[FILE_SIZE_LENGTH], *file_ptr;
  int32_t i, packet_length, file_len, write_len, session_done, file_done, packets_received, errors, session_begin, size = 0;
  
  for (session_done = 0, errors = 0, session_begin = 0; ;)
  {
    for (packets_received = 0, file_done = 0; ;)
    {
      switch (Receive_Packet(packet_data, &packet_length, NAK_TIMEOUT))
      {
        case 0:
          switch (packet_length)
          {
            /* Abort by sender */
            case -1:
              Send_Byte(ACK);
              return 0;
            /* End of transmission */
            case 0:
              Send_Byte(ACK);
              file_done = 1;
              break;
            /* Normal packet */
            default:
              if ((packet_data[PACKET_SEQNO_INDEX] & 0xff) != (packets_received & 0xff))
              {
                errors ++;
                if (errors > MAX_ERRORS)
                {
                  send_CA();
                  return 0;
                }
                Send_Byte(NAK);
              }
              else
              {
                errors = 0;
                if (packets_received == 0)
                {
                  /* Filename packet */
                  if (packet_data[PACKET_HEADER] != 0)
                  {
                    // Filename packet has valid data
                    if (getname == 0) {
                      for (i = 0, file_ptr = packet_data + PACKET_HEADER; (*file_ptr != 0) && (i < SPIFFS_OBJ_NAME_LEN);)
                      {
                        FileName[i++] = *file_ptr++;
                      }
                      FileName[i++] = '\0';
                    }
                    for (i = 0, file_ptr = packet_data + PACKET_HEADER; (*file_ptr != 0) && (i < packet_length);)
                    {
                      file_ptr++;
                    }
                    for (i = 0, file_ptr ++; (*file_ptr != ' ') && (i < FILE_SIZE_LENGTH);)
                    {
                      file_size[i++] = *file_ptr++;
                    }
                    file_size[i++] = '\0';
                    Str2Int(file_size, &size);

                    // Test the size of the file
                    if (size < 1 || size > maxsize) {
                      file_fd = FILE_NOT_OPENED;
                      /* End session */
                      send_CA();
                      return -4;
                    }

                    /* *** Open the file *** */
                    if (FILE_NOT_OPENED != file_fd) {
                      SPIFFS_close(&fs,file_fd);
                      file_fd = FILE_NOT_OPENED;
                    }
                    file_fd = SPIFFS_open(&fs, (char*)FileName, mode2flag("w"), 0);
                    if (file_fd <= FILE_NOT_OPENED) {
                      file_fd = FILE_NOT_OPENED;
                      /* End session */
                      send_CA();
                      return -2;
                    }
                    file_len = 0;
                    Send_Byte(ACK);
                    Send_Byte(CRC16);
                  }
                  /* Filename packet is empty, end session */
                  else
                  {
                    Send_Byte(ACK);
                    file_done = 1;
                    session_done = 1;
                    break;
                  }
                }
                /* Data packet */
                else
                {
                  /* Write received data to file */
                  if (file_len < size) {
                    file_len = file_len + packet_length;
                    if (file_len > size) {
                      write_len = packet_length - (file_len - size);
                    }
                    else {
                      write_len = packet_length;
                    }
                    if (file_fd <= FILE_NOT_OPENED) {
                      file_fd = FILE_NOT_OPENED;
                      // File not opened, End session
                      send_CA();
                      return -2;
                    }
                    if (SPIFFS_write(&fs,file_fd, (char*)(packet_data + PACKET_HEADER), write_len) < 0)
                    { //failed
                      SPIFFS_close(&fs,file_fd);
                      file_fd = FILE_NOT_OPENED;
                      /* End session */
                      send_CA();
                      return -1;
                    }
                  }
                  //success
                  Send_Byte(ACK);
                }
                packets_received ++;
                session_begin = 1;
              }
          }
          break;
        case 1:
          send_CA();
          return -3;
        default:
          if (session_begin >= 0)
          {
            errors ++;
          }
          if (errors > MAX_ERRORS)
          {
            send_CA();
            return 0;
          }
          Send_Byte(CRC16);
          break;
      }
      if (file_done != 0)
      {
        break;
      }
    }
    if (session_done != 0)
    {
      break;
    }
  }
  return (int32_t)size;
}

//-----------------------------------------------------------
static void Ymodem_SendPacket(uint8_t *data, uint16_t length)
{
  uint16_t i;

  luaWdgReload();
  i = 0;
  while (i < length)
  {
    Send_Byte(data[i]);
    i++;
  }
}

//----------------------------------------------------------------------------------------------
static void Ymodem_PrepareIntialPacket(uint8_t *data, const uint8_t* fileName, uint32_t *length)
{
  uint16_t i, j;
  uint8_t file_ptr[10];
  uint16_t tempCRC;
  
  // Make first three packet
  data[0] = SOH;
  data[1] = 0x00;
  data[2] = 0xff;
  
  // Filename packet has valid data
  for (i = 0; (fileName[i] != '\0') && (i <SPIFFS_OBJ_NAME_LEN);i++)
  {
     data[i + PACKET_HEADER] = fileName[i];
  }

  data[i + PACKET_HEADER] = 0x00;
  
  Int2Str (file_ptr, *length);
  for (j =0, i = i + PACKET_HEADER + 1; file_ptr[j] != '\0' ; )
  {
     data[i++] = file_ptr[j++];
  }
  data[i++] = 0x20;
  
  for (j = i; j < PACKET_SIZE + PACKET_HEADER; j++)
  {
    data[j] = 0;
  }
  tempCRC = Cal_CRC16(&data[PACKET_HEADER], PACKET_SIZE);
  data[PACKET_SIZE + PACKET_HEADER] = tempCRC >> 8;
  data[PACKET_SIZE + PACKET_HEADER + 1] = tempCRC & 0xFF;
}

//-------------------------------------------------
static void Ymodem_PrepareLastPacket(uint8_t *data)
{
  uint16_t i;
  uint16_t tempCRC;
  
  data[0] = SOH;
  data[1] = 0x00;
  data[2] = 0xff;
  for (i = PACKET_HEADER; i < (PACKET_SIZE + PACKET_HEADER); i++) {
    data[i] = 0x00;
  }
  tempCRC = Cal_CRC16(&data[PACKET_HEADER], PACKET_SIZE);
  data[PACKET_SIZE + PACKET_HEADER] = tempCRC >> 8;
  data[PACKET_SIZE + PACKET_HEADER + 1] = tempCRC & 0xFF;
}

//------------------------------------------------------------------------------
static void Ymodem_PreparePacket(uint8_t *data, uint8_t pktNo, uint32_t sizeBlk)
{
  uint16_t i, size;
  uint16_t tempCRC;
  
  data[0] = STX;
  data[1] = pktNo;
  data[2] = (~pktNo);

  size = sizeBlk < PACKET_1K_SIZE ? sizeBlk :PACKET_1K_SIZE;
  // Read block from file
  if (size > 0) SPIFFS_read(&fs, (spiffs_file)file_fd, data + PACKET_HEADER, size);

  if ( size  <= PACKET_1K_SIZE)
  {
    for (i = size + PACKET_HEADER; i < PACKET_1K_SIZE + PACKET_HEADER; i++)
    {
      data[i] = 0x1A; // EOF (0x1A) or 0x00
    }
  }
  tempCRC = Cal_CRC16(&data[PACKET_HEADER], PACKET_1K_SIZE);
  data[PACKET_1K_SIZE + PACKET_HEADER] = tempCRC >> 8;
  data[PACKET_1K_SIZE + PACKET_HEADER + 1] = tempCRC & 0xFF;
}

//--------------------------------------------------------
static uint8_t Ymodem_WaitACK(uint8_t ackchr, uint8_t tmo)
{
  uint8_t receivedC[2];
  uint32_t errors = 0;

  do {
    if (Receive_Byte(&receivedC[0], NAK_TIMEOUT) == 0) {
      if (receivedC[0] == ackchr) {
        return 1;
      }
      else if (receivedC[0] == CA) {
        send_CA();
        return 2; // CA received, Sender abort
      }
      else if (receivedC[0] == NAK) {
        return 3;
      }
      else {
        return 4;
      }
    }
    else {
      errors++;
    }
    luaWdgReload();
  }while (errors < tmo);
  return 0;
}


//--------------------------------------------------------------------------
static uint8_t Ymodem_Transmit (const char* sendFileName, uint32_t sizeFile)
{
  uint8_t packet_data[PACKET_1K_SIZE + PACKET_OVERHEAD];
  uint8_t filename[SPIFFS_OBJ_NAME_LEN];
  uint16_t blkNumber;
  uint8_t receivedC[1], i, err;
  uint32_t size = 0;

  for (i = 0; i < (SPIFFS_OBJ_NAME_LEN - 1); i++)
  {
    filename[i] = sendFileName[i];
  }
    
  while (MicoUartRecv( MICO_UART_1, &receivedC[0], 1, 10 ) == kNoErr) {};

  // Wait for response from receiver
  err = 0;
  do {
    luaWdgReload();
    Send_Byte(CRC16);
  } while (Receive_Byte(&receivedC[0], NAK_TIMEOUT) < 0 && err++ < 45);
  if (err >= 45 || receivedC[0] != CRC16) {
    send_CA();
    return 99;
  }
  
  // === Prepare first block and send it =======================================
  /* When the receiving program receives this block and successfully
   * opened the output file, it shall acknowledge this block with an ACK
   * character and then proceed with a normal YMODEM file transfer
   * beginning with a "C" or NAK tranmsitted by the receiver.
   */
  Ymodem_PrepareIntialPacket(&packet_data[0], filename, &sizeFile);
  do 
  {
    // Send Packet
    Ymodem_SendPacket(packet_data, PACKET_SIZE + PACKET_OVERHEAD);
    // Wait for Ack
    err = Ymodem_WaitACK(ACK, 10);
    if (err == 0 || err == 4) {
      send_CA();
      return 90;                  // timeout or wrong response
    }
    else if (err == 2) return 98; // abort
  }while (err != 1);

  // After initial block the receiver sends 'C' after ACK
  if (Ymodem_WaitACK(CRC16, 10) != 1) {
    send_CA();
    return 90;
  }
  
  // === Send file blocks ======================================================
  size = sizeFile;
  blkNumber = 0x01;
  
  // Resend packet if NAK  for a count of 10 else end of communication
  while (size)
  {
    // Prepare and send next packet
    Ymodem_PreparePacket(&packet_data[0], blkNumber, size);
    do
    {
      Ymodem_SendPacket(packet_data, PACKET_1K_SIZE + PACKET_OVERHEAD);
      // Wait for Ack
      err = Ymodem_WaitACK(ACK, 10);
      if (err == 1) {
        blkNumber++;
        if (size > PACKET_1K_SIZE) size -= PACKET_1K_SIZE; // Next packet
        else size = 0; // Last packet sent
      }
      else if (err == 0 || err == 4) {
        send_CA();
        return 90;                  // timeout or wrong response
      }
      else if (err == 2) return 98; // abort
    }while(err != 1);
  }
  
  // === Send EOT ==============================================================
  Send_Byte(EOT); // Send (EOT)
  // Wait for Ack
  do 
  {
    // Wait for Ack
    err = Ymodem_WaitACK(ACK, 10);
    if (err == 3) {   // NAK
      Send_Byte(EOT); // Send (EOT)
    }
    else if (err == 0 || err == 4) {
      send_CA();
      return 90;                  // timeout or wrong response
    }
    else if (err == 2) return 98; // abort
  }while (err != 1);
  
  // === Receiver requests next file, prepare and send last packet =============
  if (Ymodem_WaitACK(CRC16, 10) != 1) {
    send_CA();
    return 90;
  }

  Ymodem_PrepareLastPacket(&packet_data[0]);
  do 
  {
    Ymodem_SendPacket(packet_data, PACKET_SIZE + PACKET_OVERHEAD); // Send Packet
    // Wait for Ack
    err = Ymodem_WaitACK(ACK, 10);
    if (err == 0 || err == 4) {
      send_CA();
      return 90;                  // timeout or wrong response
    }
    else if (err == 2) return 98; // abort
  }while (err != 1);
  
  return 0; // file transmitted successfully
}


//==================================
static int file_recv( lua_State* L )
{
  int32_t fsize = 0;
  uint8_t c, gnm;
  char fnm[SPIFFS_OBJ_NAME_LEN];
  char buff[LUAL_BUFFERSIZE];
  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;
  uint32_t total, used;

  SPIFFS_info(&fs, &total, &used);
  if(total>2000000 || used>2000000 || used > total)
  {
    return luaL_error(L, "file system error");;
  }

  gnm = 0;
  if (lua_gettop(L) == 1 && lua_type( L, 1 ) == LUA_TSTRING) {
    size_t len;
    const char *fname = luaL_checklstring( L, 1, &len );
    if (len > 0 && len < SPIFFS_OBJ_NAME_LEN) {
      // use given file name
      for (c=0; c<len; c++) {
        fnm[c] = fname[c];
      }
      fnm[len] = '\0';
      gnm = 1;
    }
  }

  if (FILE_NOT_OPENED != file_fd) {
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
  }

  l_message(NULL,"Start Ymodem file transfer...");

  while (MicoUartRecv( MICO_UART_1, &c, 1, 10 ) == kNoErr) {}
  
  fsize = Ymodem_Receive(fnm, total-used-10000, gnm);
  
  luaWdgReload();
  mico_thread_msleep(500);
  while (MicoUartRecv( MICO_UART_1, &c, 1, 10 ) == kNoErr) {}

  if (FILE_NOT_OPENED != file_fd) {
    SPIFFS_fflush(&fs,file_fd);
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
  }

  mico_thread_msleep(500);
  if (fsize > 0)
  {
    sprintf(buff,"\r\nReceived successfully, %d\r\n",fsize);
    l_message(NULL,buff);
  }
  else if (fsize == -1)
  {
    l_message(NULL,"\r\nFile write error!\r\n");
  }
  else if (fsize == -2)
  {
    l_message(NULL,"\r\nFile open error!\r\n");
  }
  else if (fsize == -3)
  {
    l_message(NULL,"\r\nAborted.\r\n");
  }
  else if (fsize == -4)
  {
    l_message(NULL,"\r\nFile size too big, aborted.\r\n");
  }
  else
  {
    l_message(NULL,"\r\nReceive failed!");
  }
  
  if (fsize > 0) {
    SPIFFS_opendir(&fs, "/", &d);
    while ((pe = SPIFFS_readdir(&d, pe))) {
      sprintf(buff," %-32s size: %i", pe->name, pe->size);
      l_message(NULL,buff);
    }
    SPIFFS_closedir(&d);
  }

  return 0;
}


//==================================
static int file_send( lua_State* L )
{
  int8_t res = 0;
  int8_t newname = 0;
  uint8_t c;
  spiffs_stat s;
  const char *fname;
  const char *newfname;
  size_t len;
  char buff[LUAL_BUFFERSIZE];

  fname = luaL_checklstring( L, 1, &len );
  
  if( len > SPIFFS_OBJ_NAME_LEN )
    return luaL_error(L, "filename too long");
  
  if(FILE_NOT_OPENED!=file_fd){
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
  }
  
  if (lua_gettop(L) == 2 && lua_type( L, 2 ) == LUA_TSTRING) {
    size_t len;
    newfname = luaL_checklstring( L, 2, &len );
    newname = 1;
  }

  // Open the file
  file_fd = SPIFFS_open(&fs,(char*)fname,mode2flag("r"),0);
  if(file_fd < FILE_NOT_OPENED){
    file_fd = FILE_NOT_OPENED;
    l_message(NULL,"Error opening file.");
    return 0;
  }

  // Get file size
  SPIFFS_fstat(&fs, file_fd, &s);
  if (newname == 1) {
    sprintf(buff,"sending \"%s\" as \"%s\"\r\n", fname, newfname);
    l_message(NULL,buff);
    fname = newfname;
  }
  
  l_message(NULL,"Start Ymodem file transfer...");

  while (MicoUartRecv( MICO_UART_1, &c, 1, 10 ) == kNoErr) {}
  
  res = Ymodem_Transmit(fname, s.size);
  
  luaWdgReload();
  mico_thread_msleep(500);
  while (MicoUartRecv( MICO_UART_1, &c, 1, 10 ) == kNoErr) {}

  if(FILE_NOT_OPENED!=file_fd){
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
  }

  if (res == 0) {
    l_message(NULL,"\r\nFile sent successfuly.");
  }
  else if (res == 99) {
    l_message(NULL,"\r\nNo response.");
  }
  else if (res == 98) {
    l_message(NULL,"\r\nAborted.");
  }
  else {
    l_message(NULL,"\r\nError sending file.");
  }
  return 0;
}


//table = file.list()
//==================================
static int file_list( lua_State* L )
{
  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;

  lua_newtable( L );
  SPIFFS_opendir(&fs, "/", &d);
  while ((pe = SPIFFS_readdir(&d, pe))) {
    lua_pushinteger(L, pe->size);
    lua_setfield( L, -2, (char*)pe->name );
    //MCU_DBG("  %s size:%i\r\n", pe->name, pe->size);
  }
  SPIFFS_closedir(&d);
  return 1;
}

//file.slist() print lile list
//===================================
static int file_slist( lua_State* L )
{
  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;
  static char buff[LUAL_BUFFERSIZE];
  
  SPIFFS_opendir(&fs, "/", &d);
  while ((pe = SPIFFS_readdir(&d, pe))) {
    sprintf(buff," %s size: %i", pe->name, pe->size);
    l_message(NULL,buff);
  }
  SPIFFS_closedir(&d);
  return 0;
}

//file.format()
//====================================
static int file_format( lua_State* L )
{
  if(SPIFFS_mounted(&fs)==false) lua_spiffs_mount();
   
  SPIFFS_unmount(&fs);
  
  l_message(NULL,"formating, please wait...\r\n");
  int ret = SPIFFS_format(&fs);
  if(ret==SPIFFS_OK)
  {
    l_message(NULL,"format done\r\n");
    lua_spiffs_mount();    
  }
  else
    l_message(NULL,"format error\r\n");
  return 0;
}

// file.open(filename, mode)
//==================================
static int file_open( lua_State* L )
{
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );
  
  if( len > SPIFFS_OBJ_NAME_LEN )
    return luaL_error(L, "filename too long");
  
  if(FILE_NOT_OPENED!=file_fd){
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
  }
  
  const char *mode = luaL_optstring(L, 2, "r");
  file_fd = SPIFFS_open(&fs,(char*)fname,mode2flag((char*)mode),0);
  if(file_fd < FILE_NOT_OPENED){
    file_fd = FILE_NOT_OPENED;
    lua_pushnil(L);
  } else {
    lua_pushboolean(L, true);
  }
  return 1; 
}

// file.close()
//===================================
static int file_close( lua_State* L )
{
  if(FILE_NOT_OPENED!=file_fd){
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
  }
  return 0;  
}

// file.write("string")
//===================================
static int file_write( lua_State* L )
{
  if(FILE_NOT_OPENED==file_fd)
    return luaL_error(L, "open a file first");
  
  size_t len;
  const char *s = luaL_checklstring(L, 1, &len);
  
  if(SPIFFS_write(&fs,file_fd, (char*)s, len)<0)
  {//failed
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
    lua_pushnil(L);
  }
  else//success
    lua_pushboolean(L, true);
  return 1;
}

// file.writeline("string")
//=======================================
static int file_writeline( lua_State* L )
{
  if(FILE_NOT_OPENED==file_fd)
    return luaL_error(L, "open a file first");
  
  size_t len;
  const char *s = luaL_checklstring(L, 1, &len);
  
  if(SPIFFS_write(&fs,file_fd, (char*)s, len)<0)
  {//failed
    lua_pushnil(L);
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
  }
  else
  {//success
     if(SPIFFS_write(&fs,file_fd, "\r\n", 2)<0)
     {
        SPIFFS_close(&fs,file_fd);
        file_fd = FILE_NOT_OPENED;
        lua_pushnil(L);
     }
     else
        lua_pushboolean(L, true);
  }
  return 1;
}

//-------------------------------------------------------------
static int file_g_read( lua_State* L, int n, int16_t end_char )
{
  if(n< 0 || n>LUAL_BUFFERSIZE) 
    n = LUAL_BUFFERSIZE;
  if(end_char < 0 || end_char >255)
    end_char = EOF;
  int ec = (int)end_char;
  
  static luaL_Buffer b;
  if(FILE_NOT_OPENED==file_fd)
    return luaL_error(L, "open a file first");

  luaL_buffinit(L, &b);
  char *p = luaL_prepbuffer(&b);
  int c = EOF;
  int i = 0;

  do{
    c = EOF;
    SPIFFS_read(&fs, (spiffs_file)file_fd, &c, 1);
    if(c==EOF)break;
    p[i++] = (char)(0xFF & c);
  }while((c!=EOF) && ((char)c !=(char)ec) && (i<n) );

#if 0
  if(i>0 && p[i-1] == '\n')
    i--;    /* do not include `eol' */
#endif
    
  if(i==0){
    luaL_pushresult(&b);  /* close buffer */
    return (lua_objlen(L, -1) > 0);  /* check whether read something */
  }

  luaL_addsize(&b, i);
  luaL_pushresult(&b);  /* close buffer */
  return 1;  /* read at least an `eol' */ 
}

// file.read()
// file.read() read all byte in file LUAL_BUFFERSIZE(512) max
// file.read(10) will read 10 byte from file, or EOF is reached.
// file.read('q') will read until 'q' or EOF is reached. 
//==================================
static int file_read( lua_State* L )
{
  unsigned need_len = LUAL_BUFFERSIZE;
  int16_t end_char = EOF;
  size_t el;
  
  if( lua_type( L, 1 ) == LUA_TNUMBER )
  {
    need_len = ( unsigned )luaL_checkinteger( L, 1 );
    if( need_len > LUAL_BUFFERSIZE ){
      need_len = LUAL_BUFFERSIZE;
    }
  }
  else if(lua_isstring(L, 1))
  {
    const char *end = luaL_checklstring( L, 1, &el );
    if(el!=1){
      return luaL_error( L, "wrong arg range" );
    }
    end_char = (int16_t)end[0];
  }
  return file_g_read(L, need_len, end_char);
}

// file.readline()
static int file_readline( lua_State* L )
{
  return file_g_read(L, LUAL_BUFFERSIZE, '\n');
}

//file.seek(whence, offset)
//=================================
static int file_seek (lua_State *L) 
{
  static const int mode[] = {SPIFFS_SEEK_SET, SPIFFS_SEEK_CUR, SPIFFS_SEEK_END};
  static const char *const modenames[] = {"set", "cur", "end", NULL};
  
  if(FILE_NOT_OPENED==file_fd)
    return luaL_error(L, "open a file first");
  
  int op = luaL_checkoption(L, 1, "cur", modenames);
  long offset = luaL_optlong(L, 2, 0);
  op = SPIFFS_lseek(&fs,file_fd, offset, mode[op]);
  if (op)
    lua_pushnil(L);  /* error */
  else
  {
    spiffs_fd *fd;
    spiffs_fd_get(&fs, file_fd, &fd);
    lua_pushinteger(L, fd->fdoffset);
  }
  return 1;
}

// file.flush()
//===================================
static int file_flush( lua_State* L )
{
  if(FILE_NOT_OPENED==file_fd)
    return luaL_error(L, "open a file first");
  if(SPIFFS_fflush(&fs,file_fd) == 0)
    lua_pushboolean(L, 1);
  else
    lua_pushnil(L);
  return 1;
}

// file.remove(filename)
//====================================
static int file_remove( lua_State* L )
{
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );
  if( len > SPIFFS_OBJ_NAME_LEN )
    return luaL_error(L, "filename too long");
  file_close(L);
  SPIFFS_remove(&fs, (char *)fname);
  return 0;  
}

// file.rename("oldname", "newname")
//====================================
static int file_rename( lua_State* L )
{
  size_t len;

  if(FILE_NOT_OPENED!=file_fd){
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
  }

  const char *oldname = luaL_checklstring( L, 1, &len );
  if( len > SPIFFS_OBJ_NAME_LEN )
    return luaL_error(L, "filename too long");

  const char *newname = luaL_checklstring( L, 2, &len );
  if( len > SPIFFS_OBJ_NAME_LEN )
    return luaL_error(L, "filename too long");

  if(SPIFFS_OK==SPIFFS_rename(&fs, (char*)oldname, (char*)newname )){
    lua_pushboolean(L, 1);
  } else {
    lua_pushboolean(L, 0);
  }
  return 1;
}

//file.info()
//==================================
static int file_info( lua_State* L )
{
  uint32_t total, used;
  SPIFFS_info(&fs, &total, &used);
  if(total>2000000 || used>2000000 || used > total)
  {
    return luaL_error(L, "file system error");;
  }
  lua_pushinteger(L, total-used);
  lua_pushinteger(L, used);
  lua_pushinteger(L, total);
  return 3;
}

//file.state()
//===================================
static int file_state( lua_State* L )
{
  if(FILE_NOT_OPENED==file_fd)
  return luaL_error(L, "open a file first");
  
  spiffs_stat s;
  SPIFFS_fstat(&fs, file_fd, &s);
  
  lua_pushstring(L,(char*)s.name);
  lua_pushinteger(L,s.size);
  return 2;
}

#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstring.h"
#include "lundump.h"
static int writer(lua_State* L, const void* p, size_t size, void* u)
{
  UNUSED(L);
  int file_fd = *( (int *)u );
  if ( FILE_NOT_OPENED == file_fd)
    return 1;
  //MCU_DBG("get fd:%d,size:%d\n", file_fd, size);

  if (size != 0 && (size != SPIFFS_write(&fs,file_fd, (char *)p, size)) )
    return 1;
  //MCU_DBG("write fd:%d,size:%d\n", file_fd, size);
  return 0;
}

//rewrite lauxlib.c:luaL_loadfile
#define toproto(L,i) (clvalue(L->top+(i))->l.p)
static int file_compile( lua_State* L )
{
  Proto* f;
  int file_fd = FILE_NOT_OPENED;
  size_t len;
  const char *fname = luaL_checklstring( L, 1, &len );
  if ( len > SPIFFS_OBJ_NAME_LEN )
    return luaL_error(L, "filename too long");

  char output[SPIFFS_OBJ_NAME_LEN];
  strcpy(output, fname);
  // check here that filename end with ".lua".
  if (len < 4 || (strcmp( output + len - 4, ".lua") != 0) )
    return luaL_error(L, "not a .lua file");

  output[strlen(output) - 2] = 'c';
  output[strlen(output) - 1] = '\0';
  
  if (luaL_loadfile(L, fname) != 0) {
    return luaL_error(L, lua_tostring(L, -1));
  }

  f = toproto(L, -1);

  int stripping = 1;      /* strip debug information? */

  file_fd = SPIFFS_open(&fs,(char*)output,mode2flag("w+"),0);
  if (file_fd < FILE_NOT_OPENED)
  {
    return luaL_error(L, "cannot open/write to file");
  }

  lua_lock(L);
  int result = luaU_dump(L, f, writer, &file_fd, stripping);
  lua_unlock(L);

  SPIFFS_fflush(&fs,file_fd);
  SPIFFS_close(&fs,file_fd);
  file_fd =FILE_NOT_OPENED;

  if (result == LUA_ERR_CC_INTOVERFLOW) {
    return luaL_error(L, "value too big or small for target integer type");
  }
  if (result == LUA_ERR_CC_NOTINTEGER) {
    return luaL_error(L, "target lua_Number is integral but fractional value found");
  }

  return 0;
}

#define MIN_OPT_LEVEL   2
#include "lrodefs.h"
const LUA_REG_TYPE file_map[] =
{
  { LSTRKEY( "list" ), LFUNCVAL( file_list ) },
  { LSTRKEY( "slist" ), LFUNCVAL( file_slist ) },
  { LSTRKEY( "format" ), LFUNCVAL( file_format ) },
  { LSTRKEY( "open" ), LFUNCVAL( file_open ) },
  { LSTRKEY( "close" ), LFUNCVAL( file_close ) },
  { LSTRKEY( "write" ), LFUNCVAL( file_write ) },
  { LSTRKEY( "writeline" ), LFUNCVAL( file_writeline ) },
  { LSTRKEY( "read" ), LFUNCVAL( file_read ) },
  { LSTRKEY( "readline" ), LFUNCVAL( file_readline ) },
  { LSTRKEY( "remove" ), LFUNCVAL( file_remove ) },
  { LSTRKEY( "seek" ), LFUNCVAL( file_seek ) },
  { LSTRKEY( "flush" ), LFUNCVAL( file_flush ) },
  { LSTRKEY( "rename" ), LFUNCVAL( file_rename ) },
  { LSTRKEY( "info" ), LFUNCVAL( file_info ) },
  { LSTRKEY( "state" ), LFUNCVAL( file_state ) },
  { LSTRKEY( "compile" ), LFUNCVAL( file_compile ) },
  { LSTRKEY( "recv" ), LFUNCVAL( file_recv ) },
  { LSTRKEY( "send" ), LFUNCVAL( file_send ) },
#if LUA_OPTIMIZE_MEMORY > 0
#endif  
  {LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_file(lua_State *L)
{
  //lua_spiffs_mount();  
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else  
  luaL_register( L, EXLIB_FILE, file_map );
  return 1;
#endif
}
