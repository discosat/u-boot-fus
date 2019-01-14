/*
 * (C) Copyright 2010
 * Jason Kridner <jkridner@beagleboard.org>
 *
 * (C) Copyright 2014
 * F&S Elektronik Systeme GmbH <keller@fs-net.d>
 *
 * Based on cmd_led.c patch from:
 * http://www.mail-archive.com/u-boot@lists.denx.de/msg06873.html
 * (C) Copyright 2008
 * Ulf Samuelsson <ulf.samuelsson@atmel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <config.h>
#include <command.h>
#include <status_led.h>

struct led_tbl_s {
	char		*string;	/* String for use in the command */
	led_id_t	mask;		/* Mask used for calling __led_set() */
	void		(*off)(void);	/* Optional function for turning LED off */
	void		(*on)(void);	/* Optional function for turning LED on */
	void		(*toggle)(void);/* Optional function for toggling LED */
};

typedef struct led_tbl_s led_tbl_t;

/************************************************************************
 * Coloured LED functionality
 ************************************************************************
 * May be supplied by boards if desired
 */
static inline void __coloured_LED_init(void) {}
void coloured_LED_init(void)
	__attribute__((weak, alias("__coloured_LED_init")));

#define led_function_group(led, bit)		\
static void led ## _off(void)			\
{						\
	__led_set(bit, 0);			\
}						\
static void led ## _on(void)			\
{						\
	__led_set(bit, 1);			\
}						\
static void led ## _toggle(void)		\
{						\
	__led_toggle(bit);			\
}						\

#ifdef CONFIG_LED_STATUS_BIT
led_function_group(led0, CONFIG_LED_STATUS_BIT)
#endif
#ifdef CONFIG_LED_STATUS_BIT1
led_function_group(led1, CONFIG_LED_STATUS_BIT1)
#endif
#ifdef CONFIG_LED_STATUS_BIT2
led_function_group(led2, CONFIG_LED_STATUS_BIT2)
#endif
#ifdef CONFIG_LED_STATUS_BIT3
led_function_group(led3, CONFIG_LED_STATUS_BIT3)
#endif
#ifdef CONFIG_LED_STATUS_BIT4
led_function_group(led4, CONFIG_LED_STATUS_BIT4)
#endif
#ifdef CONFIG_LED_STATUS_BIT5
led_function_group(led5, CONFIG_LED_STATUS_BIT5)
#endif

static const led_tbl_t led_commands[] = {
#ifdef CONFIG_LED_STATUS_BOARD_SPECIFIC
#ifdef CONFIG_LED_STATUS0
	{ "0", CONFIG_LED_STATUS_BIT, led0_off, led0_on, led0_toggle },
#endif
#ifdef CONFIG_LED_STATUS1
	{ "1", CONFIG_LED_STATUS_BIT1, led1_off, led1_on, led1_toggle },
#endif
#ifdef CONFIG_LED_STATUS2
	{ "2", CONFIG_LED_STATUS_BIT2, led2_off, led2_on, led2_toggle },
#endif
#ifdef CONFIG_LED_STATUS3
	{ "3", CONFIG_LED_STATUS_BIT3, led3_off, led3_on, led3_toggle },
#endif
#ifdef CONFIG_LED_STATUS4
	{ "4", CONFIG_LED_STATUS_BIT4, led4_off, led4_on, led4_toggle },
#endif
#ifdef CONFIG_LED_STATUS5
	{ "5", CONFIG_LED_STATUS_BIT5, led5_off, led5_on, led5_toggle },
#endif
#endif
#ifdef CONFIG_LED_STATUS_GREEN
	{ "green", CONFIG_LED_STATUS_GREEN, green_led_off, green_led_on, NULL },
#endif
#ifdef CONFIG_LED_STATUS_YELLOW
	{ "yellow", CONFIG_LED_STATUS_YELLOW, yellow_led_off, yellow_led_on,
	  NULL },
#endif
#ifdef CONFIG_LED_STATUS_RED
	{ "red", CONFIG_LED_STATUS_RED, red_led_off, red_led_on, NULL },
#endif
#ifdef CONFIG_LED_STATUS_BLUE
	{ "blue", CONFIG_LED_STATUS_BLUE, blue_led_off, blue_led_on, NULL },
#endif
};

