/***********************************************************************
 ***********************************************************************
 ***
 ***  ASCII green-screen terminal emulator
 ***
 ***  Written by Oleg Yakovlev
 ***  MIT license, all text below must be included in any redistribution
 ***
 ***********************************************************************
 ***********************************************************************/

typedef struct _cursor {
	uint16_t	row;
	uint16_t	col;
	uint8_t		*bitmap; // not used yet
} Cursor;

static struct __screen {
	Cursor 		c;
	uint8_t 	nrow;
	uint8_t 	ncol;
	Font 		fnt;
	uint16_t 	fg;
	uint16_t 	bg;
	char		*scr;
} _screen;

static void _putch(uint8_t c);

#define _scr(r,c) ((char *)(_screen.scr + ((r) * _screen.ncol) + (c)))

/********************************************************************
*********************************************************************
*********************** Private functions ***************************
*********************************************************************
*********************************************************************/
static void _scrollup() {
	int r,c;
	_screen.c.row = 0;
	_screen.c.col = 0;
	for(r=1;r<_screen.nrow;r++)
		for(c=0;c<_screen.ncol;c++) {
			_putch(*_scr(r,c));
			_screen.c.col++;
			if( _screen.c.col == _screen.ncol ) {			
				_screen.c.col = 0;
				_screen.c.row++;
			}
		}
		for(c=0;c<_screen.ncol;c++) {
			_putch(' ');
			_screen.c.col++;
		}
		_screen.c.row = _screen.nrow - 1;
		_screen.c.col = 0;
}

static void cursor_expose(int flg) {
	uint8_t i,fz;
	uint16_t j;
	int x,y;

	fz = _screen.fnt.x_size/8;
	x = _screen.c.col * _screen.fnt.x_size;
	y = _screen.c.row * _screen.fnt.y_size;
	lcd7735_setAddrWindow(x,y,x+_screen.fnt.x_size-1,y+_screen.fnt.y_size-1);
	for(j=0;j<((fz)*_screen.fnt.y_size);j++) {
		for(i=0;i<8;i++) {
			if( flg )
				lcd7735_pushColor(_screen.fg);
			else
				lcd7735_pushColor(_screen.bg);
		}
	}
}

#define cursor_draw	cursor_expose(1)
#define cursor_erase	cursor_expose(0)

static void cursor_nl() {
	_screen.c.col = 0;
	_screen.c.row++;
	if( _screen.c.row == _screen.nrow ) {
		_scrollup();
	}
}

static void cursor_fwd() {
	_screen.c.col++; 
	if( _screen.c.col == _screen.ncol ) {
		cursor_nl();
	}
}


static void cursor_init() {
	_screen.c.row = 0;
	_screen.c.col = 0;
}

static void _putch(uint8_t c) {
	uint8_t i,ch,fz;
	uint16_t j;
	uint16_t temp; 
	int x,y;

	fz = _screen.fnt.x_size/8;
	x = _screen.c.col * _screen.fnt.x_size;
	y = _screen.c.row * _screen.fnt.y_size;
	lcd7735_setAddrWindow(x,y,x+_screen.fnt.x_size-1,y+_screen.fnt.y_size-1);
	temp=((c-_screen.fnt.offset)*((fz)*_screen.fnt.y_size))+4;
	for(j=0;j<((fz)*_screen.fnt.y_size);j++) {
		ch = _screen.fnt.font[temp];
		for(i=0;i<8;i++) {   
			if((ch&(1<<(7-i)))!=0) {
				lcd7735_pushColor(_screen.fg);
			} else {
				lcd7735_pushColor(_screen.bg);
			}   
		}
		temp++;
	}
	*_scr(_screen.c.row, _screen.c.col) = c;
}



/********************************************************************
*********************************************************************
*********************** Public functions ***************************
*********************************************************************
*********************************************************************/
static void lcd7735_init_screen(void *font,uint16_t fg, uint16_t bg, uint8_t orientation) {
	lcd7735_setRotation(orientation);
	lcd7735_fillScreen(bg);
	_screen.fg = fg;
	_screen.bg = bg;
	_screen.fnt.font = (uint8_t *)font;
	_screen.fnt.x_size = _screen.fnt.font[0];
	_screen.fnt.y_size = _screen.fnt.font[1];
	_screen.fnt.offset = _screen.fnt.font[2];
	_screen.fnt.numchars = _screen.fnt.font[3];
	_screen.nrow = _height / _screen.fnt.y_size;
	_screen.ncol = _width  / _screen.fnt.x_size;
	_screen.scr = malloc(_screen.nrow * _screen.ncol);
	memset((void*)_screen.scr,' ',_screen.nrow * _screen.ncol);
	cursor_init();
	cursor_draw;
}

static void lcd7735_putc(char c) {
	if( c != '\n' && c != '\r' ) {
		_putch(c);
		cursor_fwd();
	} else {
		cursor_erase;
		cursor_nl();
	}
	cursor_draw;
}

static void lcd7735_puts(char *str) {
	int i;
	for(i=0;i<strlen(str);i++) {
		if( str[i] != '\n' && str[i] != '\r' ) {
			_putch(str[i]);
			cursor_fwd();
		} else {
			cursor_erase;
			cursor_nl();
		}
	}
	cursor_draw;
}

static void lcd7735_cursor_set(uint16_t row, uint16_t col) {
	if( row < _screen.nrow && col < _screen.ncol ) {
		_screen.c.row = row;
		_screen.c.col = col;
	}
	cursor_draw;
}

