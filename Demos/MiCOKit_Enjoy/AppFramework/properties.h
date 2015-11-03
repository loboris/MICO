/**
  ******************************************************************************
  * @file    properties.h
  * @author  Eshen Wang
  * @version V1.0.0
  * @date    18-Mar-2015
  * @brief   device properties operations.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, MXCHIP Inc. SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2014 MXCHIP Inc.</center></h2>
  ******************************************************************************
  */ 

#ifndef __MICO_DEVICE_PROPERTIES_H_
#define __MICO_DEVICE_PROPERTIES_H_

#include "mico.h"
#include "MiCOAppDefine.h"
#include "json_c/json.h"


/*******************************************************************************
 *                               DEFINES
 ******************************************************************************/
// max number of services && properties per serivce
#define MAX_PROPERTY_NUMBER_PER_SERVICE        (5)
#define MAX_SERVICE_NUMBER                     (10)

// property value access attribute
#define MICO_PROP_PERMS_RO                     (0x01)  // can read
#define MICO_PROP_PERMS_WO                     (0x02)  // can write
#define MICO_PROP_PERMS_EV                     (0x04)  // can notification

#define MICO_PROP_PERMS_READABLE(perms_u8)     ( MICO_PROP_PERMS_RO == (uint8_t)(perms_u8 & MICO_PROP_PERMS_RO) )
#define MICO_PROP_PERMS_WRITABLE(perms_u8)     ( MICO_PROP_PERMS_WO == (uint8_t)(perms_u8 & MICO_PROP_PERMS_WO) )
#define MICO_PROP_PERMS_NOTIFIABLE(perms_u8)   ( MICO_PROP_PERMS_EV == (uint8_t)(perms_u8 & MICO_PROP_PERMS_EV) )

// status code
#define MICO_PROP_KEY_RESP_STATUS              "status"
#define MICO_PROP_KEY_RESP_SERVICES            "services"
#define MICO_PROP_KEY_RESP_READ                "read"
#define MICO_PROP_KEY_RESP_WRITE               "write"
#define MICO_PROP_KEY_RESP_ERROR               "err"
#define MICO_PROP_KEY_RESP_ERROR_PROPERTIES    "properties"

#define MICO_PROP_CODE_READ_SUCCESS            (0)
#define MICO_PROP_CODE_WRITE_SUCCESS           (0)
#define MICO_PROP_CODE_READ_FAILED             (-70101)
#define MICO_PROP_CODE_WRITE_FAILED            (-70102)
#define MICO_PROP_CODE_READ_PARTIAL_FAILED     (-70103)
#define MICO_PROP_CODE_WRITE_PARTIAL_FAILED    (-70104)
#define MICO_PROP_CODE_NOT_READABLE            (-70401)
#define MICO_PROP_CODE_NOT_WRITABLE            (-70402)
#define MICO_PROP_CODE_NOT_FOUND               (-70403)
#define MICO_PROP_CODE_NO_GET_FUNC             (-70404)
#define MICO_PROP_CODE_NO_SET_FUNC             (-70405)
#define MICO_PROP_CODE_NO_NOTIFY_FUNC          (-70406)
#define MICO_PROP_CODE_DATA_FORMAT_ERR         (-70501)
#define MICO_PROP_CODE_NOT_SUPPORTED           (-70502)
   

/*******************************************************************************
 *                               STRUCTURES
 ******************************************************************************/
// property value format
enum mico_prop_format_t{
  MICO_PROP_TYPE_INT = 0,
  MICO_PROP_TYPE_FLOAT = 1,
  MICO_PROP_TYPE_BOOL = 2,
  MICO_PROP_TYPE_STRING = 3,
  MICO_PROP_TYPE_MAX
};

// property type
struct mico_prop_t{
  const char *type;
  //uint32_t iid;                             // property iid (auto allocate)
  
  void* value;                                // point to current property data
  uint32_t *value_len;                        // point to current property data length
  enum mico_prop_format_t format;             // property value format: int, float, bool, string
  uint8_t perms;                              // property value access attribute: RO | WO | RW | EV
  
  int (*set)(struct mico_prop_t *prop, void *arg, void *val, uint32_t val_len);  // hardware operation function
  int (*get)(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len);  // get hardware status function
  int (*notify_check)(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len);  // check value to notify
  void *arg;                                  // arg for set or get or notify function
  
  bool *event;                                // point to notification enable/disable flag
  
  bool hasMeta;                               // property meta attribute flag, if true max/min/step must be set
  union {
    int   intValue;
    float floatValue;
  }maxValue;                                  // max value for int or float property
  
  union {
    int   intValue;
    float floatValue;
  }minValue;                                  // min value for int or float property
  
  union {
    int   intValue;
    float floatValue;
  }minStep;                                  // min step for int or float property
  
  uint32_t maxStringLen;                     // max length for string property
  
  char* unit;                                // property value unit name
};

// service type
 struct mico_service_t {
  const char *type;
  //int iid£»                                // iid (auto allocate)
  struct mico_prop_t  properties[MAX_PROPERTY_NUMBER_PER_SERVICE];
};


/*******************************************************************************
 *                               FUNCTIONS
 ******************************************************************************/
// create dev_info json data
json_object* mico_get_device_info(struct mico_service_t service_table[]);

// read multiple properties
json_object* mico_read_properties(struct mico_service_t *service_table, 
                                  json_object *prop_read_list_obj);
// write multiple properties
json_object* mico_write_properties(struct mico_service_t *service_table,
                                   json_object *prop_write_list_obj);

// properties update check
OSStatus mico_properties_notify_check(app_context_t * const inContext, 
                                      struct mico_service_t *service_table,
                                      json_object* notify_obj);

#endif // __MICO_DEVICE_PROPERTIES_H_
