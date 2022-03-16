/*
 * fsimx7ulp.c
 *
 * (C) Copyright 2020
 * Patrik Jakob, F&S Elektronik Systeme GmbH, jakob@fs-net.de
 * Philipp Gerbach, F&S Elektronik Systeme GmbH, gerbach@fs-net.de
 *
 * Board specific functions for F&S boards based on Freescale i.MX7ULP CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <mmc.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/mx7ulp-pins.h>
#include <asm/arch/iomux.h>
#include <asm/gpio.h>
#include <usb.h>
#include <dm.h>
#include <environment.h>		/* enum env_operation */
#include "../common/fs_fdt_common.h"	/* fs_fdt_set_val(), ... */
#include "../common/fs_board_common.h"	/* fs_board_*() */
#include "../common/fs_eth_common.h"	/* fs_eth_*() */

/* ------------------------------------------------------------------------- */

DECLARE_GLOBAL_DATA_PTR;

static struct fs_nboot_args nbootargs;

#define BT_PCOREMX7ULP 	0

/* Board features; these values can be resorted and redefined at will */
#define FEAT2_EMMC	(1<<0)
#define FEAT2_RTC85063	(1<<1)
#define FEAT2_QSPI	(1<<2)
#define FEAT2_AUDIO	(1<<3)
#define FEAT2_WLAN	(1<<4)
#define FEAT2_MIPI_DSI	(1<<5)


#define UART_PAD_CTRL	(PAD_CTL_PUS_UP)
#define QSPI_PAD_CTRL1	(PAD_CTL_PUS_UP | PAD_CTL_DSE)
#define OTG_ID_GPIO_PAD_CTRL	(PAD_CTL_IBE_ENABLE | PAD_CTL_PUS_UP | \
				 PAD_CTL_PUE)
#define PWR_GPIO_PAD_CTRL	(PAD_CTL_OBE_ENABLE | PAD_CTL_PUS_UP | \
				 PAD_CTL_PUE)
#define MIPI_GPIO_PAD_CTRL	(PAD_CTL_OBE_ENABLE)

#define INSTALL_RAM "ram@60300000"
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

const struct fs_board_info board_info[] = {
	{	/* 0 (BT_PCOREMX7ULP) */
		.name = "PicoCoreMX7ULP",
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_init",
		.flags = 0,
	},
	{	/* (last) (unknown board) */
		.name = "unknown",
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_init",
		.flags = 0,
	},
};

/* ---- Stage 'f': RAM not valid, variables can *not* be used yet ---------- */

