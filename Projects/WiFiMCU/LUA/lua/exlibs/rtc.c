/**
 * rtc.c
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"
   
#include "mico_platform.h"
#include "platform_init.h"
#include "user_config.h"
#include "mico_wlan.h"
#include "MICO.h"
#include "StringUtils.h"
#include "time.h" 

#define DEFAULT_WATCHDOG_TIMEOUT 10*1000

//-------------------------------------------------
static uint8_t _get_tm ( struct tm *currentTime ) {
  mico_rtc_time_t ttime;
  
  if ( MicoRtcGetTime(&ttime) == kNoErr ) {
    currentTime->tm_sec = ttime.sec;
    currentTime->tm_min = ttime.min;
    currentTime->tm_hour = ttime.hr;
    currentTime->tm_mday = ttime.date;
    currentTime->tm_mon = ttime.month - 1;
    currentTime->tm_year = ttime.year + 100;
    currentTime->tm_wday = ttime.weekday;
    currentTime->tm_yday = 0;
    currentTime->tm_isdst = -1;
    return 1;
  }
  else {
    return 0;
  }
}

//===================================
static int rtc_getasc( lua_State* L )
{
  struct tm currentTime;
   
  if (_get_tm(&currentTime)) {
    lua_pushstring(L,asctime(&currentTime));
  }else {
    lua_pushstring(L, "RTC unsupported"); 
  }  
  return 1;
}

//=========================================
static int rtc_get_str_time( lua_State* L )
{
  struct tm currentTime;
  char tmps[64] = "%Y-%m-%d %H:%M:%S";
  char tmpt[80];
  const char* buf;
  size_t len;
  uint8_t i;
  
  if ( lua_gettop( L ) > 0 ) {
    if ( lua_type( L, 1 ) == LUA_TSTRING ) {
      buf = lua_tolstring( L, 1, &len );
      if (len > 0) {
        i = 0;
        while (buf[i] != '\0' && i < (sizeof(tmps)-1)) {
           tmps[i] = buf[i];
           i++;
        }
        tmps[i] = '\0';
      }
    }
  }
  
  if (_get_tm(&currentTime)) {
   strftime(tmpt, sizeof(tmpt), tmps, &currentTime);
   lua_pushstring(L, tmpt);
   }else {
     lua_pushstring(L, "RTC unsupported"); 
   }  
  return 1;
}

//================================
static int rtc_get( lua_State* L )
{
  lua_newtable(L);
  mico_rtc_time_t ttime;
  
  if( MicoRtcGetTime(&ttime) == kNoErr ){
    lua_pushinteger( L, ttime.sec);
    lua_rawseti(L,-2, 1);
    lua_pushinteger( L, ttime.min);
    lua_rawseti(L,-2, 2);
    lua_pushinteger( L, ttime.hr);
    lua_rawseti(L,-2, 3);
    lua_pushinteger( L, ttime.weekday);
    lua_rawseti(L,-2, 4);
    lua_pushinteger( L, ttime.date);
    lua_rawseti(L,-2, 5);
    lua_pushinteger( L, ttime.month);
    lua_rawseti(L,-2, 6);
    lua_pushinteger( L, ttime.year+2000); 
    lua_rawseti(L,-2, 7);
  }
  return 1;
}

//=====================================
static int rtc_getalarm( lua_State* L )
{
  lua_newtable(L);

  RTC_AlarmTypeDef  RTC_AlarmStructure;
  RTC_GetAlarm(RTC_Format_BIN, RTC_Alarm_A, &RTC_AlarmStructure);
  lua_pushinteger( L, RTC_AlarmStructure.RTC_AlarmTime.RTC_H12);
  lua_rawseti(L,-2, 1);
  lua_pushinteger( L, RTC_AlarmStructure.RTC_AlarmTime.RTC_Hours);
  lua_rawseti(L,-2, 2);
  lua_pushinteger( L, RTC_AlarmStructure.RTC_AlarmTime.RTC_Minutes);
  lua_rawseti(L,-2, 3);
  lua_pushinteger( L, RTC_AlarmStructure.RTC_AlarmTime.RTC_Seconds);
  lua_rawseti(L,-2, 4);
  lua_pushinteger( L, RTC_AlarmStructure.RTC_AlarmDateWeekDay);
  lua_rawseti(L,-2, 5);
  lua_pushinteger( L, RTC_AlarmStructure.RTC_AlarmDateWeekDaySel);
  lua_rawseti(L,-2, 6);
  lua_pushinteger( L, RTC_AlarmStructure.RTC_AlarmMask);
  lua_rawseti(L,-2, 7);
  return 1;
}

//====================================
static int rtc_standby( lua_State* L )
{
   char buff[32];
   int nsec = luaL_checkinteger( L, 1 );
   if ((nsec < 0) || (nsec > 84559)) {
     lua_pushstring(L, "wrong interval"); 
     return 1;
   }
   sprintf(buff,"Going to standby for %d seconds\r\n", nsec);
   l_message(NULL,buff);
   mico_rtos_suspend_all_thread();
   
   MicoSystemStandBy(nsec);
   
   mico_rtos_resume_all_thread();
   lua_pushstring(L, "RETURN FROM STANDBY"); 
   return 1;
}

//=========================================
static int rtc_standbyUntil( lua_State* L )
{
  uint8_t h,m,s;
  
  if (!lua_istable(L, 1)) {
    l_message( NULL, "table arg needed" );
    lua_pushinteger(L,0);
    return 1;
  }
  if (lua_objlen( L, 1 ) != 3) {
    l_message( NULL, "hour,minute,second expected" );
    lua_pushinteger(L,0);
    return 1;
  }
  lua_rawgeti( L, 1, 1 );
  h = ( int )luaL_checkinteger( L, -1 );
  lua_pop( L, 1 );
  lua_rawgeti( L, 1, 2 );
  m = ( int )luaL_checkinteger( L, -1 );
  lua_pop( L, 1 );
  lua_rawgeti( L, 1, 3 );
  s = ( int )luaL_checkinteger( L, -1 );
  lua_pop( L, 1 );

  RTC_AlarmTypeDef  RTC_AlarmStructure;
 
  RTC_AlarmStructure.RTC_AlarmTime.RTC_H12     = RTC_HourFormat_24;
  RTC_AlarmStructure.RTC_AlarmTime.RTC_Hours   = h;
  RTC_AlarmStructure.RTC_AlarmTime.RTC_Minutes = m;
  RTC_AlarmStructure.RTC_AlarmTime.RTC_Seconds = s;
  RTC_AlarmStructure.RTC_AlarmDateWeekDay = 0x31;
  RTC_AlarmStructure.RTC_AlarmDateWeekDaySel = RTC_AlarmDateWeekDaySel_Date;
  RTC_AlarmStructure.RTC_AlarmMask = RTC_AlarmMask_DateWeekDay ;

  RTC_AlarmCmd(RTC_Alarm_A, DISABLE);
  /* Disable the Alarm A */
  RTC_ITConfig(RTC_IT_ALRA, DISABLE);

  /* Clear RTC Alarm Flag */ 
  RTC_ClearFlag(RTC_FLAG_ALRAF);

  RTC_SetAlarm(RTC_Format_BIN, RTC_Alarm_A, &RTC_AlarmStructure);

  /* Enable RTC Alarm A Interrupt: this Interrupt will wake-up the system from
     STANDBY mode (RTC Alarm IT not enabled in NVIC) */
  RTC_ITConfig(RTC_IT_ALRA, ENABLE);

  /* Enable the Alarm A */
  RTC_AlarmCmd(RTC_Alarm_A, ENABLE);

  char buff[32];
  sprintf(buff,"Wake up at %02d:%02d:%02d\r\n", RTC_AlarmStructure.RTC_AlarmTime.RTC_Hours, RTC_AlarmStructure.RTC_AlarmTime.RTC_Minutes, RTC_AlarmStructure.RTC_AlarmTime.RTC_Seconds);
  l_message(NULL,buff);

  mico_rtos_suspend_all_thread();
  PWR_EnterSTANDBYMode();
  /*PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
  init_clocks();*/
  mico_rtos_resume_all_thread();

  l_message( NULL, "RETURN FROM STANDBY"); 
  lua_pushinteger(L, 1);
  return 1;
}

