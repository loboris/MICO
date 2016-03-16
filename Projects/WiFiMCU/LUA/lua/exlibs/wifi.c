/**
 * wifi.c
 */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"

#include "platform.h"
//#include "MiCO.h"
#include "mico.h"
#include "system.h"
#include "mico_wlan.h"
#include "mico_system.h"
#include "sntp.h"
#include "Common.h"

static lua_State *gL=NULL;
static int wifi_scan_succeed = LUA_NOREF;
static int wifi_status_changed_AP = LUA_NOREF;
static int wifi_status_changed_STA = LUA_NOREF;
static uint8_t wifi_scanned = 0;
static uint8_t wifi_scanned_print = 0;
static uint8_t wifi_sta_started = 0;
static uint8_t wifi_ap_started = 0;

extern mico_queue_t os_queue;
extern void luaWdgReload( void );
extern int getLua_systemParams(lua_system_param_t* lua_system_param);

//char gWiFiSSID[33], gWiFiPSW[65];

typedef struct _ApList {  
    char ssid[32];  /**< The SSID of an access point.*/
    char ApPower;   /**< Signal strength, min:0, max:100*/
    char bssid[6];  /**< The BSSID of an access point.*/
    char channel;   /**< The RF frequency, 1-13*/
    SECURITY_TYPE_E security;   /**< Security type, @ref SECURITY_TYPE_E*/
} _ApList; 


//static bool log = true;
//#define wifi_log(M, ...) if (log == true) printf(M, ##__VA_ARGS__)

//------------------------------------
static int is_valid_ip(const char *ip) 
{
  int n[4];
  char c[4];
  if (sscanf(ip, "%d%c%d%c%d%c%d%c",
             &n[0], &c[0], &n[1], &c[1], &n[2], &c[2], &n[3], &c[3]) == 7) {
    int i;
    for (i = 0; i < 3; ++i) {
      if (c[i] != '.') return 0;
    }
    for (i = 0; i < 4; ++i) {
      if (n[i] > 255 || n[i] < 0) return 0;
    }
    return 1;
  }
  else return 0;
}

