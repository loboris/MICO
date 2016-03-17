/**
 * mcu.c
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"
   
#include "mico_platform.h"
#include "platform.h"
#include "user_config.h"
#include "mico_wlan.h"
#include "mico.h"
#include "system.h"
#include "StringUtils.h"
#include "CheckSumUtils.h"

#ifndef RNG_NVIC_PREEMPTION_PRIORITY
#define RNG_NVIC_PREEMPTION_PRIORITY   0x02
#endif

#ifndef RNG_NVIC_SUBPRIORITY
#define RNG_NVIC_SUBPRIORITY           0x00
#endif

   
extern mico_queue_t os_queue;
extern int getLua_systemParams(lua_system_param_t* lua_system_param);
extern int saveLua_systemParams(lua_system_param_t* lua_system_param);

//static lua_State* gL = NULL;

/*
//===================================
static int queue_push( lua_State* L )
{
  if (lua_type(L, 1) == LUA_TFUNCTION || lua_type(L, 1) == LUA_TLIGHTFUNCTION)
  {
    lua_pushvalue(L, 1);  // copy argument (func) to the top of stack
    gL = L;
    queue_msg_t msg;
    msg.L = gL;
    msg.source = USER;
    
    if (lua_type(L, 2) == LUA_TNUMBER) msg.para1 = luaL_checkinteger( L, 2 );
    else msg.para1 = -9999;

    msg.para2 = luaL_ref(L, LUA_REGISTRYINDEX);
    mico_rtos_push_to_queue( &os_queue, &msg, 0);
    l_message( NULL, "pushed");
  }
  else
    return luaL_error( L, "callback function needed" );

  
  return 0;
}
*/

//===================================
static int get_params( lua_State* L )
{
  lua_system_param_t lua_system_param;

  if (getLua_systemParams(&lua_system_param) == 1) {
    lua_newtable( L );
    lua_pushinteger(L, lua_system_param.use_wwdg );
    lua_setfield( L, -2, "use_wwdg" );
    lua_pushinteger(L, lua_system_param.wdg_tmo );
    lua_setfield( L, -2, "wdg_tmo" );
    lua_pushinteger(L, lua_system_param.stack_size );
    lua_setfield( L, -2, "stack_size" );
    lua_pushinteger(L, lua_system_param.inbuf_size );
    lua_setfield( L, -2, "inbuf_size" );
    lua_pushstring(L, lua_system_param.init_file );
    lua_setfield( L, -2, "init_file" );
    lua_pushstring(L, lua_system_param.wifi_ssid );
    lua_setfield( L, -2, "wifi_ssid" );
    lua_pushstring(L, lua_system_param.wifi_key );
    lua_setfield( L, -2, "wifi_key" );
    lua_pushinteger(L, lua_system_param.wifi_start );
    lua_setfield( L, -2, "wifi_start" );
    lua_pushinteger(L, lua_system_param.tz );
    lua_setfield( L, -2, "tz" );
    lua_pushinteger(L, lua_system_param.baud_rate );
    lua_setfield( L, -2, "baud_rate" );
    if (lua_system_param.parity == NO_PARITY)
      lua_pushstring(L, "NO_PARITY");
    else if (lua_system_param.parity == ODD_PARITY)
      lua_pushstring(L, "ODD_PARITY");
    else if (lua_system_param.parity == EVEN_PARITY)
      lua_pushstring(L, "EVEN_PARITY");
    else
      lua_pushstring(L, "?");
    lua_setfield( L, -2, "parity" );
  }
  else lua_pushnil(L);
  
  return 1;
}

//====================================
static int get_sparams( lua_State* L )
{
  lua_system_param_t lua_system_param;

  if (getLua_systemParams(&lua_system_param) == 1) {
    printf("   use_wwdg = %d\r\n", lua_system_param.use_wwdg);
    printf("    wdg_tmo = %d\r\n", lua_system_param.wdg_tmo);
    printf(" stack_size = %d\r\n", lua_system_param.stack_size);
    printf(" inbuf_size = %d\r\n", lua_system_param.inbuf_size);
    printf("  init_file = \"%s\"\r\n", lua_system_param.init_file);
    printf("  wifi_ssid = \"%s\"\r\n", lua_system_param.wifi_ssid);
    printf("   wifi_key = \"%s\"\r\n", lua_system_param.wifi_key);
    printf(" wifi_start = %d\r\n", lua_system_param.wifi_start);
    printf("         tz = %d\r\n", lua_system_param.tz);
    printf("  baud_rate = %d\r\n", lua_system_param.baud_rate);
    if (lua_system_param.parity == NO_PARITY)
      printf("     parity = 'n'\r\n");
    else if (lua_system_param.parity == ODD_PARITY)
      printf("     parity = 'o'\r\n");
    else if (lua_system_param.parity == EVEN_PARITY)
      printf("     parity = 'e'\r\n");
  }
  else printf("Error or BAD crc\r\n");
  
  return 0;
}

