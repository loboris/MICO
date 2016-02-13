
#include "MiCO.h" 
#include "mico_system.h"
#include "time.h" 
#include "platform_config.h"
#include "CheckSumUtils.h"
#include "lua.h"
#include "lauxlib.h"
#include "MQTTClient.h"

extern platform_uart_driver_t platform_uart_drivers[];
extern const platform_uart_t  platform_uart_peripherals[];
extern unsigned char boot_reason;
extern void _timer_net_handle( lua_State* gL );
extern void _do_freeBuf(uint8_t id);
//extern uint8_t *MQTT_topicbuf;
//extern uint8_t *MQTT_msgbuf;

#define DEFAULT_WATCHDOG_TIMEOUT        10*1000  // 10 seconds
#define main_log(M, ...) custom_log("main", M, ##__VA_ARGS__)

#define LUA_PARAMS_ID   0xA7
#define LUA_UART        (MICO_UART_1)
#define INBUF_SIZE      256
#define OUTBUF_SIZE     1024

uint8_t _lua_redir = 0;
char * _lua_redir_buf = NULL;
uint16_t _lua_redir_ptr = 0;

//----------------------------------------
lua_system_param_t lua_system_param =
{
  .ID = LUA_PARAMS_ID,
  .use_wwdg   = 0,
  .wdg_tmo    = DEFAULT_WATCHDOG_TIMEOUT,
  .stack_size = 20*1024,
  .inbuf_size = INBUF_SIZE,
  .baud_rate  = 115200,
  .parity     = NO_PARITY,
  .init_file  = "",
  .crc = 0
};

static mico_thread_t lua_queue_thread = NULL;
mico_mutex_t  lua_queue_mut; // Used to synchronize the queue thread

static uint32_t wwdgCount = 0;


//----------------------------------
uint16_t _get_luaparamsCRC( void ) {
  CRC16_Context paramcrc;
  uint16_t crc = 0;
  uint8_t *p_id = &lua_system_param.ID;

  CRC16_Init( &paramcrc );
  CRC16_Update( &paramcrc, p_id, sizeof(lua_system_param_t)-sizeof(uint16_t) );
  CRC16_Final( &paramcrc, &crc );
  return crc;
}

//----------------------------------------------------
void luaWdgReload( void ) {
  if (lua_system_param.use_wwdg == 0) MicoWdgReload();
  else wwdgCount = 0;
}
//----------------------------------------------------

static uint8_t *lua_rx_data;
static ring_buffer_t lua_rx_buffer;
static mico_uart_config_t lua_uart_config =
{
  .baud_rate    = STDIO_UART_BAUDRATE,
  .data_width   = DATA_WIDTH_8BIT,
  .parity       = NO_PARITY,
  .stop_bits    = STOP_BITS_1,
  .flow_control = FLOW_CONTROL_DISABLED,
  .flags        = UART_WAKEUP_DISABLE,
};


//-----------------------------
int lua_putstr(const char *msg)
{
  if (msg[0] != 0) {
    if (_lua_redir == 0) MicoUartSend( LUA_UART, (const char*)msg, strlen(msg) );
    else if (_lua_redir == 1) {
      MicoUartSend( LUA_UART, "[[", 2 );
      MicoUartSend( LUA_UART, (const char*)msg, strlen(msg) );
      MicoUartSend( LUA_UART, "]]", 2 );
    }
    else MicoUartSend( LUA_UART, (const char*)msg, strlen(msg) );
  }
  return 0;
}

//----------------------------------
int lua_printf(const char *msg, ...)
{
  va_list ap; 
  char *pos, message[256]; 
  int sz; 
  int nMessageLen = 0;
  
  memset(message, 0, 256);
  pos = message;
  
  sz = 0;
  va_start(ap, msg);
  nMessageLen = vsnprintf(pos, 256 - sz, msg, ap);
  va_end(ap);
  
  if( nMessageLen<=0 ) return 0;
  
  lua_putstr((const char*)message);
  return 0;
}

//==========================
int lua_getchar(char *inbuf)
{
  if (MicoUartRecv(LUA_UART, inbuf, 1, 20) == 0)
    return 1;  //OK
  else
    return 0;  //err
}