//------------------------------------------------------------------------------------------
static void _micoNotify_WifiStatusHandler(WiFiEvent event, mico_Context_t * const inContext)
{
  (void)inContext;
  
  queue_msg_t msg;
  msg.L = gL;
  msg.source = onWIFI;
  msg.para3 = NULL;
  msg.para4 = NULL;
  
  switch (event) {
    case NOTIFY_STATION_UP:
      wifi_sta_started = 1;
      if (wifi_status_changed_STA != LUA_NOREF) {
        msg.para1 = 0;
        msg.para2 = wifi_status_changed_STA;
        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
      break;
    case NOTIFY_STATION_DOWN:
      wifi_sta_started = 0;
      if (wifi_status_changed_STA != LUA_NOREF) {
        msg.para1 = 1;
        msg.para2 = wifi_status_changed_STA;
        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
      break;
    case NOTIFY_AP_UP:
      wifi_ap_started = 1;
      if (wifi_status_changed_AP != LUA_NOREF) {
        msg.para1 = 2;
        msg.para2 = wifi_status_changed_AP;
        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
      break;
    case NOTIFY_AP_DOWN:
      wifi_ap_started = 0;
      if (wifi_status_changed_AP != LUA_NOREF) {
        msg.para1 = 3;
        msg.para2 = wifi_status_changed_AP;
        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
      break;
    default:
      if (wifi_status_changed_AP != LUA_NOREF) {
        msg.para1 = 4;
        msg.para2 = wifi_status_changed_AP;
        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
      if (wifi_status_changed_STA != LUA_NOREF) {
        msg.para1 = 4;
        msg.para2 = wifi_status_changed_STA;
        mico_rtos_push_to_queue( &os_queue, &msg, 0);
      }
      break;
  }
}

/*
cfg={}
cfg.ssid=""
cfg.pwd=""
cfg.ip             (optional, default: 11.11.11.1)
cfg.netmask        (optional, default: 255.255.255.0)
cfg.gateway        (optional, default: 11.11.11.1)
cfg.dnsSrv         (optional, default: 11.11.11.1)
cfg.retry_interval (optional, default: 1000ms)

wifi.startap(cfg, function(optional))
*/
//======================================
static int lwifi_startap( lua_State* L )
{
  //4 stations Max
  network_InitTypeDef_st wNetConfig;
  size_t len=0;
  
  memset(&wNetConfig, 0x0, sizeof(network_InitTypeDef_st));
  
  if (!lua_istable(L, 1)) {
    // ==== Call without parameters, return status ===
    if (wifi_ap_started == 1) lua_pushboolean(L, true);
    else lua_pushboolean(L, false);
    return 1;
  }
  
  //ssid  
  lua_getfield(L, 1, "ssid");
  if (!lua_isnil(L, -1)) {
    if( lua_isstring(L, -1) ) {
      const char *ssid = luaL_checklstring( L, -1, &len );
      if(len >= 32) return luaL_error( L, "ssid: <32" );
      strncpy(wNetConfig.wifi_ssid,ssid,len);
    } 
    else return luaL_error( L, "wrong arg type: ssid" );
  }
  else return luaL_error( L, "arg: ssid needed" );

  //pwd  
  lua_getfield(L, 1, "pwd");
  if (!lua_isnil(L, -1)) {
    if( lua_isstring(L, -1) ) {
      const char *pwd = luaL_checklstring( L, -1, &len );
      if (len >= 64) return luaL_error( L, "pwd: <64" );
      if (len > 0) strncpy(wNetConfig.wifi_key, pwd, len);
      else strcpy(wNetConfig.wifi_key, "");  
    } 
    else return luaL_error( L, "wrong arg type: pwd" );
  }
  else return luaL_error( L, "arg: pwd needed" );
  
  //ip  
  lua_getfield(L, 1, "ip");
  if (!lua_isnil(L, -1)) {
    if( lua_isstring(L, -1) ) {
      const char *ip = luaL_checklstring( L, -1, &len );
      if (len >= 16) return luaL_error( L, "ip: <16" );
      if (is_valid_ip(ip) == false) return luaL_error( L, "ip invalid" );
      strncpy(wNetConfig.local_ip_addr, ip, len);
    } 
    else return luaL_error( L, "wrong arg type: ip" );
  }
  else {
    strcpy(wNetConfig.local_ip_addr, "11.11.11.1");
  }

  //netmask  
  lua_getfield(L, 1, "netmask");
  if (!lua_isnil(L, -1)) {
    if ( lua_isstring(L, -1) ) {
      const char *netmask = luaL_checklstring( L, -1, &len );
      if (len >= 16) return luaL_error( L, "netmask: <16" );
      if (is_valid_ip(netmask) == false) return luaL_error( L, "netmask invalid" );
      strncpy(wNetConfig.net_mask,netmask,len);
    }
    else return luaL_error( L, "wrong arg type: netmask" );
  }
  else {
    strcpy(wNetConfig.net_mask, "255.255.255.0");
  }

  //gateway  
  lua_getfield(L, 1, "gateway");
  if (!lua_isnil(L, -1)) {
    if ( lua_isstring(L, -1) ) {
      const char *gateway = luaL_checklstring( L, -1, &len );
      if (len >= 16) return luaL_error( L, "gateway: <16" );
      if (is_valid_ip(gateway) == false) return luaL_error( L, "gateway invalid" );
      strncpy(wNetConfig.gateway_ip_addr,gateway,len);
    }
    else return luaL_error( L, "wrong arg type: gateway" );
  }
  else {
    strcpy(wNetConfig.gateway_ip_addr,"11.11.11.1");
  }

  //dnsSrv  
  lua_getfield(L, 1, "dnsSrv");
  if (!lua_isnil(L, -1)) {
    if ( lua_isstring(L, -1) ) {
      const char *dnsSrv = luaL_checklstring( L, -1, &len );
      if (len >= 16) return luaL_error( L, "dnsSrv: <16" );
      if (is_valid_ip(dnsSrv) == false) return luaL_error( L, "dnsSrv invalid" );
      strncpy(wNetConfig.dnsServer_ip_addr,dnsSrv,len);
    }
    else return luaL_error( L, "wrong arg type: dnsSrv" );
  }
  else {
    strcpy(wNetConfig.dnsServer_ip_addr, "11.11.11.1");
  }

  //retry_interval
  signed retry_interval = 0;
  lua_getfield(L, 1, "retry_interval");
  if (!lua_isnil(L, -1)) {
      retry_interval = (signed)luaL_checknumber( L, -1 );
      if (retry_interval < 0) return luaL_error( L, "retry_interval: >=0ms" );
      if (retry_interval == 0) retry_interval = 0x7FFFFFFF;
  }
  else retry_interval = 1000; 
  wNetConfig.wifi_retry_interval = retry_interval;
  
  //notify
  gL = L;

  if (wifi_status_changed_AP != LUA_NOREF)
    luaL_unref(L, LUA_REGISTRYINDEX, wifi_status_changed_AP);
  wifi_status_changed_AP = LUA_NOREF;

  if (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, 2);  // copy argument (func) to the top of stack
    if (wifi_status_changed_AP != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, wifi_status_changed_AP);    
      
    wifi_status_changed_AP = luaL_ref(L, LUA_REGISTRYINDEX);
  } 
  mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED, (void *)_micoNotify_WifiStatusHandler, NULL );

  //start
  wifi_ap_started = 0;
  wNetConfig.dhcpMode = DHCP_Server;  
  wNetConfig.wifi_mode = Soft_AP;
  micoWlanStart(&wNetConfig);  

  int tmo = mico_get_time();
  while (wifi_ap_started == 0) {
    if ((mico_get_time() - tmo) > 1000) break;
    mico_thread_msleep(10);
    luaWdgReload();
  }
  if (wifi_ap_started == 1) lua_pushboolean(L, true);
  else lua_pushboolean(L, false);
  return 1;
}

//------------------------------
static void _stopWifiSta(void) {
  network_InitTypeDef_st wNetConfig;
  
  memset(&wNetConfig, 0x00, sizeof(network_InitTypeDef_st));
  wNetConfig.wifi_mode = Station;
  wNetConfig.wifi_retry_interval = 0x7FFFFFFF;
  sprintf((char*)&wNetConfig.dnsServer_ip_addr, "208.67.222.222");
  sprintf((char*)&wNetConfig.gateway_ip_addr, "0.0.0.0");
  sprintf((char*)&wNetConfig.local_ip_addr, "0.0.0.0");
  sprintf((char*)&wNetConfig.net_mask, "0.0.0.0");
    
  micoWlanSuspendStation();
  //mico_thread_msleep(10);
  micoWlanStart(&wNetConfig);
  mico_thread_msleep(10);

  micoWlanSuspendStation();
  wifi_sta_started = 0;
}

/*
cfg={}
cfg.ssid=""
cfg.pwd=""
cfg.dhcp=enable/disable (default:enable)
cfg.ip                  (depends on dhcp)
cfg.netmask             (depends on dhcp)
cfg.gateway             (depends on dhcp)
cfg.dnsSrv              (depends on dhcp)
cfg.retry_interval      (default:1000ms)

wifi.startap(cfg)
*/
//=======================================
static int lwifi_startsta( lua_State* L )
{
  LinkStatusTypeDef link;
  network_InitTypeDef_st wNetConfig;
  size_t len=0;
  lua_system_param_t lua_system_param;
  uint8_t has_default = 0;
  signed retry_interval = 0;
  int con_wait = 0;
  int tmo = mico_get_time();
  
  // check if wifi is already connected  
  memset(&link, 0x00, sizeof(link));
  micoWlanGetLinkStatus(&link);

  if (!lua_istable(L, 1)) {
    // ==== Call without parameters, return connection status ===
    if (link.is_connected != 0) lua_pushboolean(L, true);
    else lua_pushboolean(L, false);
    return 1;
  }

  // ==== parameters exists, configure and start ====
  if (wifi_sta_started == 1) _stopWifiSta();
  
  memset(&wNetConfig, 0x0, sizeof(network_InitTypeDef_st));
  // check if default params exists
  if (getLua_systemParams(&lua_system_param) == 1) {
    if ((strlen(lua_system_param.wifi_ssid) > 0) && (strlen(lua_system_param.wifi_key) > 0)) {
      has_default = 1;
    }
  }
  
  //wait for connection
  lua_getfield(L, 1, "wait");
  if (!lua_isnil(L, -1)) {
      int wfc = luaL_checkinteger( L, -1 );
      if ((wfc > 0) && (wfc < 16)) con_wait = wfc * 1000;
      else return luaL_error( L, "wait must be 1 ~ 15");
  }

  //ssid  
  lua_getfield(L, 1, "ssid");
  if (!lua_isnil(L, -1)) {
    if ( lua_isstring(L, -1) ) {
      const char *ssid = luaL_checklstring( L, -1, &len );
      if (len >= 32) return luaL_error( L, "ssid: <32" );
      strncpy(wNetConfig.wifi_ssid,ssid,len);
    } 
    else return luaL_error( L, "wrong arg type:ssid" );
  }
  else if (has_default == 1) {
    strcpy(wNetConfig.wifi_ssid, lua_system_param.wifi_ssid);
  }
  else return luaL_error( L, "arg: ssid needed" );
  
  //pwd  
  lua_getfield(L, 1, "pwd");
  if (!lua_isnil(L, -1)) {
    if ( lua_isstring(L, -1) ) {
      const char *pwd = luaL_checklstring( L, -1, &len );
      if (len >= 64) return luaL_error( L, "pwd: <64" );
      if (len > 0) strncpy(wNetConfig.wifi_key,pwd,len);
      else strcpy(wNetConfig.wifi_key,"");  
    } 
    else return luaL_error( L, "wrong arg type: pwd" );
  }
  else if (has_default == 1) {
    strcpy(wNetConfig.wifi_key, lua_system_param.wifi_key);
  }
  else return luaL_error( L, "arg: pwd needed" );

  //dhcp
  wNetConfig.dhcpMode = DHCP_Client;
  lua_getfield(L, 1, "dhcp");
  if (!lua_isnil(L, -1)) {
    if ( lua_isstring(L, -1) ) {
      const char *pwd = luaL_checklstring( L, -1, &len );
      if (strcmp(pwd, "disable") == 0) wNetConfig.dhcpMode = DHCP_Disable;
    }   
    else return luaL_error( L, "wrong arg type: dhcp" );
  }

  //ip
  lua_getfield(L, 1, "ip");
  if (!lua_isnil(L, -1)) {
    if ( lua_isstring(L, -1) ) {
      const char *ip = luaL_checklstring( L, -1, &len );
      if (len >= 16) return luaL_error( L, "ip: <16" );
      if (is_valid_ip(ip) == false) return luaL_error( L, "ip invalid" );
      strncpy(wNetConfig.local_ip_addr,ip,len);
    } 
    else return luaL_error( L, "wrong arg type:ip" );
  }
  else if (wNetConfig.dhcpMode == DHCP_Disable) return luaL_error( L, "arg: ip needed" );

  //netmask  
  lua_getfield(L, 1, "netmask");
  if (!lua_isnil(L, -1)) {
    if ( lua_isstring(L, -1) ) {
      const char *netmask = luaL_checklstring( L, -1, &len );
      if (len >= 16) return luaL_error( L, "netmask: <16" );
      if (is_valid_ip(netmask) == false) return luaL_error( L, "netmask invalid" );
      strncpy(wNetConfig.net_mask,netmask,len);
    } 
    else return luaL_error( L, "wrong arg type: netmask" );
  }
  else if (wNetConfig.dhcpMode == DHCP_Disable) return luaL_error( L, "arg: netmask needed" );
  
  //gateway 
  lua_getfield(L, 1, "gateway");
  if (!lua_isnil(L, -1)) {
    if ( lua_isstring(L, -1) ) {
      const char *gateway = luaL_checklstring( L, -1, &len );
      if (len >= 16) return luaL_error( L, "gateway: <16" );
      if (is_valid_ip(gateway) == false) return luaL_error( L, "gateway invalid" );
      strncpy(wNetConfig.gateway_ip_addr,gateway,len);
    } 
    else return luaL_error( L, "wrong arg type: gateway" );
  }
  else if(wNetConfig.dhcpMode == DHCP_Disable) return luaL_error( L, "arg: gateway needed" );
  
  //dnsSrv
  lua_getfield(L, 1, "dnsSrv");
  if (!lua_isnil(L, -1)) {
    if ( lua_isstring(L, -1) ) {
      const char *dnsSrv = luaL_checklstring( L, -1, &len );
      if (len >= 16) return luaL_error( L, "dnsSrv: <16" );
      if (is_valid_ip(dnsSrv) == false) return luaL_error( L, "dnsSrv invalid" );
      strncpy(wNetConfig.dnsServer_ip_addr,dnsSrv,len);
    } 
    else return luaL_error( L, "wrong arg type: dnsSrv" );
  }
  else if (wNetConfig.dhcpMode == DHCP_Disable) return luaL_error( L, "arg: dnsSrv needed" );
  
  //retry_interval
  lua_getfield(L, 1, "retry_interval");
  if (!lua_isnil(L, -1)) {
      retry_interval = (signed)luaL_checknumber( L, -1 );
      if (retry_interval < 0) return luaL_error( L, "retry_interval: >=0ms" );
      if (retry_interval == 0) retry_interval = 0x7FFFFFFF;
  }
  else retry_interval = 1000; 
  wNetConfig.wifi_retry_interval = retry_interval;
  
  gL = L;
  //notify, set CB function for wifi state change
  if (wifi_status_changed_STA != LUA_NOREF)
    luaL_unref(L, LUA_REGISTRYINDEX, wifi_status_changed_STA);
  wifi_status_changed_STA = LUA_NOREF;

  if (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, 2);  // copy argument (func) to the top of stack
      
    wifi_status_changed_STA = luaL_ref(L, LUA_REGISTRYINDEX);
  } 
  mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED, (void *)_micoNotify_WifiStatusHandler, NULL );
  
  //start  
  wNetConfig.wifi_mode = Station;
  micoWlanStart(&wNetConfig);
  wifi_sta_started = 1;

  if (con_wait == 0) {
    lua_pushboolean(L, false);
    return 1;
  }

  tmo = mico_get_time();
  micoWlanGetLinkStatus(&link);
  while (link.is_connected == 0) {
    if ((mico_get_time() - tmo) > con_wait) break;
    mico_thread_msleep(50);
    luaWdgReload();
    micoWlanGetLinkStatus(&link);
  }

  if (link.is_connected == 0) lua_pushboolean(L, false);
  else lua_pushboolean(L, true);

  return 1;
}

//---------------------------------------------------------------------------
void _WiFi_Scan_OK (lua_State *L, char ApNum, _ApList* ApList, uint8_t print)
{
  uint8_t ssidlen = 0;
  
  if ((ApNum == 0) && (print == 0)) lua_pushnil(L);
  else if ((ApList == NULL) && (print == 0)) lua_pushnil(L);
  else {
    ssidlen = 0;
    // push lua table with all APs
    char ssid[33];
    char temp[128];
    char buf_ptr[20];
    char security[12] = "open";
    char *inBuf = NULL;
    lua_newtable( L );
    
    for (int i=0; i<ApNum; i++) {
      uint8_t slen = strlen(ApList[i].ssid);
      if (slen > ssidlen) ssidlen = slen;
    }
    if (print == 1) printf("\r\n%*s  %17s, Pwr, Ch, Security\r\n", ssidlen, "SSID", "BSSID");
    
    for (int i=0; i<ApNum; i++) {
      //if(strlen(pApList->ApList[i].ssid)==0) continue;
      sprintf(ssid, ApList[i].ssid);
      //bssid
      inBuf = ApList[i].bssid;
      memset(buf_ptr, 0x00, 20);
      for (int j = 0; j < 6; j++) {
        if ( j == 6 - 1 ) { 
          sprintf(temp, "%02X", inBuf[j]);
          strcat(buf_ptr, temp);
        }
        else {
          sprintf(temp, "%02X:", inBuf[j]);
          strcat(buf_ptr, temp);
        }
      }

      switch(ApList[i].security)
      {
        case SECURITY_TYPE_NONE:          strcpy(security, "OPEN"); break;
        case SECURITY_TYPE_WEP:           strcpy(security, "WEP"); break;
        case SECURITY_TYPE_WPA_TKIP:      strcpy(security, "WPA TKIP"); break;
        case SECURITY_TYPE_WPA_AES:       strcpy(security, "WPA AES"); break;
        case SECURITY_TYPE_WPA2_TKIP:     strcpy(security, "WPA2 TKIP"); break;
        case SECURITY_TYPE_WPA2_AES:      strcpy(security, "WPA2 AES"); break;
        case SECURITY_TYPE_WPA2_MIXED:    strcpy(security, "WAP2 MIXED"); break;
        case SECURITY_TYPE_AUTO:          strcpy(security, "AUTO"); break;
        default:break;
      }       
      sprintf(temp, "%s, %3d, %2d, %s", buf_ptr, ApList[i].ApPower, ApList[i].channel, security);
      if (print == 0) {
        lua_pushstring(L, temp);     //value
        lua_setfield( L, -2, ssid ); //key
      }
      else {
        printf("%*s  %s\r\n", ssidlen, ssid, temp);
      }
    }
  }
  // push maximum ssid length
  if (print == 0) lua_pushinteger(L, ssidlen);
}

//----------------------------------------------------------------------------------------------
static void _micoNotify_WiFi_Scan_OK (ScanResult_adv *pApList, mico_Context_t * const inContext)
{
  (void)inContext;

  queue_msg_t msg;
  msg.L = gL;
  msg.source = onWIFI | 0x10;
  msg.para1 = pApList->ApNum;
  msg.para2 = wifi_scan_succeed;
  msg.para4 = NULL;

  if (pApList->ApNum > 0) {
    msg.para3 = (uint8_t*)malloc(sizeof(_ApList)*pApList->ApNum);
    if (msg.para3 != NULL) {
      memcpy((_ApList*)msg.para3, pApList->ApList, sizeof(_ApList)*pApList->ApNum);
    }
    else msg.para3 = NULL;
  }
  else msg.para3 = NULL;

  if (wifi_scan_succeed != LUA_NOREF) {
    mico_rtos_push_to_queue( &os_queue, &msg, 0);
  }
  else {
    _WiFi_Scan_OK(gL, msg.para1, (_ApList*)msg.para3, wifi_scanned_print);
    free(msg.para3);
    msg.para3 = NULL;
  }
  wifi_scanned = 1;
}

/*
** usage: **
-- callback function
function listap(t) if t then for k,v in pairs(t) do print(k.."\t"..v) end else print('no ap') end end

function listap(t)
  if t then
    for k,v in pairs(t) do
      print(k.."\t"..v)
    end
  else
    print('no ap')
  end
end

wifi.scan(listap)
*/
//===================================
static int lwifi_scan( lua_State* L )
{
  OSStatus err = 0;
  int tmo = mico_get_time();
  
  wifi_scanned_print = 0;
  
  if (lua_type(L, 1) == LUA_TFUNCTION || lua_type(L, 1) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, 1);  // copy argument (func) to the top of stack
    if (wifi_scan_succeed != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, wifi_scan_succeed);
    
    wifi_scan_succeed = luaL_ref(L, LUA_REGISTRYINDEX);
  } 
  else {
    if (wifi_scan_succeed != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, wifi_scan_succeed);
    wifi_scan_succeed = LUA_NOREF;
    if (lua_type(L, 1) == LUA_TNUMBER) {
      int prn = luaL_checkinteger( L, 1 );
      if (prn == 1) wifi_scanned_print = 1;
    }
  }
  err = mico_system_notify_register( mico_notify_WIFI_SCAN_ADV_COMPLETED, (void *)_micoNotify_WiFi_Scan_OK, NULL );
  require_noerr( err, exit );
  gL = L;
  wifi_scanned = 0;
  micoWlanStartScanAdv();

  tmo = mico_get_time();
  if (wifi_scan_succeed == LUA_NOREF) {
    while (wifi_scanned == 0) {
      if ((mico_get_time() - tmo) > 8000) break;
      mico_thread_msleep(100);
      luaWdgReload();
    }
    if ((wifi_scanned == 1) && (wifi_scanned_print == 0)) {
      return 2;
    }
  }
exit:
  return 0;
}

