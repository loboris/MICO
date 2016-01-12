

#include "mico.h"
#include "HomeKit.h"
#include "mico_app_define.h"

#include "StringUtils.h"

#ifdef USE_MiCOKit_EXT
#include "MiCOKit_EXT/micokit_ext.h"
#endif

extern app_context_t* app_context;

HkStatus HKExcuteUnpairedIdentityRoutine( void )
{
#ifdef USE_MiCOKit_EXT
  hsb2rgb_led_open( app_context->lightbulb.hue, app_context->lightbulb.saturation, app_context->lightbulb.brightness );
  mico_thread_msleep(200);
  hsb2rgb_led_close(  );
  mico_thread_msleep(200);
  hsb2rgb_led_open( app_context->lightbulb.hue, app_context->lightbulb.saturation, app_context->lightbulb.brightness );
  mico_thread_msleep(200);
  hsb2rgb_led_close(  );
  mico_thread_msleep(200);
  hsb2rgb_led_open( app_context->lightbulb.hue, app_context->lightbulb.saturation, app_context->lightbulb.brightness );
  mico_thread_msleep(200);
  hsb2rgb_led_close(  );
#endif
  /* Device identity routine */
  return kNoErr;
}

HkStatus HKReadCharacteristicStatus(int accessoryID, int serviceID, int characteristicID)
{
  UNUSED_PARAMETER(accessoryID);
  if(serviceID == 2){
    switch(characteristicID){
      case 1:
        return app_context->lightbulb.on_status;
        break;
      case 2:
        return app_context->lightbulb.brightness_status;
        break;
      case 3:
        return app_context->lightbulb.hue_status;
        break;
      case 4:
        return app_context->lightbulb.saturation_status;
        break;
      default:
        return kHKNoErr;
        break;       
    }
  }else if(serviceID == 3){
    switch(characteristicID){
      case 1:
        return app_context->fan.on_status;
        break;
      default:
        return kHKNoErr;
        break;       
    }    
  }else if(serviceID == 4){
    switch(characteristicID){
      case 1:
        return app_context->temp.temp_status;
        break;
      default:
        return kHKNoErr;
        break;       
    }    
  }else if(serviceID == 5){
    switch(characteristicID){
      case 1:
        return app_context->humidity.humidity_status;
        break;
      default:
        return kHKNoErr;
        break;       
    }    
  }
  return kHKNoErr;
}


HkStatus HKReadCharacteristicValue( int accessoryID, int serviceID, int characteristicID, value_union *value)
{
  (void)accessoryID;
  HkStatus err = kHKNoErr; 
  mico_Context_t *mico_context = mico_system_context_get();

  if(serviceID == 1){
    switch(characteristicID){
      case 1:
        (*value).stringValue = mico_context->flashContentInRam.micoSystemConfig.name;
        err = kHKNoErr;
        break;
    }
  }
  else if(serviceID == 2){
    switch(characteristicID){
      case 1:
        (*value).boolValue = app_context->lightbulb.on; 
        err = app_context->lightbulb.on_status;
        break;
      
      case 2:
        (*value).intValue = app_context->lightbulb.brightness;  
        err = app_context->lightbulb.brightness_status;
        break;

      case 3:
        (*value).floatValue = app_context->lightbulb.hue; 
        err = app_context->lightbulb.hue_status;
        break;
           
      case 4:
        (*value).floatValue = app_context->lightbulb.saturation;    
        err = app_context->lightbulb.saturation_status;
        break;
    }
  }else if(serviceID == 3){
    switch(characteristicID){
      case 1:
        (*value).boolValue = app_context->fan.on; 
        err = app_context->fan.on_status;
        break;
    }
  }else if(serviceID == 4){
    switch(characteristicID){
      case 1:
        (*value).floatValue = app_context->temp.temp; 
        err = app_context->temp.temp_status;
        break;
    }
  }else if(serviceID == 5){
    switch(characteristicID){
      case 1:
        (*value).floatValue = app_context->humidity.humidity; 
        err = app_context->humidity.humidity_status;
        break;
    }
  }else if(serviceID == 6){
    switch(characteristicID){
      case 1:
        (*value).intValue = app_context->occupancy.occupancy; 
        err = app_context->occupancy.occupancy_status;
        break;
    }
  }
  return err;
}


