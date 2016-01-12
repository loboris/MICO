/**
******************************************************************************
* @file    button.h 
* @author  Eshen Wang
* @version V1.0.0
* @date    1-May-2015
* @brief   user key operation. 
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

#ifndef __BUTTON_H_
#define __BUTTON_H_

#include "platform.h"
#include "platform_peripheral.h"

//--------------------------------  pin defines --------------------------------
typedef enum _button_index_e{
	IOBUTTON_EASYLINK = 0,
	IOBUTTON_USER_1,
	IOBUTTON_USER_2,
	IOBUTTON_USER_3,
	IOBUTTON_USER_4,
} button_index_e;

typedef void (*button_pressed_cb)(void) ;
typedef void (*button_long_pressed_cb)(void) ;

typedef struct _button_init_t{
	mico_gpio_t gpio;
	int long_pressed_timeout;
	button_pressed_cb pressed_func;
	button_long_pressed_cb long_pressed_func;
} button_init_t;

//------------------------------ user interfaces -------------------------------
void button_init( int index, button_init_t init );


#endif  // __BUTTON_H_