//---------------------------------------------------------------------
static void _getIPStatus(net_para_st* para, WiFi_Interface inInterface)
{
  memset(para, 0x00, sizeof(net_para_st));
  micoWlanGetIPStatus(para, inInterface);
}

//============================================
static int lwifi_station_getip( lua_State* L )
{
  net_para_st para;
  
  _getIPStatus(&para, Station);
  lua_pushstring(L,para.ip);
  return 1;
}

//===============================================
static int lwifi_station_getipadv( lua_State* L )
{
  net_para_st para;
  
  _getIPStatus(&para, Station);
  
  if (para.dhcp == DHCP_Client) lua_pushstring(L, "DHCP_Client");
  else if (para.dhcp == DHCP_Server) lua_pushstring(L, "DHCP_Server");
  else lua_pushstring(L, "DHCP_Disabled");
  lua_pushstring(L,para.ip);
  lua_pushstring(L,para.gate);
  lua_pushstring(L,para.mask);
  lua_pushstring(L,para.dns);
  lua_pushstring(L,para.mac);
  lua_pushstring(L,para.broadcastip);
  return 7;
}

//==============================================
static int lwifi_station_getlink (lua_State* L )
{
  LinkStatusTypeDef link;
  memset(&link,0x00,sizeof(link));
  micoWlanGetLinkStatus(&link);
  
  if (link.is_connected==0) {
    lua_pushstring(L, "disconnected");
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushnil(L);
  }
  else {
    lua_pushstring(L, "connected");
    lua_pushinteger(L, link.wifi_strength);
    lua_pushstring(L, (char*)link.ssid);

    char temp[20]={0};
    char temp2[4];
    temp[0]=0x00;
    for (int j = 0; j < 6; j++) {
      if ( j == 6 - 1 ) { 
        sprintf(temp2,"%02X",link.bssid[j]);
        strcat(temp,temp2);
      }
      else {
        sprintf(temp2,"%02X:",link.bssid[j]);
        strcat(temp,temp2);
      }
    }    
    lua_pushstring(L,temp);
  }
  return 4;
}

