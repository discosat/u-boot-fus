/*
 * Commands draw, adraw and bminfo
 *
 * (C) Copyright 2012
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <config.h>
#include <common.h>
#include <cmd_lcd.h>			  /* wininfo_t, kwinfo_t, ... */
#include <video_font.h>			  /* VIDEO_FONT_WIDTH, ... */
#include <stdio_dev.h>			  /* stdio_dev, stdio_register(), ... */
#include <linux/ctype.h>		  /* isdigit(), toupper() */
#include <watchdog.h>			  /* WATCHDOG_RESET */

#if defined(CONFIG_XLCD_PNG) \
	|| defined(CONFIG_XLCD_BMP) \
	|| defined(CONFIG_XLCD_JPG)
#include <xlcd_bitmap.h>		  /* scan_png(), draw_png(), ... */
#endif

#ifdef CONFIG_CMD_DRAW
#include <xlcd_draw_ll.h>		  /* draw_ll_*() */
#endif
#ifdef CONFIG_CMD_ADRAW
#include <xlcd_adraw_ll.h>		  /* adraw_ll_*() */
#endif


/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/


/************************************************************************/
/* ENUMERATIONS								*/
/************************************************************************/

/* Settings that correspond with commands "draw" and "adraw"; the order
   decides in what sequence the commands are searched for, which may be
   important for sub-commands with the same prefix. */
enum DRAW_INDEX {
	DI_COLOR,
#if CONFIG_XLCD_DRAW & XLCD_DRAW_PIXEL
	DI_PIXEL,
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_LINE
	DI_LINE,
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_RECT
	DI_FRAME,
	DI_RECT,
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_CIRC
	DI_RFRAME,
	DI_RRECT,
	DI_CIRCLE,
	DI_DISC,
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_TEXT
	DI_TEXT,
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_BITMAP
	DI_BITMAP,
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_TURTLE
	DI_TURTLE,
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_FILL
	DI_FILL,
	DI_CLEAR,
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_PROG
	DI_PBT,
	DI_PBR,
	DI_PROG,
#endif
#if (CONFIG_XLCD_DRAW & XLCD_DRAW_TEST) && defined(CONFIG_CMD_DRAW)
	DI_TEST,
#endif
	DI_CLIP,
	DI_ORIGIN,
	DI_HELP
};


/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/


/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/

/* Expression parser for draw command */
static char *pNextChar;			  /* Pointer to next character */
static char *pParseError;		  /* NULL or error reason */

/* Keywords available with draw and adraw; info1 holds the number of
   coordinate pairs, info2 holds the index for the first rgba value (use a
   value higher than argc_max if no rgba value at all) */
static kwinfo_t const draw_kw[] = {
	[DI_COLOR] =  {1, 2, 0, 0, "color"},  /* rgba [rgba] */
#if CONFIG_XLCD_DRAW & XLCD_DRAW_PIXEL
	[DI_PIXEL] =  {2, 3, 1, 2, "pixel"},  /* x1 y1 [rgba] */
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_LINE
	[DI_LINE] =   {4, 5, 2, 4, "line"},   /* x1 y1 x2 y2 [rgba] */
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_RECT
	[DI_FRAME] =  {4, 5, 2, 4, "frame"},  /* x1 y1 x2 y2 [rgba] */
	[DI_RECT] =   {4, 6, 2, 4, "rect"},   /* x1 y1 x2 y2 [rgba [rgba]] */
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_CIRC
	[DI_RFRAME] = {5, 6, 2, 5, "rframe"}, /* x1 y1 x2 y2 r [rgba] */
	[DI_RRECT] =  {5, 7, 2, 5, "rrect"},  /* x1 y1 x2 y2 r [rgba [rgba]] */
	[DI_CIRCLE] = {3, 4, 1, 3, "circle"}, /* x1 y1 r [rgba] */
	[DI_DISC] =   {3, 5, 1, 3, "disc"},   /* x1 y1 r [rgba [rgba]] */
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_TEXT
	[DI_TEXT] =   {3, 6, 1, 4, "text"},   /* x1 y1 s [attr [rgba [rgba]]] */
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_BITMAP
	[DI_BITMAP] = {3, 5, 1, 9, "bm"},     /* x1 y1 addr [n [attr]] */
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_TURTLE
	[DI_TURTLE] = {3, 4, 1, 3, "turtle"}, /* x1 y1 s [rgba] */
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_FILL
	[DI_FILL] =   {0, 1, 0, 0, "fill"},   /* [rgba] */
	[DI_CLEAR] =  {0, 1, 0, 9, "clear"},  /* [rgba] */
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_PROG
	[DI_PBT] =    {0, 3, 0, 1, "pbt"},    /* [attr [rgba [rgba]]] */
	[DI_PBR] =    {4, 6, 2, 4, "pbr"},    /* x1 y1 x2 y2 [rgba [rgba]] */
	[DI_PROG] =   {0, 1, 0, 9, "prog"},   /* [n] */
#endif
#if (CONFIG_XLCD_DRAW & XLCD_DRAW_TEST) && defined(CONFIG_CMD_DRAW)
	[DI_TEST] =   {0, 1, 0, 9, "test"},   /* [n] */
#endif
	[DI_CLIP] =   {4, 4, 2, 9, "clip"},   /* x1 y1 x2 y2 */
	[DI_ORIGIN] = {2, 2, 1, 9, "origin"}, /* x1 y1 */
	[DI_HELP] =   {0, 0, 0, 9, "help"},   /* (no args, show usage) */
};


/************************************************************************/
/* Prototypes of local functions					*/
/************************************************************************/



/************************************************************************/
/* EXPRESSION PARSER							*/
/************************************************************************/

#ifdef CONFIG_XLCD_EXPR

/* We need this as forward declaration */
static int parse_sum(wininfo_t *pwi);

/* Parse a factor, i.e. a sub-expression in '('..')', a variable or a number;
   the factor can have any number of prefix signs '+' or '-'. After return,
   pNextChar is either on the next character behind the factor or in case of
   error on the erroneous character itself */
static int parse_factor(wininfo_t *pwi)
{
	char c;
	int sign = 1;
	int result;

	/* Parse sign prefix (multiple prefixes allowed, e.g. --+-5) */
	for (;;) {
		c = *pNextChar;
		if (c == '-')
			sign = -sign;
		else if (c != '+')
			break;
		pNextChar++;
	}

	/* Check for subexpression in parantheses */
	if (c == '(') {
		pNextChar++;
		result = parse_sum(pwi);
		if (pParseError)
			return result;

		/* Check for closing parenthesis */
		if (*pNextChar == ')')
			pNextChar++;
		else
			pParseError = "Missing ')'";
	}
	/* Check for number */
	else if (isdigit(c)) {
		char *pStop;

		result = (int)simple_strtoul(pNextChar, &pStop, 0);
		pNextChar = pStop;
	}
	/* Check for variables */
	else {
		int bUseFB = 0;
		XYPOS res;
		XYPOS origin;

		if (strncmp(pNextChar, "fb", 2) == 0) {
			bUseFB = 1;
			pNextChar += 2;
		}
		c = *pNextChar++;
		if (c == 'h') {
			/* Horizontal values */
			res = bUseFB ? pwi->fbhres
				: (pwi->clip_right - pwi->clip_left + 1);
			origin = pwi->horigin;
		} else if (c == 'v') {
			res = bUseFB ? pwi->fbvres
				: (pwi->clip_bottom - pwi->clip_top + 1);
			origin = pwi->vorigin;
		} else {
		BADVAR:
			/* Bad variable name; reset pointer to beginning of
			   variable name */
			pParseError = "Bad name";
			pNextChar -= bUseFB ? 3 : 1;
			return 0;
		}
		if (strncmp(pNextChar, "min", 3) == 0)
			result = -origin;
		else if (strncmp(pNextChar, "max", 3) == 0)
			result = res - 1 - origin;
		else if (strncmp(pNextChar, "mid", 6) == 0)
			result = res/2 - origin;
		else if (strncmp(pNextChar, "res", 3) == 0)
			result = res;
		else
			goto BADVAR;
		pNextChar += 3;
	}

	/* Apply sign */
	if (sign < 0)
		result = -result;

	return result;
}


/* Parse a product i.e. a sub-expression that combines factors with '*' or '/'.
   After return, pNextChar is either on the next character behind the product
   or in case of error on the erroneous character itself */
static int parse_product(wininfo_t *pwi)
{
	int result;
	char c;

	result = parse_factor(pwi);

	while (!pParseError) {
		c = *pNextChar;
		if (c == '*') {
			pNextChar++;
			result *= parse_product(pwi);
		}
		else if (c == '/') {
			int tmp;
			pNextChar++;
			tmp = parse_product(pwi);
			if (pParseError)
				break;

			if (!tmp)
				pParseError = "Division by zero";
			else
				result /= tmp;
		}
		else
			break;
	}

	return result;
}


/* Parse a sum, i.e. a sub-expression that combines products with '+' or '-'.
   After return, pNextChar is either on the next character behind the sum
   or in case of error on the erroneous character itself */
static int parse_sum(wininfo_t *pwi)
{
	int result;
	char c;

	result = parse_product(pwi);
	while (!pParseError) {
		c = *pNextChar;
		if (c == '+') {
			pNextChar++;
			result += parse_product(pwi);
		}
		else if (c == '-') {
			pNextChar++;
			result -= parse_product(pwi);
		}
		else
			break;
	}

	return result;
}


/* Parse the expression at pExpr for coordinates, return result in pResult.
   Return 0 on success and 1 on error. */
static int parse_expr(wininfo_t *pwi, char *pExpr, XYPOS *pResult)
{
	char c;

	/* Initialize parser pointer and parser error */
	pNextChar = pExpr;
	pParseError = NULL;

	/* Parse expression as a sum */
	*pResult = parse_sum(pwi);

	/* If the expression does not contain an error, it should now be fully
	   consumed. */
	c = *pNextChar;
	if (c && !pParseError)
		pParseError = "Invalid character";

	/* Show any error */
	if (pParseError) {
		printf("%s at %s\n", pParseError,
		       c ? pNextChar : "end of expression");
	}

	return (pParseError != NULL);
}
#endif /* CONFIG_XLCD_EXPR */


/************************************************************************/
/* GRAPHICS PRIMITIVES							*/
/************************************************************************/

