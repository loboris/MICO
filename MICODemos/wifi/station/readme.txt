/**
  @page "wifi_station"  demo
  
  @verbatim
  ******************** (C) COPYRIGHT 2015 MXCHIP MiCO SDK*******************
  * @file    wifi_station/readme.txt 
  * @author  MDWG (MiCO Documentation Working Group)
  * @version v2.3.x
  * @date    24-August-2015
  * @brief   Description of the  "wifi_station"  demo.
  ******************************************************************************

  @par Demo Description 
 This demo includes 2 parts:
    - wifi_station_system_api.c:  Shows how to configure device as Station mode flexiblly. 
                                               This demo need user to start" Easylink" and configure network for device.
                                               Download and Get more information  about "Easylink"  please refer to http://mico.io/wiki/doku.php?id=micoappdownload
    - wifi_station_core_api.c:     Shows how to configure device as station mode fixed.
                                                This demo writes WiFi information to the device firmware.

@par Directory contents 
    - COM.MXCHIP.BASIC/wifi/station/wifi_station_system_api.c  Easylink network configuration  Applicaiton program
    - COM.MXCHIP.BASIC/wifi/station/wifi_station_core_api.c      Fixed network configuration Applicaiton program
    - COM.MXCHIP.BASIC/wifi/station/mico_config.h   MiCO function header file


@par Hardware and Software environment         
    - This demo has been tested on MiCOKit-3165 board.
    - This demo can be easily tailored to any other supported device and development board.


@par How to use it ? 
In order to make the program work, you must do the following :
 - Open your preferred toolchain, 
    - IDE:  IAR 7.30.4 or Keil MDK 5.13.       
    - Debugging Tools: JLINK or STLINK
 - Modify header file path of  "mico_config.h".   Please referring to "http://mico.io/wiki/doku.php?id=confighchange"
 - Rebuild all files and load your image into target memory.   Please referring to "http://mico.io/wiki/doku.php?id=debug"
 - Run the demo.
 - View operating results and system serial log (Serial port: Baud rate: 115200, data bits: 8bit, parity: No, stop bits: 1).   Please referring to http://mico.io/wiki/doku.php?id=com.mxchip.basic

**/

