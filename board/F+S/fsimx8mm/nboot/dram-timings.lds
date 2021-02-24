/*
 * (C) Copyright 2020
 * Hartmut Keller <keller@fs-net.de>
 *
 * Linker script to generate DDR RAM timing blobs for i.MX8 CPUs.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

MEMORY { .ocram : ORIGIN = ###DRAM_TIMING_ADDR###, LENGTH = 0x4000 }
OUTPUT_FORMAT("elf64-littleaarch64", "elf64-littleaarch64", "elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
SECTIONS
{
	. = 0x00000000;
	.data : {
		_start = .;
		QUAD(dram_timing)	/* Pointer to dram_timing variable */
		*(.data*)		/* DDR RAM timing data */
		. = ALIGN(8);
		_end = .;
	} >.ocram
}