#if CONFIG_XLCD_DRAW & (XLCD_DRAW_PIXEL | XLCD_DRAW_LINE | XLCD_DRAW_CIRC \
			| XLCD_DRAW_TURTLE)
/* Draw pixel at (x, y) with given color */
void lcd_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y, const colinfo_t *pci)
{
	if ((x < pwi->clip_left) || (x > pwi->clip_right)
	    || (y < pwi->clip_top) || (y > pwi->clip_bottom))
		return;

#if defined(CONFIG_CMD_DRAW) && defined(CONFIG_CMD_ADRAW)
	if (pwi->attr & ATTR_ALPHA)
		adraw_ll_pixel(pwi, x, y, pci);
	else
		draw_ll_pixel(pwi, x, y, pci->col);
#elif defined(CONFIG_CMD_DRAW)
	draw_ll_pixel(pwi, x, y, pci->col);
#elif defined(CONFIG_CMD_ADRAW)
	adraw_ll_pixel(pwi, x, y, pci);
#endif
}
#endif


#if CONFIG_XLCD_DRAW & (XLCD_DRAW_RECT | XLCD_DRAW_CIRC | XLCD_DRAW_PROG)
/* Draw filled rectangle from (x1, y1) to (x2, y2) in color; x1<=x2 and
   y1<=y2 must be valid! */
void lcd_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2,
	      const colinfo_t *pci)
{
	XYPOS xmin, ymin;
	XYPOS xmax, ymax;

	/* Check if object is fully left, right, above or below screen */
	xmin = pwi->clip_left;
	ymin = pwi->clip_top;
	xmax = pwi->clip_right;
	ymax = pwi->clip_bottom;
	if ((x2 < xmin) || (y2 < ymin) || (x1 > xmax) || (y1 > ymax))
		return;			  /* Done, object not visible */

	/* Clip rectangle to framebuffer boundaries */
	if (x1 < xmin)
		x1 = xmin;
	if (y1 < ymin)
		y1 = ymin;
	if (x2 > xmax)
		x2 = xmax;
	if (y2 >= ymax)
		y2 = ymax;

	/* Finally draw rectangle */
#if defined(CONFIG_CMD_DRAW) && defined(CONFIG_CMD_ADRAW)
	if (pwi->attr & ATTR_ALPHA)
		adraw_ll_rect(pwi, x1, y1, x2, y2, pci);
	else
		draw_ll_rect(pwi, x1, y1, x2, y2, pci->col);
#elif defined(CONFIG_CMD_DRAW)
	draw_ll_rect(pwi, x1, y1, x2, y2, pci->col);
#elif defined(CONFIG_CMD_ADRAW)
	adraw_ll_rect(pwi, x1, y1, x2, y2, pci);
#endif
}
#endif


#if CONFIG_XLCD_DRAW & (XLCD_DRAW_RECT | XLCD_DRAW_CIRC)
/* Draw rectangular frame from (x1, y1) to (x2, y2) in given color; x1<=x2 and
   y1<=y2 must be valid! */
void lcd_frame(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2,
	       const colinfo_t *pci)
{
	/* If the frame is wider than two pixels, we need to draw
	   horizontal lines at the top and bottom; clipping is done in
	   lcd_rect() so we don't care about clipping here. */
	if (x2 - x1 > 1) {
		/* Draw top line */
		lcd_rect(pwi, x1, y1, x2, y1, pci);

		/* We are done if rectangle is exactly one pixel high */
		if (y1 == y2)
			return;

		/* Draw bottom line */
		lcd_rect(pwi, x1, y2, x2, y2, pci);

		/* For the vertical lines we only need to draw the region
		   between the horizontal lines, so increment y1 and decrement
		   y2; if rectangle is exactly two pixels high, we don't
		   need to draw any vertical lines at all. */
		if (++y1 == y2--)
			return;
	}

	/* Draw left line */
	lcd_rect(pwi, x1, y1, x1, y2, pci);

	/* Return if rectangle is exactly one pixel wide */
	if (x1 == x2)
		return;

	/* Draw right line */
	lcd_rect(pwi, x2, y1, x2, y2, pci);
}
#endif


#if CONFIG_XLCD_DRAW & (XLCD_DRAW_LINE | XLCD_DRAW_TURTLE)
/* Draw line from (x1, y1) to (x2, y2) in color */
void lcd_line(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2,
	      const colinfo_t *pci)
{
	int dx, dy, dd;
	XYPOS xmin, ymin, xmax, ymax;
	XYPOS xoffs, yoffs;
	XYPOS xinc, yinc;

	dx = (int)x2 - (int)x1;
	if (dx < 0)
		dx = -dx;
	dy = (int)y2 - (int)y1;
	if (dy < 0)
		dy = -dy;

	xmin = pwi->clip_left;
	ymin = pwi->clip_top;
	xmax = pwi->clip_right;
	ymax = pwi->clip_bottom;

	if (dy > dx) {			  /* High slope */
		/* Sort pixels so that y1 <= y2 */
		if (y1 > y2) {
			XYPOS temp;

			temp = x1;
			x1 = x2;
			x2 = temp;
			temp = y1;
			y1 = y2;
			y2 = temp;
		}

		/* Return if line is completely above or below the display */
		if ((y2 < ymin) || (y1 > ymax))
			return;

		dd = dy;
		dx <<= 1;
		dy <<= 1;

		xinc = (x1 > x2) ? -1 : 1;
		if (y1 < ymin) {
			/* Clip with upper screen edge */
			yoffs = ymin - y1;
			xoffs = (dd + (int)yoffs * dx)/dy;
			dd += xoffs*dy - yoffs*dx;
			y1 = ymin;
			x1 += xinc*xoffs;
		}

		/* Return if line fragment is fully left or right of display */
		if (((x1 < xmin) && (x2 < xmin))
		    || ((x1 > xmax) && (x2 > xmax)))
			return;

		/* We only need y2 as end coordinate */
		if (y2 > ymax)
			y2 = ymax;

#if CONFIG_XLCD_DRAW & XLCD_DRAW_RECT
		/* If line is vertical, we can use the more efficient
		   rectangle function */
		if (dx == 0) {
			lcd_rect(pwi, x1, y1, x2, y2, pci);
			return;
		}
#endif

		/* Draw line from top to bottom, i.e. every loop cycle go one
		   pixel down and sometimes one pixel left or right */
		for (;;) {
			lcd_pixel(pwi, x1, y1, pci);
			if (y1 == y2)
				break;
			y1++;
			dd += dx;
			if (dd >= dy) {
				dd -= dy;
				x1 += xinc;
			}
		}
	} else {			  /* Low slope */
		/* Sort pixels so that x1 <= x2 */
		if (x1 > x2) {
			XYPOS temp;

			temp = x1;
			x1 = x2;
			x2 = temp;
			temp = y1;
			y1 = y2;
			y2 = temp;
		}

		/* Return if line is completely left or right of the display */
		if ((x2 < xmin) || (x1 > xmax))
			return;

		dd = dx;
		dx <<= 1;
		dy <<= 1;

		yinc = (y1 > y2) ? -1 : 1;
		if (x1 < xmin) {
			/* Clip with left screen edge */
			xoffs = xmin - x1;
			yoffs = (dd + (int)xoffs * dy)/dx;
			dd += yoffs*dx - xoffs*dy;
			x1 = xmin;
			y1 += yinc*yoffs;
		}

		/* Return if line fragment is fully above or below display */
		if (((y1 < xmin) && (y2 < xmin))
		    || ((y1 > ymax) && (y2 > ymax)))
			return;

		/* We only need x2 as end coordinate */
		if (x2 > xmax)
			x2 = xmax;

#if CONFIG_XLCD_DRAW & XLCD_DRAW_RECT
		/* If line is horizontal, we can use the more efficient
		   rectangle function */
		if (dy == 0) {
			/* Draw horizontal line */
			lcd_rect(pwi, x1, y1, x2, y2, pci);
			return;
		}
#endif

		/* Draw line from left to right, i.e. every loop cycle go one
		   pixel right and sometimes one pixel up or down */
		for (;;) {
			lcd_pixel(pwi, x1, y1, pci);
			if (x1 == x2)
				break;
			x1++;
			dd += dy;
			if (dd >= dx) {
				dd -= dx;
				y1 += yinc;
			}
		}
	}
}
#endif