extern char gWiFiSSID[];
extern char gWiFiPSW[];
//=========================================
static void do_queue_task(queue_msg_t* msg)
{
  if ((msg->source == TMR) || (msg->source == GPIO))
  { // === execute timer or gpio interrupt function ===
    if(msg->para2 == LUA_NOREF) return;
    lua_rawgeti(msg->L, LUA_REGISTRYINDEX, msg->para2);
    lua_call(msg->L, 0, 0);
    lua_gc(msg->L, LUA_GCCOLLECT, 0);
  }
  else if (msg->source == NETTMR)
  { // === execute net timer interrupt function ===
    _timer_net_handle(msg->L);
  }
  else if (msg->source == onMQTTmsg)
  { // === execute onMQTT msg function ===
    if (msg->para2 == LUA_NOREF) return;
    if ((msg->para3 == NULL) || (msg->para4 == NULL)) return;
    
    lua_rawgeti(msg->L, LUA_REGISTRYINDEX, msg->para2);
    lua_pushlstring(msg->L, (const char*)(msg->para3), msg->para1 >> 16);
    lua_pushinteger(msg->L, msg->para1 & 0xFFFF);
    lua_pushlstring(msg->L, (const char*)(msg->para4), msg->para1 & 0xFFFF);
    free(msg->para3);
    msg->para3 = NULL;
    free(msg->para4);
    msg->para4 = NULL;
    lua_call(msg->L, 3, 0);
    lua_gc(msg->L, LUA_GCCOLLECT, 0);
  }
  else if (msg->source == onMQTT)
  { // === execute onMQTT function ===
    //_mqtt_handler();
    if(msg->para2 == LUA_NOREF) return;
    lua_rawgeti(msg->L, LUA_REGISTRYINDEX, msg->para2);
    if ((msg->para1 >> 16) != 0) {
      lua_pushinteger(msg->L, msg->para1 & 0xFFFF);
      lua_pushinteger(msg->L, msg->para1 >> 16);
      lua_call(msg->L, 2, 0);
    }
    else {
      lua_pushinteger(msg->L, msg->para1);
      lua_call(msg->L, 1, 0);
    }
    lua_gc(msg->L, LUA_GCCOLLECT, 0);
  }
  else if ((msg->source == onUART1) || (msg->source == onUART2))
  { // === execute UART ON function ===
    if (msg->para2 == LUA_NOREF) return;
    if (msg->para3 == NULL) return;
    if (msg->L == NULL) {
      if (msg->source == onUART1) _do_freeBuf(1);
      else _do_freeBuf(2);
      return;
    }
    
    lua_rawgeti(msg->L, LUA_REGISTRYINDEX, msg->para2);
    lua_pushinteger(msg->L, msg->para1);
    lua_pushlstring(msg->L, (const char*)(msg->para3), msg->para1);
    if (msg->source == onUART1) _do_freeBuf(1);
    else _do_freeBuf(2);
    lua_call(msg->L, 2, 0);
    lua_gc(msg->L, LUA_GCCOLLECT, 0);
  }
  else if (msg->source == onFTP)
  { // === execute on FTP function ===
    if (msg->para2 == LUA_NOREF) return;
    if (msg->L == NULL) return;
    
    lua_rawgeti(msg->L, LUA_REGISTRYINDEX, msg->para2);
    if (msg->para3 == NULL) {
      lua_pushinteger(msg->L, msg->para1);
      lua_call(msg->L, 1, 0);
    }
    else {
      lua_pushinteger(msg->L, msg->para1);
      lua_pushlstring(msg->L, (const char*)(msg->para3), msg->para1);
      free(msg->para3);
      msg->para3 = NULL;
      lua_call(msg->L, 2, 0);
    }
    lua_gc(msg->L, LUA_GCCOLLECT, 0);
  }
  else if(msg->source == USER)
  { // === execute user function ===
    if(msg->para2 == LUA_NOREF) return;
    lua_rawgeti(msg->L, LUA_REGISTRYINDEX, msg->para2);
    lua_pushstring(msg->L, "User function");
    lua_call(msg->L, 1, 0);
    luaL_unref(msg->L, LUA_REGISTRYINDEX, msg->para2);
    lua_gc(msg->L, LUA_GCCOLLECT, 0);
  }
  else if(msg->source == WIFI)
  { // === execute wifi function ===
    if(msg->para2 == LUA_NOREF || msg->para1 > 5) return;
      lua_rawgeti(msg->L, LUA_REGISTRYINDEX, msg->para2);
    switch(msg->para1)
    {
      case 0:lua_pushstring(msg->L, "STATION_UP");lua_call(msg->L, 1, 0);break;
      case 1:lua_pushstring(msg->L, "STATION_DOWN");lua_call(msg->L, 1, 0);break;
      case 2:lua_pushstring(msg->L, "AP_UP");lua_call(msg->L, 1, 0);break;
      case 3:lua_pushstring(msg->L, "AP_DOWN");lua_call(msg->L, 1, 0);break;
      case 4:lua_pushstring(msg->L, "ERROR");lua_call(msg->L, 1, 0);break;
      case 5:
            if(gWiFiSSID[0]==0x00){
                lua_pushnil(msg->L);lua_pushnil(msg->L);
              }
              else{
                lua_pushstring(msg->L,gWiFiSSID);lua_pushstring(msg->L,gWiFiPSW);
              }
              lua_call(msg->L, 2, 0);
            break;
    default:lua_pushstring(msg->L, "ERROR");lua_call(msg->L, 1, 0);break;
    }
    lua_gc(msg->L, LUA_GCCOLLECT, 0);
  }
}

