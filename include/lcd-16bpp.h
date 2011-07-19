/* Define graphic primitives for 16 bit truecolor modes */
#ifndef _LCD_16BPP_H_
#define _LCD_16BPP_H_

#include <lcd.h>			  /* XYPOS, COLORVAL, WIN_INFO */

/* Draw characters at given position */
extern void lcd_char16(WIN_INFO *wininfo, XYPOS x, XYPOS y, uchar c);

/* Draw filled rectangle */
extern void lcd_rect16(WIN_INFO *wininfo, XYPOS x, XYPOS y,
		       ushort width, ushort height, COLORVAL col);

/* Draw a pixel */
extern void lcd_pixel16(WIN_INFO *wininfo, ushort x, ushort y, COLORVAL col);

#endif /*!_LCD_16BPP_H_*/
