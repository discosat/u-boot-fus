/*
 * fs_memtest_common.c
 *
 * (C) Copyright 2022
 * F&S Elektronik Systeme GmbH
 *
 * Common memory test based on memtester by Charles Cazabon.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/arch/ddr.h>
#include <asm/arch/clock.h>
#include <asm/arch/ddr.h>
#include <asm/arch/sys_proto.h>
#include "fs_memtest_common.h"

typedef unsigned long ul;
typedef unsigned long long ull;
typedef unsigned long volatile ulv;
typedef unsigned char volatile u8v;
typedef unsigned short volatile u16v;

struct test {
    char *name;
    int (*fp)();
};

#define rand32() ((unsigned int) rand() | ( (unsigned int) rand() << 16))
//#define rand32() ranval(&ctx)

#define rand_ul() rand32()
#define UL_ONEBITS 0xffffffff
#define UL_LEN 32
#define CHECKERBOARD1 0x55555555
#define CHECKERBOARD2 0xaaaaaaaa
#define UL_BYTE(x) ((x | x << 8 | x << 16 | x << 24))

#define EXIT_FAIL_NONSTARTER    0x01
#define EXIT_FAIL_ADDRESSLINES  0x02
#define EXIT_FAIL_OTHERTEST     0x04


#define ONE 0x00000001L

union {
    unsigned char bytes[UL_LEN/8];
    ul val;
} mword8;

union {
    unsigned short u16s[UL_LEN/16];
    ul val;
} mword16;

/* Function definitions. */
static int show_progress = 1;
static int wheel_pos;

static void out_test_start(void)
{
    if (show_progress) {
        printf("           ");
    }
}

static void out_test_setting(unsigned int j)
{
    if (show_progress) {
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        printf("setting %3u", j);
    }
}

static void out_test_testing(unsigned int j)
{
    if (show_progress) {
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        printf("testing %3u", j);
    }
}

static void out_test_end(void)
{
    if (show_progress) {
        printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");
    }
}

static void out_wheel_start(void)
{
    if (show_progress) {
        printf(" ");
        wheel_pos = 0;
    }
}

static void out_wheel_advance(unsigned int i)
{
    static const unsigned int wheel_often = 2500;
    static const unsigned int n_chars = 4;
    char wheel_char[4] = {'-', '\\', '|', '/'};

    if (show_progress) {
        if (!(i % wheel_often)) {
            printf("\b");
            printf("%c", wheel_char[++wheel_pos % n_chars]);
        }
    }
}

static void out_wheel_end(void)
{
    if (show_progress) {
        printf("\b \b");
    }
}


/* Function definitions. */

int compare_regions(ulv *bufa, ulv *bufb, size_t count) {
    int r = 0;
    size_t i;
    ulv *p1 = bufa;
    ulv *p2 = bufb;

    for (i = 0; i < count; i++, p1++, p2++) {
        if (*p1 != *p2) {
                printf( "FAILURE: 0x%08lx != 0x%08lx at offset 0x%08lx.\n",
                        (ul) *p1, (ul) *p2, (ul) (i * sizeof(ul)));
            /* printf("Skipping to next test..."); */
            r = -1;
			return r;
        }
    }
    return r;
}

int test_stuck_address(ulv *bufa, size_t count) {
    ulv *p1 = bufa;
    unsigned int j;
    size_t i;

    out_test_start();
    for (j = 0; j < 16; j++) {
        p1 = (ulv *) bufa;
        out_test_setting(j);
        for (i = 0; i < count; i++) {
            *p1 = ((j + i) % 2) == 0 ? (ul) p1 : ~((ul) p1);
            *p1++;
        }
        out_test_testing(j);
        p1 = (ulv *) bufa;
        for (i = 0; i < count; i++, p1++) {
            if (*p1 != (((j + i) % 2) == 0 ? (ul) p1 : ~((ul) p1))) {
                printf("FAILURE: possible bad address line at offset "
                        "0x%08lx.\n",
                        (ul) (i * sizeof(ul)));
                printf("Skipping to next test...\n");
                return -1;
            }
        }
    }
    out_test_end();
    return 0;
}

int test_random_value(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;

    out_wheel_start();
    for (i = 0; i < count; i++) {
        *p1++ = *p2++ = rand_ul();
        out_wheel_advance(i);
    }
    out_wheel_end();
    return compare_regions(bufa, bufb, count);
}

int test_xor_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ ^= q;
        *p2++ ^= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_sub_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ -= q;
        *p2++ -= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_mul_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ *= q;
        *p2++ *= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_div_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        if (!q) {
            q++;
        }
        *p1++ /= q;
        *p2++ /= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_or_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ |= q;
        *p2++ |= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_and_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ &= q;
        *p2++ &= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_seqinc_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ = *p2++ = (i + q);
    }
    return compare_regions(bufa, bufb, count);
}

int test_solidbits_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    ul q;
    size_t i;

    out_test_start();
    for (j = 0; j < 64; j++) {
        q = (j % 2) == 0 ? UL_ONEBITS : 0;
        out_test_setting(j);
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        for (i = 0; i < count; i++) {
            *p1++ = *p2++ = (i % 2) == 0 ? q : ~q;
        }
        out_test_testing(j);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    out_test_end();
    return 0;
}

int test_checkerboard_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    ul q;
    size_t i;

    out_test_start();
    for (j = 0; j < 64; j++) {
        q = (j % 2) == 0 ? CHECKERBOARD1 : CHECKERBOARD2;
        out_test_setting(j);
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        for (i = 0; i < count; i++) {
            *p1++ = *p2++ = (i % 2) == 0 ? q : ~q;
        }
        out_test_testing(j);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    out_test_end();
    return 0;
}