mico_queue_t os_queue;
//================================
static void queue_thread(void*arg)
{
  UNUSED_PARAMETER( arg );
  OSStatus err;
  queue_msg_t queue_msg={0,NULL,0,0};
  
  while(1)
  {
    //Wait until queue has data
    err = mico_rtos_pop_from_queue( &os_queue, &queue_msg, MICO_WAIT_FOREVER);
    require_noerr( err, exit );
    mico_rtos_lock_mutex(&lua_queue_mut);
    do_queue_task(&queue_msg);
    mico_rtos_unlock_mutex(&lua_queue_mut);
  }
exit:
  lua_printf("queue_thread error\r\n");
  mico_rtos_delete_thread( NULL );  
}

//=================================================================
int readline4lua(const char *prompt, char *buffer, int buffer_size)
{
  char ch;
  int line_position;
    
  mico_rtos_unlock_mutex(&lua_queue_mut);
  _lua_redir = 0;
  
start:
  lua_printf(prompt); // show prompt
  line_position = 0;
  memset(buffer, 0, buffer_size);
  while (1)
  {
    luaWdgReload();
    while (lua_getchar(&ch) == 1) {
      if (ch == '\r') {
      // CR key
        char next;
        if (lua_getchar(&next)== 1) ch = next;
      }
      else if (ch == 0x7f || ch == 0x08) {
      // backspace key
        if (line_position > 0) {
          lua_printf("%c %c", ch, ch);
          line_position--;
        }
        buffer[line_position] = 0;
        continue;
      }
      else if (ch == 0x04) {
      // EOF(ctrl+d)
        if (line_position == 0) {
          mico_rtos_lock_mutex(&lua_queue_mut);
          return 0; // No input which makes lua interpreter close
        }
        else continue;
      }            
      if (ch == '\r' || ch == '\n') {
      // end of line
        buffer[line_position] = 0;
        lua_printf("\r\n"); //doit
        if (line_position == 0) goto start; // Get a empty line, then go to get a new line
        else {
          buffer[line_position+1] = 0;
          luaWdgReload();
          mico_rtos_lock_mutex(&lua_queue_mut);
          return line_position;
        }
      }
      if (ch < 0x20 || ch >= 0x80) continue; // other control character or not an acsii character

      lua_printf("%c", ch);       // character echo
      buffer[line_position] = ch; // put received character in buffer
      ch = 0;
      line_position++;
      if (line_position >= buffer_size) {
        // it's a large line, discard it
        goto start;
      }
   }
   // nothing is received
  }
}

