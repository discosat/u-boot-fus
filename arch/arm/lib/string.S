/*
 * Optimized string and memory functions for ARM v6 (S3C64XX)
 *
 * (C) Copyright 2011
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

#include <asm/assembler.h>		  // PLD()

/* We don't want the strange macros for push and pull from assembler.h */
#undef push
#undef pull

/*********************************************************************
*** Function:    void *memcpy(void *dest, void *src, size_t count) ***
*** 								   ***
*** Parameters:  dest:  Pointer to destination			   ***
***              src:   Pointer to source			   ***
***              count: Number of bytes to copy			   ***
*** 								   ***
*** Return:      dest						   ***
*** 								   ***
*** Description							   ***
*** -----------							   ***
*** Copy data from src to dest. This is an optimized version that  ***
*** tries to copy in chunks of 32 bytes (8 words). A problem	   ***
*** occurs if the pointers are not aligned to words. The algorithm ***
*** works as follows: move single bytes, until the source pointer  ***
*** is 8-bytes aligned (this is the fastest alignment for ldmia).  ***
*** Then copy 32 bytes with ldmia into 8 registers. Depending on   ***
*** the alignment offset of the dest pointer now, shift the	   ***
*** registers accordingly and write them to the destination in	   ***
*** largest possible units. If the remaining bytes are less than   ***
*** 32, copy the remaining 16/8/4/2/1 bytes in a similar manner,   ***
*** i.e. in largest possible units. As the src pointer is always   ***
*** word aligned, we can always use ldmia to load all src data in  ***
*** one go. By using the preload instruction pld, the data should  ***
*** already be available in cache, also improving speed.	   ***
*********************************************************************/
#if 0
/* Starting with u-boot-2011.06, there is a new memcpy version available
   in arch/arm/lib/memcpy.S, so we don't need our version anymore */
	.globl	memcpy
memcpy:
	/* Return immediately if count is zero or dest == src */
	cmp	r2, #0
	bxeq	lr
	cmp	r0, r1
	bxeq	lr
	push	{r0, r4, lr}
	b	1f

	/* Move single bytes until src pointer is 8-bytes aligned */
0:	ldrb	ip, [r1], #1
	subs	r2, r2, #1
	strb	ip, [r0], #1
	popeq	{r0, r4, pc}
1:	tst	r1, #7
	bne	0b

	/* Select working procedure depending on dest pointer alignment */
	tst	r0, #2
	beq	3f			  //Offset 0 or 1

	tst	r0, #1
	beq	2f			  //Offset 2

	/* ----- */

	/* Destination pointer has alignment offset 3: when reading a src word,
	   1 byte goes to this dest word and 3 bytes to the next dest word */
	cmp	r2, #31
	bls	1f

	/* Copy 8 words (32 bytes) per cycle */
	push	{r5-r8}
0:	ldmia	r1!, {r3-r8, ip, lr}
	sub	r2, r2, #32
