/**
 * oleddisp.c
 */

#include <math.h>
#include <string.h>
#include <stdlib.h> 

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lrotable.h"
   
#include "mico_platform.h"
#include "user_config.h"
#include "MICO.h"
#include "StringUtils.h"

#include <spiffs.h>
#include <spiffs_nucleus.h>

#define LCD_SOFT_RESET // if not using RST pin
#define NUM_GPIO        18

#define INITR_GREENTAB 0x0
#define INITR_REDTAB   0x1
#define INITR_BLACKTAB   0x2

#define ST7735_TFTWIDTH  128
#define ST7735_TFTHEIGHT 160

#define ST7735_NOP     0x00
#define ST7735_SWRESET 0x01
#define ST7735_RDDID   0x04
#define ST7735_RDDST   0x09

#define ST7735_SLPIN   0x10
#define ST7735_SLPOUT  0x11
#define ST7735_PTLON   0x12
#define ST7735_NORON   0x13

#define ST7735_INVOFF  0x20
#define ST7735_INVON   0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_RAMRD   0x2E

#define ST7735_PTLAR   0x30
#define ST7735_COLMOD  0x3A
#define ST7735_MADCTL  0x36

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR  0xB4
#define ST7735_DISSET5 0xB6

#define ST7735_PWCTR1  0xC0
#define ST7735_PWCTR2  0xC1
#define ST7735_PWCTR3  0xC2
#define ST7735_PWCTR4  0xC3
#define ST7735_PWCTR5  0xC4
#define ST7735_VMCTR1  0xC5

#define ST7735_RDID1   0xDA
#define ST7735_RDID2   0xDB
#define ST7735_RDID3   0xDC
#define ST7735_RDID4   0xDD

#define ST7735_PWCTR6  0xFC

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

// Color definitions
#define TFT_BLACK       0x0000      /*   0,   0,   0 */
#define TFT_NAVY        0x000F      /*   0,   0, 128 */
#define TFT_DARKGREEN   0x03E0      /*   0, 128,   0 */
#define TFT_DARKCYAN    0x03EF      /*   0, 128, 128 */
#define TFT_MAROON      0x7800      /* 128,   0,   0 */
#define TFT_PURPLE      0x780F      /* 128,   0, 128 */
#define TFT_OLIVE       0x7BE0      /* 128, 128,   0 */
#define TFT_LIGHTGREY   0xC618      /* 192, 192, 192 */
#define TFT_DARKGREY    0x7BEF      /* 128, 128, 128 */
#define TFT_BLUE        0x001F      /*   0,   0, 255 */
#define TFT_GREEN       0x07E0      /*   0, 255,   0 */
#define TFT_CYAN        0x07FF      /*   0, 255, 255 */
#define TFT_RED         0xF800      /* 255,   0,   0 */
#define TFT_MAGENTA     0xF81F      /* 255,   0, 255 */
#define TFT_YELLOW      0xFFE0      /* 255, 255,   0 */
#define TFT_WHITE       0xFFFF      /* 255, 255, 255 */
#define TFT_ORANGE      0xFD20      /* 255, 165,   0 */
#define TFT_GREENYELLOW 0xAFE5      /* 173, 255,  47 */
#define TFT_PINK        0xF81F

#define INVERT_ON		1
#define INVERT_OFF		0

#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_ML  0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08
#define MADCTL_MH  0x04

#define PORTRAIT	0
#define LANDSCAPE	1
#define PORTRAIT_FLIP	2
#define LANDSCAPE_FLIP	3

#define CENTER	-1
#define RIGHT	-2

#define SMALL_FONT  0
#define BIG_FONT    1
#define FONT_7SEG   2
#define FONT_8x8    3

#define bitmapdatatype uint16_t *

#define FILE_NOT_OPENED 0
   
extern const char wifimcu_gpio_map[];
extern uint8_t spiInit[];
extern uint16_t _spi_write(uint8_t id, uint8_t databits,uint8_t* data, uint16_t count, uint16_t rep);
extern uint8_t SmallFont[];         
extern uint8_t Font8x8[];
extern unsigned char BigFont[];
extern spiffs fs;
extern volatile int file_fd;
int mode2flag(char *mode);

uint8_t TFT_SPI_ID = 255;
uint8_t TFT_pinDC  = 255;

static int platform_gpio_exists( unsigned pin )
{
  return pin < NUM_GPIO;
}


static uint16_t _width = ST7735_TFTWIDTH;
static uint16_t _height = ST7735_TFTHEIGHT;

#ifndef LCD_SOFT_RESET
  uint8_t lcd_pinRST  = 255;
  #define LCD_RST1  MicoGpioOutputHigh( (mico_gpio_t)lcd_pinRST )
  #define LCD_RST0  MicoGpioOutputLow( (mico_gpio_t)lcd_pinRST );
#endif
  
#define LCD_DC1   MicoGpioOutputHigh( (mico_gpio_t)TFT_pinDC )
#define LCD_DC0   MicoGpioOutputLow( (mico_gpio_t)TFT_pinDC );

static int colstart = 0;
static int rowstart = 0; // May be overridden in init func
//static uint8_t tabcolor	= 0;
static uint8_t orientation = PORTRAIT;
static int rotation = 0;

typedef struct _font {
	uint8_t 	*font;
	uint8_t 	x_size;
	uint8_t 	y_size;
	uint8_t	        offset;
	uint16_t	numchars;
        uint8_t         bitmap;
	uint16_t        color;
} Font;

static Font cfont;
static uint8_t _transparent = 0;
static uint16_t _fg = TFT_GREEN;
static uint16_t _bg = TFT_BLACK;
static uint8_t ccbuf[32];

#define swap(a, b) { int16_t t = a; a = b; b = t; }


// Send control command to controller
//---------------------------------
static void ST7735_sendCmd(uint8_t cmd) {
  ccbuf[0] = cmd;
  LCD_DC0;
  _spi_write(TFT_SPI_ID, 8, &ccbuf[0], 1, 1);
}

// Send parameters of the command to controller
//-----------------------------------
static void ST7735_sendData(uint8_t data) {
  ccbuf[0] = data;
  LCD_DC1;
  _spi_write(TFT_SPI_ID, 8, &ccbuf[0], 1, 1);
}

//--------------------------------------------------------------------------
static void ST7735_setAddrWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
  
  ccbuf[0] = 0x00;
  ccbuf[1] = x0+colstart;          // XSTART 
  ccbuf[2] = 0x00;
  ccbuf[3] = x1+colstart;          // XEND
  ST7735_sendCmd(ST7735_CASET);  // Column addr set
  LCD_DC1;
  _spi_write(TFT_SPI_ID, 8, &ccbuf[0], 4, 1);

  ccbuf[0] = 0x00;
  ccbuf[1] = y0+rowstart;          // YSTART
  ccbuf[2] = 0x00;
  ccbuf[3] = y1+rowstart;          // YEND
  ST7735_sendCmd(ST7735_RASET);  // Row addr set
  LCD_DC1;
  _spi_write(TFT_SPI_ID, 8, &ccbuf[0], 4, 1);
  ST7735_sendCmd(ST7735_RAMWR);  // write to RAM
}
/*
//-----------------------
static void ST7735_putpix(uint16_t c) {
  ccbuf[0] = (uint8_t)(c >> 8);
  ccbuf[1] = (uint8_t)(c & 0xFF);
  _spi_write(TFT_SPI_ID, 8, &ccbuf[0], 2, 1);
}
*/
//----------------------------------------
static void ST7735_putpixels(uint16_t c, uint16_t cnt) {
  ccbuf[0] = (uint8_t)(c >> 8);
  ccbuf[1] = (uint8_t)(c & 0xFF);
  LCD_DC1;  
  _spi_write(TFT_SPI_ID, 8, &ccbuf[0], 2, cnt);
}

