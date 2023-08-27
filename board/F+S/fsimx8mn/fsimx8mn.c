/*
 * fsimx8mn.c
 *
 * (C) Copyright 2021 F&S Elektronik Systeme GmbH
 *
 * Board specific functions for F&S boards based on Freescale i.MX8MN CPU
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
#include <asm/arch/imx8mn_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/gpio.h>
#include <i2c.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/clock.h>
#include <spl.h>
#include <asm/mach-imx/dma.h>
#include <power/bd71837.h>
#ifdef CONFIG_USB_TCPC
#include "../common/tcpc.h"
#endif
#include <usb.h>
#include <sec_mipi_dsim.h>
#include <imx_mipi_dsi_bridge.h>
#include <mipi_dsi_panel.h>
#include <asm/mach-imx/video.h>
#include <env_internal.h>		/* enum env_operation */
#include <fdt_support.h>		/* fdt_getprop_u32_default_node() */
#include <hang.h>			/* hang() */
#include <serial.h>			/* get_serial_device() */
#include "../common/fs_fdt_common.h"	/* fs_fdt_set_val(), ... */
#include "../common/fs_board_common.h"	/* fs_board_*() */
#include "../common/fs_eth_common.h"	/* fs_eth_*() */
#include "../common/fs_image_common.h"	/* fs_image_*() */
#include <nand.h>
#include "sec_mipi_dphy_ln14lpp.h"
#include "sec_mipi_pll_1432x.h"

/* ------------------------------------------------------------------------- */

DECLARE_GLOBAL_DATA_PTR;

#define BT_PICOCOREMX8MN 	0
#define BT_PICOCOREMX8MX	1

/* Board features; these values can be resorted and redefined at will */
#define FEAT_ETH_A	(1<<0)
#define FEAT_ETH_B	(1<<1)
#define FEAT_ETH_A_PHY	(1<<2)
#define FEAT_ETH_B_PHY	(1<<3)
#define FEAT_NAND	(1<<4)
#define FEAT_EMMC	(1<<5)
#define FEAT_SGTL5000	(1<<6)
#define FEAT_WLAN	(1<<7)
#define FEAT_LVDS	(1<<8)
#define FEAT_MIPI_DSI	(1<<9)
#define FEAT_RTC85063	(1<<10)
#define FEAT_RTC85263	(1<<11)
#define FEAT_SEC_CHIP	(1<<12)
#define FEAT_CAN	(1<<13)
#define FEAT_EEPROM	(1<<14)

#define FEAT_ETH_MASK 	(FEAT_ETH_A | FEAT_ETH_B)

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

#ifdef CONFIG_FS_UPDATE_SUPPORT
#define INIT_DEF ".init_fs_updater"
#else
#define INIT_DEF ".init_init"
#endif

const struct fs_board_info board_info[] = {
	{	/* 0 (BT_PICOCOREMX8MN) */
		.name = "PicoCoreMX8MN-LPDDR4",
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = INIT_DEF,
		.flags = 0,
	},
	{	/* 1 (BT_PICOCOREMX8MX) */
		.name = "PicoCoreMX8MN-DDR3L",
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = INIT_DEF,
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
		.init = INIT_DEF,
		.flags = 0,
	},
};

/* ---- Stage 'f': RAM not valid, variables can *not* be used yet ---------- */

#ifdef CONFIG_NAND_MXS
//###static void setup_gpmi_nand(void);
#endif

static iomux_v3_cfg_t const wdog_pads[] = {
	IMX8MN_PAD_GPIO1_IO02__WDOG1_WDOG_B  | MUX_PAD_CTRL(WDOG_PAD_CTRL),
};

