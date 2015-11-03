/**
******************************************************************************
* @file    mico_easylink_wac.c
* @author  William Xu
* @version V1.0.0
* @date    05-May-2014
* @brief   This file provide header file for start a Apple WAC (wireless accessory
*          configuration) function thread.
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


#include "StringUtils.h"
#include "mico.h"

#define BUNDLE_SEED_ID          "C6P64J2MZX"  
#define EA_PROTOCOL             "com.issc.datapath"

const char *eaProtocols[1] = {EA_PROTOCOL};


/**
 *   @brief Parameters controlled by the platform to configure the WAC process. 
 */
typedef struct
{
    uint8_t macAddress[ 6 ];        /**< REQUIRED: Accessory MAC address, e.g. 00:11:22:33:44:55. */
    
    bool    isUnconfigured;         /**< TRUE/FALSE: whether the accessory is unconfigured. Should be true for current cases. */
    bool    supportsAirPlay;        /**< TRUE/FALSE: whether the accessory supports AirPlay. */
    bool    supportsAirPrint;       /**< TRUE/FALSE: whether the accessory supports AirPrint. */
    bool    supports2_4GHzWiFi;     /**< TRUE/FALSE: whether the accessory supports 2.4 GHz Wi-Fi. */
    bool    supports5GHzWiFi;       /**< TRUE/FALSE: whether the accessory supports 5 GHz Wi-Fi. */
    bool    supportsWakeOnWireless; /**< TRUE/FALSE: whether the accessory supports Wake On Wireless. */
    
    char    *firmwareRevision;      /**< REQUIRED: Version of the accessory's firmware, e.g. 1.0.0. */
    char    *hardwareRevision;      /**< REQUIRED: Version of the accessory's hardware, e.g. 1.0.0. */
    char    *serialNumber;          /**< OPTIONAL: Accessory's serial number. */
    
    char    *name;                  /**< REQUIRED: Name of the accessory. */
    char    *model;                 /**< REQUIRED: Model name of the accessory. */
    char    *manufacturer;          /**< REQUIRED: Manufacturer name of the accessory. */
    
    char    **eaProtocols;          /**< OPTIONAL: Array of EA Protocol strings. */
    uint8_t numEAProtocols;         /**< OPTIONAL: Number of EA Protocol strings contained in the eaProtocols array. */
    char    *eaBundleSeedID;        /**< OPTIONAL: Accessory manufacturer's BundleSeedID. */
    
} WACPlatformParameters_t;

OSStatus startMFiWAC( mico_Context_t * const inContext, WACPlatformParameters_t *inWACPara, mico_i2c_t i2c, int timeOut )
{
	return kNoErr;
}

OSStatus system_easylink_wac_start( mico_Context_t * const inContext )
{
    OSStatus err = kNoErr;
    IPStatusTypedef para;

    WACPlatformParameters_t* WAC_Params = NULL;
    WAC_Params = calloc(1, sizeof(WACPlatformParameters_t));
    require(WAC_Params, exit);

    micoWlanGetIPStatus(&para, Station);

    str2hex((unsigned char *)para.mac, WAC_Params->macAddress, 6);
    WAC_Params->isUnconfigured          = 1;
    WAC_Params->supportsAirPlay         = 0;
    WAC_Params->supportsAirPrint        = 0;
    WAC_Params->supports2_4GHzWiFi      = 1;
    WAC_Params->supports5GHzWiFi        = 0;
    WAC_Params->supportsWakeOnWireless  = 0;

    WAC_Params->firmwareRevision =  FIRMWARE_REVISION;
    WAC_Params->hardwareRevision =  HARDWARE_REVISION;
    WAC_Params->serialNumber =      SERIAL_NUMBER;
    WAC_Params->name =              inContext->flashContentInRam.micoSystemConfig.name;
    WAC_Params->model =             MODEL;
    WAC_Params->manufacturer =      MANUFACTURER;

    WAC_Params->numEAProtocols =    1;
    WAC_Params->eaBundleSeedID =    BUNDLE_SEED_ID;
    WAC_Params->eaProtocols =       (char **)eaProtocols;

    err = startMFiWAC( inContext, WAC_Params, MICO_I2C_CP, 1200 );
    free(WAC_Params);
    require_noerr( err, exit );  
exit:
    return err; 
}










