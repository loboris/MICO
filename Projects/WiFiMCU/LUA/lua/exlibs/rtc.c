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

static int rtc_standbyUntil( lua_State* L )
{
    RTC_AlarmTypeDef  RTC_AlarmStructure;
 
    mico_rtos_suspend_all_thread();

    RTC_AlarmStructure.RTC_AlarmTime.RTC_H12     = RTC_HourFormat_24;
    RTC_AlarmStructure.RTC_AlarmTime.RTC_Hours   = luaL_checkinteger( L, 1 );
    RTC_AlarmStructure.RTC_AlarmTime.RTC_Minutes = luaL_checkinteger( L, 2 );
    RTC_AlarmStructure.RTC_AlarmTime.RTC_Seconds = luaL_checkinteger( L, 3 );
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

    PWR_EnterSTANDBYMode();
   
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