//----------------------
void _listParams( void )
{
  l_message( NULL, "Lua table with params needed:\r\n\
  use_wwdg - use Window watchdog\r\n\
   wdg_tmo - watchdog timeout in ms\r\n\
 init_file - lua init file name\r\n\
 wifi_ssid - wifi ssid\r\n\
  wifi_key - wifi key\r\n\
wifi_start - auto start wifi\r\n\
        tz - time zone\r\n\
stack_size - lua thread stack size\r\n\
inbuf_size - lua input buffer size\r\n\
 baud_rate - serial interface baud rate\r\n\
    parity - serial interface parity" );
}

//====================================
static int set_sparams( lua_State* L )
{
  if (!lua_istable(L, 1)) {
    _listParams();
    return 0;
  }

  uint8_t change = 0;
  const char* buf;
  lua_system_param_t lua_system_param;

  if (getLua_systemParams(&lua_system_param) == 0) {
    l_message(NULL,"Error reading params or BAD crc");
    return 0;
  }
  
  lua_getfield(L, 1, "use_wwdg");
  if (!lua_isnil(L, -1)) {
    if (lua_isstring(L, -1)) {
      uint8_t wd = luaL_checkinteger( L, -1 );
      if (wd == 0) lua_system_param.use_wwdg = 0;
      else lua_system_param.use_wwdg = 1;
      l_message( NULL, "updated: use_wwdg" );
      change++;
    }
    else l_message( NULL, "wrong arg type: use_wwdg" );
  }

  lua_getfield(L, 1, "wdg_tmo");
  if (!lua_isnil(L, -1)) {
    if (lua_isstring(L, -1)) {
      uint32_t wdtmo = luaL_checkinteger( L, -1 );
      if (wdtmo < 2000 || wdtmo > 3600000) wdtmo = 10000;
      lua_system_param.wdg_tmo = wdtmo;
      l_message( NULL, "updated: wdg_tmo" );
      change++;
    }
    else l_message( NULL, "wrong arg type: wdg_tmo" );
  }

  lua_getfield(L, 1, "stack_size");
  if (!lua_isnil(L, -1)) {
    if (lua_isstring(L, -1)) {
      uint16_t stksz = luaL_checkinteger( L, -1 );
      if (stksz < 8192 || stksz > 24576) {
        l_message( NULL, "stack_size: 8192 ~ 24576, not updated" );
      }
      else {
        lua_system_param.stack_size = stksz;
        l_message( NULL, "updated: stack_size" );
        change++;
      }
    }
    else l_message( NULL, "wrong arg type: stack_size" );
  }

  lua_getfield(L, 1, "inbuf_size");
  if (!lua_isnil(L, -1)) {
    if (lua_isstring(L, -1)) {
      uint16_t inbsz = luaL_checkinteger( L, -1 );
      if (inbsz < 128 || inbsz > 1024) inbsz = 256;
      lua_system_param.inbuf_size = inbsz;
      l_message( NULL, "updated: inbuf_size" );
      change++;
    }
    else l_message( NULL, "wrong arg type: inbuf_size" );
  }

  lua_getfield(L, 1, "baud_rate");
  if (!lua_isnil(L, -1)) {
    if (lua_isstring(L, -1)) {
      uint32_t bdr = luaL_checkinteger( L, -1 );
      lua_system_param.baud_rate = bdr;
      l_message( NULL, "updated: baud_rate" );
      change++;
    }
    else l_message( NULL, "wrong arg type: baud_rate" );
  }

  lua_getfield(L, 1, "parity");
  if (!lua_isnil(L, -1)) {
    if (lua_isstring(L, -1)) {
      buf = luaL_checkstring( L, -1 );
      if (strlen(buf) == 1) {
        change++;
        if (strcmp(buf, "n") == 0)
          lua_system_param.parity = NO_PARITY;
        else if (strcmp(buf, "o") == 0)
          lua_system_param.parity = ODD_PARITY;
        else if (strcmp(buf, "e") == 0)
          lua_system_param.parity = EVEN_PARITY;
        else {
          l_message( NULL, "arg parity should be 'n' or 'o' or 'e' " );
          change--;
        }
        l_message( NULL, "updated: parity" );
      }
      else l_message( NULL, "arg parity should be 'n' or 'o' or 'e' " );
    }
    else l_message( NULL, "wrong arg type: parity" );
  }

  lua_getfield(L, 1, "init_file");
  if (!lua_isnil(L, -1)) {
    if (lua_isstring(L, -1)) {
      buf = luaL_checkstring( L, -1 );
      if (strlen(buf) < 16) {
        sprintf(&lua_system_param.init_file[0], buf);
        change++;
        l_message( NULL, "updated: init_file" );
      }
      else l_message( NULL, "file name too long" );
    }
    else l_message( NULL, "wrong arg type: init_file" );
  }

  lua_getfield(L, 1, "wifi_ssid");
  if (!lua_isnil(L, -1)) {
    if (lua_isstring(L, -1)) {
      buf = luaL_checkstring( L, -1 );
      if (strlen(buf) < 32) {
        sprintf(&lua_system_param.wifi_ssid[0], buf);
        change++;
        l_message( NULL, "updated: wifi_ssid" );
      }
      else l_message( NULL, "wifi_ssid too long" );
    }
    else l_message( NULL, "wrong arg type: wifi_ssid" );
  }

  lua_getfield(L, 1, "wifi_key");
  if (!lua_isnil(L, -1)) {
    if (lua_isstring(L, -1)) {
      buf = luaL_checkstring( L, -1 );
      if (strlen(buf) < 64) {
        sprintf(&lua_system_param.wifi_key[0], buf);
        change++;
        l_message( NULL, "updated: wifi_key" );
      }
      else l_message( NULL, "wifi_key too long" );
    }
    else l_message( NULL, "wrong arg type: wifi_key" );
  }

  lua_getfield(L, 1, "wifi_start");
  if (!lua_isnil(L, -1)) {
    if (lua_isstring(L, -1)) {
      int wfs = luaL_checkinteger( L, -1 );
      if (wfs == 1) lua_system_param.wifi_start = 1;
      else lua_system_param.wifi_start = 0;
      l_message( NULL, "updated: wifi_start" );
      change++;
    }
    else l_message( NULL, "wrong arg type: wifi_start" );
  }

  lua_getfield(L, 1, "tz");
  if (!lua_isnil(L, -1)) {
    if (lua_isstring(L, -1)) {
      int tz = luaL_checkinteger( L, -1 );
      if ((tz <= 14) && (tz >= -12)) {
        lua_system_param.tz = tz;
        l_message( NULL, "updated: tz" );
        change++;
      }
      else l_message( NULL, "time zome must be -12 ~ +14" );
    }
    else l_message( NULL, "wrong arg type: tz" );
  }
  if (change) {
    if (saveLua_systemParams(&lua_system_param) == 0) {
      l_message( NULL, "ERROR saving system params!");
    }
    else l_message( NULL, "New params saved." );
  }
  else _listParams();

  return 0;
}