void HKWriteCharacteristicValue(int accessoryID, int serviceID, int characteristicID, value_union value, bool moreComing)
{
  (void)accessoryID;  
   mico_Context_t *mico_context = mico_system_context_get();

  /*You can store the new value in xxxx_new and operate target later when moreComing == false
    or write to target right now!*/
  if(serviceID == 1){
    if(characteristicID == 1)
      strncpy(mico_context->flashContentInRam.micoSystemConfig.name, value.stringValue, 64);
    else if(characteristicID == 5)
      HKExcuteUnpairedIdentityRoutine( );
  }
  else if(serviceID == 2){
    switch(characteristicID){
      case 1:
        app_context->lightbulb.on_new = value.boolValue;
        app_context->lightbulb.on_status = kHKBusyErr;
        break;
      
      case 2:
        app_context->lightbulb.brightness_new = value.intValue;
        app_context->lightbulb.brightness_status = kHKBusyErr;
        break;

      case 3:
        app_context->lightbulb.hue_new = value.floatValue; 
        app_context->lightbulb.hue_status = kHKBusyErr;
        break;
           
      case 4:
        app_context->lightbulb.saturation_new = value.floatValue;
        app_context->lightbulb.saturation_status = kHKBusyErr;
        break;
      }
    }else if(serviceID == 3){
      switch(characteristicID){
        case 1:
          app_context->fan.on_new = value.boolValue;
          app_context->fan.on_status = kHKBusyErr;
          break;
      }      
    }

    /*Operate hardware to write all Characteristic in one service in one time, this is useful when 
      one command send to taget device can set multiple Characteristic*/
  if(!moreComing){
    /*Control lightbulb*/
    /*.................*/

    if(app_context->lightbulb.on_status == kHKBusyErr){
      app_context->lightbulb.on = app_context->lightbulb.on_new;
      app_context->lightbulb.on_status = kNoErr;      
    }

    if(app_context->lightbulb.brightness_status == kHKBusyErr){
      app_context->lightbulb.brightness = app_context->lightbulb.brightness_new;
      app_context->lightbulb.brightness_status = kNoErr;
    }

    if(app_context->lightbulb.hue_status == kHKBusyErr){
      app_context->lightbulb.hue = app_context->lightbulb.hue_new; 
      app_context->lightbulb.hue_status = kNoErr;
    }

    if(app_context->lightbulb.saturation_status == kHKBusyErr){
      app_context->lightbulb.saturation = app_context->lightbulb.saturation_new;
      app_context->lightbulb.saturation_status = kNoErr;
    }

#ifdef USE_MiCOKit_EXT
    if( app_context->lightbulb.on == false){
      hsb2rgb_led_close(  );
      OLED_ShowString( 0, 2, "RGB LED OFF   " );
    }else{
      hsb2rgb_led_open( app_context->lightbulb.hue, app_context->lightbulb.saturation, app_context->lightbulb.brightness );
      OLED_ShowString( 0, 2, "RGB LED ON    " );
    }
#endif

    if(app_context->fan.on_status == kHKBusyErr){
      app_context->fan.on = app_context->fan.on_new;
      app_context->fan.on_status = kNoErr;    
#ifdef USE_MiCOKit_EXT
      dc_motor_set( (app_context->fan.on)?1:0 );   
      OLED_ShowString( 0, 4, (app_context->fan.on)?(uint8_t *)"DC MOTOR ON    ":(uint8_t *)"DC MOTOR OFF   " );
#endif
    }
  }
  return;
}


