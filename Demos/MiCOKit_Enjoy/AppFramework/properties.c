/**
******************************************************************************
* @file    properties.c 
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

#include "mico.h"
#include "MiCOAppDefine.h"
#include "json_c/json.h"
#include "StringUtils.h"
#include "properties.h"

#define properties_log(M, ...) custom_log("DEV_PROPERTIES", M, ##__VA_ARGS__)
#define properties_log_trace() custom_log_trace("DEV_PROPERTIES")


/*******************************************************************************
*                          DEFINES && STRUCTURES
******************************************************************************/

// notify list node
typedef struct _mico_prop_notify_node_t{
  int iid;
  int s_idx;
  int p_idx;
  struct _mico_prop_notify_node_t *next;
} mico_prop_notify_node_t;

// notify list
volatile bool notify_list_inited = false;
mico_prop_notify_node_t *g_notify_list = NULL;


/*******************************************************************************
*                                FUNCTIONS
*******************************************************************************/

int notify_check_default(struct mico_prop_t *prop, void *arg, void *val, uint32_t *val_len)
{
  int ret = 0;
  properties_log("prop: %s, do default notify_check.", prop->type);
  return ret;
}

OSStatus FindPropertyByIID(struct mico_service_t *service_table, int iid, 
                           int *service_index, int *property_index)
{
  int s_idx = 0, p_idx = 0, tmpIID = 1;
  *service_index = 0;
  *property_index = 0;
  
  for(s_idx = 0; NULL != service_table[s_idx].type; s_idx++){
    if( tmpIID > iid){
      return kNotFoundErr;
    }
    
    if(tmpIID == iid){
      *service_index = s_idx;
      return kRequestErr;  // iid is a service
    }
    tmpIID ++;
    
    for(p_idx = 0; NULL != service_table[s_idx].properties[p_idx].type; p_idx++){
      if( tmpIID > iid){
        return kNotFoundErr;
      }
      
      if(tmpIID == iid){
        *service_index = s_idx;
        *property_index = p_idx;
        return kNoErr;
      }
      tmpIID ++;
    }
  }
  
  return kNotFoundErr;
}

OSStatus getIndexByIID(struct mico_service_t *service_table, int iid, 
                       int *service_index, int *property_index)
{
  int s_idx = 0, p_idx = 0, tmpIID = 1;
  *service_index = 0;
  *property_index = 0;
  
  for(s_idx = 0; NULL != service_table[s_idx].type; s_idx++){
    if( tmpIID > iid){
      *service_index = -1;
      *property_index = -1;
      return kNotFoundErr;
    }
    
    if(tmpIID == iid){
      *service_index = s_idx;
      *property_index = -1;
      return kNoErr;  // iid is a service
    }
    tmpIID ++;
    
    for(p_idx = 0; NULL != service_table[s_idx].properties[p_idx].type; p_idx++){
      if( tmpIID > iid){
        *service_index = -1;
        *property_index = -1;
        return kNotFoundErr;
      }
      
      if(tmpIID == iid){
        *service_index = s_idx;
        *property_index = p_idx;
        return kNoErr;   // iid is a property
      }
      tmpIID ++;
    }
  }
  
  return kNotFoundErr;
}

OSStatus PropertyNotifyListAdd(int iid, int service_index, int property_index,
                               mico_prop_notify_node_t **p_notify_list )
{
  OSStatus err = kNoErr;
  mico_prop_notify_node_t *plist = *p_notify_list;
  //mico_prop_notify_node_t *plist_next = *p_notify_list;
  mico_prop_notify_node_t *notify = NULL;
  
  // create node
  notify = (mico_prop_notify_node_t *)malloc(sizeof(mico_prop_notify_node_t)); 
  require_action(notify, exit, err = kNoMemoryErr);
  
  notify->iid = iid;
  notify->s_idx = service_index;
  notify->p_idx = property_index;
  notify->next = NULL;
  
  if(NULL == *p_notify_list){  // add to first node
    *p_notify_list = notify;
    properties_log("notify add: iid=%d, s_id=%d, p_id=%d.", 
                   (*p_notify_list)->iid, (*p_notify_list)->s_idx, (*p_notify_list)->p_idx);
    return kNoErr;  
  }
  else{  // add to node to the end
    do{
      if(plist->iid == iid){
        plist->s_idx = service_index;
        plist->p_idx = property_index;
        free(notify);
        return kNoErr;   // Nodify already exist, update index
      }
      else{
        if(NULL == plist->next){  // add node to the end.
          plist->next = notify;
          properties_log("notify add: iid=%d, s_id=%d, p_id=%d.", 
                         plist->next->iid, plist->next->s_idx, plist->next->p_idx);
          return kNoErr;
        }
        else {
          plist = plist->next;
        }
      }
    }while(1);  // get to end
    
//    // add node
//    plist->next = notify;
//    properties_log("notify add: iid=%d, s_id=%d, p_id=%d.", 
//                   plist->next->iid, plist->next->s_idx, plist->next->p_idx);
  }
  
exit:
  return err;
}


