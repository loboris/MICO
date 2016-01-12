/**
 * oleddisp.c
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"
   
#include "mico_platform.h"
#include "user_config.h"
#include "MICO.h"
#include "StringUtils.h"
#include "oledfont.h"  	 


#define Max_Column	128
#define Max_Row		64
#define NUM_GPIO        18

extern const char wifimcu_gpio_map[];
extern uint8_t spiInit[];
extern uint16_t _spi_write(uint8_t id, uint8_t databits,uint8_t* data, uint16_t count, uint16_t rep);
extern int _i2c_write(uint8_t id, uint16_t dev_adr, uint8_t* data, uint16_t count, uint16_t rep);
extern bool IIC_Init;
extern bool hw_IIC_Init;

uint8_t oled_ID = 255;
static uint8_t oled_pinDC  = 255;
static uint8_t oled_fontSize = 8;
static uint8_t oled_fontWidth = 6;
static uint8_t oled_inverse = 0;
static uint8_t oled_CharSpace = 0;
static uint8_t oled_lastX = 0;
static uint8_t oled_lastY = 0;
static uint8_t oled_fixedWidth = 1;

static uint8_t oled_i2caddr = 0x3C;

static uint8_t rowbuf[132];

//---------------------------------------------
static int platform_gpio_exists( unsigned pin )
{
  return pin < NUM_GPIO;
}

//---------------------------------
static const uint8_t initdata[] = {
    0xAE,	// display off
    0xD5,0x80,  // clock div
    0xA8,0x3F,  // multiplex (height-1
    0xD3,0x00,  // display offset
    0x40,       // start line
    0x8D,0x14,  // charge pump, 0x14 on; 0x10 off
    0x20,0x00,  // memory address mode (0 horizontal; 1 vertical; 2 page)
    0x22,0,7,	// page start&end address
    0x21,0,127,	// column start&end address
    0xA1,	// seg remap 1
    0xC8,	// remapperd mode (0xC0 = normal mode)
    0xDA,0x12,	// seq com pins (64 line 0x12; 32 line 0x02)
    0x81,0xCF,	// contrast (extVcc 0x9F; charge pump 0xCF)
    0xD9,0xF1,	// precharge (extVcc 0x22; charge pump 0xF1)
    0xDB,0x40,	// vcomh detect level (0x20 default)
    0xA4,	// disable display all on
    0xA6,	// display normal (not inverted)
    0xAF	// display on
};
  

//-------------------------------------------
static void oled_setpos(uint8_t x, uint8_t y)
{
  uint8_t rbuf[6];
  
  rbuf[0] = 0x00; // command
  rbuf[1] = x & 0x0F; 
  rbuf[2] = ((x & 0xF0) >> 4) | 0x10;
  rbuf[3] = 0x22;
  rbuf[4] = (y & 0x07);
  rbuf[5] = 7;

  if (oled_ID < 3) {
    MicoGpioOutputLow( (mico_gpio_t)oled_pinDC );
    _spi_write(oled_ID, 8, &rbuf[1], 5, 1 );
  }
  else {
    uint8_t rpt;
    for (rpt=0; rpt<5; rpt++) {
      if (_i2c_write(oled_ID-3, oled_i2caddr, &rbuf[0], 6, 1) == 0) break;
    }
  }
}   	  

//--------------------------------------
static int _oled_write_buf(uint16_t len)
{
  uint16_t i;
  
  rowbuf[0] = 0x40;
  if (oled_inverse) {
    for (i=1;i<=len;i++) {
      rowbuf[i] = rowbuf[i] ^ 0xFF;
    }
  }
  
  if (oled_ID < 3) {
    MicoGpioOutputHigh( (mico_gpio_t)oled_pinDC );
    _spi_write(oled_ID, 8, &rowbuf[1], len, 1);
    return 0;
  }
  else {
    return _i2c_write(oled_ID-3, oled_i2caddr, &rowbuf[0], len+1, 1);
  }
}

//----------------------------------------------------------------
static void _oled_write_bufpos(uint8_t x, uint8_t y, uint16_t len)
{
  if (oled_ID < 3) {
    oled_setpos(x,y);
    _oled_write_buf(len);
  }
  else {
    uint8_t rpt;
    for (rpt=0; rpt<5; rpt++) {
      oled_setpos(x,y);
      if (_oled_write_buf(len) == 0) break;
    }
  }
}

// Displays a character at the specified location, including part of the character
// x:0~127
// y:0~7
// mode:0, highlighted, 1, display
// size: select the font 16/12
//-----------------------------------------------------------
static uint8_t OLED_ShowChar(uint8_t x,uint8_t y,uint8_t chr)
{      	
  unsigned char c=0;
  uint8_t i=0, j=0;	
  uint8_t fs = 0, fe = oled_fontWidth;
  
  c=chr-' ';  //Get the offset value

  if (!oled_fixedWidth) {
    if(oled_fontSize ==16) {
      for(i=0;i<oled_fontWidth;i++) {
        if(F8X16[c*16+i] > 0 || F8X16[c*16+i+8] > 0) break;
        fs++;
      }
      for(i=oled_fontWidth;i>0;i--) {
        if(F8X16[c*16+i-1] > 0 || F8X16[c*16+i+7] > 0) break;
        fe--;
      }
    }
    else {
      for(i=0;i<oled_fontWidth;i++) {
        if(F6x8[c][i] > 0) break;
        fs++;
      }
      for(i=oled_fontWidth;i>0;i--) {
        if(F6x8[c][i-1] > 0) break;
        fe--;
      }
    }
    
    if (fe < oled_fontWidth) fe++;
    else if (fs > 0) fs--;
    while ((fe-fs) < 2) {
      if (fe < oled_fontWidth) fe++;
      else if (fs > 0) fs--;
    }
  }
  
  if (x > Max_Column-oled_fontWidth) {x= 0; y= y+(oled_fontSize / 8);}
  if (y > Max_Row-1) {x= 0; y= 0;}
  
  if(oled_fontSize ==16)
  {
    // upper char part
    j = 1;
    for (i=fs;i<fe;i++) {
      rowbuf[j++] = F8X16[c*16+i];
    }
    for (i=0;i<oled_CharSpace;i++) { // add char_space
      if ((i+x) >= Max_Column) break;
      rowbuf[j++] = 0;
    }
    _oled_write_bufpos(x,y,j-1);
    
    // low char part
    j = 1;
    for (i=fs;i<fe;i++) {
      rowbuf[j++] = F8X16[c*16+i+8];
    }
    for (i=0;i<oled_CharSpace;i++) { // add char_space
      if ((i+x) >= Max_Column) break;
      rowbuf[j++] = 0;
    }
    _oled_write_bufpos(x,y+1,j-1);
  }
  else {	
    j = 1;
    for(i=fs;i<fe;i++) {
      rowbuf[j++] = F6x8[c][i];
    }
    for (i=0;i<oled_CharSpace;i++) { // add char_space
      if ((i+x) >= Max_Column) break;
      rowbuf[j++] = 0;
    }
    _oled_write_bufpos(x,y,j-1);
  }
  return (fe-fs);
}

/*
//m^n function
//-------------------------------------------
static uint32_t oled_pow(uint8_t m,uint8_t n)
{
  uint32_t result = 1;	 
  while(n--) result *= m;    
  return result;
}				  
*/