/* Parse the FDT of the BOARD-CFG in OCRAM and create binary info in OCRAM */
static void fs_setup_cfg_info(void)
{
	void *fdt;
	int offs;
	int i;
	struct cfg_info *info;
	const char *tmp;
	unsigned int features;
	u32 flags = 0;

	/*
	 * If the BOARD-CFG cannot be found in OCRAM or it is corrupted, this
	 * is fatal. However no output is possible this early, so simply stop.
	 * If the BOARD-CFG is not at the expected location in OCRAM but is
	 * found somewhere else, output a warning later in board_late_init().
	 */
	if (!fs_image_find_cfg_in_ocram())
		hang();

	/*
	 * The flag if running from Primary or Secondary SPL is misusing a
	 * byte in the BOARD-CFG in OCRAM, so we have to remove this before
	 * validating the BOARD-CFG.
	 */
	if (fs_image_is_secondary())
		flags |= CI_FLAGS_SECONDARY;

	/* Make sure that the BOARD-CFG in OCRAM is still valid */
	if (!fs_image_is_ocram_cfg_valid())
		hang();

	fdt = fs_image_get_cfg_fdt();
	offs = fs_image_get_board_cfg_offs(fdt);
	info = fs_board_get_cfg_info();
	memset(info, 0, sizeof(struct cfg_info));

	/* Parse BOARD-CFG entries and set according entries and flags */
	tmp = fdt_getprop(fdt, offs, "board-name", NULL);
	for (i = 0; i < ARRAY_SIZE(board_info) - 1; i++) {
		if (!strcmp(tmp, board_info[i].name))
			break;
	}
	info->board_type = i;

	tmp = fdt_getprop(fdt, offs, "boot-dev", NULL);
	info->boot_dev = fs_board_get_boot_dev_from_name(tmp);

	info->board_rev = fdt_getprop_u32_default_node(fdt, offs, 0,
						       "board-rev", 100);
	info->dram_chips = fdt_getprop_u32_default_node(fdt, offs, 0,
							"dram-chips", 1);
	info->dram_size = fdt_getprop_u32_default_node(fdt, offs, 0,
						       "dram-size", 0x400);
	info->flags = flags;

	features = 0;
	if (fdt_getprop(fdt, offs, "have-nand", NULL))
		features |= FEAT_NAND;
	if (fdt_getprop(fdt, offs, "have-emmc", NULL))
		features |= FEAT_EMMC;
	if (fdt_getprop(fdt, offs, "have-sgtl5000", NULL))
		features |= FEAT_SGTL5000;
	if (fdt_getprop(fdt, offs, "have-eth-phy", NULL)) {
		features |= FEAT_ETH_A;
		if (info->board_type == BT_PICOCOREMX8MX)
			features |= FEAT_ETH_B;
	}
	if (fdt_getprop(fdt, offs, "have-wlan", NULL))
		features |= FEAT_WLAN;
	if (fdt_getprop(fdt, offs, "have-lvds", NULL))
		features |= FEAT_LVDS;
	if (fdt_getprop(fdt, offs, "have-mipi-dsi", NULL))
		features |= FEAT_MIPI_DSI;
	if (fdt_getprop(fdt, offs, "have-rtc-pcf85063", NULL))
		features |= FEAT_RTC85063;
	if (fdt_getprop(fdt, offs, "have-rtc-pcf85263", NULL))
		features |= FEAT_RTC85263;
	if (fdt_getprop(fdt, offs, "have-security", NULL))
		features |= FEAT_SEC_CHIP;
	if (fdt_getprop(fdt, offs, "have-can", NULL))
		features |= FEAT_CAN;
	if (fdt_getprop(fdt, offs, "have-eeprom", NULL))
		features |= FEAT_EEPROM;
	info->features = features;
}

/* Do some very early board specific setup */
int board_early_init_f(void)
{
	struct wdog_regs *wdog = (struct wdog_regs*) WDOG1_BASE_ADDR;

	fs_setup_cfg_info();

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));

	set_wdog_reset(wdog);

#ifdef CONFIG_NAND_MXS
//###	setup_gpmi_nand(); /* SPL will call the board_early_init_f */
#endif

	return 0;
}

/* Return the appropriate environment depending on the fused boot device */
enum env_location env_get_location(enum env_operation op, int prio)
{
	if (prio == 0) {
		switch (fs_board_get_boot_dev()) {
		case NAND_BOOT:
			return ENVL_NAND;
		case MMC3_BOOT:
			return ENVL_MMC;
		default:
			break;
		}
	}

	return ENVL_UNKNOWN;
}

