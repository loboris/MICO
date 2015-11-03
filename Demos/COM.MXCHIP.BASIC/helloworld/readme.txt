/**
  @page " hello_world"  demo
  
  @verbatim
  ******************** (C) COPYRIGHT 2015 MXCHIP MiCO SDK*******************
  * @file    hello_world/readme.txt 
  * @author  MDWG (MiCO Documentation Working Group)
  * @version v2.3.x 
  * @date    24-August-2015
  * @brief   Description of the  "hello world"  demo.
  ******************************************************************************

  @par Demo Description 
This demo shows:  
    - how to run MiCO system based on a hello world applicaiton program. 
      Include the following specifical function:
       - how to Print logs through MiCO UART2(system debug serial port of MiCO);
       - how to flip a LED(connected to MiCO GPIO) ON or OFF  status by calling MiCO API;
       - how to Hang up the current thread for a while by calling MiCO API.

    - MiCO UART2 is mapping to UARTx of MCU, which can refer to platform.c.
    - MICO_SYS_LED is mapping  to GPIOB 13, which can refer to platform.c. 
 

@par Directory contents 
    - COM.MXCHIP.BASIC/helloworld/hello_world.c    Applicaiton program
    - COM.MXCHIP.BASIC/helloworld/mico_config.h   MiCO function header file


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