//=====================================
//*** Main Lua thread *****************
//=====================================
static void lua_main_thread(void *data)
{
  //lua setup  
  char *argv[] = {"lua", NULL};
  
  // === Execute the main Lua loop ===
  lua_main(1, argv);
  
  // === Error happened  =============
  lua_printf("lua exited, reboot\r\n");
  
  if (lua_system_param.use_wwdg != 0) {
    MicoWdgInitialize( lua_system_param.wdg_tmo);
  }
  mico_thread_msleep(500);
  free(lua_rx_data);
  lua_rx_data = NULL;
  MicoSystemReboot();

  mico_rtos_delete_thread(NULL);
}
//=====================================


//==================================================
void TIM1_BRK_TIM9_IRQHandler(void)
{
  if (TIM_GetITStatus(TIM9, TIM_IT_Update) != RESET)
  {
    TIM_ClearITPendingBit(TIM9, TIM_IT_Update);
    if (wwdgCount < lua_system_param.wdg_tmo) {
      WWDG_SetCounter(0x7E);
    }
    wwdgCount += 20; // +20 msec
  }
}
//==================================================


// === TIM9 works as watchdog timer, resets WWDG every 20 msec ===
//---------------------------
static void wwdg_Config(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure; // Time base structure

  // Enable the TIM9 global Interrupt
  NVIC_InitStructure.NVIC_IRQChannel = TIM1_BRK_TIM9_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
   
  // TIM9 clock enable
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM9, ENABLE);
  // Time base configuration
  TIM_TimeBaseStructure.TIM_Period = 40000 - 1; // 1 MHz down to 250 Hz (40 ms)
  TIM_TimeBaseStructure.TIM_Prescaler = 50 - 1; // 50 MHz Clock down to 1 MHz
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInit(TIM9, &TIM_TimeBaseStructure);
  // TIM IT enable
  TIM_ITConfig(TIM9, TIM_IT_Update, ENABLE);

  // --- configure window watchdog (WWDG) ---
  // Enable WWDG clock
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_WWDG, ENABLE);
  // WWDG clock counter = (PCLK1 (50MHz)/4096)/8 = 1525 Hz (~655.36 us)
  WWDG_SetPrescaler(WWDG_Prescaler_8);
  // Set Window value to 127; WWDG counter should be refreshed only when the counter
  // is below 127 (and greater than 64) otherwise a reset will be generated
  WWDG_SetWindowValue(0x7F);
  // Enable WWDG and set counter value to 127, WWDG timeout = ~656 us * 64 = 42 ms 
  // In this case the refresh window is: ~655.36 * 64 = ~42ms

  WWDG_Enable(0x7E);     // start WWDG reset timer
  TIM_Cmd(TIM9, ENABLE); // and enable TIM9 counter
}