#if CONFIG_XLCD_DRAW & XLCD_DRAW_CIRC
/* Draw unfilled frame from (x1, y1) to (x2, y2) using rounded corners with
 * radius r. Call with x1=x-r, y1=y-r, x2=x+r, y2=y+r to draw a circle with
 * radius r at centerpoint (x,y).
 *
 * Circle algorithm for corners
 * ----------------------------
 * The circle is computed as if it was at the coordinate system origin. We
 * only compute the pixels (dx, dy) for the first quadrant (top right),
 * starting from the top position. The other pixels of the circle can be
 * obtained by utilizing the circle symmetry by inverting dx or dy. The final
 * circle pixel on the display is obtained by adding the circle center
 * coordinates.
 *
 * The algorithm is a so-called midpoint circle algorithm. In the first part
 * of the circle with slope >-1 (low slope, dy>dx), we check whether we have
 * to go from pixel (dx, dy) east (E) to (dx+1, dy) or southeast (SE) to
 * (dx+1, dy-1). This is done by checking the midpoint (dx+1, dy-1/2). If it
 * mathematically lies within the circle, we go E, otherwise SE.
 *
 * Similar to this, we check in the part with slope <-1 (high slope, dy<=dx)
 * whether we have to go southeast (SE) to (dx+1, dy-1) or south (S) to (dx,
 * dy-1). This is done by checking midpoint (dx+1/2, dy-1). If it lies within
 * the circle, we go SE, else S.
 *
 * A point (dx, dy) lies exactly on the circle line, if dx^2 + dy^2 = r^2
 * (circle equation). Thus a pixel is within the circle area, if function
 * F(dx, dy) = dx^2 + dy^2 - r^2 is less than 0. It is outside if F(dx, dy) is
 * greater than 0.
 *
 * Computing the value for this formula for each midpoint anew would be rather
 * time consuming, considering the fractions and squares. However if we only
 * compute the differences from midpoint to midpoint, it will get much easier.
 * Let's assume we already have the value dd = F(dx+1, dy-1/2) at some
 * midpoint, then we can easily obtain the value of the next midpoint:
 *   dd<0:  E:	deltaE	= F(dx+2, dy-1/2) - dd = 2*dx + 3
 *   dd>=0: SE: deltaSE = F(dx+2, dy-3/2) - dd = 2*dx - 2*dy + 5
 *
 * We have to start with the midpoint of pixel (dx=0, dy=r) which is
 *   dd = F(1, r-1/2) = 5/4 - r
 * By a transition about -1/4, we get dd = 1-r. However now we would have to
 * compare with -1/4 instead of with 0. But as center and radius are always
 * integers, this can be neglected.
 *
 * For the second part of the circle with high slope, the differences from
 * point to point can also be easily computed:
 *   dd<0:  SE: deltaSE = F(dx+3/2, dy-2) - dd = 2*dx - 2*dy + 5
 *   dd>=0: S:	deltaS	= F(dx+1/2, dy-2) - dd = -2*dy + 3
 *
 * We also have to consider the case when switching the slope, i.e. when we go
 * from midpoint (dx+1, dy-1/2) to midpoint (dx+1/2, dy-1). Again we only need
 * the difference:
 *   delta = F(dx+1/2, dy-1) - F(dx+1, dy-1/2) = F(dx+1/2, dy-1) - dd
 *	   = -dx - dy
 *
 * This results in the following basic circle algorithm:
 *     dx=0; dy=r; dd=1-r;
 *     while (dy>dx)				       Slope >-1 (low)
 *	   SetPixel(dx, dy);
 *	   if (dd<0)				       (*)
 *	       dd = dd+2*dx+3; dx=dx+1;		       East E
 *	   else
 *	       dd = dd+2*(dx-dy)+5; dx=dx+1; dy=dy-1;  Southeast SE
 *     dd = dd-dx-dy;
 *     while (dy>=0)				       Slope <-1 (high)
 *	   SetPixel(dx, dy)
 *	   if (dd<0)
 *	       dd = dd+2*(dx-dy)+5; dx=dx+1; dy=dy-1;  Southeast SE
 *	   else
 *	       dd = dd-2*dy+3; dy=dy-1;		       South S
 *
 * A small improvement can be obtained if adding && (dy > dx+1) at position
 * (*). Then there are no corners at slope -1 (45 degrees).
 *
 * To avoid drawing pixels twice when using the symmetry, we further handle
 * the first pixel (dx=0, dy=r) and the last pixel (dx=r, dy=0) separately.
 * Clipping is done in lcd_pixel() so we don't care about clipping here.
 *
 * Remark: this algorithm computes an optimal approximation to a circle, i.e.
 * the result is also symmetric to the angle bisector. */
void lcd_rframe(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2,
		XYPOS r, const colinfo_t *pci)
{
	XYPOS dx, dy, dd, maxr;

	if (r < 0)
		return;

	/* Check for the maximum possible radius for these coordinates */
	maxr = x2-x1;
	if (maxr > y2-y1)
		maxr = y2-y1;
	maxr = maxr/2;
	if (r > maxr)
		r = maxr;

	/* If r=0, draw standard frame without rounded corners */
	if (r == 0) {
		lcd_frame(pwi, x1, y1, x2, y2, pci);
		return;
	}

	/* Move coordinates to the centers of the quarter circle centers */
	x1 += r;
	y1 += r;
	x2 -= r;
	y2 -= r;

	/* Initialize midpoint values */
	dx = 0;
	dy = r;
	dd = 1-r;

	/* Draw top and bottom horizontal lines (dx == 0) */
	lcd_rect(pwi, x1, y1 - dy, x2, y1 - dy, pci);
	lcd_rect(pwi, x1, y2 + dy, x2, y2 + dy, pci);
	if (dd < 0)
		dd += 3;		  /* 2*dx + 3, but dx is 0 */
	else				  /* Only possible for r==1 */
		dy--;			  /* dd does not matter, dy is 0 */
	dx++;

	/* Draw part with low slope (every step changes dx, sometimes dy) */
	while (dy > dx) {
		lcd_pixel(pwi, x1 - dx, y1 - dy, pci);
		lcd_pixel(pwi, x2 + dx, y1 - dy, pci);
		lcd_pixel(pwi, x1 - dx, y2 + dy, pci);
		lcd_pixel(pwi, x2 + dx, y2 + dy, pci);
		if ((dd < 0) && (dy > dx + 1))
			dd += 2*dx + 3;	       /* E */
		else {
			dd += (dx - dy)*2 + 5; /* SE */
			dy--;
		}
		dx++;
	}

	/* Switch to high slope */
	dd = dd - dx - dy;

	/* Draw part with high slope (every step changes dym sometimes dx) */
	while (dy) {
		lcd_pixel(pwi, x1 - dx, y1 - dy, pci);
		lcd_pixel(pwi, x2 + dx, y1 - dy, pci);
		lcd_pixel(pwi, x1 - dx, y2 + dy, pci);
		lcd_pixel(pwi, x2 + dx, y2 + dy, pci);

		if (dd < 0) {
			dd += (dx - dy)*2 + 5; /* SE */
			dx++;
		} else
			dd += 3 - dy*2;	       /* S */
		dy--;
	}

	/* Draw left and right vertical lines (dy == 0) */
	lcd_rect(pwi, x1 - dx, y1, x1 - dx, y2, pci);
	lcd_rect(pwi, x2 + dx, y1, x2 + dx, y2, pci);
}


/* Draw filled rectangle from (x1, y1) to (x2, y2) using rounded corners with
   radius r in given color. Call with x1=x-r, y1=y-r, x2=x+r, y2=y+r to draw a
   filled circle at (x, y) with radius r. The algorithm is the same as
   explained above at lcd_rframe(), however we can skip some tests as we
   always draw a full line from the left to the right of the circle. As
   clipping is done in lcd_rect(), we don't care about clipping here. */
void lcd_rrect(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2,
	       XYPOS r, const colinfo_t *pci)
{
	XYPOS dx, dy, dd, maxr;

	if (r < 0)
		return;

	/* Check for the maximum possible radius for these coordinates */
	maxr = x2-x1;
	if (maxr > y2-y1)
		maxr = y2-y1;
	maxr = maxr/2;
	if (r > maxr)
		r = maxr;

	/* Move coordinates to the centers of the quarter circle centers */
	x1 += r;
	y1 += r;
	x2 -= r;
	y2 -= r;

	/* Initialize midpoint values */
	dx = 0;
	dy = r;
	dd = 1-r;

	/* Draw part with low slope (every step changes dx, sometimes dy) */
	while (dy > dx) {
		lcd_rect(pwi, x1 - dx, y1 - dy, x2 + dx, y1 - dy, pci);
		lcd_rect(pwi, x1 - dx, y2 + dy, x2 + dx, y2 + dy, pci);
		if ((dd < 0) && (dy > dx + 1))
			dd += 2*dx + 3;	       /* E */
		else {
			dd += (dx - dy)*2 + 5; /* SE */
			dy--;
		}
		dx++;
	}

	/* Switch to high slope */
	dd = dd - dx - dy;

	/* Draw part with high slope (every step changes dym sometimes dx) */
	while (dy > 0) {
		lcd_rect(pwi, x1 - dx, y1 - dy, x2 + dx, y1 - dy, pci);
		lcd_rect(pwi, x1 - dx, y2 + dy, x2 + dx, y2 + dy, pci);
		if (dd < 0) {
			dd += (dx - dy)*2 + 5; /* SE */
			dx++;
		} else
			dd += 3 - dy*2;	       /* S */
		dy--;
	}

	/* Draw final vertical middle part (dy == 0) */
	lcd_rect(pwi, x1 - dx, y1, x2 + dx, y2, pci);
}
#endif /* CONFIG_XLCD_DRAW & XLCD_DRAW_CIRC */


#if CONFIG_XLCD_DRAW & (XLCD_DRAW_TEXT | XLCD_DRAW_PROG)
/* Draw text string s at (x, y) with alignment/attribute a and colors fg/bg
   the attributes are as follows:
     Bit 1..0: horizontal refpoint:  00: left, 01: hcenter,
		10: right, 11: right+1
     Bit 3..2: vertical refpoint: 00: top, 01: vcenter,
		10: bottom, 11: bottom+1
     Bit 5..4: character width: 00: normal (1x), 01: double (2x),
		10: triple (3x), 11: quadruple (4x)
     Bit 7..6: character height: 00: normal (1x), 01: double (2x),
		10: triple (3x), 11: quadruple (4x)
     Bit 8:    0: normal, 1: bold
     Bit 9:    0: normal, 1: inverse
     Bit 10:   0: normal, 1: underline
     Bit 11:   0: normal, 1: strike-through
     Bit 12:   0: FG+BG, 1: only FG (BG transparent)

   We only draw fully visible characters. If a character would be fully or
   partly outside of the framebuffer, it is not drawn at all. If you need
   partly visible characters, use a larger framebuffer and show only the part
   with the partly visible characters in a window. */