static iomux_cfg_t const lpuart4_pads[] = {
	MX7ULP_PAD_PTC3__LPUART4_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX7ULP_PAD_PTC2__LPUART4_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

static void setup_iomux_uart(void)
{
	mx7ulp_iomux_setup_multiple_pads(lpuart4_pads,
					 ARRAY_SIZE(lpuart4_pads));
}

/* Check how to add more dynamically, SPL? */
void fs_board_init_nboot_args(void)
{
	struct fs_nboot_args *pargs = (struct fs_nboot_args*)
				(CONFIG_SYS_SDRAM_BASE + 0x00001000);

	/* initalize ram area with zero before set */
	memset(pargs, 0x0, sizeof(struct fs_nboot_args));

    	nbootargs.dwID = FSHWCONFIG_ARGS_ID;
	nbootargs.dwSize = 16*4;
	nbootargs.dwNBOOT_VER = 1;

	nbootargs.dwMemSize = PHYS_SDRAM_SIZE >> 20;

	nbootargs.dwNumDram = CONFIG_NR_DRAM_BANKS;
	nbootargs.dwFlashSize = 0x0;		/* size of NAND flash in MB */
	nbootargs.dwDbgSerPortPA = LPUART_BASE;

	/* get board type */
        nbootargs.chBoardType = 0x0;
	/* get board revision */
        nbootargs.chBoardRev = 120;

	/* fill nboot args first after ram initialization */
	pargs = fs_board_get_nboot_args();

	pargs->dwID = nbootargs.dwID;
	pargs->dwSize = nbootargs.dwSize;
	pargs->dwNBOOT_VER = nbootargs.dwNBOOT_VER;
	pargs->dwMemSize = nbootargs.dwMemSize;
	pargs->dwNumDram = nbootargs.dwNumDram;
	pargs->dwFlashSize = nbootargs.dwFlashSize;
	pargs->dwDbgSerPortPA = nbootargs.dwDbgSerPortPA;
	pargs->chBoardRev = nbootargs.chBoardRev;
	pargs->chBoardType = nbootargs.chBoardType;
	pargs->chFeatures2 = 0x3F;
}

/* Do some very early board specific setup */
int board_early_init_f(void)
{
	/* fs_board_init_nboot_args */
	fs_board_init_nboot_args();

	setup_iomux_uart();

	return 0;
}

/* Return the appropriate environment depending on the fused boot device */
enum env_location env_get_location(enum env_operation op, int prio)
{
	if (prio == 0) {
		switch (fs_board_get_boot_dev()) {
		case SD1_BOOT:
			return ENVL_MMC;
		case MMC1_BOOT:
			return ENVL_MMC;
		default:
			break;
		}
	}

	return ENVL_UNKNOWN;
}

/* Check board type */
int checkboard(void)
{
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();
	unsigned int features2;

	features2 = pargs->chFeatures2;

	printf ("Board: %s Rev %u.%02u (", board_info[board_type].name,
		board_rev / 100, board_rev % 100);
	if (features2 & FEAT2_WLAN)
		puts ("WLAN, ");
	if (features2 & FEAT2_EMMC)
		puts ("eMMC, ");
	if (features2 & FEAT2_QSPI)
		puts("QSPI, ");
	printf("%dx DRAM)\n", pargs->dwNumDram);

	//fs_board_show_nboot_args(pargs);

	return 0;
}

/* ---- Stage 'r': RAM valid, U-Boot relocated, variables can be used ------ */
#ifdef CONFIG_FSL_QSPI
int board_qspi_init(void);
#endif

int board_init(void)
{
	unsigned int board_type = fs_board_get_type();
  	/* Copy NBoot args to variables and prepare command prompt string */
  	fs_board_init_common(&board_info[board_type]);
	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

#ifdef CONFIG_FSL_QSPI
	board_qspi_init();
#endif

	return 0;
}

#ifndef CONFIG_DM_USB
/*
 * USB Host support.
 *
 * USB0 is OTG. By default this is used as device port. However on some F&S
 * boards this port may optionally be configured as a host port. So if
 * environment variable usb0mode is set to "host" on these boards, or if it is
 * set to "otg" and the ID pin is low when usb is started, use host mode.
 *
 *    Board               USB_OTG_PWR                 USB_OTG_ID
 *    ------------------------------------------------------------------------
 *    PicoCoreMX7ULP      PTE15_PTE15 (GPIO2_IO15)(*) PTC13_PTC13 (GPIO0_IO13)
 *
 * (*) Signal on SKIT is active low, usually USB_OTG_PWR is active high
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
#define USB_PWR_GPIO	IMX_GPIO_NR(5, 15)
#define USB_OTG_ID_GPIO	IMX_GPIO_NR(3, 13)

static iomux_cfg_t const usb_otg1_pads[] = {
	/* gpio for otg id */
	MX7ULP_PAD_PTC13__PTC13 | MUX_PAD_CTRL(OTG_ID_GPIO_PAD_CTRL),
	/* gpio for pwr */
	MX7ULP_PAD_PTE15__PTE15 | MUX_PAD_CTRL(PWR_GPIO_PAD_CTRL),
};

static void setup_usb(void)
{
	mx7ulp_iomux_setup_multiple_pads(usb_otg1_pads,
		ARRAY_SIZE(usb_otg1_pads));
	gpio_request(USB_OTG_ID_GPIO, "otg_id");
	gpio_direction_input(USB_OTG_ID_GPIO);

	gpio_request(USB_PWR_GPIO, "otg_pwr");
	gpio_direction_output(USB_PWR_GPIO, 1);
}

int board_ehci_power(int port, int on)
{
	int pwr_pol = 1;	/* 0 = active high, 1 = active low */

	if (pwr_pol)
		on = !on;

	switch (port) {
	case 0:
		gpio_set_value(USB_PWR_GPIO, on);
		break;
	default:
		printf("MXC USB port %d not yet supported\n", port);
		return -EINVAL;
	}

	return 0;
}

int board_usb_phy_mode(int port)
{
	int ret = -1;

	if (port == 0) {
		ret = gpio_get_value(USB_OTG_ID_GPIO);

		if (ret)
			return USB_INIT_DEVICE;
		else
			return USB_INIT_HOST;
	}
	return USB_INIT_HOST;
}
#endif

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

#if 0
	env_set("tee", "no");
#ifdef CONFIG_IMX_OPTEE
	env_set("tee", "yes");
