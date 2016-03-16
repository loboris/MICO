/**
 * uart.c
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"
#include "mico_platform.h"

extern void luaWdgReload( void );
extern int lua_putstr(const char *msg);
extern const char  wifimcu_gpio_map[];
extern mico_queue_t os_queue;

#define NUM_GPIO 18
static int platform_gpio_exists( unsigned pin )
{
  return pin < NUM_GPIO;
}
static int platform_uart_exists( unsigned id )
{
  if ((id==1) || (id==2)) return true;
  else return false;
}

static lua_State *gL            = NULL;
static int usr_uart_cb_ref      = LUA_NOREF;
#define LUA_USR_UART              (MICO_UART_2)
#define USR_UART_LENGTH           512
#define USR_INBUF_SIZE            USR_UART_LENGTH
#define USR_OUTBUF_SIZE           USR_UART_LENGTH
static uint8_t *lua_usr_rx_data = NULL;
static ring_buffer_t lua_usr_rx_buffer;
static uint8_t usrUART_init = 0;

static mico_uart_config_t lua_usr_uart_config =
{
  .baud_rate    = 115200,
  .data_width   = DATA_WIDTH_8BIT,
  .parity       = NO_PARITY,
  .stop_bits    = STOP_BITS_1,
  .flow_control = FLOW_CONTROL_DISABLED,
  .flags        = UART_WAKEUP_DISABLE,
};

static mico_thread_t *plua_usr_usart_thread=NULL;

typedef struct {
  int         usr_uart_cb_ref;
  lua_State   *gL;
  
  uint8_t     init;
  uint32_t    baud_rate;
  uint16_t    period;
  uint8_t     parity;
  uint8_t     stop_bits;
  uint8_t     nbits;
  mico_gpio_t tx_pin;
  mico_gpio_t rx_pin;
  
  uint8_t   *tx_buf;
  uint16_t  tx_len;
  uint16_t  tx_ptr;
  uint16_t  tx_byte;
  uint16_t  tx_bit;
  uint16_t  tx_bmask;

  uint8_t   *rx_buf;
  uint16_t  rx_len;
  uint16_t  rx_ptr;
  uint16_t  rx_first;
  uint16_t  rx_byte;
  uint16_t  rx_bit;
  uint16_t  rx_stopmask;
  uint16_t  rx_nerr;
  uint8_t   rx_npar;
} swuart_t;

static swuart_t swUART =
{
  .usr_uart_cb_ref = LUA_NOREF,
  .gL         = NULL,
  
  .init       = 0,
  .baud_rate  = 115200,
  .period     = (50000000 / 115200),
  .parity     = NO_PARITY,
  .stop_bits  = STOP_BITS_2,
  .nbits      = 8,
  .tx_pin     = (mico_gpio_t)255,
  .rx_pin     = (mico_gpio_t)255,
  
  .tx_buf     = NULL,
  .tx_len     = 0,
  .tx_ptr     = 0,
  .tx_byte    = 0,
  .tx_bit     = 0x0001,
  .tx_bmask   = 0x0800,

  .rx_buf     = NULL,
  .rx_len     = 0,
  .rx_ptr     = 0,
  .rx_first   = 0,
  .rx_byte    = 0,
  .rx_bit     = 0x0001,
  .rx_stopmask   = 0x0200,
  .rx_nerr    = 0,
  .rx_npar    = 0,
};

// =============================================================================
// === Software emulated UART ==================================================
// =============================================================================

// == Start bit detection ==============
//--------------------------------------
static void _rx_irq_handler( void* arg )
{
  TIM_SetAutoreload(TIM11, (swUART.period / 2) - 85);
  TIM_SetCounter(TIM11, 0);
  TIM_ClearITPendingBit(TIM11, TIM_IT_Update);
  swUART.rx_bit = 0x001;
  swUART.rx_byte = 0;

  // start frame receive
  TIM_Cmd(TIM11, ENABLE);
  MicoGpioDisableIRQ(swUART.rx_pin);
}
//======================================

// == Byte receive timer ===============
//--------------------------------------
void TIM1_TRG_COM_TIM11_IRQHandler(void)
{

  if (TIM_GetITStatus(TIM11, TIM_IT_Update) != RESET)
  {
    TIM_ClearITPendingBit(TIM11, TIM_IT_Update);
    
    // --- Get bit value -------------------------------------------------------
    if (MicoGpioInputGet(swUART.rx_pin)) swUART.rx_byte |= swUART.rx_bit;
    // -------------------------------------------------------------------------
    
    if (swUART.rx_bit == 0x001) {
      // --- start bit received ------------------------------------------------
      // wait full bit time for the next bits
      TIM_SetAutoreload(TIM11, swUART.period);
      swUART.rx_npar = 0; // init bit '1' counter
      // -----------------------------------------------------------------------
    }  
    else if (swUART.rx_bit == swUART.rx_stopmask) {
      // --- complete byte (stop bit) received ---------------------------------
      // * Test start and stop bits *
      if (((swUART.rx_byte & 0x0001) == 0) && ((swUART.rx_byte & swUART.rx_stopmask) != 0)) {
        // * start & stop bits OK *
        // remove start bit
        swUART.rx_byte >>= 1;

        // * Test parity *
        swUART.rx_npar &= 1;
        if (swUART.parity == EVEN_PARITY) {
          if ((swUART.rx_npar == 1) && ((swUART.rx_byte & (1 << swUART.nbits)) == 0)) {
            // if odd number of 1 bits, parity bit has to be 1
            swUART.rx_byte |= 0x8000;
          }
        }
        else if (swUART.parity == ODD_PARITY) {
          if ((swUART.rx_npar == 0) && ((swUART.rx_byte & (1 << swUART.nbits)) == 0)) {
            // if even number of 1 bits, parity bit has to be 1
            swUART.rx_byte |= 0x8000;
          }
        }
        
        if (swUART.rx_len < USR_INBUF_SIZE) {
          if ((swUART.rx_byte & 0x8000) == 0) {
            // * parity OK or NO_PARITY *
            swUART.rx_byte &= ~(0xFFFF << swUART.nbits); // mask only data bits
            // save received byte
            *(swUART.rx_buf+swUART.rx_ptr) = (uint8_t)swUART.rx_byte;
            swUART.rx_len++;
            swUART.rx_ptr++;
            if (swUART.rx_ptr >= USR_INBUF_SIZE) swUART.rx_ptr = 0;
          }
          else { // parity error
            swUART.rx_nerr++;
          }
        }
      }
      else { // frame error
        swUART.rx_nerr++;
      }

      // Enable detection of the next start bit
      MicoGpioEnableIRQ(swUART.rx_pin, IRQ_TRIGGER_FALLING_EDGE, _rx_irq_handler, (void*)swUART.rx_pin);

      TIM_Cmd(TIM11, DISABLE);
      // -----------------------------------------------------------------------
    }
    else {
      // --- Data bit or stop bit ----------------------------------------------
      if (swUART.rx_bit <= (1 << swUART.nbits)) {
        if ((swUART.rx_byte & swUART.rx_bit) != 0) swUART.rx_npar++;
      }
      // -----------------------------------------------------------------------
    }
    swUART.rx_bit <<= 1; // next bit
  }
}

//------------------------
void TIM4_IRQHandler(void)
{
  if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET)
  {
    TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    
    if (swUART.tx_len > 0) {
      if (swUART.tx_bit == 0x001) {
        // --- prepare output ---
        swUART.tx_byte = (uint16_t)*(swUART.tx_buf+swUART.tx_ptr);
        if (swUART.parity != NO_PARITY) {
          uint8_t n = 0;
          uint8_t i = 0;
          for (i=0;i<swUART.nbits;i++) {
            if ((swUART.tx_byte & swUART.tx_bit)) n++;
            swUART.tx_bit <<= 1;
          }
          swUART.tx_bit = 0x001;
          n &= 0x01;
          // add parity bit to output
          if ((swUART.parity == EVEN_PARITY) && (n==1)) swUART.tx_byte |= (1 << swUART.nbits);
          else if ((swUART.parity == ODD_PARITY) && (n==0)) swUART.tx_byte |= (1 << swUART.nbits);
          // add stop bits to output
          swUART.tx_byte |= (0x03 << (swUART.nbits+1));
        }
        else {
          swUART.tx_byte |= (0x03 << swUART.nbits); // add stop bits to output
        }
        swUART.tx_byte <<= 1;    // add start bit to output
        TIM_SetCounter(TIM4, 0); // to get full bit length
      }
      
      if ((swUART.tx_byte & swUART.tx_bit)) {
        MicoGpioOutputHigh(swUART.tx_pin);
      }
      else MicoGpioOutputLow(swUART.tx_pin);
      
      swUART.tx_bit <<= 1;  // next bit mask
      if (swUART.tx_bit == swUART.tx_bmask) { // byte finished
        swUART.tx_bit = 0x001;
        swUART.tx_len--;
        swUART.tx_ptr++;
        if (swUART.tx_len == 0) TIM_Cmd(TIM4, DISABLE);   // transfer end
      }
    }
  }
}

//-------------------------------------------------------
static uint16_t swUART_get (uint8_t *buf, uint16_t len) {
  uint16_t i,n;
  
  if (swUART.rx_len == 0) return 0;
  
  n = 0;
  for (i=0;i<len;i++) {
    *(buf+i) = *(swUART.rx_buf+swUART.rx_first);
    n++;
    swUART.rx_first = (swUART.rx_first + 1) % USR_INBUF_SIZE; 
    swUART.rx_len--;
    if (swUART.rx_len == 0) break;
  }
  return n;
}


//-------------------------------
static void init_SW_UART (void) {
  NVIC_InitTypeDef NVIC_InitStructure;
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure; // Time base structure

  // == Configure TX timer (TIM4) ===
  // Enable TIM4 global Interrupt
  NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  // TIM4 clock enable
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
  // Time base configuration
  TIM_TimeBaseStructure.TIM_Period = swUART.period;
  TIM_TimeBaseStructure.TIM_Prescaler = 1;  // 50 MHz Clock
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);
  TIM_Cmd(TIM4, DISABLE);
  // TIM4 IT enable
  TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);

  // == Configure RX timer (TIM11) ===
  // Enable TIM11 global Interrupt
  NVIC_InitStructure.NVIC_IRQChannel = TIM1_TRG_COM_TIM11_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  // TIM11 clock enable
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM11, ENABLE);
  // Time base configuration
  TIM_TimeBaseStructure.TIM_Period = (swUART.period / 2) - 85;
  TIM_TimeBaseStructure.TIM_Prescaler = 1;  // 50 MHz Clock
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInit(TIM11, &TIM_TimeBaseStructure);
  TIM_Cmd(TIM11, DISABLE);
  // TIM11 IT enable
  TIM_ITConfig(TIM11, TIM_IT_Update, ENABLE);

  swUART.init = 1;
}

// =============================================================================

//------------------------------------------
static void lua_usr_usart_thread(void *data)
{
  uint16_t len1 = 0, len2 = 0;
  uint32_t lastTick1 = 0, lastTick2 = 0;

  while(1)
  {
    if ((usrUART_init == 1) && (usr_uart_cb_ref != LUA_NOREF)) {
      // UART initialized & function assigned
      if (len1 != MicoUartGetLengthInBuffer(LUA_USR_UART)) {
        // more data received
        lastTick1 = mico_get_time();
        len1 = MicoUartGetLengthInBuffer(LUA_USR_UART);
      }
      if (len1 > 0) {
        if ((mico_get_time() - lastTick1) >= 100) {
          len1 = MicoUartGetLengthInBuffer(LUA_USR_UART);
          queue_msg_t msg;
          msg.para3 = (uint8_t*)malloc(len1);
          if (msg.para3 != NULL) {
            MicoUartRecv(LUA_USR_UART, msg.para3, len1, 2);
            msg.L = gL;
            msg.source = onUART;
            msg.para1 = len1;
            msg.para2 = usr_uart_cb_ref;
            mico_rtos_push_to_queue( &os_queue, &msg,0);
          }
          len1 = 0;
        }
      }
    }

    if ((swUART.init == 1) && (swUART.usr_uart_cb_ref != LUA_NOREF)) {
      if (len2 != swUART.rx_len) {
        // more data received
        lastTick2 = mico_get_time();
        len2 = swUART.rx_len;
      }
      if (len2 > 0) {
        if ((mico_get_time() - lastTick2) >= 100) {
          len2 = swUART.rx_len;
          queue_msg_t msg;
          msg.para3 = (uint8_t*)malloc(len2);
          if (msg.para3 != NULL) {
            uint16_t len = swUART_get(msg.para3, len2);
            msg.L = swUART.gL;
            msg.source = onUART;
            msg.para1 = len2;
            msg.para2 = swUART.usr_uart_cb_ref;
            mico_rtos_push_to_queue( &os_queue, &msg, 0);
          }
          len2 = 0;
        }
      }
    }
    mico_thread_msleep(10);
  }
}

/*================================
  uart.setup(1,9600,'n',8,1)
  uart.setup(1,115200,'e',7,2,2,3)

baud: all supported
parity:
      n,NO_PARITY;
      o,ODD_PARITY;
      e,EVEN_PARITY
databits:
      5:DATA_WIDTH_5BIT,
      6:DATA_WIDTH_6BIT,
      7:DATA_WIDTH_7BIT,
      8:DATA_WIDTH_8BIT,
      9:DATA_WIDTH_9BIT
stopbits:
      1,STOP_BITS_1
      2,STOP_BITS_2
txpin: (sw UART only)
      GPIO pin no.
rxpin: (sw UART only)
      GPIO pin no.
================================*/
//===================================
static int uart_setup( lua_State* L )
{
  uint16_t id, databits=DATA_WIDTH_8BIT, parity=NO_PARITY, stopbits=STOP_BITS_1;
  uint32_t baud=9600;
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( uart, id );
  
  baud = luaL_checkinteger( L, 2 );
  
  size_t sl=0;
  char const *str = luaL_checklstring( L, 3, &sl );
  if(sl == 1 && strcmp(str, "n") == 0)
    parity = NO_PARITY;
  else if(sl == 1 && strcmp(str, "o") == 0)
    parity = ODD_PARITY;
  else if(sl == 1 && strcmp(str, "e") == 0)
    parity = EVEN_PARITY;
  else
    return luaL_error( L, "arg parity should be 'n' or 'o' or 'e' " );
  
  databits = luaL_checkinteger( L, 4 );
  if ((databits < 5) || (databits > 9))  
    return luaL_error( L, "arg databits should be 5~9 " );
  
  if (id == 1) {
         if (databits == 5) databits = DATA_WIDTH_5BIT;
    else if (databits == 6) databits = DATA_WIDTH_6BIT;
    else if (databits == 7) databits = DATA_WIDTH_7BIT;
    else if (databits == 8) databits = DATA_WIDTH_8BIT;
    else if (databits == 9) databits = DATA_WIDTH_9BIT;
  }
  
  stopbits = luaL_checkinteger( L, 5 );
  if ((stopbits < 1) || (stopbits > 2))  
    return luaL_error( L, "arg stopbits should be 1 or 2 " );

  if (id == 1) {
         if (stopbits == 1) stopbits = STOP_BITS_1;
    else if (stopbits == 2) stopbits = STOP_BITS_2;

    lua_usr_uart_config.baud_rate = baud;
    lua_usr_uart_config.parity=(platform_uart_parity_t)parity;
    lua_usr_uart_config.data_width =(platform_uart_data_width_t)databits;
    lua_usr_uart_config.stop_bits =(platform_uart_stop_bits_t)stopbits;
    
    if(lua_usr_rx_data !=NULL) free(lua_usr_rx_data);
    lua_usr_rx_data = (uint8_t*)malloc(USR_INBUF_SIZE);
    ring_buffer_init( (ring_buffer_t*)&lua_usr_rx_buffer, (uint8_t*)lua_usr_rx_data, USR_INBUF_SIZE );
    
    //MicoUartFinalize(LUA_USR_UART);
    MicoUartInitialize( LUA_USR_UART, &lua_usr_uart_config, (ring_buffer_t*)&lua_usr_rx_buffer );
    gL = L;
    usr_uart_cb_ref = LUA_NOREF;
    //if(plua_usr_usart_thread !=NULL) mico_rtos_delete_thread(plua_usr_usart_thread);
    if (plua_usr_usart_thread == NULL)
      mico_rtos_create_thread(plua_usr_usart_thread, MICO_DEFAULT_WORKER_PRIORITY, "lua_usr_usart_thread", lua_usr_usart_thread, 0x200, 0);
    usrUART_init = 1;
  }
  else {
    unsigned txpin = luaL_checkinteger( L, 6 );
    unsigned rxpin = luaL_checkinteger( L, 7 );

    if (swUART.init == 0) {
      MOD_CHECK_ID(gpio, txpin);
      MOD_CHECK_ID(gpio, rxpin);
      
      swUART.tx_pin = (mico_gpio_t)wifimcu_gpio_map[txpin];
      MicoGpioFinalize(swUART.tx_pin);
      MicoGpioInitialize(swUART.tx_pin,(mico_gpio_config_t)OUTPUT_PUSH_PULL);
      MicoGpioOutputHigh(swUART.tx_pin);
      
      swUART.rx_pin = (mico_gpio_t)wifimcu_gpio_map[rxpin];
      MicoGpioFinalize(swUART.rx_pin);
      MicoGpioInitialize(swUART.rx_pin, (mico_gpio_config_t)INPUT_PULL_UP);
      
      if (swUART.tx_buf !=NULL) free(swUART.tx_buf);
      swUART.tx_buf = (uint8_t*)malloc(USR_OUTBUF_SIZE);
      if (swUART.rx_buf !=NULL) free(swUART.rx_buf);
      swUART.rx_buf = (uint8_t*)malloc(USR_INBUF_SIZE);
    }

    MicoGpioDisableIRQ(swUART.rx_pin);
    TIM_Cmd(TIM11, DISABLE);
    TIM_ClearITPendingBit(TIM11, TIM_IT_Update);
    swUART.parity    = parity;
    swUART.nbits     = databits;
    swUART.stop_bits = stopbits;
    swUART.baud_rate = baud;
    swUART.period    = (uint16_t)((50000000 / swUART.baud_rate) - 1);
    swUART.rx_len    = 0;
    swUART.rx_ptr    = 0;
    swUART.rx_first  = 0;
    swUART.rx_nerr   = 0;
    swUART.rx_bit    = 0x001;
    swUART.rx_byte   = 0;
    if (swUART.parity != NO_PARITY) {
      swUART.rx_stopmask = (0x01 << (swUART.nbits+2));
      swUART.tx_bmask = (0x01 << (swUART.nbits+swUART.stop_bits+2));
    }
    else {
      swUART.rx_stopmask = (0x01 << (swUART.nbits+1));
      swUART.tx_bmask = (0x01 << (swUART.nbits+swUART.stop_bits+1));
    }
    
    init_SW_UART();
    swUART.gL = L;
    MicoGpioEnableIRQ(swUART.rx_pin, IRQ_TRIGGER_FALLING_EDGE, _rx_irq_handler, (void*)swUART.rx_pin);
    if (plua_usr_usart_thread == NULL)
      mico_rtos_create_thread(plua_usr_usart_thread, MICO_DEFAULT_WORKER_PRIORITY, "lua_usr_usart_thread", lua_usr_usart_thread, 0x200, 0);
  }
  
  return 0;
}

