/**
 * i2c.c
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"

#include "mico_platform.h"
#include "user_config.h"

extern const char wifimcu_gpio_map[];
#define NUM_GPIO 18
static int platform_gpio_exists( unsigned pin )
{
  return pin < NUM_GPIO;
}

static mico_i2c_device_t hw_i2c;
static uint8_t pinSDA = 0;
static uint8_t pinSCL = 0;
bool IIC_Init = false;
bool hw_IIC_Init = false;
static bool IIC_started = false;
#define I2C_speed 360  // Standard mode, 100 kHz

//-------------------------------------------------------------------------
// we use cycle counter for precise timing with software I2C
#define CYCLE_COUNTING_INIT() \
    do \
    { \
        /* enable DWT hardware and cycle counting */ \
        CoreDebug->DEMCR = CoreDebug->DEMCR | CoreDebug_DEMCR_TRCENA_Msk; \
        /* reset a counter */ \
        DWT->CYCCNT = 0;  \
        /* enable the counter */ \
        DWT->CTRL = (DWT->CTRL | DWT_CTRL_CYCCNTENA_Msk) ; \
    } \
    while(0)
//-------------------------------------------------------------------------

//-------------------------
static void IIC_Start(void)
{
  if ( IIC_started ) {
    // if started, do a restart cond
    // set SDA to 1
    MicoGpioOutputHigh( (mico_gpio_t)pinSDA);
    CYCLE_COUNTING_INIT();
    while (DWT->CYCCNT < (I2C_speed * 2)) ;

    // set SCL to 1
    MicoGpioOutputHigh( (mico_gpio_t)pinSCL);
    if (MicoGpioInputGet((mico_gpio_t)pinSCL) == 0) { // Allow for clock stretching
      while ((MicoGpioInputGet((mico_gpio_t)pinSCL) == 0) && (DWT->CYCCNT < 10000)) ;
      DWT->CYCCNT = 0;
    }
    while (DWT->CYCCNT < (I2C_speed * 2)) ;
  }

  //if( read_SDA() == 0 ) arbitration_lost();

  // set SDA to 0
  MicoGpioOutputLow( (mico_gpio_t)pinSDA);
  CYCLE_COUNTING_INIT();
  while (DWT->CYCCNT < (I2C_speed * 2)) ;

  // set SCL to 0
  MicoGpioOutputLow( (mico_gpio_t)pinSCL);
  CYCLE_COUNTING_INIT();
  while (DWT->CYCCNT < (I2C_speed * 2)) ;

  IIC_started = true;
}

//------------------------
static void IIC_Stop(void)
{
  // set SDA to 0
  MicoGpioOutputLow( (mico_gpio_t)pinSDA);
  CYCLE_COUNTING_INIT();
  while (DWT->CYCCNT < (I2C_speed * 2)) ;

  // set SCL to 1
  MicoGpioOutputHigh( (mico_gpio_t)pinSCL);
  CYCLE_COUNTING_INIT();
  if (MicoGpioInputGet((mico_gpio_t)pinSCL) == 0) { // Allow for clock stretching
    while ((MicoGpioInputGet((mico_gpio_t)pinSCL) == 0) && (DWT->CYCCNT < 10000)) ;
    DWT->CYCCNT = 0;
  }
  while (DWT->CYCCNT < (I2C_speed * 2)) ;

  // set SDA to 1
  MicoGpioOutputHigh( (mico_gpio_t)pinSDA);
  CYCLE_COUNTING_INIT();
  while (DWT->CYCCNT < (I2C_speed * 2)) ;
}

