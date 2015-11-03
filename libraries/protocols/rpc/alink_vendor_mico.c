#include "platform.h"
#include "MICO.h"
#include "MICODefine.h"
#include "MICONotificationCenter.h"

#include "alink_export.h"
#include "alink_export_rawdata.h"
#include "alink_vendor_mico.h"
#include "json.h"
#include "device.h"

extern const char *mico_generate_cid(void);
#ifdef SANDBOX
static const char* g_cert_str_sandbox = 
"-----BEGIN CERTIFICATE-----\r\n"
"MIIEoTCCA4mgAwIBAgIJAMQSGd3GuNGVMA0GCSqGSIb3DQEBBQUAMIGRMQswCQYD\r\n"
"VQQGEwJDTjERMA8GA1UECBMIWmhlamlhbmcxETAPBgNVBAcTCEhhbmd6aG91MRYw\r\n"
"FAYDVQQKEw1BbGliYWJhIEdyb3VwMQ4wDAYDVQQLEwVBbGluazEOMAwGA1UEAxMF\r\n"
"QWxpbmsxJDAiBgkqhkiG9w0BCQEWFWFsaW5rQGFsaWJhYmEtaW5jLmNvbTAeFw0x\r\n"
"NDA4MjkwMzA5NDhaFw0yNDA4MjYwMzA5NDhaMIGRMQswCQYDVQQGEwJDTjERMA8G\r\n"
"A1UECBMIWmhlamlhbmcxETAPBgNVBAcTCEhhbmd6aG91MRYwFAYDVQQKEw1BbGli\r\n"
"YWJhIEdyb3VwMQ4wDAYDVQQLEwVBbGluazEOMAwGA1UEAxMFQWxpbmsxJDAiBgkq\r\n"
"hkiG9w0BCQEWFWFsaW5rQGFsaWJhYmEtaW5jLmNvbTCCASIwDQYJKoZIhvcNAQEB\r\n"
"BQADggEPADCCAQoCggEBAMHr21qKVy3g1GKWdeGQj3by+lN7dMjGoPquLxiJHzEs\r\n"
"6auxiAiWez8pFktlekIL7FwK5F7nH1px5W5G8s3cTSqRvunex/Cojw8LbNAStpXy\r\n"
"HrqUuDhL3DYF7x87M/7H3lqFuIlucSKNC60Yc03yuIR1I/W0di40rDNeXYuCzXIv\r\n"
"yheg+b7zD939HOe+RS878hDW5/p75FY+ChI8GA4dPEQb5fyRrjHAXneo+S8jdnqr\r\n"
"SCjHSS7+jI36dyEfZ72rkLNJ3v1WboH02Rchj1fFIfagn+Ij4v0ruejOTIvc/ngD\r\n"
"OLZNTUyF4B3EG4IAZRlO12jDECc4Com0yfFQ0IxkNVMCAwEAAaOB+TCB9jAdBgNV\r\n"
"HQ4EFgQU9iyOWx+oGSOhdlpHeWMYsHXRwwkwgcYGA1UdIwSBvjCBu4AU9iyOWx+o\r\n"
"GSOhdlpHeWMYsHXRwwmhgZekgZQwgZExCzAJBgNVBAYTAkNOMREwDwYDVQQIEwha\r\n"
"aGVqaWFuZzERMA8GA1UEBxMISGFuZ3pob3UxFjAUBgNVBAoTDUFsaWJhYmEgR3Jv\r\n"
"dXAxDjAMBgNVBAsTBUFsaW5rMQ4wDAYDVQQDEwVBbGluazEkMCIGCSqGSIb3DQEJ\r\n"
"ARYVYWxpbmtAYWxpYmFiYS1pbmMuY29tggkAxBIZ3ca40ZUwDAYDVR0TBAUwAwEB\r\n"
"/zANBgkqhkiG9w0BAQUFAAOCAQEAO7u7ozEES2TgTepq3ZTk1VD5qh2zhcSLLr+b\r\n"
"yDQlkbm0lnah/GOGmpr/Wlr8JSXUJEWhsLcbnG1dhbP72DzFHri8ME4wO8hbeyXU\r\n"
"7cFOHmP4DZi+Ia2gyy/GZ66P6L9df89MJzMOt46NYn+A3rI12M0qTJ6GNdUHz2R9\r\n"
"VGkahs6bfGQGi0Se24jj4Es+MeAlrG5U0d0wVY5Dt4jpS9wHLupyAiANbj4Ls5x2\r\n"time_t
"6cwS4+Q4ErezFMnKiQ5VKc6S88ohYszt82CYMPEqIiZRkCfjsVz/r6vw2DFwN0Ox\r\n"
"8Cb9myZwXypcOZbI7M+9W8909Z+TSHW1UlNkyrIsqDGuzE866w==\r\n"
"-----END CERTIFICATE-----\r\n";

