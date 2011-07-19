/* Code from lcd.h moved here, to allow better separation between hardware
   independent and hardware specific part of the driver */

#ifndef _ASM_ARCH_PXAFB_H_
#define _ASM_ARCH_PXAFB_H_

#include <asm/byteorder.h>

#define CONFIG_LCDWIN_EXT

/*
 * PXA LCD DMA descriptor
 */
struct pxafb_dma_descriptor {
	u_long	fdadr;		/* Frame descriptor address register */
	u_long	fsadr;		/* Frame source address register */
	u_long	fidr;		/* Frame ID register */
	u_long	ldcmd;		/* Command register */
};

/*
 * PXA LCD info
 */
struct wininfo_ext {			  /* was: struct pxafb_info */
	/* Misc registers */
	u_long	reg_lccr3;
	u_long	reg_lccr2;
	u_long	reg_lccr1;
	u_long	reg_lccr0;
	u_long	fdadr0;
	u_long	fdadr1;

	/* DMA descriptors */
	struct	pxafb_dma_descriptor *	dmadesc_fblow;
	struct	pxafb_dma_descriptor *	dmadesc_fbhigh;
	struct	pxafb_dma_descriptor *	dmadesc_palette;

	u_long	screen;		/* physical address of frame buffer */
	u_long	palette;	/* physical address of palette memory */
	u_int	palette_size;
	u_char	datapol;	/* DATA polarity (0=normal, 1=inverted) */
};


#endif /*!_ASM_ARCH_PXAFB_H_*/