//Displays a character string
//-----------------------------------------------------------
static void OLED_ShowString(uint8_t x,uint8_t y,uint8_t *chr)
{
  unsigned char j=0;
  uint8_t x_t = x, y_t = y;
  uint8_t fntw, k;
  
  while (chr[j]!='\0')
  {	
    // add for CR/LF
    if (('\r' == chr[j]) || ('\n' == chr[j])) {   // CR or LF
      k = 1;
      while((x_t + k - 1) < (Max_Column)) {
        // clear to the end of the current line
        rowbuf[k++] = 0;
      }
      if (k > 1) {
        _oled_write_bufpos(x_t, y_t, k-1);
        x_t += (k-1);
      }
      j += 1;
      if ('\n' == chr[j]) j += 1;   // CR & LF
    }
    else {
      if (x_t > (Max_Column-oled_fontWidth)) {  // line end, goto next line
        x_t = 0;
        y_t += (oled_fontSize / 8);
        if (y_t >= 8) {  // can only display 8/4 lines
          break;
        }
      }
      fntw = OLED_ShowChar(x_t, y_t, chr[j]);
      x_t += (fntw+oled_CharSpace);
      j++;
    }
  }
  oled_lastX = x_t;
  oled_lastY = y_t;
}

//-----------------------------
static void _oled_clear( void )
{
  uint8_t i;
  

  if (oled_ID < 3) {
    oled_setpos(0,0);
    rowbuf[0] = 0x40;
    rowbuf[1] = 0;
    MicoGpioOutputHigh( (mico_gpio_t)oled_pinDC );
    _spi_write(oled_ID, 8, &rowbuf[1], 1, 1024);
  }
  else {
    for (i=1;i<=128;i++) {
      rowbuf[i] = 0;
    }
    for (i=0;i<8;i++) {
      _oled_write_bufpos(0,i,128);
    }
  }
}

