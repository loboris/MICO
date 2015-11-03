
/*
 *    @author/email: songyang.sy@alibaba-inc.com
 */

/*
 *    IMPORTANT: All params passed to ALINK-SDK can be (and should be) released right after the callee returns.
 */

#ifndef _ALINK_EXPORT_H_
#define _ALINK_EXPORT_H_


#define ALINK_SDK_VERSION "1.1.141215"

/******************************************/
#ifndef _ALINK_TIME_T_
#define _ALINK_TIME_T_

#ifdef _PLATFORM_LINUX_
#include <time.h>
typedef time_t alink_time_t;
#endif

#ifdef _PLATFORM_MICO_
typedef unsigned int alink_time_t;
#endif

#endif

/******************************************/

typedef int (*alink_func) (void *, void *);

#define MAX_SYS_CALLBACK 1
enum ALINK_SYSTEM_CALLBACK {
    ALINK_FUNC_SERVER_STATUS = 0
};

/*
  *  ALINK_FUNC_SERVER_STATUS. Example: 
  *  int func(void* mac, void* status) {
  *      char* mac_str = (char)* mac;
  *      int i_status = *(int*)status;
  *      .... // Do your own logic.
  *  } 
  */

/******************************************/
#define MAX_NAME_LEN (80)

#define ALINK_TRUE 1
#define ALINK_FALSE 0

#define ALINK_OK 0
#define ALINK_ERR -1
#define ALINK_SOCKET_ERR -2
#define ALINK_ERR_NO_MEMORY -3

#define OUT

enum ALINK_CALLBACK {
    ACB_GET_DEVICE_STATUS = 0,
    ACB_SET_DEVICE_STATUS = 1,
    ACB_SET_DEVICE_STATUS_ARRAY = 2,
    ACB_REQUEST_REMOTE_SERVICE = 3,
    ACB_GET_DEVICE_STATUS_BY_RAWDATA = 4,
    ACB_SET_DEVICE_STATUS_BY_RAWDATA = 5,
    MAX_CALLBACK_NUM
};

enum ALINK_SYSTEM_FUNC {
    ALINK_FUNC_AVAILABLE_MEMORY = 0,
    ALINK_FUNC_PREPARE_NETWORK,
    ALINK_FUNC_WAITING_NETWORK,
    ALINK_FUNC_SET_UTC_TIME
};

enum ALINK_STATUS {
    ALINK_STATUS_INITED = 1,
    ALINK_STATUS_REGISTERED,
    ALINK_STATUS_LOGGED,
    ALINK_STATUS_LOGOUT
};

enum ALINK_LOGLEVEL {
    ALINK_LL_NONE = 0,
    ALINK_LL_ERROR,
    ALINK_LL_WARN,
    ALINK_LL_INFO,
    ALINK_LL_DEBUG
};

/******************************************/
#define STR_UUID_LEN	(32 + 1)
typedef struct alink_down_cmd {
    int id;
    int time;
    char *account;
    char *param;
    int method;
    char uuid[STR_UUID_LEN];
} alink_down_cmd, *alink_down_cmd_ptr;

typedef struct alink_up_cmd {

    /*If the command is not a response of any down-cmd, resp_id MUST be set to -1 */
    int resp_id;
    int emergency;
    const char *target;
    const char *param;

} alink_up_cmd, *alink_up_cmd_ptr;

/******************************************/
typedef int (*alink_callback) (alink_down_cmd_ptr);

/******************************************/
#define STR_NAME_LEN	(32 + 1)
#define STR_SN_LEN	(64 + 1)
#define STR_CID_LEN	(64 + 1)
#define STR_MODEL_LEN	(80 + 1)
#define STR_MAC_LEN	(17 + 1)/* include : */
#define STR_KEY_LEN	(20 + 1)
#define STR_SEC_LEN	(40 + 1)
struct device_info {
    /* optional */
    char sn[STR_SN_LEN];
    char name[STR_NAME_LEN];
    char brand[STR_NAME_LEN];
    char type[STR_NAME_LEN];
    char category[STR_NAME_LEN];
    char manufacturer[STR_NAME_LEN];
    char version[STR_NAME_LEN];

    /* mandatory */
    /* must follow the format xx:xx:xx:xx:xx:xx */
    char mac[STR_MAC_LEN];

    /* manufacturer_category_type_name */
    char model[STR_MODEL_LEN];

    /* mandatory for gateway, optional for child dev */
    char cid[STR_CID_LEN];
    char key[STR_KEY_LEN];
    char secret[STR_SEC_LEN];

    alink_callback dev_callback[MAX_CALLBACK_NUM];
    alink_func sys_callback[MAX_SYS_CALLBACK];
};

/******************************************/


struct alink_config {

    int max_thread_num;         // Max thread number that thread pool should holds, NOT used for now
    int def_thread_num;         // Default thread number that thread pool holds when it is initial
    int auto_create_thread;     // Whether sdk should automatically create new thread if thread pool if full, NOT implemented

    const char *cert;           // Server certificate str, ex. ""-----BEGIN CERTIFICATE-----\nXXXXXX....XXXXX\n-----END CERTIFICATE-----\n""
    const char *server;         // Server address, ex. "alink.tcp.aliyun.com", "10.28.1.55"
    int port;                   // Server port, ex 443, 1818, 18080
    char factory_reset;         // Set this flag when need to clear dev's account binding info
};