static void fs_nand_get_env_info(struct mtd_info *mtd, struct cfg_info *cfg)
{
	void *fdt;
	int offs;
	unsigned int align;
	int err;

	if (cfg->flags & CI_FLAGS_HAVE_ENV)
		return;

	/*
	 * To find the environment location, we must access the BOARD-CFG in
	 * OCRAM. However this is not a safe resource, because it can be
	 * modified at any time. This is why we copy all relevant info to
	 * struct cfg_info right early in fs_setup_cfg_info() where we checked
	 * that the BOARD-CFG is valid. But at this early stage, the flash
	 * environment is not running yet and we cannot determine writesize
	 * and erasesize. So it is not possible to get the environment
	 * addresses there.
	 *
	 * Therefore we use a cache method. The first time we are called here,
	 * which is still in board_init_f(), we can assume that the BOARD-CFG
	 * is still valid. So access the BOARD-CFG, determine the environment
	 * location and store the start addresses for both copies in the
	 * cfg_info. From then on, we can use the cfg_info when we are called.
	 *
	 * If the environment location is not contained in nboot-info, it was
	 * located in the device tree of the previous U-Boot and NBoot didn't
	 * know anything about it. We have a list of known places where the
	 * environment was located in the past, so we take the first one
	 * (=newest) of these. As the NAND list only has one entry, this
	 * should be OK.
	 */

	fdt = fs_image_get_cfg_fdt();
	offs = fs_image_get_nboot_info_offs(fdt);
	align = mtd->erasesize;

	err = fs_image_get_fdt_val(fdt, offs, "env-start", align,
				   2, cfg->env_start);
	if (err == -ENOENT) {
		/* This is an old version, use the old known position */
		err = fs_image_get_known_env_nand(0, cfg->env_start, NULL);
	}
	if (err) {
		cfg->env_start[0] = CONFIG_ENV_NAND_OFFSET;
		cfg->env_start[0] = CONFIG_ENV_NAND_OFFSET_REDUND;
	}

	cfg->flags |= CI_FLAGS_HAVE_ENV;
}

/* Return environment information if in NAND */
loff_t board_nand_get_env_offset(struct mtd_info *mtd, int copy)
{
	struct cfg_info *cfg = fs_board_get_cfg_info();

	fs_nand_get_env_info(mtd, cfg);

	return cfg->env_start[copy];
}

loff_t board_nand_get_env_range(struct mtd_info *mtd)
{
	return CONFIG_ENV_NAND_RANGE;
}

/* Check board type */
int checkboard(void)
{
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();
	unsigned int features = fs_board_get_features();

	printf ("Board: %s Rev %u.%02u (", board_info[board_type].name,
		board_rev / 100, board_rev % 100);
	if ((features & FEAT_ETH_MASK) == FEAT_ETH_MASK)
		puts ("2x ");
	if (features & FEAT_ETH_MASK)
		puts ("LAN, ");
	if (features & FEAT_WLAN)
		puts ("WLAN, ");
	if (features & FEAT_EMMC)
		puts ("eMMC, ");
	if (features & FEAT_NAND)
		puts("NAND, ");

	printf ("%dx DRAM)\n", fs_board_get_cfg_info()->dram_chips);

	return 0;
}

/* ---- Stage 'r': RAM valid, U-Boot relocated, variables can be used ------ */
#ifdef CONFIG_USB_TCPC
#define  USB_INIT_UNKNOWN (USB_INIT_DEVICE + 1)
static int setup_typec(void);
#endif
static int setup_fec(void);
void fs_ethaddr_init(void);
static int board_setup_ksz9893r(void);

int board_init(void)
{
	unsigned int board_type = fs_board_get_type();

	/* Prepare command prompt string */
	fs_board_init_common(&board_info[board_type]);

#ifdef CONFIG_USB_TCPC
	setup_typec();
#endif

#ifdef CONFIG_FEC_MXC
	setup_fec();
#endif

	if (board_type == BT_PICOCOREMX8MX) {
		board_setup_ksz9893r();
	}

	return 0;
}

#ifndef CONFIG_NAND_MXS_DT
extern void mxs_nand_register(void);

void board_nand_init(void)
{
	if (fs_board_get_features() & FEAT_NAND)
		mxs_nand_register();
}
#endif

#ifdef CONFIG_VIDEO_MXS
/*
 * Possible display configurations
 *
 *   Board               MIPI      LVDS0      LVDS1
 *   -------------------------------------------------------------
 *   PicoCoreMX8MN       4 lanes*  24 bit²    24 bit²
 *   PicoCoreMX8MX-Nano  4 lanes*  24 bit²    24 bit²
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
 *  5. fsimx8mn.c: board_video_skip parse display parameter of display_info_t,
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
 * 14. fsimx8mn.c: board_late_init() calls fs_board_set_backlight_all() to
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

	switch (fs_board_get_type())
	{
	case BT_PICOCOREMX8MN:
		i2c_bus = 3;
		break;
	case BT_PICOCOREMX8MX:
		i2c_bus = 0;
		break;
	}

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
static const struct sec_mipi_dsim_plat_data imx8mn_mipi_dsim_plat_data = {
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
#define BUS_RSTN_BLK_SYNC_SFT_EN	BIT(8)
#define LCDIF_APB_CLK_RSTN		BIT(5)
#define LCDIF_PIXEL_CLK_RSTN		BIT(4)

/* 'DISP_MIX_CLK_EN_CSR' bit fields */
#define BUS_BLK_CLK_SFT_EN		BIT(8)
#define LCDIF_PIXEL_CLK_SFT_EN		BIT(5)
#define LCDIF_APB_CLK_SFT_EN		BIT(4)

