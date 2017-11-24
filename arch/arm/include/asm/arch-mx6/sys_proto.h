/*
 * (C) Copyright 2009
 * Stefano Babic, DENX Software Engineering, sbabic@denx.de.
 *
 * (C) Copyright 2009-2015 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _SYS_PROTO_H_
#define _SYS_PROTO_H_

#include <asm/imx-common/regs-common.h>
#include "../arch-imx/cpu.h"

#define soc_rev() (get_cpu_rev() & 0xFF)
#define is_soc_rev(rev)        (int)(soc_rev() - rev)

u32 get_nr_cpus(void);
u32 get_cpu_rev(void);

/* returns MXC_CPU_ value */
#define cpu_type(rev) (((rev) >> 12)&0xff)

/* both macros return/take MXC_CPU_ constants */
#define get_cpu_type()	(cpu_type(get_cpu_rev()))
#define is_cpu_type(cpu) (get_cpu_type() == cpu)

const char *get_imx_type(u32 imxtype);
unsigned imx_ddr_size(void);

/*
 * Initializes on-chip ethernet controllers.
 * to override, implement board_eth_init()
 */

int fecmxc_initialize(bd_t *bis);
u32 get_ahb_clk(void);
u32 get_periph_clk(void);

int mxs_reset_block(struct mxs_register_32 *reg);
int mxs_wait_mask_set(struct mxs_register_32 *reg,
		       uint32_t mask,
		       unsigned int timeout);
int mxs_wait_mask_clr(struct mxs_register_32 *reg,
		       uint32_t mask,
		       unsigned int timeout);


/* For the bootaux command we implemented a state machine to switch between
 * the different modes. Below you find the bit coding for the state machine.
 * The different states will be set in the arch_auxiliary_core_set function and
 * to get the current state you can use the function arch_auxiliary_core_get.
 * If we get a undefined state we will immediately set the state to aux_off.
 */
/******************************************************************************
****************|              bit coding             | state |****************
-------------------------------------------------------------------------------
****************| assert_reset | m4_clock | m4_enable | state |****************
===============================================================================
****************|       1      |     0    |     0     |    off    |************
****************|       1      |     1    |     1     |  stopped  |************
****************|       0      |     1    |     1     |  running  |************
****************|       0      |     0    |     1     |  paused   |************
****************|       0      |     0    |     0     | undefined |************
****************|       0      |     1    |     0     | undefined |************
****************|       1      |     0    |     1     | undefined |************
****************|       1      |     1    |     0     | undefined |************
*******************************************************************************
**|   transitions    |   state   |            transitions             |********
*******************************************************************************
                      -----------
                      |   OFF   |
                      -----------
          |          |           ^           ^                   ^
          |    Start |           |  off      |                   |
          |          v           |           |                   |
          |           -----------            |                   |
    run/  |           | Stopped |            | off               |
          |           -----------            |                   |
    addr  |          |           ^           |           ^       |
          | run/addr |           | stop      |           |       | off
          v          v           |                       |       |
                      -----------                        |       |
                      | Running |                        | stop  |
                      -----------                        |       |
                     |           ^                       |       |
               pause |           | continue / run / addr |       |
                     v           |
                      -----------
                      | Paused  |
                      -----------
******************************************************************************/
enum aux_state {
	aux_off,
	aux_stopped,
	aux_running,
	aux_paused,
	aux_undefined,
};

int arch_auxiliary_core_set(u32 core_id, enum aux_state state, u32 boot_private_data);
enum aux_state arch_auxiliary_core_get(u32 core_id);

#endif