//====================================
static int mcu_version( lua_State* L )
{
  lua_pushstring(L,MCU_VERSION);
  lua_pushstring(L,BUILD_DATE);
  return 2;
}

static int mcu_wifiinfo( lua_State* L )
{
  char wifi_ver[64] = {0};
  IPStatusTypedef para;
  char  mac[18];
  MicoGetRfVer(wifi_ver, sizeof(wifi_ver));
  micoWlanGetIPStatus(&para, Station);
  formatMACAddr(mac, (char *)&para.mac);  
  lua_pushstring(L,MicoGetVer());//mxchipWNet library version
  lua_pushstring(L,mac);//mac
  //lua_pushstring(L,mico_get_bootloader_ver());//bootloader_ver
  lua_pushstring(L,wifi_ver);//Wi-Fi driver version    
  return 3;
}

static int mcu_reboot( lua_State* L )
{
   MicoSystemReboot();
   return 0;
}

static int mcu_memory( lua_State* L )
{
   lua_pushinteger(L,MicoGetMemoryInfo()->free_memory);     // total free space
   lua_pushinteger(L,MicoGetMemoryInfo()->allocted_memory); // total allocated space
   lua_pushinteger(L,MicoGetMemoryInfo()->total_memory);    // maximum total allocated space
   lua_pushinteger(L,MicoGetMemoryInfo()->num_of_chunks);   // number of free chunks
   return 4;
}
static int mcu_chipid( lua_State* L )
{
    uint32_t mcuID[3];
    mcuID[0] = *(__IO uint32_t*)(0x1FFF7A20);
    mcuID[1] = *(__IO uint32_t*)(0x1FFF7A24);
    mcuID[2] = *(__IO uint32_t*)(0x1FFF7A28);
    char str[25];
    sprintf(str,"%08X%08X%08X",mcuID[0],mcuID[1],mcuID[2]);
    lua_pushstring(L,str);
    return 1;
}