void disp_mix_bus_rstn_reset(ulong gpr_base, bool reset)
{
	if (!reset)
		/* release reset */
		setbits_le32 (gpr_base + DISPLAY_MIX_SFT_RSTN_CSR,
			      BUS_RSTN_BLK_SYNC_SFT_EN | LCDIF_APB_CLK_RSTN | LCDIF_PIXEL_CLK_RSTN);
	else
		/* hold reset */
		clrbits_le32 (gpr_base + DISPLAY_MIX_SFT_RSTN_CSR,
			      BUS_RSTN_BLK_SYNC_SFT_EN | LCDIF_APB_CLK_RSTN | LCDIF_PIXEL_CLK_RSTN);
}

void disp_mix_lcdif_clks_enable(ulong gpr_base, bool enable)
{
	if (enable)
		/* enable lcdif clks */
		setbits_le32 (gpr_base + DISPLAY_MIX_CLK_EN_CSR,
			     BUS_BLK_CLK_SFT_EN | LCDIF_PIXEL_CLK_SFT_EN | LCDIF_APB_CLK_SFT_EN);
	else
		/* disable lcdif clks */
		clrbits_le32 (gpr_base + DISPLAY_MIX_CLK_EN_CSR,
			     BUS_BLK_CLK_SFT_EN | LCDIF_PIXEL_CLK_SFT_EN | LCDIF_APB_CLK_SFT_EN);
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
	IMX8MN_PAD_SPDIF_TX__GPIO5_IO3 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

#define VLCD_ON_8MN_PAD IMX_GPIO_NR(4, 28)
static iomux_v3_cfg_t const vlcd_on_8mn_pads[] = {
	IMX8MN_PAD_SAI3_RXFS__GPIO4_IO28 | MUX_PAD_CTRL(NO_PAD_CTRL),
};
#define LVDS_RST_8MN_PAD IMX_GPIO_NR(1, 13)
static iomux_v3_cfg_t const lvds_rst_8mn_pads[] = {
	IMX8MN_PAD_GPIO1_IO13__GPIO1_IO13 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

#define VLCD_ON_8MX_PAD IMX_GPIO_NR(5, 1)
static iomux_v3_cfg_t const vlcd_on_8mx_pads[] = {
	IMX8MN_PAD_SAI3_TXD__GPIO5_IO1 | MUX_PAD_CTRL(NO_PAD_CTRL),
};
#define LVDS_RST_8MX_PAD IMX_GPIO_NR(1, 8)
#define LVDS_STBY_8MX_PAD IMX_GPIO_NR(1, 4)
static iomux_v3_cfg_t const lvds_rst_8mx_pads[] = {
	IMX8MN_PAD_GPIO1_IO08__GPIO1_IO8 | MUX_PAD_CTRL(NO_PAD_CTRL),
	IMX8MN_PAD_GPIO1_IO04__GPIO1_IO4 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

void enable_tc358764(struct display_info_t const *dev)
{
	int ret = 0;

	mxs_set_lcdclk(dev->bus, PICOS2KHZ(dev->mode.pixclock));

	clock_set_target_val (IPP_DO_CLKO2, CLK_ROOT_ON
			      | CLK_ROOT_SOURCE_SEL(1) | CLK_ROOT_POST_DIV(CLK_ROOT_POST_DIV6));
	switch (fs_board_get_type())
	{
	case BT_PICOCOREMX8MN:
		imx_iomux_v3_setup_multiple_pads (lvds_rst_8mn_pads, ARRAY_SIZE (lvds_rst_8mn_pads));
		gpio_request (LVDS_RST_8MN_PAD, "LVDS_RST");
		gpio_direction_output (LVDS_RST_8MN_PAD, 0);
		/* period of reset signal > 50 ns */
		udelay (5);
		gpio_direction_output (LVDS_RST_8MN_PAD, 1);

		break;
	case BT_PICOCOREMX8MX:
		imx_iomux_v3_setup_multiple_pads (lvds_rst_8mx_pads, ARRAY_SIZE (lvds_rst_8mx_pads));
		gpio_request (LVDS_STBY_8MX_PAD, "LVDS_STBY");
		gpio_direction_output (LVDS_STBY_8MX_PAD, 1);
		udelay (50);
		gpio_request (LVDS_RST_8MX_PAD, "LVDS_RST");
		gpio_direction_output (LVDS_RST_8MX_PAD, 0);
		/* period of reset signal > 50 ns */
		udelay (5);
		gpio_direction_output (LVDS_RST_8MX_PAD, 1);
		break;
	}

	udelay (500);
	/* enable the dispmix & mipi phy power domain */
	call_imx_sip (FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, DISPMIX, true, 0);
	call_imx_sip (FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, MIPI, true, 0);

	/* Put lcdif out of reset */
	disp_mix_bus_rstn_reset (imx8mn_mipi_dsim_plat_data.gpr_base, false);
	disp_mix_lcdif_clks_enable (imx8mn_mipi_dsim_plat_data.gpr_base, true);

	/* Setup mipi dsim */
	ret = sec_mipi_dsim_setup (&imx8mn_mipi_dsim_plat_data);

	if (ret)
		return;

	ret = imx_mipi_dsi_bridge_attach (&tc358764_dev); /* attach tc358764 device */
}

int detect_tc358764(struct display_info_t const *dev)
{
	return (fs_board_get_features() & FEAT_LVDS) ? 1 : 0;
}

int detect_mipi_disp(struct display_info_t const *dev)
{
	return (fs_board_get_features() & FEAT_MIPI_DSI) ? 1 : 0;
}

void enable_mipi_disp(struct display_info_t const *dev)
{
	/* enable the dispmix & mipi phy power domain */
	call_imx_sip (FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, DISPMIX, true, 0);
	call_imx_sip (FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, MIPI, true, 0);

	/* Put lcdif out of reset */
	disp_mix_bus_rstn_reset (imx8mn_mipi_dsim_plat_data.gpr_base, false);
	disp_mix_lcdif_clks_enable (imx8mn_mipi_dsim_plat_data.gpr_base, true);

	/* Setup mipi dsim */
	sec_mipi_dsim_setup (&imx8mn_mipi_dsim_plat_data);

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

/* Return the HW partition where U-Boot environment is on eMMC */
unsigned int mmc_get_env_part(struct mmc *mmc)
{
	unsigned int boot_part;

	boot_part = (mmc->part_config >> 3) & PART_ACCESS_MASK;
	if (boot_part == 7)
		boot_part = 0;

	return boot_part;
}

#ifdef CONFIG_USB_TCPC
struct tcpc_port port;

struct tcpc_port_config port_config = {
	.i2c_bus = 0,
	.addr = 0x52,
	.port_type = TYPEC_PORT_UFP,
	.max_snk_mv = 5000,
	.max_snk_ma = 3000,
	.max_snk_mw = 40000,
	.op_snk_mv = 9000,
	.switch_setup_func = NULL,
};

static int setup_typec(void)
{
	int ret;

	switch (fs_board_get_type())
	{
	case BT_PICOCOREMX8MN:
		port_config.i2c_bus = 3;
		break;
	case BT_PICOCOREMX8MX:
		port_config.i2c_bus = 0;
		break;
	}

	debug("tcpc_init port\n");
	ret = tcpc_init(&port, port_config, NULL);
	if (ret) {
		port.i2c_dev = NULL;
	}
	return ret;
}

int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;
	struct tcpc_port *port_ptr = &port;

	debug("board_usb_init %d, type %d\n", index, init);

	imx8m_usb_power(index, true);

	if (index == 0) {
		if (port.i2c_dev) {
			if (init == USB_INIT_HOST)
				tcpc_setup_dfp_mode(port_ptr);
			else
				tcpc_setup_ufp_mode(port_ptr);
		}
	}

	return ret;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;
	struct tcpc_port *port_ptr = &port;

	debug("board_usb_cleanup %d, type %d\n", index, init);

	if (index == 0) {
		if (port.i2c_dev) {
			if (init == USB_INIT_HOST)
				ret = tcpc_disable_src_vbus(port_ptr);
		}
	}

	imx8m_usb_power(index, false);
	return ret;
}

int board_ehci_usb_phy_mode(struct udevice *dev)
{
	int ret = 0;
	enum typec_cc_polarity pol;
	enum typec_cc_state state;
	struct tcpc_port *port_ptr = &port;

	if (port.i2c_dev) {
		if (dev_seq(dev) == 0) {

			tcpc_setup_ufp_mode(port_ptr);

			ret = tcpc_get_cc_status(port_ptr, &pol, &state);
			if (!ret) {
				if (state == TYPEC_STATE_SRC_RD_RA || state == TYPEC_STATE_SRC_RD)
					return USB_INIT_HOST;
			}
		}

		return USB_INIT_DEVICE;
	}
	else
		return USB_INIT_UNKNOWN;
}
#else
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
 *    PicoCoreMX8MN       GPIO1_12 (GPIO1_IO12)(*) -
 *    PicoCoreMX8MX-Nano  GPIO1_12 (GPIO1_IO12)(*) -
 *
 * (*) Signal on SKIT is active low, usually USB_OTG_PWR is active high
 *
 * USB1 is a host-only port (USB_H1). It is used on all boards. Some boards
 * may have an additional USB hub with a reset signal connected to this port.
 *
 *    Board               USB_H1_PWR               Hub Reset
 *    -------------------------------------------------------------------------
 *    PicoCoreMX8MN       GPIO1_14 (GPIO1_IO14)(*) -
 *    PicoCoreMX8MX-Nano  GPIO1_14 (GPIO1_IO14)(*) -
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

#if 0 //###
	/* TODO: Set here because otherwise platform would be generated from
         * name.
         */
	if (fs_board_get_type() == BT_PICOCOREMX8MX)
		env_set("platform", "picocoremx8mx-nano");
#endif

	env_set("tee", "no");
#ifdef CONFIG_IMX_OPTEE
	env_set("tee", "yes");
#endif

	/* Set up all board specific variables */
	fs_board_late_init_common("ttymxc");

	/* Set mac addresses for corresponding boards */
	fs_ethaddr_init();
#ifdef CONFIG_VIDEO_MXS
	imx_iomux_v3_setup_multiple_pads (bl_on_pads, ARRAY_SIZE (bl_on_pads));
	/* backlight off */
	gpio_request (BL_ON_PAD, "BL_ON");
	gpio_direction_output (BL_ON_PAD, 0);

	if(detect_tc358764(0))
	{
		/* initialize TC358764 over I2C */
		if(tc358764_init())
			/* error case... */
			return 0;
	}

	/* set vlcd on*/
	switch (fs_board_get_type())
	{
	case BT_PICOCOREMX8MN:
		imx_iomux_v3_setup_multiple_pads (vlcd_on_8mn_pads, ARRAY_SIZE (vlcd_on_8mn_pads));
		gpio_request (VLCD_ON_8MN_PAD, "VLCD_ON");
		gpio_direction_output (VLCD_ON_8MN_PAD, 1);
		break;
	case BT_PICOCOREMX8MX:
		imx_iomux_v3_setup_multiple_pads (vlcd_on_8mx_pads, ARRAY_SIZE (vlcd_on_8mx_pads));
		gpio_request (VLCD_ON_8MX_PAD, "VLCD_ON");
		gpio_direction_output (VLCD_ON_8MX_PAD, 1);
		break;
	default:
		break;
	}
	/* backlight on */
	gpio_direction_output (BL_ON_PAD, 1);
#endif
	return 0;
}
#endif /* CONFIG_BOARD_LATE_INIT */

#ifdef CONFIG_FEC_MXC
#define FEC_RST_PAD IMX_GPIO_NR(1, 5)
#define FEC_SIM_PAD IMX_GPIO_NR(1, 26)
static iomux_v3_cfg_t const fec1_rst_pads[] = {
	IMX8MN_PAD_GPIO1_IO05__GPIO1_IO5 | MUX_PAD_CTRL(NO_PAD_CTRL),
	IMX8MN_PAD_ENET_RD0__GPIO1_IO26 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void setup_iomux_fec(void)
{
	imx_iomux_v3_setup_multiple_pads(fec1_rst_pads,
					 ARRAY_SIZE (fec1_rst_pads));

	/* before resetting the ethernet switch for PCoreMX8MX revision 1.10 we
	 * have to configure a strapping pin to use the Serial Interface Mode
	 * "I2C". The eth node in device tree will overwrite the mux option for
	 * ENET_RD0 so we don´t have to change it back to dedicated function.
	 */
	if(fs_board_get_rev() == 110) {
		gpio_request(FEC_SIM_PAD, "SerialInterfaceMode");
		gpio_direction_output(FEC_SIM_PAD, 1);
		gpio_free(FEC_SIM_PAD);
	}
	gpio_request(FEC_RST_PAD, "fec1_rst");
	fs_board_issue_reset(11000, 1000, FEC_RST_PAD, ~0, ~0);
}

void fs_ethaddr_init(void)
{
	unsigned int features = fs_board_get_features();
	int eth_id = 0;

	if (features & FEAT_ETH_A)
		fs_eth_set_ethaddr(eth_id++);
	if (features & FEAT_ETH_B)
		fs_eth_set_ethaddr(eth_id++);
	/* All fsimx8mn boards have a WLAN module
	 * which have an integrated mac address. So we don´t
	 * have to set an own mac address for the module.
	 */
//	if (features & FEAT_WLAN)
//		fs_eth_set_ethaddr(eth_id++);
}

static int setup_fec(void)
{
	struct iomuxc_gpr_base_regs *const iomuxc_gpr_regs =
		(struct iomuxc_gpr_base_regs*) IOMUXC_GPR_BASE_ADDR;

	if(fs_board_get_type() == BT_PICOCOREMX8MX)
		setup_iomux_fec();

	/* Use 125M anatop REF_CLK1 for ENET1, not from external */
	clrsetbits_le32(&iomuxc_gpr_regs->gpr[1],
			IOMUXC_GPR_GPR1_GPR_ENET1_TX_CLK_SEL_SHIFT, 0);

	return set_clk_enet(ENET_125MHZ);
}

#define KSZ9893R_SLAVE_ADDR		0x5F
#define KSZ9893R_CHIP_ID_MSB		0x1
#define KSZ9893R_CHIP_ID_LSB		0x2
#define KSZ9893R_CHIP_ID		0x9893
#define KSZ9893R_REG_PORT_3_CTRL_1	0x3301
#define KSZ9893R_XMII_MODES		BIT(2)
#define KSZ9893R_RGMII_ID_IG		BIT(4)
static int ksz9893r_check_id(struct udevice *ksz9893_dev)
{
	uint8_t val = 0;
	uint16_t chip_id = 0;
	int ret;

	ret = dm_i2c_read(ksz9893_dev, KSZ9893R_CHIP_ID_MSB, &val, sizeof(val));
	if (ret != 0) {
		printf("%s: Cannot access ksz9893r %d\n", __func__, ret);
		return ret;
	}
	chip_id |= val << 8;
	ret = dm_i2c_read(ksz9893_dev, KSZ9893R_CHIP_ID_LSB, &val, sizeof(val));
	if (ret != 0) {
		printf("%s: Cannot access ksz9893r %d\n", __func__, ret);
		return ret;
	}
	chip_id |= val;

	if (KSZ9893R_CHIP_ID == chip_id) {
		return 0;
	} else {
		printf("%s: Device with ID register %x is not a ksz9893r\n", __func__,
			   chip_id);
		return 1;
	}
}

static int board_setup_ksz9893r(void)
{
	struct udevice *bus = 0;
	struct udevice *ksz9893_dev = NULL;
	int ret;
	int i2c_bus = 4;
	uint8_t val = 0;

	ret = uclass_get_device_by_seq(UCLASS_I2C, i2c_bus, &bus);
	if (ret)
	{
		printf("%s: No bus %d\n", __func__, i2c_bus);
		return -EINVAL;
	}

	ret = dm_i2c_probe(bus, KSZ9893R_SLAVE_ADDR, 0, &ksz9893_dev);
	if (ret)
	{
		printf("%s: No device id=0x%x, on bus %d, ret %d\n",
		       __func__, KSZ9893R_SLAVE_ADDR, i2c_bus, ret);
		return -ENODEV;
	}

	/* offset - 16-bit address */
	i2c_set_chip_offset_len(ksz9893_dev, 2);

	/* check id if ksz9893 is available */
	ret = ksz9893r_check_id(ksz9893_dev);
	if (ret != 0)
		return ret;

	/* Set ingress delay (on TXC) to 1.5ns and disable In-Band Status */
	ret = dm_i2c_read(ksz9893_dev, KSZ9893R_REG_PORT_3_CTRL_1, &val,
					  sizeof(val));
	if (ret != 0) {
		printf("%s: Cannot access register %x of ksz9893r %d\n",
		       __func__, KSZ9893R_REG_PORT_3_CTRL_1, ret);
		return ret;
	}
	val |= KSZ9893R_RGMII_ID_IG;
	val &= ~KSZ9893R_XMII_MODES;
	ret = dm_i2c_write(ksz9893_dev, KSZ9893R_REG_PORT_3_CTRL_1, &val,
					   sizeof(val));
	if (ret != 0) {
		printf("%s: Cannot access register %x of ksz9893r %d\n",
		       __func__, KSZ9893R_REG_PORT_3_CTRL_1, ret);
		return ret;
	}

	return ret;
}

int board_phy_config(struct phy_device *phydev)
{
	if (fs_board_get_type() != BT_PICOCOREMX8MX) {
		/* enable rgmii rxc skew and phy mode select to RGMII copper */
		phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x1f);
		phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x8);

		phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x05);
		phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x100);
	}

	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}