//--------------------------------------
static void ST7735_pushColor(uint16_t color) {
  ccbuf[0] = (uint8_t)(color >> 8);
  ccbuf[1] = (uint8_t)(color & 0xFF);
  LCD_DC1;  
  _spi_write(TFT_SPI_ID, 8, &ccbuf[0], 2, 1);
}


// === ST7735 INITIALIZATION ===================================================
// Rather than a bazillion ST7735_sendCmd() and ST7735_sendData() calls, screen
// initialization commands and arguments are organized in these tables
// stored in PROGMEM.  The table may look bulky, but that's mostly the
// formatting -- storage-wise this is hundreds of bytes more compact
// than the equivalent code.  Companion function follows.
#define DELAY 0x80

// Initialization commands for 7735B screens
static const uint8_t Bcmd[] = {
  18,				// 18 commands in list:
  ST7735_SWRESET,   DELAY,	//  1: Software reset, no args, w/delay
  50,				//     50 ms delay
  ST7735_SLPOUT ,   DELAY,	//  2: Out of sleep mode, no args, w/delay
  255,				//     255 = 500 ms delay
  ST7735_COLMOD , 1+DELAY,	//  3: Set color mode, 1 arg + delay:
  0x05,				//     16-bit color 5-6-5 color format
  10,				//     10 ms delay
  ST7735_FRMCTR1, 3+DELAY,	//  4: Frame rate control, 3 args + delay:
  0x00,				//     fastest refresh
  0x06,				//     6 lines front porch
  0x03,				//     3 lines back porch
  10,				//     10 ms delay
  ST7735_MADCTL , 1      ,	//  5: Memory access ctrl (directions), 1 arg:
  0x08,				//     Row addr/col addr, bottom to top refresh
  ST7735_DISSET5, 2      ,	//  6: Display settings #5, 2 args, no delay:
  0x15,				//     1 clk cycle nonoverlap, 2 cycle gate
  //     rise, 3 cycle osc equalize
  0x02,				//     Fix on VTL
  ST7735_INVCTR , 1      ,	//  7: Display inversion control, 1 arg:
  0x0,				//     Line inversion
  ST7735_PWCTR1 , 2+DELAY,	//  8: Power control, 2 args + delay:
  0x02,				//     GVDD = 4.7V
  0x70,				//     1.0uA
  10,				//     10 ms delay
  ST7735_PWCTR2 , 1      ,	//  9: Power control, 1 arg, no delay:
  0x05,				//     VGH = 14.7V, VGL = -7.35V
  ST7735_PWCTR3 , 2      ,	// 10: Power control, 2 args, no delay:
  0x01,				//     Opamp current small
  0x02,				//     Boost frequency
  ST7735_VMCTR1 , 2+DELAY,	// 11: Power control, 2 args + delay:
  0x3C,				//     VCOMH = 4V
  0x38,				//     VCOML = -1.1V
  10,				//     10 ms delay
  ST7735_PWCTR6 , 2      ,	// 12: Power control, 2 args, no delay:
  0x11, 0x15,
  ST7735_GMCTRP1,16      ,	// 13: Magical unicorn dust, 16 args, no delay:
  0x09, 0x16, 0x09, 0x20,	//     (seriously though, not sure what
  0x21, 0x1B, 0x13, 0x19,	//      these config values represent)
  0x17, 0x15, 0x1E, 0x2B,
  0x04, 0x05, 0x02, 0x0E,
  ST7735_GMCTRN1,16+DELAY,	// 14: Sparkles and rainbows, 16 args + delay:
  0x0B, 0x14, 0x08, 0x1E,	//     (ditto)
  0x22, 0x1D, 0x18, 0x1E,
  0x1B, 0x1A, 0x24, 0x2B,
  0x06, 0x06, 0x02, 0x0F,
  10,				//     10 ms delay
  ST7735_CASET  , 4      ,	// 15: Column addr set, 4 args, no delay:
  0x00, 0x02,			//     XSTART = 2
  0x00, 0x81,			//     XEND = 129
  ST7735_RASET  , 4      ,	// 16: Row addr set, 4 args, no delay:
  0x00, 0x02,			//     XSTART = 1
  0x00, 0x81,			//     XEND = 160
  ST7735_NORON  ,   DELAY,	// 17: Normal display on, no args, w/delay
  10,				//     10 ms delay
  ST7735_DISPON ,   DELAY,	// 18: Main screen turn on, no args, w/delay
  255				//     255 = 500 ms delay
};

// Init for 7735R, part 1 (red or green tab)
static const uint8_t  Rcmd1[] = {                 
  15,				// 15 commands in list:
  ST7735_SWRESET,   DELAY,	//  1: Software reset, 0 args, w/delay
  150,				//     150 ms delay
  ST7735_SLPOUT ,   DELAY,	//  2: Out of sleep mode, 0 args, w/delay
  255,				//     500 ms delay
  ST7735_FRMCTR1, 3      ,	//  3: Frame rate ctrl - normal mode, 3 args:
  0x01, 0x2C, 0x2D,		//     Rate = fosc/(1x2+40) * (LINE+2C+2D)
  ST7735_FRMCTR2, 3      ,	//  4: Frame rate control - idle mode, 3 args:
  0x01, 0x2C, 0x2D,		//     Rate = fosc/(1x2+40) * (LINE+2C+2D)
  ST7735_FRMCTR3, 6      ,	//  5: Frame rate ctrl - partial mode, 6 args:
  0x01, 0x2C, 0x2D,		//     Dot inversion mode
  0x01, 0x2C, 0x2D,		//     Line inversion mode
  ST7735_INVCTR , 1      ,	//  6: Display inversion ctrl, 1 arg, no delay:
  0x07,				//     No inversion
  ST7735_PWCTR1 , 3      ,	//  7: Power control, 3 args, no delay:
  0xA2,
  0x02,				//     -4.6V
  0x84,				//     AUTO mode
  ST7735_PWCTR2 , 1      ,	//  8: Power control, 1 arg, no delay:
  0xC5,				//     VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
  ST7735_PWCTR3 , 2      ,	//  9: Power control, 2 args, no delay:
  0x0A,				//     Opamp current small
  0x00,				//     Boost frequency
  ST7735_PWCTR4 , 2      ,	// 10: Power control, 2 args, no delay:
  0x8A,				//     BCLK/2, Opamp current small & Medium low
  0x2A,  
  ST7735_PWCTR5 , 2      ,	// 11: Power control, 2 args, no delay:
  0x8A, 0xEE,
  ST7735_VMCTR1 , 1      ,	// 12: Power control, 1 arg, no delay:
  0x0E,
  ST7735_INVOFF , 0      ,	// 13: Don't invert display, no args, no delay
  ST7735_MADCTL , 1      ,	// 14: Memory access control (directions), 1 arg:
  0xC0,				//     row addr/col addr, bottom to top refresh, RGB order
  ST7735_COLMOD , 1+DELAY,	//  15: Set color mode, 1 arg + delay:
  0x05,				//     16-bit color 5-6-5 color format
  10				//     10 ms delay
};

// Init for 7735R, part 2 (green tab only)
static const uint8_t Rcmd2green[] = {
  2,				//  2 commands in list:
  ST7735_CASET  , 4      ,	//  1: Column addr set, 4 args, no delay:
  0x00, 0x02,			//     XSTART = 0
  0x00, 0x7F+0x02,		//     XEND = 129
  ST7735_RASET  , 4      ,	//  2: Row addr set, 4 args, no delay:
  0x00, 0x01,			//     XSTART = 0
  0x00, 0x9F+0x01		//     XEND = 160
};