OSStatus getNextNotify( mico_prop_notify_node_t *p_notify_list, 
                       int *iid, int *s_idx, int *p_idx, 
                       mico_prop_notify_node_t **outNextNotifyList )
{
  if(NULL == outNextNotifyList){
    return kParamErr;
  }
  
  if(NULL == p_notify_list){
    return kNotFoundErr;
  }
  
  *iid = p_notify_list->iid;
  *s_idx = p_notify_list->s_idx;
  *p_idx = p_notify_list->p_idx;
  *outNextNotifyList = p_notify_list->next;
  
  return kNoErr;
}

OSStatus PropertyNotifyListClean(mico_prop_notify_node_t **p_notify_list )
{
  mico_prop_notify_node_t* temp = *p_notify_list;
  mico_prop_notify_node_t* temp_next;
  
  if(NULL == *p_notify_list){
    return kNoErr;
  }
  
  do{
    temp_next = temp->next;
    free(temp);
    temp = temp_next;
  }while(NULL != temp);    
  
  *p_notify_list = NULL;
  
  properties_log("notify list clean ok.");
  
  return kNoErr;
}

/* create notify list from service_table
* input: service_table
* return: notify list
*/
OSStatus create_notify_list(struct mico_service_t *service_table,
                            mico_prop_notify_node_t **outNotifyList)
{
  OSStatus err = kNoErr;
  int iid = 1, s_idx = 0, p_idx = 0;
  
  require_action(outNotifyList, exit, err = kParamErr);
  require_action(service_table, exit, err = kParamErr);
  
  properties_log("create notify list...");
  for(s_idx = 0; NULL != service_table[s_idx].type; s_idx++){
    iid++;  // is a service, jump to next service or property
    for(p_idx = 0; NULL != service_table[s_idx].properties[p_idx].type; p_idx++){     
      if( MICO_PROP_PERMS_NOTIFIABLE( service_table[s_idx].properties[p_idx].perms ) &&
         (NULL != service_table[s_idx].properties[p_idx].notify_check) ){  //must set notify_check func
           // add to notify list
           err = PropertyNotifyListAdd(iid, s_idx, p_idx, outNotifyList);
           require_noerr(err, exit);
         }
      iid++;  // next property
    }
  }
  
exit:
  if(kNoErr != err){
    properties_log("ERROR: create notify list failed, err = %d.", err);
    PropertyNotifyListClean(outNotifyList);
  }
  properties_log("notify list create ok.");
  return err;
}