/******************************************/

#ifdef __cplusplus
extern "C" {
#endif

	

/***
 * @desc:    Configure sdk features, such as default thread number of thread pool
 * @para:    config: configure that need to be set
 * @retc:    None/Always Success.
 */
    void alink_set_config(struct alink_config *config);

/***
 * @desc:    Start ALINK-SDK. Note alink_config should be called first if there's any special settings.
 * @para:    dev: Device info. If ALINK-SDK is running on gateway, the 'dev' MUST implies the gateway device.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_start(struct device_info *dev);

/***
 * @desc:    End ALINK-SDK. Note this function returns when all thread has already been shutdown.
 * @para:    None
 * @retc:    Always return ALINK_OK in normal mode. Returns ALINK_ERR if DEBUG_MEM is set and MEMORY_CHECK is failed.
 */
    int alink_end(void);

/***
 * @desc:    Attach a new sub-device to ALINK SDK and server. This API is supposed to return immediatly.
 * @para:    dev: Device info of sub-device.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_attach_sub_device(struct device_info *dev);

/***
 * @desc:    Detach an existed sub-device to ALINK SDK and server. This API is supposed to return immediatly.
 * @para:    dev: Device info of sub-device.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_detach_sub_device(struct device_info *dev);

/***
 * @desc:    Remove all device information (including sub-device) from both server and local.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_reset(void);


/***
 * @desc:    Set special callbacks to ALINK-SDK. 
 * @para:    method:    
 *               func:    
 * @retc:    None/Always Success.
 */
    int alink_set_callback(int method, alink_func func);

/***
 * @desc:    Get alink time. The SDK sync with server every 30 minutes, 
 *           or anytime if SDK is notified by the server if the timestamp is out order.
 * @para:    None
 * @retc:    For linux platform, alink_time_t is typedef of time_t, which implies the number of seconds since Epoth.
 */
    alink_time_t alink_get_time(void);

/***
 * @desc:    Post device data. Refer to ALINK protocol.
 * @para:    cmd: Command for the API. Memory referred by cmd is ready to release right after the function returns.
 * @retc:    Refer to ALINK protocol API returns
 */
    int alink_post_device_data(alink_up_cmd_ptr cmd);
/***
 * @desc:    Post device data array. Refer to ALINK protocol.
 * @para:    cmd: Command for the API. Memory referred by cmd is ready to release right after the function returns.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_post_device_data_array(alink_up_cmd_ptr cmd);

/***
 * @desc:    Register remote service. Refer to ALINK protocol.
 * @para:    cmd: Command for the API. Memory referred by cmd is ready to release right after the function returns.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_register_remote_service(alink_up_cmd_ptr cmd);
/***
 * @desc:    Unregister remote service. Refer to ALINK protocol.
 * @para:    cmd: Command for the API. Memory referred by cmd is ready to release right after the function returns.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_unregister_remote_service(alink_up_cmd_ptr cmd);
/***
 * @desc:    Post remote service response. Refer to ALINK protocol.
 * @para:    cmd: Command for the API. Memory referred by cmd is ready to release right after the function returns.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_post_remote_service_rsp(alink_up_cmd_ptr cmd);

/***
 * @desc:    Normally, the ALINK-SDK is responsible to decide when to send the message to server 
             if alink_up_cmd_ptr.emergency is set to ALINK_FALSE. However, one could demand ALINK-SDK to send
             all reserved messages (if any) immediatly by calling this method.
 * @para:    None
 * @retc:    ALINK_OK/ALINK_ERR (Only if ALINK is not started)
 */
    int alink_flush(void);

/***
 * @desc:    Get uuid by device mac or sn. Set mac or sn to NULL if one wish to get the main device's uuid (also by main device's mac or sn).
 * @para:    id: Mac address. ("xx:xx:xx:xx:xx:xx") or SN (max len refs STR_SN_LEN)
 * @retc:    NULL if mac or sn does NOT match any device. Blank string ("") if the uuid is not set yet.
 */
    const char *alink_get_uuid(const char *id);

/***
 * @desc:    Wait until alink is connected to server and successfully logged in, or simple timeout.
 *               The function can be interrupted in other thread by calling alink_wakeup_connect_waiting.
 * @para:    id: Device mac or sn of which the function is waiting. Set to NULL implies the main device.
                  timeout: seconds before the function timeout, set to -1 if wait forever.
 * @retc:    ALINK_OK if alink is connected to server and successfully logged in. ALINK_ERR if timeout or being wakeup.
 */
    int alink_wait_connect(const char *id, int timeout /*second */ );

/***
 * @desc:    Wake up the device waiting function.
 * @para:    id: Device mac or sn of which the function was waiting. Set to NULL implies the main device.
 * @retc:    None. Return immediately.
 */
    void alink_wakeup_connect_waiting(const char *id);

/***
 * @desc:    Set alink loglevel, default is ALINK_LL_ERROR.
 * @retc:    None.
 */
    void alink_set_loglevel(int loglevel);

#ifdef __cplusplus
}
#endif
/******************************************/
#endif