void lcd_text(const wininfo_t *pwi, XYPOS x, XYPOS y, char *s,
	      const colinfo_t *pci_fg, const colinfo_t *pci_bg)
{
	XYPOS len = (XYPOS)strlen(s);
	XYPOS width = VIDEO_FONT_WIDTH;
	XYPOS height = VIDEO_FONT_HEIGHT;
	XYPOS xmin = pwi->clip_left;
	XYPOS ymin = pwi->clip_top;
	XYPOS xmax = pwi->clip_right+1;
	XYPOS ymax = pwi->clip_bottom+1;
	u_int attr = pwi->attr;

	/* Return if string is empty */
	if (s == 0)
		return;

	/* Apply multiple width and multiple height */
	width *= ((attr & ATTR_HS_MASK) >> 4) + 1;
	height *= ((attr & ATTR_VS_MASK) >> 6) + 1;

	/* Compute y from vertical alignment */
	switch (attr & ATTR_VMASK) {
	case ATTR_VTOP:
		break;

	case ATTR_VCENTER:
		y -= height/2;
		break;

	case ATTR_VBOTTOM:
		y++;
		/* Fall through to case ATTR_VBOTTOM1 */

	case ATTR_VBOTTOM1:
		y -= height;
		break;
	}

	/* Return if text is completely or partly above or below framebuffer */
	if ((y < ymin) || (y + height > ymax))
		return;

	/* Compute x from horizontal alignment */
	switch (attr & ATTR_HMASK) {
	case ATTR_HLEFT:
		break;

	case ATTR_HCENTER:
		x -= len*width/2;
		break;

	case ATTR_HRIGHT:
		x++;
		/* Fall through to ATTR_HRIGHT1 */

	case ATTR_HRIGHT1:
		x -= len*width;
		break;
	}

	/* Return if text is completely right of framebuffer or if only the
	   first character would be partly inside of the framebuffer */
	if (x + width > xmax)
		return;

	if (x < xmin) {
		/* Compute number of characters left of framebuffer */
		unsigned offs = (xmin - x - 1)/width + 1;

		/* Return if string would be completeley left of framebuffer */
		if (offs >= len)
			return;

		/* Increase x and string position */
		s += offs;
		x += offs*width;
	}

	/* At least one character is within the framebuffer */
	for (;;) {
		char c = *s++;

		/* Stop on end of string or if character would not fit into
		   framebuffer anymore */
		if (!c || (x + width > xmax))
			break;

		/* Output character and move position */
#if defined(CONFIG_CMD_DRAW) && defined(CONFIG_CMD_ADRAW)
		if (attr & ATTR_ALPHA)
			adraw_ll_char(pwi, x, y, c, pci_fg, pci_bg);
		else
			draw_ll_char(pwi, x, y, c, pci_fg->col, pci_bg->col);
#elif defined(CONFIG_CMD_DRAW)
		draw_ll_char(pwi, x, y, c, pci_fg->col, pci_bg->col);
#elif defined(CONFIG_CMD_ADRAW)
		adraw_ll_char(pwi, x, y, c, pci_fg, pci_bg);
#endif
		x += width;
	}
}
#endif


#if CONFIG_XLCD_DRAW & XLCD_DRAW_TURTLE
/* Draw turtle graphics until end of string, closing bracket or error */
static int lcd_turtle(const wininfo_t *pwi, XYPOS *px, XYPOS *py, char *s,
		      u_int count)
{
	XYPOS x = *px;
	XYPOS y = *py;
	u_char flagB = 0;
	u_char flagN = 0;
	char *s_new;
	int param;
	char c;
	XYPOS nx;
	XYPOS ny;
	int i = 0;
	char *errmsg = NULL;

	do {
		c = toupper(s[i++]);
		if (c == 'B')		  /* "Blank" prefix */
			flagB = 1;
		else if (c == 'N')	  /* "No-update" prefix */
			flagN = 1;
		else if (c == 0) {	  /* End of string */
			if (!count) {
				i--;
				goto DONE;
			}
			errmsg = "Missing ']'";
		} else if (c == ']') {	  /* End of repeat */
			if (!count)
				errmsg = "Invalid ']'";
			else if (!--count)
				goto DONE;/* Loop completed */
			i = 0;		  /* Next repeat iteration */
		} else if (c == '(') {	  /* Call address */
			u_int addr;
			int ret;

			addr = simple_strtoul(s+i, &s_new, 16);
			if (!s_new)
				errmsg = "Missing address";
			else {
				i = s_new - s;
				if (s[i++] != ')')
					errmsg = "Missing ')'";
				ret = lcd_turtle(pwi, &x, &y, (char *)addr, 0);
				if (ret < 0) {
					printf(" in substring at 0x%08x\n",
					       addr);
					errmsg = "called";
				}
			}
		} else {
			/* Parse number; if no number found, use 1 */
			param = (XYPOS)simple_strtol(s+i, &s_new, 0);
			if (s_new)
				i = s_new - s;
			else
				param = 0;
			nx = x;
			ny = y;
			if ((c != 'M') && (param == 0))
				param = 1;

			if (c == '[') {
				int ret = lcd_turtle(pwi, &x, &y, s+i, param);

				if (ret < 0)
					return -1;
				i += ret;
				continue;
			}
			switch (c) {
			case 'E':
				ny -= param;
				/* Fall through to case 'R' */
			case 'R':
				nx += param;
				break;

			case 'F':
				nx += param;
				/* Fall through to case 'D' */
			case 'D':
				ny += param;
				break;

			case 'G':
				ny += param;
				/* Fall through to case 'L' */
			case 'L':
				nx -= param;
				break;

			case 'H':
				nx -= param;
				/* Fall through to case 'U' */
			case 'U':
				ny -= param;
				break;

			case 'M':
				if (s[i++] != ',') {
					errmsg = "Missing ','";
					break;
				}
				nx += param;
				param = (XYPOS)simple_strtol(s+i, &s_new, 0);
				if (s_new)
					i = s_new - s;
				else
					param = 0;
				ny += param;
				break;

			default:
				errmsg = "Unknown turtle command";
				break;
			}

			if (!errmsg) {
				if (!flagB)
					lcd_line(pwi, x, y, nx, ny, &pwi->fg);
				if (!flagN) {
					x = nx;
					y = ny;
				}
				flagB = 0;
				flagN = 0;
			}
		}
	} while (!errmsg);

	/* Handle error message */
	i--;
	printf("%s at offset %i\n", errmsg, i);

DONE:
	*px = x;
	*py = y;
	return i;
}
#endif /* CONFIG_XLCD_DRAW & XLCD_DRAW_TURTLE */


#if CONFIG_XLCD_DRAW & XLCD_DRAW_FILL
/* Fill clipping region with given color */
void lcd_fill(const wininfo_t *pwi, const colinfo_t *pci)
{
	XYPOS x1, y1, x2, y2;

	/* Move from clipping region coordinates to absolute coordinates */
	x1 = pwi->clip_left;
	y1 = pwi->clip_top;
	x2 = pwi->clip_right;
	y2 = pwi->clip_bottom;

#if defined(CONFIG_CMD_DRAW) && defined(CONFIG_CMD_ADRAW)
	if (pwi->attr & ATTR_ALPHA)
		adraw_ll_rect(pwi, x1, y1, x2, y2, pci);
	else
		draw_ll_rect(pwi, x1, y1, x2, y2, pci->col);
#elif defined(CONFIG_CMD_DRAW)
	draw_ll_rect(pwi, x1, y1, x2, y2, pci->col);
#elif defined(CONFIG_CMD_ADRAW)
	adraw_ll_rect(pwi, x1, y1, x2, y2, pci);
#endif
}
#endif


#if CONFIG_XLCD_DRAW & XLCD_DRAW_PROG
/* Draw progress bar; pwi->attr is either 0 or ATTR_ALPHA */
static void lcd_progbar(wininfo_t *pwi)
{
	XYPOS x1, y1, x2, y2, x;
	u_int attr;

	x1 = pwi->pbi.x1;
	y1 = pwi->pbi.y1;
	x2 = pwi->pbi.x2;
	y2 = pwi->pbi.y2;
	x = ((x2 - x1 + 1) * pwi->pbi.prog + 50) / 100 + x1;
	if (x > x1) {
		/* Draw progress bar in FG color */
		lcd_rect(pwi, x1, y1, x-1, y2, &pwi->pbi.rect_fg);
	}
	if (x <= x2) {
		/* Draw remaining part of rectangle with BG color */
		lcd_rect(pwi, x, y1, x2, y2, &pwi->pbi.rect_bg);
	}

	/* Draw percentage unless attribute ATTR_NO_TEXT is set */
	attr = pwi->pbi.attr;
	if ((attr & ATTR_VMASK) != ATTR_NO_TEXT) {
		char s[5];		  /* "0%" .. "100%" */

		/* Depending on reference point, compute alignment */
		switch (attr & ATTR_HMASK) {
		default:
			break;

		case ATTR_HCENTER:
			x1 = (x1 + x2)/2;
			break;

		case ATTR_HRIGHT:
			x1 = x2;
			break;

		case ATTR_HFOLLOW:
			x1 = x+2;
			attr = (attr & ~ATTR_HMASK) | ATTR_HLEFT;
			break;
		}

		switch (attr & ATTR_VMASK) {
		default:
			break;

		case ATTR_VCENTER:
			y1 = (y1 + y2)/2;
			break;

		case ATTR_VBOTTOM:
			y1 = y2;
			break;
		}
		pwi->attr |= attr;	  /* Add text attributes */

		/* Draw percentage text */
		sprintf(s, "%d%%", pwi->pbi.prog);
		lcd_text(pwi, x1, y1, s, &pwi->pbi.text_fg, &pwi->pbi.text_bg);
	}
}
#endif /* CONFIG_XLCD_DRAW & XLCD_DRAW_PROG */


/************************************************************************/
/* BITMAPS								*/
/************************************************************************/

#if CONFIG_XLCD_DRAW & XLCD_DRAW_BITMAP

/* Bitmap color types */
static const char * const ctype_tab[] = {
	"(unknown)",			  /* CT_UNKNOWN */
	"palette",			  /* CT_PALETTE */
	"grayscale",			  /* CT_GRAY */
	"grayscale+alpha",		  /* CT_GRAY_ALPHA */
	"truecolor",			  /* CT_TRUECOL */
	"truecolor+alpha",		  /* CT_TRUECOL_ALPHA */
};

const struct bmtype {
	const char *name;
	u_long (*scan_bm)(u_long addr);
	int (*get_bminfo)(bminfo_t *pbi, u_long addr);
	const char *(*draw_bm)(imginfo_t *pii, u_long addr);
} bmtype_tab[] = {
	[BT_UNKNOWN] = {"???", NULL,     NULL,           NULL},
#ifdef CONFIG_XLCD_PNG
	[BT_PNG] =     {"PNG", scan_png, get_bminfo_png, draw_png},
#endif
#ifdef CONFIG_XLCD_BMP
	[BT_BMP] =     {"BMP", scan_bmp, get_bminfo_bmp, draw_bmp},
#endif
#ifdef CONFIG_XLCD_JPG
	[BT_JPG] =     {"JPG", scan_jpg, get_bminfo_jpg, draw_jpg},
#endif
};


/* Get bitmap information; should only be called if bitmap integrity is OK,
   i.e. after lcd_scan_bitmap() was successful */