/* property notify check
* description: check update of all properties in notify list, 
*              if notify list is not created, create is first from service_table.
* input: mico context;
*        service table
* output: json object contains properties updated, like {k:v, k:v}
*         if no update or error, return NULL
* return: kNoErr if succeed.
*/
OSStatus mico_properties_notify_check(app_context_t * const inContext, struct mico_service_t *service_table,
                                      json_object* notify_obj)
{
  OSStatus err = kNoErr;
  int s_idx = 0; 
  int p_idx = 0;
  int iid = 0;
  char iid_str[16] = {0};
  int ret = 0;
  mico_prop_notify_node_t *_notify_list = NULL;
  
  require_action(inContext, exit, err = kParamErr);
  require_action(service_table, exit, err = kParamErr);
  require_action(notify_obj, exit, err = kParamErr);
  
  //properties_log("properties update check...");
  
  // if notify list not created, create it the first time
  if(!notify_list_inited){
    if(NULL != g_notify_list){
      PropertyNotifyListClean(&g_notify_list);  // clean g_notify_list  
    }
    err = create_notify_list(service_table, &g_notify_list);
    require_noerr(err, exit);
    notify_list_inited = true;
  }
  
  _notify_list = g_notify_list;
  // search notify list
  while(getNextNotify(_notify_list, &iid, &s_idx, &p_idx, &_notify_list) == kNoErr){
    //properties_log("notify prop check: iid=%d, s_idx=%d, p_idx=%d.", iid, s_idx, p_idx);
    // get key string
    memset((void*)iid_str, '\0', sizeof(iid_str));
    Int2Str((uint8_t*)iid_str, iid);
    
    // add updated property to json object
    if((NULL != service_table[s_idx].properties[p_idx].event) &&
       (*(service_table[s_idx].properties[p_idx].event)) ){  // prop event enable
         // do prop update check && update prop value && len
         ret = service_table[s_idx].properties[p_idx].notify_check(&(service_table[s_idx].properties[p_idx]), 
                                                                   service_table[s_idx].properties[p_idx].arg, 
                                                                   service_table[s_idx].properties[p_idx].value, 
                                                                   service_table[s_idx].properties[p_idx].value_len);
         if(1 == ret){  // prop updated, add new value to notify json object
           switch(service_table[s_idx].properties[p_idx].format){
           case MICO_PROP_TYPE_INT:{
             json_object_object_add(notify_obj, iid_str, 
                                    json_object_new_int(*((int*)(service_table[s_idx].properties[p_idx].value))));
             break;
           }
           case MICO_PROP_TYPE_FLOAT:{
             json_object_object_add(notify_obj, iid_str, 
                                    json_object_new_double(*((float*)service_table[s_idx].properties[p_idx].value)));
             break;
           }
           case MICO_PROP_TYPE_STRING:{
             json_object_object_add(notify_obj, iid_str, 
                                    json_object_new_string((char*)service_table[s_idx].properties[p_idx].value));
             break;
           }
           case MICO_PROP_TYPE_BOOL:{
             json_object_object_add(notify_obj, iid_str, 
                                    json_object_new_boolean(*((bool*)service_table[s_idx].properties[p_idx].value)));
             break;
           }
           default:
             properties_log("ERROR: prop format unsupport!");
             break;
           }
         }
       }
  }
  
exit:
  return err;
}


// add json object if read success, if faild no return json object add, return err.
OSStatus _property_read_create_response_by_index(struct mico_service_t *service_table, 
                                            const char *key, int iid, 
                                            int service_index, int property_index,
                                            json_object *out_read_obj, json_object *out_err_prop_obj)
{
  OSStatus err = kUnknownErr;
  
  require_action( service_table, exit, err = kParamErr);
  require_action( out_read_obj, exit, err = kParamErr);
  require_action( out_err_prop_obj, exit, err = kParamErr);
  
  if( MICO_PROP_PERMS_READABLE (service_table[service_index].properties[property_index].perms) ){
    switch(service_table[service_index].properties[property_index].format){
    case MICO_PROP_TYPE_INT:{
      //properties_log("prop got: %s, iid=%d, value=%d", 
      //               service_table[service_index].properties[property_index].type, iid, 
      //               *((int*)service_table[service_index].properties[property_index].value));
      // prop->get (read hardware status)
      if(NULL != service_table[service_index].properties[property_index].get){
        service_table[service_index].properties[property_index].get(&service_table[service_index].properties[property_index], 
                                                                    service_table[service_index].properties[property_index].arg,
                                                                    service_table[service_index].properties[property_index].value, 
                                                                    service_table[service_index].properties[property_index].value_len);
      }
      // return prop value
      json_object_object_add(out_read_obj, key, json_object_new_int(*((int*)service_table[service_index].properties[property_index].value)));
      err = kNoErr;
      break;
    }
    case MICO_PROP_TYPE_FLOAT:{
      //properties_log("prop got: %s, iid=%d, value=%f", 
      //               service_table[service_index].properties[property_index].type, iid, 
      //               *((float*)service_table[service_index].properties[property_index].value));
      // prop->get (read hardware status)
      if(NULL != service_table[service_index].properties[property_index].get){
        service_table[service_index].properties[property_index].get(&service_table[service_index].properties[property_index], 
                                                                    service_table[service_index].properties[property_index].arg,
                                                                    service_table[service_index].properties[property_index].value, 
                                                                    service_table[service_index].properties[property_index].value_len);
      }
      // return value
      json_object_object_add(out_read_obj, key, json_object_new_double(*((float*)service_table[service_index].properties[property_index].value)));
      err = kNoErr;
      break;
    }
    case MICO_PROP_TYPE_STRING:{
      //properties_log("prop got: %s, iid=%d, value=%s", 
      //               service_table[service_index].properties[property_index].type, iid, 
      //               (char*)service_table[service_index].properties[property_index].value);
      // prop->get (read hardware status)
      if(NULL != service_table[service_index].properties[property_index].get){
        service_table[service_index].properties[property_index].get(&service_table[service_index].properties[property_index], 
                                                                    service_table[service_index].properties[property_index].arg,
                                                                    service_table[service_index].properties[property_index].value, 
                                                                    service_table[service_index].properties[property_index].value_len);
      }
      // return value
      json_object_object_add(out_read_obj, key, json_object_new_string((char*)service_table[service_index].properties[property_index].value));
      err = kNoErr;
      break;
    }
    case MICO_PROP_TYPE_BOOL:{
      //properties_log("prop got: %s, iid=%d, value=%d", 
      //               service_table[service_index].properties[property_index].type, iid, 
      //               *((bool*)service_table[service_index].properties[property_index].value));
      // prop->get (read hardware status)
      if(NULL != service_table[service_index].properties[property_index].get){
        service_table[service_index].properties[property_index].get(&service_table[service_index].properties[property_index], 
                                                                    service_table[service_index].properties[property_index].arg,
                                                                    service_table[service_index].properties[property_index].value, 
                                                                    service_table[service_index].properties[property_index].value_len);
      }
      // return value
      json_object_object_add(out_read_obj, key, json_object_new_boolean(*((bool*)service_table[service_index].properties[property_index].value)));
      err = kNoErr;
      break;
    }
    default:
      properties_log("ERROR: property format unsupported!");
      json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_DATA_FORMAT_ERR));
      err = kUnsupportedDataErr;
      break;
    }
  }
  else{
    properties_log("ERROR: property %d is not readable!", iid);
    json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_NOT_READABLE));
    err = kNotReadableErr;
  }
  
