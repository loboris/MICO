MICO
====

###Mico-controller based Internet Connectivity Operation System ver 2.3.0.2


###Feathers
* Designed for embedded devices
* Based on a Real time operation system
* Support abundant MCUs
* Wi-Fi connectivity total solution
* Build-in protocols for cloud service (Plan)
* State-of-the-art low power management
* Application framework for I.O.T product
* Rich tools and mobile APP to accelerate development

###How to use:
	1. Install IAR workbench for ARM v7.4 or higher

###Links:
* [MICO](http://translate.google.com/translate?depth=1&hl=en&rurl=translate.google.com&sl=auto&tl=en&u=http://mico.io/wiki/doku.php)

# **WiFiMCU** #
WiFiMCU is developed based on EMW3165 module produced by [MXCHIP.INC](http://en.mxchip.com/). A Lua interpreter is builded inside with hardware support. A light weight file system and socket protocols can help to realize IoT development easily and quickly. Basically, you can load Lua scripts on your device and then run it on board with nothing more than a terminal connection. <br/>

#Overview
- Based on Lua 5.1.4 (package, string, table, math modules)<br/>
- Build-in modules: mcu,gpio, timer, wifi, net, file, pwm, uart, adc, spi, i2c, bit<br/>
- Modules to be builded: 1-wire, bit, mqtt...<br/>
- Integer version provided<br/>
- `Free memory >48k bytes`<br/>

##Highlights
#####Cortex-M4 microcotroller<br/>
- `STM32F411CE`<br/>
- 100MHz,Cortex-M4 core<br/>
- 2M bytes of SPI flash and 512K bytes of on-chip flash<br/>
- 128K bytes of RAM<br/>
- Operation Temperature:-30? ~ +85?<br/>

#####Multi-Interface<br/>
- 18 GPIO Pins<br/>
- 2 UARTs<br/>
- ADC(5)/SPI(1)/I2C(1)/USB(1)<br/>
- SWD debug interface<br/>
- 11 PWMs<br/>

#####`Broadcom IEEE 802.11 b/g/n RF Chip`<br/>
- Supports 802.11 b/g/n<br/>
- WEP,WPA/WPA2,PSK/Enterprise<br/>
- 16.5dBm@11b,14.5dBm@11g,13.5dBm@11n<br/>
- Receiver sensitivity:-87 dBm<br/>
- Station,Soft AP and Station+Soft AP<br/>
- CE, FCC suitable<br/>

### [Documentation](https://github.com/loboris/MICO/tree/master/Document/WiFiMCU)<br/>

### [Tutoriam](https://fineshang.gitbooks.io/wifimcu-based-on-emw3165-user-manual/content/)<br/>
