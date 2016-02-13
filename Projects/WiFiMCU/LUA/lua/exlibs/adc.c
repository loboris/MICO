/**
 * adc.c
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"
#include "mico_platform.h"
#include "math.h"

#define MAX_SAMPLES 128

extern const char wifimcu_gpio_map[];

const char wifimcu_adc_map[] =
{
  [MICO_GPIO_9]   = MICO_ADC_1, // D1
  [MICO_GPIO_34]  = MICO_ADC_2, // D13
  [MICO_GPIO_36]  = MICO_ADC_3, // D15
  [MICO_GPIO_37]  = MICO_ADC_4, // D16
  [MICO_GPIO_38]  = MICO_ADC_5, // D17
};

static float adc_ref = 3.3;
static uint8_t adc_autocal = 0;

static int platform_adcpin_exists( unsigned pin )
{
  if(pin ==1 || pin ==13 ||  pin ==15 || pin ==16 ||  pin ==17) 
       return true;
  else
    return false;

}

//-----------------------------------
static uint16_t _getLen(lua_State* L)
{
  if (lua_gettop(L) < 2) return 1;
  int len = luaL_checkinteger( L, 2);
  if (len < 2) return 1;
  if (len > MAX_SAMPLES) return MAX_SAMPLES;
  return (uint16_t)len;
}

//------------------------------
static int _getPin(lua_State* L)
{
  unsigned pin = luaL_checkinteger( L, 1);
  int adcPinID;
  if (pin < 18) {
    MOD_CHECK_ID( adcpin, pin);
    adcPinID = wifimcu_adc_map[wifimcu_gpio_map[pin]];  
  }
  else {
    if (pin == 98) adcPinID = MICO_ADC_6;
    else if (pin == 99) adcPinID = MICO_ADC_7;
    else adcPinID = 999;
  }
  return adcPinID;
}

//-----------------------------------------------------------------------
static float _adcRead(int adcpin, uint16_t len, uint8_t type, float* std)
{
  uint16_t data[MAX_SAMPLES];
  *std = 0.0;

  if (adcpin == 999) return -99999;
  // init ADC
  if (kNoErr != MicoAdcInitialize((mico_adc_t)adcpin, 3)) return -99999.0;
  // get ADC data
  if (len < 2) {
    if (kNoErr != MicoAdcTakeSample((mico_adc_t)adcpin, &data[0])) return -99999.0;
  }
  else {
    if (kNoErr != MicoAdcTakeSampleStream((mico_adc_t)adcpin, &data[0], len)) return -99999.0;
  }
  
  float adata, sum, sum2;

  if (len > 1) {
    uint16_t i;
    sum = 0.0;
    for (i=0; i<len; i++) {
      if (type == 0) adata = (float)data[i];
      else adata = ((float)data[i] * adc_ref) / 4095.0;
      if (adcpin == MICO_ADC_6) {
        // temperature
        adata = ((adata - 0.760) / 0.0025) + 25.0;
      }
      sum += adata;
    }
    sum = sum / (float)len; // data mean
      
    sum2 = 0.0;
    for (i=0; i<len; i++) {
      if (type == 0) adata = (float)data[i];
      else adata = ((float)data[i] * adc_ref) / 4095.0;
      if (adcpin == MICO_ADC_6) {
        // temperature
        adata = ((adata - 0.760) / 0.0025) + 25.0;
      }
      sum2 += (adata - sum) * (adata - sum);
    }
    *std = sqrt(sum2 / (float)len);
  }
  else {
    sum = (float)data[0];
    if (type == 1) {
      if (adcpin == MICO_ADC_6) {
        // temperature
        sum = ((((sum * adc_ref) / 4095.0) - 0.760) / 0.0025) + 25.0;
      }
      else sum = (sum * adc_ref) / 4095.0;
    }
  }
  
  return sum;
}

//------------------------------------------------------------
static float _adc_read(lua_State* L, uint8_t type, float* std)
{
  int adcpin = _getPin(L);
  uint16_t len = _getLen(L);
  
  if ((type == 0) && (adcpin == MICO_ADC_6)) return -99999.0;

  if (adc_autocal) {
    float res = _adcRead(MICO_ADC_7, len, 1, std);
    if (res > -99990.0) {
      adc_ref = ((1.21 / res) * 3.3);
    }
  }
  return _adcRead(adcpin, len, type, std);
}

//---------------------------------
static int adc_read( lua_State* L )
{
  float std;
  int res = (int)_adc_read(L, 0, &std);

  if (res > -99990.0) {
    lua_pushinteger(L, res);
    lua_pushnumber(L, std);
    return 2;
  }
  
  lua_pushnil(L);
  return 1;
}

//-----------------------------------
static int adc_read_v( lua_State* L )
{
  float std;
  float res = _adc_read(L, 1, &std);
  if (res > -99990.0) {
    lua_pushnumber(L, res);
    lua_pushnumber(L, std);
    return 2;
  }
  
  lua_pushnil(L);
  return 1;
}

//-----------------------------------
static int adc_setref( lua_State* L )
{
  if (lua_gettop(L) >= 1) {
    float vref = luaL_checknumber( L, 1);
    if ((vref < 3.0) || (vref > 3.6)) vref = 3.3;
    adc_ref = vref;
  }
  else {
    float std;
    float res = _adcRead(MICO_ADC_7, 64, 1, &std);
    if (res > -99990.0) {
      adc_ref = ((1.21 / res) * 3.3);
    }
    else adc_ref = 3.3;
  }
  lua_pushnumber(L, adc_ref);
  return 1;
}

//---------------------------------------
static int adc_setautocal( lua_State* L )
{
  int acal = luaL_checkinteger( L, 1);
  if (acal == 1) adc_autocal = 1;
  else adc_autocal = 0;
  return 0;
}


#define MIN_OPT_LEVEL  2
#include "lrodefs.h"
const LUA_REG_TYPE adc_map[] =
{
  { LSTRKEY( "read" ), LFUNCVAL( adc_read )},
  { LSTRKEY( "readV" ), LFUNCVAL( adc_read_v )},
  { LSTRKEY( "setref" ), LFUNCVAL( adc_setref )},
  { LSTRKEY( "setautocal" ), LFUNCVAL( adc_setautocal )},
#if LUA_OPTIMIZE_MEMORY > 0
#endif    
  {LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_adc(lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else  
  luaL_register( L, EXLIB_ADC, adc_map );
  return 1;
#endif
}