static void lcd_get_bminfo(bminfo_t *pbi, u_long addr)
{
	int i;

	for (i = 1; i < ARRAYSIZE(bmtype_tab); i++) {
		if (bmtype_tab[i].get_bminfo(pbi, addr))
			break;
	}

	if (i >= ARRAYSIZE(bmtype_tab)) {
		/* Unknown format */
		pbi->type = BT_UNKNOWN;
		pbi->colortype = CT_UNKNOWN;
		pbi->bitdepth = 0;
		pbi->flags = 0;
		pbi->hres = 0;
		pbi->vres = 0;
	}

	/* Load bitmap type and color type string */
	pbi->bm_name = bmtype_tab[pbi->type].name;
	pbi->ct_name = ctype_tab[pbi->colortype];
}
	

/* Scan bitmap (check integrity) at addr and return end address */
static u_long lcd_scan_bitmap(u_long addr)
{
	u_long newaddr;
	int i;

	for (i = 1; i < ARRAYSIZE(bmtype_tab); i++) {
		newaddr = bmtype_tab[i].scan_bm(addr);
		if (newaddr)
			return newaddr;
	}
	
	return 0;			  /* Unknown bitmap type */
}


/* Draw bitmap from address addr at (x, y) with alignment/attribute a */
const char *lcd_bitmap(const wininfo_t *pwi, XYPOS x, XYPOS y, u_long addr)
{
	imginfo_t ii;
	XYPOS hres, vres;
	XYPOS xmin, ymin, xmax, ymax;
	int xpos;
	u_int attr;

	/* Do a quick scan if bitmap integrity is OK */
	if (!lcd_scan_bitmap(addr))
		return "Unknown bitmap type\n";

	/* Get bitmap info */
	lcd_get_bminfo(&ii.bi, addr);
	if (ii.bi.colortype == CT_UNKNOWN)
		return "Invalid bitmap color type\n";

	/* Prepare attribute */
	attr = pwi->attr;
	ii.applyalpha = ((attr & ATTR_ALPHA) != 0);
	ii.multiwidth = ((attr & ATTR_HS_MASK) >> 4) + 1;
	ii.multiheight = ((attr & ATTR_VS_MASK) >> 6) + 1;

	/* Apply double width and double height */
	hres = ii.bi.hres * ii.multiwidth;
	vres = ii.bi.vres * ii.multiheight;

	switch (attr & ATTR_HMASK) {
	case ATTR_HLEFT:
		break;

	case ATTR_HCENTER:
		x -= hres/2;
		break;

	case ATTR_HRIGHT:
		x++;
		/* Fall through to case ATTR_HRIGHT1 */

	case ATTR_HRIGHT1:
		x -= hres;
		break;
	}

	/* Apply vertical alignment */
	switch (attr & ATTR_VMASK) {
	case ATTR_VTOP:
		break;

	case ATTR_VCENTER:
		y -= vres/2;
		break;

	case ATTR_VBOTTOM:
		y++;
		/* Fall through to case ATTR_VBOTTOM1 */

	case ATTR_VBOTTOM1:
		y -= vres;
		break;
	}

	/* Apply horizontal alignment */
	xmin = pwi->clip_left;
	xmax = pwi->clip_right + 1;
	ymin = pwi->clip_top;
	ymax = pwi->clip_bottom + 1;

	/* Return if image is completely outside of framebuffer */
	if ((x >= xmax) || (x+hres <= xmin) || (y >= xmax) || (y+vres <= xmin))
		return NULL;

	/* Compute end pixel in this row */
	ii.xend = hres;
	if (ii.xend + x > xmax)
		ii.xend = xmax - x;

	/* xpix counts the bitmap columns from 0 to xend; however if x < 0, we
	   start at the appropriate offset. */
	ii.xpix = 0;
	if (x < 0) {
		ii.xpix = -x;		  /* We start at an offset */
		x = 0;			  /* Current row position (pixels) */
	}

	ii.pwi = pwi;
	ii.bpp = 1 << pwi->ppi->bpp_shift;
	xpos = x * ii.bpp;
	ii.fbuf = pwi->pfbuf[pwi->fbdraw] + ((xpos >> 5) << 2);
	ii.shift = 32 - (xpos & 31);
	ii.mask = (1 << ii.bpp) - 1;	  /* This also works for bpp==32! */
	ii.trans_rgba = 0x000000FF;	  /* No transparent color set yet */
	ii.hash_rgba = 0x000000FF;	  /* Preload hash color */
	ii.hash_col = pwi->ppi->rgba2col(pwi, 0x000000FF);

	ii.ypix = 0;
	ii.yend = vres;
	ii.y = y;
	if (ii.yend + y > ymax)
		ii.yend = ymax - y;

	/* Actually draw the bitmap */
	return bmtype_tab[ii.bi.type].draw_bm(&ii, addr);
}

#endif /* CONFIG_XLCD_DRAW & XLCD_DRAW_BITMAP */


/************************************************************************/
/* TEST PATTERNS (not possible with CONFIG_CMD_ADRAW alone)		*/
/************************************************************************/

#if (CONFIG_XLCD_DRAW & XLCD_DRAW_TEST) && defined(CONFIG_CMD_DRAW)

#if CONFIG_XLCD_TEST & XLCD_TEST_GRID
/* Draw test pattern with grid, basic colors, color gradients and circles */
static void lcd_test_grid(const wininfo_t *pwi,
			  XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2)
{
	const pixinfo_t *ppi = pwi->ppi;
	XYPOS dx, dy;
	XYPOS x, y;
	XYPOS i;
	XYPOS hleft, vtop, hright, vbottom;
	XYPOS r1, r2, scale;
	XYPOS hres, vres;
	COLOR32 col;
	colinfo_t ci;

	static const RGBA const coltab[] = {
		0xFF0000FF,		  /* R */
		0x00FF00FF,		  /* G */
		0x0000FFFF,		  /* B */
		0xFFFFFFFF,		  /* W */
		0xFFFF00FF,		  /* Y */
		0xFF00FFFF,		  /* M */
		0x00FFFFFF,		  /* C */
	};

	/* Use hres divided by 12 and vres divided by 8 as grid size */
	hres = x2-x1+1;
	vres = y2-y1+1;
	dx = hres/12;
	dy = vres/8;

	/* Compute left and top margin for first line as half of the remaining
	   space (that was not multiple of 12 or 8 respectively) and half of
	   a grid rectangle size */
	hleft = (dx + hres % 12)/2 + x1;
	vtop = (dy + vres % 8)/2 + y1;

	/* Compute right and bottom margin for last line in a similar way */
	hright = hleft + (12-1)*dx;
	vbottom = vtop + (8-1)*dy;

	/* Draw lines and circles in white; the circle command needs a colinfo
	   structure for the color; however we know that ATTR_ALPHA is cleared
	   so it is enough to set the col entry of this structure. */
	col = ppi->rgba2col(pwi, 0xFFFFFFFF);  /* White */
	ci.col = col;

	/* Draw vertical lines of grid */
	for (x = hleft; x <= hright; x += dx)
		draw_ll_rect(pwi, x, y1, x, y2, col);

	/* Draw horizontal lines of grid */
	for (y = vtop; y <= vbottom; y += dy)
		draw_ll_rect(pwi, x1, y, x2, y, col);

	/* Draw 7 of the 8 basic colors (without black) as rectangles */
	x = 2*dx + hleft + 1;
	for (i=0; i<7; i++) {
		draw_ll_rect(pwi, x, vbottom-2*dy+1, x+dx-2, vbottom-1,
			     ppi->rgba2col(pwi, coltab[6-i]));
		draw_ll_rect(pwi, x, vtop+1, x+dx-2, vtop+2*dy-1,
			     ppi->rgba2col(pwi, coltab[i]));
		x += dx;
	}

	/* Draw grayscale gradient on left, R, G, B gradient on right side */
	scale = vbottom-vtop-2;
	y = vtop+1;
	for (i=0; i<=scale; i++) {
		RGBA rgba;

		rgba = (i*255/scale) << 8;
		rgba |= (rgba << 8) | (rgba << 16) | 0xFF;
		draw_ll_rect(pwi, hleft+1, y, hleft+dx-1, y,
			     ppi->rgba2col(pwi, rgba));
		draw_ll_rect(pwi, hright-dx+1, y, hright-2*dx/3, y,
			     ppi->rgba2col(pwi, rgba & 0xFF0000FF));
		draw_ll_rect(pwi, hright-2*dx/3+1, y, hright-dx/3, y,
			     ppi->rgba2col(pwi, rgba & 0x00FF00FF));
		draw_ll_rect(pwi, hright-dx/3+1, y, hright-1, y,
			     ppi->rgba2col(pwi, rgba & 0x0000FFFF));
		y++;
	}

	/* Draw big and small circle; make sure that circle fits on screen */
	if (hres > vres) {
		r1 = vres-1;
		r2 = dy;
	} else {
		r1 = hres-1;
		r2 = dx;
	}
	r1 = r1/2;
	x = hres/2 + x1;
	y = vres/2 + y1;

#if CONFIG_XLCD_DRAW & XLCD_DRAW_CIRC
	/* Draw two circles */
	lcd_rframe(pwi, x-r1, y-r1, x+r1-1, y+r1-1, r1, &ci);
	lcd_rframe(pwi, x-r2, y-r2, x+r2-1, y+r2-1, r2, &ci);
#endif

	/* Draw corners; the window is min. 24x16, so +/-7 will always fit */
	col = ppi->rgba2col(pwi, 0x00FF00FF);  /* Green */
	draw_ll_rect(pwi, x1, y1, x1+7, y1, col); /* top left */
	draw_ll_rect(pwi, x1, y1, x1, y1+7, col);
	draw_ll_rect(pwi, x2-7, y1, x2, y1, col); /* top right */
	draw_ll_rect(pwi, x2, y1, x2, y1+7, col);
	draw_ll_rect(pwi, x1, y2-7, x1, y2, col); /* bottom left */
	draw_ll_rect(pwi, x1, y2, x1+7, y2, col);
	draw_ll_rect(pwi, x2, y2-7, x2, y2, col); /* bottom right */
	draw_ll_rect(pwi, x2-7, y2, x2, y2, col);
}
#endif /* CONFIG_XLCD_TEST & XLCD_TEST_GRID */


