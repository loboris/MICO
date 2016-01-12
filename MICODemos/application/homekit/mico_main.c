/**
  ******************************************************************************
  * @file    MICOAppEntrance.c 
  * @author  William Xu
  * @version V1.0.0
  * @date    05-May-2014
  * @brief   Mico application entrance, addd user application functons and threads.
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
#include "mico_app_define.h"
#include "HomeKit.h"
#include "HomeKitPairList.h"

#ifdef USE_MiCOKit_EXT
#include "MiCOKit_EXT/micokit_ext.h"
#endif

#define USE_PASSWORD

app_context_t* app_context = NULL;
extern hapProduct_t hap_product;

#ifdef USE_PASSWORD
/* Raw type password */
static const char *password = "454-45-454";
#else
/* Verifier and salt for raw type password "454-45-454" */
static const uint8_t verifier[] =  {0x2C,0x21,0x47,0x4C,0x67,0x0B,0xEF,0x8F,0xA6,0x4A,0xF9,0x61,0x46,0xDA,0xB4,0x30,
                                    0x9B,0xB0,0x73,0x35,0xC6,0x7C,0xB5,0xA9,0xFC,0x7A,0x2A,0x02,0xC1,0x04,0x3A,0x49,
                                    0x9E,0xEA,0x6B,0x7F,0x52,0xDD,0xBF,0x75,0xB6,0x4A,0xB6,0x7D,0x78,0x39,0x09,0x6C,
                                    0x6B,0x26,0x9B,0x20,0xE8,0x3B,0xB6,0x13,0xCD,0x2F,0x9C,0x3E,0x08,0x93,0x9F,0x3C,
                                    0x76,0xDA,0x75,0x40,0xD6,0xC9,0xAB,0x76,0xF5,0xD8,0x18,0xF4,0xD6,0x16,0xE1,0xDF,
                                    0xD9,0x99,0xD1,0x16,0xEC,0xE0,0x76,0x04,0x83,0xC1,0x58,0x42,0x17,0x4F,0x0A,0x8B,
                                    0x8A,0xC7,0xF5,0xAA,0x76,0xA7,0x7E,0xF0,0xE8,0x1D,0xBB,0xD6,0xBE,0xCF,0xA5,0x7B,
                                    0x12,0x72,0xAD,0x2B,0x9B,0x5C,0xAB,0x12,0x9E,0xE1,0x1B,0xF8,0x92,0xB0,0xF4,0xDA,
                                    0x37,0x38,0x7C,0x0D,0x6F,0x8F,0xE1,0xF4,0x0D,0xB4,0x4E,0xFD,0x4B,0xB5,0xEE,0xB1,
                                    0xD5,0xBF,0x0A,0xC9,0xCB,0xC9,0x2D,0xA6,0xFC,0x6B,0x51,0x32,0x07,0x86,0x29,0x09,
                                    0xAA,0x02,0x56,0x83,0x26,0xAC,0x55,0xD8,0x21,0x61,0x1A,0xE4,0xFF,0x8B,0x4F,0xDA,
                                    0x48,0x89,0x75,0x05,0x5B,0xD0,0x76,0x98,0xA1,0xC7,0x72,0x8D,0xD3,0x6C,0x7C,0x86,
                                    0x3A,0x69,0x47,0xD0,0xEA,0xED,0x95,0x82,0x36,0x67,0xEF,0x94,0xEB,0x13,0xCC,0x96,
                                    0xB9,0x9C,0x1E,0x30,0xF9,0x14,0x75,0xBA,0x71,0x41,0x7E,0x81,0xD2,0x10,0xC5,0xF9,
                                    0x37,0x20,0xC9,0x78,0x28,0xF9,0x67,0x47,0x69,0x5D,0xB9,0xBF,0x6D,0x86,0x40,0x90,
                                    0xC4,0x97,0x38,0x4E,0x13,0x6F,0xE8,0xB3,0xA8,0x6C,0x8C,0xF9,0x6B,0xD2,0x46,0x13,
                                    0xC8,0x80,0x90,0xCE,0x72,0xB3,0xCF,0x04,0xC8,0x62,0x1B,0x80,0xA2,0x0C,0x60,0xAA,
                                    0xD3,0x5E,0x6E,0x68,0xB1,0x57,0xFA,0x58,0x3D,0x10,0x5F,0x2B,0xA5,0x5C,0x34,0xD0,
                                    0x55,0x53,0x01,0x01,0xA6,0xEA,0x0A,0xD1,0x3F,0x61,0xAF,0xCE,0x9E,0x57,0xAE,0xE5,
                                    0xF6,0x88,0x86,0x23,0x1A,0x76,0x56,0x3B,0x9F,0x60,0x6D,0x8F,0x38,0xA0,0xAD,0x14,
                                    0x1C,0x82,0x5B,0xC9,0xA3,0xAE,0x7A,0x3E,0xB3,0xE1,0x24,0x16,0xBE,0x0D,0x07,0x8E,
                                    0xFA,0xBF,0xBD,0x4C,0xA0,0x39,0xE5,0xC1,0x8F,0x4C,0xAE,0xD5,0x7D,0xAB,0x74,0x29,
                                    0xE3,0x15,0xC4,0xC6,0x57,0x14,0x94,0xA7,0xAB,0x1E,0x05,0x85,0x93,0x2A,0x8A,0x72,
                                    0xC8,0x1C,0x96,0x9D,0xAA,0x03,0xD6,0x82,0x54,0x64,0x7E,0x6F,0xBB,0xD1,0xB4,0x1A};