//==================================
static int oled_init( lua_State* L )
{
  uint8_t id = luaL_checkinteger( L, 1 );
  if (id > 4) {
    l_message( NULL, "id should assigend 0 ~ 4" );
    lua_pushinteger( L, -1 );
    return 1;
  }
  
  if (id < 3) {
    if (!spiInit[id]) {
      l_message( NULL, "spi not yet initialized" );
      lua_pushinteger( L, -2 );
      return 1;
    }
  }
  else {
    if ((id == 3) && (!IIC_Init)) {
      l_message( NULL, "software i2c not yet initialized" );
      lua_pushinteger( L, -2 );
      return 1;
    }
    else if ((id == 4) && (!hw_IIC_Init)) {
      l_message( NULL, "hardware i2c not yet initialized" );
      lua_pushinteger( L, -2 );
      return 1;
    }
  }

  oled_ID = id;
  uint8_t user_param = 0;
  
  if (oled_ID < 3) {
    uint8_t pin = luaL_checkinteger( L, 2 );
    MOD_CHECK_ID( gpio, pin );
    oled_pinDC = wifimcu_gpio_map[pin];
    MicoGpioFinalize((mico_gpio_t)oled_pinDC);
    MicoGpioInitialize((mico_gpio_t)oled_pinDC, (mico_gpio_config_t)OUTPUT_PUSH_PULL);
    MicoGpioOutputLow( (mico_gpio_t)oled_pinDC );
    
    if (lua_gettop(L) >= 3) {
      if (lua_istable( L, 3 )) user_param = 3;
    }
  }
  else {
    if (lua_gettop(L) >= 2) {
      if (lua_istable( L, 2 )) user_param = 2;
    }
  }
    
  uint16_t size = sizeof(initdata);
  uint16_t i;

  if (user_param > 0) {
    l_message( NULL, "user params detected" );
    size = lua_objlen( L, user_param );
    if (size > 0) {
      int pdata;
          
      for( i = 0; i < size; i++ )
      {
        lua_rawgeti( L, user_param, i + 1 );
        pdata = (uint8_t)luaL_checkinteger( L, -1 );
        lua_pop( L, 1 );
        if ((pdata < 0) || (pdata > 255)) {
          user_param = 0;
          break;
        }
        rowbuf[i+1] = (uint8_t)(pdata & 0xFF);
      }
    }
    else {
      l_message( NULL, "user params size is 0" );
      user_param = 0;
    }
  }
  
  if (user_param == 0) {  
    size = sizeof(initdata);
    for( i = 0; i < size; i++ ) {
      rowbuf[i+1] = initdata[i];
    }
  }
  
  rowbuf[0] = 0x00;
  if (oled_ID < 3) _spi_write(oled_ID, 8, &rowbuf[1], size, 1);
  else {
    for( i = 0; i < 5; i++ ) {
      if (_i2c_write(oled_ID-3, oled_i2caddr, &rowbuf[0], size+1, 1) == 0) break;
    }
  }
  
  // clear screen
  _oled_clear();  
  
  oled_fontSize = 8;
  oled_fontWidth = 6;
  
  lua_pushinteger( L, 0 );
  return 1;
}

//===================================    
static int oled_clear( lua_State* L )
{
  if (oled_ID > 4) return 0;

  _oled_clear();  
  return 0;
}