#if CONFIG_XLCD_TEST & XLCD_TEST_COLORS
/* Draw the eight basic colors in two rows of four */
static void lcd_test_color(const wininfo_t *pwi,
			   XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2)
{
	const pixinfo_t *ppi = pwi->ppi;
	XYPOS xres_1_4, xres_2_4, xres_3_4;
	XYPOS yres_1_2 = (y1+y2)/2;

	/* Draw red and cyan rectangles in first column */
	xres_1_4 = (3*x1 + x2)/4;	  /* 1/4 hres */
	draw_ll_rect(pwi, x1, y1, xres_1_4, yres_1_2,
		     ppi->rgba2col(pwi, 0xFF0000FF)); /* Red */
	draw_ll_rect(pwi, x1, yres_1_2 + 1, xres_1_4, y2,
		     ppi->rgba2col(pwi, 0x00FFFFFF)); /* Cyan */

	/* Draw green and magenta rectangles in second column */
	xres_1_4++;
	xres_2_4 = (x1 + x2)/2;		  /* 2/4 hres */
	draw_ll_rect(pwi, xres_1_4, y1, xres_2_4, yres_1_2,
		     ppi->rgba2col(pwi, 0x00FF00FF)); /* Green */
	draw_ll_rect(pwi, xres_1_4, yres_1_2 + 1, xres_2_4, y2,
		     ppi->rgba2col(pwi, 0xFF00FFFF)); /* Magenta */

	/* Draw blue and yellow rectangles in third column */
	xres_2_4++;
	xres_3_4 = (x1 + 3*x2)/4;	  /* 3/4 hres */
	draw_ll_rect(pwi, xres_2_4, y1, xres_3_4, yres_1_2,
		     ppi->rgba2col(pwi, 0x0000FFFF)); /* Blue */
	draw_ll_rect(pwi, xres_2_4, yres_1_2 + 1, xres_3_4, y2,
		     ppi->rgba2col(pwi, 0xFFFF00FF)); /* Yellow */

	/* Draw black and white rectangles in fourth column */
	xres_3_4++;
#if 0	/* Drawing black not necessary, window was already cleared black */
	draw_ll_rect(pwi, xres_3_4, y1, x2, yres_1_2,
		     ppi->rgba2col(pwi, 0x000000FF)); /* Black */
#endif
	draw_ll_rect(pwi, xres_3_4, yres_1_2 + 1, x2, y2,
		     ppi->rgba2col(pwi, 0xFFFFFFFF)); /* White */
}
#endif /* CONFIG_XLCD_TEST & XLCD_TEST_COLOR */


#if CONFIG_XLCD_TEST & XLCD_TEST_D2B
/* Draw color gradient, horizontal: hue, vertical: brightness */
static void lcd_test_d2b(const wininfo_t *pwi,
			 XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2)
{
	const pixinfo_t *ppi = pwi->ppi;
	int xres = x2 - x1;
	int yres_1_2 = (y2 - y1)/2 + 1 + y1;
	int xfrom = x1;
	int hue = 0;
	struct iRGB {			  /* Color structure with int */
		int R;
		int G;
		int B;
	};
	static const struct iRGB const target[] = {
		{0xFF, 0x00, 0x00},	  /* R */
		{0xFF, 0xFF, 0x00},	  /* Y */
		{0x00, 0xFF, 0x00},	  /* G */
		{0x00, 0xFF, 0xFF},	  /* C */
		{0x00, 0x00, 0xFF},	  /* B */
		{0xFF, 0x00, 0xFF},	  /* M */
		{0xFF, 0x00, 0x00}	  /* R */
	};

	do {
		struct iRGB from = target[hue++];
		struct iRGB to = target[hue];
		int xto = hue * xres / 6 + 1 + x1;
		int dx = xto - xfrom;
		int x;

		for (x = xfrom; x < xto; x++) {
			int sx = x - xfrom;
			int dy, y;
			struct iRGB temp;
			RGBA rgba;

			temp.R = (to.R - from.R)*sx/dx + from.R;
			temp.G = (to.G - from.G)*sx/dx + from.G;
			temp.B = (to.B - from.B)*sx/dx + from.B;

			dy = yres_1_2 - y1;
			for (y = y1; y < yres_1_2; y++) {
				int sy = y - y1;

				rgba = (temp.R * sy/dy) << 24;
				rgba |= (temp.G * sy/dy) << 16;
				rgba |= (temp.B * sy/dy) << 8;
				rgba |= 0xFF;
				draw_ll_pixel(pwi, (XYPOS)x, (XYPOS)y,
					      ppi->rgba2col(pwi, rgba));
			}

			dy = y2 - yres_1_2;
			for (y = yres_1_2; y <= y2; y++) {
				int sy = y - yres_1_2;

				rgba = ((0xFF-temp.R)*sy/dy + temp.R) << 24;
				rgba |= ((0xFF-temp.G)*sy/dy + temp.G) << 16;
				rgba |= ((0xFF-temp.B)*sy/dy + temp.B) << 8;
				rgba |= 0xFF;
				draw_ll_pixel(pwi, (XYPOS)x, (XYPOS)y,
					      ppi->rgba2col(pwi, rgba));
			}
			WATCHDOG_RESET();
		}
		xfrom = xto;
	} while (hue < 6);
}
#endif /* CONFIG_XLCD_TEST & XLCD_TEST_D2B */


#if CONFIG_XLCD_TEST & XLCD_TEST_GRAD
/* Draw color gradient: 8 basic colors along edges, gray in the center */
static void lcd_test_grad(const wininfo_t *pwi,
			  XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2)
{
	const pixinfo_t *ppi = pwi->ppi;
	int xres_1_2 = (x2 - x1)/2 + 1 + x1;
	int yres_1_2 = (y2 - y1)/2 + 1 + y1;
	int y;
	struct iRGB {			  /* Color structure with int */
		int R;
		int G;
		int B;
	};
	struct iRGB const tl = {0x00, 0x00, 0x00}; /* Top left: Black */
	struct iRGB const tm = {0x00, 0x00, 0xFF}; /* Top middle: Blue */
	struct iRGB const tr = {0x00, 0xFF, 0xFF}; /* Top right: Cyan */
	struct iRGB const ml = {0xFF, 0x00, 0x00}; /* Middle left: Red */
	struct iRGB const mm = {0x80, 0x80, 0x80}; /* Middle middle: Gray */
	struct iRGB const mr = {0x00, 0xFF, 0x00}; /* Middle right: Green */
	struct iRGB const bl = {0xFF, 0x00, 0xFF}; /* Bottom left: Magenta */
	struct iRGB const bm = {0xFF, 0xFF, 0xFF}; /* Bottom middle: White */
	struct iRGB const br = {0xFF, 0xFF, 0x00}; /* Bottom right: Yellow */

	for (y = y1; y <= y2; y++) {
		struct iRGB l, m, r;
		int x, dx;
		int sy, dy;
		RGBA rgb;

		/* Compute left, middle and right colors for next row */
		if (y < yres_1_2) {
			sy = y - y1;
			dy = yres_1_2 - y1;

			l.R = (ml.R - tl.R)*sy/dy + tl.R;
			l.G = (ml.G - tl.G)*sy/dy + tl.G;
			l.B = (ml.B - tl.B)*sy/dy + tl.B;

			m.R = (mm.R - tm.R)*sy/dy + tm.R;
			m.G = (mm.G - tm.G)*sy/dy + tm.G;
			m.B = (mm.B - tm.B)*sy/dy + tm.B;

			r.R = (mr.R - tr.R)*sy/dy + tr.R;
			r.G = (mr.G - tr.G)*sy/dy + tr.G;
			r.B = (mr.B - tr.B)*sy/dy + tr.B;
		} else {
			sy = y - yres_1_2;
			dy = y2 - yres_1_2;

			l.R = (bl.R - ml.R)*sy/dy + ml.R;
			l.G = (bl.G - ml.G)*sy/dy + ml.G;
			l.B = (bl.B - ml.B)*sy/dy + ml.B;

			m.R = (bm.R - mm.R)*sy/dy + mm.R;
			m.G = (bm.G - mm.G)*sy/dy + mm.G;
			m.B = (bm.B - mm.B)*sy/dy + mm.B;

			r.R = (br.R - mr.R)*sy/dy + mr.R;
			r.G = (br.G - mr.G)*sy/dy + mr.G;
			r.B = (br.B - mr.B)*sy/dy + mr.B;
		}

		/* Draw left half of row */
		dx = xres_1_2 - x1;
		for (x = x1; x < xres_1_2; x++) {
			int sx = x - x1;

			rgb = ((m.R - l.R)*sx/dx + l.R) << 24;
			rgb |= ((m.G - l.G)*sx/dx + l.G) << 16;
			rgb |= ((m.B - l.B)*sx/dx + l.B) << 8;
			rgb |= 0xFF;

			draw_ll_pixel(pwi, (XYPOS)x, (XYPOS)y,
				      ppi->rgba2col(pwi, rgb));
		}

		/* Draw right half of row */
		dx = x2 - xres_1_2;
		for (x = xres_1_2; x <= x2; x++) {
			int sx = x - xres_1_2;

			rgb = ((r.R - m.R)*sx/dx + m.R) << 24;
			rgb |= ((r.G - m.G)*sx/dx + m.G) << 16;
			rgb |= ((r.B - m.B)*sx/dx + m.B) << 8;
			rgb |= 0xFF;

			draw_ll_pixel(pwi, (XYPOS)x, (XYPOS)y,
				      ppi->rgba2col(pwi, rgb));
		}
		WATCHDOG_RESET();
	}
}
#endif /* CONFIG_XLCD_TEST & XLCD_TEST_GRAD */


struct testinfo {
	char *kw;
	XYPOS minhres;
	XYPOS minvres;
	void (*draw_ll_pattern)(const wininfo_t *pwi,
				XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2);
};

#define TEST_GRID_NAME "grid"
#define TEST_COLORS_NAME "colors"
#define TEST_D2B_NAME "d2b"
#define TEST_GRAD_NAME "grad"

/* Array of available test patterns */
const struct testinfo ti[] = {
#if CONFIG_XLCD_TEST & XLCD_TEST_GRID
	{TEST_GRID_NAME, 24, 16, lcd_test_grid},
#endif
#if CONFIG_XLCD_TEST & XLCD_TEST_COLORS
	{TEST_COLORS_NAME, 4,  2, lcd_test_color},
#endif
#if CONFIG_XLCD_TEST & XLCD_TEST_D2B
	{TEST_D2B_NAME, 6,  3, lcd_test_d2b},
#endif
#if CONFIG_XLCD_TEST & XLCD_TEST_GRAD
	{TEST_GRAD_NAME, 6,  3, lcd_test_grad},
#endif
};