const uint8_t salt[] = {0x8F,0xCC,0xC9,0xC7,0xEA,0x97,0xA7,0x76,0xE5,0xCA,0x9C,0xF0,0x69,0x0C,0x0A,0xAF};
#endif

static mico_timer_t _sensor_timer;

static void _sensor_read_handler( void* arg );
static void HKCharacteristicInit( void );


/* MICO system callback: Restore default configuration provided by application */
void appRestoreDefault_callback(void * const user_config_data, uint32_t size)
{
  UNUSED_PARAMETER(size);
  application_config_t* appConfig = user_config_data;
  appConfig->configDataVer = CONFIGURATION_VERSION;
  memset(appConfig->LTSK, 0x0, 32);
  memset(appConfig->LTPK, 0x0, 32);
  appConfig->paired = false;
  appConfig->config_number = 1;
  memset(appConfig->pairList.pairInfo, 0x0, sizeof(pair_list_in_flash_t));
}

/* HomeKit daemon callback function */
OSStatus pk_sk_cb(unsigned char *pk, unsigned char *sk, bool write)
{
  OSStatus err = kNoErr;
  if( write == true){ //write key to flash
    memcpy(app_context->appConfig->LTPK, pk, 32);
    memcpy(app_context->appConfig->LTSK, sk, 32);
    app_context->appConfig->paired = true;   
    err =  mico_system_context_update( mico_system_context_get( ) );
  }else{ //read key from flash
    require_action(app_context->appConfig->paired == true, exit, err = kNotInitializedErr);
    memcpy(pk, app_context->appConfig->LTPK, 32);
    memcpy(sk, app_context->appConfig->LTSK, 32);
  }

exit:
  return err;
}

/* Application entrance */
OSStatus application_start( void *arg )
{
  UNUSED_PARAMETER(arg);
  OSStatus err = kNoErr;
  hk_init_t hk_init;
  
  mico_Context_t* mico_context;

  /* Create application context */
  app_context = ( app_context_t *)calloc(1, sizeof(app_context_t) );
  require_action( app_context, exit, err = kNoMemoryErr );

  /* Create mico system context and read application's config data from flash */
  mico_context = mico_system_context_init( sizeof( application_config_t) );
  app_context->appConfig = mico_system_context_get_user_data( mico_context );
  
  /* mico system initialize */
  err = mico_system_init( mico_context );
  require_noerr( err, exit );
  
  /* Start HomeKit daemon */
  memset(&hk_init, 0x0, sizeof(hk_init_t));
  hk_init.hap_product = &hap_product;
  hk_init.mfi_cp_port = MICO_I2C_CP;
  hk_init.ci          = CI_LIGHTBULB;
  hk_init.model       = MODEL;
  hk_init.config_number = 2;
#ifdef USE_PASSWORD
  hk_init.password    = (uint8_t *)password;
  hk_init.password_len = strlen(password);
#else
  hk_init.verifier    = (uint8_t *)verifier;
  hk_init.verifier_len= sizeof(verifier);
  hk_init.salt        = (uint8_t *)salt;
  hk_init.salt_len    = sizeof(salt);
#endif
  hk_init.key_storage = pk_sk_cb;

  err = hk_server_start( hk_init );
  require_noerr( err, exit );
  
  HKCharacteristicInit( );

exit:
  mico_rtos_delete_thread(NULL);
  return err;
}