//-----------------------------------
static int IIC_Send_Byte(uint8_t txd)
{
  uint8_t t, data;   
  
  data = txd;
  // Send 8 bits, MSB first
  for(t=0;t<8;t++)
    {
      CYCLE_COUNTING_INIT();
      if ((data & 0x80) == 0x80) MicoGpioOutputHigh( (mico_gpio_t)pinSDA);
      else MicoGpioOutputLow( (mico_gpio_t)pinSDA);
      data <<= 1;
      // SDA change propagation delay, SCL=Low
      while (DWT->CYCCNT < I2C_speed) ;

      // Set SCL high to indicate a new valid SDA value is available
      MicoGpioOutputHigh( (mico_gpio_t)pinSCL);
      CYCLE_COUNTING_INIT();
      if (MicoGpioInputGet((mico_gpio_t)pinSCL) == 0) { // Allow for clock stretching
        while ((MicoGpioInputGet((mico_gpio_t)pinSCL) == 0) && (DWT->CYCCNT < 10000)) ;
        DWT->CYCCNT = 0;
      }
      // Wait for SDA value to be read by slave
      while (DWT->CYCCNT < I2C_speed) ;

      // Clear the SCL to low in preparation for next change
      MicoGpioOutputLow( (mico_gpio_t)pinSCL);
    }

  // Check ACK/NACK
  CYCLE_COUNTING_INIT();
  t = 1;
  MicoGpioOutputHigh( (mico_gpio_t)pinSDA);
  while (DWT->CYCCNT < I2C_speed) ;

  MicoGpioOutputHigh( (mico_gpio_t)pinSCL);
  CYCLE_COUNTING_INIT();
  if (MicoGpioInputGet((mico_gpio_t)pinSCL) == 0) { // Allow for clock stretching
    while ((MicoGpioInputGet((mico_gpio_t)pinSCL) == 0) && (DWT->CYCCNT < 10000)) ;
    DWT->CYCCNT = 0;
  }
  // Wait for ACK to be set by slave
  while (DWT->CYCCNT < I2C_speed) {
    if (MicoGpioInputGet((mico_gpio_t)pinSDA) == 0) t = 0;
  }

  // Clear the SCL to low in preparation for next change
  MicoGpioOutputLow( (mico_gpio_t)pinSCL);
  
  if (t == 0) return 0;
  else return -1;
}

//---------------------------------------------------------------------------------------
int _i2c_write(uint8_t id, uint16_t dev_adr, uint8_t* data, uint16_t count, uint16_t rep)
{
  uint16_t i, j;
  
  if (id == 0) { // === software i2c ===
    // Send start condition
    IIC_Start();
    // Send address
    int res = IIC_Send_Byte((uint8_t)(dev_adr << 1));
    if (res != 0) return -9;
    
    for ( i = 0; i < count; i++ ) {
      if (i == (count-1)) {
        // repeat last byte rep times
        for ( j = 0; j < rep; j++ ) {
          res = IIC_Send_Byte(*(data + i));
          if (res != 0) break; 
        }
        if (res != 0) break; 
      }
      else {
        res = IIC_Send_Byte(*(data + i));
        if (res != 0) break; 
      }
    }
    // Send stop condition
    IIC_Stop();

    return res;
  }
  else { // === hardware i2c ===
    hw_i2c.address = dev_adr;

    OSStatus err = kNoErr;
    mico_i2c_message_t i2c_msg = {NULL, NULL, 0, 0, 10, false};

    err = MicoI2cBuildTxMessage(&i2c_msg, data, count, 3);
    if (err != kNoErr) return -9;

    err = MicoI2cTransfer(&hw_i2c, &i2c_msg, 1, rep);
    if (err != kNoErr) return err;
    return 0;
  }
}

//ack=1 send ACK; ack=0 send nACK
//---------------------------------------------
static uint8_t IIC_Read_Byte(unsigned char ack)
{
  unsigned char i,receive=0;
  
  // Let the slave drive data
  MicoGpioOutputHigh( (mico_gpio_t)pinSDA);
  for(i=0;i<8;i++ )
  {
    // Wait for SDA value to be written by slave
    CYCLE_COUNTING_INIT();
    while (DWT->CYCCNT < I2C_speed) ;

    // Set SCL high
    MicoGpioOutputHigh( (mico_gpio_t)pinSCL);
    CYCLE_COUNTING_INIT();
    while (DWT->CYCCNT < I2C_speed) ;
    // SCL is high, read out bit
    if (MicoGpioInputGet((mico_gpio_t)pinSDA)) receive |= 1;
    receive <<= 1;
    // Set SCL low in preparation for next operation
    MicoGpioOutputLow( (mico_gpio_t)pinSCL);
  }					 

  // Set ACK/NACK
  CYCLE_COUNTING_INIT();
  if (!ack) MicoGpioOutputHigh( (mico_gpio_t)pinSDA);
  else MicoGpioOutputLow( (mico_gpio_t)pinSDA);

  // SDA change propagation delay, SCL=Low
  while (DWT->CYCCNT < I2C_speed) ;

  // Set SCL high to indicate a new valid SDA value is available
  MicoGpioOutputHigh( (mico_gpio_t)pinSCL);
  CYCLE_COUNTING_INIT();
  if (MicoGpioInputGet((mico_gpio_t)pinSCL) == 0) { // Allow for clock stretching
    while ((MicoGpioInputGet((mico_gpio_t)pinSCL) == 0) && (DWT->CYCCNT < 10000)) ;
    DWT->CYCCNT = 0;
  }
  // Wait for SDA value to be read by slave
  while (DWT->CYCCNT < I2C_speed) ;

  // Clear the SCL to low in preparation for next change
  MicoGpioOutputLow( (mico_gpio_t)pinSCL);

  return receive;
}


