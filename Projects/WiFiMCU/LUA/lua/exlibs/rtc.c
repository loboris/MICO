/**
 * mcu.c
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"
   
#include "mico_platform.h"
#include "user_config.h"
#include "mico_wlan.h"
#include "MICO.h"
#include "StringUtils.h"
#include "time.h" 

static int rtc_getasc( lua_State* L )
{
   struct tm currentTime;
   mico_rtc_time_t ttime;
   if( MicoRtcGetTime(&ttime) == kNoErr ){
    currentTime.tm_sec = ttime.sec;
    currentTime.tm_min = ttime.min;
    currentTime.tm_hour = ttime.hr;
    currentTime.tm_mday = ttime.date;
    currentTime.tm_wday = ttime.weekday;
    currentTime.tm_mon = ttime.month - 1;
    currentTime.tm_year = ttime.year + 100; 
    lua_pushstring(L,asctime(&currentTime));
   }else {
     lua_pushstring(L, "RTC function unsupported"); 
   }  
  return 1;
}

static int rtc_get( lua_State* L )
{
   mico_rtc_time_t ttime;
   if( MicoRtcGetTime(&ttime) == kNoErr ){
    lua_pushinteger( L, ttime.sec);
    lua_pushinteger( L, ttime.min);
    lua_pushinteger( L, ttime.hr);
    lua_pushinteger( L, ttime.weekday);
    lua_pushinteger( L, ttime.date);
    lua_pushinteger( L, ttime.month);
    lua_pushinteger( L, ttime.year); 
    return 7;
   }else {
     lua_pushstring(L, "RTC function unsupported"); 
   }  
  return 1;
}

static int rtc_getalarm( lua_State* L )
{
   RTC_AlarmTypeDef  RTC_AlarmStructure;
   RTC_GetAlarm(RTC_Format_BIN, RTC_Alarm_A, &RTC_AlarmStructure);
   lua_pushinteger( L, RTC_AlarmStructure.RTC_AlarmTime.RTC_H12);
   lua_pushinteger( L, RTC_AlarmStructure.RTC_AlarmTime.RTC_Hours);
   lua_pushinteger( L, RTC_AlarmStructure.RTC_AlarmTime.RTC_Minutes);
   lua_pushinteger( L, RTC_AlarmStructure.RTC_AlarmTime.RTC_Seconds);
   lua_pushinteger( L, RTC_AlarmStructure.RTC_AlarmDateWeekDay);
   lua_pushinteger( L, RTC_AlarmStructure.RTC_AlarmDateWeekDaySel);
   lua_pushinteger( L, RTC_AlarmStructure.RTC_AlarmMask);
   return 7;
}

static int rtc_standby( lua_State* L )
{
   mico_rtos_suspend_all_thread();
   MicoSystemStandBy(luaL_checkinteger( L, 1 ));
   mico_rtos_resume_all_thread();
   lua_pushstring(L, "RETURN FROM STANDBY"); 
   return 1;
}

static int rtc_set( lua_State* L )
{
   mico_rtc_time_t time;
   
   time.sec = luaL_checkinteger( L, 1 );
   time.min = luaL_checkinteger( L, 2 );
   time.hr = luaL_checkinteger( L, 3 );
   time.weekday = luaL_checkinteger( L, 4 );
   time.date = luaL_checkinteger( L, 5 );
   time.month = luaL_checkinteger( L, 6 );
   time.year = luaL_checkinteger( L, 7 );

   if( MicoRtcSetTime(&time) == kNoErr ){
    lua_pushstring(L,"OK");
   }else {
     lua_pushstring(L, "RTC function unsupported"); 
   }  
  return 1;
}

#define MIN_OPT_LEVEL       2
#include "lrodefs.h"
const LUA_REG_TYPE rtc_map[] =
{
  { LSTRKEY( "getasc" ), LFUNCVAL( rtc_getasc )},
  { LSTRKEY( "set" ), LFUNCVAL( rtc_set )},
  { LSTRKEY( "get" ), LFUNCVAL( rtc_get )},
  { LSTRKEY( "getalarm" ), LFUNCVAL( rtc_getalarm )},
  { LSTRKEY( "standby" ), LFUNCVAL( rtc_standby )},
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
