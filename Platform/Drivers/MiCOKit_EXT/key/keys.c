/**
******************************************************************************
* @file    keys.c
* @author  Eshen Wang
* @version V1.0.0
* @date    1-May-2015
* @brief   user keys operation. 
  operation
******************************************************************************
* @attention
*
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDATAG CUSTOMERS
* WITH CODATAG INFORMATION REGARDATAG THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
* TIME. AS A RESULT, MXCHIP Inc. SHALL NOT BE HELD LIABLE FOR ANY
* DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
* FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
* CODATAG INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
* <h2><center>&copy; COPYRIGHT 2014 MXCHIP Inc.</center></h2>
******************************************************************************
*/ 

#include "mico_platform.h"
#include "keys.h"

#define keys_log(M, ...) custom_log("USER_KEYS", M, ##__VA_ARGS__)
#define keys_log_trace() custom_log_trace("USER_KEYS")

/*-------------------------------- VARIABLES ---------------------------------*/
static uint32_t _default_key1_start_time = 0;
static mico_timer_t _user_key1_timer;
static uint32_t _default_key2_start_time = 0;
static mico_timer_t _user_key2_timer;

/*------------------------------ USER INTERFACES -----------------------------*/
WEAK void user_key1_clicked_callback(void)
{
  return;
}

// Key1 long pressed callback
WEAK void user_key1_long_pressed_callback(void)
{
  return;
}

// Key2 clicked callback:  next test module in test mode
WEAK void user_key2_clicked_callback(void)
{
  return;
}

// Key2 long pressed callback(use for enter MFG MODE when reset)
WEAK void user_key2_long_pressed_callback(void)
{
  return;
}

static void _user_key1_irq_handler( void* arg )
{
  (void)(arg);
  int interval = -1;
  
  if ( MicoGpioInputGet( (mico_gpio_t)USER_KEY1 ) == 0 ) {
    _default_key1_start_time = mico_get_time()+1;
    mico_start_timer(&_user_key1_timer);
    MicoGpioEnableIRQ( (mico_gpio_t)USER_KEY1, IRQ_TRIGGER_RISING_EDGE, _user_key1_irq_handler, NULL );
  }
  else {
    interval = mico_get_time() + 1 - _default_key1_start_time;
    if ( (_default_key1_start_time != 0) && interval > 50 && interval < user_key1_long_press_timeout){
      /* button clicked once */
      user_key1_clicked_callback();
      MicoGpioEnableIRQ( (mico_gpio_t)USER_KEY1, IRQ_TRIGGER_FALLING_EDGE, _user_key1_irq_handler, NULL );
    }
    mico_stop_timer(&_user_key1_timer);
    _default_key1_start_time = 0;
  }
}

static void _user_key1_timeout_handler( void* arg )
{
  (void)(arg);
  _default_key1_start_time = 0;
  MicoGpioEnableIRQ( (mico_gpio_t)USER_KEY1, IRQ_TRIGGER_FALLING_EDGE, _user_key1_irq_handler, NULL );
  if( MicoGpioInputGet( (mico_gpio_t)USER_KEY1 ) == 0){
    user_key1_long_pressed_callback();
  }
  mico_stop_timer(&_user_key1_timer);
}

static void _user_key2_irq_handler( void* arg )
{
  (void)(arg);
  int interval = -1;
  
  if ( MicoGpioInputGet( (mico_gpio_t)USER_KEY2 ) == 0 ) {
    //keys_log("_user_key2_irq_handler1");
    _default_key2_start_time = mico_get_time()+1;
    mico_start_timer(&_user_key2_timer);
    MicoGpioEnableIRQ( (mico_gpio_t)USER_KEY2, IRQ_TRIGGER_RISING_EDGE, _user_key2_irq_handler, NULL );
  } 
  else {
    //keys_log("_user_key2_irq_handler2");
    interval = mico_get_time() + 1 - _default_key2_start_time;
    if ( (_default_key2_start_time != 0) && interval > 50 && interval < user_key2_long_press_timeout){
      /* button clicked once */
      user_key2_clicked_callback();
      MicoGpioEnableIRQ( (mico_gpio_t)USER_KEY2, IRQ_TRIGGER_FALLING_EDGE, _user_key2_irq_handler, NULL );
    }
    mico_stop_timer(&_user_key2_timer);
    _default_key2_start_time = 0;
  }
}

static void _user_key2_timeout_handler( void* arg )
{
  (void)(arg);
  _default_key2_start_time = 0;
  MicoGpioEnableIRQ( (mico_gpio_t)USER_KEY2, IRQ_TRIGGER_FALLING_EDGE, _user_key2_irq_handler, NULL );
  if( MicoGpioInputGet( (mico_gpio_t)USER_KEY2 ) == 0){
    user_key2_long_pressed_callback();
  }
  mico_stop_timer(&_user_key2_timer);
}

void user_key1_init(void)
{
   MicoGpioInitialize( (mico_gpio_t)USER_KEY1, INPUT_PULL_UP );
   mico_init_timer(&_user_key1_timer, user_key1_long_press_timeout, _user_key1_timeout_handler, NULL);
   MicoGpioEnableIRQ( (mico_gpio_t)USER_KEY1, IRQ_TRIGGER_FALLING_EDGE, _user_key1_irq_handler, NULL );
}

void user_key2_init(void)
{
   MicoGpioInitialize( (mico_gpio_t)USER_KEY2, INPUT_PULL_UP );
   mico_init_timer(&_user_key2_timer, user_key2_long_press_timeout, _user_key2_timeout_handler, NULL);
   MicoGpioEnableIRQ( (mico_gpio_t)USER_KEY2, IRQ_TRIGGER_FALLING_EDGE, _user_key2_irq_handler, NULL );
}