//i2c.setup(0, pinSDA, pinSCL)
//i2c.setup(1, adr_len, mode)
//==================================
static int i2c_setup( lua_State* L )
{
  unsigned id =  luaL_checkinteger( L, 1 );
  if ((id != 0) && (id != 1)) return luaL_error( L, "id should be assigend 0 or 1" );

  unsigned sda = luaL_checkinteger( L, 2 );
  unsigned scl = luaL_checkinteger( L, 3 );
  
  if (id == 0) {
    MOD_CHECK_ID( gpio, sda );
    MOD_CHECK_ID( gpio, scl );
    pinSDA = wifimcu_gpio_map[sda];
    pinSCL = wifimcu_gpio_map[scl];
    
    MicoGpioFinalize((mico_gpio_t)pinSDA);
    MicoGpioFinalize((mico_gpio_t)pinSCL);
    
    MicoGpioInitialize((mico_gpio_t)pinSDA,(mico_gpio_config_t)OUTPUT_OPEN_DRAIN_NO_PULL);  
    MicoGpioOutputHigh( (mico_gpio_t)pinSDA);
    MicoGpioInitialize((mico_gpio_t)pinSCL,(mico_gpio_config_t)OUTPUT_OPEN_DRAIN_NO_PULL);  
    MicoGpioOutputHigh( (mico_gpio_t)pinSCL);

    IIC_Init = true;
    IIC_started = false;
    IIC_Start();
    IIC_Send_Byte(0xFE);
    IIC_Stop();
  }
  else {
    hw_i2c.port = MICO_I2C_1;
    if (sda == 1) hw_i2c.address_width = I2C_ADDRESS_WIDTH_10BIT;
    else if (sda == 0) hw_i2c.address_width = I2C_ADDRESS_WIDTH_7BIT;
    else return luaL_error( L, "addr_len should be assigend 0 or 1" );
    if (scl == 0) hw_i2c.speed_mode = I2C_STANDARD_SPEED_MODE;
    else if (scl == 1) hw_i2c.speed_mode = I2C_HIGH_SPEED_MODE;
    else return luaL_error( L, "mode should be assigend 0 or 1" );
      
    hw_i2c.address = 0;
    MicoI2cInitialize(&hw_i2c);
    hw_IIC_Init = true;
  }

  return 0;
}


//===================================
static int i2c_deinit( lua_State* L )
{
  unsigned id =  luaL_checkinteger( L, 1 );
  if ((id != 0) && (id != 1)) return luaL_error( L, "id should be assigend 0 or 1" );

  
  if (id == 0) {
    if (IIC_Init) {
      MicoGpioFinalize((mico_gpio_t)pinSDA);
      MicoGpioFinalize((mico_gpio_t)pinSCL);
      IIC_Init = false;
      IIC_started = false;
    }
  }
  else {
    if (hw_IIC_Init) MicoI2cFinalize(&hw_i2c);
  }

  return 0;
}


//i2c.write(id, dev_id, data1,[data2],...)
//==================================
static int i2c_write( lua_State* L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  if ((id != 0) && (id != 1)) return luaL_error( L, "id should be assigend 0 or 1" );
  
  if( lua_gettop( L ) < 3 ) return luaL_error( L, "id,dev_id,data arg needed" );
  
  unsigned int dev_id = luaL_checkinteger( L, 2 ); 
  
  uint8_t b[512];
  
  // Copy data to buffer
  size_t datalen=0, i=0;
  int numdata=0;
  uint16_t wrote = 0;
  unsigned argn=0;
  
  int res = 0;
  for( argn = 3; argn <= lua_gettop( L ); argn ++ )
  {
    if( lua_type( L, argn ) == LUA_TNUMBER ) {
      numdata = ( int )luaL_checkinteger( L, argn );
      if ((numdata >= 0) && (numdata <= 255)) {
        b[wrote++] = (uint8_t)numdata;
      }
    }
    else if ( lua_istable( L, argn ) ) {
      datalen = lua_objlen( L, argn );
      for( i = 0; i < datalen; i ++ ) {
        lua_rawgeti( L, argn, i + 1 );
        numdata = ( int )luaL_checkinteger( L, -1 );
        lua_pop( L, 1 );
        if ((numdata >= 0) && (numdata <= 255)) {
          b[wrote++] = (uint8_t)numdata;
        }
      }
      if (res != 0) break; 
    }
    else {
      const char *pdata = luaL_checklstring( L, argn, &datalen );
      for ( i = 0; i < datalen; i ++ ) {
        b[wrote++] = (uint8_t)numdata;
      }
      if (res != 0) break; 
    }
  }

  if (id == 0) { // === software i2c ===
    if (!IIC_Init) return luaL_error( L, "software i2c not initialized" );
    if ((dev_id < 7) || (dev_id >= 0x78)) {
      return luaL_error( L, "dev_id is wrong" );
    }
  }
  else { // === hardware i2c ===
    if (!hw_IIC_Init) return luaL_error( L, "hardware i2c not initialized" );

    if (hw_i2c.address_width == I2C_ADDRESS_WIDTH_10BIT) {
      if ((dev_id < 7) || (dev_id >= 0x0400)) dev_id = 0;
    }
    else {
      if ((dev_id < 7) || (dev_id >= 0x78)) dev_id = 0;
    }
    if (dev_id == 0) return luaL_error( L, "dev_id is wrong" );

  }

  if (wrote > 0) {
    res = _i2c_write(id, dev_id, &b[0], wrote, 1);
    if (res == 0) res = wrote;
  }
  else res = 0;
  
  lua_pushinteger( L, res );
  
  return 1;
}