exit:
  return err;
}


// return prop value in "read" json object if read success, else return err code in "err" object.
// NOTE: val is not used now.
OSStatus _property_read_create_response(struct mico_service_t *service_table, 
                                   const char *key, int val,
                                   json_object *out_read_obj, json_object *out_err_prop_obj)
{
  OSStatus err = kUnknownErr;
  int service_index = 0; 
  int property_index = 0;
  int iid = 0;
  int tmp_iid = 0;
  char iid_str[16] = {0};
  const char* pProertyType = NULL;
  
  require_action( service_table, exit, err = kParamErr);
  require_action( out_read_obj, exit, err = kParamErr);
  require_action( out_err_prop_obj, exit, err = kParamErr);
  
  Str2Int((uint8_t*)key, &iid);
  err = getIndexByIID(service_table, iid, &service_index, &property_index);
  require_noerr(err, exit);
  
  if( -1 == property_index){  // is a service
    tmp_iid = iid + 1;
    for(property_index = 0, pProertyType = service_table[service_index].properties[0].type; NULL != pProertyType;){
      // iid as response key
      memset(iid_str, '\0', sizeof(iid_str));
      Int2Str((uint8_t*)iid_str, tmp_iid);
      
      err = _property_read_create_response_by_index(service_table, iid_str, tmp_iid, 
                                               service_index, property_index, 
                                               out_read_obj, out_err_prop_obj);
      tmp_iid++;
      property_index++;
      pProertyType = service_table[service_index].properties[property_index].type;
    }
  }
  else{  // is a property
    err = _property_read_create_response_by_index(service_table, key, iid, 
                                             service_index, property_index, 
                                             out_read_obj, out_err_prop_obj);
  }
  
exit:
  if(kNotFoundErr == err){
    json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_NOT_FOUND));
  }
  if(kParamErr == err){
    json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_DATA_FORMAT_ERR));
  }
  return err;
}

