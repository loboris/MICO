/**
******************************************************************************
* @file    json_op.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   MiCO RTOS thread control demo.
******************************************************************************
*
*  The MIT License
*  Copyright (c) 2014 MXCHIP Inc.
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy 
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights 
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is furnished
*  to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in
*  all copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
*  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR 
*  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************
*/

#include "MiCO.h" 
#include "json_c/json.h"
#include "micokit_ext.h"
#define os_json_log(M, ...) custom_log("JSON", M, ##__VA_ARGS__)

  
void test_jsonc()  
{  
  /*control info*/
  bool rgb_sw = false;
  int  rgb_hue = 0;
  int  rgb_sat = 0;
  int  rgb_bri = 0;
  
  /*1:construct json object*/
  struct json_object *recv_json_object=NULL;
  recv_json_object=json_object_new_object();

  struct json_object *device_object=NULL;
  device_object=json_object_new_object();
  json_object_object_add(device_object, "Hardware", json_object_new_string("MiCOKit3288"));   
  json_object_object_add(device_object, "RGBSwitch", json_object_new_boolean(false)); 
  json_object_object_add(device_object, "RGBHues", json_object_new_int(0)); 
  json_object_object_add(device_object, "RGBSaturation", json_object_new_int(100));  
  json_object_object_add(device_object, "RGBBrightness", json_object_new_int(100)); 
  
  json_object_object_add(recv_json_object,"device_info",device_object);/*one pair K-V*/
  os_json_log("%s",json_object_to_json_string(recv_json_object));
  
  /*recv_json_object*/
  /*
  {"device_info": 
      {  "Hardware": "MiCOKit3288", 
         "RGBSwitch": false, 
         "RGBHues": 0, 
         "RGBSaturation": 100, 
         "RGBBrightness": 100 
      } 
  }
  */
  
  /*2:parse json object*/
  json_object* parse_json_object=json_object_object_get(recv_json_object,"device_info");
  /*get data one by one*/
  json_object_object_foreach(parse_json_object, key, val) {
          if(!strcmp(key, "RGBSwitch")){
            rgb_sw = json_object_get_boolean(val);
            os_json_log("rgb_sw=%d",rgb_sw);
          }
          else if(!strcmp(key, "RGBHues")){
            rgb_hue = json_object_get_int(val);
            os_json_log("rgb_hue=%d",rgb_hue);
          }
          else if(!strcmp(key, "RGBSaturation")){
            rgb_sat = json_object_get_int(val);
            os_json_log("rgb_sat=%d",rgb_sat);
          }
          else if(!strcmp(key, "RGBBrightness")){
            rgb_bri = json_object_get_int(val);
            os_json_log("rgb_bri=%d",rgb_bri);
          }
        }
    /*3:parse finished,free memory*/
    json_object_put(recv_json_object);/*free memory*/   
    recv_json_object=NULL;
    
    /*4:operate rgb*/
   os_json_log("control rgb led now");
   rgb_led_init();
   hsb2rgb_led_open(rgb_hue, rgb_sat, rgb_bri);/*turn red*/
}  
  
  
int application_start( void )
{  
    test_jsonc();  
    return 0;  
}  
