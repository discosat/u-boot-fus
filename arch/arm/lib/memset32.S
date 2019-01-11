/*
 * Optimized memset32() for ARM v6 (S3C64XX)
 *
 * (C) Copyright 2011
 * H. Keller, F&S Elektronik Systeme GmbH (keller@fs-net.de)
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>
#include <asm/assembler.h>		  // PLD()
#include <linux/linkage.h>

/* We don't want the strange macros for push and pull from assembler.h */
#undef push
#undef pull

/*********************************************************************
*** Function:    u32 *memset32(u32 *dest, u32 value, size_t count) ***
*** 								   ***
*** Parameters:  dest:  Pointer to memory to fill		   ***
***              value: Value to store				   ***
***              count: Number of u32 words (!) to fill		   ***
*** 								   ***
*** Return:      dest						   ***
*** 								   ***
*** Description							   ***
*** -----------							   ***
*** Fill the given memory region with the given value. This is an  ***
*** optimized version that fills memory in rather large units of   ***
*** 32 words (128 bytes) per cycle to reduce loop control overhead.***
*** The remaining words are filled in steps of 8/4/2/1 word units. ***
*********************************************************************/
ENTRY(memset32)
	/* If count is less than 8 words, we don't need more registers */
	cmp	r2, #7
	push	{lr}
	mov	r3, r1
	mov	ip, r1
	mov	lr, r1
	add	r0, r0, r2, lsl #2	  //Move pointer to end of data
	bls	2f

	/* We need a set of 8 registers with the value */
	push	{r4 - r7}
	cmp	r2, #31
	mov	r4, r1
	mov	r5, r3
	mov	r6, r1
	mov	r7, r3
	bls	1f

	/* Store 32 words (128 bytes) per cycle */
0:	sub	r2, r2, #32
	stmdb	r0!, {r1, r3 - r7, ip, lr}
	stmdb	r0!, {r1, r3 - r7, ip, lr}
	cmp	r2, #31
	stmdb	r0!, {r1, r3 - r7, ip, lr}
	stmdb	r0!, {r1, r3 - r7, ip, lr}
	bhi	0b

	/* Store next 16 words (64 bytes) */
1:	cmp	r2, #15
	bls	1f
	sub	r2, r2, #16
	stmdb	r0!, {r1, r3 - r7, ip, lr}
	stmdb	r0!, {r1, r3 - r7, ip, lr}

	/* Store next 8 words (32 bytes) */
1:	cmp	r2, #7
	bls	1f
	sub	r2, r2, #8
	stmdb	r0!, {r1, r3 - r7, ip, lr}
1:	pop	{r4 - r7}

	/* Store next 4 words (16 bytes) */
2:	cmp	r2, #3
	bls	1f
	sub	r2, r2, #4
	stmdb	r0!, {r1, r3, ip, lr}

	/* Store next 2 words */
1:	cmp	r2, #1
	bls	1f
	cmp	r2, #3
	stmdb	r0!, {ip, lr}

	/* Store last word (if any) */
1:	stmdbeq	r0!, {ip}		  //r0 is now back at original dest
	pop	{pc}
ENDPROC(memset32)

/*********************************************************************
*** Function:    void *memset(void *dest, int value, size_t count) ***
*** 								   ***
*** Parameters:  dest:  Pointer to memory to fill		   ***
***              value: Value to store (in LSB)			   ***
***              count: Number of bytes to fill		   	   ***
*** 								   ***
*** Return:      dest						   ***
*** 								   ***
*** Description							   ***
*** -----------							   ***
*** Fill the given memory region with the given value. Filling is  ***
*** done from back to front. First single bytes are filled until   ***
*** the address is word aligned. Then memset32() is used to fill   ***
*** the big part of the region. Then again up to three bytes are   ***
*** filled with single bytes until the start address is reached.   ***
*********************************************************************/
#if 0
/* Starting with u-boot-2011.06, there is a new memset version available
   in arch/arm/lib/memset.S, so we don't need our version anymore */
ENTRY(memset)
	/* If count is less than four, just set as bytes */
	cmp	r2, #3
	add	r0, r0, r2		  //Move pointer to end
	bls	2f

	/* If dest is only byte aligned, store one byte */
	tst	r0, #1
	orr	r1, r1, r1, lsl #8
	beq	1f
	sub	r2, r2, #1
	strb	r1, [r0, #-1]!

	/* If dest is only halfword aligned, store one halfword */
1:	tst	r0, #2
	orr	r1, r1, r1, lsl #16
	beq	1f
	sub	r2, r2, #2
	strh	r1, [r0, #-2]!

	/* Here the pointer is word aligned; we can copy full words now by
	   calling memset32(). */
1:	and	r3, r2, #3		  //Remaining bytes after memset32()
	lsr	r2, r2, #2		  //Number of full words
	push	{r1, r3, lr}
	sub	r0, r0, r2, lsl #2
	bl	memset32
	pop	{r1, r2, lr}		  //Get back remaining count in r2

	/* Set the remaining bytes (up to 3) */
2:	cmp	r2, #1
	bls	1f
	cmp	r2, #3
	strb	r1, [r0, #-1]!		  //Set the next 2 bytes
	strb	r1, [r0, #-1]!

1:	streqb	r1, [r0, #-1]!		  //Set the last byte (if any)
	bx	lr			  //Done
ENDPROC(memset)
#endif