#endif /* CONFIG_FEC_MXC */

#define RDC_PDAP70      0x303d0518
#define RDC_PDAP105     0x303d05A4
#define FDT_UART_C      "serial3"
#define FDT_NAND        "nand"
#define FDT_EMMC        "emmc"
#define FDT_CMA         "/reserved-memory/linux,cma"
#define FDT_RTC85063    "rtcpcf85063"
#define FDT_RTC85263    "rtcpcf85263"
#define FDT_EEPROM      "eeprom"
#define FDT_CAN         "mcp2518fd"
#define FDT_SGTL5000    "sgtl5000"
#define FDT_I2C_SWITCH  "i2c4"

/* Do all fixups that are done on both, U-Boot and Linux device tree */
static int do_fdt_board_setup_common(void *fdt)
{
	unsigned int features = fs_board_get_features();

	/* Disable NAND if it is not available */
	if (!(features & FEAT_NAND))
		fs_fdt_enable(fdt, FDT_NAND, 0);

	/* Disable eMMC if it is not available */
	if (!(features & FEAT_EMMC))
		fs_fdt_enable(fdt, FDT_EMMC, 0);

	return 0;
}

/* Do any board-specific modifications on U-Boot device tree before starting */
int board_fix_fdt(void *fdt)
{
	/* Make some room in the FDT */
	fdt_shrink_to_minimum(fdt, 8192);

	return do_fdt_board_setup_common(fdt);
}