//=======================================
static int lwifi_ap_getip( lua_State* L )
{
  net_para_st para;
  
  _getIPStatus(&para, Soft_AP);
  lua_pushstring(L, para.ip);
  return 1;
}

//==========================================
static int lwifi_ap_getipadv( lua_State* L )
{
  net_para_st para;

  _getIPStatus(&para, Soft_AP);
  
  lua_pushstring(L, "DHCP_Server");
  lua_pushstring(L, para.ip);
  lua_pushstring(L, para.gate);
  lua_pushstring(L, para.mask);
  lua_pushstring(L, para.dns);
  lua_pushstring(L, para.mac);
  lua_pushstring(L, para.broadcastip);
  return 7;
}

//===========================================
static int lwifi_station_stop( lua_State* L )
{
  _stopWifiSta();
  return 0;
}

//======================================
static int lwifi_ap_stop( lua_State* L )
{
  micoWlanSuspendSoftAP();
  return 0;
}

//===================================
static int lwifi_stop( lua_State* L )
{
  _stopWifiSta();

  micoWlanSuspend();
  return 0;
}

//========================================
static int lwifi_powersave( lua_State* L )
{
   int arg = lua_toboolean( L, 1 );
   
   if (lua_isboolean(L,1)) {
     if (arg == true) micoWlanEnablePowerSave();
     else micoWlanDisablePowerSave();
   }
   else luaL_error( L, "boolean arg needed" );
   
  return 0;
}

