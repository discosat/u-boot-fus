/*
 * fsimx8mx.c
 *
 * (C) Copyright 2020
 * Patrik Jakob, F&S Elektronik Systeme GmbH, jakob@fs-net.de
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

#define BT_DUMMY			0
#define BT_PICOCOREMX8MM 	1
#define BT_PICOCOREMX8MN	2

/* Features set in fs_nboot_args.chFeature2 (available since NBoot VN27) */
#define FEAT2_DDR3L_X2 		(1<<0)	/* 0: DDR3L x1, 1; DDR3L x2 */
#define FEAT2_NAND_EMMC		(1<<1)	/* 0: NAND, 1: has eMMC */
#define FEAT2_CAN			(1<<2)	/* 0: no CAN, 1: has CAN */
#define FEAT2_SEC_CHIP		(1<<3)	/* 0: no Security Chip, 1: has Security Chip */
#define FEAT2_AUDIO 		(1<<4)	/* 0: no Audio, 1: Audio */
#define FEAT2_EXT_RTC   	(1<<5)	/* 0: internal RTC, 1: external RTC */
#define FEAT2_LVDS   		(1<<6)	/* 0: MIPI DSI, 1: LVDS */
#define FEAT2_ETH   		(1<<7)	/* 0: no LAN, 1; has LAN */

/* Device tree paths */
#define FDT_ETH	"/soc/ethernet@30be0000"

#define UART_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL1)
#define WDOG_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_ODE | PAD_CTL_PUE | PAD_CTL_PE)
#define ENET_PAD_CTRL ( \
		PAD_CTL_PUE |	\
		PAD_CTL_DSE6   | PAD_CTL_HYS)

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

#ifdef CONFIG_ENV_IS_IN_MMC
#define ROOTFS ".rootfs_mmc"
#define KERNEL ".kernel_mmc"
#define FDT ".fdt_mmc"
#elif CONFIG_ENV_IS_IN_NAND
#define ROOTFS ".rootfs_ubifs"
#define KERNEL ".kernel_nand"
#define FDT ".fdt_nand"
#else /* Default = Nand */
#define ROOTFS ".rootfs_ubifs"
#define KERNEL ".kernel_nand"
#define FDT ".fdt_nand"
#endif

