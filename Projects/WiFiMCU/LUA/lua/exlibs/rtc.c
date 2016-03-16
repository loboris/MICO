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

extern uint8_t use_wwdg;
extern unsigned long wdg_tmo;
extern void luaWdgReload( void );

#define TM_RTC_Int_Disable       0x00    /*!< Disable RTC wakeup interrupts */

/* NVIC global Priority set */
#ifndef RTC_PRIORITY
#define RTC_PRIORITY		0x04
#endif
/* Sub priority for wakeup trigger */
#ifndef RTC_WAKEUP_SUBPRIORITY
#define RTC_WAKEUP_SUBPRIORITY	0x00
#endif
/* Sub priority for alarm trigger */
#ifndef RTC_ALARM_SUBPRIORITY
#define RTC_ALARM_SUBPRIORITY	0x01
#endif

/* RTC declarations */
RTC_TimeTypeDef RTC_TimeStruct;
RTC_InitTypeDef RTC_InitStruct;
RTC_DateTypeDef RTC_DateStruct;
NVIC_InitTypeDef NVIC_InitStruct;
EXTI_InitTypeDef EXTI_InitStruct;

typedef enum {
  TM_RTC_Alarm_A = 0x00, /*!< Work with alarm A */
  TM_RTC_Alarm_B         /*!< Work with alarm B */
} TM_RTC_Alarm_t;


// Callbacks
__weak void TM_RTC_RequestHandler(void) {
  // If user needs this function, then they should be defined separatelly in your project
}

__weak void TM_RTC_AlarmAHandler(void) {
  // If user needs this function, then they should be defined separatelly in your project
}

__weak void TM_RTC_AlarmBHandler(void) {
  // If user needs this function, then they should be defined separatelly in your project
}

// Private RTC IRQ handlers
//----------------------------
void RTC_WKUP_IRQHandler(void) {
  /* Check for RTC interrupt */
  if (RTC_GetITStatus(RTC_IT_WUT) != RESET) {
    /* Clear interrupt flags */
    RTC_ClearITPendingBit(RTC_IT_WUT);
    /* Call user function */
    TM_RTC_RequestHandler();
  }
  /* Clear EXTI line 22 bit */
  EXTI->PR = 0x00400000;
}

//-------------------------------
void RTC_Alarm_IRQHandler(void) {
  /* RTC Alarm A check */
  if (RTC_GetITStatus(RTC_IT_ALRA) != RESET) {
    /* Clear RTC Alarm A interrupt flag */
    RTC_ClearITPendingBit(RTC_IT_ALRA);
    /* Call user function for Alarm A */
    TM_RTC_AlarmAHandler();
  }

  /* RTC Alarm B check */
  if (RTC_GetITStatus(RTC_IT_ALRB) != RESET) {
    /* Clear RTC Alarm A interrupt flag */
    RTC_ClearITPendingBit(RTC_IT_ALRB);
    /* Call user function for Alarm B */
    TM_RTC_AlarmBHandler();
  }
  /* Clear EXTI line 17 bit */
  EXTI->PR = 0x00020000;
}

//----------------------------------------------
void TM_RTC_DisableAlarm(TM_RTC_Alarm_t Alarm) {
  switch (Alarm) {
    case TM_RTC_Alarm_A:
      /* Disable Alarm A */
      RTC_AlarmCmd(RTC_Alarm_A, DISABLE);
      /* Disable Alarm A interrupt */
      RTC_ITConfig(RTC_IT_ALRA, DISABLE);
      /* Clear Alarm A pending bit */
      RTC_ClearFlag(RTC_IT_ALRA);
      break;

    case TM_RTC_Alarm_B:
      /* Disable Alarm B */
      RTC_AlarmCmd(RTC_Alarm_B, DISABLE);
      /* Disable Alarm B interrupt */
      RTC_ITConfig(RTC_IT_ALRB, DISABLE);
      /* Clear Alarm B pending bit */
      RTC_ClearFlag(RTC_IT_ALRB);
      break;
    default:
            break;
  }
  /* Clear RTC Alarm pending bit */
  EXTI->PR = 0x00020000;

  /* Configure EXTI 17 as interrupt */
  EXTI_InitStruct.EXTI_Line = EXTI_Line17;
  EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
  EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
  EXTI_InitStruct.EXTI_LineCmd = ENABLE;
  /* Initialite Alarm EXTI interrupt */
  EXTI_Init(&EXTI_InitStruct);
  /* Configure the RTC Alarm Interrupt */
  NVIC_InitStruct.NVIC_IRQChannel = RTC_Alarm_IRQn;
  NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = RTC_PRIORITY;
  NVIC_InitStruct.NVIC_IRQChannelSubPriority = RTC_ALARM_SUBPRIORITY;
  NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
  /* Initialize RTC Alarm Interrupt */
  NVIC_Init(&NVIC_InitStruct);
}