/*
//======================================
static int lwifi_poweron( lua_State* L )
{
  host_platform_bus_init();
  micoWlanPowerInit();
  micoWlanPowerOn();
  return 0;
}
*/

//=======================================
static int lwifi_poweroff( lua_State* L )
{
  micoWlanSuspend();
  wifi_sta_started = 0;
  mico_thread_msleep(100);
  micoWlanPowerOff();
   
  return 0;
}

//==================================================
static int lwifi_station_hasdefaults( lua_State* L )
{
  lua_system_param_t lua_system_param;
  
  if (getLua_systemParams(&lua_system_param) == 1) {
    if ((strlen(lua_system_param.wifi_ssid) > 0) && (strlen(lua_system_param.wifi_key) > 0)) {
      lua_pushboolean(L, true);
    }
    else lua_pushboolean(L, false);
  }
  else lua_pushboolean(L, false);
  return 1;
}

//================================================
static int lwifi_station_ntpstatus (lua_State* L )
{
  if (isNtpTimeSet() == 1) lua_pushinteger(L, 1);
  else {
    if (ntpTthreadStarted() == 0) lua_pushinteger(L, -1);
    else lua_pushinteger(L, 0);
  }
  return 1;
}

//=======================================
static int lwifi_ntp_time (lua_State* L )
{
  int tz = 0;
  int npar = 0;
  size_t len;
  const char *ntpserv = NULL;

  npar = lua_gettop(L);
  bool lg = false;
  
  if (npar > 0) {
    tz = luaL_checkinteger( L, 1 );
    if ((tz > 14) || (tz < -12)) { tz = 0; }
  }
  if (npar > 1) {
    ntpserv = luaL_checklstring( L, 2, &len );
    if ((len == 0) || (len >= 32)) ntpserv = NULL;
  }
  if (npar > 2) {
    uint8_t ilg = luaL_checkinteger( L, 3 );
    if (ilg == 1) lg = true;
  }
  
  sntp_client_start(tz, ntpserv, lg);
  return 0;
}