//===================================
static int mcu_random( lua_State* L )
{
    int randn, minn, maxn;
    uint16_t i;
    uint8_t sd = 0;
    
    maxn = 0x7FFFFFFE;
    minn = 0;
    if (lua_gettop(L) >= 1) maxn = luaL_checkinteger( L, 1 );
    if (lua_gettop(L) >= 2) minn = luaL_checkinteger( L, 2 );
    if (lua_gettop(L) >= 3) sd = luaL_checkinteger( L, 3 );
    if (maxn < minn) maxn = minn+1;
    if (maxn <= 0) maxn = 1;
    
    if (sd) srand(mico_get_time());
      
    i = 0;
    do {
      //MicoRandomNumberRead(&rbuf, n);
      randn = rand();
      if (randn > maxn) randn = randn % (maxn+1);
      i++;
    } while ((i < 10000) && (randn < minn));
    
    if (randn < minn) randn = minn;
    
    lua_pushinteger(L,randn);
    return 1;
}

extern unsigned char boot_reason;
static int mcu_bootreason( lua_State* L )
{
    char str[12]={0x00};
    switch(boot_reason)
    {
      case BOOT_REASON_NONE:            strcpy(str,"NONE");break;
      case BOOT_REASON_SOFT_RST:        strcpy(str,"SOFT_RST");break;
      case BOOT_REASON_PWRON_RST:       strcpy(str,"PWRON_RST");break;
      case BOOT_REASON_EXPIN_RST:       strcpy(str,"EXPIN_RST");break;
      case BOOT_REASON_WDG_RST:         strcpy(str,"WDG_RST");break;
      case BOOT_REASON_WWDG_RST:        strcpy(str,"WWDG_RST");break;
      case BOOT_REASON_LOWPWR_RST:      strcpy(str,"LOWPWR_RST");break;
      case BOOT_REASON_BOR_RST:         strcpy(str,"BOR_RST");break;
      default:strcpy(str,"NONE");break;
    }
    lua_pushstring(L,str);
    return 1;
}

#define MIN_OPT_LEVEL       2
#include "lrodefs.h"
const LUA_REG_TYPE mcu_map[] =
{
  { LSTRKEY( "ver" ), LFUNCVAL( mcu_version )},
  { LSTRKEY( "info" ), LFUNCVAL( mcu_wifiinfo )},
  { LSTRKEY( "reboot" ), LFUNCVAL( mcu_reboot )},
  { LSTRKEY( "mem" ), LFUNCVAL( mcu_memory )},
  { LSTRKEY( "chipid" ), LFUNCVAL( mcu_chipid )},
  { LSTRKEY( "bootreason" ), LFUNCVAL(mcu_bootreason)},
  { LSTRKEY( "getparams" ), LFUNCVAL(get_params)},
  { LSTRKEY( "sgetparams" ), LFUNCVAL(get_sparams)},
  { LSTRKEY( "setparams" ), LFUNCVAL(set_sparams)},
  //{ LSTRKEY( "queuepush" ), LFUNCVAL(queue_push)},
  { LSTRKEY( "random" ), LFUNCVAL(mcu_random)},
#if LUA_OPTIMIZE_MEMORY > 0
#endif      
  {LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_mcu(lua_State *L)
{
  srand(mico_get_time());
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else    
  luaL_register( L, EXLIB_MCU, mcu_map );
  return 1;
#endif
}
