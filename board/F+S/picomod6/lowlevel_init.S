/*
 * Memory Setup stuff - taken from blob memsetup.S
 *
 * Copyright (C) 1999 2000 2001 Erik Mouw (J.A.K.Mouw@its.tudelft.nl) and
 *                     Jan-Derk Bakker (J.D.Bakker@its.tudelft.nl)
 *
 * Modified for the F&S PicoMOD6 by
 * (C) Copyright 2012
 * H. Keller, F&S Elektronik Systeme GmbH (keller@fs-net.de)
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */


#include <config.h>
#include <version.h>

#include <asm/arch/s3c64xx-regs.h>

	.globl lowlevel_init
lowlevel_init:
	mov	r12, lr

#ifndef CONFIG_PICOMOD6
	/* Set signal strength of memory port 1 to 10mA */
	ldr	r1, =0x55555555
	str	r1, [r0, #MEM1DRVCON_OFFSET]

	/* Disable Watchdog */
	ldr	r0, =0x7e000000		@0x7e004000
	orr	r0, r0, #0x4000
	mov	r1, #0
	str	r1, [r0]

	/* External interrupt pending clear */
	ldr	r0, =(ELFIN_GPIO_BASE+EINTPEND_OFFSET)	/*EINTPEND*/
	ldr	r1, [r0]
	str	r1, [r0]

	ldr	r0, =ELFIN_VIC0_BASE_ADDR 	@0x71200000
	ldr	r1, =ELFIN_VIC1_BASE_ADDR 	@0x71300000

	/* Disable all interrupts (VIC0 and VIC1) */
	mvn	r3, #0x0
	str	r3, [r0, #oINTMSK]
	str	r3, [r1, #oINTMSK]

	/* Set all interrupts as IRQ */
	mov	r3, #0x0
	str	r3, [r0, #oINTMOD]
	str	r3, [r1, #oINTMOD]

	/* Pending Interrupt Clear */
	mov	r3, #0x0
	str	r3, [r0, #oVECTADDR]
	str	r3, [r1, #oVECTADDR]

	/* init system clock */
	bl	system_clock_init

#ifndef CONFIG_NAND_SPL
	/* for UART */
	bl	uart_asm_init
#endif

#ifdef CONFIG_BOOT_NAND
	/* simple init for NAND */
	bl	nand_asm_init
#endif

	bl	mem_ctrl_asm_init	/* Configure DRAM */
#endif  /* !CONFIG_PICOMOD6 */

/* Wakeup support. Don't know if it's going to be used, untested. */
	ldr	r0, =(ELFIN_CLOCK_POWER_BASE)
	ldr	r1, [r0, #RST_STAT_OFFSET]
	bic	r1, r1, #0xfffffff7
	cmp	r1, #0x8
	beq	wakeup_reset

	mov	lr, r12
	mov	pc, lr

wakeup_reset:
	/* Clear wakeup status register */
	ldr	r1, [r0, #WAKEUP_STAT_OFFSET]
	str	r1, [r0, #WAKEUP_STAT_OFFSET]

	/* Load return address and jump to kernel */
	/* r1 = physical address of s3c6400_cpu_resume function */
	ldr	r1, [r0, #INF_REG0_OFFSET]
	/* Jump to kernel (sleep-s3c6400.S) */
	mov	pc, r1
	nop
	nop

#ifndef CONFIG_PICOMOD6  /* System clock already configured by NBoot */
/*
 * system_clock_init: Initialize core clock and bus clock.
 * void system_clock_init(void)
 */
system_clock_init:
	ldr	r0, =ELFIN_CLOCK_POWER_BASE	/* 0x7e00f000 */

#ifdef CONFIG_SYNC_MODE
	ldr	r1, [r0, #OTHERS_OFFSET]
	mov	r2, #0x40
	orr	r1, r1, r2
	str	r1, [r0, #OTHERS_OFFSET]

	nop
	nop
	nop
	nop
	nop

	ldr	r2, =0x80
	orr	r1, r1, r2
	str	r1, [r0, #OTHERS_OFFSET]

check_syncack:
	ldr	r1, [r0, #OTHERS_OFFSET]
	ldr	r2, =0xf00
	and	r1, r1, r2
	cmp	r1, #0xf00
	bne	check_syncack
#else	/* ASYNC Mode */
	nop
	nop
	nop
	nop
	nop

	/*
	 * This was unconditional in original Samsung sources, but it doesn't
	 * seem to make much sense on S3C6400.
	 */
#ifndef CONFIG_S3C6400
	ldr	r1, [r0, #OTHERS_OFFSET]
	bic	r1, r1, #0xC0
	orr	r1, r1, #0x40
	str	r1, [r0, #OTHERS_OFFSET]

wait_for_async:
	ldr	r1, [r0, #OTHERS_OFFSET]
	and	r1, r1, #0xf00
	cmp	r1, #0x0
	bne	wait_for_async
#endif

	ldr	r1, [r0, #OTHERS_OFFSET]
	bic	r1, r1, #0x40
	str	r1, [r0, #OTHERS_OFFSET]
#endif

	mov	r1, #0xff00
	orr	r1, r1, #0xff
	str	r1, [r0, #APLL_LOCK_OFFSET]
	str	r1, [r0, #MPLL_LOCK_OFFSET]
//	str	r1, [r0, #EPLL_LOCK_OFFSET]

	/* Set Clock Divider */
	ldr	r1, [r0, #CLK_DIV0_OFFSET]
	bic	r1, r1, #0x30000
	bic	r1, r1, #0xff00
	bic	r1, r1, #0xff
	ldr	r2, =CLK_DIV_VAL
	orr	r1, r1, r2
	str	r1, [r0, #CLK_DIV0_OFFSET]

	ldr	r1, =APLL_VAL
	str	r1, [r0, #APLL_CON_OFFSET]
	ldr	r1, =MPLL_VAL
	str	r1, [r0, #MPLL_CON_OFFSET]

	/* FOUT of EPLL is 96MHz */
	//ldr	r1, =0x80200203
	ldr	r1, =0x200203
	str	r1, [r0, #EPLL_CON0_OFFSET]
	ldr	r1, =0x0
	str	r1, [r0, #EPLL_CON1_OFFSET]

	/* APLL, MPLL, EPLL select to Fout */
	ldr	r1, [r0, #CLK_SRC_OFFSET]
	orr	r1, r1, #0x7
	str	r1, [r0, #CLK_SRC_OFFSET]

	/* wait at least 200us to stablize all clock */
	mov	r1, #0x10000
1:	subs	r1, r1, #1
	bne	1b

	/* Synchronization for VIC port */
#ifdef CONFIG_SYNC_MODE
	ldr	r1, [r0, #OTHERS_OFFSET]
	orr	r1, r1, #0x20
	str	r1, [r0, #OTHERS_OFFSET]
#else
	ldr	r1, [r0, #OTHERS_OFFSET]
	bic	r1, r1, #0x20
	str	r1, [r0, #OTHERS_OFFSET]
#endif
	mov	pc, lr
#endif /* !CONFIG_PICOMOD6 */


#ifndef CONFIG_NAND_SPL
/*
 * uart_asm_init: Initialize UART's pins
 */
uart_asm_init:
	/* set GPIO to enable UART */
	ldr	r0, =ELFIN_GPIO_BASE
	ldr	r1, =0x220022
	str	r1, [r0, #GPACON_OFFSET]
	mov	pc, lr
#endif

#ifdef CONFIG_BOOT_NAND
/*
 * NAND Interface init for SMDK6400
 */
nand_asm_init:
	ldr	r0, =ELFIN_NAND_BASE
	ldr	r1, [r0, #NFCONF_OFFSET]
	orr	r1, r1, #0x70
	orr	r1, r1, #0x7700
	str	r1, [r0, #NFCONF_OFFSET]

	ldr	r1, [r0, #NFCONT_OFFSET]
	orr	r1, r1, #0x07
	str	r1, [r0, #NFCONT_OFFSET]

	mov	pc, lr
#endif

#ifdef CONFIG_ENABLE_MMU
	/*  0x00000000 - 0x4FFFFFFF: 1:1, no caching
	    0x50000000 - 0x57FFFFFF: 1:1, write-through
	    0x58000000 - 0x5FFFFFFF: 1:1, no caching
	    0x60000000 - 0x67FFFFFF -> 0x50000000 - 0x57FFFFFF, no caching
	    0x68000000 - 0x9FFFFFFF: 1:1, no caching
	    0xA0000000 - 0xFFFFFFFF: Invalid */
	/* void setup_mmu_table(ulong tlb_addr) */
	.globl	setup_mmu_table
setup_mmu_table:			/* r0: tlb_addr */
	mcr	p15, 0, r7, c2, c0, 0	/* Set the TTB register */

#ifdef CONFIG_SOFT_MMUTABLE
	/* Create MMU table at tbl_addr by software */
	mov	r1, #0			/* r1: upper 12 bits of address */
1:	mov	r2, #0			/* r2: table entry */
	cmp	r1, #0xA00
	bhs	3f
	lsl	r2, r1, #20
	cmp	r1, #0x600
	blo	2f
	cmp	r1, #0x680
	bhs	2f
	sub	r2, r2, #0x10000000
2:	orr	r2, r2, #(3<<10)	 // AP
	orr	r2, r2, #(1<<4)|2	 // NX, 1MB Section entry
	cmp	r1, #0x500
	blo	3f
	cmp	r1, #0x580
	bhs	3f
#if 0
	/* Write-Back-Cache */
	orr	r2, r2, #(3<<2)		  // RAM region: add read/write access
#else
	/* Write-Through-Cache */
	orr	r2, r2, #(2<<2)		  // RAM region: add read/write access
#endif
3:	str	r2, [r0], #4		  // Store first level page table entry
	add	r1, r1, #1		  // Next 1MB section
	cmp	r1, #0x1000		  // After 4096 entries we are done
	blo	1b
	mov	pc, lr

#else /* !CONFIG_SOFT_MMUTABLE */

	/* Copy constant MMU table from mmu_table to tlb_addr */
	adr	r1, mmu_table
	mov	r2, #0x4000
4:	ldmia	r1!, {r3, r12}		/* copy from source address [r1] */
	subs	r2, #8			/* 2 registers are 8 bytes */
	stmia	r0!, {r3, r12}		/* copy to   target address [r0] */
	bne	4b
	mov	pc, lr

	/* Macro to form a first-level section entry */
.macro FL_SECTION_ENTRY base,ap,d,c,b
	.word (\base << 20) | (\ap << 10) | \
	      (\d << 5) | (1<<4) | (\c << 3) | (\b << 2) | (1<<1)
.endm

	/* Build a constant mmu_table */
//###	.section .mmudata, "a"
	.section .data, "a"
//###	.align 14 // not necessary anymore, content is copied to another place
	.globl mmu_table
mmu_table:
	.set __base, 0
	/* 1280MB 1:1 mapping for debugging 0x00000000-0x4fffffff */
	.rept 0x500
	FL_SECTION_ENTRY __base, 3, 0, 0, 0
	.set __base, __base + 1
	.endr

	/* 128MB for SDRAM at 0x50000000-0x57ffffff */
	.rept 0x580 - 0x500
	FL_SECTION_ENTRY __base, 3, 0, 1, 1
	.set __base, __base + 1
	.endr

	/* 1152MB 1:1 mapping 0x58000000-0x9fffffff */
	.rept 0xA00 - 0x580
	FL_SECTION_ENTRY __base, 3, 0, 0, 0
	.set __base,__base + 1
	.endr

	/* 1536MB access is not allowed 0xa0000000-ffffffff */
	.rept 0x1000 - 0xa00
	.word 0x00000000
	.endr

#endif /* CONFIG_SOFT_MMUTABLE */
#endif /* CONFIG_ENABLE_MMU */
