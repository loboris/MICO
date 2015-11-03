
#ifdef _PLATFORM_MICO_


#include "platform.h"
#include "MICO.h"
#include "MICODefine.h"
#include "MICONotificationCenter.h"
#include "alink_export.h"
#include "time.h"


static unsigned int synced_time = 0, synced_utc_time = 0;

USED alink_time_t convert_time(const char *str)
{
    time_t t;
    sscanf(str, "%lu", &t);
    return t;
}

/*------------------------------------------------------------------------------*/

void clearLocalUTC()
{
  synced_utc_time = 0;
}

void setLocalUTC(const char *str)
{
  
  unsigned int t;
  
  sscanf(str, "%d", &t);
  synced_time = mico_get_time();
  synced_utc_time = t;
  
  //    setDeviceTime(t);
  
}

unsigned int getLocalUTC()
{
  
  return synced_utc_time + (mico_get_time() - synced_time) / 1000;
  
}

int needSyncUTC()
{
  int now;
  if (synced_utc_time == 0)
    return 1;
  
  now = mico_get_time();
  if (now < synced_time)
    return 1;
  
  // require sync utc time every 30 mins
  if ((now - synced_time) / 1000 > (30 * 60))
    return 1;
  
  return 0;
  
}

/*------------------------------------------------------------------------------*/

int set_kv(const char *k, const void *v, size_t len)
{
  mico_Context_t *context = getGlobalContext();
  
  memcpy(context->flashContentInRam.appConfig.uuid, v, 32);
  context->flashContentInRam.appConfig.uuid[32] = '\0';
  MICOUpdateConfiguration(context);
  return ALINK_OK;
}

int get_kv(const char *k, char *v, size_t len)
{
  mico_Context_t *context = getGlobalContext();
  if (context->flashContentInRam.appConfig.uuid[0] != (unsigned char)0xff) {
    memcpy(v, context->flashContentInRam.appConfig.uuid, 32);
    v[32] = '\0';
  } else {
    v[0] = '\0';
  }
  
  return ALINK_OK;
}

int is_main_uuid_inited()
{
  mico_Context_t *context = getGlobalContext();

  return context->flashContentInRam.appConfig.uuid[0] != (unsigned char)0xff;
}

void alink_sleep(int millsec)
{
  msleep(millsec);
}

/*----------------------------------------------------------------------------*/

extern bool global_wifi_status;

void wait_network_up()
{
  while(global_wifi_status == false) {
    msleep(100);
  }
}


void wait_network_connected()
{
  wait_network_up();
}

int prepare_network()
{
  return ALINK_OK;
}

#endif
