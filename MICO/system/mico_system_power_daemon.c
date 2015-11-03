/**
******************************************************************************
* @file    mico_system_power_daemon.c 
* @author  William Xu
* @version V1.0.0
* @date    05-May-2014
* @brief   This file provide the power management daemon and provide a safety
*          power operation to developer.
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

#include "MICO.h"

static bool                     needs_update          = false;


extern void sendNotifySYSWillPowerOff(void);


USED void PlatformEasyLinkButtonClickedCallback(void)
{
  system_log_trace();
  mico_Context_t* context = NULL;
  
  context = mico_system_context_get( );
  require( context, exit );
  
  if(context->flashContentInRam.micoSystemConfig.easyLinkByPass != EASYLINK_BYPASS_NO){
    context->flashContentInRam.micoSystemConfig.easyLinkByPass = EASYLINK_BYPASS_NO;
    needs_update = true;
  }
  
  if(context->flashContentInRam.micoSystemConfig.configured == allConfigured){
    context->flashContentInRam.micoSystemConfig.configured = wLanUnConfigured;
    needs_update = true;
  }

  mico_system_power_perform( context, eState_Software_Reset );

exit: 
  return;
}

USED void PlatformEasyLinkButtonLongPressedCallback(void)
{
  system_log_trace();
  mico_Context_t* context = NULL;
  
  context = mico_system_context_get( );
  require( context, exit );

  mico_system_context_restore( context );
  
  mico_system_power_perform( context, eState_Software_Reset );

exit:
  return;
}

USED void PlatformStandbyButtonClickedCallback(void)
{
  system_log_trace();
  mico_Context_t* context = NULL;
  
  context = mico_system_context_get( );
  require( context, exit );
  
  mico_system_power_perform( context, eState_Standby );

exit: 
  return;
}


static void _sys_state_thread(void *arg)
{  
  UNUSED_PARAMETER(arg);
  mico_Context_t* context = arg;
  
  /*System status changed*/
  while(mico_rtos_get_semaphore( &context->micoStatus.sys_state_change_sem, MICO_WAIT_FOREVER) == kNoErr ){
    
    if(needs_update == true)
      mico_system_context_update( context );
    
    switch( context->micoStatus.current_sys_state ){
    case eState_Normal:
      break;
    case eState_Software_Reset:
      sendNotifySYSWillPowerOff( );
      mico_thread_msleep( 500 );
      MicoSystemReboot( );
      break;
    case eState_Wlan_Powerdown:
      sendNotifySYSWillPowerOff( );
      mico_thread_msleep( 500 );
      micoWlanPowerOff( );
      break;
    case eState_Standby:
      sendNotifySYSWillPowerOff( );
      mico_thread_msleep( 500 );
      micoWlanPowerOff( );
      MicoSystemStandBy( MICO_WAIT_FOREVER );
      break;
    default:
      break;
    }
  }
  mico_rtos_delete_thread( NULL );
}

OSStatus mico_system_power_daemon_start( mico_Context_t* const in_context )
{
  OSStatus err = kNoErr;

  err = mico_rtos_init_semaphore( &in_context->micoStatus.sys_state_change_sem, 1 ); 
  require_noerr(err, exit);

  in_context->micoStatus.current_sys_state = eState_Normal;

  mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "Power Daemon", _sys_state_thread, 800, (void *)in_context ); 
  require_noerr(err, exit);
  
exit:
  return kNoErr;
}


OSStatus mico_system_power_perform( mico_Context_t* const in_context, mico_system_state_t new_state )
{
  OSStatus err = kNoErr;
  mico_Context_t* context = NULL;
  
  context = mico_system_context_get( );
  require_action( context, exit, err = kNotPreparedErr );

  in_context->micoStatus.current_sys_state = new_state;
  require( in_context->micoStatus.sys_state_change_sem, exit);
  mico_rtos_set_semaphore( &in_context->micoStatus.sys_state_change_sem );   

exit:
  return err; 
}