// Init for 7735R, part 2 (red tab only)
static const uint8_t Rcmd2red[] = {
  2,				//  2 commands in list:
  ST7735_CASET  , 4      ,	//  1: Column addr set, 4 args, no delay:
  0x00, 0x00,			//     XSTART = 0
  0x00, 0x7F,			//     XEND = 127
  ST7735_RASET  , 4      ,	//  2: Row addr set, 4 args, no delay:
  0x00, 0x00,			//     XSTART = 0
  0x00, 0x9F			//     XEND = 159
};

// Init for 7735R, part 3 (red or green tab)
static const uint8_t Rcmd3[] = {
  4,				//  4 commands in list:
  ST7735_GMCTRP1, 16      ,	//  1: Magical unicorn dust, 16 args, no delay:
  0x02, 0x1c, 0x07, 0x12,
  0x37, 0x32, 0x29, 0x2d,
  0x29, 0x25, 0x2B, 0x39,
  0x00, 0x01, 0x03, 0x10,
  ST7735_GMCTRN1, 16      ,	//  2: Sparkles and rainbows, 16 args, no delay:
  0x03, 0x1d, 0x07, 0x06,
  0x2E, 0x2C, 0x29, 0x2D,
  0x2E, 0x2E, 0x37, 0x3F,
  0x00, 0x00, 0x02, 0x10,
  ST7735_NORON  ,    DELAY,	//  3: Normal display on, no args, w/delay
  10,				//     10 ms delay
  ST7735_DISPON ,    DELAY,	//  4: Main screen turn on, no args w/delay
  100				//     100 ms delay
};


// Companion code to the above tables.  Reads and issues
// a series of LCD commands stored in PROGMEM byte array.
static void commandList(const uint8_t *addr) {
  uint8_t  numCommands, numArgs;
  uint16_t ms;

  numCommands = *addr++;         // Number of commands to follow
  while(numCommands--) {         // For each command...
    ST7735_sendCmd(*addr++);     //   Read, issue command
    numArgs  = *addr++;          //   Number of args to follow
    ms       = numArgs & DELAY;  //   If hibit set, delay follows args
    numArgs &= ~DELAY;           //   Mask out delay bit
    while(numArgs--) {           //   For each argument...
      ST7735_sendData(*addr++);  //     Read, issue argument
    }

    if(ms) {
      ms = *addr++;              // Read post-command delay time (ms)
      if(ms == 255) ms = 500;    // If 255, delay for 500 ms
      mico_thread_msleep(ms);
    }
  }
}

// Initialization code common to both 'B' and 'R' type displays
static void ST7735_commonInit(const uint8_t *cmdList) {
	// toggle RST low to reset; CS low so it'll listen to us
#ifdef LCD_SOFT_RESET
  ST7735_sendCmd(ST7735_SWRESET);
  mico_thread_msleep(125);
#else
  LCD_RST1;
  mico_thread_msleep(10);
  LCD_RST0;
  mico_thread_msleep(50);
  LCD_RST1;
  mico_thread_msleep(125);
#endif    
  if(cmdList) commandList(cmdList);
}

// Initialization for ST7735B screens
static void ST7735_initB(void) {
  ST7735_commonInit(Bcmd);
}

// Initialization for ST7735R screens (green or red tabs)
static void ST7735_initR(uint8_t options) {
  mico_thread_msleep(50);
  ST7735_commonInit(Rcmd1);
  if(options == INITR_GREENTAB) {
    commandList(Rcmd2green);
    colstart = 2;
    rowstart = 1;
  } else {
    // colstart, rowstart left at default '0' values
    commandList(Rcmd2red);
  }
  commandList(Rcmd3);

  // if black, change MADCTL color filter
  if (options == INITR_BLACKTAB) {
    ST7735_sendCmd(ST7735_MADCTL);
    ST7735_sendData(0xC0);
  }

  //  tabcolor = options;
}


// draw color pixel on screen
static void TFT_drawPixel(int16_t x, int16_t y, uint16_t color) {

	if((x < 0) ||(x >= _width) || (y < 0) || (y >= _height)) return;

	ST7735_setAddrWindow(x,y,x+1,y+1);
	ST7735_pushColor(color);
}

// fill a rectangle
static void TFT_fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {	
	// rudimentary clipping (drawChar w/big text requires this)
	if((x >= _width) || (y >= _height)) return;
	if((x + w - 1) >= _width)  w = _width  - x;
	if((y + h - 1) >= _height) h = _height - y;

	ST7735_setAddrWindow(x, y, x+w-1, y+h-1);
        ST7735_putpixels(color, h*w);
}

static void TFT_drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
	// Rudimentary clipping
	if((x >= _width) || (y >= _height)) return;
	if((y+h-1) >= _height) h = _height-y;
	ST7735_setAddrWindow(x, y, x, y+h-1);
        ST7735_putpixels(color, h);
}

static void TFT_drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
	// Rudimentary clipping
	if((x >= _width) || (y >= _height)) return;
	if((x+w-1) >= _width)  w = _width-x;
	ST7735_setAddrWindow(x, y, x+w-1, y);
        ST7735_putpixels(color, w);
}

// Bresenham's algorithm - thx wikipedia - speed enhanced by Bodmer this uses
// the eficient FastH/V Line draw routine for segments of 2 pixels or more
static void TFT_drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
  boolean steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    swap(x0, y0);
    swap(x1, y1);
  }
  if (x0 > x1) {
    swap(x0, x1);
    swap(y0, y1);
  }

  int16_t dx = x1 - x0, dy = abs(y1 - y0);;
  int16_t err = dx >> 1, ystep = -1, xs = x0, dlen = 0;

  if (y0 < y1) ystep = 1;

  // Split into steep and not steep for FastH/V separation
  if (steep) {
    for (; x0 <= x1; x0++) {
      dlen++;
      err -= dy;
      if (err < 0) {
        err += dx;
        if (dlen == 1) TFT_drawPixel(y0, xs, color);
        else TFT_drawFastVLine(y0, xs, dlen, color);
        dlen = 0; y0 += ystep; xs = x0 + 1;
      }
    }
    if (dlen) TFT_drawFastVLine(y0, xs, dlen, color);
  }
  else
  {
    for (; x0 <= x1; x0++) {
      dlen++;
      err -= dy;
      if (err < 0) {
        err += dx;
        if (dlen == 1) TFT_drawPixel(xs, y0, color);
        else TFT_drawFastHLine(xs, y0, dlen, color);
        dlen = 0; y0 += ystep; xs = x0 + 1;
      }
    }
    if (dlen) TFT_drawFastHLine(xs, y0, dlen, color);
  }
}



void TFT_drawFastLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint16_t color) {
	signed char   dx, dy, sx, sy;
	unsigned char  x,  y, mdx, mdy, l;

	if (x1==x2) {
		TFT_fillRect(x1,y1, x1,y2, color);
		return;
	}

	if (y1==y2) {
		TFT_fillRect(x1,y1, x2,y1, color);
		return;
	}

	dx=x2-x1; dy=y2-y1;

	if (dx>=0) { mdx=dx; sx=1; } else { mdx=x1-x2; sx=-1; }
	if (dy>=0) { mdy=dy; sy=1; } else { mdy=y1-y2; sy=-1; }

	x=x1; y=y1;

	if (mdx>=mdy) {
		l=mdx;
		while (l>0) {
			if (dy>0) { y=y1+mdy*(x-x1)/mdx; }
			else { y=y1-mdy*(x-x1)/mdx; }
			TFT_drawPixel(x,y,color);
			x=x+sx;
			l--;
		}
	} else {
		l=mdy;
		while (l>0) {
			if (dy>0) { x=x1+((mdx*(y-y1))/mdy); }
			else { x=x1+((mdx*(y1-y))/mdy); }
			TFT_drawPixel(x,y,color);
			y=y+sy;
			l--;
		}
	}
	TFT_drawPixel(x2, y2, color);
}
/*
static void TFT_drawRect(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2, uint16_t color) {
	TFT_drawFastHLine(x1,y1,x2-x1, color);
	TFT_drawFastVLine(x2,y1,y2-y1, color);
	TFT_drawFastHLine(x1,y2,x2-x1, color);
	TFT_drawFastVLine(x1,y1,y2-y1, color);
}
*/
static void TFT_drawRect(uint8_t x1,uint8_t y1,uint8_t w,uint8_t h, uint16_t color) {
	TFT_drawFastHLine(x1,y1,w, color);
	TFT_drawFastVLine(x1+w-1,y1,h, color);
	TFT_drawFastHLine(x1,y1+h-1,w, color);
	TFT_drawFastVLine(x1,y1,h, color);
}