int test_blockseq_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    out_test_start();
    for (j = 0; j < 256; j++) {
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        out_test_setting(j);
        for (i = 0; i < count; i++) {
            *p1++ = *p2++ = (ul) UL_BYTE(j);
        }
        out_test_testing(j);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    out_test_end();
    return 0;
}

int test_walkbits0_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    out_test_start();
    for (j = 0; j < UL_LEN * 2; j++) {
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        out_test_setting(j);
        for (i = 0; i < count; i++) {
            if (j < UL_LEN) { /* Walk it up. */
                *p1++ = *p2++ = ONE << j;
            } else { /* Walk it back down. */
                *p1++ = *p2++ = ONE << (UL_LEN * 2 - j - 1);
            }
        }
        out_test_testing(j);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    out_test_end();
    return 0;
}

int test_walkbits1_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    out_test_start();
    for (j = 0; j < UL_LEN * 2; j++) {
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        out_test_setting(j);
        for (i = 0; i < count; i++) {
            if (j < UL_LEN) { /* Walk it up. */
                *p1++ = *p2++ = UL_ONEBITS ^ (ONE << j);
            } else { /* Walk it back down. */
                *p1++ = *p2++ = UL_ONEBITS ^ (ONE << (UL_LEN * 2 - j - 1));
            }
        }
        out_test_testing(j);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    out_test_end();
    return 0;
}

int test_bitspread_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    out_test_start();
    for (j = 0; j < UL_LEN * 2; j++) {
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        out_test_setting(j);
        for (i = 0; i < count; i++) {
            if (j < UL_LEN) { /* Walk it up. */
                *p1++ = *p2++ = (i % 2 == 0)
                    ? (ONE << j) | (ONE << (j + 2))
                    : UL_ONEBITS ^ ((ONE << j)
                                    | (ONE << (j + 2)));
            } else { /* Walk it back down. */
                *p1++ = *p2++ = (i % 2 == 0)
                    ? (ONE << (UL_LEN * 2 - 1 - j)) | (ONE << (UL_LEN * 2 + 1 - j))
                    : UL_ONEBITS ^ (ONE << (UL_LEN * 2 - 1 - j)
                                    | (ONE << (UL_LEN * 2 + 1 - j)));
            }
        }
        out_test_testing(j);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    out_test_end();
    return 0;
}

int test_bitflip_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j, k;
    ul q;
    size_t i;

    out_test_start();
    for (k = 0; k < UL_LEN; k++) {
        q = ONE << k;
        for (j = 0; j < 8; j++) {
            q = ~q;
            out_test_setting(k * 8 + j);
            p1 = (ulv *) bufa;
            p2 = (ulv *) bufb;
            for (i = 0; i < count; i++) {
                *p1++ = *p2++ = (i % 2) == 0 ? q : ~q;
            }
            out_test_testing(k * 8 + j);
            if (compare_regions(bufa, bufb, count)) {
                return -1;
            }
        }
    }
    out_test_end();
    return 0;
}


struct test tests[] = {

   // { "Ones", test_ones_comparison },
    { "Random Value", test_random_value },
    { "Compare XOR", test_xor_comparison },
    { "Compare SUB", test_sub_comparison },
    { "Compare MUL", test_mul_comparison },
    { "Compare DIV",test_div_comparison },
    { "Compare OR", test_or_comparison },
    { "Compare AND", test_and_comparison },
    { "Sequential Increment", test_seqinc_comparison },
    { "Solid Bits", test_solidbits_comparison },
    { "Block Sequential", test_blockseq_comparison },
    { "Checkerboard", test_checkerboard_comparison },
    { "Bit Spread", test_bitspread_comparison },
    { "Bit Flip", test_bitflip_comparison },
    { "Walking Ones", test_walkbits1_comparison },
    { "Walking Zeroes", test_walkbits0_comparison },
#ifdef TEST_NARROW_WRITES
    { "8-bit Writes", test_8bit_wide_random },
    { "16-bit Writes", test_16bit_wide_random },
#endif
    { NULL, NULL }
};

void memtester(size_t dramStartAddress, size_t memsize)
{
    ul i;
    size_t halflen, count;
    ulv *bufa, *bufb;
    ul testmask = 0;
	int exit_code = 0;

	srand(memsize);

    printf("testing %ld bytes of memory\n", memsize);

    halflen = memsize / 2;
    count = halflen / sizeof(ul);
    bufa = (ulv *)(dramStartAddress);
    bufb = (ulv *)((size_t) dramStartAddress + halflen);

    printf("bufa = %08lx, bufb = %08lx, count = %lx\n", (ul)bufa, (ul)bufb, count);

    printf("\n  %-20s: ", "Stuck Address");
    if (!test_stuck_address((ulv *)dramStartAddress, memsize / sizeof(ul))) {
        printf("ok\r\n");
    } else {
        exit_code |= EXIT_FAIL_ADDRESSLINES;
    }

    for (i=0;;i++) {
		if (exit_code || !tests[i].name)
			break;
        /* clear buffer */
        memset((void *) bufa, 255, memsize);
        /* If using a custom testmask, only run this test if the
        bit corresponding to this test was set by the user.
        */
        if (testmask && (!((1 << i) & testmask))) {
            continue;
        }
        printf("  %-20s: ", tests[i].name);
        if (!tests[i].fp(bufa, bufb, count))
        {
            printf("ok\n");
        }
        else
        {
            exit_code |= EXIT_FAIL_OTHERTEST;
        }
    }
	if (exit_code)
   		printf("\nDram Test FAILED.\n\n");
	else 
		printf("\nDram Test OK.\n\n");
}