static void HKCharacteristicInit( void )
{
  OSStatus err = kNoErr;
  int32_t temperature = 24;
  uint32_t humidity = 50;
  uint16_t infra_data = 3000;
  mico_Context_t* mico_context = mico_system_context_get();
  
#ifdef USE_MiCOKit_EXT
  hsb2rgb_led_close( );
  dc_motor_set(0); 
  err = temp_hum_sensor_read(&temperature,  &humidity);
  infrared_reflective_read( &infra_data );
  OLED_Clear();
  OLED_ShowString( 0, 0, "HomeKit Demo" );
  OLED_ShowString( 0, 2, "RGB LED OFF   " );
  OLED_ShowString( 0, 4, "DC MOTOR OFF   " );
  OLED_ShowString( 0, 6, (uint8_t *)mico_context->micoStatus.mac );
#endif
  
  mico_init_timer(&_sensor_timer, 2000, _sensor_read_handler, NULL);
  mico_start_timer(&_sensor_timer);

  app_context->lightbulb.on                               = false;
  app_context->lightbulb.brightness                       = 100;
  app_context->lightbulb.hue                              = 180;
  app_context->lightbulb.saturation                       = 80;

  app_context->fan.on                                     = false;

  if( err==kNoErr ){
    app_context->temp.temp                                  = temperature;
    app_context->temp.temp_status                           = kHKNoErr;
    app_context->humidity.humidity                          = humidity;
    app_context->humidity.humidity_status                   = kHKNoErr;
  }else{
    app_context->temp.temp_status                           = kHKCommunicateErr;   
    app_context->humidity.humidity_status                   = kHKCommunicateErr; 
  }

  app_context->occupancy.occupancy = ( infra_data >= 3000 )?0:1;
}

#ifdef USE_MiCOKit_EXT  
void user_key1_clicked_callback(void)
{
  if( app_context->lightbulb.on == true ){
    hsb2rgb_led_close( );
    app_context->lightbulb.on = false;
    OLED_ShowString( 0, 2, "RGB LED OFF   " );
  }else{
    hsb2rgb_led_open( app_context->lightbulb.hue, app_context->lightbulb.saturation, app_context->lightbulb.brightness );
    app_context->lightbulb.on = true;
    OLED_ShowString( 0, 2, "RGB LED ON    " );
  }
}

void user_key2_clicked_callback(void)
{
  if( app_context->fan.on == true ){
    dc_motor_set( 0 );
    app_context->fan.on = false;
    OLED_ShowString( 0, 4, "DC MOTOR OFF   " );
  }else{
    dc_motor_set( 1 );
    app_context->fan.on = true;
    OLED_ShowString( 0, 4, "DC MOTOR ON    " );
  }
}
#endif



static void _sensor_read_handler( void* arg )
{
  UNUSED_PARAMETER(arg);
  OSStatus err = kNoErr;
  int32_t temperature = 24;
  uint32_t humidity = 50;
  uint16_t infra_data = 3000;
  int occupancy = 0;
  
#ifdef USE_MiCOKit_EXT  
  err = temp_hum_sensor_read(&temperature,  &humidity);
  infrared_reflective_read( &infra_data );
  occupancy = (infra_data >= 3000)? 0:1;
#endif
  
  if( err==kNoErr ){
    app_context->temp.temp                                  = temperature;
    app_context->temp.temp_status                           = kHKNoErr;
    app_context->humidity.humidity                          = humidity;
    app_context->humidity.humidity_status                   = kHKNoErr;
  }else{
    app_context->temp.temp_status                           = kHKCommunicateErr;   
    app_context->humidity.humidity_status                   = kHKCommunicateErr; 
  }

  app_context->occupancy.occupancy                        = occupancy;
  app_context->occupancy.occupancy_status                 = kHKNoErr;
}