OSStatus _property_write_create_response(struct mico_service_t *service_table, 
                                    char *key, json_object *val,
                                    json_object *out_write_obj, json_object *out_err_prop_obj)
{
  OSStatus err = kUnknownErr;
  int iid = 0;
  int service_index = 0;
  int property_index = 0;
  int ret = 0;
  
  int int_value = 0;
  float float_value = 0;
  bool boolean_value = false;
  const char *set_string = NULL;
  int set_string_len = 0;
  
  require_action(service_table, exit, err = kParamErr);
  require_action(key, exit, err = kParamErr);
  require_action(val, exit, err = kParamErr);
  require_action(out_write_obj, exit, err = kParamErr);
  require_action(out_err_prop_obj, exit, err = kParamErr);
  
  Str2Int((uint8_t*)key, &iid);
  //properties_log("properties write iid=%d.", iid);
  
  err = FindPropertyByIID(service_table, iid, &service_index, &property_index);
  require_noerr(err, exit);
  
  if( MICO_PROP_PERMS_WRITABLE (service_table[service_index].properties[property_index].perms) ){
    switch(service_table[service_index].properties[property_index].format){
    case MICO_PROP_TYPE_INT:{
      //properties_log("prop got: %s, iid=%d, value=%d", 
      //               service_table[service_index].properties[property_index].type, iid, 
      //               *((int*)service_table[service_index].properties[property_index].value));
      // property set (hardware operation)
      if(NULL != service_table[service_index].properties[property_index].set){
        int_value = json_object_get_int(val);
        ret = service_table[service_index].properties[property_index].set(&service_table[service_index].properties[property_index],
                                                                          service_table[service_index].properties[property_index].arg,
                                                                          (void*)&int_value, sizeof(int));
        if (0 == ret){  // set ok, update property value
          *((int*)service_table[service_index].properties[property_index].value) =  int_value;
          json_object_object_add(out_write_obj, key, json_object_new_int(int_value));
          err = kNoErr;
        }
        else{  // return write err status
          json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_WRITE_FAILED));
          err = kWriteErr;
        }
      }
      else{ // no set func err
        json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_NO_SET_FUNC));
        err = kWriteErr;
      }
      break;
    }
    case MICO_PROP_TYPE_FLOAT:{
      //properties_log("prop got: %s, iid=%d, value=%f", 
      //               service_table[service_index].properties[property_index].type, iid, 
      //               *((float*)service_table[service_index].properties[property_index].value));
      // property set(hardware operation)
      if(NULL != service_table[service_index].properties[property_index].set){
        float_value = json_object_get_double(val);
        ret = service_table[service_index].properties[property_index].set(&service_table[service_index].properties[property_index], 
                                                                          service_table[service_index].properties[property_index].arg,
                                                                          (void*)&float_value, sizeof(float));
        if (0 == ret){  // set ok, update property value
          *((float*)service_table[service_index].properties[property_index].value) = float_value;
          json_object_object_add(out_write_obj, key, json_object_new_double(float_value));
          err = kNoErr;
        }
        else{  // return write err status
          json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_WRITE_FAILED));
          err = kWriteErr;
        }
      }
      else{ // no set func err
        json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_NO_SET_FUNC));
        err = kWriteErr;
      }
      break;
    }
    case MICO_PROP_TYPE_STRING:{
      //properties_log("prop got: %s, iid=%d, value=%s", 
      //               service_table[service_index].properties[property_index].type, iid, 
      //               (char*)service_table[service_index].properties[property_index].value);
      
      // property set(hardware operation)
      if(NULL != service_table[service_index].properties[property_index].set){
        set_string = json_object_get_string(val);
        set_string_len = json_object_get_string_len(val);
        ret = service_table[service_index].properties[property_index].set(&service_table[service_index].properties[property_index], 
                                                                          service_table[service_index].properties[property_index].arg,
                                                                          (void*)set_string, set_string_len);
        if (0 == ret){  // set ok, update property value
          memset((char*)(service_table[service_index].properties[property_index].value), '\0', service_table[service_index].properties[property_index].maxStringLen);
          strncpy((char*)(service_table[service_index].properties[property_index].value), set_string, set_string_len);
          *(service_table[service_index].properties[property_index].value_len) = set_string_len;
          json_object_object_add(out_write_obj, key, json_object_new_string(set_string));
          err = kNoErr;
        }
        else{  // return write err status
          json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_WRITE_FAILED));
          err = kWriteErr;
        }
      }
      else{ // no set func err
        json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_NO_SET_FUNC));
        err = kWriteErr;
      }
      break;
    }
    case MICO_PROP_TYPE_BOOL:{
      //properties_log("prop got: %s, iid=%d, value=%d", 
      //               service_table[service_index].properties[property_index].type, iid, 
      //               *((bool*)service_table[service_index].properties[property_index].value));
      // property set(hardware operation)
      if(NULL != service_table[service_index].properties[property_index].set){
        boolean_value = json_object_get_boolean(val);
        ret = service_table[service_index].properties[property_index].set(&service_table[service_index].properties[property_index], 
                                                                          service_table[service_index].properties[property_index].arg,
                                                                          (void*)&boolean_value, sizeof(bool));
        if (0 == ret){  // set ok, update property value
          *((bool*)service_table[service_index].properties[property_index].value) = boolean_value;
          json_object_object_add(out_write_obj, key, json_object_new_boolean(boolean_value));
          err = kNoErr;
        }
        else{  // return write err status
          json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_WRITE_FAILED));
          err = kWriteErr;
        }
      }
      else{ // no set func err
        json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_NO_SET_FUNC));
        err = kWriteErr;
      }
      break;
    }
    default:
      properties_log("ERROR: Unsupported format!");
      json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_DATA_FORMAT_ERR));
      err = kWriteErr;
      break;
    }
  }
  else{
    properties_log("ERROR: property is read only!");
    json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_NOT_WRITABLE));
    err = kNotWritableErr;
  }
  