// Draw a triangle
static void TFT_drawTriangle(uint8_t x0, uint8_t y0,
				uint8_t x1, uint8_t y1,
				uint8_t x2, uint8_t y2, uint16_t color) {
  TFT_drawLine(x0, y0, x1, y1, color);
  TFT_drawLine(x1, y1, x2, y2, color);
  TFT_drawLine(x2, y2, x0, y0, color);
}

// Fill a triangle
static void TFT_fillTriangle(uint8_t x0, uint8_t y0,
				uint8_t x1, uint8_t y1,
				uint8_t x2, uint8_t y2, uint16_t color)
{
  int16_t a, b, y, last;

  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1) {
    swap(y0, y1); swap(x0, x1);
  }
  if (y1 > y2) {
    swap(y2, y1); swap(x2, x1);
  }
  if (y0 > y1) {
    swap(y0, y1); swap(x0, x1);
  }

  if(y0 == y2) { // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if(x1 < a)      a = x1;
    else if(x1 > b) b = x1;
    if(x2 < a)      a = x2;
    else if(x2 > b) b = x2;
    TFT_drawFastHLine(a, y0, b-a+1, color);
    return;
  }

  int16_t
    dx01 = x1 - x0,
    dy01 = y1 - y0,
    dx02 = x2 - x0,
    dy02 = y2 - y0,
    dx12 = x2 - x1,
    dy12 = y2 - y1;
  int32_t
    sa   = 0,
    sb   = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if(y1 == y2) last = y1;   // Include y1 scanline
  else         last = y1-1; // Skip it

  for(y=y0; y<=last; y++) {
    a   = x0 + sa / dy01;
    b   = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;
    /* longhand:
    a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) swap(a,b);
    TFT_drawFastHLine(a, y, b-a+1, color);
  }
  
  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);
  for(; y<=y2; y++) {
    a   = x1 + sa / dy12;
    b   = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;
    /* longhand:
    a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) swap(a,b);
    TFT_drawFastHLine(a, y, b-a+1, color);
  }
}

static void TFT_drawCircle(int16_t x, int16_t y, int radius, uint16_t color) {
	int f = 1 - radius;
	int ddF_x = 1;
	int ddF_y = -2 * radius;
	int x1 = 0;
	int y1 = radius;

	ST7735_setAddrWindow(x, y + radius, x, y + radius);
	ST7735_pushColor(color);
	ST7735_setAddrWindow(x, y - radius, x, y - radius);
	ST7735_pushColor(color);
	ST7735_setAddrWindow(x + radius, y, x + radius, y);
	ST7735_pushColor(color);
	ST7735_setAddrWindow(x - radius, y, x - radius, y);
	ST7735_pushColor(color);
	while(x1 < y1) {
		if(f >= 0) 
		{
			y1--;
			ddF_y += 2;
			f += ddF_y;
		}
		x1++;
		ddF_x += 2;
		f += ddF_x;    
		ST7735_setAddrWindow(x + x1, y + y1, x + x1, y + y1);
		ST7735_pushColor(color);
		ST7735_setAddrWindow(x - x1, y + y1, x - x1, y + y1);
		ST7735_pushColor(color);
		ST7735_setAddrWindow(x + x1, y - y1, x + x1, y - y1);
		ST7735_pushColor(color);
		ST7735_setAddrWindow(x - x1, y - y1, x - x1, y - y1);
		ST7735_pushColor(color);
		ST7735_setAddrWindow(x + y1, y + x1, x + y1, y + x1);
		ST7735_pushColor(color);
		ST7735_setAddrWindow(x - y1, y + x1, x - y1, y + x1);
		ST7735_pushColor(color);
		ST7735_setAddrWindow(x + y1, y - x1, x + y1, y - x1);
		ST7735_pushColor(color);
		ST7735_setAddrWindow(x - y1, y - x1, x - y1, y - x1);
		ST7735_pushColor(color);
	}
}

static void TFT_fillCircle(int16_t x, int16_t y, int radius, uint16_t color) {
	int x1,y1;
	for(y1=-radius; y1<=0; y1++) 
		for(x1=-radius; x1<=0; x1++)
			if(x1*x1+y1*y1 <= radius*radius) {
				TFT_drawFastHLine(x+x1, y+y1, 2*(-x1), color);
				TFT_drawFastHLine(x+x1, y-y1, 2*(-x1), color);
				break;
			}
}

/*
static void TFT_drawBitmap(int x, int y, int sx, int sy, bitmapdatatype data, int scale) {
	int tx, ty, tc, tsx, tsy;

	if (scale==1) {
		if (orientation == PORTRAIT || orientation == PORTRAIT_FLIP)
		{
			ST7735_setAddrWindow(x, y, x+sx-1, y+sy-1);
			LCD_DC1;
			for (tc=0; tc<(sx*sy); tc++)
				ST7735_putpix(data[tc]);
		} else {
			for (ty=0; ty<sy; ty++) {
				ST7735_setAddrWindow(x, y+ty, x+sx-1, y+ty);
				LCD_DC1;
				for (tx=sx-1; tx>=0; tx--)
					ST7735_putpix(data[(ty*sx)+tx]);
			}
		}
	} else {
		if (orientation == PORTRAIT || orientation == PORTRAIT_FLIP) {
			for (ty=0; ty<sy; ty++) {
				ST7735_setAddrWindow(x, y+(ty*scale), x+((sx*scale)-1), y+(ty*scale)+scale);
				for (tsy=0; tsy<scale; tsy++)
					for (tx=0; tx<sx; tx++) {
						for (tsx=0; tsx<scale; tsx++)
							ST7735_pushColor(data[(ty*sx)+tx]);
					}
			}
		} else {
			for (ty=0; ty<sy; ty++) {
				for (tsy=0; tsy<scale; tsy++) {
					ST7735_setAddrWindow(x, y+(ty*scale)+tsy, x+((sx*scale)-1), y+(ty*scale)+tsy);
					for (tx=sx-1; tx>=0; tx--) {
						for (tsx=0; tsx<scale; tsx++)
							ST7735_pushColor(data[(ty*sx)+tx]);
					}
				}
			}
		}
	}
}

static void TFT_drawBitmapRotate(int x, int y, int sx, int sy, bitmapdatatype data, int deg, int rox, int roy) {
	int tx, ty, newx, newy;
	double radian;
	radian=deg*0.0175;  

	if (deg==0)
		TFT_drawBitmap(x, y, sx, sy, data, 1);
	else
	{
		for (ty=0; ty<sy; ty++)
			for (tx=0; tx<sx; tx++) {
				newx=(int)(x+rox+(((tx-rox)*cos(radian))-((ty-roy)*sin(radian))));
				newy=(int)(y+roy+(((ty-roy)*cos(radian))+((tx-rox)*sin(radian))));

				ST7735_setAddrWindow(newx, newy, newx, newy);
				ST7735_pushColor(data[(ty*sx)+tx]);
			}
	}
}
*/

//==============================================================================
/**
 * bit-encoded bar position of all digits' bcd segments
 *
 *                   6
 * 		  +-----+
 * 		3 |  .	| 2
 * 		  +--5--+
 * 		1 |  .	| 0
 * 		  +--.--+
 * 		     4
 */
static const uint16_t font_bcd[] = {
  0x200, // 0010 0000 0000  // -
  0x080, // 0000 1000 0000  // . 
  0x06C, // 0100 0110 1100  // /, degree
  0x05f, // 0000 0101 1111, // 0 
  0x005, // 0000 0000 0101, // 1 
  0x076, // 0000 0111 0110, // 2 
  0x075, // 0000 0111 0101, // 3 
  0x02d, // 0000 0010 1101, // 4 
  0x079, // 0000 0111 1001, // 5 
  0x07b, // 0000 0111 1011, // 6 
  0x045, // 0000 0100 0101, // 7 
  0x07f, // 0000 0111 1111, // 8 
  0x07d, // 0000 0111 1101  // 9 
  0x900  // 1001 0000 0000  // :
};

//-------------------------------------------------------------------------------
static void barVert(int16_t x, int16_t y, int16_t w, int16_t l, uint16_t color) {
  TFT_fillTriangle(x+1, y+2*w, x+w, y+w+1, x+2*w-1, y+2*w, color);
  TFT_fillTriangle(x+1, y+2*w+l+1, x+w, y+3*w+l, x+2*w-1, y+2*w+l+1, color);
  TFT_fillRect(x, y+2*w+1, 2*w+1, l, color);
  if (cfont.offset) {
    TFT_drawTriangle(x+1, y+2*w, x+w, y+w+1, x+2*w-1, y+2*w, cfont.color);
    TFT_drawTriangle(x+1, y+2*w+l+1, x+w, y+3*w+l, x+2*w-1, y+2*w+l+1, cfont.color);
    TFT_drawRect(x, y+2*w+1, 2*w+1, l, cfont.color);
  }
}

//------------------------------------------------------------------------------
static void barHor(int16_t x, int16_t y, int16_t w, int16_t l, uint16_t color) {
  TFT_fillTriangle(x+2*w, y+2*w-1, x+w+1, y+w, x+2*w, y+1, color);
  TFT_fillTriangle(x+2*w+l+1, y+2*w-1, x+3*w+l, y+w, x+2*w+l+1, y+1, color);
  TFT_fillRect(x+2*w+1, y, l, 2*w+1, color);
  if (cfont.offset) {
    TFT_drawTriangle(x+2*w, y+2*w-1, x+w+1, y+w, x+2*w, y+1, cfont.color);
    TFT_drawTriangle(x+2*w+l+1, y+2*w-1, x+3*w+l, y+w, x+2*w+l+1, y+1, cfont.color);
    TFT_drawRect(x+2*w+1, y, l, 2*w+1, cfont.color);
  }
}

//------------------------------------------------------------------------------------------------
static void TFT_draw7seg(int16_t x, int16_t y, int8_t num, int16_t w, int16_t l, uint16_t color) {
  // @todo clipping
  if (num < 0x2D || num > 0x3A) return;
  
  int16_t c = font_bcd[num-0x2D];
  int16_t d = 2*w+l+1;
  
  if (!_transparent) TFT_fillRect(x, y, (2 * (2 * w + 1)) + l, (3 * (2 * w + 1)) + (2 * l), _bg);

  if (c & 0x001) barVert(x+d, y+d, w, l, color);               // down right
  if (c & 0x002) barVert(x,   y+d, w, l, color);               // down left
  if (c & 0x004) barVert(x+d, y, w, l, color);                 // up right
  if (c & 0x008) barVert(x,   y, w, l, color);                 // up left
  
  if (c & 0x010) barHor(x, y+2*d, w, l, color);                // down
  if (c & 0x020) barHor(x, y+d, w, l, color);                  // middle
  if (c & 0x040) barHor(x, y, w, l, color);                    // up

  if (c & 0x080) {
    TFT_fillRect(x+(d/2), y+2*d, 2*w+1, 2*w+1, color);         // low point
    if (cfont.offset) TFT_drawRect(x+(d/2), y+2*d, 2*w+1, 2*w+1, cfont.color);
  }
  if (c & 0x100) {
    TFT_fillRect(x+(d/2), y+d+2*w+1, 2*w+1, l/2, color);       // down middle point
    if (cfont.offset) TFT_drawRect(x+(d/2), y+d+2*w+1, 2*w+1, l/2, cfont.color);
  }
  if (c & 0x800) {
    TFT_fillRect(x+(d/2), y+(2*w)+1+(l/2), 2*w+1, l/2, color); // up middle point
    if (cfont.offset) TFT_drawRect(x+(d/2), y+(2*w)+1+(l/2), 2*w+1, l/2, cfont.color);
  }
  if (c & 0x200) {
    TFT_fillRect(x+2*w+1, y+d, l, 2*w+1, color);               // middle, minus
    if (cfont.offset) TFT_drawRect(x+2*w+1, y+d, l, 2*w+1, cfont.color);
  }
}
//==============================================================================


//-------------------------------------
static void TFT_setFont(uint8_t font) {
  if (font == SMALL_FONT) cfont.font=&SmallFont[0];
  else if (font == BIG_FONT) cfont.font=&BigFont[0];
  else if (font == 3) cfont.font=&Font8x8[0];
  else {
    cfont.font = NULL;
    if (font == FONT_7SEG) {
      cfont.bitmap = 2;
      cfont.x_size = 24;
      cfont.y_size = 6;
      cfont.offset = 0;
      cfont.color  = _fg;
    }
    else  cfont.bitmap = 0;
  }
  if (cfont.font != NULL) {
    cfont.bitmap = 1;
    cfont.x_size=cfont.font[0];
    cfont.y_size=cfont.font[1];
    cfont.offset=cfont.font[2];
    cfont.numchars=cfont.font[3];
  }
}

//----------------------------------------------
static void printChar(uint8_t c, int x, int y) {
  uint8_t i,ch,fz;
  uint16_t j;
  uint16_t temp; 
  int zz;

  if (cfont.bitmap != 1) return;
  if ((x+cfont.x_size-1) >= _width) return;
  
  if( cfont.x_size < 8 ) 
    fz = cfont.x_size;
  else
    fz = cfont.x_size/8;
  if (!_transparent) {
    ST7735_setAddrWindow(x,y,x+cfont.x_size-1,y+cfont.y_size-1);

    temp=((c-cfont.offset)*((fz)*cfont.y_size))+4;
    for(j=0;j<((fz)*cfont.y_size);j++) {
      ch = cfont.font[temp];
      for(i=0;i<8;i++) {   
        if((ch&(1<<(7-i)))!=0)   
        {
          ST7735_pushColor(_fg);
        } 
        else
        {
          ST7735_pushColor(_bg);
        }   
      }
      temp++;
    }
  } else {
    temp=((c-cfont.offset)*((fz)*cfont.y_size))+4;
    for(j=0;j<cfont.y_size;j++) 
    {
      for (zz=0; zz<(fz); zz++)
      {
        ch = cfont.font[temp+zz]; 
        for(i=0;i<8;i++)
        {   
          ST7735_setAddrWindow(x+i+(zz*8),y+j,x+i+(zz*8)+1,y+j+1);

          if((ch&(1<<(7-i)))!=0)   
          {
            ST7735_pushColor(_fg);
          } 
        }
      }
      temp+=(fz);
    }
  }
}

//--------------------------------------------------------
static void rotateChar(uint8_t c, int x, int y, int pos) {
  uint8_t i,j,ch,fz;
  uint16_t temp; 
  int newx,newy;
  double radian = rotation*0.0175;
  int zz;

  if (cfont.bitmap != 1) return;

  if( cfont.x_size < 8 ) fz = cfont.x_size;
  else fz = cfont.x_size/8;	
  temp=((c-cfont.offset)*((fz)*cfont.y_size))+4;

  for(j=0;j<cfont.y_size;j++) {
    for (zz=0; zz<(fz); zz++) {
      ch = cfont.font[temp+zz]; 
      for(i=0;i<8;i++) {   
        newx=(int)(x+(((i+(zz*8)+(pos*cfont.x_size))*cos(radian))-((j)*sin(radian))));
        newy=(int)(y+(((j)*cos(radian))+((i+(zz*8)+(pos*cfont.x_size))*sin(radian))));

        if ((newx+1 < _width) && (newy+1 < _height)) {
          ST7735_setAddrWindow(newx,newy,newx+1,newy+1);

          if ((ch&(1<<(7-i)))!=0) {
            ST7735_pushColor(_fg);
          }
          else  {
            if (!_transparent) ST7735_pushColor(_bg);
          }
        }
      }
    }
    temp+=(fz);
  }
}

//---------------------------------------------
static void TFT_print(char *st, int x, int y) {
  int stl, i, w, h;

  if (cfont.bitmap == 0) return;
  
  if (cfont.bitmap == 2) {
    w = (2 * (2 * cfont.y_size + 1)) + cfont.x_size;        // character width
    h = (3 * (2 * cfont.y_size + 1)) + (2 * cfont.x_size);  // character height
    if (x + w >= _width) return;
    if (y + h >= _height) return;
  }
  else {
    if ((x+cfont.x_size-1) >= _width) return;
    if ((y+cfont.y_size-1) >= _height) return;
  }
  
  stl = strlen(st);
  if (x==RIGHT) {
    x=_width-(stl*cfont.x_size);
    if (x < 0) x = 0;
  }
  if (x==CENTER) {
    x=(_width-(stl*cfont.x_size))/2;
    if (x < 0) x = 0;
  }
  for (i=0; i<stl; i++)
    if (cfont.bitmap == 1) {
      if (rotation==0)
        printChar(*st++, x + (i*(cfont.x_size)), y);
      else
        rotateChar(*st++, x, y, i);
    }
    else if (cfont.bitmap == 2) {
      if (x + (i*(w+2)) + w >= _width) break; // no space for the next character
      TFT_draw7seg(x + (i*(w+2)), y, *st++, cfont.y_size, cfont.x_size, _fg);
    }
}


/********************************************************************
*********************** Service functions ***************************
*********************************************************************/

static void TFT_fillScreen(uint16_t color) {
	TFT_fillRect(0, 0,  _width, _height, color);
}

// Change the image rotation.
// Requires 2 bytes of transmission
// Input: m new rotation value (0 to 3)
// Output: none
//-----------------------------------
static void TFT_setRotation(uint8_t m) {
	uint8_t rotation = m % 4; // can't be higher than 3

        orientation = m;
	ST7735_sendCmd(ST7735_MADCTL);
	switch (rotation) {
   case PORTRAIT:
	   ST7735_sendData(MADCTL_MX | MADCTL_MY | MADCTL_RGB);
	   _width  = ST7735_TFTWIDTH;
	   _height = ST7735_TFTHEIGHT;
	   break;
   case LANDSCAPE:
	   ST7735_sendData(MADCTL_MY | MADCTL_MV | MADCTL_RGB);
	   _width  = ST7735_TFTHEIGHT;
	   _height = ST7735_TFTWIDTH;
	   break;
   case PORTRAIT_FLIP:
	   ST7735_sendData(MADCTL_RGB);
	   _width  = ST7735_TFTWIDTH;
	   _height = ST7735_TFTHEIGHT;
	   break;
   case LANDSCAPE_FLIP:
	   ST7735_sendData(MADCTL_MX | MADCTL_MV | MADCTL_RGB);
	   _width  = ST7735_TFTHEIGHT;
	   _height = ST7735_TFTWIDTH;
	   break;
   default:
	   return;
	}
	orientation = m;
}

// Send the command to invert all of the colors.
// Input: i 0 to disable inversion; non-zero to enable inversion
static void TFT_invertDisplay(const uint8_t mode) {
  if ( mode == INVERT_ON ) ST7735_sendCmd(ST7735_INVON);
  else ST7735_sendCmd(ST7735_INVOFF);
}


//--------------------------------------------------
static uint8_t checkParam(uint8_t n, lua_State* L) {
  if (TFT_SPI_ID > 2) return 1;
  if (lua_gettop(L) < n) {
    l_message( NULL, "not enough parameters" );
    return 1;
  }
  return 0;
}

//-------------------------------------------------
static uint16_t getColor(lua_State* L, uint8_t n) {
  if( lua_istable( L, n ) ) {
    uint8_t i;
    uint8_t cl[3];
    uint8_t datalen = lua_objlen( L, n );
    if (datalen < 3) return _fg;
    
    for( i = 0; i < 3; i++ )
    {
      lua_rawgeti( L, n, i + 1 );
      cl[i] = ( int )luaL_checkinteger( L, -1 );
      lua_pop( L, 1 );
    }
    if (cl[0] > 0x1F) cl[0] = 0x1F;
    if (cl[1] > 0x3F) cl[1] = 0x3F;
    if (cl[2] > 0x1F) cl[2] = 0x1F;
    return (cl[0] << 11) | (cl[1] << 5) | cl[2];
  }
  else {
    return luaL_checkinteger( L, n );
  }
}

//=================================
static int lcd_init( lua_State* L )
{
  uint8_t id = luaL_checkinteger( L, 1 );
  if ((id !=0) && (id !=1) && (id !=2)) {
    l_message( NULL, "id should assigend 0,1 or 2" );
    lua_pushinteger( L, -1 );
    return 1;
  }
  
  if (!spiInit[id]) {
    l_message( NULL, "spi not yet initialized" );
    lua_pushinteger( L, -2 );
    return 1;
  }
      
  uint8_t pin = luaL_checkinteger( L, 2 );
  MOD_CHECK_ID( gpio, pin );
  TFT_pinDC = wifimcu_gpio_map[pin];
  MicoGpioFinalize((mico_gpio_t)TFT_pinDC);
  MicoGpioInitialize((mico_gpio_t)TFT_pinDC, (mico_gpio_config_t)OUTPUT_PUSH_PULL);
  MicoGpioOutputLow( (mico_gpio_t)TFT_pinDC );

  uint8_t typ = luaL_checkinteger( L, 3 );
  
  TFT_SPI_ID = id;
  TFT_setFont(FONT_8x8);
  _fg = TFT_GREEN;
  _bg = TFT_BLACK;
  if (typ == 0) ST7735_initB();
  else if (typ == 1) ST7735_initR(INITR_BLACKTAB);
  else ST7735_initR(INITR_GREENTAB);
  
  TFT_fillScreen(TFT_BLACK);
  if (lua_gettop(L) > 3) {
    typ = luaL_checkinteger( L, 4 );
    TFT_setRotation(typ);
  }
  
  lua_pushinteger( L, 0 );
  return 1;
}

//================================
static int lcd_cls( lua_State* L )
{
  if (TFT_SPI_ID > 2) return 0;
  
  uint16_t color = TFT_BLACK;
  
  if (lua_gettop(L) > 0) color = getColor( L, 1 );
  TFT_fillScreen(color);
  return 0;
}

//===================================
static int lcd_invert( lua_State* L )
{
  if (TFT_SPI_ID > 2) return 0;
  
  uint16_t inv = luaL_checkinteger( L, 1 );
  TFT_invertDisplay(inv);
  return 0;
}

//======================================
static int lcd_settransp( lua_State* L )
{
  if (TFT_SPI_ID > 2) return 0;
  
  _transparent = luaL_checkinteger( L, 1 );
  return 0;
}

//===================================
static int lcd_setrot( lua_State* L )
{
  if (TFT_SPI_ID > 2) return 0;
  
  rotation = luaL_checkinteger( L, 1 );
  return 0;
}

//====================================
static int lcd_setfont( lua_State* L )
{
  if (checkParam(1, L)) return 0;
  
  uint8_t fnt = luaL_checkinteger( L, 1 );
  TFT_setFont(fnt);
  if (fnt == FONT_7SEG) {
    if (lua_gettop(L) > 2) {
      uint8_t l = luaL_checkinteger( L, 2 );
      uint8_t w = luaL_checkinteger( L, 3 );
      if (l < 6) l = 6;
      if (l > 40) l = 40;
      if (w < 1) w = 1;
      if (w > (l/2)) w = l/2;
      if (w > 12) w = 12;
      cfont.x_size = l;
      cfont.y_size = w;
      cfont.offset = 0;
      cfont.color  = _fg;
      if (lua_gettop(L) > 3) {
        if (w > 1) {
          cfont.offset = 1;
          cfont.color  = getColor( L, 4 );
        }
      }
    }
    else {
      cfont.x_size = 12;
      cfont.y_size = 2;
    }
  }
  return 0;
}

//========================================
static int lcd_getfontsize( lua_State* L )
{
  if (cfont.bitmap == 1) {
    lua_pushinteger( L, cfont.x_size );
    lua_pushinteger( L, cfont.y_size );
  }
  else if (cfont.bitmap == 2) {
    lua_pushinteger( L, (2 * (2 * cfont.y_size + 1)) + cfont.x_size );
    lua_pushinteger( L, (3 * (2 * cfont.y_size + 1)) + (2 * cfont.x_size) );
  }
  else {
    lua_pushinteger( L, 0);
    lua_pushinteger( L, 0);
  }
  return 2;
}

//===============================
static int lcd_on( lua_State* L )
{
  if (TFT_SPI_ID > 2) return 0;
  
  ST7735_sendCmd(ST7735_DISPON);
  return 0;
}

//================================
static int lcd_off( lua_State* L )
{
  if (TFT_SPI_ID > 2) return 0;
  
  ST7735_sendCmd(ST7735_DISPOFF);
  return 0;
}

//=====================================
static int lcd_setcolor( lua_State* L )
{
  if (checkParam(1, L)) return 0;
  
  _fg = getColor( L, 1 );
  if (lua_gettop(L) > 1) _bg = getColor( L, 2 );
  return 0;
}

//=================================
static int lcd_line( lua_State* L )
{
  if (checkParam(4, L)) return 0;
  
  uint16_t color = _fg;
  if (lua_gettop(L) > 4) color = getColor( L, 5 );
  uint8_t x0 = luaL_checkinteger( L, 1 );
  uint8_t y0 = luaL_checkinteger( L, 2 );
  uint8_t x1 = luaL_checkinteger( L, 3 );
  uint8_t y1 = luaL_checkinteger( L, 4 );
  TFT_drawLine(x0,y0,x1,y1,color);
  return 0;
}

//=================================
static int lcd_rect( lua_State* L )
{
  if (checkParam(5, L)) return 0;
  
  uint16_t fillcolor = _bg;
  if (lua_gettop(L) > 5) fillcolor = getColor( L, 6 );
  uint8_t x = luaL_checkinteger( L, 1 );
  uint8_t y = luaL_checkinteger( L, 2 );
  uint8_t w = luaL_checkinteger( L, 3 );
  uint8_t h = luaL_checkinteger( L, 4 );
  uint16_t color = getColor( L, 5 );
  if (fillcolor != _bg) TFT_fillRect(x,y,w,h,fillcolor);
  TFT_drawRect(x,y,w,h,color);
  return 0;
}

//=================================
static int lcd_circle( lua_State* L )
{
  if (checkParam(4, L)) return 0;
  
  uint16_t fillcolor = _bg;
  if (lua_gettop(L) > 4) fillcolor = getColor( L, 5 );
  uint8_t x = luaL_checkinteger( L, 1 );
  uint8_t y = luaL_checkinteger( L, 2 );
  uint8_t r = luaL_checkinteger( L, 3 );
  uint16_t color = getColor( L, 4 );
  if (fillcolor != _bg) TFT_fillCircle(x,y,r,fillcolor);
  TFT_drawCircle(x,y,r,color);
  return 0;
}

//=====================================
static int lcd_triangle( lua_State* L )
{
  if (checkParam(7, L)) return 0;
  
  uint16_t fillcolor = _bg;
  if (lua_gettop(L) > 7) fillcolor = getColor( L, 8 );
  uint8_t x0 = luaL_checkinteger( L, 1 );
  uint8_t y0 = luaL_checkinteger( L, 2 );
  uint8_t x1 = luaL_checkinteger( L, 3 );
  uint8_t y1 = luaL_checkinteger( L, 4 );
  uint8_t x2 = luaL_checkinteger( L, 5 );
  uint8_t y2 = luaL_checkinteger( L, 6 );
  uint16_t color = getColor( L, 7 );
  if (fillcolor != _bg) TFT_fillTriangle(x0,y0,x1,y1,x2,y2,fillcolor);
  TFT_drawTriangle(x0,y0,x1,y1,x2,y2,color);
  return 0;
}

//lcd.write(x,y,string|intnum|{floatnum,dec},...)
//==================================
static int lcd_write( lua_State* L )
{
  if (checkParam(3, L)) return 0;

  const char* buf;
  char tmps[16];
  size_t len;
  uint8_t numdec = 0;
  uint8_t argn = 0;
  float fnum;
  
  int x = luaL_checkinteger( L, 1 );
  int y = luaL_checkinteger( L, 2 );
  
  for( argn = 3; argn <= lua_gettop( L ); argn++ )
  {
    if ( lua_type( L, argn ) == LUA_TNUMBER )
    { // write integer number
      len = lua_tointeger( L, argn );
      sprintf(tmps,"%d",len);
      TFT_print(&tmps[0], x, y);
    }
    else if ( lua_type( L, argn ) == LUA_TTABLE ) {
      if (lua_objlen( L, argn ) == 2) {
        lua_rawgeti( L, argn, 1 );
        fnum = luaL_checknumber( L, -1 );
        lua_pop( L, 1 );
        lua_rawgeti( L, argn, 2 );
        numdec = ( int )luaL_checkinteger( L, -1 );
        lua_pop( L, 1 );
        sprintf(tmps,"%.*f",numdec, fnum);
        TFT_print(&tmps[0], x, y);
      }
    }
    else if ( lua_type( L, argn ) == LUA_TSTRING )
    { // write string
      luaL_checktype( L, argn, LUA_TSTRING );
      buf = lua_tolstring( L, argn, &len );
      TFT_print((char*)buf, x, y);
    }
  }  
  return 0;
}

//==================================
static int lcd_image( lua_State* L )
{
  if (checkParam(5, L)) return 0;

  const char *fname;
  uint8_t buf[320];
  uint16_t xrd = 0;
  size_t len;
  
  int x = luaL_checkinteger( L, 1 );
  int y = luaL_checkinteger( L, 2 );
  int xsize = luaL_checkinteger( L, 3 );
  int ysize = luaL_checkinteger( L, 4 );
  fname = luaL_checklstring( L, 5, &len );
  
  if( len > SPIFFS_OBJ_NAME_LEN ) {
    l_message(NULL,"filename too long.");
    return 0;
  }
  
  if(FILE_NOT_OPENED!=file_fd){
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
  }
  
  // Open the file
  file_fd = SPIFFS_open(&fs,(char*)fname,mode2flag("r"),0);
  if(file_fd < FILE_NOT_OPENED){
    file_fd = FILE_NOT_OPENED;
    l_message(NULL,"Error opening file.");
    return 0;
  }
  
  do {
    // read 1 imege line from file
    xrd = SPIFFS_read(&fs, (spiffs_file)file_fd, &buf[0], 2*xsize);
    if (xrd == 2*xsize) {
      //TFT_drawBitmap(x, y, xsize, ysize, (bitmapdatatype)buf, 1);
      ST7735_setAddrWindow(x, y, x+xsize-1, y);
      LCD_DC1;
      _spi_write(TFT_SPI_ID, 8, &buf[0], xrd, 1);
      y++;
      ysize--;
    }
    else xrd = 0;
  }while (xrd > 0 && ysize > 0);
  
  if(FILE_NOT_OPENED!=file_fd){
    SPIFFS_close(&fs,file_fd);
    file_fd = FILE_NOT_OPENED;
  }
  
  return 0;
}



#define MIN_OPT_LEVEL       2
#include "lrodefs.h"
const LUA_REG_TYPE lcd_map[] =
{
  { LSTRKEY( "init" ), LFUNCVAL( lcd_init )},
  { LSTRKEY( "clear" ), LFUNCVAL( lcd_cls )},
  { LSTRKEY( "on" ), LFUNCVAL( lcd_on )},
  { LSTRKEY( "off" ), LFUNCVAL( lcd_off )},
  { LSTRKEY( "setfont" ), LFUNCVAL( lcd_setfont )},
  { LSTRKEY( "getfontsize" ), LFUNCVAL( lcd_getfontsize )},
  { LSTRKEY( "setrot" ), LFUNCVAL( lcd_setrot )},
  { LSTRKEY( "setcolor" ), LFUNCVAL( lcd_setcolor )},
  { LSTRKEY( "settransp" ), LFUNCVAL( lcd_settransp )},
  { LSTRKEY( "invert" ), LFUNCVAL( lcd_invert )},
  { LSTRKEY( "line" ), LFUNCVAL( lcd_line )},
  { LSTRKEY( "rect" ), LFUNCVAL( lcd_rect )},
  { LSTRKEY( "circle" ), LFUNCVAL( lcd_circle )},
  { LSTRKEY( "triangle" ), LFUNCVAL( lcd_triangle )},
  { LSTRKEY( "write" ), LFUNCVAL( lcd_write )},
  { LSTRKEY( "image" ), LFUNCVAL( lcd_image )},
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY( "PORTRAIT" ),       LNUMVAL( PORTRAIT ) },
  { LSTRKEY( "PORTRAIT_FLIP" ),  LNUMVAL( PORTRAIT_FLIP ) },
  { LSTRKEY( "LANDSCAPE" ),      LNUMVAL( LANDSCAPE ) },
  { LSTRKEY( "LANDSCAPE_FLIP" ), LNUMVAL( LANDSCAPE_FLIP ) },
  { LSTRKEY( "CENTER" ),         LNUMVAL( CENTER ) },
  { LSTRKEY( "RIGHT" ),          LNUMVAL( RIGHT ) },
  { LSTRKEY( "BLACK" ),          LNUMVAL( TFT_BLACK ) },
  { LSTRKEY( "NAVY" ),           LNUMVAL( TFT_NAVY ) },
  { LSTRKEY( "DARKGREEN" ),      LNUMVAL( TFT_DARKGREEN ) },
  { LSTRKEY( "DARKCYAN" ),       LNUMVAL( TFT_DARKCYAN ) },
  { LSTRKEY( "MAROON" ),         LNUMVAL( TFT_MAROON ) },
  { LSTRKEY( "PURPLE" ),         LNUMVAL( TFT_PURPLE ) },
  { LSTRKEY( "OLIVE" ),          LNUMVAL( TFT_OLIVE ) },
  { LSTRKEY( "LIGHTGREY" ),      LNUMVAL( TFT_LIGHTGREY ) },
  { LSTRKEY( "DARKGREY" ),       LNUMVAL( TFT_DARKGREY ) },
  { LSTRKEY( "BLUE" ),           LNUMVAL( TFT_BLUE ) },
  { LSTRKEY( "GREEN" ),          LNUMVAL( TFT_GREEN ) },
  { LSTRKEY( "CYAN" ),           LNUMVAL( TFT_CYAN ) },
  { LSTRKEY( "RED" ),            LNUMVAL( TFT_RED ) },
  { LSTRKEY( "MAGENTA" ),        LNUMVAL( TFT_MAGENTA ) },
  { LSTRKEY( "YELLOW" ),         LNUMVAL( TFT_YELLOW ) },
  { LSTRKEY( "WHITE" ),          LNUMVAL( TFT_WHITE ) },
  { LSTRKEY( "ORANGE" ),         LNUMVAL( TFT_ORANGE ) },
  { LSTRKEY( "GREENYELLOW" ),    LNUMVAL( TFT_GREENYELLOW ) },
  { LSTRKEY( "PINK" ),           LNUMVAL( TFT_PINK ) },
  { LSTRKEY( "FONT_SMALL" ),     LNUMVAL( SMALL_FONT ) },
  { LSTRKEY( "FONT_BIG" ),       LNUMVAL( BIG_FONT ) },
  { LSTRKEY( "FONT_7SEG" ),      LNUMVAL( FONT_7SEG ) },
  { LSTRKEY( "FONT_8x8" ),       LNUMVAL( FONT_8x8 ) },
#endif      
  {LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_lcd(lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else    
  luaL_register( L, EXLIB_LCD, lcd_map );
  MOD_REG_NUMBER( L, "PORTRAIT",        PORTRAIT);
  MOD_REG_NUMBER( L, "PORTRAIT_FLIP",   PORTRAIT_FLIP);
  MOD_REG_NUMBER( L, "LANDSCAPE",       LANDSCAPE);
  MOD_REG_NUMBER( L, "LANDSCAPE_FLIP",  LANDSCAPE_FLIP);
  MOD_REG_NUMBER( L, "CENTER" ),        CENTER);
  MOD_REG_NUMBER( L, "RIGHT" ),         RIGHT;
  MOD_REG_NUMBER( L, "BLACK",           TFT_BLACK);
  MOD_REG_NUMBER( L, "NAVY",            TFT_NAVY);
  MOD_REG_NUMBER( L, "DARKGREEN",       TFT_DARKGREEN);
  MOD_REG_NUMBER( L, "DARKCYAN",        TFT_DARKCYAN);
  MOD_REG_NUMBER( L, "MAROON",          TFT_MAROON);
  MOD_REG_NUMBER( L, "PURPLE",          TFT_PURPLE);
  MOD_REG_NUMBER( L, "OLIVE",           TFT_OLIVE);
  MOD_REG_NUMBER( L, "LIGHTGREY",       TFT_LIGHTGREY);
  MOD_REG_NUMBER( L, "DARKGREY",        TFT_DARKGREY);
  MOD_REG_NUMBER( L, "BLUE" ,           TFT_BLUE);
  MOD_REG_NUMBER( L, "GREEN",           TFT_GREEN);
  MOD_REG_NUMBER( L, "CYAN" ,           TFT_CYAN);
  MOD_REG_NUMBER( L, "RED",             TFT_RED);
  MOD_REG_NUMBER( L, "MAGENTA",         TFT_MAGENTA);
  MOD_REG_NUMBER( L, "YELLOW",          TFT_YELLOW);
  MOD_REG_NUMBER( L, "WHITE",           TFT_WHITE);
  MOD_REG_NUMBER( L, "ORANGE",          TFT_ORANGE);
  MOD_REG_NUMBER( L, "GREENYELLOW",     TFT_GREENYELLOW);
  MOD_REG_NUMBER( L, "PINK" ,           TFT_PINK);
  MOD_REG_NUMBER( L, "FONT_SMALL" ,     SMALL_FONT);
  MOD_REG_NUMBER( L, "FONT_BIG" ,       BIG_FONT);
  MOD_REG_NUMBER( L, "7SEG_FONT" ,      FONT_7SEG);
  MOD_REG_NUMBER( L, "FONT_8x8" ,       FONT_8x8);
  return 1;
#endif
}
