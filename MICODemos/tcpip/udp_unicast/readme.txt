/**
  @page "udp_unicast"  demo
  
  @verbatim
  ******************** (C) COPYRIGHT 2015 MXCHIP MiCO SDK*******************
  * @file    udp_unicast/readme.txt 
  * @author  MDWG (MiCO Documentation Working Group)
  * @version v2.3.x
  * @date    24-August-2015
  * @brief   Description of the  "udp_unicast"  demo.
  ******************************************************************************


 @par Demo Description 
  This demo shows:  how to start an UDP unicast sevice, and receive data from designated IP adress and designated port after Easylink configuration.


@par Directory contents 
    - COM.MXCHIP.BASIC/tcpip/udp_unicast/udp_unicast.c    Applicaiton program
    - COM.MXCHIP.BASIC/tcpip/udp_unicast/mico_config.h   MiCO function header file


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