exit:
  if(kNotFoundErr == err){   // property not found
    properties_log("ERROR: property not found!");
    json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_NOT_FOUND));
  }
  if(kRequestErr == err){   // service can not be set
    properties_log("ERROR: service can not be set!");
    json_object_object_add(out_err_prop_obj, key, json_object_new_int(MICO_PROP_CODE_NOT_SUPPORTED));
  }
  return err;
}


OSStatus add_top(json_object **outTop, char* const service_name, json_object* services)
{
  OSStatus err = kNoErr;
  json_object *object;
  
  object = json_object_new_object();
  
  
  json_object_object_add(object, service_name, services);
  *outTop = object;
  
  return err;
}

OSStatus add_service(json_object* services, 
                     const char* type_name,  const char* type_content, 
                     const char* iid_name,  int iid_value,
                     const char* properties_name,  json_object *properties)
{
  OSStatus err;
  json_object *object;
  err = kNoErr;
  
  object = json_object_new_object();
  require_action(object, exit, err = kNoResourcesErr);
  json_object_object_add(object, type_name, json_object_new_string(type_content));
  json_object_object_add(object, iid_name, json_object_new_int(iid_value));
  json_object_object_add(object, properties_name, properties);
  json_object_array_add(services, object);
  
exit:
  return err;
}

OSStatus add_property(json_object* properties,  struct mico_prop_t property, int iid)
{
  OSStatus err = kUnknownErr;
  json_object *object, *perms_array;
  
  object = json_object_new_object();
  require_action(object, exit, err = kNoResourcesErr);
  
  // type &&¡¡iid
  json_object_object_add(object, "type", json_object_new_string(property.type));
  json_object_object_add(object, "iid", json_object_new_int(iid));
  
  // value && format
  switch(property.format){
  case MICO_PROP_TYPE_INT:{
    json_object_object_add(object, "value", json_object_new_int( *((int*)(property.value)) ) );
    json_object_object_add(object, "format", json_object_new_string("int"));
    break;
  }
  case MICO_PROP_TYPE_STRING:{
    json_object_object_add(object, "value", json_object_new_string( (char*)(property.value)) );
    json_object_object_add(object, "format", json_object_new_string("string"));
    break;
  }
  case MICO_PROP_TYPE_FLOAT:{
    json_object_object_add(object, "value", json_object_new_double( *((double*)(property.value))) );
    json_object_object_add(object, "format", json_object_new_string("float"));
    break;
  }
  case MICO_PROP_TYPE_BOOL:{
    json_object_object_add(object, "value", json_object_new_boolean( *((bool*)(property.value))) );
    json_object_object_add(object, "format", json_object_new_string("bool"));
    break;
  }
  default:{
    err= kParamErr;
    //break;
    goto exit;
  }
  }
  
  // value_len
  //json_object_object_add(object, "value_len", json_object_new_int(*((uint32_t*)(property.value_len))));
  
  // perms
  perms_array = json_object_new_array();
  require_action(perms_array, exit, err = kNoResourcesErr);
  
  if( MICO_PROP_PERMS_READABLE (property.perms) ){
    json_object_array_add(perms_array, json_object_new_string("pr"));
  }
  if( MICO_PROP_PERMS_WRITABLE (property.perms) ) {
    json_object_array_add(perms_array, json_object_new_string("pw"));
  }
  if( MICO_PROP_PERMS_NOTIFIABLE (property.perms) ) {
    json_object_array_add(perms_array, json_object_new_string("ev"));
  }
  json_object_object_add(object, "perms", perms_array);
  
  // meta data: maxValue/minValue/minStep
  if(property.hasMeta){
    if(MICO_PROP_TYPE_INT == property.format){  
      json_object_object_add(object, "maxValue", json_object_new_int(property.maxValue.intValue));
      json_object_object_add(object, "minValue", json_object_new_int(property.minValue.intValue));
      json_object_object_add(object, "minStep", json_object_new_int(property.minStep.intValue));
    }else if(MICO_PROP_TYPE_FLOAT == property.format){
      json_object_object_add(object, "maxValue", json_object_new_double(property.maxValue.floatValue));
      json_object_object_add(object, "maxValue", json_object_new_double(property.minValue.floatValue));
      json_object_object_add(object, "maxValue", json_object_new_double(property.minStep.floatValue));
    }
  }
  
  // maxStringLen
  if(MICO_PROP_TYPE_STRING == property.format){
    json_object_object_add(object, "maxStringLen", json_object_new_int(property.maxStringLen));
  }
  
  // uint
  if(NULL != property.unit){
    json_object_object_add(object, "unit", json_object_new_string(property.unit));    
  }
  
  // add to property table
  json_object_array_add(properties, object);
  err = kNoErr;
  
exit:
  if(err != kNoErr){
    if (NULL != object){
      json_object_put(object);
      object = NULL;
    }
    if(NULL != perms_array){
      json_object_put(perms_array);
      perms_array = NULL;
    }
  }
  return err;
}

