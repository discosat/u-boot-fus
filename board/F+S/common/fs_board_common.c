/*
 * fs_board_common.c
 *
 * (C) Copyright 2018
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Common board configuration and information
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>
#include <common.h>			/* types, get_board_name(), ... */
#include <serial.h>			/* get_serial_device() */
#include <asm/gpio.h>			/* gpio_direction_output(), ... */
#include <asm/arch/sys_proto.h>		/* is_mx6*() */
#include <linux/mtd/rawnand.h>		/* struct mtd_info */
#include "fs_board_common.h"		/* Own interface */
#include "fs_mmc_common.h"

/* ------------------------------------------------------------------------- */

/* Addresses of arguments coming from NBoot and going to Linux */
#define NBOOT_ARGS_BASE (CONFIG_SYS_SDRAM_BASE + 0x00001000)
#define BOOT_PARAMS_BASE (CONFIG_SYS_SDRAM_BASE + 0x100)

#define ACTION_RECOVER 0x00000040	/* Start recovery instead of update */


/* String used for system prompt */
static char fs_sys_prompt[20];

/* Copy of the NBoot arguments, split into nboot_args and m4_args */
static struct fs_nboot_args nboot_args;
static struct fs_m4_args m4_args;

/* Store a pointer to the current board info */
static const struct fs_board_info *current_bi;

#if 0
/* List NBoot args; function can be activated and used for debugging */
void fs_board_show_nboot_args(struct fs_nboot_args *pargs)
{
	printf("dwNumDram = 0x%08x\n", pargs->dwNumDram);
	printf("dwMemSize = 0x%08x\n", pargs->dwMemSize);
	printf("dwFlashSize = 0x%08x\n", pargs->dwFlashSize);
	printf("dwDbgSerPortPA = 0x%08x\n", pargs->dwDbgSerPortPA);
	printf("chBoardType = 0x%02x\n", pargs->chBoardType);
	printf("chBoardRev = 0x%02x\n", pargs->chBoardRev);
	printf("chFeatures1 = 0x%02x\n", pargs->chFeatures1);
	printf("chFeatures2 = 0x%02x\n", pargs->chFeatures2);
}
#endif

/* Get a pointer to the NBoot args */
struct fs_nboot_args *fs_board_get_nboot_args(void)
{
	DECLARE_GLOBAL_DATA_PTR;

	/* As long as GD_FLG_RELOC is not set, we can not access variable
	   nboot_args and therefore have to use the NBoot args at
	   NBOOT_ARGS_BASE. However GD_FLG_RELOC may be set before the NBoot
	   arguments are actually copied from NBOOT_ARGS_BASE to nboot_args
	   (see fs_board_init_common() below). But then at least the .bss
	   section and therefore nboot_args is cleared. We check this by
	   looking at nboot_args.dwDbgSerPortPA. If this is 0, the
	   structure is not yet copied and we still have to look at
	   NBOOT_ARGS_BASE. Otherwise we can (and must) use nboot_args. */
	if ((gd->flags & GD_FLG_RELOC) && nboot_args.dwDbgSerPortPA)
		return &nboot_args;

	return (struct fs_nboot_args *)NBOOT_ARGS_BASE;
}

/* Get board type (zero-based) */
unsigned int fs_board_get_type(void)
{
	return fs_board_get_nboot_args()->chBoardType - CONFIG_FS_BOARD_OFFS;
}

/* Get board revision (major * 100 + minor, e.g. 120 for rev 1.20) */
unsigned int fs_board_get_rev(void)
{
	return fs_board_get_nboot_args()->chBoardRev;
}

/* Get the number of the debug port reported by NBoot */
static unsigned int get_debug_port(unsigned int dwDbgSerPortPA)
{
	unsigned int port = 6;
	struct serial_device *sdev;

	do {
		sdev = get_serial_device(--port);
		if (sdev && sdev->dev.priv == (void *)(ulong)dwDbgSerPortPA)
			return port;
	} while (port);

	return CONFIG_SYS_UART_PORT;
}

/* Get the number of the debug port reported by NBoot */
struct serial_device *default_serial_console(void)
{
#ifndef CONFIG_SPL_BUILD
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();

	return get_serial_device(get_debug_port(pargs->dwDbgSerPortPA));
#else
	return get_serial_device(CONFIG_SYS_UART_PORT);
#endif
}