//uart.deinit(id)
//====================================
static int uart_deinit( lua_State* L )
{
  uint8_t id = luaL_checkinteger( L, 1 );

  if ((id == 1) && (usrUART_init == 0)) {
    return luaL_error( L, "UART not initialized" );
  }
  else if ((id == 2) && (swUART.init == 0)) {
    return luaL_error( L, "swUART not initialized" );
  }
  
  if (id == 1) {
    MicoUartFinalize(LUA_USR_UART);
    usr_uart_cb_ref = LUA_NOREF;
    //if (plua_usr_usart_thread !=NULL) mico_rtos_delete_thread(plua_usr_usart_thread);
    usrUART_init = 0;
  }
  else if (id == 2) {
    TIM_Cmd(TIM11, DISABLE);
    TIM_ClearITPendingBit(TIM11, TIM_IT_Update);
    MicoGpioDisableIRQ(swUART.rx_pin);
    MicoGpioFinalize(swUART.tx_pin);
    MicoGpioFinalize(swUART.rx_pin);
    
    if (swUART.tx_buf !=NULL) free(swUART.tx_buf);
    if (swUART.rx_buf !=NULL) free(swUART.rx_buf);
    swUART.init = 0;
  }
  return 0;
}

//uart.on(id,function(t))
//================================
static int uart_on( lua_State* L )
{
  uint16_t id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( uart, id );
  
  if ((id == 1) && (usrUART_init == 0)) {
    return luaL_error( L, "UART not initialized" );
  }
  else if ((id == 2) && (swUART.init == 0)) {
    return luaL_error( L, "swUART not initialized" );
  }

  size_t sl;
  const char *method = luaL_checklstring( L, 2, &sl );
  if (method == NULL)
    return luaL_error( L, "wrong arg type" );
  
  if ((sl == 4) && (strcmp(method, "data") == 0)) {
    if ((lua_type(L, 3) == LUA_TFUNCTION) || (lua_type(L, 3) == LUA_TLIGHTFUNCTION)) {
      if (id == 1) {
        lua_pushvalue(L, 3);
        if(usr_uart_cb_ref != LUA_NOREF) {
          luaL_unref(L, LUA_REGISTRYINDEX, usr_uart_cb_ref);
        }
        usr_uart_cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
      }
      else if (id == 2) {
        lua_pushvalue(L, 3);
        if(swUART.usr_uart_cb_ref != LUA_NOREF) {
          luaL_unref(L, LUA_REGISTRYINDEX, swUART.usr_uart_cb_ref);
        }
        swUART.usr_uart_cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
      }
    }
  }
  else {
    return luaL_error( L, "wrong arg type" );
  }
  
  return 0;
}

