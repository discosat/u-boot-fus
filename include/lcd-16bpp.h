/* Define graphic primitives for 16 bit truecolor modes */
#ifndef _LCD_16BPP_H_
#define _LCD_16BPP_H_

#include <lcd.h>			  /* XYPOS, COLORVAL, WIN_INFO */

/* Draw characters at given position */
extern void lcd_char16(wininfo_t *pwi, XYPOS x, XYPOS y, u_char c);

/* Draw filled rectangle from (x1, y1) to (x1, y2) */
extern void lcd_rect16(wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2,
		       COLORVAL col);
/* Draw a pixel */
extern void lcd_pixel16(wininfo_t *pwi, XYPOS x, XYPOS y, COLORVAL col);

#endif /*!_LCD_16BPP_H_*/