//i2c.read(id, dev_id, n)
//=================================
static int i2c_read( lua_State* L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  if ((id != 0) && (id != 1)) return luaL_error( L, "id should be assigend 0 or 1" );

  unsigned int dev_id = luaL_checkinteger( L, 2 ); 

  uint16_t size = ( uint32_t )luaL_checkinteger( L, 3 );
  if ( size == 0 ) {
    lua_pushinteger(L, 0);
    lua_pushinteger(L, 0);
    return 2;
  }
  
  if (size > 512) size = 512;
  
  uint8_t b[512];
  int i=0;
  uint8_t data;

  if (id == 0) { // === software i2c ===
    if (!IIC_Init) return luaL_error( L, "software i2c not initialized" );
    if ((dev_id < 7) || (dev_id >= 0x78)) {
      return luaL_error( L, "dev_id is wrong" );
    }

    // Send start condition
    IIC_Start();
    // Send address
    int res = IIC_Send_Byte((dev_id << 1) | 1);
    if (res != 0) {
      IIC_Stop();
      lua_pushinteger(L, -1);
      lua_pushinteger(L, res);
      goto exit;
    }
    
    for( i = 0; i < size; i ++ )
    {
      if (i == (size-1)) data = IIC_Read_Byte(0);
      else  data = IIC_Read_Byte(1);
      b[i] = data;
    }

    // Send stop condition
    IIC_Stop();
  }
  else { // === hardware i2c ===
    if (!hw_IIC_Init) return luaL_error( L, "hardware i2c not initialized" );

    if (hw_i2c.address_width == I2C_ADDRESS_WIDTH_10BIT) {
      if ((dev_id < 7) || (dev_id >= 0x0400)) dev_id = 0;
    }
    else {
      if ((dev_id < 7) || (dev_id >= 0x78)) dev_id = 0;
    }
    if (dev_id == 0) return luaL_error( L, "dev_id is wrong" );

    hw_i2c.address = dev_id;

    OSStatus err = kNoErr;
    mico_i2c_message_t i2c_msg = {NULL, NULL, 0, 0, 10, false};

    err = MicoI2cBuildRxMessage(&i2c_msg, b, size, 3);
    if (err != kNoErr) {
      lua_pushinteger( L, -1 );
      lua_pushinteger( L, err );
      goto exit;
    }
    err = MicoI2cTransfer(&hw_i2c, &i2c_msg, 1, 1);
    if (err != kNoErr) {
      lua_pushinteger( L, -2 );
      lua_pushinteger( L, err );
      goto exit;
    }
  }

  if (size == 1) {
    lua_pushinteger( L, 1);
    lua_pushinteger( L, b[0]);
  }
  else {
    lua_pushinteger( L, size);
    lua_newtable(L);
    if (id == 0) {
      for( i = 0; i < size; i ++ ) {
        lua_pushinteger( L, b[i] );
        lua_rawseti(L,-2,i + 1);
      }
    }
  }
  
exit:  
  return 2;
}


#define MIN_OPT_LEVEL  2
#include "lrodefs.h"
const LUA_REG_TYPE i2c_map[] =
{
  { LSTRKEY( "setup" ),  LFUNCVAL( i2c_setup )},
  { LSTRKEY( "deinit" ), LFUNCVAL( i2c_deinit )},
  { LSTRKEY( "write" ),  LFUNCVAL( i2c_write )},
  { LSTRKEY( "read" ),   LFUNCVAL( i2c_read )},
#if LUA_OPTIMIZE_MEMORY > 0
#endif          
  {LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_i2c(lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else  
  luaL_register( L, EXLIB_I2C, i2c_map );
  return 1;
#endif
}