//uart.send(1,string1,number,...[stringn])
//==================================
static int uart_send( lua_State* L )
{
  uint16_t id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( uart, id );
  
  if ((id == 1) && (usrUART_init == 0)) {
    return luaL_error( L, "UART not initialized" );
  }
  else if ((id == 2) && (swUART.init == 0)) {
    return luaL_error( L, "swUART not initialized" );
  }

  const char* buf;
  size_t len;
  int total = lua_gettop( L ), s;
  uint16_t idx = 0;
  
  for( s = 2; s <= total; s ++ )
  {
    if( lua_type( L, s ) == LUA_TNUMBER )
    {
      len = lua_tointeger( L, s );
      if( len > 255 ) return luaL_error( L, "invalid number" );
      if (id == 1) {
        MicoUartSend( LUA_USR_UART,(char*)len,1);
      }
      else {
        if ((idx+1) < USR_OUTBUF_SIZE) {
          *(swUART.tx_buf+idx) = (uint8_t)len;
          idx++;
        }
      }
    }
    else
    {
      luaL_checktype( L, s, LUA_TSTRING );
      buf = lua_tolstring( L, s, &len );
      if (id == 1) {
        MicoUartSend( LUA_USR_UART, buf,len);
      }
      else {
        if ((idx+len) > USR_OUTBUF_SIZE) len = USR_OUTBUF_SIZE-idx;
        if (len > 0) {
          memcpy(swUART.tx_buf+idx, buf, len);
          idx += len;
        }
      }
    }
  }
  if ((id == 2) & (idx > 0)) {
    swUART.tx_len = idx;
    swUART.tx_ptr = 0;
    swUART.tx_bit = 0x001;
    TIM_SetCounter(TIM4, 0);
    // start transfer
    TIM_Cmd(TIM4, ENABLE);
  }
  
  return 0;
}