//------------------------------------------
void TM_RTC_Interrupts(uint32_t int_value) {
  /* Clear pending bit */
  EXTI->PR = 0x00400000;

  /* Disable wakeup interrupt */
  RTC_WakeUpCmd(DISABLE);
  /* Disable RTC interrupt flag */
  RTC_ITConfig(RTC_IT_WUT, DISABLE);

  /* NVIC init for RTC */
  NVIC_InitStruct.NVIC_IRQChannel = RTC_WKUP_IRQn;
  NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = RTC_PRIORITY;
  NVIC_InitStruct.NVIC_IRQChannelSubPriority = RTC_WAKEUP_SUBPRIORITY;
  NVIC_InitStruct.NVIC_IRQChannelCmd = DISABLE;
  NVIC_Init(&NVIC_InitStruct); 

  /* RTC connected to EXTI_Line22 */
  EXTI_InitStruct.EXTI_Line = EXTI_Line22;
  EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
  EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
  EXTI_InitStruct.EXTI_LineCmd = DISABLE;
  EXTI_Init(&EXTI_InitStruct);

  if (int_value != TM_RTC_Int_Disable) {
    /* Enable NVIC */
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct); 
    /* Enable EXT1 interrupt */
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStruct);

    /* First disable wake up command */
    RTC_WakeUpCmd(DISABLE);

    /* Clock divided by 8, 32768 / 8 = 4096 */
    /* 4096 ticks for 1second interrupt */
    RTC_WakeUpClockConfig(RTC_WakeUpClock_RTCCLK_Div8);
    /* Set RTC wakeup counter */
    RTC_SetWakeUpCounter(0x0FFF * int_value);
    /* Enable wakeup interrupt */
    RTC_ITConfig(RTC_IT_WUT, ENABLE);
    /* Enable wakeup command */
    RTC_WakeUpCmd(ENABLE);
  }
}


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
    char tmpt[32];
    sprintf(tmpt, "%s", asctime(&currentTime));
    tmpt[strlen(tmpt)-1] = 0x00;
    lua_pushstring(L, tmpt);
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
  char buff[60];

  uint8_t mode = luaL_checkinteger( L, 1 );
  if (mode > 1) {
    l_message( NULL, "mode has to be 0 or 1" );
    return 0;
  }
  if (mode==1 && use_wwdg == 0) {
    l_message(NULL,"IWDG active, cannot enter STOP mode."); 
    return 0;
  }
  int nsec = luaL_checkinteger( L, 2 );
  if ((nsec < 1) || (nsec > 84559)) {
    l_message(NULL,"wrong interval (1~84599)"); 
    return 0;
  }

  TM_RTC_DisableAlarm(TM_RTC_Alarm_A);
  TM_RTC_DisableAlarm(TM_RTC_Alarm_B);

  platform_rtc_time_t time;
  uint32_t currentSecond;
  RTC_AlarmTypeDef  RTC_AlarmStructure;

  platform_rtc_get_time(&time);
  currentSecond = time.hr*3600 + time.min*60 + time.sec;
  currentSecond += nsec;
  RTC_AlarmStructure.RTC_AlarmTime.RTC_H12     = RTC_HourFormat_24;
  RTC_AlarmStructure.RTC_AlarmTime.RTC_Hours   = currentSecond/3600%24;
  RTC_AlarmStructure.RTC_AlarmTime.RTC_Minutes = currentSecond/60%60;
  RTC_AlarmStructure.RTC_AlarmTime.RTC_Seconds = currentSecond%60;
  RTC_AlarmStructure.RTC_AlarmDateWeekDay = 0x31;
  RTC_AlarmStructure.RTC_AlarmDateWeekDaySel = RTC_AlarmDateWeekDaySel_Date;
  RTC_AlarmStructure.RTC_AlarmMask = RTC_AlarmMask_DateWeekDay ;

  RTC_SetAlarm(RTC_Format_BIN, RTC_Alarm_A, &RTC_AlarmStructure);

  // Enable RTC Alarm A Interrupt
  RTC_ITConfig(RTC_IT_ALRA, ENABLE);
  // Enable the Alarm A
  RTC_AlarmCmd(RTC_Alarm_A, ENABLE);
  /* Clear Alarm A pending bit */
  /* Clear RTC Alarm Flag */ 
  RTC_ClearFlag(RTC_FLAG_ALRAF);
  RTC_ClearFlag(RTC_IT_ALRA);

  if (mode == 0) sprintf(buff,"Going to STANDBY MODE...\r\n");
  else if (mode == 1) sprintf(buff,"Going to STOP MODE...\r\n");
  l_message(NULL,buff);
  sprintf(buff,"Wake up in %d second(s)\r\n", nsec);
  l_message(NULL,buff);

  //mico_rtos_suspend_all_thread();
  if (mode == 0) {
    PWR_EnterSTANDBYMode();
    // RESET
  }
  else if (mode == 1) {
    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
    // restore clocks
    init_clocks();
  }
  //mico_rtos_resume_all_thread();

  // *** Back from stop ***
  TM_RTC_DisableAlarm(TM_RTC_Alarm_A);
  TM_RTC_DisableAlarm(TM_RTC_Alarm_B);
  luaWdgReload();

  l_message(NULL,"Back from power save mode.");
  return 0;
}

