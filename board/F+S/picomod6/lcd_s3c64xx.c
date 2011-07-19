#include <config.h>
#include <common.h>
//#include <version.h>
//#include <stdarg.h>
//#include <linux/types.h>
//#include <devices.h>
#include <lcd.h>
#include <lcd_s3c64xx.h>

int lcd_line_length;

int lcd_color_fg;
int lcd_color_bg;

void *lcd_base;			/* Start of framebuffer memory	*/
void *lcd_console_address;	/* Start of console buffer	*/

short console_col;
short console_row;

/* LB064V02 640x480@60 */
vidinfo_t panel_info = {
	vl_name:	"LG.Philips LB064V02 640x480",
	vl_hfp:		24,
	vl_hsw:		96,
	vl_hbp:		40,
	vl_col:         640,
	vl_hspol:	CFG_LOW,
	vl_vfp:		10,
	vl_vsw:		2,
	vl_vbp:		33,
        vl_row:         480,
	vl_vspol:	CFG_LOW,
	vl_clk:		0,
        vl_clkpol:      CFG_HIGH,
        vl_denpol:      CFG_HIGH,
	vl_fps:		60,
	vl_strength:	3,
	vl_fbbase:	0,		  /* #### */
	vl_fbsize:	0,		  /* #### */
	vl_bpix:	4,		  /* 0=1bpp, 1=2bpp, 2=4bpp, 3=8bpp, 4=16bpp */
	vl_bpp:		16,
	vl_pwm:		0,		  /* #### */
	vl_pwmfreq:	0,		  /* #### */
        vl_width:       132,
        vl_height:      98,
};

#if 0
ulong calc_fbsize (void)
{
	//#### TODO
	return 0;
}
#endif

/* Compute framebuffer base address from size (immediately before U-Boot) */
ulong lcd_fbsize(ulong fbsize)
{
	/* Round size up to pages */
	fbsize = (fbsize + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);

	return CFG_UBOOT_BASE - fbsize;
}


void lcd_ctrl_init(void *lcdbase)
{
	printf("#### lcd_ctrl_init: lcdbase=0x%08lx\n", (ulong)lcdbase);
	//#### TODO
}

void lcd_setcolreg (ushort regno, ushort red, ushort green, ushort blue)
{
	//#### TODO
}

void lcd_enable (void)
{
	//#### TODO
}

/* LCD controller */
void lcd_disable (void)
{
	//#### TODO
}

/* Backlight & Co */
void lcd_panel_disable(void)
{
	//#### TODO
}