//==================================
static int uart_recv( lua_State* L )
{
  uint8_t id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( uart, id );

  if ((id == 1) && (usrUART_init == 0)) {
    return luaL_error( L, "UART not initialized" );
  }
  else if ((id == 2) && (swUART.init == 0)) {
    return luaL_error( L, "swUART not initialized" );
  }

  uint8_t buf[USR_OUTBUF_SIZE+1];
  size_t len,blen;
  
  if (id == 1) blen=MicoUartGetLengthInBuffer(LUA_USR_UART);
  else blen = swUART.rx_len;

  if (lua_gettop(L) >= 2) {
    len = luaL_checkinteger(L, 2);
    if (len > blen) len = blen;
  }
  else {
    len = blen;
  }
  
  if (id == 1) {
    if ((usrUART_init == 0) || (MicoUartGetLengthInBuffer(LUA_USR_UART) == 0)) {
      lua_pushstring(L, "[nil]");
    }
    else {  
      MicoUartRecv(LUA_USR_UART, &buf[0], len, 10);
      buf[len] = 0;
      lua_pushstring(L, (const char*)&buf[0]);
    }
  }
  else {
    if ((swUART.init == 0) || (swUART.rx_len == 0)) {
      lua_pushstring(L, "[nil]");
    }
    else {  
      len = swUART_get(&buf[0], len);
      buf[len] = 0;
      lua_pushstring(L, (const char*)&buf[0]);
      if (swUART.rx_len == 0) swUART.rx_nerr = 0;
    }
  }
  return 1;
}