//oled.write(x,y,ndec,string|number)
//===================================
static int oled_write( lua_State* L )
{
  if (oled_ID > 4) return 0;

  const char* buf;
  char tmps[16];
  size_t len;
  uint8_t numdec = 0;
  uint8_t argn = 0;
  float fnum;
  
  if ( lua_gettop( L ) < 4 ) {
    l_message( NULL, "not enough arg" );
    return 0;
  }

  uint8_t x = luaL_checkinteger( L, 1 );
  uint8_t y = luaL_checkinteger( L, 2 );
  numdec = luaL_checkinteger( L, 3 );
  
  if (x > Max_Column) {
    x = oled_lastX;
    y = oled_lastY;
  }
  
  for( argn = 4; argn <= lua_gettop( L ); argn++ )
  {
    if ( lua_type( L, argn ) == LUA_TNUMBER )
    { // write number
      if (numdec == 0 ) {
        len = lua_tointeger( L, argn );
        sprintf(tmps,"%d",len);
      }
      else {
        fnum = luaL_checknumber( L, argn );
        sprintf(tmps,"%.*f",numdec, fnum);
      }
      OLED_ShowString(x,y, (uint8_t*)tmps);
      x = oled_lastX;
      y = oled_lastY;
    }
    else if ( lua_type( L, argn ) == LUA_TSTRING )
    { // write string
      luaL_checktype( L, argn, LUA_TSTRING );
      buf = lua_tolstring( L, argn, &len );
      OLED_ShowString(x, y, (uint8_t*)buf);
      x = oled_lastX;
      y = oled_lastY;
    }
  }  
  return 0;
}

//oled.writechar(x,y,char)
//=======================================
static int oled_writechar( lua_State* L )
{
  if (oled_ID > 4) return 0;

  uint8_t x = luaL_checkinteger( L, 1 );
  uint8_t y = luaL_checkinteger( L, 2 );
  uint8_t chr = luaL_checkinteger( L, 3 );
  
  OLED_ShowChar(x, y, chr);
  return 0;
}

//======================================
static int oled_fontsize( lua_State* L )
{
  uint8_t fs = luaL_checkinteger( L, 1 );
  if (fs > 8) {
    oled_fontSize = 16;
    oled_fontWidth = 8;
  }
  else {
    oled_fontSize = 8;  
    oled_fontWidth = 6;
  }
  return 0;
}

//==========================================
static int oled_writeinverse( lua_State* L )
{
  uint8_t inv = luaL_checkinteger( L, 1 );
  
  if (inv == 0) oled_inverse = 0;
  else oled_inverse = 1;
  return 0;
}

//===================================
static int oled_fixed( lua_State* L )
{
  uint8_t ff = luaL_checkinteger( L, 1 );
  
  if (ff == 0) oled_fixedWidth = 0;
  else oled_fixedWidth = 1;
  return 0;
}

//==========================================
static int oled_charspace( lua_State* L )
{
  uint8_t chrsp = luaL_checkinteger( L, 1 );
  
  if (chrsp > 8) chrsp = 8;
  oled_CharSpace = chrsp;
  return 0;
}

//========================================
static int oled_seti2caddr( lua_State* L )
{
  if (oled_ID > 4) {
    l_message( NULL, "oled not yet initialized" );
    return 0;
  }
  
  uint16_t id = luaL_checkinteger( L, 1 );
  
  if (oled_ID > 2) { // === software i2c ===
    if ((id < 7) || (id >= 0x78)) {
      l_message( NULL, "address is wrong" );
      return 0;
    }
  }
  else {
    l_message( NULL, "spi is used, address not set" );
    return 0;
  }
  
  oled_i2caddr = id;
  return 0;
}



#define MIN_OPT_LEVEL       2
#include "lrodefs.h"
const LUA_REG_TYPE oled_map[] =
{
  { LSTRKEY( "init" ), LFUNCVAL( oled_init )},
  { LSTRKEY( "clear" ), LFUNCVAL( oled_clear )},
  { LSTRKEY( "write" ), LFUNCVAL( oled_write )},
  { LSTRKEY( "writechar" ), LFUNCVAL( oled_writechar )},
  { LSTRKEY( "fontsize" ), LFUNCVAL( oled_fontsize )},
  { LSTRKEY( "inverse" ), LFUNCVAL( oled_writeinverse )},
  { LSTRKEY( "fixedwidth" ), LFUNCVAL( oled_fixed )},
  { LSTRKEY( "charspace" ), LFUNCVAL( oled_charspace )},
  { LSTRKEY( "seti2caddr" ), LFUNCVAL( oled_seti2caddr )},
#if LUA_OPTIMIZE_MEMORY > 0
#endif      
  {LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_oled(lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else    
  luaL_register( L, EXLIB_OLED, oled_map );
  return 1;
#endif
}