const char* sandbox_server = "110.75.122.2";
int sandbox_port = 80;


void sandbox_setting() {
  struct alink_config con;
  con.cert = g_cert_str_sandbox;
  con.server = sandbox_server;
  con.port = sandbox_port;
  con.factory_reset = 0;
  
  alink_set_config(&con);
}

#endif

const char *mico_get_mac_addr()
{
  mico_Context_t *context = getGlobalContext();
  return context->micoStatus.mac;
}



int get_available_memory(void* p1, void* p2)
{
  return MicoGetMemoryInfo()->free_memory;
}

volatile char alink_network_ready;
int alink_network_callback(void *mac, void *state)
{

  if (*(int *)state == ALINK_STATUS_LOGGED) {
      alink_network_ready = 1;
  }

  return 0;
}

int mico_start_alink(void)
{
  mico_Context_t *context = getGlobalContext();
  int ret = -1;
  
  struct device_info* dev = (struct device_info*) malloc(sizeof(struct device_info));
  
  memset(dev, 0, sizeof(struct device_info));
  strcpy(dev->key, ALINK_KEY);
  strcpy(dev->secret, ALINK_SECRET);
  
  sprintf(dev->name, "%s(%c%c%c%c)",DEV_NAME,
          context->micoStatus.mac[12], context->micoStatus.mac[13],
          context->micoStatus.mac[15], context->micoStatus.mac[16]);
  
  strcpy(dev->model, DEV_MODEL);
  strcpy(dev->manufacturer, DEV_MANUFACTURE);
  
  strcpy(dev->type, DEV_TYPE);
  strcpy(dev->category, DEV_CATEGORY);
  strcpy(dev->sn, DEV_SN);
  strcpy(dev->version, DEV_VERSION);
  memcpy(dev->mac, mico_get_mac_addr(), 18);
  strcpy(dev->cid, mico_generate_cid());
  
  custom_log("WSF", "dev->mac %s", dev->mac);
  custom_log("WSF", "dev->cid %s", dev->cid);
  
  dev->dev_callback[ACB_GET_DEVICE_STATUS] = device_status_post;
  dev->dev_callback[ACB_SET_DEVICE_STATUS] = device_command_execute;  
  dev->sys_callback[ALINK_FUNC_SERVER_STATUS] = alink_network_callback;
  
  alink_set_callback(ALINK_FUNC_AVAILABLE_MEMORY, get_available_memory);
  
  alink_set_loglevel(ALINK_LL_DEBUG);
  
#ifdef SANDBOX
  sandbox_setting();
#endif
  if(context->flashContentInRam.appConfig.resetflag==1)
  {
    struct alink_config con;
    memset(&con, 0, sizeof(struct alink_config));
    con.factory_reset = 1;
    
    alink_set_config(&con);
  }
  
  ret = alink_start(dev);
  
  free((void*)dev);
  
  return ret;
}