//======================================
static int uart_recvstat( lua_State* L )
{
  uint8_t id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( uart, id );

  if ((id == 1) && (usrUART_init == 0)) {
    return luaL_error( L, "UART not initialized" );
  }
  else if ((id == 2) && (swUART.init == 0)) {
    return luaL_error( L, "swUART not initialized" );
  }

  uint16_t len;
  uint8_t  err = 0;
  if (id == 1) len=MicoUartGetLengthInBuffer(LUA_USR_UART);
  else {
    len = swUART.rx_len;
    if (len > 0) err = swUART.rx_nerr;
  }

  lua_pushinteger(L, len);
  lua_pushinteger(L, err);
  return 2;
}

//uart.getchar(uart_id,timeout,timer_id)
//=====================================
static int uart_getchar( lua_State* L )
{
  char ch[2];
  char buf[2];

  //uint16_t id = luaL_checkinteger( L, 1 );
  uint32_t tmo = luaL_checkinteger( L, 2 );
  //MOD_CHECK_ID( uart, id );

  uint32_t wt = 0;
  buf[0] = '\0';
  buf[1] = '\0';
  while (wt < tmo) {
    if (MicoUartRecv(MICO_UART_1, &ch, 1, 100) == 0) {
      buf[0] = ch[0];
      break;
    }
    wt += 100;
    luaWdgReload();
  }
  
  lua_pushstring(L, buf);
  return 1;
}
  
