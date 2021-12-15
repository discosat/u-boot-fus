/*
 * Copyright 2018 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <spl.h>
#include <asm/arch/clock.h>
#include <asm/arch/sci/sci.h>
#include <asm/arch/imx8-pins.h>
#include <asm/arch/snvs_security_sc.h>
#include <asm/arch/iomux.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/lpcg.h>
#include <dm/uclass.h>
#include <dm/device.h>
#include <dm/uclass-internal.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <malloc.h>
#include <bootm.h>
#include <hang.h>
#include <fdt_support.h>
#include <power-domain.h>
#include <asm/mach-imx/boot_mode.h>	/* BOOT_TYPE_* */
#include "../common/fs_image_common.h"	/* fs_image_*() */
#include "../common/fs_board_common.h"	/* fs_board_*() */


DECLARE_GLOBAL_DATA_PTR;

#define BT_EFUSMX8X 0x0

static const char *board_names[] = {
	"efusMX8X",
	"(unknown)"
};

static unsigned int board_type;
static unsigned int board_rev;
static const char *board_name;
static const char *board_fdt;
static enum boot_device used_boot_dev;	/* Boot device used for NAND/MMC */
static bool boot_dev_init_done;
static unsigned int uboot_offs;
static bool secondary;			/* 0: primary, 1: secondary SPL */

