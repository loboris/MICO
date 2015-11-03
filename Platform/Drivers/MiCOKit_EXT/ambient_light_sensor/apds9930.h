/**
******************************************************************************
* @file    apds9930.h
* @author  William Xu
* @version V1.0.0
* @date    17-Mar-2015
* @brief   apds9930 controller operation
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
#ifndef __APDS9930_H_
#define __APDE9930_H_

#include "mico_platform.h"


#ifdef USE_MiCOKit_EXT
  #include "micokit_ext_def.h"
  #define APDS9930_I2C_DEVICE      Arduino_I2C
#else
  #define APDS9930_I2C_DEVICE      MICO_I2C_NONE
#endif

#define APDS9930_ID   0x39    //APDS-9930 device id
#define GA  0.49              //Glass (or Lens) Attenuation Factor
#define DF  52                //Device Factor, DF = 52 for APDS-9930

/*Specifis register address*/
/*R/W*/
#define ENABLE_ADDR   0x00    //Enable of states and interrupts
#define ATIME_ADDR    0x01    //ALS ADC time
#define PTIME_ADDR    0x02    //Proximity ADC time
#define WTIME_ADDR    0x03    //Wait time
#define AILTL_ADDR    0x04    //ALS interrupt low threshold low byte
#define AILTH_ADDR    0x05    //ALS interrupt low threshold hi byte
#define AIHTL_ADDR    0x06    //ALS interrupt hi threshold low byte
#define AIHTH_ADDR    0x07    //ALS interrupt hi threshold hi byte
#define PILTL_ADDR    0x08    //Proximity interrupt low threshold low byte
#define PILTH_ADDR    0x09    //Proximity interrupt low threshold hi byte
#define PIHTL_ADDR    0x0A    //Proximity interrupt hi threshold low byte
#define PIHTH_ADDR    0x0B    //Proximity interrupt hi threshold hi byte
#define PERS_ADDR     0x0C    //Interrupt persistence fiters
#define CONFIG_ADDR   0x0D    //Confiuration
#define PPULSE_ADDR   0x0E    //Proximity pulse count
#define CONTROL_ADDR  0x0F    //Gain control register
#define POFFSET_ADDR  0x1E    //Proximity offet register
#define CLIT_ADDR     0xE7    //Clear interrupt
/*R*/
#define ID_ADDR       0x12    //Device ID
#define STATUS_ADDR   0x13    //Device status
#define Ch0DATAL_ADDR 0x14    //Ch0 ADC low data register
#define Ch0DATAH_ADDR 0x15    //Ch0 ADC high data register
#define Ch1DATAL_ADDR 0x16    //Ch1 ADC low data register
#define Ch1DATAH_ADDR 0x17    //Ch1 ADC high data register
#define PDATAL_ADDR   0x18    //Proximity ADC low data register
#define PDATAH_ADDR   0x19    //Proximity ADC high data register


/*Command Value*/
/*ENABLE reg value*/
#define APDS9930_DISABLE       0x00    //Disable and Powerdown
#define WEN           0x08    // Enable Wait
#define PEN           0x04    //Enable Prox
#define AEN           0x02    //Enable ALS
#define PON           0x01    //Enable Power On
/*CONFIG reg value*/
#define RECONFIG      0x00 
/*CONTROL reg value*/
#define PDRIVE_100    0x00    //100mA of LED Power
#define PDIODE_CH1    0x20    //CH1 Diode
#define PGAIN_1x      0x00    //1x Prox gain
#define AGAIN_1x      0x00    //1x ALS gain
/*ATIME reg value*/
#define ATIME_1C      0xFF    //2.7 ms 每 minimum ALS integration time
#define ATIME_256C    0x00    //699 ms 每 ALS integration time
/*PTIME reg value*/
#define PTIME_1C      0xFF    //2.7 ms 每 minimum Prox integration time
#define PTIME_10C     0xF6    //Prox integration time
/*WTIME reg value*/
#define WTIME_1C      0xFF    // 2.7 ms 每 minimum Wait time
#define WTIME_74C     0xB6    //202
#define WTIME_256C    0x00    //699
/*PPULSE reg value*/
#define PPULSE_MIN    0x08    //Minimum prox pulse count


OSStatus apds9930_sensor_init(void);
OSStatus apds9930_sensor_deinit(void);
OSStatus apds9930_data_readout(uint16_t *Prox_data, uint16_t *Lux_data);
void apds9930_enable(void);

#endif  // __APDS9930_H_