const struct fs_board_info board_info[3] = {
	{	/* 0 (unknown) */
		.name = "unknown",
	},
	{	/* 1 (BT_PICOCOREMX8MM) */
		.name = "PicoCoreMX8MM",
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
	{	/* 2 (BT_PICOCOREMX8MN) */
		.name = "PicoCoreMX8MN",
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

#ifdef CONFIG_NAND_MXS
static void setup_gpmi_nand(void);
#endif

static iomux_v3_cfg_t const uart_pads[] = {
	IMX8MM_PAD_SAI2_RXC_UART1_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	IMX8MM_PAD_SAI2_RXFS_UART1_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

static iomux_v3_cfg_t const wdog_pads[] = {
	IMX8MM_PAD_GPIO1_IO02_WDOG1_WDOG_B  | MUX_PAD_CTRL(WDOG_PAD_CTRL),
};

/* Do some very early board specific setup */
int board_early_init_f(void)
{
	struct wdog_regs *wdog = (struct wdog_regs*) WDOG1_BASE_ADDR;

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));

	set_wdog_reset(wdog);

	/* Setup UART pads */
	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));

#ifdef CONFIG_NAND_MXS
	setup_gpmi_nand(); /* SPL will call the board_early_init_f */
#endif

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

	printf ("Board: %s Rev %u.%02u (", board_info[board_type].name,
		board_rev / 100, board_rev % 100);
	if (features2 & FEAT2_ETH)
		puts ("LAN, ");
	if (features2 & FEAT2_NAND_EMMC)
		puts ("eMMC, ");
	else
		puts("NAND, ");
	printf ("%dx DRAM)\n", pargs->dwNumDram);

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

/* nand flash pads  */
#ifdef CONFIG_NAND_MXS
#define NAND_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL2 | PAD_CTL_HYS)
#define NAND_PAD_READY0_CTRL (PAD_CTL_DSE6 | PAD_CTL_FSEL2 | PAD_CTL_PUE)
static iomux_v3_cfg_t const gpmi_pads[] = {
	IMX8MM_PAD_NAND_ALE_RAWNAND_ALE | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_CE0_B_RAWNAND_CE0_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_CLE_RAWNAND_CLE | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA00_RAWNAND_DATA00 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA01_RAWNAND_DATA01 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA02_RAWNAND_DATA02 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA03_RAWNAND_DATA03 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA04_RAWNAND_DATA04 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA05_RAWNAND_DATA05	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA06_RAWNAND_DATA06	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA07_RAWNAND_DATA07	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_RE_B_RAWNAND_RE_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_READY_B_RAWNAND_READY_B | MUX_PAD_CTRL(NAND_PAD_READY0_CTRL),
	IMX8MM_PAD_NAND_WE_B_RAWNAND_WE_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_WP_B_RAWNAND_WP_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
};

static void setup_gpmi_nand(void)
{
	imx_iomux_v3_setup_multiple_pads(gpmi_pads, ARRAY_SIZE(gpmi_pads));
}
#endif /* CONFIG_NAND_MXS */

#ifdef CONFIG_VIDEO_MXS
/*
 * Possible display configurations
 *
 *   Board          MIPI      LVDS0      LVDS1
 *   -------------------------------------------------------------
 *   PicoCoreMX8MM  4 lanes*  24 bit²    24 bit²
 *   PicoCoreMX8MN  4 lanes*  24 bit²    24 bit²
 *
 * The entry marked with * is the default port.
 * The entry marked with ² only work with a MIPI to LVDS converter
 *
 * Display initialization sequence:
 *
 *  1. board_r.c: board_init_r() calls stdio_add_devices().
 *  2. stdio.c: stdio_add_devices() calls drv_video_init().
 *  3. cfb_console.c: drv_video_init() calls board_video_skip(); if this
 *     returns non-zero, the display will not be started.
 *  4. video.c: board_video_skip(): Parse env variable "panel" if available
 *     and search struct display_info_t of board specific file. If env
 *     variable "panel" is not available parse struct display_info_t and call
 *     detect function, if successful use this display or try to detect next
 *     display. If no detect function is available use first display of struct
 *     display_info_t.
 *  5. fsimx8mx.c: board_video_skip parse display parameter of display_info_t,
 *     detect and enable function.
 *  6. cfb_console.c: drv_video_init() calls cfb_video_init().
 *  7. cfb_console.c: video_init() calls video_hw_init().
 *  8. video_common.c: video_hw_init() calls imx8m_display_init().
 *  9. video_common.c: imx8m_display_init() initialize registers of dccs.
 * 10. cfb_console.c: calls video_logo().
 * 11. cfb_console.c: video_logo() draws either the console logo and the welcome
 *     message, or if environment variable splashimage is set, the splash
 *     screen.
 * 12. cfb_console.c: drv_video_init() registers the console as stdio device.
 * 13. board_r.c: board_init_r() calls board_late_init().
 * 14. fsimx8mx.c: board_late_init() calls fs_board_set_backlight_all() to
 *     enable all active displays.
 */


#define TC358764_ADDR 0xF

static int tc358764_i2c_reg_write(struct udevice *dev, uint addr, uint8_t *data, int length)
{
	int err;

	err = dm_i2c_write (dev, addr, data, length);
	return err;
}

static int tc358764_i2c_reg_read(struct udevice *dev, uint addr, uint8_t *data, int length)
{
	int err;

	err = dm_i2c_read (dev, addr, data, length);
	if (err)
		{
		return err;
		}
	return 0;
}

/* System registers */
#define SYS_RST			0x0504 /* System Reset */
#define SYS_ID			0x0580 /* System ID */

static int tc358764_init(void)
{
	struct udevice *bus = 0, *mipi2lvds_dev = 0;
	int i2c_bus = 0;
	int ret;
	uint8_t val[4] =
		{ 0 };
	uint *uptr = (uint*) val;

	ret = uclass_get_device_by_seq (UCLASS_I2C, i2c_bus, &bus);
	if (ret)
		{
		printf ("%s: No bus %d\n", __func__, i2c_bus);
		return 1;
		}

	ret = dm_i2c_probe (bus, TC358764_ADDR, 0, &mipi2lvds_dev);
	if (ret)
		{
		printf ("%s: Can't find device id=0x%x, on bus %d, ret %d\n", __func__,
			TC358764_ADDR, i2c_bus, ret);
		return 1;
		}

	/* offset */
	i2c_set_chip_offset_len (mipi2lvds_dev, 2);

	/* read chip/rev register with */
	tc358764_i2c_reg_read (mipi2lvds_dev, SYS_ID, val, sizeof(val));

	if (val[1] == 0x65)
		printf ("DSI2LVDS:  TC358764 Rev. 0x%x.\n", (uint8_t) (val[0] & 0xFF));
	else
		printf ("DSI2LVDS:  ID: 0x%x Rev. 0x%x.\n", (uint8_t) (val[1] & 0xFF),
			(uint8_t) (val[0] & 0xFF));

	/* DSI Basic parameters. Have to be in LP mode...*/
	#define PPI_TX_RX_TA 0x13C
	*uptr = 0x00010002; // BTA paramters
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_TX_RX_TA, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write PPI_TX_TA...\n", __func__);
		return 1;
		}

	#define PPI_LPTXTIMCNT 0x114
	*uptr = 0x00000001; // BTA paramters
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_LPTXTIMCNT, val,
					sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write PPI_LPTXTIMCNT...\n", __func__);
		return 1;
		}

	#define PPI_D0S_CLRSIPOCOUNT 0x164
	*uptr = 0x00000000; // BTA paramters
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_D0S_CLRSIPOCOUNT, val,
					sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write PPI_D0S_CLRSIPOCOUNT...\n", __func__);
		return 1;
		}
	#define PPI_D1S_CLRSIPOCOUNT 0x168
	*uptr = 0x00000000; // BTA paramters
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_D1S_CLRSIPOCOUNT, val,
					sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write PPI_D1S_CLRSIPOCOUNT...\n", __func__);
		return 1;
		}
	#define PPI_D2S_CLRSIPOCOUNT 0x16C
	*uptr = 0x00000000; // BTA paramters
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_D2S_CLRSIPOCOUNT, val,
					sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write PPI_D2S_CLRSIPOCOUNT...\n", __func__);
		return 1;
		}
	#define PPI_D3S_CLRSIPOCOUNT 0x170
	*uptr = 0x00000000; //4; // BTA paramters
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_D3S_CLRSIPOCOUNT, val,
					sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write PPI_D3S_CLRSIPOCOUNT...\n", __func__);
		return 1;
		}
	#define PPI_LANEENABLE 0x134
	*uptr = 0x0000001F; // BTA paramters
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_LANEENABLE, val,
					sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write PPI_LANEENABLE...\n", __func__);
		return 1;
		}
	#define DSI_LANEENABLE 0x210
	*uptr = 0x0000001F; // BTA paramters
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, DSI_LANEENABLE, val,
					sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write DSI_LANEENABLE...\n", __func__);
		return 1;
		}
	#define PPI_SARTPPI 0x104
	*uptr = 0x00000001; // BTA paramters
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_SARTPPI, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write PPI_SARTPPI...\n", __func__);
		return 1;
		}

	#define DSI_SARTPPI 0x204
	*uptr = 0x00000001; // BTA paramters
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, DSI_SARTPPI, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write DSI_SARTPPI...\n", __func__);
		return 1;
		}

	/* Timing and mode setting */
	#define VPCTRL 0x450
	*uptr = 0x03F00120; // BTA paramters
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, VPCTRL, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write VPCTRL...\n", __func__);
		return 1;
		}

	#define HTIM1 0x454
	*uptr = 0x002E0005;
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, HTIM1, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write HTIM1...\n", __func__);
		return 1;
		}

	#define HTIM2 0x458
	*uptr = 0x00D20320;
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, HTIM2, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write VPCTRL...\n", __func__);
		return 1;
		}

	#define VTIM1 0x45C
	*uptr = 0x0017000A;
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, VTIM1, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write VTIM1...\n", __func__);
		return 1;
		}
	#define VTIM2 0x460
	*uptr = 0x001601E0;
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, VTIM2, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write VTIM2...\n", __func__);
		return 1;
		}
	#define VFUEN 0x464
	*uptr = 0x00000001;
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, VFUEN, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write VFUEN...\n", __func__);
		return 1;
		}
	#define LVPHY0 0x4A0
	*uptr = 0x0044802D;
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVPHY0, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write LVPHY0...\n", __func__);
		return 1;
		}
	udelay (100);

	*uptr = 0x0004802D;
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVPHY0, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write LVPHY0...\n", __func__);
		return 1;
		}
	#define SYSRST 0x504
	*uptr = 0x00000004;
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, SYSRST, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write SYSRST...\n", __func__);
		return 1;
		}

	#define LVMX0003 0x0480
	*uptr = 0x03020100;
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVMX0003, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write LVMX0003...\n", __func__);
		return 1;
		}

	#define LVMX0407 0x0484
	*uptr = 0x08050704;
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVMX0407, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write LVMX0407...\n", __func__);
		return 1;
		}

	#define LVMX0811 0x0488
	*uptr = 0x0F0E0A09;
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVMX0811, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write LVMX0811...\n", __func__);
		return 1;
		}

	#define LVMX1215 0x048C
	*uptr = 0x100D0C0B;
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVMX1215, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write LVMX1215...\n", __func__);
		return 1;
		}

	#define LVMX1619 0x0490
	*uptr = 0x12111716;
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVMX1619, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write LVMX1619...\n", __func__);
		return 1;
		}

	#define LVMX2023 0x0494
	*uptr = 0x1B151413;
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVMX2023, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write LVMX2023...\n", __func__);
		return 1;
		}

	#define LVMX2427 0x0498
	*uptr = 0x061A1918;

	ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVMX2427, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write LVMX2427...\n", __func__);
		return 1;
		}

	/* LVDS enable */
	#define LVCFG 0x49C
	*uptr = 0x00000031;
	ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVCFG, val, sizeof(val));
	if (ret)
		{
		printf ("%s: Can't write LVCFG...\n", __func__);
		return 1;
		}

	return 0;
}