#define MIN_OPT_LEVEL       2
#include "lrodefs.h"
static const LUA_REG_TYPE wifi_station_map[] =
{
  { LSTRKEY( "getip" ), LFUNCVAL ( lwifi_station_getip ) },
  { LSTRKEY( "getipadv" ), LFUNCVAL ( lwifi_station_getipadv ) },
  { LSTRKEY( "getlink" ), LFUNCVAL ( lwifi_station_getlink ) },
  { LSTRKEY( "stop" ), LFUNCVAL ( lwifi_station_stop ) },
  { LSTRKEY( "ntptime" ), LFUNCVAL ( lwifi_ntp_time ) },
  { LSTRKEY( "ntpstatus" ), LFUNCVAL ( lwifi_station_ntpstatus ) },
  { LSTRKEY( "hasdefoults" ), LFUNCVAL ( lwifi_station_hasdefaults ) },
#if LUA_OPTIMIZE_MEMORY > 0
#endif        
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE wifi_ap_map[] =
{
  { LSTRKEY( "getip" ), LFUNCVAL ( lwifi_ap_getip ) },
  { LSTRKEY( "getipadv" ), LFUNCVAL ( lwifi_ap_getipadv ) },
  { LSTRKEY( "stop" ), LFUNCVAL ( lwifi_ap_stop ) },
#if LUA_OPTIMIZE_MEMORY > 0
#endif        
  { LNILKEY, LNILVAL }
};
const LUA_REG_TYPE wifi_map[] =
{
  { LSTRKEY( "startap" ), LFUNCVAL( lwifi_startap )},
  { LSTRKEY( "startsta" ), LFUNCVAL( lwifi_startsta )},
  { LSTRKEY( "scan" ), LFUNCVAL( lwifi_scan ) },
  { LSTRKEY( "stop" ), LFUNCVAL( lwifi_stop ) },
  { LSTRKEY( "powersave" ), LFUNCVAL( lwifi_powersave ) },
  //{ LSTRKEY( "poweron" ), LFUNCVAL( lwifi_poweron ) },
  { LSTRKEY( "poweroff" ), LFUNCVAL( lwifi_poweroff ) },
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY( "sta" ), LROVAL( wifi_station_map ) },
  { LSTRKEY( "ap" ), LROVAL( wifi_ap_map ) },
#endif        
  {LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_wifi(lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else
  luaL_register( L, EXLIB_WIFI, wifi_map );
  // Setup the new tables (station and ap) inside wifi
  lua_newtable( L );
  luaL_register( L, NULL, wifi_station_map );
  lua_setfield( L, -2, "sta" );

  lua_newtable( L );
  luaL_register( L, NULL, wifi_ap_map );
  lua_setfield( L, -2, "ap" );
  return 1;
#endif  
}
