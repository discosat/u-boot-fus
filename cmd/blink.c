/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Blink timer support, can be used for LEDs, cursor, etc.
 *
 * (C) Copyright 2014
 * F&S Elektronik Systeme GmbH <keller@fs-net.d>
 */

#include <common.h>
#include <blink.h>

#ifndef CONFIG_SYS_BLINK_CLIENTS
#define CONFIG_SYS_BLINK_CLIENTS 1
#endif

#define BLINK_DELAY_DEF	500

static unsigned int blink_delay = BLINK_DELAY_DEF;
static int is_blinking;

struct blink_handler {
	void *data;
	interrupt_handler_t *callback;
};

static struct blink_handler blink_handler[CONFIG_SYS_BLINK_CLIENTS];

/* This function is called in interrupt context from the hardware specific
   blink timer ISR; it calls all the registered blink callbacks */
void run_blink_callbacks(void)
{
	int i;

	for (i = 0; i < CONFIG_SYS_BLINK_CLIENTS; i++) {
		struct blink_handler *bh = &blink_handler[i];

		if (bh->callback)
			bh->callback(bh->data);
	}
}

/* Add a blink callback function to the list of blinking elements */
int add_blink_callback(interrupt_handler_t *callback, void *data)
{
	int i;

	/* Look for a free slot for this callback */
	for (i = 0; i < CONFIG_SYS_BLINK_CLIENTS; i++) {
		struct blink_handler *bh = &blink_handler[i];

		if (!bh->callback) {
			bh->data = data;
			bh->callback = callback;

			if (!is_blinking) {
				is_blinking = 1;
				start_blink_timer(blink_delay);
			}
			return 0;
		}
	}

	/* No more slots */
	return -1;
}

/* Remove the given blink callback */
void remove_blink_callback(interrupt_handler_t *callback)
{
	int need_blink = 0;
	int i;

	for (i = 0; i < CONFIG_SYS_BLINK_CLIENTS; i++) {
		struct blink_handler *bh = &blink_handler[i];

		if (bh->callback == callback)
			bh->callback = NULL;
		else if (bh->callback)
			need_blink = 1;
	}

	if (is_blinking && !need_blink) {
		is_blinking = 0;
		stop_blink_timer();
	}
}

int do_blink(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (argc > 2)
		return CMD_RET_USAGE;

	if (argc == 2) {
		blink_delay = simple_strtoul(argv[1], NULL, 0);
		if (!blink_delay)
			blink_delay = BLINK_DELAY_DEF;

		if (is_blinking)
			set_blink_timer(blink_delay);
	}

	printf("Blink delay %d ms\n", blink_delay);

	return 0;
}

U_BOOT_CMD(
	blink, 2, 1, do_blink,
	"set speed of all blinking elements (leds, etc.)",
	"delay - Set blink delay (in ms)\n"
	"blink - Show current blink delay (in ms)\n"
);