#endif
#endif

	/* Set up all board specific variables */
	fs_board_late_init_common("ttyLP");

	return 0;
}
#endif

#ifdef CONFIG_FSL_QSPI
#ifndef CONFIG_DM_SPI
static iomux_cfg_t const quadspi_pads[] = {
	MX7ULP_PAD_PTB8__QSPIA_SS0_B | MUX_PAD_CTRL(QSPI_PAD_CTRL1),
	MX7ULP_PAD_PTB15__QSPIA_SCLK  | MUX_PAD_CTRL(QSPI_PAD_CTRL1),
	MX7ULP_PAD_PTB16__QSPIA_DATA3 | MUX_PAD_CTRL(QSPI_PAD_CTRL1),
	MX7ULP_PAD_PTB17__QSPIA_DATA2 | MUX_PAD_CTRL(QSPI_PAD_CTRL1),
	MX7ULP_PAD_PTB18__QSPIA_DATA1 | MUX_PAD_CTRL(QSPI_PAD_CTRL1),
	MX7ULP_PAD_PTB19__QSPIA_DATA0 | MUX_PAD_CTRL(QSPI_PAD_CTRL1),
};
#endif

int board_qspi_init(void)
{
	u32 val;
#ifndef CONFIG_DM_SPI
	mx7ulp_iomux_setup_multiple_pads(quadspi_pads,
					ARRAY_SIZE(quadspi_pads));
#endif

	/* enable clock */
	val = readl(PCC1_RBASE + 0x94);

	if (!(val & 0x20000000)) {
		writel(0x03000003, (PCC1_RBASE + 0x94));
		writel(0x43000003, (PCC1_RBASE + 0x94));
	}

	/* Enable QSPI as a wakeup source on B0 */
	if (soc_rev() >= CHIP_REV_2_0)
		setbits_le32(SIM0_RBASE + WKPU_WAKEUP_EN, WKPU_QSPI_CHANNEL);
	return 0;
}
#endif

#ifndef CONFIG_DM_ETH
/* Set the ethaddr environment variable according to index */
void fs_eth_set_ethaddr(int index)
{
	uchar enetaddr[6];
	int i;
	int offs = index;

	/* Try to fulfil the request in the following order:
	 *   1. From environment variable
	 *   2. CONFIG_ETHADDR_BASE
	 */
	if (eth_env_get_enetaddr_by_index("eth", index, enetaddr))
		return;

	eth_parse_enetaddr(CONFIG_ETHADDR_BASE, enetaddr);

	i = 6;
	do {
		offs += (int)enetaddr[--i];
		enetaddr[i] = offs & 0xFF;
		offs >>= 8;
	} while (i);

	eth_env_set_enetaddr_by_index("eth", index, enetaddr);
}

int board_eth_init(bd_t *bis)
{
	int rc = 0;

	/* set mac address, necessary for usb_ether */
	fs_eth_set_ethaddr(0);

#ifdef CONFIG_USB_ETH_RNDIS
	rc = usb_eth_initialize(bis);
#endif
	return rc;
}
#endif

#define FDT_CMA         "/reserved-memory/linux,cma"
/* Do any additional board-specific modifications on Linux device tree */
int ft_board_setup(void *fdt, bd_t *bd)
{
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();
	int offs;

	/* Set bdinfo entries */
	offs = fs_fdt_path_offset(fdt, "/bdinfo");
	if (offs >= 0) {
		/* Set common bdinfo entries */
		fs_fdt_set_bdinfo(fdt, offs);

		/* MAC addresses */
		/* All fsimx7ulp boards have a WLAN module
		 * which have an integrated mac address. So we don´t
		 * have to set an own mac address for the module.
		 */
//		if (features & FEAT2_WLAN)
//			fs_fdt_set_macaddr(fdt, offs, id++);
	}

	/* Set linux,cma size depending on RAM size. Default is 128MB. */
	offs = fs_fdt_path_offset(fdt, FDT_CMA);
	if (fdt_get_property(fdt, offs, "no-uboot-override", NULL) == NULL) {
		unsigned int dram_size = pargs->dwMemSize;
		if ((dram_size == 1023) || (dram_size == 1024)) {
			fdt32_t tmp;
			tmp = cpu_to_fdt32(0x08000000);
			fs_fdt_set_val(fdt, offs, "size", &tmp, sizeof(tmp), 1);
		}
	}

	return 0;
}

int mmc_map_to_kernel_blk(int devno)
{
	return devno;
}
