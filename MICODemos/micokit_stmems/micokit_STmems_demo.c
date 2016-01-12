/**
******************************************************************************
* @file    micokit_STmems_demo.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   Sensor test for MiCOKit ST mems board
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

#include "mico.h" 
#include "MiCOKit_STmems/micokit_STmems.h"

#define app_log(format, ...)  custom_log("MiCOKit_STmems", format, ##__VA_ARGS__)

void micokit_STmems_key1_clicked_callback(void)
{
  dc_motor_set(0);
}

void micokit_STmems_key2_clicked_callback(void)
{
  dc_motor_set(1);
}


int application_start( void )
{
  OSStatus err = kNoErr;
  float lps25hb_temp_data = 0;
  float lps25hb_pres_data = 0;
  float hts221_temp_data = 0;
  float hts221_hum_data = 0;
  float uvis25_index_data = 0;
  int16_t ACC_X, ACC_Y, ACC_Z;
  int16_t MAG_X, MAG_Y, MAG_Z;
  int rgb_led_hue = 0;
  uint16_t light_adc;
  char oled_string[32];
  mico_Context_t* context;
  
  /* Start MiCO system functions according to mico_config.h*/
  err = mico_system_init( mico_system_context_init( 0 ) );
  require_noerr_string( err, exit, "ERROR: Unable to Init MiCO core" );
  
  micokit_STmems_init();
  
  context = mico_system_context_get();
  OLED_Clear();
  OLED_ShowString( 0, OLED_DISPLAY_ROW_1, (uint8_t *)context->micoStatus.mac );
  
  while(1)
  {
    hsb2rgb_led_open(rgb_led_hue, 100, 5);
    rgb_led_hue = (rgb_led_hue + 120)%360;
    
    app_log("//////////////////////////");
    err = hts221_Read_Data( &hts221_temp_data, &hts221_hum_data );
    require_noerr_string( err, exit, "ERROR: Can't Read HTS221 Data" );
    sprintf( oled_string, "%.1fC %.1f%%", hts221_temp_data, hts221_hum_data);
    OLED_ShowString( 0, OLED_DISPLAY_ROW_2, (uint8_t *)oled_string );
    app_log("Temp:%.1fC Humi%.1f%%",hts221_temp_data, hts221_hum_data);
    
    err = lps25hb_Read_Data( &lps25hb_temp_data, &lps25hb_pres_data );
    require_noerr_string( err, exit, "ERROR: Can't Read LPS25HB Data" );
    sprintf( oled_string, "%.2fm", lps25hb_pres_data );
    OLED_ShowString( 0, OLED_DISPLAY_ROW_3, (uint8_t *)oled_string );
    app_log("Pressure: %.2fm", lps25hb_pres_data);
    
    err = uvis25_Read_Data(&uvis25_index_data);
    require_noerr_string( err, exit, "ERROR: Can't Read UVIS25 Data" );
    sprintf( oled_string, "%.1fuw", uvis25_index_data );
    OLED_ShowString( 9*8, OLED_DISPLAY_ROW_3, (uint8_t *)oled_string );
    app_log("%.1fuw",uvis25_index_data);
    
    err = lsm9ds1_acc_read_data( &ACC_X, &ACC_Y, &ACC_Z );
    require_noerr_string( err, exit, "ERROR: Can't Read LSM9DS1 Data" );
    sprintf( oled_string, "%d %d %d        ", ACC_X, ACC_Y, ACC_Z );
    OLED_ShowString( 0, OLED_DISPLAY_ROW_4, (uint8_t *)oled_string );
    app_log( "X=%d Y=%d Z=%d", ACC_X, ACC_Y, ACC_Z );
    
    err = lsm9ds1_mag_read_data( &MAG_X, &MAG_Y, &MAG_Z );
    require_noerr_string( err, exit, "ERROR: Can't Read LSM9DS1 Data" );
    app_log( "MAGX=%d MAGY=%d MAGZ=%d", MAG_X, MAG_Y, MAG_Z ); 
    
    light_sensor_read(&light_adc);
    app_log("Light ADC: %d",light_adc);    

    mico_thread_sleep(1); 
  }
  
exit:
  mico_rtos_delete_thread( NULL );
  return err; 
}

