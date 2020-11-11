/*
 * fsimx8mm.c
 *
 * (C) Copyright 2019
 * Anatol Derksen, F&S Elektronik Systeme GmbH, derksen@fs-net.de
 *
 * Board specific functions for F&S boards based on Freescale i.MX8MM CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <dm.h>
#include <errno.h>
#include <asm/io.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm-generic/gpio.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <asm/arch/imx8mm_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/gpio.h>
#include <i2c.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/clock.h>
#include <spl.h>
#include <asm/mach-imx/dma.h>
#include <power/bd71837.h>
#include <usb.h>
#include <sec_mipi_dsim.h>
#include <imx_mipi_dsi_bridge.h>
#include <mipi_dsi_panel.h>
#include <asm/mach-imx/video.h>
#include <serial.h>			/* get_serial_device() */
#include "../common/fs_fdt_common.h"	/* fs_fdt_set_val(), ... */
#include "../common/fs_board_common.h"	/* fs_board_*() */
#include <nand.h>
#include "sec_mipi_dphy_ln14lpp.h"
#include "sec_mipi_pll_1432x.h"

/* ------------------------------------------------------------------------- */

DECLARE_GLOBAL_DATA_PTR;

#define BT_TBS2				0

/* Features set in fs_nboot_args.chFeature2 (available since NBoot VN27) */
#define FEAT2_ETH		(1<<0)		/* 0: no LAN, 1; has LAN */
#define FEAT2_EMMC		(1<<1)		/* 0: no eMMC, 1: has eMMC */
#define FEAT2_WLAN		(1<<2)		/* 0: no WLAN, 1: has WLAN */

#define INSTALL_RAM "ram@43800000"
#if defined(CONFIG_MMC) && defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
#define UPDATE_DEF "mmc,usb"
#define INSTALL_DEF INSTALL_RAM "," UPDATE_DEF
#elif defined(CONFIG_MMC) && defined(CONFIG_FS_FAT)
#define UPDATE_DEF "mmc"
#define INSTALL_DEF INSTALL_RAM "," UPDATE_DEF
#elif defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
#define UPDATE_DEF "usb"
#define INSTALL_DEF INSTALL_RAM "," UPDATE_DEF
#else
#define UPDATE_DEF NULL
#define INSTALL_DEF INSTALL_RAM
#endif

#define ROOTFS ".rootfs_mmc"
#define KERNEL ".kernel_mmc"
#define FDT ".fdt_mmc"

#define RDC_PDAP70     0x303d0518
#define FDT_UART_C	"/serial@30a60000"


const struct fs_board_info board_info[1] = {
	{	/* 0 (BT_TBS2) */
		.name = "TBS2",
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_init",
		.rootfs = ROOTFS,
		.kernel = KERNEL,
		.fdt = FDT,
	},
};

/* ---- Stage 'f': RAM not valid, variables can *not* be used yet ---------- */

int board_early_init_f(void)
{
	return 0;
}

/* Check board type */
int checkboard(void)
{
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();
	unsigned int features2;

	features2 = pargs->chFeatures2;

	printf("Board: %s Rev %u.%02u (", board_info[board_type].name,
		board_rev / 100, board_rev % 100);
	if (features2 & FEAT2_ETH)
		puts("LAN, ");
	if (features2 & FEAT2_WLAN)
		puts("WLAN, ");
	if (features2 & FEAT2_EMMC)
		puts("eMMC, ");
	printf("%dx DRAM)\n", pargs->dwNumDram);

	//fs_board_show_nboot_args(pargs);

	return 0;
}

/* ---- Stage 'r': RAM valid, U-Boot relocated, variables can be used ------ */
static int setup_fec(void);
int board_init(void)
{
	unsigned int board_type = fs_board_get_type();

	/* Copy NBoot args to variables and prepare command prompt string */
	fs_board_init_common(&board_info[board_type]);

#ifdef CONFIG_FEC_MXC
	setup_fec();
#endif

	return 0;
}

/*
 * SD/MMC support.
 *
 *   Board          USDHC   CD-Pin                 Slot
 *   -----------------------------------------------------------------------
 *            TBS2: USDHC1  GPIO_1 (GPIO1_IO06)    On-board (micro-SD)
 *          either: [USDHC2 -                      WLAN]
 *              or: USDHC3  -                      eMMC (8-Bit)
 *   -----------------------------------------------------------------------
 *
 * Remark: The WP pin is ignored in U-Boot, also WLAN.
 */

int mmc_map_to_kernel_blk(int devno)
{
	return devno + 1;
}

/*
 * USB Host support.
 *
 * USB0 is OTG. By default this is used as device port. However on some F&S
 * boards this port may optionally be configured as a second host port. So if
 * environment variable usb0mode is set to "host" on these boards, or if it is
 * set to "otg" and the ID pin is low when usb is started, use host mode.
 *
 *    Board               USB_OTG_PWR              USB_OTG_ID
 *    --------------------------------------------------------------------
 *    TBS2                GPIO1 (GPIO1_IO12)       - (dedicated)
 *
 * (*) Signal on SKIT is active low, usually USB_OTG_PWR is active high
 *
 * USB1 is a host-only port (USB_H1). It is used on all boards. Some boards
 * may have an additional USB hub with a reset signal connected to this port.
 *
 *    Board               USB_H1_PWR               Hub Reset
 *    -------------------------------------------------------------------------
 *    TBS2                GPIO1 (GPIO1_IO12)       -
 *
 * The polarity for the VBUS power can be set with environment variable
 * usbxpwr, where x is the port index (0 or 1). If this variable is set to
 * "low", the power pin is active low, if it is set to "high", the power pin
 * is active high. Default is board-dependent, so that when F&S SKITs are
 * used, only usbxmode must be set.
 *
 * Example: setenv usb1pwr low
 *
 * Usually the VBUS power for a host port is connected to a dedicated pin, i.e.
 * USB_H1_PWR or USB_OTG_PWR. Then the USB controller can switch power
 * automatically and we only have to tell the controller whether this signal is
 * active high or active low. In all other cases, VBUS power is simply handled
 * by a regular GPIO.
 *
 * If CONFIG_FS_USB_PWR_USBNC is set, the dedicated PWR function of the USB
 * controller will be used to switch host power (where available). Otherwise
 * the host power will be switched by using the pad as GPIO.
 */