/* Issue reset signal on up to three gpios (~0: gpio unused) */
void fs_board_issue_reset(uint active_us, uint delay_us,
			  uint gpio0, uint gpio1, uint gpio2)
{
	/* Assert reset */
	gpio_direction_output(gpio0, 0);
	if (gpio1 != ~0)
		gpio_direction_output(gpio1, 0);
	if (gpio2 != ~0)
		gpio_direction_output(gpio2, 0);

	/* Delay for the active pulse time */
	udelay(active_us);

	/* De-assert reset */
	gpio_set_value(gpio0, 1);
	if (gpio1 != ~0)
		gpio_set_value(gpio1, 1);
	if (gpio2 != ~0)
		gpio_set_value(gpio2, 1);

	/* Delay some more time if requested */
	if (delay_us)
		udelay(delay_us);
}

/* Copy NBoot args to variables and prepare command prompt string */
void fs_board_init_common(const struct fs_board_info *board_info)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct fs_nboot_args *pargs = (struct fs_nboot_args *)NBOOT_ARGS_BASE;

	/* Save a pointer to this board info */
	current_bi = board_info;

	/* Save a copy of the NBoot args */
	memcpy(&nboot_args, pargs, sizeof(struct fs_nboot_args));
	nboot_args.dwSize = sizeof(struct fs_nboot_args);
	memcpy(&m4_args, pargs + 1, sizeof(struct fs_m4_args));
	m4_args.dwSize = sizeof(struct fs_m4_args);

	gd->bd->bi_arch_number = 0xFFFFFFFF;
	gd->bd->bi_boot_params = BOOT_PARAMS_BASE;

	/* Prepare the command prompt */
	sprintf(fs_sys_prompt, "%s # ", board_info->name);
}

/* Set RAM size (as given by NBoot) and RAM base */
int dram_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();

	gd->ram_size = pargs->dwMemSize << 20;

	return 0;
}

void board_nand_state(struct mtd_info *mtd, unsigned int state)
{
	/* Save state to pass it to Linux later */
	nboot_args.chECCstate |= (unsigned char)state;
}

#ifdef CONFIG_CMD_UPDATE
enum update_action board_check_for_recover(void)
{
	char *recover_gpio;

	/* On some platforms, the check for recovery is already done in NBoot.
	   Then the ACTION_RECOVER bit in the dwAction value is set. */
	if (nboot_args.dwAction & ACTION_RECOVER)
		return UPDATE_ACTION_RECOVER;

	/*
	 * If a recover GPIO is defined, check if it is in active state. The
	 * variable contains the number of a gpio, followed by an optional '-'
	 * or '_', followed by an optional "high" or "low" for active high or
	 * active low signal. Actually only the first character is checked,
	 * 'h' and 'H' mean "high", everything else is taken for "low".
	 * Default is active low.
	 *
	 * Examples:
	 *    123_high  GPIO #123, active high
	 *    65-low    GPIO #65, active low
	 *    13        GPIO #13, active low
	 *    0x1fh     GPIO #31, active high (this shows why a dash or
	 *              underscore before "high" or "low" makes sense)
	 *
	 * Remark:
	 * We do not have any clue here what the GPIO represents and therefore
	 * we do not assume any pad settings. So for example if the GPIO
	 * represents a button that is floating in the released state, an
	 * external pull-up or pull-down must be used to avoid unintentionally
	 * detecting the active state.
	 */
	recover_gpio = env_get("recovergpio");
	if (recover_gpio) {
		char *endp;
		int active_state = 0;
		unsigned int gpio = simple_strtoul(recover_gpio, &endp, 0);

		if (endp != recover_gpio) {
			char c = *endp;

			if ((c == '-') || (c == '_'))
				c = *(++endp);
			if ((c == 'h') || (c == 'H'))
				active_state = 1;
			if (!gpio_direction_input(gpio)
			    && (gpio_get_value(gpio) == active_state))
				return UPDATE_ACTION_RECOVER;
		}
	}

	return UPDATE_ACTION_UPDATE;
}
#endif /* CONFIG_CMD_UPDATE */

#ifdef CONFIG_BOARD_LATE_INIT
/* If variable has value "undef", update it with a board specific value */
static void setup_var(const char *varname, const char *content, int runvar)
{
	char *envvar = env_get(varname);

	/* If variable is not set or does not contain string "undef", do not
	   change it */
	if (!envvar || strcmp(envvar, "undef"))
		return;

	/* Either set variable directly with value ... */
	if (!runvar) {
		env_set(varname, content);
		return;
	}

	/* ... or set variable by running the variable with name in content */
	content = env_get(content);
	if (content)
		run_command(content, 0);
}