json_object* mico_get_device_info(struct mico_service_t service_table[])
{
  OSStatus err = kUnknownErr;
  properties_log_trace();
  json_object *dev_info_obj = NULL, *services = NULL, *properties = NULL;
  int service_index = 0, property_index = 0;
  const char *pServiceType = NULL;
  const char *pPropertyType = NULL;
  int iid = 1;
  
  services = json_object_new_array();
  require( services, exit );
  err = add_top(&dev_info_obj, MICO_PROP_KEY_RESP_SERVICES, services);
  
  for(service_index = 0, pServiceType = service_table[0].type; NULL != pServiceType; ){
    properties = json_object_new_array();
    require_action( properties, exit, err = kNoResourcesErr );
    err = add_service(services, "type", pServiceType, 
                      "iid", iid, "properties", properties);
    require_noerr( err, exit );
    iid++;
    
    for(property_index = 0, pPropertyType = service_table[service_index].properties[0].type;  NULL != pPropertyType; ){
      err = add_property(properties, service_table[service_index].properties[property_index], iid);
      require_noerr( err, exit );
      iid++;
      property_index++;
      pPropertyType = service_table[service_index].properties[property_index].type;
    }
    
    service_index++;
    pServiceType = service_table[service_index].type;
  }
  
exit:
  if(err != kNoErr && dev_info_obj){
    properties_log("ERROR: get_device_info err, err=%d", err);
    json_object_put(dev_info_obj);
    dev_info_obj = NULL;
  }
  return dev_info_obj;
}

/* read multiple properties;
* input:  json object of property iids to read, like {"1":1, "2":2}, 
*   NOTE: function get iid from key string.
*         if input is {}, means read device meta data.
* return: 
*         return json object:
*         {
*          "services": [ ... ],
*          "read": { k:v, k:v, ... },
*          "err": { "status": <err_code>,
*                   "properties": {k:v, k:v, ...}
*                 }
*         }
*         object "read" return successfully read properties value, like {"1":100, "2":99};
*         object "services" return device meta data table;
*         object "err" return err properties and err code in value, liek {"3": -70401, "4": -70402}
*         if execute error, return NULL.
*/
json_object*  mico_read_properties(struct mico_service_t *service_table, 
                                   json_object *prop_read_list_obj)
{
  json_object *outJsonObj = NULL;
  json_object *out_read_obj = NULL;
  json_object *out_err_obj = NULL;
  json_object *out_err_prop_obj = NULL;
  int iid = 0;
  
  require( service_table, exit );
  require( prop_read_list_obj, exit );
  
  // read meata data
  if( (NULL == prop_read_list_obj) || (NULL == json_object_get_object(prop_read_list_obj)->head) ){  // input: {}
    // create "services" json data
    outJsonObj = mico_get_device_info(service_table);
    if(NULL == outJsonObj){
      properties_log("ERROR: mico_get_device_info error!");
      outJsonObj = json_object_new_object();
      require( outJsonObj, exit );
      out_err_obj = json_object_new_object();
      require( out_err_obj, exit );
      json_object_object_add(outJsonObj, MICO_PROP_KEY_RESP_ERROR, out_err_obj);
      json_object_object_add(out_err_obj, MICO_PROP_KEY_RESP_STATUS, json_object_new_int(MICO_PROP_CODE_READ_FAILED));
    }
    else{
      out_err_obj = json_object_new_object();
      require( out_err_obj, exit );
      json_object_object_add(outJsonObj, MICO_PROP_KEY_RESP_ERROR, out_err_obj);
      json_object_object_add(out_err_obj, MICO_PROP_KEY_RESP_STATUS, json_object_new_int(MICO_PROP_CODE_READ_SUCCESS));
    }
  }
  else{
    // create "read" && "err" sub-obj
    outJsonObj = json_object_new_object();
    require( outJsonObj, exit );
    
    out_read_obj = json_object_new_object();
    require( out_read_obj, exit );
    json_object_object_add(outJsonObj, MICO_PROP_KEY_RESP_READ, out_read_obj);
    
    out_err_obj = json_object_new_object();
    require( out_err_obj, exit );
    json_object_object_add(outJsonObj, MICO_PROP_KEY_RESP_ERROR, out_err_obj);
    out_err_prop_obj = json_object_new_object();
    require( out_err_prop_obj, exit );
    json_object_object_add(out_err_obj, MICO_PROP_KEY_RESP_ERROR_PROPERTIES, out_err_prop_obj);
    
    // read for each prop
    json_object_object_foreach(prop_read_list_obj, key, val) {
      iid = json_object_get_int(val);
      _property_read_create_response(service_table, key, iid, out_read_obj, out_err_prop_obj);
    }
    
    // "status" obj for read status code
    if( (NULL == json_object_get_object(out_err_prop_obj)->head) ){  // no err
      json_object_object_del(out_err_obj, MICO_PROP_KEY_RESP_ERROR_PROPERTIES);  // remove empty "properties" sub-obj
      json_object_object_add(out_err_obj,
                             MICO_PROP_KEY_RESP_STATUS, json_object_new_int(MICO_PROP_CODE_READ_SUCCESS));
    }
    else{
      json_object_object_add(out_err_obj,
                             MICO_PROP_KEY_RESP_STATUS, json_object_new_int(MICO_PROP_CODE_READ_PARTIAL_FAILED));
    }
  }
  
exit:
  return outJsonObj;
}