#ifdef CONFIG_IMX_SEC_MIPI_DSI
static const struct sec_mipi_dsim_plat_data imx8mm_mipi_dsim_plat_data = {
	.version	= 0x1060200,
	.max_data_lanes = 4,
	.max_data_rate  = 1500000000ULL,
	.reg_base = MIPI_DSI_BASE_ADDR,
	.gpr_base = CSI_BASE_ADDR + 0x8000,
	.dphy_pll	= &pll_1432x,
	.dphy_timing	= dphy_timing_ln14lpp_v1p2,
	.num_dphy_timing = ARRAY_SIZE(dphy_timing_ln14lpp_v1p2),
	.dphy_timing_cmp = dphy_timing_default_cmp,
};

#define DISPLAY_MIX_SFT_RSTN_CSR		0x00
#define DISPLAY_MIX_CLK_EN_CSR		0x04

   /* 'DISP_MIX_SFT_RSTN_CSR' bit fields */
#define BUS_RSTN_BLK_SYNC_SFT_EN	BIT(6)

   /* 'DISP_MIX_CLK_EN_CSR' bit fields */
#define LCDIF_PIXEL_CLK_SFT_EN		BIT(7)
#define LCDIF_APB_CLK_SFT_EN		BIT(6)

void disp_mix_bus_rstn_reset(ulong gpr_base, bool reset)
{
	if (!reset)
		/* release reset */
		setbits_le32 (gpr_base + DISPLAY_MIX_SFT_RSTN_CSR,
			BUS_RSTN_BLK_SYNC_SFT_EN);
	else
		/* hold reset */
		clrbits_le32 (gpr_base + DISPLAY_MIX_SFT_RSTN_CSR,
			BUS_RSTN_BLK_SYNC_SFT_EN);
}