int board_usb_init(int index, enum usb_init_type init)
{
	debug("board_usb_init %d, type %d\n", index, init);

	imx8m_usb_power(index, true);
	return 0;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	debug("board_usb_cleanup %d, type %d\n", index, init);

	imx8m_usb_power(index, false);
	return 0;
}

#ifdef CONFIG_BOARD_LATE_INIT
/*
 * Use this slot to init some final things before the network is started. The
 * F&S configuration heavily depends on this to set up the board specific
 * environment, i.e. environment variables that can't be defined as a constant
 * value at compile time.
 */
int board_late_init(void)
{
	/* Remove 'fdtcontroladdr' env. because we are using
	 * compiled-in version. In this case it is not possible
	 * to use this env. as saved in NAND flash. (s. readme for fdt control)
	 */
	env_set("fdtcontroladdr", "");
	/* Set up all board specific variables */
	fs_board_late_init_common("ttymxc");

	return 0;
}
#endif /* CONFIG_BOARD_LATE_INIT */

#ifdef CONFIG_CMD_NET
#define FEC_RST_PAD IMX_GPIO_NR(1, 5)
static iomux_v3_cfg_t const fec1_rst_pads[] = {
	IMX8MM_PAD_GPIO1_IO05_GPIO1_IO5 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void setup_iomux_fec(void)
{
	unsigned int board_type = fs_board_get_type();

	switch(board_type) {
	case BT_TBS2:
		SETUP_IOMUX_PADS(fec1_rst_pads);

		gpio_request(FEC_RST_PAD, "fec1_rst");
		gpio_direction_output(FEC_RST_PAD, 0);
		udelay (1000);
		gpio_direction_output(FEC_RST_PAD, 1);
		udelay (100);
		break;
	}
}

static int setup_fec(void)
{
	struct iomuxc_gpr_base_regs *const iomuxc_gpr_regs =
			(struct iomuxc_gpr_base_regs*) IOMUXC_GPR_BASE_ADDR;
	unsigned int board_type = fs_board_get_type();
	enum enet_freq freq;

	setup_iomux_fec();

	switch(board_type) {
	case BT_TBS2:
		/* external 25 MHz oscilator REF_CLK => 50 MHz */
		clrbits_le32(&iomuxc_gpr_regs->gpr[1],
				IOMUXC_GPR_GPR1_GPR_ENET1_TX_CLK_SEL_MASK);
		freq = ENET_50MHZ;
		break;
	}
	return set_clk_enet(freq);
}

int board_phy_config(struct phy_device *phydev)
{
	unsigned int board_type = fs_board_get_type();
	u16 reg = 0;

	switch(board_type) {
	case BT_TBS2:
		/* do not use KSZ8081RNA specific config funcion.
		 * This function says clock input to XI is 50 MHz, but
		 * we have an 25 MHz oscilator, so we need to set
		 * bit 7 to 0 (register 0x1f)
		 */
		reg = phy_read(phydev, 0x0, 0x1f);
		reg &= 0xff7f;
		phy_write(phydev, 0x0, 0x1f, reg);
		break;
	}

	return 0;
}
#endif /* CONFIG_CMD_NET */

#ifdef CONFIG_OF_BOARD_SETUP
/* Do any additional board-specific device tree modifications */
int ft_board_setup(void *fdt, bd_t *bd)
{
	int offs;
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();
	const char *envvar;

	/* Set bdinfo entries */
	offs = fs_fdt_path_offset(fdt, "/bdinfo");
	if (offs >= 0)
	{
		int id = 0;
		/* Set common bdinfo entries */
		fs_fdt_set_bdinfo(fdt, offs);

		/* MAC addresses */
		if (pargs->chFeatures2 & FEAT2_ETH)
			fs_fdt_set_macaddr(fdt, offs, id++);

		if (pargs->chFeatures2 & FEAT2_WLAN)
			fs_fdt_set_macaddr(fdt, offs, id++);

	/*TODO: Its workaround to use UART4 */
	envvar = env_get("m4_uart4");

		if (!envvar || !strcmp(envvar, "disable")) {
			/* Disable UART4 for M4. Enabled by ATF. */
			writel(0xff, RDC_PDAP70);
		} else {
			/* Disable UART_C in DT */
			fs_fdt_enable(fdt, FDT_UART_C, 0);
		}
	}

	return 0;
}
#endif /* CONFIG_OF_BOARD_SETUP */

#ifdef CONFIG_BOARD_POSTCLK_INIT
int board_postclk_init(void)
{
	/* TODO */
	return 0;
}
#endif /* CONFIG_BOARD_POSTCLK_INIT */