/* Draw test pattern */
static int lcd_test(const wininfo_t *pwi, u_int pattern)
{
	XYPOS x1 = pwi->clip_left;
	XYPOS y1 = pwi->clip_top;
	XYPOS x2 = pwi->clip_right;
	XYPOS y2 = pwi->clip_bottom;
	const struct testinfo *pti;

	/* Get info to the given pattern */
	if (pattern > ARRAYSIZE(ti))
		pattern = 0;
	pti = &ti[pattern];

	/* Return with error if window is too small */
	if ((x2-x1+1 < pti->minhres) || (y2-y1+1 < pti->minvres))
		return 1;

	/* Clear window in black */
	draw_ll_rect(pwi, x1, y1, x2, y2, pwi->ppi->rgba2col(pwi, 0x000000FF));

	/* Call lowlevel drawing function for pattern */
	pti->draw_ll_pattern(pwi, x1, y1, x2, y2);

	return 0;
}
#endif /* (CONFIG_XLCD_DRAW & XLCD_DRAW_TEST) && defined(CONFIG_CMD_DRAW) */


/************************************************************************/
/* Command draw								*/
/************************************************************************/

/* Handle draw command */
static int do_draw(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	wininfo_t *pwi;
	const vidinfo_t *pvi;
	u_short sc;
	XYPOS x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	RGBA rgba1, rgba2;
	u_char coord_pairs, colindex;

	pvi = lcd_get_sel_vidinfo_p();
	pwi = lcd_get_wininfo_p(pvi, pvi->win_sel);

	/* Without arguments, print current drawing parameters */
	if (argc < 2) {
		printf("%s:\n", pwi->name);
		printf("color:\tFG #%08x (#%08x), BG #%08x (#%08x)\n",
		       pwi->fg.rgba, pwi->ppi->col2rgba(pwi, pwi->fg.col),
		       pwi->bg.rgba, pwi->ppi->col2rgba(pwi, pwi->bg.col));
		printf("clip:\t(%d, %d) - (%d, %d)\n", pwi->clip_left,
		       pwi->clip_top, pwi->clip_right, pwi->clip_bottom);
		printf("origin:\t(%d, %d)\n", pwi->horigin, pwi->vorigin);
		printf("pbr:\t(%d, %d) - (%d, %d), FG #%08x, BG #%08x\n",
		       pwi->pbi.x1, pwi->pbi.y1, pwi->pbi.x2, pwi->pbi.y2,
		       pwi->pbi.rect_fg.col, pwi->pbi.rect_bg.col);
		printf("pbt:\tattr 0x%08x, FG #%08x, BG #%08x\n", pwi->pbi.attr,
		       pwi->pbi.text_fg.col, pwi->pbi.text_bg.col);
		printf("prog:\t%u\n", pwi->pbi.prog);
		return 0;
	}

#if defined(CONFIG_CMD_DRAW) && defined(CONFIG_CMD_ADRAW)
	/* Set "apply alpha" attribute if command was "adraw" */
	pwi->attr = (argv[0][0] == 'a') ? ATTR_ALPHA : 0;
#elif defined(CONFIG_CMD_ADRAW)
	/* All draw commands are "adraw" commands */
	pwi->attr = ATTR_ALPHA;
#endif

	/* Search for keyword in draw keyword list */
	sc = parse_sc(argc, argv[1], DI_HELP, draw_kw, ARRAYSIZE(draw_kw));

	/* Print usage if command not valid */
	if (sc == DI_HELP) {
		cmd_usage(cmdtp);
		return 1;
	}

	/* If selected window is not active do nothing */
	if (!pwi->active) {
		//PRINT_WIN(vid_sel, pvi->win_sel);
		printf("Selected %s is not active\n", pwi->name);
		return 1;
	}

	/* Parse one or two coordinate pairs for commands with coordinates */
	coord_pairs = draw_kw[sc].info1;
	if (coord_pairs > 0) {
#ifdef CONFIG_XLCD_EXPR
		if (parse_expr(pwi, argv[2], &x1)
		    || parse_expr(pwi, argv[3], &y1))
			return 1;
#else
		x1 = (XYPOS)simple_strtol(argv[2], NULL, 0);
		y1 = (XYPOS)simple_strtol(argv[3], NULL, 0);
#endif
		x1 += pwi->horigin;
		y1 += pwi->vorigin;

		if (coord_pairs > 1) {
#ifdef CONFIG_XLCD_EXPR
			if (parse_expr(pwi, argv[4], &x2)
			    || parse_expr(pwi, argv[5], &y2))
				return 1;
#else
		x2 = (XYPOS)simple_strtol(argv[4], NULL, 0);
		y2 = (XYPOS)simple_strtol(argv[5], NULL, 0);
#endif
			x2 += pwi->horigin;
			y2 += pwi->vorigin;

#if CONFIG_XLCD_DRAW & XLCD_DRAW_LINE
			if (sc != DI_LINE)
#endif
			{
				XYPOS tmp;

				/* Sort coordinates unless drawing a line */
				if (x1 > x2) {
					tmp = x1;
					x1 = x2;
					x2 = tmp;
				}
				if (y1 > y2) {
					tmp = y1;
					y1 = y2;
					y2 = tmp;
				}
			}
		}
	}

	/* Parse one or two optional colors for commands who may have colors */
	colindex = draw_kw[sc].info2 + 2;
	rgba1 = pwi->fg.rgba;
	rgba2 = pwi->bg.rgba;
	if (argc > colindex) {
		/* Parse first color */
		if (parse_rgb(argv[colindex], &rgba1))
			return 1;
		colindex++;
		if (argc > colindex) {
			/* Parse second color */
			if (parse_rgb(argv[colindex], &rgba2))
				return 1;
		}
	}

	/* Finally execute the drawing command */
	switch (sc) {
	case DI_COLOR:			  /* Set FG and BG color */
		lcd_set_fg(pwi, rgba1);
		lcd_set_bg(pwi, rgba2);
		break;

#if CONFIG_XLCD_DRAW & XLCD_DRAW_PIXEL
	case DI_PIXEL:			  /* Draw pixel */
		lcd_set_fg(pwi, rgba1);
		lcd_pixel(pwi, x1, y1, &pwi->fg);
		break;
#endif

#if CONFIG_XLCD_DRAW & XLCD_DRAW_LINE
	case DI_LINE:			  /* Draw line */
		lcd_set_fg(pwi, rgba1);
		lcd_line(pwi, x1, y1, x2, y2, &pwi->fg);
		break;
#endif

#if CONFIG_XLCD_DRAW & XLCD_DRAW_RECT
	case DI_RECT:			  /* Draw filled rectangle */
	case DI_FRAME:			  /* Draw rectangle outline */
		lcd_set_fg(pwi, rgba1);
		if (sc == DI_RECT) {
			lcd_rect(pwi, x1, y1, x2, y2, &pwi->fg);
			if ((argc < 8) || (rgba1 == rgba2))
				break;
			lcd_set_fg(pwi, rgba2);
		}
		lcd_frame(pwi, x1, y1, x2, y2, &pwi->fg);
		break;
#endif

#if CONFIG_XLCD_DRAW & XLCD_DRAW_CIRC
	case DI_RRECT:			  /* Draw rounded filled rectangle */
	case DI_RFRAME: {		  /* Draw rounded rectangle outline */
		XYPOS r;

		r = (XYPOS)simple_strtol(argv[6], NULL, 0); /* Parse radius */
		lcd_set_fg(pwi, rgba1);
		if (sc == DI_RRECT) {
			lcd_rrect(pwi, x1, y1, x2, y2, r, &pwi->fg);
			if ((argc < 9) || (rgba1 == rgba2))
				break;
			lcd_set_fg(pwi, rgba2);
		}
		lcd_rframe(pwi, x1, y1, x2, y2, r, &pwi->fg);
		break;
	}

	case DI_CIRCLE:			  /* Draw circle outline */
	case DI_DISC: {			  /* Draw filled circle */
		XYPOS r;

		r = (XYPOS)simple_strtol(argv[4], NULL, 0); /* Parse radius */
		lcd_set_fg(pwi, rgba1);
		if (sc == DI_DISC) {
			lcd_rrect(pwi, x1-r, y1-r, x1+r, y1+r, r, &pwi->fg);
			if ((argc < 7) || (rgba1 == rgba2))
				break;
			lcd_set_fg(pwi, rgba2);
		}
		lcd_rframe(pwi, x1-r, y1-r, x1+r, y1+r, r, &pwi->fg);
		break;
	}
#endif

#if CONFIG_XLCD_DRAW & XLCD_DRAW_TEXT
	case DI_TEXT: {			  /* Draw text */
		u_int a;

		lcd_set_fg(pwi, rgba1);
		lcd_set_bg(pwi, rgba2);

		/* Optional argument 4: attribute */
		if (argc > 5) {
			a = simple_strtoul(argv[5], NULL, 0);
			pwi->text_attr = a;
		} else
			a = pwi->text_attr;
		pwi->attr |= a;
		lcd_text(pwi, x1, y1, argv[4], &pwi->fg, &pwi->bg);
		break;
	}
#endif

#if CONFIG_XLCD_DRAW & XLCD_DRAW_BITMAP
	case DI_BITMAP:	{		  /* Draw bitmap */
		u_int addr;
		u_int a;
		const char *errmsg;

		/* Argument 3: address */
		addr = simple_strtoul(argv[4], NULL, 16);

		/* Optional argument 4: bitmap number (for multi bitmaps) */
		if (argc > 5) {
			u_int n = simple_strtoul(argv[5], NULL, 0);
			u_int i;

			for (i = 0; i < n; i++) {
				addr = lcd_scan_bitmap(addr);
				if (!addr)
					break;
			}

			if (!addr || !lcd_scan_bitmap(addr)) {
				printf("Bitmap %d not found\n", n);
				return 1;
			}
		}

		/* Optional argument 5: attribute */
		a = (argc > 6) ? simple_strtoul(argv[6], NULL, 0) : 0;
		pwi->attr |= a;
		errmsg = lcd_bitmap(pwi, x1, y1, addr);
		if (errmsg) {
			puts(errmsg);
			return 1;
		}
		break;
	}
#endif

#if CONFIG_XLCD_DRAW & XLCD_DRAW_TURTLE
	case DI_TURTLE:			  /* Draw turtle graphics */
		lcd_set_fg(pwi, rgba1);
		if (lcd_turtle(pwi, &x1, &y1, argv[4], 0) < 0)
			puts(" in argument string\n");
		break;
#endif

#if CONFIG_XLCD_DRAW & XLCD_DRAW_FILL
	case DI_FILL:			  /* Fill window with FG color */
		lcd_set_fg(pwi, rgba1);
		lcd_fill(pwi, &pwi->fg);
		break;

	case DI_CLEAR:			  /* Fill window with BG color */
		if ((argc > 2) && (parse_rgb(argv[colindex], &rgba2)))
		    return 1;
		lcd_set_bg(pwi, rgba2);
		lcd_fill(pwi, &pwi->bg);
		break;
#endif

#if CONFIG_XLCD_DRAW & XLCD_DRAW_PROG
	case DI_PBR:			  /* Define progress bar rectangle */
		pwi->pbi.x1 = x1;
		pwi->pbi.x2 = x2;
		pwi->pbi.y1 = y1;
		pwi->pbi.y2 = y2;
		lcd_set_col(pwi, rgba1, &pwi->pbi.rect_fg);
		lcd_set_col(pwi, rgba2, &pwi->pbi.rect_bg);
		break;

	case DI_PBT: {			  /* Define progress bar text params */
		u_int a;

		a = (argc > 2) ? simple_strtoul(argv[2], NULL, 0)
			: (ATTR_HFOLLOW | ATTR_VCENTER);
		pwi->pbi.attr = a;
		lcd_set_col(pwi, rgba1, &pwi->pbi.text_fg);
		lcd_set_col(pwi, rgba2, &pwi->pbi.text_bg);
		break;
	}

	case DI_PROG: {			  /* Draw progress bar */
		u_int percent;

		if (argc > 2) {
			percent = simple_strtoul(argv[2], NULL, 0);
			if (percent > 100)
				percent = 100;
			else if (percent < 0)
				percent = 0;
			pwi->pbi.prog = percent;
		}
		lcd_progbar(pwi);
		break;
	}
#endif

#if (CONFIG_XLCD_DRAW & XLCD_DRAW_TEST) && defined(CONFIG_CMD_DRAW)
	case DI_TEST: {			  /* Draw test pattern */
		u_int pat = 0;

		if (argc > 2) {
			if (isdigit(argv[2][0])) {
				/* Parse pattern number */
				pat = simple_strtoul(argv[2], NULL, 0);
			} else {
				/* Parse pattern name */
				for (pat = 0; pat < ARRAYSIZE(ti); pat++) {
					if (!strcmp(argv[2], ti[pat].kw))
						break;
				}
			}
		}
		pwi->attr = 0;		  /* Clear ATTR_ALPHA if adraw */
		if (lcd_test(pwi, pat)) {
			printf("Window too small\n");
			return 1;
		}
		break;
	}
#endif

	case DI_CLIP:			  /* Set new clipping region */
		if (x1 < 0)
			x1 = 0;
		if (y1 < 0)
			y1 = 0;
		if (x2 >= pwi->fbhres)
			x2 = pwi->fbhres - 1;
		if (y2 >= pwi->fbvres)
			y2 = pwi->fbvres - 1;
		pwi->clip_left = x1;
		pwi->clip_top = y1;
		pwi->clip_right = x2;
		pwi->clip_bottom = y2;
		break;

	case DI_ORIGIN:			  /* Set new origin position */
		pwi->horigin = x1;
		pwi->vorigin = y1;
		break;

	default:			  /* Should not happen */
		printf("Unhandled draw command '%s'\n", argv[1]);
		return 1;
	}

	return 0;
}