void disp_mix_lcdif_clks_enable(ulong gpr_base, bool enable)
{
	if (enable)
		/* enable lcdif clks */
		setbits_le32 (gpr_base + DISPLAY_MIX_CLK_EN_CSR,
			LCDIF_PIXEL_CLK_SFT_EN | LCDIF_APB_CLK_SFT_EN);
	else
		/* disable lcdif clks */
		clrbits_le32 (gpr_base + DISPLAY_MIX_CLK_EN_CSR,
			LCDIF_PIXEL_CLK_SFT_EN | LCDIF_APB_CLK_SFT_EN);
}

struct mipi_dsi_client_dev tc358764_dev = {
	.channel	= 0,
	.lanes = 4,
	.format  = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_VIDEO_AUTO_VERT,
	.name = "TC358764",
};

struct mipi_dsi_client_dev g050tan01_dev = {
	.channel	= 0,
	.lanes = 4,
	.format  = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO
                | MIPI_DSI_MODE_EOT_PACKET | MIPI_DSI_MODE_VIDEO_SYNC_PULSE,
};

#define FSL_SIP_GPC			0xC2000000
#define FSL_SIP_CONFIG_GPC_PM_DOMAIN	0x3
#define DISPMIX				9
#define MIPI				10


#define BL_ON_PAD IMX_GPIO_NR(5, 3)
static iomux_v3_cfg_t const bl_on_pads[] = {
	IMX8MM_PAD_SPDIF_TX_GPIO5_IO3 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

#define VLCD_ON_PAD IMX_GPIO_NR(5, 1)
static iomux_v3_cfg_t const vlcd_on_pads[] = {
	IMX8MM_PAD_SAI3_TXD_GPIO5_IO1 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

#define LVDS_RST_PAD IMX_GPIO_NR(1, 13)
#define LVDS_STBY_PAD IMX_GPIO_NR(1, 4)
static iomux_v3_cfg_t const lvds_rst_pads[] = {
	IMX8MM_PAD_GPIO1_IO08_GPIO1_IO8 | MUX_PAD_CTRL(NO_PAD_CTRL),
	IMX8MM_PAD_GPIO1_IO04_GPIO1_IO4 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

void enable_tc358764(struct display_info_t const *dev)
{
	int ret = 0;

	mxs_set_lcdclk(dev->bus, PICOS2KHZ(dev->mode.pixclock));

	clock_set_target_val (IPP_DO_CLKO2, CLK_ROOT_ON
		| CLK_ROOT_SOURCE_SEL(1) | CLK_ROOT_POST_DIV(CLK_ROOT_POST_DIV6));

	imx_iomux_v3_setup_multiple_pads (lvds_rst_pads, ARRAY_SIZE (lvds_rst_pads));
	gpio_request (LVDS_STBY_PAD, "LVDS_STBY");
	gpio_direction_output (LVDS_STBY_PAD, 1);
	udelay (50);
	gpio_request (LVDS_RST_PAD, "LVDS_RST");
	gpio_direction_output (LVDS_RST_PAD, 0);
	/* period of reset signal > 50 ns */
	udelay (5);
	gpio_direction_output (LVDS_RST_PAD, 1);
	udelay (500);

	/* enable the dispmix & mipi phy power domain */
	call_imx_sip (FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, DISPMIX, true, 0);
	call_imx_sip (FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, MIPI, true, 0);

	/* Put lcdif out of reset */
	disp_mix_bus_rstn_reset (imx8mm_mipi_dsim_plat_data.gpr_base, false);
	disp_mix_lcdif_clks_enable (imx8mm_mipi_dsim_plat_data.gpr_base, true);

	/* Setup mipi dsim */
	ret = sec_mipi_dsim_setup (&imx8mm_mipi_dsim_plat_data);

	if (ret)
		return;

	ret = imx_mipi_dsi_bridge_attach (&tc358764_dev); /* attach tc358764 device */
}

int detect_tc358764(struct display_info_t const *dev)
{
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();
	unsigned int features2;

	features2 = pargs->chFeatures2;

	/* if LVDS controller is equipped  */
	if(features2 & FEAT2_LVDS) {
		return 1;
	}

	return 0;
}

int detect_mipi_disp(struct display_info_t const *dev)
{
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();
	unsigned int features2;

	features2 = pargs->chFeatures2;

	/* if LVDS controller is equipped  */
	if(features2 & FEAT2_LVDS) {
		return 0;
	}

	return 1;
}

void enable_mipi_disp(struct display_info_t const *dev)
{
	/* enable the dispmix & mipi phy power domain */
	call_imx_sip (FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, DISPMIX, true, 0);
	call_imx_sip (FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, MIPI, true, 0);

	/* Put lcdif out of reset */
	disp_mix_bus_rstn_reset (imx8mm_mipi_dsim_plat_data.gpr_base, false);
	disp_mix_lcdif_clks_enable (imx8mm_mipi_dsim_plat_data.gpr_base, true);

	/* Setup mipi dsim */
	sec_mipi_dsim_setup (&imx8mm_mipi_dsim_plat_data);

	nt35521_init ();
	g050tan01_dev.name = displays[0].mode.name;
	imx_mipi_dsi_bridge_attach(&g050tan01_dev); /* attach g050tan01 device */
}

void board_quiesce_devices(void)
{
	gpio_request (IMX_GPIO_NR(1, 13), "DSI EN");
	gpio_direction_output (IMX_GPIO_NR(1, 13), 0);
}

#endif // end of mipi

struct display_info_t const displays[] = {
{
	.bus = LCDIF_BASE_ADDR,
	.addr = 0,
	.pixfmt = 24,
	.detect = detect_mipi_disp,
	.enable	= enable_mipi_disp,
	.mode	= {
		.name			= "NT35521_OLED",
		.refresh		= 60,
		.xres			= 720,
		.yres			= 1280,
		.pixclock		= 12830, // 10^12/freq
		.left_margin	= 72,
		.right_margin	= 56,
		.hsync_len		= 128,
		.upper_margin	= 38,
		.lower_margin	= 3,
		.vsync_len		= 10,
		.sync			= FB_SYNC_EXT,
		.vmode			= FB_VMODE_NONINTERLACED
	}
},
{
	.bus = LCDIF_BASE_ADDR,
	.addr = 0,
	.pixfmt = 24,
	.detect = detect_tc358764,
	.enable	= enable_tc358764,
	.mode	= {
		.name			= "TC358764",
		.refresh		= 60,
		.xres			= 800,
		.yres			= 480,
		.pixclock		= 29850, // 10^12/freq
		.left_margin	= 20,
		.right_margin	= 247,
		.hsync_len	= 5,
		.upper_margin	= 33,
		.lower_margin	= 20,
		.vsync_len		= 2,
		.sync			= FB_SYNC_EXT,
		.vmode			= FB_VMODE_NONINTERLACED
	}
},
};
size_t display_count = ARRAY_SIZE(displays);
#endif /* CONFIG_VIDEO_MXS */


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
 *    PicoCoreMX8MM       GPIO1_12 (GPIO1_IO12)(*) -
 *    PicoCoreMX8MN       GPIO1_12 (GPIO1_IO12)(*) -
 *
 * (*) Signal on SKIT is active low, usually USB_OTG_PWR is active high
 *
 * USB1 is a host-only port (USB_H1). It is used on all boards. Some boards
 * may have an additional USB hub with a reset signal connected to this port.
 *
 *    Board               USB_H1_PWR               Hub Reset
 *    -------------------------------------------------------------------------
 *    PicoCoreMX8MM       GPIO1_14 (GPIO1_IO14)(*) -
 *    PicoCoreMX8MN       GPIO1_14 (GPIO1_IO14)(*) -
 *
 * (*) Signal on SKIT is active low, usually USB_HOST_PWR is active high
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

	imx8m_usb_power (index, true);
	return 0;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	debug("board_usb_cleanup %d, type %d\n", index, init);

	imx8m_usb_power (index, false);
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
	/* TODO: Set here because otherwise platform would be generated from
         * name.
         */
	env_set("platform", "picocoremx8mx");
	/* Set up all board specific variables */
	fs_board_late_init_common("ttymxc");

	#ifdef CONFIG_VIDEO_MXS
	imx_iomux_v3_setup_multiple_pads (vlcd_on_pads, ARRAY_SIZE (bl_on_pads));
	/* backlight off */
	gpio_request (BL_ON_PAD, "BL_ON");
	gpio_direction_output (BL_ON_PAD, 0);

	imx_iomux_v3_setup_multiple_pads (vlcd_on_pads, ARRAY_SIZE (vlcd_on_pads));

	if(detect_tc358764(0))
	{
		/* initialize TC358764 over I2C */
		if(tc358764_init())
			/* error case... */
			return 0;
	}

	/* set vlcd on*/
	gpio_request (VLCD_ON_PAD, "VLCD_ON");
	gpio_direction_output (VLCD_ON_PAD, 1);
	/* backlight on */
	gpio_direction_output (BL_ON_PAD, 1);
	#endif
	return 0;
}
#endif /* CONFIG_BOARD_LATE_INIT */

#ifdef CONFIG_FEC_MXC

#if 0
#define ENET_PAD_CTRL ( \
		PAD_CTL_PUE |	\
		PAD_CTL_DSE6   | PAD_CTL_HYS)

				MX8MM_IOMUXC_ENET_MDC_ENET1_MDC			0x00003
				MX8MM_IOMUXC_ENET_MDIO_ENET1_MDIO		0x00023
				MX8MM_IOMUXC_ENET_TD3_ENET1_RGMII_TD3		0x0001f
				MX8MM_IOMUXC_ENET_TD2_ENET1_RGMII_TD2		0x0001f
				MX8MM_IOMUXC_ENET_TD1_ENET1_RGMII_TD1		0x0001f
				MX8MM_IOMUXC_ENET_TD0_ENET1_RGMII_TD0		0x0001f
				MX8MM_IOMUXC_ENET_RD3_ENET1_RGMII_RD3		0x00091
				MX8MM_IOMUXC_ENET_RD2_ENET1_RGMII_RD2		0x00091
				MX8MM_IOMUXC_ENET_RD1_ENET1_RGMII_RD1		0x00091
				MX8MM_IOMUXC_ENET_RD0_ENET1_RGMII_RD0		0x00091
				MX8MM_IOMUXC_ENET_TXC_ENET1_RGMII_TXC		0x0001f
				MX8MM_IOMUXC_ENET_RXC_ENET1_RGMII_RXC		0x00091
				MX8MM_IOMUXC_ENET_RX_CTL_ENET1_RGMII_RX_CTL	0x00091
			        MX8MM_IOMUXC_ENET_TX_CTL_ENET1_RGMII_TX_CTL	0x0001f
                                /* ETH_SW_RST  */
                		MX8MM_IOMUXC_GPIO1_IO05_GPIO1_IO5		0x00019
                                /* ETH_SW_INTn  */
			        MX8MM_IOMUXC_GPIO1_IO11_GPIO1_IO11		0x00064
#endif

/* enet pads definition */
static iomux_v3_cfg_t const enet_pads_rgmii[] = {
	IMX8MM_PAD_ENET_MDIO_ENET1_MDIO | MUX_PAD_CTRL(PAD_CTL_DSE4  | PAD_CTL_ODE),
	IMX8MM_PAD_ENET_MDC_ENET1_MDC | MUX_PAD_CTRL(PAD_CTL_DSE4 | PAD_CTL_ODE),
	IMX8MM_PAD_ENET_TXC_ENET1_RGMII_TXC | MUX_PAD_CTRL(ENET_PAD_CTRL),
	IMX8MM_PAD_ENET_TX_CTL_ENET1_RGMII_TX_CTL | MUX_PAD_CTRL(ENET_PAD_CTRL),
	IMX8MM_PAD_ENET_TD0_ENET1_RGMII_TD0 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	IMX8MM_PAD_ENET_TD1_ENET1_RGMII_TD1 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	IMX8MM_PAD_ENET_TD2_ENET1_RGMII_TD2 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	IMX8MM_PAD_ENET_TD3_ENET1_RGMII_TD3 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	IMX8MM_PAD_ENET_RXC_ENET1_RGMII_RXC | MUX_PAD_CTRL(ENET_PAD_CTRL),
	IMX8MM_PAD_ENET_RX_CTL_ENET1_RGMII_RX_CTL | MUX_PAD_CTRL(ENET_PAD_CTRL),
	IMX8MM_PAD_ENET_RD0_ENET1_RGMII_RD0 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	IMX8MM_PAD_ENET_RD1_ENET1_RGMII_RD1 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	IMX8MM_PAD_ENET_RD2_ENET1_RGMII_RD2 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	IMX8MM_PAD_ENET_RD3_ENET1_RGMII_RD3 | MUX_PAD_CTRL(ENET_PAD_CTRL),

	/* Phy Interrupt */
	IMX8MM_PAD_GPIO1_IO04_GPIO1_IO4 | MUX_PAD_CTRL(PAD_CTL_DSE4 | PAD_CTL_PUE),
};

#define FEC_RST_PAD IMX_GPIO_NR(1, 5)
static iomux_v3_cfg_t const fec1_rst_pads[] = {
	IMX8MM_PAD_GPIO1_IO05_GPIO1_IO5 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void setup_iomux_fec(void)
{
	imx_iomux_v3_setup_multiple_pads (enet_pads_rgmii,
						ARRAY_SIZE (enet_pads_rgmii));

	imx_iomux_v3_setup_multiple_pads (fec1_rst_pads, ARRAY_SIZE (fec1_rst_pads));

	gpio_request (FEC_RST_PAD, "fec1_rst");
	gpio_direction_output (FEC_RST_PAD, 0);
	udelay (10000);
	gpio_direction_output (FEC_RST_PAD, 1);
	udelay (1000);
}

static int setup_fec(void)
{
	struct iomuxc_gpr_base_regs *const iomuxc_gpr_regs =
		(struct iomuxc_gpr_base_regs*) IOMUXC_GPR_BASE_ADDR;

	setup_iomux_fec();

	/* Use 125M anatop REF_CLK1 for ENET1, not from external */
	clrsetbits_le32(&iomuxc_gpr_regs->gpr[1],
	IOMUXC_GPR_GPR1_GPR_ENET1_TX_CLK_SEL_SHIFT,
			0);
	return set_clk_enet(ENET_125MHZ);
}

int board_phy_config(struct phy_device *phydev)
{
	/* enable rgmii rxc skew and phy mode select to RGMII copper */
	phy_write (phydev, MDIO_DEVAD_NONE, 0x1d, 0x1f);
	phy_write (phydev, MDIO_DEVAD_NONE, 0x1e, 0x8);

	phy_write (phydev, MDIO_DEVAD_NONE, 0x1d, 0x05);
	phy_write (phydev, MDIO_DEVAD_NONE, 0x1e, 0x100);

	if (phydev->drv->config)
		phydev->drv->config (phydev);

	return 0;
}
#endif /* CONFIG_FEC_MXC */

#ifdef CONFIG_OF_BOARD_SETUP

#define RDC_PDAP70      0x303d0518
#define FDT_UART_C	"/serial@30a60000"
#define FDT_NAND        "/gpmi-nand@33002000"
#define FDT_EMMC        "/mmc@30b60000"
#define FDT_CMA 	"/reserved-memory/linux,cma"

/* Do any additional board-specific device tree modifications */
int ft_board_setup(void *fdt, bd_t *bd)
{
	int offs;
	struct fs_nboot_args *pargs = fs_board_get_nboot_args ();
	const char *envvar;

	/* Set bdinfo entries */
	offs = fs_fdt_path_offset (fdt, "/bdinfo");
	if (offs >= 0)
	{
		int id = 0;
		/* Set common bdinfo entries */
		fs_fdt_set_bdinfo (fdt, offs);

		/* MAC addresses */
		if (pargs->chFeatures2 & FEAT2_ETH)
			fs_fdt_set_macaddr (fdt, offs, id++);
	}


	if(pargs->chFeatures2 & FEAT2_NAND_EMMC)
	{
		/* enable emmc node  */
		fs_fdt_enable(fdt, FDT_EMMC, 1);

		/* disable nand node  */
		fs_fdt_enable(fdt, FDT_NAND, 0);

	}

	/*TODO: Its workaround to use UART4 */
	envvar = env_get("m4_uart4");

	if (!envvar || !strcmp(envvar, "disable")) {
		/* Disable UART4 for M4. Enabled by ATF. */
		writel(0xff, RDC_PDAP70);
	}else{
		/* Disable UART_C in DT */
		fs_fdt_enable(fdt, FDT_UART_C, 0);
	}
	if (pargs->dwMemSize==1023 || pargs->dwMemSize==1024){
		fdt32_t tmp[2];
		tmp[0] = cpu_to_fdt32(0x0);
		tmp[1] = cpu_to_fdt32(0x28000000);
		offs = fs_fdt_path_offset(fdt, FDT_CMA);
		fs_fdt_set_val(fdt, offs, "size", tmp, sizeof(tmp), 1);
	}

	return 0;
}
#endif /* CONFIG_OF_BOARD_SETUP */

#ifdef CONFIG_FASTBOOT_STORAGE_MMC
int mmc_map_to_kernel_blk(int devno)
{
	return devno + 1;
}
#endif /* CONFIG_FASTBOOT_STORAGE_MMC */

#ifdef CONFIG_BOARD_POSTCLK_INIT
int board_postclk_init(void)
{
	/* TODO */
	return 0;
}
#endif /* CONFIG_BOARD_POSTCLK_INIT */