PLD(	pld	[r1, #32] )
	strb	r3, [r0], #1		  //Now r0 is word aligned
	mov	r3, r3, lsr #8
	orr	r3, r3, r4, lsl #24
	mov	r4, r4, lsr #8
	orr	r4, r4, r5, lsl #24
	mov	r5, r5, lsr #8
	orr	r5, r5, r6, lsl #24
	mov	r6, r6, lsr #8
	orr	r6, r6, r7, lsl #24
	mov	r7, r7, lsr #8
	orr	r7, r7, r8, lsl #24
	mov	r8, r8, lsr #8
	orr	r8, r8, ip, lsl #24
	mov	ip, ip, lsr #8
	orr	ip, ip, lr, lsl #24
	mov	lr, lr, lsr #8
	stmia	r0!, {r3-r8, ip}	  //Store 7 full words
	mov	ip, lr, lsr #16
	strh	lr, [r0], #2		  //Store 2 of the 3 bytes as halfword
	cmp	r2, #31
	strb	ip, [r0], #1		  //Store last byte
	bhi	0b
	pop	{r5-r8}

1:	cmp	r2, #15
	bls	1f

	/* Copy next 4 words (16 bytes) */
	ldmia	r1!, {r3, r4, ip, lr}
	sub	r2, r2, #16
	strb	r3, [r0], #1		  //Now r0 is word aligned
	mov	r3, r3, lsr #8
	orr	r3, r3, r4, lsl #24
	mov	r4, r4, lsr #8
	orr	r4, r4, ip, lsl #24
	mov	ip, ip, lsr #8
	orr	ip, ip, lr, lsl #24
	mov	lr, lr, lsr #8
	stmia	r0!, {r3, r4, ip}	  //Store 3 full words
	mov	ip, lr, lsr #16
	strh	lr, [r0], #2		  //Store 2 of the 3 bytes as halfword
	strb	ip, [r0], #1		  //Store last byte

1:	cmp	r2, #7
	bls	6f

	/* Copy next 2 words (8 bytes) */
	ldmia	r1!, {ip, lr}
	sub	r2, r2, #8
	strb	ip, [r0], #1		  //Now r0 is word aligned
	mov	ip, ip, lsr #8
	orr	ip, ip, lr, lsl #24
	mov	lr, lr, lsr #8
	str	ip, [r0], #4		  //Store a full word
	mov	ip, lr, lsr #16
	strh	lr, [r0], #2		  //Store 2 of the 3 bytes as halfword
	strb	ip, [r0], #1		  //Store last byte
	b	6f

	/* ----- */

	/* Destination pointer has alignment offset 2: when reading a src word,
	   2 bytes go to this dest word and 2 bytes to the next dest word */
2:	cmp	r2, #31
	bls	1f

	/* Copy 8 words (32 bytes) per cycle */
	push	{r5-r8}
0:	ldmia	r1!, {r3-r8, ip, lr}
	sub	r2, r2, #32
PLD(	pld	[r1, #32] )
	strh	r3, [r0], #2		  //Now r0 is word aligned
	mov	r3, r3, lsr #16
	orr	r3, r3, r4, lsl #16
	mov	r4, r4, lsr #16
	orr	r4, r4, r5, lsl #16
	mov	r5, r5, lsr #16
	orr	r5, r5, r6, lsl #16
	mov	r6, r6, lsr #16
	orr	r6, r6, r7, lsl #16
	mov	r7, r7, lsr #16
	orr	r7, r7, r8, lsl #16
	mov	r8, r8, lsr #16
	orr	r8, r8, ip, lsl #16
	mov	ip, ip, lsr #16
	orr	ip, ip, lr, lsl #16
	mov	lr, lr, lsr #16
	stmia	r0!, {r3-r8, ip}	  //Store 7 full words
	cmp	r2, #31
	strh	lr, [r0], #2		  //Store final halfword
	bhi	0b
	pop	{r5-r8}

1:	cmp	r2, #15
	bls	1f

	/* Copy next 4 words (16 bytes) */
	ldmia	r1!, {r3, r4, ip, lr}
	sub	r2, r2, #16
	strh	r3, [r0], #2		  //Now r0 is word aligned
	mov	r3, r3, lsr #16
	orr	r3, r3, r4, lsl #16
	mov	r4, r4, lsr #16
	orr	r4, r4, ip, lsl #16
	mov	ip, ip, lsr #16
	orr	ip, ip, lr, lsl #16
	mov	lr, lr, lsr #16
	stmia	r0!, {r3, r4, ip}	  //Store 3 full words
	strh	lr, [r0], #2		  //Store final halfword

1:	cmp	r2, #7
	bls	1f

	/* Copy next 2 words (8 bytes) */
	ldmia	r1!, {ip, lr}
	sub	r2, r2, #8
	strh	ip, [r0], #2		  //Now r0 is word aligned
	mov	ip, ip, lsr #16
	orr	ip, ip, lr, lsl #16
	mov	lr, lr, lsr #16
	str	ip, [r0], #4		  //Store a full word
	strh	lr, [r0], #2		  //Store final halfword

1:	cmp	r2, #3
	bls	8f

	/* Copy next word (4 bytes) */
	ldr	ip, [r1], #4
	sub	r2, r2, #4
	mov	lr, ip, lsr #16
	strh	ip, [r0], #2
	strh	lr, [r0], #2
	b	8f

	/* ----- */

3:	tst	r0, #1
	beq	4f			  //Offset 0

	/* Destination pointer has alignment offset 1: when reading a src word,
	   3 bytes go to this dest word and 1 byte to the next dest word */

	cmp	r2, #31
	bls	1f

	/* Copy 8 words (32 bytes) per cycle */
	push	{r5-r8}
0:	ldmia	r1!, {r3-r8, ip, lr}
	sub	r2, r2, #32
	strb	r3, [r0], #1		  //r0 is now halfword aligned
	mov	r3, r3, lsr #8
PLD(	pld	[r1, #32] )
	strh	r3, [r0], #2		  //r0 is now word aligned
	mov	r3, r3, lsr #16
	orr	r3, r3, r4, lsl #8
	mov	r4, r4, lsr #24
	orr	r4, r4, r5, lsl #8
	mov	r5, r5, lsr #24
	orr	r5, r5, r6, lsl #8
	mov	r6, r6, lsr #24
	orr	r6, r6, r7, lsl #8
	mov	r7, r7, lsr #24
	orr	r7, r7, r8, lsl #8
	mov	r8, r8, lsr #24
	orr	r8, r8, ip, lsl #8
	mov	ip, ip, lsr #24
	orr	ip, ip, lr, lsl #8
	mov	lr, lr, lsr #24
	stmia	r0!, {r3-r8, ip}	  //Store 7 full words
	cmp	r2, #31
	strb	lr, [r0], #1		  //Store last byte
	bhi	0b
	pop	{r5-r8}

1:	cmp	r2, #15
	bls	1f

	/* Copy next 4 words (16 bytes) */
	ldmia	r1!, {r3, r4, ip, lr}
	sub	r2, r2, #16
	strb	r3, [r0], #1		  //r0 is now halfword aligned
	mov	r3, r3, lsr #8
	strh	r3, [r0], #2		  //r0 is now word aligned
	mov	r3, r3, lsr #16
	orr	r3, r3, r4, lsl #8
	mov	r4, r4, lsr #24
	orr	r4, r4, ip, lsl #8
	mov	ip, ip, lsr #24
	orr	ip, ip, lr, lsl #8
	mov	lr, lr, lsr #24
	stmia	r0!, {r3, r4, ip}	  //Store 3 full words
	strb	lr, [r0], #1		  //Store last byte

1:	cmp	r2, #7
	bls	6f

	/* Copy next 2 words (8 bytes) */
	ldmia	r1!, {ip, lr}
	sub	r2, r2, #8
	strb	ip, [r0], #1		  //r0 is now halfword aligned
	mov	ip, ip, lsr #8
	strh	ip, [r0], #2		  //r0 is now word aligned
	mov	ip, ip, lsr #16
	orr	ip, ip, lr, lsl #8
	mov	lr, lr, lsr #24
	str	ip, [r0], #4		  //Store a full word
	strb	lr, [r0], #1		  //Store last byte

6:	cmp	r2, #3
	bls	1f

	/* Copy next word (4 bytes) */
	ldr	ip, [r1], #4
	sub	r2, r2, #4
	mov	lr, ip, lsr #8
	strb	ip, [r0], #1
	mov	ip, lr, lsr #16
	strh	lr, [r0], #2
	strb	ip, [r0], #1

1:	cmp	r2, #1
	bls	9f

	/* Copy next halfword (2 bytes) */
	ldrh	ip, [r1], #2
	cmp	r2, #3
	mov	lr, ip, lsr #8
	strb	ip, [r0], #1
	strb	lr, [r0], #1
	b	9f

	/* ----- */

	/* Destination pointer has alignment offset 0: when reading a src word,
	   all 4 bytes go to this dest word (matching alignment) */
4:	cmp	r2, #31
	bls	1f

	/* Copy 8 words (32 bytes) per cycle */
	push	{r5-r8}
0:	ldmia	r1!, {r3-r8, ip, lr}
	sub	r2, r2, #32
PLD(	pld	[r1, #32] )
	cmp	r2, #31
	stmia	r0!, {r3-r8, ip, lr}
	bhi	0b
	pop	{r5-r8}

1:	cmp	r2, #15
	bls	1f

	/* Copy next 4 words (16 bytes) */
	ldmia	r1!, {r3, r4, ip, lr}
	sub	r2, r2, #16
	stmia	r0!, {r3, r4, ip, lr}

1:	cmp	r2, #7
	bls	1f

	/* Copy next 2 words (8 bytes) */
	ldmia	r1!, {ip, lr}
	sub	r2, r2, #8
	stmia	r0!, {ip, lr}

1:	cmp	r2, #3
	bls	8f

	/* Copy next word (4 bytes) */
	ldr	ip, [r1], #4
	sub	r2, r2, #4
	str	ip, [r0], #4

8:	cmp	r2, #1
	bls	9f

	/* Copy next halfword (2 bytes) */
	ldrh	ip, [r1], #2
	cmp	r2, #3
	strh	ip, [r0], #2

	/* Copy last byte (if any) */
9:	ldreqb	lr, [r1], #1
	streqb	lr, [r0]
	pop	{r0, r4, pc}
#endif


/*********************************************************************
*** Function:    void *memmove(void *dest, void *src, size_t count)***
*** 								   ***
*** Parameters:  dest:  Pointer to destination			   ***
***              src:   Pointer to source			   ***
***              count: Number of bytes to copy			   ***
*** 								   ***
*** Return:      dest						   ***
*** 								   ***
*** Description							   ***
*** -----------							   ***
*** Copy data from src to dest. If dest <= src, copy from low to   ***
*** high addresses (in fact use memcpy()), otherwise from high to  ***
*** low addresses. This allows for overlapping memory regions. The ***
*** optimization works similar to memcpy() (see above).            ***
*********************************************************************/
	.globl	memmove
memmove:
	/* If copy can be done from low to high, use memcpy() */
	cmp	r0, r1
	bls	memcpy

	/* Return immediately if count is zero */
	cmp	r2, #0
	bxeq	lr

	push	{r4, lr}
	add	r0, r0, r2		  //Move pointers to end
	add	r1, r1, r2
	b	1f

	/* Move single bytes until src pointer is 8-bytes aligned */
0:	ldrb	ip, [r1, #-1]!
	subs	r2, r2, #1
	strb	ip, [r0, #-1]!
	popeq	{r4, pc}
1:	tst	r1, #7
	bne	0b

	/* Select working procedure depending on dest pointer alignment */
	tst	r0, #2
	beq	3f			  //Offset 0 or 1

	tst	r0, #1
	beq	2f			  //Offset 2

	/* ----- */

	/* Destination pointer has alignment offset 3: when reading a src word,
	   1 byte goes to this dest word and 3 bytes to the next dest word */
	cmp	r2, #31
	add	r0, r0, #1	          //Move dest to next word boundary
	bls	1f

	/* Copy 8 words (32 bytes) per cycle */
	push	{r5-r8}
0:	ldmdb	r1!, {r3-r8, ip, lr}
	sub	r0, r0, #32
	sub	r2, r2, #32
PLD(	pld	[r1, #-32] )
	strb	r3, [r0, #-1]
	mov	r3, r3, lsr #8
	orr	r3, r3, r4, lsl #24
	mov	r4, r4, lsr #8
	orr	r4, r4, r5, lsl #24
	mov	r5, r5, lsr #8
	orr	r5, r5, r6, lsl #24
	mov	r6, r6, lsr #8
	orr	r6, r6, r7, lsl #24
	mov	r7, r7, lsr #8
	orr	r7, r7, r8, lsl #24
	mov	r8, r8, lsr #8
	orr	r8, r8, ip, lsl #24
	mov	ip, ip, lsr #8
	orr	ip, ip, lr, lsl #24
	mov	lr, lr, lsr #8
	stmia	r0, {r3-r8, ip}		  //Store 7 full words
	mov	ip, lr, lsr #16
	strh	lr, [r0, #28]		  //Store 2 of the 3 bytes as halfword
	cmp	r2, #31
	strb	ip, [r0, #30]		  //Store last byte
	bhi	0b
	pop	{r5-r8}

1:	cmp	r2, #15
	bls	1f

	/* Copy next 4 words (16 bytes) */
	ldmdb	r1!, {r3, r4, ip, lr}
	sub	r0, r0, #16
	sub	r2, r2, #16
	strb	r3, [r0, #-1]
	mov	r3, r3, lsr #8
	orr	r3, r3, r4, lsl #24
	mov	r4, r4, lsr #8
	orr	r4, r4, ip, lsl #24
	mov	ip, ip, lsr #8
	orr	ip, ip, lr, lsl #24
	mov	lr, lr, lsr #8
	stmia	r0, {r3, r4, ip}	  //Store 3 full words
	mov	ip, lr, lsr #16
	strh	lr, [r0, #12]		  //Store 2 of the 3 bytes as halfword
	strb	ip, [r0, #14]		  //Store last byte

1:	cmp	r2, #7
	sub	r0, r0, #1	          //Go back to old alignment offset 3
	bls	6f

	/* Copy next 2 words (8 bytes) */
	ldmdb	r1!, {ip, lr}
	sub	r0, r0, #8
	sub	r2, r2, #8
	strb	ip, [r0]
	mov	ip, ip, lsr #8
	orr	ip, ip, lr, lsl #24
	mov	lr, lr, lsr #8
	str	ip, [r0, #1]		  //Store a full word
	mov	ip, lr, lsr #16
	strh	lr, [r0, #5]		  //Store 2 of the 3 bytes as halfword
	strb	ip, [r0, #7]		  //Store last byte
	b	6f

	/* ----- */

	/* Destination pointer has alignment offset 2: when reading a src word,
	   2 bytes go to this dest word and 2 bytes to the previous dest word */
2:	cmp	r2, #31
	add	r0, r0, #2		  //Move dest to next word boundary
	bls	1f

	/* Copy 8 words (32 bytes) per cycle */
	push	{r5-r8}
0:	ldmdb	r1!, {r3-r8, ip, lr}
	sub	r0, r0, #32
	sub	r2, r2, #32
PLD(	pld	[r1, #-32] )
	strh	r3, [r0, #-2]
	mov	r3, r3, lsr #16
	orr	r3, r3, r4, lsl #16
	mov	r4, r4, lsr #16
	orr	r4, r4, r5, lsl #16
	mov	r5, r5, lsr #16
	orr	r5, r5, r6, lsl #16
	mov	r6, r6, lsr #16
	orr	r6, r6, r7, lsl #16
	mov	r7, r7, lsr #16
	orr	r7, r7, r8, lsl #16
	mov	r8, r8, lsr #16
	orr	r8, r8, ip, lsl #16
	mov	ip, ip, lsr #16
	orr	ip, ip, lr, lsl #16
	mov	lr, lr, lsr #16
	stmia	r0, {r3-r8, ip}		  //Store 7 full words
	cmp	r2, #31
	strh	lr, [r0, #28]		  //Store final halfword
	bhi	0b
	pop	{r5-r8}

1:	cmp	r2, #15
	bls	1f

	/* Copy next 4 words (16 bytes) */
	ldmdb	r1!, {r3, r4, ip, lr}
	sub	r0, r0, #16
	sub	r2, r2, #16
	strh	r3, [r0, #-2]
	mov	r3, r3, lsr #16
	orr	r3, r3, r4, lsl #16
	mov	r4, r4, lsr #16
	orr	r4, r4, ip, lsl #16
	mov	ip, ip, lsr #16
	orr	ip, ip, lr, lsl #16
	mov	lr, lr, lsr #16
	stmia	r0, {r3, r4, ip}	  //Store 3 full words
	strh	lr, [r0, #12]		  //Store final halfword

1:	cmp	r2, #7
	bls	1f

	/* Copy next 2 words (8 bytes) */
	ldmdb	r1!, {ip, lr}
	sub	r0, r0, #8
	sub	r2, r2, #8
	strh	ip, [r0, #-2]
	mov	ip, ip, lsr #16
	orr	ip, ip, lr, lsl #16
	mov	lr, lr, lsr #16
	str	ip, [r0]		  //Store a full word
	strh	lr, [r0, #4]		  //Store final halfword

1:	cmp	r2, #3
	sub	r0, r0, #2		  //Go back to old alignment offset 2
	bls	8f

	/* Copy next word (4 bytes) */
	ldmdb	r1!, {ip}
	sub	r0, r0, #4
	sub	r2, r2, #4
	mov	lr, ip, lsr #16
	strh	ip, [r0]
	strh	lr, [r0, #2]
	b	8f

	/* ----- */

3:	tst	r0, #1
	beq	4f			  //Offset 0

	/* Destination pointer has alignment offset 1: when reading a src word,
	   1 byte goes to this dest word and 3 bytes to the previous word */

	cmp	r2, #31
	add	r0, r0, #3		  //Move dest to next word boundary
	bls	1f

	/* Copy 8 words (32 bytes) per cycle */
	push	{r5-r8}
0:	ldmdb	r1!, {r3-r8, ip, lr}
	sub	r0, r0, #32
	sub	r2, r2, #32
	strb	r3, [r0, #-3]
	mov	r3, r3, lsr #8
PLD(	pld	[r1, #-32] )
	strh	r3, [r0, #-2]
	mov	r3, r3, lsr #16
	orr	r3, r3, r4, lsl #8
	mov	r4, r4, lsr #24
	orr	r4, r4, r5, lsl #8
	mov	r5, r5, lsr #24
	orr	r5, r5, r6, lsl #8
	mov	r6, r6, lsr #24
	orr	r6, r6, r7, lsl #8
	mov	r7, r7, lsr #24
	orr	r7, r7, r8, lsl #8
	mov	r8, r8, lsr #24
	orr	r8, r8, ip, lsl #8
	mov	ip, ip, lsr #24
	orr	ip, ip, lr, lsl #8
	mov	lr, lr, lsr #24
	stmia	r0, {r3-r8, ip}		  //Store 7 full words
	cmp	r2, #31
	strb	lr, [r0, #28]		  //Store last byte
	bhi	0b
	pop	{r5-r8}

1:	cmp	r2, #15
	bls	1f

	/* Copy next 4 words (16 bytes) */
	ldmdb	r1!, {r3, r4, ip, lr}
	sub	r0, r0, #16
	sub	r2, r2, #16
	strb	r3, [r0, #-3]
	mov	r3, r3, lsr #8
	strh	r3, [r0, #-2]
	mov	r3, r3, lsr #16
	orr	r3, r3, r4, lsl #8
	mov	r4, r4, lsr #24
	orr	r4, r4, ip, lsl #8
	mov	ip, ip, lsr #24
	orr	ip, ip, lr, lsl #8
	mov	lr, lr, lsr #24
	stmia	r0, {r3, r4, ip}	  //Store 3 full words
	strb	lr, [r0, #12]		  //Store last byte

1:	cmp	r2, #7
	sub	r0, r0, #3		  //Go back to old alignment offset 1
	bls	6f

	/* Copy next 2 words (8 bytes) */
	ldmdb	r1!, {ip, lr}
	sub	r0, r0, #8
	sub	r2, r2, #8
	strb	ip, [r0]
	mov	ip, ip, lsr #8
	strh	ip, [r0, #1]
	mov	ip, ip, lsr #16
	orr	ip, ip, lr, lsl #8
	mov	lr, lr, lsr #24
	str	ip, [r0, #3]		  //Store a full word
	strb	lr, [r0, #7]		  //Store last byte

6:	cmp	r2, #3
	bls	1f

	/* Copy next word (4 bytes) */
	ldmdb	r1!, {ip}
	sub	r0, r0, #4
	sub	r2, r2, #4
	mov	lr, ip, lsr #8
	strb	ip, [r0]
	mov	ip, lr, lsr #16
	strh	lr, [r0, #1]
	strb	ip, [r0, #3]

1:	cmp	r2, #1
	bls	9f

	/* Copy next halfword (2 bytes) */
	ldrh	ip, [r1, #-2]!
	sub	r0, r0, #2
	cmp	r2, #3
	mov	lr, ip, lsr #8
	strb	ip, [r0]
	strb	lr, [r0, #1]
	b	9f

	/* ----- */

	/* Destination pointer has alignment offset 0: when reading a src word,
	   all 4 bytes go to this dest word (matching alignment) */
4:	cmp	r2, #31
	bls	1f

	/* Copy 8 words (32 bytes) per cycle */
	push	{r5-r8}
0:	ldmdb	r1!, {r3-r8, ip, lr}
	sub	r2, r2, #32
PLD(	pld	[r1, #-32] )
	cmp	r2, #31
	stmdb	r0!, {r3-r8, ip, lr}
	bhi	0b
	pop	{r5-r8}

1:	cmp	r2, #15
	bls	1f

	/* Copy next 4 words (16 bytes) */
	ldmdb	r1!, {r3, r4, ip, lr}
	sub	r2, r2, #16
	stmdb	r0!, {r3, r4, ip, lr}

1:	cmp	r2, #7
	bls	1f

	/* Copy next 2 words (8 bytes) */
	ldmdb	r1!, {ip, lr}
	sub	r2, r2, #8
	stmdb	r0!, {ip, lr}

1:	cmp	r2, #3
	bls	8f

	/* Copy next word (4 bytes) */
	ldmdb	r1!, {ip}
	sub	r2, r2, #4
	stmdb	r0!, {ip}

8:	cmp	r2, #1
	bls	9f

	/* Copy next halfword (2 bytes) */
	ldrh	ip, [r1, #-2]!
	cmp	r2, #3
	strh	ip, [r0, #-2]!

	/* Copy last byte (if any) */
9:	ldreqb	lr, [r1, #-1]!
	streqb	lr, [r0, #-1]!
	pop	{r4, pc}


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
	.globl	memset32
memset32:
	/* If count is less than 8 words, we don't need more registers */
	cmp	r2, #7
	push	{lr}
	mov	r3, r1
	mov	ip, r1
	mov	lr, r1
	add	r0, r2, lsl #2		  //Move pointer to end of data
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
1:	stmeqdb	r0!, {ip}		  //r0 is now back at original dest
	pop	{pc}


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
	.globl	memset
memset:
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
#endif