/* Do any additional board-specific modifications on Linux device tree */
int ft_board_setup(void *fdt, struct bd_info *bd)
{
	const char *envvar;
	int offs;
	unsigned int board_type = fs_board_get_type();
	unsigned int features = fs_board_get_features();

	int id = 0;

	/* The following stuff is only set in Linux device tree */
	/* Disable RTC85063 if it is not available */
	if (!(features & FEAT_RTC85063))
		fs_fdt_enable(fdt, FDT_RTC85063, 0);

	/* Disable RTC85263 if it is not available */
	if (!(features & FEAT_RTC85263))
		fs_fdt_enable(fdt, FDT_RTC85263, 0);

	/* Disable EEPROM if it is not available */
	if (!(features & FEAT_EEPROM))
		fs_fdt_enable(fdt, FDT_EEPROM, 0);

	/* Disable CAN-FD if it is not available */
	if (!(features & FEAT_CAN))
		fs_fdt_enable(fdt, FDT_CAN, 0);

	/* Disable SGTL5000 if it is not available */
	if (!(features & FEAT_SGTL5000))
		fs_fdt_enable(fdt, FDT_SGTL5000, 0);

	/* Disable I2C for switch if it is not available */
	if (!(features & FEAT_ETH_A) && (board_type == BT_PICOCOREMX8MX))
		fs_fdt_enable(fdt, FDT_I2C_SWITCH, 0);

	/* Set bdinfo entries */
	offs = fs_fdt_path_offset(fdt, "/bdinfo");
	if (offs >= 0) {
		/* Set common bdinfo entries */
		fs_fdt_set_bdinfo(fdt, offs);

		/* MAC addresses */
		if (features & FEAT_ETH_A)
			fs_fdt_set_macaddr(fdt, offs, id++);
		if (features & FEAT_ETH_B)
			fs_fdt_set_macaddr(fdt, offs, id++);
		/* All fsimx8mn boards have a WLAN module
		 * which have an integrated mac address. So we don´t
		 * have to set an own mac address for the module.
		 */
//		if (features & FEAT_WLAN)
//			fs_fdt_set_macaddr(fdt, offs, id++);
	}

	/*TODO: Its workaround to use UART4 */
	envvar = env_get("m4_uart4");
	if (!envvar || !strcmp(envvar, "disable")) {
		/* Disable UART4 for M4. Enabled by ATF. */
		writel(0xff, RDC_PDAP70);
		writel(0xff, RDC_PDAP105);
	} else {
		/* Disable UART_C in DT */
		fs_fdt_enable(fdt, FDT_UART_C, 0);
	}

	/*
	 * Set linux,cma size depending on RAM size. Keep default (320MB) from
	 * device tree if < 1GB, increase to 640MB otherwise.
	 */
	if (fs_board_get_cfg_info()->dram_size >= 1023)	{
		fdt32_t tmp[2];

		tmp[0] = cpu_to_fdt32(0x0);
		tmp[1] = cpu_to_fdt32(0x28000000);

		offs = fs_fdt_path_offset(fdt, FDT_CMA);
		fs_fdt_set_val(fdt, offs, "size", tmp, sizeof(tmp), 1);
	}

	return do_fdt_board_setup_common(fdt);
}

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