/* Set up all board specific variables */
void fs_board_late_init_common(void)
{
	const char *envvar;
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();
#ifdef FS_MMC_COMMON
	int usdhc_boot_device = get_usdhc_boot_device();
	int mmc_boot_device = get_mmc_boot_device();
#else // support FSVYBRID
	int usdhc_boot_device = 0;
	int mmc_boot_device = 0;
#endif

	/* Set sercon variable if not already set */
	envvar = env_get("sercon");
	if (!envvar || !strcmp(envvar, "undef")) {
		char sercon[DEV_NAME_SIZE];

		sprintf(sercon, "%s%c", CONFIG_SYS_SERCON_NAME,
			'0' + get_debug_port(pargs->dwDbgSerPortPA));
		env_set("sercon", sercon);
	}

	/* Set platform variable if not already set */
	envvar = env_get("platform");
	if (!envvar || !strcmp(envvar, "undef")) {
		char lcasename[20];
		char *p = current_bi->name;
		char *l = lcasename;
		char c;

		do {
			c = *p++;
			if ((c >= 'A') && (c <= 'Z'))
				c += 'a' - 'A';
			*l++ = c;
		} while (c);

#ifdef CONFIG_MX6QDL
		/* In case of regular i.MX, add 'dl' or 'q' */
		if (is_mx6sdl())
			sprintf(lcasename, "%sdl", lcasename);
		else if (is_mx6dq())
			sprintf(lcasename, "%sq", lcasename);
#elif defined(CONFIG_MX6UL) || defined(CONFIG_MX6ULL)
		/*
		 * In case of i.MX6ULL, append a second 'l' if the name already
		 * ends with 'ul', otherwise append 'ull'. This results in the
		 * names efusa7ull, cubea7ull, picocom1.2ull, cube2.0ull, ...
		 */
		if (is_mx6ull()) {
			l--;
			/* Names have > 2 chars, so negative index is valid */
			if ((l[-2] != 'u') || (l[-1] != 'l')) {
				*l++ = 'u';
				*l++ = 'l';
			}
			*l++ = 'l';
			*l++ = '\0';
		}
#endif
		env_set("platform", lcasename);
	}

	/* Set mmcsdhc variable if not already set */
	envvar = env_get("usdhcdev");
	if (!envvar || !strcmp(envvar, "undef")) {
		char usdhcdev[DEV_NAME_SIZE];

		sprintf(usdhcdev, "%c", '0' + usdhc_boot_device);
		env_set("usdhcdev", usdhcdev);
	}

	/* Set mmcdev variable if not already set */
	envvar = env_get("mmcdev");
	if (!envvar || !strcmp(envvar, "undef")) {
		char mmcdev[DEV_NAME_SIZE];

		sprintf(mmcdev, "%c", '0' + mmc_boot_device);
		env_set("mmcdev", mmcdev);
	}

	/* Set some variables with a direct value */
	setup_var("bootdelay", current_bi->bootdelay, 0);
	setup_var("updatecheck", current_bi->updatecheck, 0);
	setup_var("installcheck", current_bi->installcheck, 0);
	setup_var("recovercheck", current_bi->recovercheck, 0);
	setup_var("mtdids", MTDIDS_DEFAULT, 0);
	setup_var("partition", MTDPART_DEFAULT, 0);
#ifdef CONFIG_FS_BOARD_MODE_RO
	setup_var("mode", "ro", 0);
#else
	setup_var("mode", "rw", 0);
#endif

	/* Set some variables by runnning another variable */
	setup_var("console", current_bi->console, 1);
	setup_var("login", current_bi->login, 1);
	setup_var("mtdparts", current_bi->mtdparts, 1);
	setup_var("network", current_bi->network, 1);
	setup_var("init", current_bi->init, 1);
	setup_var("rootfs", current_bi->rootfs, 1);
	setup_var("kernel", current_bi->kernel, 1);
	setup_var("bootfdt", "set_bootfdt", 1);
	setup_var("fdt", current_bi->fdt, 1);
	setup_var("bootargs", "set_bootargs", 1);
}
#endif /* CONFIG_BOARD_LATE_INIT */

/* Return the board name (board specific) */
char *get_board_name(void)
{
	return current_bi->name;
}

/* Return the system prompt (board specific) */
char *get_sys_prompt(void)
{
	return fs_sys_prompt;
}
