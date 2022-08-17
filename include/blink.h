/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Blinking support
 *
 * (C) Copyright 2022
 * F&S Elektronik Systeme GmbH <keller@fs-net.de>
 */

#ifndef __BLINK_H__
#define __BLINK_H__	1

#include <irq_func.h>

extern int start_blink_timer(unsigned int);
extern void stop_blink_timer(void);
extern void set_blink_timer(unsigned int);
extern void run_blink_callbacks(void);
extern int add_blink_callback(interrupt_handler_t *, void *);
extern void remove_blink_callback(interrupt_handler_t *);

#endif