#define LED_OFF		0
#define LED_ON		1
#define LED_TOGGLE	2
#define LED_BLINK       4

static int led_state[ARRAY_SIZE(led_commands)];

static void led_toggle(const struct led_tbl_s *led_command, int led)
{
	if (led_command->toggle)
		led_command->toggle();
	else {
		led_state[led] ^= LED_ON;
		if (led_state[led] & LED_ON)
			led_command->on();
		else
			led_command->off();
	}
}

#ifdef CONFIG_CMD_BLINK
/* Interrupt service routine for blinking the LEDs, must not block */
static void led_blink_callback(void *data)
{
	int led;

	for (led = 0; led < ARRAY_SIZE(led_commands); led++) {
		if (led_state[led] & LED_BLINK)
			led_toggle(&led_commands[led], led);
	}
}
#endif

int do_led(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int led, match = 0;
	int cmd;
#ifdef CONFIG_CMD_BLINK
	static int have_blink;
	int need_blink = 0;
#endif

	/* Validate arguments */
	if (argc != 3)
		return CMD_RET_USAGE;

	if (strcmp(argv[2], "off") == 0)
		cmd = LED_OFF;
	else if (strcmp(argv[2], "on") == 0)
		cmd = LED_ON;
	else if (strcmp(argv[2], "toggle") == 0)
		cmd = LED_TOGGLE;
#ifdef CONFIG_CMD_BLINK
	else if (strcmp(argv[2], "blink") == 0)
		cmd = LED_BLINK;
#endif
	else
		return CMD_RET_USAGE;

	/* Handle LED action */
	for (led = 0; led < ARRAY_SIZE(led_commands); led++) {
		const struct led_tbl_s *led_command = &led_commands[led];

		if ((strcmp("all", argv[1]) == 0) ||
		    (strcmp(led_command->string, argv[1]) == 0)) {
			match = 1;
			switch (cmd) {
			case LED_OFF:
				led_state[led] = cmd;
				if (led_command->off)
					led_command->off();
				break;
			case LED_ON:
				led_state[led] = cmd;
				if (led_command->on)
					led_command->on();
				break;
			case LED_TOGGLE:
				led_state[led] &= LED_ON;
				led_toggle(led_command, led);
				break;
#ifdef CONFIG_CMD_BLINK
			case LED_BLINK:
				led_state[led] |= LED_BLINK;
				break;
#endif
			}
		}
#ifdef CONFIG_CMD_BLINK
		need_blink |= led_state[led] & LED_BLINK;
#endif
	}

	/* If we ran out of matches, print Usage */
	if (!match)
		return CMD_RET_USAGE;

#ifdef CONFIG_CMD_BLINK
	/* If there was at least one blinking LED, activate the (interrupt
	   based) blink timer with the requested rate; otherweise deactivate
	   the blink timer */
	if (need_blink != have_blink) {
		have_blink = need_blink;
		if (need_blink)
			add_blink_callback(led_blink_callback, NULL);
		else
			remove_blink_callback(led_blink_callback);
	}
#endif

	return 0;
}

U_BOOT_CMD(
	led, 4, 1, do_led,
	"["
#ifdef CONFIG_LED_STATUS_BOARD_SPECIFIC
#ifdef CONFIG_LED_STATUS0
	"0|"
#endif
#ifdef CONFIG_LED_STATUS1
	"1|"
#endif
#ifdef CONFIG_LED_STATUS2
	"2|"
#endif
#ifdef CONFIG_LED_STATUS3
	"3|"
#endif
#ifdef CONFIG_LED_STATUS4
	"4|"
#endif
#ifdef CONFIG_LED_STATUS5
	"5|"
#endif
#endif
#ifdef CONFIG_LED_STATUS_GREEN
	"green|"
#endif
#ifdef CONFIG_LED_STATUS_YELLOW
	"yellow|"
#endif
#ifdef CONFIG_LED_STATUS_RED
	"red|"
#endif
#ifdef CONFIG_LED_STATUS_BLUE
	"blue|"
#endif
	"all] [on|off|toggle"
#ifdef CONFIG_CMD_BLINK
	"|blink"
#endif
	"]",
	"[led_name] [on|off|toggle"
#ifdef CONFIG_CMD_BLINK
	"|blink"
#endif
	"] sets or clears led(s)"
);