//uart.write(uart_id,c)
//===================================
static int uart_write( lua_State* L )
{
  const char* buf;
  size_t len;

  //uint16_t id = luaL_checkinteger( L, 1 );
  //MOD_CHECK_ID( uart, id );

  luaL_checktype( L, 2, LUA_TSTRING );
  buf = lua_tolstring( L, 2, &len );
  //MicoUartSend( MICO_UART_1, buf, 1);
  if (len == 1) lua_putstr(buf);
  return 0;
}
  

#define MIN_OPT_LEVEL   2
#include "lrodefs.h"
const LUA_REG_TYPE uart_map[] =
{
  { LSTRKEY( "setup" ), LFUNCVAL( uart_setup )},
  { LSTRKEY( "deinit" ), LFUNCVAL( uart_deinit )},
  { LSTRKEY( "on" ), LFUNCVAL( uart_on )},
  { LSTRKEY( "send" ), LFUNCVAL( uart_send )},
  { LSTRKEY( "recv" ), LFUNCVAL( uart_recv )},
  { LSTRKEY( "recvstat" ), LFUNCVAL( uart_recvstat )},
  { LSTRKEY( "getchar" ), LFUNCVAL( uart_getchar )},
  { LSTRKEY( "write" ), LFUNCVAL( uart_write )},
#if LUA_OPTIMIZE_MEMORY > 0
#endif      
  {LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_uart(lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else  
  luaL_register( L, EXLIB_UART, uart_map );
  return 1;
#endif
}