/* write multiple properties;
* input:  json object of property iids to read, like {"1":100, "2":99}, 
*   NOTE: function get iid from key string, get write data from value
* return: 
*         return json object:
*         {
*          "write": { k:v, k:v, ... },
*          "err": { "status": <err_code>,
*                   "properties": {k:v, k:v, ...}
*                 }
*         }
*         object "write" return successfully write properties value, like {"1":100, "2":99};
*         object "err" return err properties and err code in value, liek {"3": -70401, "4": -70402}
*         if execute error, return NULL.
*/
json_object*  mico_write_properties(struct mico_service_t *service_table, 
                                    json_object *prop_write_list_obj)
{
  json_object *outJsonObj = NULL;
  json_object *out_write_obj = NULL;
  json_object *out_err_obj = NULL;
  json_object *out_err_prop_obj = NULL;
  
  require( service_table, exit );
  require( prop_write_list_obj, exit );
  
  // create "write" && "err" sub-obj
  outJsonObj = json_object_new_object();
  require( outJsonObj, exit );
  
  out_write_obj = json_object_new_object();
  require( out_write_obj, exit );
  json_object_object_add(outJsonObj, MICO_PROP_KEY_RESP_WRITE, out_write_obj);
  
  out_err_obj = json_object_new_object();
  require( out_err_obj, exit );
  json_object_object_add(outJsonObj, MICO_PROP_KEY_RESP_ERROR, out_err_obj);
  out_err_prop_obj = json_object_new_object();
  require( out_err_prop_obj, exit );
  json_object_object_add(out_err_obj, MICO_PROP_KEY_RESP_ERROR_PROPERTIES, out_err_prop_obj);
  
  // write for each prop
  json_object_object_foreach(prop_write_list_obj, key, val) {
    _property_write_create_response(service_table, key, val, out_write_obj, out_err_prop_obj);
  }
  
  // "status" obj for write status code
  if( (NULL == json_object_get_object(out_err_prop_obj)->head) ){  // no err
    json_object_object_del(out_err_obj, MICO_PROP_KEY_RESP_ERROR_PROPERTIES);  // remove empty "properties" sub-obj
    json_object_object_add(out_err_obj,
                           MICO_PROP_KEY_RESP_STATUS, json_object_new_int(MICO_PROP_CODE_WRITE_SUCCESS));
  }
  else{
    json_object_object_add(out_err_obj,
                           MICO_PROP_KEY_RESP_STATUS, json_object_new_int(MICO_PROP_CODE_WRITE_PARTIAL_FAILED));
  }
  
exit:
  return outJsonObj;
}