#if defined(CONFIG_CMD_DRAW) || defined(CONFIG_CMD_ADRAW)
/* If only CONFIG_CMD_ADRAW and not CONFIG_CMD_DRAW is set, call as "draw" */
U_BOOT_CMD(
	draw, 9, 1, do_draw,
	"draw to selected window",
	"color #rgba [#rgba]\n"
	"    - set FG (and BG) color\n"
#if CONFIG_XLCD_DRAW & XLCD_DRAW_PIXEL
	"draw pixel x y [#rgba]\n"
	"    - draw pixel at (x, y)\n"
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_LINE
	"draw line x1 y1 x2 y2 [#rgba]\n"
	"    - draw line from (x1, y1) to (x2, y2)\n"
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_RECT
	"draw frame x1 y1 x2 y2 [#rgba]\n"
	"    - draw unfilled rectangle from (x1, y1) to (x2, y2)\n"
	"draw rect x1 y1 x2 y2 [#rgba [#rgba]]\n"
	"    - draw filled rectangle (with outline) from (x1, y1) to (x2, y2)\n"
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_CIRC
	"draw rframe x1 y1 x2 y2 r [#rgba]\n"
	"    - draw unfilled rectangle with rounded corners of radius r\n"
	"draw rrect x1 y1 x2 y2 r [#rgba [#rgba]]\n"
	"    - draw unfilled rectangle with rounded corners of radius r\n"
	"draw circle x y r [#rgba]\n"
	"    - draw unfilled circle at (x, y) with radius r\n"
	"draw disc x y r [#rgba [#rgba]]\n"
	"    - draw filled circle (with outline) at (x, y) with radius r\n"
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_TEXT
	"draw text x y string [a [#rgba [#rgba]]]\n"
	"    - draw text string at (x, y) with attribute a\n"
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_BITMAP
	"draw bitmap x y addr [n [a]]\n"
	"    - draw bitmap n from addr at (x, y) with attribute a\n"
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_TURTLE
	"draw turtle x y string [#rgba]\n"
	"    - draw turtle graphic command from string at (x, y)\n"
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_FILL
	"draw fill [#rgba]\n"
	"    - fill window with color\n"
	"draw clear [#rgba]\n"
	"    - clear window with color\n"
#endif
#if CONFIG_XLCD_DRAW & XLCD_DRAW_PROG
	"draw pbr x1 y1 x2 y2 [#rgba [#rgba]]\n"
	"    - define progress bar rectangle\n"
	"draw pbt [a [#rgba [#rgba]]]\n"
	"    - define progress bar text parameters\n"
	"draw prog [n]\n"
	"    - draw progress bar with given progress\n"
#endif
#if (CONFIG_XLCD_DRAW & XLCD_DRAW_TEST) && defined(CONFIG_CMD_DRAW)
	"draw test [n"
#if CONFIG_XLCD_TEST & XLCD_TEST_GRID
	" | " TEST_GRID_NAME
#endif
#if CONFIG_XLCD_TEST & XLCD_TEST_COLORS
	" | " TEST_COLORS_NAME
#endif
#if CONFIG_XLCD_TEST & XLCD_TEST_D2B
	" | " TEST_D2B_NAME
#endif
#if CONFIG_XLCD_TEST & XLCD_TEST_GRAD
	" | " TEST_GRAD_NAME
#endif
	"]\n"
	"    - draw test pattern\n"
#endif
	"draw clip x1 y1 x2 y2\n"
	"    - define clipping region from (x1, y1) to (x2, y2)\n"
	"draw origin x y\n"
	"    - move drawing origin (0, 0) to given position\n"
	"draw\n"
	"    - show current drawing parameters\n"
	"Coordinates may be expressions with +, -, *, /, (, ), and with the\n"
	"following names: hmin, hmid, hmax, hres, vmin, vmid, vmax, vres\n"
	"fbhmin, fbhmid, fbhmax, fbhres, fbvmin, fbvmid, fbvmax, fbvres\n"
);
#endif

#if defined(CONFIG_CMD_DRAW) && defined(CONFIG_CMD_ADRAW)
/* If both types of draw commands are active, use draw and adraw */
U_BOOT_CMD(
	adraw, 8, 1, do_draw,
	"draw to selected window, directly applying alpha",
	"arguments\n"
	"    - see 'help draw' for a description of the 'adraw' arguments\n"
);
#endif


#if defined(CONFIG_CMD_BMINFO) && (CONFIG_XLCD_DRAW & XLCD_DRAW_BITMAP)
/************************************************************************/
/* Command bminfo							*/
/************************************************************************/
static int do_bminfo(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	u_int base, addr;
	u_int i;
	u_int start;
	u_int count;

	if (argc < 2) {
		printf("Missing argument\n");
		return 1;
	}

	/* Get base address */
	base = simple_strtoul(argv[1], NULL, 16);

	start = 0;
	if (argc > 2)
		start = simple_strtoul(argv[2], NULL, 0);

	count = 0xFFFFFFFF;
	if (argc > 3)
		count = simple_strtoul(argv[3], NULL, 0);

	/* Print header line for bitmap info list */
	printf("#\tOffset\t\thres x vres\tbpp\tType\tCIB\tInfo\n");
	printf("--------------------------------------------"
	       "-----------------------------------\n");
	addr = base;
	for (i=0; ; i++)
	{
		bminfo_t bi;
		u_int bmaddr = addr;

		/* Scan bitmap structure and get end of current bitmap; stop
		   on error, this is usually the end of the (multi-)image */
		addr = lcd_scan_bitmap(addr);
		if (!addr)
			break;

		/* Get bitmap info and show it */
		lcd_get_bminfo(&bi, bmaddr);
		if (i >= start) {
			printf("%d\t0x%08x\t%4d x %d\t%d\t%s\t%c%c%c\t%s\n",
			       i, bmaddr - base, bi.hres, bi.vres, bi.bitdepth,
			       bi.bm_name,
			       (bi.flags & BF_COMPRESSED) ? 'C' : '-',
			       (bi.flags & BF_INTERLACED) ? 'I' : '-',
			       (bi.flags & BF_BOTTOMUP) ? 'B' : '-',
			       bi.ct_name);
			if (--count == 0)
				break;
		}
	}
	if (!i)
		puts("(no bitmap found)\n");

	return 0;
}

U_BOOT_CMD(
	bminfo, 4, 1, do_bminfo,
	"show (multi-)bitmap information in a list",
	"addr [start [count]]\n"
	"    - show information about bitmap(s) stored at addr\n"
);
#endif /* CONFIG_CMD_BMINFO && (CONFIG_XLCD_DRAW & XLCD_DRAW_BITMAP) */