//================================
static int rtc_set( lua_State* L )
{
  mico_rtc_time_t time;
  uint16_t yr;
  
  if (!lua_istable(L, 1)) {
    l_message( NULL, "table arg needed" );
    lua_pushinteger(L,0);
    return 1;
  }
  if (lua_objlen( L, 1 ) != 7) {
    l_message( NULL, "sec,min,hour,wday,date,month,year expected" );
    lua_pushinteger(L,0);
    return 1;
  }
  lua_rawgeti( L, 1, 1 );
  time.sec = ( int )luaL_checkinteger( L, -1 );
  lua_pop( L, 1 );
  lua_rawgeti( L, 1, 2 );
  time.min = ( int )luaL_checkinteger( L, -1 );
  lua_pop( L, 1 );
  lua_rawgeti( L, 1, 3 );
  time.hr = ( int )luaL_checkinteger( L, -1 );
  lua_pop( L, 1 );
  lua_rawgeti( L, 1, 4 );
  time.weekday = ( int )luaL_checkinteger( L, -1 );
  lua_pop( L, 1 );
  lua_rawgeti( L, 1, 5 );
  time.date = ( int )luaL_checkinteger( L, -1 );
  lua_pop( L, 1 );
  lua_rawgeti( L, 1, 6 );
  time.month = ( int )luaL_checkinteger( L, -1 );
  lua_pop( L, 1 );
  lua_rawgeti( L, 1, 7 );
  yr = ( int )luaL_checkinteger( L, -1 );
  lua_pop( L, 1 );
  if (yr >= 2000) yr = yr - 2000;
  time.year = (uint8_t)yr;

  if( MicoRtcSetTime(&time) == kNoErr ){
    lua_pushinteger(L,1);
  }else {
    l_message( NULL, "RTC unsupported"); 
    lua_pushinteger(L,0);
  }  
  return 1;
}

#define MIN_OPT_LEVEL       2
#include "lrodefs.h"
const LUA_REG_TYPE rtc_map[] =
{
  { LSTRKEY( "set" ), LFUNCVAL( rtc_set )},
  { LSTRKEY( "getasc" ), LFUNCVAL( rtc_getasc )},
  { LSTRKEY( "get" ), LFUNCVAL( rtc_get )},
  { LSTRKEY( "getstrf" ), LFUNCVAL( rtc_get_str_time )},
  { LSTRKEY( "getalarm" ), LFUNCVAL( rtc_getalarm )},
  { LSTRKEY( "standby" ), LFUNCVAL( rtc_standby )},
  { LSTRKEY( "standbyUntil" ), LFUNCVAL( rtc_standbyUntil )},
#if LUA_OPTIMIZE_MEMORY > 0
#endif      
  {LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_rtc(lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else    
  luaL_register( L, EXLIB_RTC, rtc_map );
  return 1;
#endif
}