#define UART_PAD_CTRL	((SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | \
			 (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
			 (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | \
			 (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

static iomux_cfg_t uart_pads[] = {
	SC_P_UART2_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	SC_P_UART2_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

/* Setup and start serial debug port */
static void config_uart(int board_type)
{
	switch (board_type)
	{
	default:
	case BT_EFUSMX8X:
		{
			sc_pm_clock_rate_t rate = 80000000;

			/* Power up UART2 */
			sc_pm_set_resource_power_mode(-1, SC_R_UART_2, SC_PM_PW_MODE_ON);

			sc_pm_set_clock_rate(-1, SC_R_UART_2, 2, &rate);

			/* Enable UART2 clock root */
			sc_pm_clock_enable(-1, SC_R_UART_2, 2, true, false);

			lpcg_all_clock_on(LPUART_2_LPCG);

			/* Setup UART pads */
			imx8_iomux_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));
		}
		break;
	}

	preloader_console_init();
}

/* Configure (and optionally start) the given boot device */
static int fs_spl_init_boot_dev(enum boot_device boot_dev, bool start,
				const char *type)
{
	if (boot_dev_init_done)
		return 0;

	used_boot_dev = boot_dev;
	switch (boot_dev) {
#if 0
#ifdef CONFIG_NAND_MXS
	case NAND_BOOT:
		fs_spl_init_nand_pads();
		if (start)
			nand_init();
		break;
#endif
#ifdef CONFIG_MMC
	case MMC3_BOOT:
		fs_spl_init_emmc_pads();
		if (start)
			mmc_initialize(NULL);
		break;
		//### TODO: Also have setups for MMC1_BOOT and MMC2_BOOT
#endif
#endif
	case USB_BOOT:
		/* Nothing to do */
		break;

	default:
		printf("Can not handle %s boot device %s\n", type,
		       fs_board_get_name_from_boot_dev(boot_dev));
		return -ENODEV;
	}

	return 0;
}

static void basic_init(void)
{
	void *fdt = fs_image_get_cfg_addr(false);
	int offs = fs_image_get_cfg_offs(fdt);
	int i;
	char c;
	int index;
	const char *boot_dev_name;
	enum boot_device boot_dev;

	board_name = fdt_getprop(fdt, offs, "board-name", NULL);
	for (i = 0; i < ARRAY_SIZE(board_names); i++) {
		if (!strcmp(board_name, board_names[i]))
			break;
	}
	board_type = i;

	/*
	 * If an fdt-name is not given, use board name in lower case. Please
	 * note that this name is only used for the U-Boot device tree. The
	 * Linux device tree name is defined by executing U-Boot's environment
	 * variable set_bootfdt.
	 */
	board_fdt = fdt_getprop(fdt, offs, "board-fdt", NULL);
	if (!board_fdt) {
		static char board_name_lc[32];

		i = 0;
		do {
			c = board_name[i];
			if ((c >= 'A') && (c <= 'Z'))
				c += 'a' - 'A';
			board_name_lc[i++] = c;
		} while (c);

		board_fdt = (const char *)&board_name_lc[0];
	}

	board_rev = fdt_getprop_u32_default_node(fdt, offs, 0,
						 "board-rev", 100);
	config_uart(board_type);

/* Dual bootloader feature will require CAAM access, but JR0 and JR1 will be
 * assigned to seco for imx8, use JR3 instead.
 */
#if defined(CONFIG_DUAL_BOOTLOADER)
	sc_pm_set_resource_power_mode(-1, SC_R_CAAM_JR3, SC_PM_PW_MODE_ON);
	sc_pm_set_resource_power_mode(-1, SC_R_CAAM_JR3_OUT, SC_PM_PW_MODE_ON);
#endif

	if (secondary)
		puts("Warning! Running secondary SPL, please check if"
		     " primary SPL is damaged.\n");

	boot_dev_name = fdt_getprop(fdt, offs, "boot-dev", NULL);
	boot_dev = fs_board_get_boot_dev_from_name(boot_dev_name);

	/* Get U-Boot offset */
#ifdef CONFIG_FS_UPDATE_SUPPORT
	index = 0;			/* ### TODO: Select slot A or B */
#else
	index = 0;
#endif
	offs = fs_image_get_info_offs(fdt);
	uboot_offs = fdt_getprop_u32_default_node(fdt, offs, index,
						  "uboot-start", 0);

	/* We need to have the boot device pads active when starting U-Boot */
	fs_spl_init_boot_dev(boot_dev, false, "BOARD-CFG");

}

void spl_board_prepare_for_boot(void)
{
	board_quiesce_devices();
}

void board_init_f(ulong dummy)
{
	int ret;
	struct udevice *dev;
	enum boot_device boot_dev;

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	arch_cpu_init();

	timer_init();

	/* Init malloc_f pool, boot stages and driver model for SCU */
	ret = spl_init();
	if (ret) {
		debug("spl_init() failed: %d\n", ret);
		hang();
	}

	/* Initialize SCU with driver model */
	uclass_find_first_device(UCLASS_MISC, &dev);
	for (; dev; uclass_find_next_device(&dev)) {
		if (device_probe(dev))
			continue;
	}

#if 0
	/*
	 * Enable this to have early debug output before BOARD-CFG is loaded
	 * You have to provide the board type, we do not know it yet
	 */
	config_uart(BT_EFUSMX8X);
#endif

#if 0
	// ### TODO: How do we determine this on i.MX8X?
	{
		struct src *src;
		/* Determine if we are running on primary or secondary SPL */
		src = (struct src *)SRC_BASE_ADDR;
		if (readl(&src->gpr10) & (1 << 30))
			secondary = true;
	}
#endif

	/* Try loading from the current boot dev. If this fails, try USB. */
	boot_dev = get_boot_device();
	if (boot_dev != USB_BOOT) {
		if (fs_spl_init_boot_dev(boot_dev, true, "current")
		    || fs_image_load_system(boot_dev, secondary, basic_init))
			boot_dev = USB_BOOT;
	}
	if (boot_dev == USB_BOOT) {
		bool need_cfg = true;

		/* Try loading a BOARD-CFG from the fused boot device first */
		boot_dev = fs_board_get_boot_dev_from_fuses();
		if (!fs_spl_init_boot_dev(boot_dev, true, "fused")
		    && !fs_image_load_system(boot_dev, secondary, NULL))
			need_cfg = false;

		/* Load the system from USB with Serial Download Protocol */
		fs_image_all_sdp(need_cfg, basic_init);
	}

	/* At this point we have a valid system configuration */
	board_init_r(NULL, 0);
}

void spl_board_init(void)
{

#ifndef CONFIG_SPL_USB_SDP_SUPPORT
	/* Serial download mode */
	if (is_usb_boot()) {
		puts("Back to ROM, SDP\n");
		restore_boot_params();
	}
#endif
	puts("Normal Boot\n");
}

#ifdef CONFIG_SPL_LOAD_FIT
/*
 * This function is called for each appended device tree. If we signal a match
 * (return value 0), the referenced device tree (and only this) is loaded
 * behind U-Boot. So from the view of U-Boot, it always has the right device
 * tree when starting. See doc/README.multi-dtb-fit for details.
 */
int board_fit_config_name_match(const char *name)
{
	return strcmp(name, board_fdt);
}
#endif