//===========================
int application_start( void )
{
//start
  struct tm currentTime;
  uint32_t lua_param_offset = 0x0;
  uint16_t crc = 0;
  uint8_t *p_id = &lua_system_param.ID;
  char *p_f = &lua_system_param.init_file[0];
  mico_rtc_time_t ttime;
  uint8_t prmstat = 0;
    
  platform_check_bootreason();
  MicoInit();

  // Read parameters from flash
  lua_param_offset = 0;
  MicoFlashRead(MICO_PARTITION_PARAMETER_1, &lua_param_offset , p_id, sizeof(lua_system_param_t));
  crc = _get_luaparamsCRC();

  if (lua_system_param.ID != LUA_PARAMS_ID || crc != lua_system_param.crc)  {
    lua_system_param.ID = LUA_PARAMS_ID;
    lua_system_param.use_wwdg = 0;
    lua_system_param.wdg_tmo    = DEFAULT_WATCHDOG_TIMEOUT;
    lua_system_param.stack_size = 10*1024;
    lua_system_param.inbuf_size = INBUF_SIZE;
    lua_system_param.baud_rate = 115200;
    lua_system_param.parity = NO_PARITY;
    sprintf(p_f,"");
    lua_system_param.crc = _get_luaparamsCRC();
    MicoFlashErase(MICO_PARTITION_PARAMETER_1, 0, sizeof(lua_system_param_t));
    lua_param_offset = 0;
    MicoFlashWrite(MICO_PARTITION_PARAMETER_1, &lua_param_offset, p_id, sizeof(lua_system_param_t));
    
  }
  else {
    prmstat = 1;
  }

  //usrinterface
  lua_rx_data = (uint8_t*)malloc(lua_system_param.inbuf_size);
  ring_buffer_init( (ring_buffer_t*)&lua_rx_buffer, (uint8_t*)lua_rx_data, lua_system_param.inbuf_size );
  lua_uart_config.baud_rate = lua_system_param.baud_rate;
  lua_uart_config.parity = (platform_uart_parity_t)lua_system_param.parity;
  
  MicoUartFinalize( STDIO_UART );
  //MicoUartInitialize( STDIO_UART, &lua_uart_config, (ring_buffer_t*)&lua_rx_buffer );
  platform_uart_init( &platform_uart_drivers[LUA_UART], &platform_uart_peripherals[LUA_UART], &lua_uart_config, (ring_buffer_t*)&lua_rx_buffer );

  lua_printf( "\r\nWiFiMCU Lua starting...(Free memory %d/%d)\r\n",MicoGetMemoryInfo()->free_memory,MicoGetMemoryInfo()->total_memory);
  lua_printf("  Lua params: ");
  if (prmstat) lua_printf( "OK");
  else lua_printf( "BAD, initialized");
  lua_printf("\r\n    Watchdog: ");
  if (lua_system_param.use_wwdg == 0) lua_printf("IWDT");
  else lua_printf( "WWDG");
  
  lua_printf("\r\n Boot reason: ");
  switch(boot_reason)
  {
    case BOOT_REASON_NONE:       lua_printf("NONE"); break;
    case BOOT_REASON_SOFT_RST:   lua_printf("SOFT_RST"); break;
    case BOOT_REASON_PWRON_RST:  lua_printf("PWRON_RST"); break;
    case BOOT_REASON_EXPIN_RST:  lua_printf("EXPIN_RST"); break;
    case BOOT_REASON_WDG_RST:    lua_printf("WDG_RST"); break;
    case BOOT_REASON_WWDG_RST:   lua_printf("WWDG_RST"); break;
    case BOOT_REASON_LOWPWR_RST: lua_printf("LOWPWR_RST"); break;
    case BOOT_REASON_BOR_RST:    lua_printf("BOR_RST"); break;
    default: lua_printf("?"); break;
  }
  lua_printf( "\r\n");

  if( MicoRtcGetTime(&ttime) == kNoErr ){
    currentTime.tm_sec = ttime.sec;
    currentTime.tm_min = ttime.min;
    currentTime.tm_hour = ttime.hr;
    currentTime.tm_mday = ttime.date;
    currentTime.tm_wday = ttime.weekday;
    currentTime.tm_mon = ttime.month - 1;
    currentTime.tm_year = ttime.year + 100; 
    lua_printf("Current Time: %s",asctime(&currentTime)); 
  }
  
  // === watch dog ==============================
  if (lua_system_param.use_wwdg == 0) {
    // === using IWDG ===
    MicoWdgInitialize( lua_system_param.wdg_tmo);
  }
  else {
    // === using WWDG
    wwdg_Config();
  }

  // Create queue for interrupt servicing  
  mico_rtos_init_mutex(&lua_queue_mut);
  mico_rtos_init_queue( &os_queue, "queue", sizeof(queue_msg_t), 10 );
  // Create and start queue thread
  if (mico_rtos_create_thread(&lua_queue_thread, MICO_APPLICATION_PRIORITY, "queue", queue_thread, lua_system_param.stack_size / 5 * 2, NULL ) != kNoErr) {
    lua_printf("error creating queue thread\r\n");
  }

  // Create and start main Lua thread
  mico_rtos_create_thread(NULL, MICO_DEFAULT_WORKER_PRIORITY, "lua_main_thread", lua_main_thread, lua_system_param.stack_size / 5 * 3, 0);

  mico_rtos_delete_thread(NULL);
  return 0;
 }