//=========================================
static int rtc_standbyUntil( lua_State* L )
{
  uint8_t h,m,s;
  RTC_AlarmTypeDef  RTC_AlarmStructure;
  char buff[64];
  
  uint8_t mode = luaL_checkinteger( L, 1 );
  if (mode > 1) {
    l_message( NULL, "mode has to be 0 or 1" );
    return 0;
  }
  if (mode==1 && use_wwdg == 0) {
    l_message(NULL,"IWDG active, cannot enter STOP mode."); 
    return 0;
  }
  
  if (!lua_istable(L, 2)) {
    l_message( NULL, "table arg needed" );
    return 0;
  }
  if (lua_objlen( L, 2 ) != 3) {
    l_message( NULL, "hour,minute,second expected" );
    return 0;
  }
  lua_rawgeti( L, 2, 1 );
  h = ( int )luaL_checkinteger( L, -1 );
  lua_pop( L, 1 );
  lua_rawgeti( L, 2, 2 );
  m = ( int )luaL_checkinteger( L, -1 );
  lua_pop( L, 1 );
  lua_rawgeti( L, 2, 3 );
  s = ( int )luaL_checkinteger( L, -1 );
  lua_pop( L, 1 );

  TM_RTC_DisableAlarm(TM_RTC_Alarm_A);
  TM_RTC_DisableAlarm(TM_RTC_Alarm_B);

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
  RTC_ClearFlag(RTC_IT_ALRA);

  RTC_SetAlarm(RTC_Format_BIN, RTC_Alarm_A, &RTC_AlarmStructure);

  /* Enable RTC Alarm A Interrupt: this Interrupt will wake-up the system from
     STANDBY mode (RTC Alarm IT not enabled in NVIC) */
  RTC_ITConfig(RTC_IT_ALRA, ENABLE);

  /* Enable the Alarm A */
  RTC_AlarmCmd(RTC_Alarm_A, ENABLE);

  if (mode == 0) sprintf(buff,"Going to STANDBY MODE...\r\n");
  else if (mode == 1) sprintf(buff,"Going to STOP MODE...\r\n");
  l_message(NULL,buff);
  sprintf(buff,"Wake up at %02d:%02d:%02d\r\n", RTC_AlarmStructure.RTC_AlarmTime.RTC_Hours, RTC_AlarmStructure.RTC_AlarmTime.RTC_Minutes, RTC_AlarmStructure.RTC_AlarmTime.RTC_Seconds);
  l_message(NULL,buff);

  //mico_rtos_suspend_all_thread();
  if (mode == 0) {
    PWR_EnterSTANDBYMode();
  }
  else if (mode == 1) {
    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
    init_clocks();
  }
  //mico_rtos_resume_all_thread();

  // *** Back from stop ***
  TM_RTC_DisableAlarm(TM_RTC_Alarm_A);
  TM_RTC_DisableAlarm(TM_RTC_Alarm_B);
  luaWdgReload();

  l_message(NULL,"Back from power save mode.");
  return 0;
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
