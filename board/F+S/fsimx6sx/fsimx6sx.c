/*
 * fsimx6sx.c
 *
 * (C) Copyright 2015
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Board specific functions for F&S boards based on Freescale i.MX6 CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#ifdef CONFIG_CMD_NET
#include <miiphy.h>
#include <netdev.h>
#include "../common/fs_eth_common.h"	/* fs_eth_*() */
#endif
#include <serial.h>			/* struct serial_device */
#include <env_internal.h>

#ifdef CONFIG_FSL_ESDHC_IMX
#include <mmc.h>
#include <fsl_esdhc_imx.h>		/* fsl_esdhc_initialize(), ... */
#include "../common/fs_mmc_common.h"	/* struct fs_mmc_cd, fs_mmc_*(), ... */
#endif

#ifdef CONFIG_CMD_LED
#include <status_led.h>			/* led_id_t */
#endif

#ifdef CONFIG_MXC_SPI
#include <spi.h>			/* SPI_MODE_*, spi_xfer(), ... */
#include <u-boot/crc.h>			/* crc32() */
#endif

#ifdef CONFIG_VIDEO_MXS
#include <linux/fb.h>
#include <mxsfb.h>
#include "../common/fs_disp_common.h"	/* struct fs_disp_port, fs_disp_*() */
#endif

#include <i2c.h>
#include <asm/mach-imx/mxc_i2c.h>

#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/video.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/iomux.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/crm_regs.h>		/* CCM_CCGR1, nandf clock settings */
#include <asm/arch/clock.h>		/* enable_fec_anatop_clock(), ... */

#include <linux/delay.h>		/* mdelay() */
#include <linux/mtd/rawnand.h>		/* struct mtd_info, struct nand_chip */
#include <mtd/mxs_nand_fus.h>		/* struct mxs_nand_fus_platform_data */
#include <usb.h>			/* USB_INIT_HOST, USB_INIT_DEVICE */
#include <malloc.h>			/* free() */
#include <fdt_support.h>		/* do_fixup_by_path_u32(), ... */
#include <fuse.h>			/* fuse_read() */
#include "../common/fs_fdt_common.h"	/* fs_fdt_set_val(), ... */
#include "../common/fs_board_common.h"	/* fs_board_*() */
#include "../common/fs_usb_common.h"	/* struct fs_usb_port_cfg, fs_usb_*() */

/* ------------------------------------------------------------------------- */

#define BT_EFUSA9X      0
#define BT_PICOCOMA9X   1
#define BT_KEN116       2			/* Not supported in Linux */
#define BT_BEMA9X       3
#define BT_CONT1        4
#define BT_PCOREMX6SX   6
#define BT_VAND3        7 /* in N-Boot number 25 */
#define BT_EFUSA9XR2    8 /* in N-Boot number 32 */
#define BT_PCOREMX6SXR2 9 /* in N-Boot number 33 */

/* Features set in fs_nboot_args.chFeature2 (available since NBoot VN27) */
#define FEAT2_ETH_A   (1<<0)		/* 0: no LAN0, 1; has LAN0 */
#define FEAT2_ETH_B   (1<<1)		/* 0: no LAN1, 1; has LAN1 */
#define FEAT2_EMMC    (1<<2)		/* 0: no eMMC, 1: has eMMC */
#define FEAT2_WLAN    (1<<3)		/* 0: no WLAN, 1: has WLAN */
#define FEAT2_HDMICAM (1<<4)		/* 0: LCD-RGB, 1: HDMI+CAM (PicoMOD) */
#define FEAT2_AUDIO   (1<<5)		/* 0: Codec onboard, 1: Codec extern */
#define FEAT2_SPEED   (1<<6)		/* 0: Full speed, 1: Limited speed */
#define FEAT2_ETH_MASK (FEAT2_ETH_A | FEAT2_ETH_B)

/* NBoot before VN27 did not report feature values; use reasonable defaults */
#define FEAT1_DEFAULT 0
#define FEAT2_DEFAULT (FEAT2_ETH_A | FEAT2_ETH_B)

#define RPMSG_SIZE	0x00010000	/* Use 64KB shared memory for RPMsg */
#define M4_DRAM_MAX_CODE_SIZE 0x10000000

/* Device tree paths */
#define FDT_NAND         "nand"
#define FDT_NAND_LEGACY  "/soc/gpmi-nand@01806000"
#define FDT_EMMC         "emmc"
#define FDT_ETH_A        "ethernet0"
#define FDT_ETH_A_LEGACY "/soc/aips-bus@02100000/ethernet@02188000"
#define FDT_ETH_B        "ethernet1"
#define FDT_ETH_B_LEGACY "/soc/aips-bus@02100000/ethernet@021b4000"
#define FDT_RPMSG        "rpmsg"
#define FDT_RPMSG_LEGACY "/soc/aips-bus@02200000/rpmsg"
#define FDT_RES_MEM      "/reserved-memory"
#define FDT_GPU          "gpu3d"
#define FDT_GPC          "gpc"

#define GPU_DISABLE_MASK (0x4)

#define UART_PAD_CTRL  (PAD_CTL_PUS_100K_UP |			\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm |			\
	PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define ENET_PAD_CTRL  (PAD_CTL_PUS_100K_UP | PAD_CTL_PUE |     \
        PAD_CTL_SPEED_HIGH   |                                   \
        PAD_CTL_DSE_48ohm   | PAD_CTL_SRE_FAST)
#define ENET_CLK_PAD_CTRL  (PAD_CTL_SPEED_MED | \
        PAD_CTL_DSE_120ohm   | PAD_CTL_SRE_FAST)
#define ENET_RX_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |          \
        PAD_CTL_SPEED_HIGH   | PAD_CTL_SRE_FAST)

#define GPMI_PAD_CTRL0 (PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP)
#define GPMI_PAD_CTRL1 (PAD_CTL_DSE_40ohm | PAD_CTL_SPEED_MED | PAD_CTL_SRE_FAST)
#define GPMI_PAD_CTRL2 (GPMI_PAD_CTRL0 | GPMI_PAD_CTRL1)

#define USDHC_PAD_EXT (PAD_CTL_HYS | PAD_CTL_PUS_47K_UP |	\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm | PAD_CTL_SRE_FAST)
#define USDHC_CLK_EXT (PAD_CTL_HYS | PAD_CTL_SPEED_MED |	\
	PAD_CTL_DSE_40ohm | PAD_CTL_SRE_FAST)
#define USDHC_PAD_INT (PAD_CTL_HYS | PAD_CTL_PUS_47K_UP |	\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_120ohm | PAD_CTL_SRE_FAST)
#define USDHC_CLK_INT (PAD_CTL_HYS | PAD_CTL_SPEED_MED |	\
	PAD_CTL_DSE_120ohm | PAD_CTL_SRE_FAST)
#define USDHC_CD_CTRL (PAD_CTL_PUS_47K_UP | PAD_CTL_SPEED_LOW | PAD_CTL_HYS)

#define LCD_CTRL PAD_CTL_DSE_120ohm

#define SPI_PAD_CTRL (PAD_CTL_HYS | PAD_CTL_SPEED_MED | \
		      PAD_CTL_DSE_40ohm | PAD_CTL_SRE_FAST)
#define SPI_CS_PAD_CTRL (PAD_CTL_HYS | PAD_CTL_PUS_100K_UP | PAD_CTL_PKE | PAD_CTL_PUE | \
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm | PAD_CTL_SRE_FAST)

#define USB_ID_PAD_CTRL (PAD_CTL_PUS_47K_UP | PAD_CTL_SPEED_LOW | PAD_CTL_HYS)

#define I2C_PAD_CTRL  (PAD_CTL_PUS_100K_UP |			\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm | PAD_CTL_HYS |	\
	PAD_CTL_ODE | PAD_CTL_SRE_FAST)

#define INSTALL_RAM "ram@80300000"
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

const struct fs_board_info board_info[10] = {
	{	/* 0 (BT_EFUSA9X) */
		.name = "efusA9X",
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
	{	/* 1 (BT_PicoCOMA9X) */
		.name = "PicoCOMA9X",
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
	{	/* 2 (BT_KEN116) */
		.name = "KEN116",
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
	{	/* 3 (BT_BEMA9X) */
		.name = "BemA9X",
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
	{	/* 4 (BT_CONT1) */
		.name = "CONT1",
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
	{	/* 5 (unknown) */
		.name = "unknown",
	},
	{	/* 6 (BT_PCOREMX6SX) */
		.name = "PicoCoreMX6SX",
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
	{	/* 7 (BT_VAND3) */
		.name = "VAND3",
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
	{	/* 8 (BT_EFUSA9XR2) */
		.name = "efusA9Xr2",
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
	{	/* 9 (BT_PCOREMX6SXR2) */
		.name = "PicoCoreMX6SXr2",
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

#ifdef CONFIG_VIDEO_MXS
static void setup_lcd_pads(int on);	/* Defined below */
#endif

int board_early_init_f(void)
{
#ifdef CONFIG_VIDEO_MXS
	/*
	 * Set pull-down resistors on display signals; some displays do not
	 * like high level on data signals when VLCD is not applied yet.
	 *
	 * FIXME: This should actually only happen if display is really in
	 * use, i.e. if device tree activates lcd. However we do not know this
	 * at this point of time.
	 *
	 * SOLUTION: When display configuration is solely done via U-Boot in
	 * the future, i.e. U-Boot changes the device tree on-the-fly, we can
	 * remove the call here and only set LCD pads to pull-down below in
	 * board_display_set_vlcd().
	 */
	setup_lcd_pads(0);
#endif

	return 0;
}

enum boot_device fs_board_get_boot_dev(void)
{
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();
	unsigned int board_type = fs_board_get_type();
	unsigned int features2 = pargs->chFeatures2;
	enum boot_device boot_dev = UNKNOWN_BOOT;

	switch (board_type) {
	case BT_PCOREMX6SX:
	case BT_PCOREMX6SXR2:
		if (features2 & FEAT2_EMMC) {
			 boot_dev = MMC2_BOOT;
			 break;
		}
	default:
		boot_dev = NAND_BOOT;
		break;
	}

	return boot_dev;
}

/* Return the appropriate environment depending on the fused boot device */
enum env_location env_get_location(enum env_operation op, int prio)
{
	if (prio == 0) {
		switch (fs_board_get_boot_dev()) {
		case NAND_BOOT:
			return ENVL_NAND;
		case SD1_BOOT:
		case SD2_BOOT:
		case SD3_BOOT:
		case SD4_BOOT:
		case MMC1_BOOT:
		case MMC2_BOOT:
		case MMC3_BOOT:
		case MMC4_BOOT:
			return ENVL_MMC;
		default:
			break;
		}
	}

	return ENVL_UNKNOWN;
}

/* We dont want to use the common "is_usb_boot" function, which returns true
 * if we are e.g. transfer an U-Boot via NetDCU-USB-Loader in N-Boot and
 * execute the image. We booted from fuse so fuse should be checked and not
 * some flags. Therefore we return always false.
 */
bool is_usb_boot(void) {
	return false;
}

/* Check board type */
int checkboard(void)
{
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();
	unsigned int features2;

	/* NBoot versions before VN27 did not report feature values */
	if ((be32_to_cpu(pargs->dwNBOOT_VER) & 0xFFFF) < 0x3237) { /* "27" */
		pargs->chFeatures1 = FEAT1_DEFAULT;
		pargs->chFeatures2 = FEAT2_DEFAULT;
	}
	features2 = pargs->chFeatures2;

	printf("Board: %s Rev %u.%02u (", board_info[board_type].name,
	       board_rev / 100, board_rev % 100);
	if ((board_type != BT_BEMA9X)
	    && ((features2 & FEAT2_ETH_MASK) == FEAT2_ETH_MASK))
		puts("2x ");
	if (features2 & FEAT2_ETH_MASK)
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

static iomux_v3_cfg_t const efusa9x_reset_pads[] = {
	IOMUX_PADS(PAD_ENET1_CRS__GPIO2_IO_1 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const efusa9x_wlanbt_en_pads[] = {
	IOMUX_PADS(PAD_GPIO1_IO03__GPIO1_IO_3 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

int board_init(void)
{
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();
	unsigned int features2 = fs_board_get_nboot_args()->chFeatures2;
	unsigned int active_us = 1000;

	/* Copy NBoot args to variables and prepare command prompt string */
	fs_board_init_common(&board_info[fs_board_get_type()]);

	/*
	 * efusA9X has a generic RESETOUTn signal to reset on-board WLAN (only
	 * board revisions before 1.20), PCIe and that is also available on
	 * the efus connector pin 14 and in turn on pin 8 of the SKIT feature
	 * connector. Because there might be some rather slow hardware
	 * involved, use a rather long low pulse of 1ms.
	 *
	 * REMARK:
	 * The WLAN chip used on board board revisions before 1.20 needs up to
	 * 300ms PDn time (which is connected to RESETOUTn). Our experience
	 * showed that 100ms is enough, so use 100ms in this case.
	 *
	 * FIXME: Should we do this somewhere else when we know the pulse time?
	 */
	switch(board_type) {
		case BT_EFUSA9X:
			if ((features2 && FEAT2_WLAN) && (board_rev < 120))
				active_us = 100000;
			SETUP_IOMUX_PADS(efusa9x_reset_pads);
			fs_board_issue_reset(active_us, 0, IMX_GPIO_NR(2, 1), ~0, ~0);

			/* Toggle WL_EN and BT_EN on Silex chip */
			if ((features2 && FEAT2_WLAN) && (board_rev >= 120)) {
				SETUP_IOMUX_PADS(efusa9x_wlanbt_en_pads);
				fs_board_issue_reset(1000, 0, IMX_GPIO_NR(1, 3),
							 ~0, ~0);
			}
			break;
		case BT_EFUSA9XR2:
			SETUP_IOMUX_PADS(efusa9x_reset_pads);
			fs_board_issue_reset(active_us, 0, IMX_GPIO_NR(2, 1), ~0, ~0);

			/* Toggle WL_EN and BT_EN on Silex chip */
			if (features2 && FEAT2_WLAN) {
				SETUP_IOMUX_PADS(efusa9x_wlanbt_en_pads);
				fs_board_issue_reset(1000, 0, IMX_GPIO_NR(1, 3),
							 ~0, ~0);
			}
			break;
	}

	return 0;
}


/* nand flash pads */
static iomux_v3_cfg_t const nfc_pads[] = {
	IOMUX_PADS(PAD_NAND_CLE__RAWNAND_CLE | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_ALE__RAWNAND_ALE | MUX_PAD_CTRL(NO_PAD_CTRL)),
	IOMUX_PADS(PAD_NAND_WP_B__RAWNAND_WP_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_READY_B__RAWNAND_READY_B | MUX_PAD_CTRL(GPMI_PAD_CTRL0)),
	IOMUX_PADS(PAD_NAND_CE0_B__RAWNAND_CE0_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_RE_B__RAWNAND_RE_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_WE_B__RAWNAND_WE_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA00__RAWNAND_DATA00 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA01__RAWNAND_DATA01 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA02__RAWNAND_DATA02 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA03__RAWNAND_DATA03 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA04__RAWNAND_DATA04 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA05__RAWNAND_DATA05 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA06__RAWNAND_DATA06 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA07__RAWNAND_DATA07 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
};

/* Register NAND devices. We actually split the NAND into two virtual devices
   to allow different ECC strategies for NBoot and the rest. */
void board_nand_init(void)
{
	int reg;
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	struct mxs_nand_fus_platform_data pdata;

	if (get_boot_device() != NAND_BOOT)
		return;

	/* config gpmi nand iomux */
	SETUP_IOMUX_PADS(nfc_pads);

#if 0 // ### Keep clock source as set by NBoot
	/* Set GPMI clock source (enfc) to USB0 (480MHz) */
	reg = readl(&mxc_ccm->cs2cdr);
	reg &= ~(3 << 16);
	reg |= (2 << 16);
	writel(reg, &mxc_ccm->cs2cdr);
#endif

	/* Enable GPMI and BCH clocks */
	reg = readl(&mxc_ccm->CCGR4);
	reg |= 0xFF003000;
	writel(reg, &mxc_ccm->CCGR4);

	/* Enable APBH DMA clock*/
	reg = readl(&mxc_ccm->CCGR0);
	reg |= 0x00000030;
	writel(reg, &mxc_ccm->CCGR0);

	/* The first device skips the NBoot region (2 blocks) to protect it
	   from inadvertent erasure. The skipped region can not be written
	   and is always read as 0xFF. */
	pdata.options = NAND_BBT_SCAN2NDPAGE;
	pdata.timing0 = 0;
	pdata.ecc_strength = fs_board_get_nboot_args()->chECCtype;
	pdata.skipblocks = 2;
	pdata.flags = MXS_NAND_CHUNK_1K;
#ifdef CONFIG_NAND_REFRESH
	pdata.backup_sblock = CONFIG_SYS_NAND_BACKUP_START_BLOCK;
	pdata.backup_eblock = CONFIG_SYS_NAND_BACKUP_END_BLOCK;
#endif
	mxs_nand_register(0, &pdata);

#if CONFIG_SYS_MAX_NAND_DEVICE > 1
	/* The second device just consists of the NBoot region (2 blocks) and
	   is software write-protected by default. It uses a different ECC
	   strategy. ### TODO ### In fact we actually need special code to
	   store the NBoot image. */
	pdata.options |= NAND_SW_WRITE_PROTECT;
	pdata.ecc_strength = 40;
	pdata.flags = MXS_NAND_SKIP_INVERSE;
#ifdef CONFIG_NAND_REFRESH
	pdata.backupstart = 0;
	pdata.backupend = 0;
#endif
	mxs_nand_register(0, &pdata);
#endif
}

#ifdef CONFIG_FSL_ESDHC_IMX
/*
 * SD/MMC support.
 *
 *   Board          USDHC   CD-Pin                 Slot
 *   -----------------------------------------------------------------------
 *   efusA9X (Board Rev < 1.20):
 *                  USDHC2  GPIO1_IO06             SD_B: Connector (SD)
 *        either:   USDHC1  GPIO1_IO02             SD_A: Connector (Micro-SD)
 *            or:  [USDHC1  GPIO1_IO02             WLAN]
 *                  USDHC4  -                      eMMC (8-Bit)
 *   efusA9X (Board Rev >= 1.20):
 *                  USDHC2  GPIO1_IO06             SD_B: Connector (SD)
 *        either:   USDHC4  SD4_DATA7 (GPIO6_IO21) SD_A: Connector (Micro-SD)
 *            or:   USDHC4  -                      eMMC (8-Bit)
 *                 [USDHC1  GPIO1_IO02             WLAN]
 *
 *   efusA9Xr2:
 *                  USDHC2  GPIO1_IO06             SD_B: Connector (SD)
 *        either:   USDHC4  SD4_DATA7 (GPIO6_IO21) SD_A: Connector (Micro-SD)
 *            or:   USDHC4  -                      eMMC (8-Bit)
 *                 [USDHC1  GPIO1_IO02             WLAN]
 *   -----------------------------------------------------------------------
 *   PicoCoreMX6SX: USDHC4  KEY_COL2 (GPIO2_IO12)  SD_A: Connector (Micro-SD)
 *   -----------------------------------------------------------------------
 *   PicoCOMA9X:    USDHC2  -                      Connector (SD)
 *                  USDHC4 [KEY_COL2 (GPIO2_IO12)] eMMC (8-Bit)
 *   -----------------------------------------------------------------------
 *   BEMA9X:        USDHC2  -                      Connector (SD)
 *                 [USDHC4  KEY_COL2 (GPIO2_IO12)  WLAN]
 *   -----------------------------------------------------------------------
 *   CONT1:         USDHC2  GPIO1_IO06             On-board Micro-SD
 *
 * Remark: The WP pin is ignored in U-Boot, also WLAN
 */

/* SD/MMC card pads definition, distinguish external from internal ports */
static iomux_v3_cfg_t const usdhc1_sd_pads_ext[] = {
	IOMUX_PADS(PAD_SD1_CLK__USDHC1_CLK | MUX_PAD_CTRL(USDHC_CLK_EXT)),
	IOMUX_PADS(PAD_SD1_CMD__USDHC1_CMD | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD1_DATA0__USDHC1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD1_DATA1__USDHC1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD1_DATA2__USDHC1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD1_DATA3__USDHC1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
};

static iomux_v3_cfg_t const usdhc2_sd_pads_ext[] = {
	IOMUX_PADS(PAD_SD2_CLK__USDHC2_CLK | MUX_PAD_CTRL(USDHC_CLK_EXT)),
	IOMUX_PADS(PAD_SD2_CMD__USDHC2_CMD | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD2_DATA0__USDHC2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD2_DATA1__USDHC2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD2_DATA2__USDHC2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD2_DATA3__USDHC2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
};

static iomux_v3_cfg_t const usdhc2_sd_pads_int[] = {
	IOMUX_PADS(PAD_SD2_CLK__USDHC2_CLK | MUX_PAD_CTRL(USDHC_CLK_INT)),
	IOMUX_PADS(PAD_SD2_CMD__USDHC2_CMD | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_NAND_RE_B__USDHC2_RESET_B | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD2_DATA0__USDHC2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD2_DATA1__USDHC2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD2_DATA2__USDHC2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD2_DATA3__USDHC2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_NAND_DATA04__USDHC2_DATA4 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_NAND_DATA05__USDHC2_DATA5 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_NAND_DATA06__USDHC2_DATA6 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_NAND_DATA07__USDHC2_DATA7 | MUX_PAD_CTRL(USDHC_PAD_INT)),
};

static iomux_v3_cfg_t const usdhc4_sd_pads_ext_rst[] = {
	IOMUX_PADS(PAD_SD4_RESET_B__USDHC4_RESET_B | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD4_CLK__USDHC4_CLK     | MUX_PAD_CTRL(USDHC_CLK_EXT)),
	IOMUX_PADS(PAD_SD4_CMD__USDHC4_CMD     | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD4_DATA0__USDHC4_DATA0 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD4_DATA1__USDHC4_DATA1 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD4_DATA2__USDHC4_DATA2 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD4_DATA3__USDHC4_DATA3 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
};

static iomux_v3_cfg_t const usdhc4_sd_pads_int[] = {
	IOMUX_PADS(PAD_SD4_CLK__USDHC4_CLK     | MUX_PAD_CTRL(USDHC_CLK_INT)),
	IOMUX_PADS(PAD_SD4_CMD__USDHC4_CMD     | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD4_RESET_B__USDHC4_RESET_B | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD4_DATA0__USDHC4_DATA0 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD4_DATA1__USDHC4_DATA1 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD4_DATA2__USDHC4_DATA2 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD4_DATA3__USDHC4_DATA3 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD4_DATA4__USDHC4_DATA4 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD4_DATA5__USDHC4_DATA5 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD4_DATA6__USDHC4_DATA6 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD4_DATA7__USDHC4_DATA7 | MUX_PAD_CTRL(USDHC_PAD_INT)),
};

/* CD on pad GPIO1_IO02 */
static iomux_v3_cfg_t const cd_gpio1_io02[] = {
	IOMUX_PADS(PAD_GPIO1_IO02__GPIO1_IO_2 | MUX_PAD_CTRL(USDHC_CD_CTRL)),
};

/* CD on pad GPIO1_IO06 */
static iomux_v3_cfg_t const cd_gpio1_io06[] = {
	IOMUX_PADS(PAD_GPIO1_IO06__GPIO1_IO_6 | MUX_PAD_CTRL(USDHC_CD_CTRL)),
};

/* CD on pad SD4_DATA7 */
static iomux_v3_cfg_t const cd_sd4_data7[] = {
	IOMUX_PADS(PAD_SD4_DATA7__GPIO6_IO_21 | MUX_PAD_CTRL(USDHC_CD_CTRL)),
};

/* CD on pad KEY_COL2 */
static iomux_v3_cfg_t const cd_key_col2[] = {
	IOMUX_PADS(PAD_KEY_COL2__GPIO2_IO_12 | MUX_PAD_CTRL(USDHC_CD_CTRL)),
};

enum usdhc_pads {
	usdhc1_ext, usdhc2_ext, usdhc2_int,
	usdhc4_ext, usdhc4_ext_rst, usdhc4_int
};

static struct fs_mmc_cfg sdhc_cfg[] = {
			  /* pads,                       count, USDHC# */
	[usdhc1_ext]     = { usdhc1_sd_pads_ext,         2,     1 },
	[usdhc2_ext]     = { usdhc2_sd_pads_ext,         2,     2 },
	[usdhc2_int]     = { usdhc2_sd_pads_int,         3,     2 },
	[usdhc4_ext_rst] = { usdhc4_sd_pads_ext_rst,     3,     4 },
	[usdhc4_ext]     = { &usdhc4_sd_pads_ext_rst[1], 2,     4 },
	[usdhc4_int]     = { usdhc4_sd_pads_int,         3,     4 },
};

enum usdhc_cds {
	gpio1_io02, gpio1_io06, gpio6_io21, gpio2_io12
};

static const struct fs_mmc_cd sdhc_cd[] = {
		      /* pad,           gpio */
	[gpio1_io02] = { cd_gpio1_io02, IMX_GPIO_NR(1, 2) },
	[gpio1_io06] = { cd_gpio1_io06, IMX_GPIO_NR(1, 6) },
	[gpio6_io21] = { cd_sd4_data7,  IMX_GPIO_NR(6, 21) },
	[gpio2_io12] = { cd_key_col2,   IMX_GPIO_NR(2, 12) }
};

int board_mmc_init(struct bd_info *bd)
{
	int ret = 0;
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();
	unsigned int features2 = fs_board_get_nboot_args()->chFeatures2;

	switch (board_type) {
	case BT_EFUSA9X:
		/* mmc0: USDHC2 (ext. SD slot, normal-size SD on efus SKIT) */
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc2_ext],
				   &sdhc_cd[gpio1_io06]);
		if (ret)
			break;

		/* mmc1 (ext. SD slot, micro SD on efus SKIT) */
		if (board_rev < 120) {
			/* Board Rev before 1.20: if no WLAN present: USDHC1 */
			if (!(features2 & FEAT2_WLAN))
				ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc1_ext],
						   &sdhc_cd[gpio1_io02]);
		} else {
			/* Board Rev since 1.20: if no eMMC present: USDHC4 */
			if (!(features2 & FEAT2_EMMC))
				ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc4_ext],
						   &sdhc_cd[gpio6_io21]);
		}

		/* mmc2: USDHC4 (eMMC, if available), no CD */
		if (!ret && (features2 & FEAT2_EMMC))
			ret = fs_mmc_setup(bd, 8, &sdhc_cfg[usdhc4_int], NULL);
		break;

	case BT_EFUSA9XR2:
		/* mmc0: USDHC2 (ext. SD slot, normal-size SD on efus SKIT) */
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc2_ext],
				   &sdhc_cd[gpio1_io06]);
		if (ret)
			break;

		/* mmc1: USDHC4 (eMMC, if available), no CD */
		if (features2 & FEAT2_EMMC)
			ret = fs_mmc_setup(bd, 8, &sdhc_cfg[usdhc4_int], NULL);
		else /* USDHC4 (ext. SD slot, micro SD on efus SKIT, if eMMC not available)  */
			ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc4_ext],
					   &sdhc_cd[gpio6_io21]);
		break;

	case BT_PICOCOMA9X:
		/* mmc0: USDHC2 (ext. SD slot via connector), no CD */
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc2_ext], NULL);

		/* mmc1: USDHC4 (eMMC, if available), ignore CD */
		if (!ret && (features2 & FEAT2_EMMC))
			ret = fs_mmc_setup(bd, 8, &sdhc_cfg[usdhc4_int], NULL);
		break;

	case BT_BEMA9X:
		/* mmc0: USDHC2 (ext. SD slot via connector), no CD */
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc2_ext], NULL);
		break;

	case BT_CONT1:
		/* mmc0: USDHC2 (int. SD slot, micro SD on CONT1) */
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc2_ext],
				   &sdhc_cd[gpio1_io06]);
		break;

	case BT_PCOREMX6SX:
	case BT_PCOREMX6SXR2:
		/* mmc0: USDHC4 (ext. SD slot, micro SD on picocore SKIT) */
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc4_ext_rst],
				   &sdhc_cd[gpio2_io12]);

		/* mmc1: USDHC2 (eMMC, if available), no CD */
		if (!ret && (features2 & FEAT2_EMMC))
			ret = fs_mmc_setup(bd, 8, &sdhc_cfg[usdhc2_int], NULL);
		break;

	default:
		return 0;		/* Neither SD card, nor eMMC */
	}

	return ret;
}
#endif /* CONFIG_FSL_ESDHC_IMX */

#ifdef CONFIG_VIDEO_MXS
/*
 * Possible display configurations
 *
 *   Board          LCD      LVDS
 *   -------------------------------------------------------------
 *   efusA9X        18 bit*  24 bit
 *   PicoCoreMX6SX  24 bit*  -
 *   PicoCOMA9X     18 bit*  -
 *   BemA9X         18 bit*  -
 *   CONT1          -        .
 *
 * The entry marked with * is the default port.
 *
 * Display initialization sequence:
 *
 *  1. board_r.c: board_init_r() calls stdio_init()
 *  2. stdio.c: stdio_init() calls drv_video_init()
 *  3. cfb_console.c: drv_video_init() calls board_video_skip(); if this
 *     returns non-zero, the display will not be started
 *  4. fsimx6sx.c: board_video_skip(): Call fs_disp_register(). This checks for
 *     display parameters and activates the display. To handle board specific
 *     stuff, it will call callback functions board_display_set_backlight(),
 *     board_display_set_power() and board_display_start() here. The latter
 *     will initialize the clocks and call mxs_lcd_panel_setup() to store mode
 *     and bpp to be used later.
 *  5. cfb_console.c: drv_video_init() calls video_init()
 *  6. cfb_console.c: video_init() calls video_hw_init()
 *  7. mxsfb.c: video_hw_init() initializes framebuffer, clocks and lcdif
 *     controller, using the values stored in step 4 above.
 *  8. cfb_console.c: video_init() clears framebuffer and calls video_logo().
 *  9. cfb_console.c: video_logo() draws either the console logo and the welcome
 *     message, or if environment variable splashimage is set, the splash
 *     screen.
 * 10. cfb_console.c: drv_video_init() registers the console as stdio device.
 * 11. board_r.c: board_init_r() calls board_late_init().
 * 12. fsimx6sx.c: board_late_init() calls fs_board_set_backlight_all() to
 *     enable all active displays.
 */

/* Pads for 18-bit LCD interface */
static iomux_v3_cfg_t const lcd18_pads_low[] = {
	IOMUX_PADS(PAD_LCD1_ENABLE__GPIO3_IO_25 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_HSYNC__GPIO3_IO_26  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_VSYNC__GPIO3_IO_28  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_CLK__GPIO3_IO_0     | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA00__GPIO3_IO_1  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA01__GPIO3_IO_2  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA02__GPIO3_IO_3  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA03__GPIO3_IO_4  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA04__GPIO3_IO_5  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA05__GPIO3_IO_6  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA06__GPIO3_IO_7  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA07__GPIO3_IO_8  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA08__GPIO3_IO_9  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA09__GPIO3_IO_10 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA10__GPIO3_IO_11 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA11__GPIO3_IO_12 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA12__GPIO3_IO_13 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA13__GPIO3_IO_14 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA14__GPIO3_IO_15 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA15__GPIO3_IO_16 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA16__GPIO3_IO_17 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA17__GPIO3_IO_18 | MUX_PAD_CTRL(0x3010)),
};

static iomux_v3_cfg_t const lcd16_pads_low[] = {
	IOMUX_PADS(PAD_LCD1_ENABLE__GPIO3_IO_25 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_HSYNC__GPIO3_IO_26  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_VSYNC__GPIO3_IO_28  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_CLK__GPIO3_IO_0     | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA01__GPIO3_IO_2  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA02__GPIO3_IO_3  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA03__GPIO3_IO_4  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA04__GPIO3_IO_5  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA05__GPIO3_IO_6  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA06__GPIO3_IO_7  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA07__GPIO3_IO_8  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA08__GPIO3_IO_9  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA09__GPIO3_IO_10 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA10__GPIO3_IO_11 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA12__GPIO3_IO_13 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA13__GPIO3_IO_14 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA14__GPIO3_IO_15 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA15__GPIO3_IO_16 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA16__GPIO3_IO_17 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA17__GPIO3_IO_18 | MUX_PAD_CTRL(0x3010)),
};

static iomux_v3_cfg_t const lcd18_pads_active[] = {
	IOMUX_PADS(PAD_LCD1_ENABLE__LCDIF1_ENABLE | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_HSYNC__LCDIF1_HSYNC | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_VSYNC__LCDIF1_VSYNC | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_CLK__LCDIF1_CLK | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA00__LCDIF1_DATA_0  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA01__LCDIF1_DATA_1  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA02__LCDIF1_DATA_2  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA03__LCDIF1_DATA_3  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA04__LCDIF1_DATA_4  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA05__LCDIF1_DATA_5  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA06__LCDIF1_DATA_6  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA07__LCDIF1_DATA_7  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA08__LCDIF1_DATA_8  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA09__LCDIF1_DATA_9  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA10__LCDIF1_DATA_10 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA11__LCDIF1_DATA_11 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA12__LCDIF1_DATA_12 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA13__LCDIF1_DATA_13 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA14__LCDIF1_DATA_14 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA15__LCDIF1_DATA_15 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA16__LCDIF1_DATA_16 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA17__LCDIF1_DATA_17 | MUX_PAD_CTRL(LCD_CTRL)),
};

static iomux_v3_cfg_t const lcd16_pads_active[] = {
	IOMUX_PADS(PAD_LCD1_ENABLE__LCDIF1_ENABLE | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_HSYNC__LCDIF1_HSYNC | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_VSYNC__LCDIF1_VSYNC | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_CLK__LCDIF1_CLK | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA01__LCDIF1_DATA_1  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA02__LCDIF1_DATA_2  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA03__LCDIF1_DATA_3  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA04__LCDIF1_DATA_4  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA05__LCDIF1_DATA_5  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA06__LCDIF1_DATA_6  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA07__LCDIF1_DATA_7  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA08__LCDIF1_DATA_8  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA09__LCDIF1_DATA_9  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA10__LCDIF1_DATA_10 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA11__LCDIF1_DATA_11 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA13__LCDIF1_DATA_13 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA14__LCDIF1_DATA_14 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA15__LCDIF1_DATA_15 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA16__LCDIF1_DATA_16 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA17__LCDIF1_DATA_17 | MUX_PAD_CTRL(LCD_CTRL)),
};


static iomux_v3_cfg_t const lcd24_pads_low[] = {
	IOMUX_PADS(PAD_LCD1_DATA18__GPIO3_IO_19 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA19__GPIO3_IO_20 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA20__GPIO3_IO_21 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA21__GPIO3_IO_22 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA22__GPIO3_IO_23 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA23__GPIO3_IO_24 | MUX_PAD_CTRL(0x3010)),
};

static iomux_v3_cfg_t const lcd24_pads_active[] = {
	IOMUX_PADS(PAD_LCD1_DATA18__LCDIF1_DATA_18 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA19__LCDIF1_DATA_19 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA20__LCDIF1_DATA_20 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA21__LCDIF1_DATA_21 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA22__LCDIF1_DATA_22 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA23__LCDIF1_DATA_23 | MUX_PAD_CTRL(LCD_CTRL)),
};

/* Pads for VLCD_ON, VCFL_ON and BKLT_PWM */
static iomux_v3_cfg_t const lcd_extra_pads_efusa9x[] = {
	/* Signals are active high -> pull-down to switch off */
	IOMUX_PADS(PAD_LCD1_DATA18__GPIO3_IO_19 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA19__GPIO3_IO_20 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_DATA22__GPIO3_IO_23 | MUX_PAD_CTRL(0x3010)),
};
static iomux_v3_cfg_t const lcd_extra_pads[] = {
	/* Signals are active low -> pull-up to switch off */
	IOMUX_PADS(PAD_LCD1_RESET__GPIO3_IO_27 | MUX_PAD_CTRL(0xb010)),
	IOMUX_PADS(PAD_LCD1_DATA21__GPIO3_IO_22 | MUX_PAD_CTRL(0xb010)),
	IOMUX_PADS(PAD_LCD1_DATA22__GPIO3_IO_23 | MUX_PAD_CTRL(0xb010)),
	IOMUX_PADS(PAD_LCD1_DATA23__GPIO3_IO_24 | MUX_PAD_CTRL(0xb010)),
};

/* Pads for VLCD_ON: active high -> pull-down to switch off */
static iomux_v3_cfg_t const lcd_extra_pads_pcoremx6sx[] = {
	IOMUX_PADS(PAD_LCD1_RESET__GPIO3_IO_27 | MUX_PAD_CTRL(0x3010)),
};

/* Use I2C2 to talk to RGB adapter on efusA9X */
I2C_PADS(efusa9x,						\
	 PAD_QSPI1B_DATA3__I2C2_SCL | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 PAD_QSPI1B_DATA3__GPIO4_IO_27 |  MUX_PAD_CTRL(I2C_PAD_CTRL),\
	 IMX_GPIO_NR(4, 27),					\
	 PAD_QSPI1B_DATA2__I2C2_SDA | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 PAD_QSPI1B_DATA2__GPIO4_IO_26 | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 IMX_GPIO_NR(4, 26));

/* Use I2C2 to talk to RGB adapter on PicoCoreMX6SX */
I2C_PADS(pcoremx6sx,						\
	 PAD_GPIO1_IO02__I2C2_SCL | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 PAD_GPIO1_IO02__GPIO1_IO_2 |  MUX_PAD_CTRL(I2C_PAD_CTRL),\
	 IMX_GPIO_NR(1, 2),					\
	 PAD_GPIO1_IO03__I2C2_SDA | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 PAD_GPIO1_IO03__GPIO1_IO_3 | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 IMX_GPIO_NR(1, 3));

enum display_port_index {
	port_lcd, port_lvds
};

/* Define possible displays ports; LVDS ports may have additional settings */
#define FS_DISP_FLAGS_LVDS						\
	(FS_DISP_FLAGS_LVDS_24BPP | FS_DISP_FLAGS_LVDS_JEIDA		\
	 | FS_DISP_FLAGS_LVDS_BL_INV | FS_DISP_FLAGS_LVDS_VCFL_INV)

static const struct fs_display_port display_ports[CONFIG_FS_DISP_COUNT] = {
	[port_lcd] =  { "lcd",  0 },
	[port_lvds] = { "lvds", FS_DISP_FLAGS_LVDS },
};

static void setup_lcd_pads(int on)
{
	switch (fs_board_get_type())
	{
	case BT_EFUSA9X:		/* 18-bit LCD, power active high */
	case BT_EFUSA9XR2:
		if (on)
			SETUP_IOMUX_PADS(lcd18_pads_active);
		else
			SETUP_IOMUX_PADS(lcd18_pads_low);
		SETUP_IOMUX_PADS(lcd_extra_pads_efusa9x);
		break;

	case BT_PCOREMX6SX:		/* 24-bit LCD, power active high */
	case BT_PCOREMX6SXR2:
		if (on) {
			SETUP_IOMUX_PADS(lcd18_pads_active);
			SETUP_IOMUX_PADS(lcd24_pads_active);
		} else {
			SETUP_IOMUX_PADS(lcd18_pads_low);
			SETUP_IOMUX_PADS(lcd24_pads_low);
		}
        SETUP_IOMUX_PADS(lcd_extra_pads_pcoremx6sx);
        break;

		/* No break, fall through to case BT_PICOCOMA9X */
	case BT_PICOCOMA9X:		/* 16-bit LCD, power active low */
		if (on)
			SETUP_IOMUX_PADS(lcd16_pads_active);
		else
			SETUP_IOMUX_PADS(lcd16_pads_low);
		SETUP_IOMUX_PADS(lcd_extra_pads);
		break;

	case BT_CONT1:			/* Boards with no display at all */
	case BT_BEMA9X:
	default:
		break;
	}
}

/* Enable or disable display voltage VLCD; if LCD, also switch LCD pads */
void board_display_set_power(int port, int on)
{
	static unsigned int vlcd_users;
	unsigned int gpio;
	unsigned int value = on;

	switch (fs_board_get_type()) {
	case BT_EFUSA9X:		/* VLCD_ON is active high */
	case BT_EFUSA9XR2:
	case BT_BEMA9X:
	default:
		gpio = IMX_GPIO_NR(3, 19);
		break;

	case BT_PCOREMX6SX:		/* VLCD_ON is active high */
	case BT_PCOREMX6SXR2:
		gpio = IMX_GPIO_NR(3, 27);
		break;

	case BT_PICOCOMA9X:		/* VLCD_ON is active low */
		gpio = IMX_GPIO_NR(3, 27);
		value = !on;
		break;

	case BT_VAND3:
		return;
	}

	/* Switch on when first user enables and off when last user disables */
	if (!on) {
		vlcd_users &= ~(1 << port);
		if (port == port_lcd)
			setup_lcd_pads(0);
	}
	if (!vlcd_users) {
		gpio_direction_output(gpio, value);
		if (on)
			mdelay(1);
	}
	if (on) {
		vlcd_users |= (1 << port);
		if (port == port_lcd)
			setup_lcd_pads(1);
	}
}

/* Enable or disable backlight (incl. backlight voltage) for a display port */
void board_display_set_backlight(int port, int on)
{
	static int i2c_init;
	unsigned int board_type = fs_board_get_type();

	switch (port) {
	case port_lcd:
		switch (board_type) {
		case BT_EFUSA9X:
		case BT_EFUSA9XR2:
			if (!i2c_init) {
				setup_i2c(1, CONFIG_SYS_I2C_SPEED, 0x60,
					  I2C_PADS_INFO(efusa9x));
				i2c_init = 1;
			}
			fs_disp_set_i2c_backlight(1, on);
			break;
		case BT_PCOREMX6SX:
		case BT_PCOREMX6SXR2:
			if (!i2c_init) {
				setup_i2c(1, CONFIG_SYS_I2C_SPEED, 0x60,
					  I2C_PADS_INFO(pcoremx6sx));
				i2c_init = 1;
			}
			fs_disp_set_i2c_backlight(1, on);
			break;
		case BT_PICOCOMA9X:
			/* VBL_ONn */
			fs_disp_set_vcfl(port, !on, IMX_GPIO_NR(3, 24));
			/* LCD_DENn */
			fs_disp_set_vcfl(port, !on, IMX_GPIO_NR(3, 23));
			/* BL_CTRL */
			fs_disp_set_vcfl(port, !on, IMX_GPIO_NR(3, 22));
			break;
		case BT_BEMA9X:
		default:
			break;
		}
		break;

	case port_lvds:
		switch (board_type) {
		case BT_EFUSA9X:
		case BT_EFUSA9XR2:
			fs_disp_set_vcfl(port, on, IMX_GPIO_NR(3, 20));
			fs_disp_set_bklt_pwm(port, on, IMX_GPIO_NR(3, 23));
			break;
		default:
			break;
		}
		break;
	}
}

/* Activate LVDS channel */
static void config_lvds(uint32_t lcdif_base, unsigned int flags,
			const struct fb_videomode *mode)
{
	struct iomuxc *iomux = (struct iomuxc *)IOMUXC_BASE_ADDR;

	u32 gpr;
	u32 vs_polarity;
	u32 bit_mapping;
	u32 data_width;
	u32 enable;

	vs_polarity = IOMUXC_GPR2_DI0_VS_POLARITY_MASK;
	bit_mapping = IOMUXC_GPR2_BIT_MAPPING_CH0_JEIDA;
	data_width = IOMUXC_GPR2_DATA_WIDTH_CH0_MASK;
	enable = IOMUXC_GPR2_LVDS_CH0_MODE_ENABLED_DI0;

	gpr = readl(&iomux->gpr[6]);
	gpr &= ~(bit_mapping | data_width | vs_polarity);
	if (!(mode->sync & FB_SYNC_VERT_HIGH_ACT))
		gpr |= vs_polarity;
	if (flags & FS_DISP_FLAGS_LVDS_JEIDA)
		gpr |= bit_mapping;
	if (flags & FS_DISP_FLAGS_LVDS_24BPP)
		gpr |= data_width;
	gpr |= enable;
	writel(gpr, &iomux->gpr[6]);

	gpr = readl(&iomux->gpr[5]);
	if (lcdif_base == LCDIF1_BASE_ADDR)
		gpr &= ~0x8;		/* Use LCDIF1 for LVDS */
	else
		gpr |= 0x8;		/* Use LCDIF2 for LVDS */
	writel(gpr, &iomux->gpr[5]);
}

/* Set display clocks and register pixel format, resolution and timings */
int board_display_start(int port, unsigned flags, struct fb_videomode *mode)
{
	unsigned int freq_khz;
	int bpp = 18;

	/*
	 * Initialize display clock and register display settings with MXS
	 * display driver. The real initialization takes place later in
	 * function cfb_console.c: video_init().
	 */
	freq_khz = PICOS2KHZ(mode->pixclock);
	if (flags & FS_DISP_FLAGS_LVDS_24BPP)
		bpp = 24;

	switch (port) {
	case port_lcd:
		if ((fs_board_get_type() == BT_PCOREMX6SX)||(fs_board_get_type() == BT_PCOREMX6SXR2))
			bpp = 24;
		mxs_lcd_panel_setup(LCDIF1_BASE_ADDR, mode, bpp, PATTERN_RGB);
		mxs_config_lcdif_clk(LCDIF1_BASE_ADDR, freq_khz);
		mxs_enable_lcdif_clk(LCDIF1_BASE_ADDR);
		break;

	case port_lvds:
		mxs_lcd_panel_setup(LCDIF1_BASE_ADDR, mode, bpp, PATTERN_RGB);
		mxs_config_lvds_clk(LCDIF1_BASE_ADDR, freq_khz);
		config_lvds(LCDIF1_BASE_ADDR, flags, mode);
		enable_ldb_di_clk(0);	/* Always use ldb_di0 on MX6SX */
		mxs_enable_lcdif_clk(LCDIF1_BASE_ADDR);
		break;
	}

	return 0;
}

int board_video_skip(void)
{
	int default_port = port_lcd;
	unsigned int valid_mask;
	unsigned int board_type = fs_board_get_type();

	/* Determine possible displays and default port */
	switch (board_type) {
	case BT_EFUSA9X:
	case BT_EFUSA9XR2:
		valid_mask = (1 << port_lcd) | (1 << port_lvds);
		break;

	case BT_PCOREMX6SX:
	case BT_PCOREMX6SXR2:
	case BT_PICOCOMA9X:
	case BT_BEMA9X:
		valid_mask = (1 << port_lcd);
		break;

	default:
		return 1;		/* No displays */
	}

	/*
	 * Check if user has defined a display. If not, return 1, i.e. skip
	 * display activation. If yes, activate display.
	 */
	return fs_disp_register(display_ports, valid_mask, default_port);
}
#endif

#ifdef CONFIG_USB_EHCI_MX6
/*
 * USB Host support.
 *
 * USB0 is OTG1 port. By default this is used as device port. However on some
 * F&S boards this port may optionally be configured as a second host port. So
 * if environment variable usb0mode is set to "host" on these boards, or if it
 * is set to "otg" and the ID pin is low when usb is started, use host mode.
 *
 *    Board               USB_OTG1_PWR             USB_OTG1_ID
 *    ----------------------------------------------------------------------
 *    efusA9X             GPIO1_IO09(*)            GPIO1_IO10
 *    PicoCoreMX6SX       GPIO1_IO09(*)            GPIO1_IO10
 *    PicoCOMA9X          (only device mode possible)
 *    BemA9X              (only device mode possible)
 *    CONT1               (always on)              QSPI1A_DATA1 (GPIO4_IO17)
 *
 * (*) Signal on SKIT is active low, usually USB_OTG1_PWR is active high
 *
 * USB1 is OTG2 port that is only used as host port at F&S. It is used on all
 * boards. Some boards may have an additional USB hub with a reset signal
 * connected to this port.
 *
 *    Board               USB_OTG2 PWR             Hub Reset
 *    -------------------------------------------------------------------------
 *    efusA9X             GPIO1_IO12               (Hub on SKIT, no reset line)
 *    PicoCoreMX6SX       GPIO1_IO12               (no Hub)
 *    PicoCOMA9X          GPIO1_IO12               (no Hub)
 *    BemA9X              GPIO1_IO12               (no Hub)
 *    CONT1               (always on)              (no Hub)
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
 * USB_OTG1_PWR or USB_OTG2_PWR. Then the USB controller can switch power
 * automatically and we only have to tell the controller whether this signal is
 * active high or active low.
 *
 * If CONFIG_FS_USB_PWR_USBNC is set, the dedicated PWR function of the USB
 * controller will be used to switch host power (where available). Otherwise
 * the host power will be switched by using the pad as GPIO.
 */

/* Some boards have access to the USB_OTG1_ID pin to check host/device mode */
static iomux_v3_cfg_t const usb_otg1_id_pad_efusa9x[] = {
	IOMUX_PADS(PAD_GPIO1_IO10__GPIO1_IO_10 | MUX_PAD_CTRL(USB_ID_PAD_CTRL)),
};

static iomux_v3_cfg_t const usb_otg1_id_pad_cont1[] = {
	IOMUX_PADS(PAD_QSPI1A_DATA1__GPIO4_IO_17 | MUX_PAD_CTRL(USB_ID_PAD_CTRL)),
};

/* Some boards can switch the USB OTG power when configured as host */
static iomux_v3_cfg_t const usb_otg1_pwr_pad[] = {
#ifdef CONFIG_FS_USB_PWR_USBNC
	IOMUX_PADS(PAD_GPIO1_IO09__USB_OTG1_PWR | MUX_PAD_CTRL(NO_PAD_CTRL)),
#else
	IOMUX_PADS(PAD_GPIO1_IO09__GPIO1_IO_9 | MUX_PAD_CTRL(NO_PAD_CTRL)),
#endif
};

/* Some boards can switch the USB Host power (USB_OTG2_PWR) */
static iomux_v3_cfg_t const usb_otg2_pwr_pad[] = {
#ifdef CONFIG_FS_USB_PWR_USBNC
	IOMUX_PADS(PAD_GPIO1_IO12__USB_OTG2_PWR | MUX_PAD_CTRL(NO_PAD_CTRL)),
#else
	IOMUX_PADS(PAD_GPIO1_IO12__GPIO1_IO_12 | MUX_PAD_CTRL(NO_PAD_CTRL)),
#endif
};

static iomux_v3_cfg_t const usb_otg2_pwr_pad_vand3[] = {
#ifdef CONFIG_FS_USB_PWR_USBNC
	IOMUX_PADS(PAD_QSPI1A_SS0_B__USB_OTG2_PWR | MUX_PAD_CTRL(NO_PAD_CTRL)),
#else
	IOMUX_PADS(PAD_QSPI1A_SS0_B__GPIO4_IO_22 | MUX_PAD_CTRL(NO_PAD_CTRL)),
#endif
};

/* Init one USB port */
int board_ehci_hcd_init(int index)
{
	unsigned int board_type = fs_board_get_type();
	struct fs_usb_port_cfg cfg;

	if (index > 1)
		return 0;		/* Unknown port */

	/* Default settings, board specific code below will override */
	cfg.mode = FS_USB_OTG_DEVICE;
	cfg.pwr_pol = 0;
	cfg.pwr_pad = NULL;
	cfg.pwr_gpio = -1;
	cfg.id_pad = NULL;
	cfg.id_gpio = -1;
	cfg.reset_pad = NULL;
	cfg.reset_gpio = -1;

	/* Determine host power pad */
	if (index == 0) {		/* USB0: OTG1, DEVICE, optional HOST */
		switch (board_type) {
		/* These boards support optional HOST function on this port */
		case BT_EFUSA9X:	/* PWR active low, ID available */
		case BT_EFUSA9XR2:
		case BT_PCOREMX6SX:
		case BT_PCOREMX6SXR2:
			cfg.pwr_pol = 1;
			cfg.pwr_pad = usb_otg1_pwr_pad;
#ifndef CONFIG_FS_USB_PWR_USBNC
			cfg.pwr_gpio = IMX_GPIO_NR(1, 9);
#endif
			cfg.id_pad = usb_otg1_id_pad_efusa9x;
			cfg.id_gpio = IMX_GPIO_NR(1, 10);
			break;
		case BT_CONT1:		/* PWR always on, ID available */
			cfg.id_pad = usb_otg1_id_pad_cont1;
			cfg.id_gpio = IMX_GPIO_NR(4, 17);
			break;
		/* These boards have only HOST function on this port */
		case BT_VAND3:
			cfg.mode = FS_USB_HOST;
			break;

		/* These boards have only DEVICE function on this port */
		case BT_PICOCOMA9X:
		case BT_BEMA9X:
		default:
			cfg.mode = FS_USB_DEVICE;
			break;
		}
	} else {			/* USB1: OTG2, HOST only */
		cfg.mode = FS_USB_HOST;

		switch (board_type) {
		/* These boards can switch host power, PWR is active high */
		case BT_EFUSA9X:
		case BT_EFUSA9XR2:
		case BT_PCOREMX6SX:
		case BT_PCOREMX6SXR2:
		case BT_PICOCOMA9X:
		case BT_BEMA9X:
			cfg.pwr_pad = usb_otg2_pwr_pad;
#ifndef CONFIG_FS_USB_PWR_USBNC
			cfg.pwr_gpio = IMX_GPIO_NR(1, 12);
#endif
			break;
		case BT_VAND3:
			cfg.pwr_pad = usb_otg2_pwr_pad_vand3;
#ifndef CONFIG_FS_USB_PWR_USBNC
			cfg.pwr_gpio = IMX_GPIO_NR(4, 22);
#endif
			break;

		/* These boards can not switch host power, it is always on */
		case BT_CONT1:
		default:
			break;
		}
	}

        return fs_usb_set_port(index, &cfg);
}
#endif /* CONFIG_USB_EHCI_MX6 */

#ifdef CONFIG_BOARD_LATE_INIT
/*
 * Use this slot to init some final things before the network is started. The
 * F&S configuration heavily depends on this to set up the board specific
 * environment, i.e. environment variables that can't be defined as a constant
 * value at compile time.
 */
int board_late_init(void)
{
	/* Set up all board specific variables */
	fs_board_late_init_common("ttymxc");

#ifdef CONFIG_VIDEO_MXS
	/* Enable backlight for displays */
	fs_disp_set_backlight_all(1);
#endif

	return 0;
}
#endif /* CONFIG_BOARD_LATE_INIT */

#ifdef CONFIG_MXC_SPI
/* ETH switch SJA1105 is only available on CONT1 */
static iomux_v3_cfg_t const ecspi4_pads[] = {
	IOMUX_PADS(PAD_SD3_CLK__ECSPI4_SCLK | MUX_PAD_CTRL(SPI_PAD_CTRL)),
	IOMUX_PADS(PAD_SD3_DATA3__ECSPI4_MISO | MUX_PAD_CTRL(SPI_PAD_CTRL)),
	IOMUX_PADS(PAD_SD3_CMD__ECSPI4_MOSI | MUX_PAD_CTRL(SPI_PAD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA03__GPIO3_IO_4 | MUX_PAD_CTRL(SPI_CS_PAD_CTRL)),
};

/* SJA1105 configuration data for RGMII mode */
static u32 sja1105_data[] = {
	/* Device ID (start of configuration data */
	0x9E00030E,

	/* L2 Policing Table */
	0x06000000, 0x00000050,	       /* ID 0x06, Length 40*2 words = 0x50 */
	0x216F256B,		       /* CRC for Header */
	0xFEF70000, 0x03FFFFFF,	       /* Data, 40 entries (64 bits) */
	0xFEF70000, 0x03FFFFFF,
	0xFEF70000, 0x03FFFFFF,
	0xFEF70000, 0x03FFFFFF,
	0xFEF70000, 0x03FFFFFF,
	0xFEF70000, 0x03FFFFFF,
	0xFEF70000, 0x03FFFFFF,
	0xFEF70000, 0x03FFFFFF,
	0xFEF70000, 0x07FFFFFF,
	0xFEF70000, 0x07FFFFFF,
	0xFEF70000, 0x07FFFFFF,
	0xFEF70000, 0x07FFFFFF,
	0xFEF70000, 0x07FFFFFF,
	0xFEF70000, 0x07FFFFFF,
	0xFEF70000, 0x07FFFFFF,
	0xFEF70000, 0x07FFFFFF,
	0xFEF70000, 0x0BFFFFFF,
	0xFEF70000, 0x0BFFFFFF,
	0xFEF70000, 0x0BFFFFFF,
	0xFEF70000, 0x0BFFFFFF,
	0xFEF70000, 0x0BFFFFFF,
	0xFEF70000, 0x0BFFFFFF,
	0xFEF70000, 0x0BFFFFFF,
	0xFEF70000, 0x0BFFFFFF,
	0xFEF70000, 0x0FFFFFFF,
	0xFEF70000, 0x0FFFFFFF,
	0xFEF70000, 0x0FFFFFFF,
	0xFEF70000, 0x0FFFFFFF,
	0xFEF70000, 0x0FFFFFFF,
	0xFEF70000, 0x0FFFFFFF,
	0xFEF70000, 0x0FFFFFFF,
	0xFEF70000, 0x0FFFFFFF,
	0xFEF70000, 0x13FFFFFF,
	0xFEF70000, 0x13FFFFFF,
	0xFEF70000, 0x13FFFFFF,
	0xFEF70000, 0x13FFFFFF,
	0xFEF70000, 0x13FFFFFF,
	0xFEF70000, 0x13FFFFFF,
	0xFEF70000, 0x13FFFFFF,
	0xFEF70000, 0x13FFFFFF,
	0xFA2E19F8,			/* Checksum for data */

	/* VLAN Lookup Table */
	0x07000000, 0x00000004,		/* ID 0x07, Length 2*2 words = 0x04 */
	0x5860942E,			/* Checksum for header */
	0x00000000, 0x003FF000,		/* Data: 2 entries (64 bits) */
	0x08000000, 0x003FFF80,
	0xC7EF16A7,			/* Checksum for data */

	/* L2 Forwarding Table */
	0x08000000, 0x0000001A,		/* ID 0x08, Length 5*2+8*2 = 0x1A */
	0x6AF62353,			/* Checksum for header */
	0x10000000, 0xF7BDF58D,		/* Data Part 1: 5 entries (64 bits) */
	0x10000000, 0xEF7BF58D,
	0x10000000, 0xDEF7F58D,
	0x10000000, 0xBDEFF58D,
	0x10000000, 0x7BDFF58D,
	0x00000000, 0x00000000,		/* Data Part 2: 8 entries (64 bits) */
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0x00000000, 0x00000000,
	0xC004A606,			/* Checksum for data */

	/* MAC Configuration Table */
	0x09000000, 0x00000023,		/* ID 0x09, Length 5*7 words = 0x23 */
	0xDAB5BDC8,			/* Checksum for header */
	0x0000000E, 0x00000000,		/* Data: 5 entries (224 bits) */
	0x07FC0100, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x0000000E, 0x00000000,
	0x07FC0100, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x0000000E, 0x00000000,
	0x07FC0100, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x0000000E, 0x00000000,
	0x07FC0100, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x0000000E, 0x00000000,
	0x07FC0100, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0xC6CE396D,			/* Checksum for data */

	/* L2 Lookup Parameters Table */
	0x0D000000, 0x00000001,		/* ID 0x0D, Length 1*1 word = 0x01 */
	0x250E7CBD,			/* Checksum for header */
	0x000125C0,			/* Data: 1 entry (32 bits) */
	0x70948450,			/* Checksum for data */

	/* L2 Forwarding Parameters Table */
	0x0E000000, 0x00000003,		/* ID 0x0E, Length 1*3 words = 0x03 */
	0xC8A7CEE6,			/* Checksum for header */
	0x0071C000, 0x00000000, 0x00000000, /* Data: 1 entry (96 bits) */
	0xC3F704B9,			/* Checksum for data */

	/* General Parameters */
	0x11000000, 0x0000000A,	       /* ID 0x11, Length 1*10 words = 0x0A */
	0x571F813F,		       /* Checksum for header */
	0x06440000, 0x00000408,	       /* Data: 1 entry (320 bits) */
	0x00000000, 0xFF0CE000, 0xFFFFFFFF, 0xFFFFFFFF,
	0x00FFFFFF, 0x00000000, 0x00000000, 0x58000000,
	0x415691F7,			/* Checksum for data */

	/* xMII Mode Parameters */
	0x4E000000, 0x00000001,		/* ID 0x4E, Length 1*1 word = 0x01 */
	0x3A5D5E24,			/* Checksum for header */
	0x4B6C0000,			/* Data: 1 entry (32 bits) */
	0x090263AF,			/* Checksum for data */

	/* End of configuration */
	0x00000000, 0x00000000,		/* ID ignored, Length 0 */
	0xDF65AA65			/* CRC for all above config data */
};

/* Predefined speeds for the switch ports */
static unsigned int sja1105_port_speeds[] = {
	_1000BASET,			/* RJ45 next to USB */
	_1000BASET,			/* RJ45 next to power */
	_1000BASET,			/* B2B connector */
	_1000BASET,			/* i.MX6SX FEC0 */
	_1000BASET			/* i.MX6SX FEC1 */
};

/* Read a 32 bit value from SJA1105 address */
static int sja1105_read_val(struct spi_slave *slave, unsigned addr, u32 *val)
{
	u8 din[8];
	u8 dout[8];
	int ret;

	/*
	 * Prepare read command for one word at given address
	 *
	 * Read access:
	 *   Control word (binary):
	 *     (bit 31) 0RRR RRRA AAAA AAAA AAAA AAAA AAAA AAAA 0000 (bit 0)
	 *                      R: Number of words to read (0 = 64 words)
	 *                      A: Address where to read from
	 *   Data phase: R data cycles with received data, in our case is R=1
	 */
	addr &= 0x1FFFFF;
	dout[0] = 0x02 | (addr >> 20);
	dout[1] = addr >> 12;
	dout[2] = addr >> 4;
	dout[3] = addr << 4;

	memset(din, 0, sizeof(din));

	/* Transfer command and one word, i.e. 64 bits */
	ret = spi_xfer(slave, 64, dout, din, SPI_XFER_ONCE);
	if (ret) {
		printf("SJA1105: Error %d reading value at 0x%x\n", ret, addr);
		return ret;
	}

	*val = (din[4] << 24) | (din[5] << 16) | (din[6] << 8) | din[7];

	return 0;
}

/* Write a 32 bit value to SJA1105 address */
static int sja1105_write_val(struct spi_slave *slave, unsigned addr, u32 val)
{
	u8 dout[8];
	int ret;

	/*
	 * Prepare write command for one word at given address
	 *
	 * Write access:
	 *   Control word (binary):
	 *     (bit 31)  1000 000A AAAA AAAA AAAA AAAA AAAA AAAA 0000 (bit 0)
	 *                       A: Address where to write to
	 *   Data phase: 1 to 64 data cycles with data to send, in our case 1
	 */
	addr &= 0x1FFFFF;
	dout[0] = 0x80 | (addr >> 20);
	dout[1] = addr >> 12;
	dout[2] = addr >> 4;
	dout[3] = addr << 4;
	dout[4] = val >> 24;
	dout[5] = val >> 16;
	dout[6] = val >> 8;
	dout[7] = val;

	/* Transfer command and one word, i.e. 64 bits */
	ret = spi_xfer(slave, 64, dout, NULL, SPI_XFER_ONCE);
	if (ret)
		printf("SJA1105: Error %d writing value at 0x%x\n", ret, addr);

	return ret;
}

/* Parse SJA1105 config data and fix CRC values if necessary */
static int sja1105_parse_config(void)
{
	unsigned int index = 1;
	unsigned int id, size, cs, crc_index;

	while (1) {
		/* We need at least 3 more words for the end entry */
		if (index + 3 > ARRAY_SIZE(sja1105_data)) {
			printf("SJA1105: Missing config end entry\n");
			return -EINVAL;
		}
		id = sja1105_data[index] >> 24;
		size = sja1105_data[index + 1] & 0x00FFFFFF;

		/* We found end entry if size is 0 */
		if (!size)
			break;

		/* Do we have enough data words? */
		if (index + size + 4 > ARRAY_SIZE(sja1105_data)) {
			printf("SJA1105: Missing data in block ID 0x%02x"
			       " at index %d\n", id, index);
			return -EINVAL;
		}

		/* Check block header CRC */
		cs = crc32(0, (const u8 *)&sja1105_data[index], 8);
		crc_index = index + 2;
		if (cs != sja1105_data[crc_index]) {
			printf("SJA1105: Fixing bad header CRC32 in"
			       " block ID 0x%02x at index %d\n"
			       "         Found 0x%08x, should be 0x%08x\n",
			       id, crc_index, sja1105_data[crc_index], cs);
			sja1105_data[crc_index] = cs;
		}

		/* Check block data CRC */
		cs = crc32(0, (const u8 *)&sja1105_data[index + 3], size * 4);
		crc_index = index + size + 3;
		if (cs != sja1105_data[crc_index]) {
			printf("SJA1105: Fixing bad data CRC32 in"
			       " block ID 0x%02x at index %d\n"
			       "         Found 0x%08x, should be 0x%08x\n",
			       id, crc_index, sja1105_data[crc_index], cs);
			sja1105_data[crc_index] = cs;
		}
		index += size + 4;
	}

	/* Check global CRC */
	crc_index = index + 2;
	cs = crc32(0, (const u8 *)&sja1105_data[0], crc_index * 4);
	if (cs != sja1105_data[crc_index]) {
		printf("SJA1105: Fixing bad global CRC32 at index %d\n"
		       "         Found 0x%08x, should be 0x%08x\n",
		       crc_index, sja1105_data[crc_index], cs);
		sja1105_data[crc_index] = cs;
	}

	/* Check for useless data at end of array */
	index += 3;
	if (index < ARRAY_SIZE(sja1105_data)) {
		printf("SJA1105: Ignoring extra config data at index %d\n",
		       index);
	}

	return 0;
}

/* Set speed of a switch port */
static int sja1105_set_speed(struct spi_slave *slave, unsigned int port,
			     unsigned int speed)
{
	u32 idiv_n_c;
	u32 clksrc;
	u32 mac_config;
	int ret;

	if (port > 4)
		return -EINVAL;
	if (speed == _1000BASET) {
		idiv_n_c = 0x0a000001;		/* IDIV power down */
		clksrc = 0x0b;;			/* CLKSRC: PLL1 (125 MHz) */
		mac_config = 0xa01c0000;	/* 1000 MBit/s */
	} else if (speed == _100BASET) {
		idiv_n_c = 0x0a000000;		/* IDIV divide by 1 */
		clksrc = 0x11 + port;		/* CLKSRC: IDIVn (25 MHz) */
		mac_config = 0xc01c0000;	/* 100 MBit/s */
	} else if (speed == _10BASET) {
		idiv_n_c = 0x0a000024;		/* IDIV divide by 10 */
		clksrc = 0x11 + port;		/* CLKSRC: IDIVn (2.5 MHz) */
		mac_config = 0xe01c0000;	/* 10 MBit/s */
	} else
		return -EINVAL;

	idiv_n_c |= 0x00000800;			/* AUTOBLOCK */
	clksrc <<= 24;				/* Shift to CLKSRC position */
	clksrc |= 0x00000800;			/* AUTOBLOCK */
	mac_config |= port << 24;

	/* Write IDIV_n_C, MIIn_RGMII_TX_CLK and MAC reconfig register */
	ret = sja1105_write_val(slave, 0x10000b + port, idiv_n_c);
	if (!ret)
		ret = sja1105_write_val(slave, 0x100016 + 7*port, clksrc);
	if (!ret)
		ret = sja1105_write_val(slave, 0x37, mac_config);

	return ret;
}

/* Send configuration data and configure ports */
static int sja1105_configure(struct spi_slave *slave)
{
	int ret;
	unsigned int offs, size, chunksize, addr, i;
	static u8 dout[65*4];		/* Max. 65 entries with 32 bits */
	u32 val;

	/* Read and check the device ID */
	ret = sja1105_read_val(slave, 0, &val);
	if (ret)
		return ret;
	if (val != sja1105_data[0]) {
		printf("SJA1105: Bad ID, expected 0x%08x, found 0x%08x\n",
		       sja1105_data[0], val);
		return -EINVAL;
	}

	/*
	 * Send configuration data to SJA1105. The transfer must be split in
	 * chunks with at most 64 words of data.
	 *
	 * Write access:
	 *   Control word (binary):
	 *     (bit 31)  1000 000A AAAA AAAA AAAA AAAA AAAA AAAA 0000 (bit 0)
	 *                       A: Address where to write to
	 *   Data phase: 1 to 64 data cycles with data to send
	 *
	 * The configuration area begins at address 0x20000 and is write-only.
	 * Reading data in this region returns arbitrary data.
	 */
	addr = 0x20000;
	offs = 0;
	size = ARRAY_SIZE(sja1105_data);
	do {
		chunksize = size;
		if (chunksize > 64)
			chunksize = 64;
		dout[0] = 0x80 | (addr >> 20);
		dout[1] = addr >> 12;
		dout[2] = addr >> 4;
		dout[3] = addr << 4;
		for (i = 0; i < chunksize; i++) {
			val = sja1105_data[offs++];
			dout[i*4 + 4] = val >> 24;
			dout[i*4 + 5] = val >> 16;
			dout[i*4 + 6] = val >> 8;
			dout[i*4 + 7] = val;
		}
		ret = spi_xfer(slave, chunksize * 32 + 32, dout, NULL,
			       SPI_XFER_ONCE);
		if (ret) {
			printf("SJA1105: Error %d when sending config data"
			       " to 0x%x\n", ret, addr);
			return ret;
		}
		addr += chunksize;
		size -= chunksize;
	} while (size);

	/* Read configuration flag register */
	ret = sja1105_read_val(slave, 1, &val);
	if (ret)
		return ret;

	if (!(val & 0x80000000)) {
		printf("SJA1105: Device did not accept configuration data\n"
		       "         Configuration Flag Register: 0x%08x\n", val);
		return -EINVAL;
	}

	for (i = 0; i < 5; i++) {
		ret = sja1105_set_speed(slave, i, sja1105_port_speeds[i]);
		if (ret)
			break;
	}

	return ret;
}

int board_spi_cs_gpio(unsigned bus, unsigned cs)
{
	return (bus == 3 && cs == 0) ? (IMX_GPIO_NR(3, 4)) : -1;
}

/* Start ECSPI4 and configure SJA1105 ethernet switch */
static int sja1105_init(void)
{
	int ret;
	u32 reg;
	struct spi_slave *slave;
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	//###struct cspi_regs *regs = (struct cspi_regs *) ECSPI4_BASE_ADDR;

	/* Check stucture of SJA1105 config data, fix checksums if necessary */
	ret = sja1105_parse_config();
	if (ret)
		return ret;

	/* Setup iomux for ECSPI4 */
	SETUP_IOMUX_PADS(ecspi4_pads);

	/* Enable ECSPI4 clock */
	reg = readl(&mxc_ccm->CCGR1);
	reg |= (3 << 6);
	writel(reg, &mxc_ccm->CCGR1);

#if 0 //### Should not be necessary
	/* Clear EN bit in conreg */
	reg = readl(&regs->ctrl);
	reg &= ~(1 << 0);
	writel(reg, &regs->ctrl);
#endif

	/* ECSPI4 has index 3, use 10 MHz, SPI mode 1, CS on GPIO3_IO04 */
	slave = spi_setup_slave(3, 0, 10000000, SPI_MODE_1);
	if (!slave)
		return -EINVAL;

	/* Claim SPI bus and actually configure SJA1105 */
	ret = spi_claim_bus(slave);
	if (!ret)
		ret = sja1105_configure(slave);

	spi_release_bus(slave);

#if 0 //### This is done in spi_release_bus() now
	/* Clear EN bit in conreg */
	reg = readl(&regs->ctrl);
	reg &= ~(1 << 0);
	writel(reg, &regs->ctrl);
#endif

	/* Disable ECSPI4 clock */
	reg = readl(&mxc_ccm->CCGR1);
	reg &= ~(3 << 6);
	writel(reg, &mxc_ccm->CCGR1);

	return ret;
}
#endif /* CONFIG_MXC_SPI */

#ifdef CONFIG_CMD_NET
/* enet pads definition */
static iomux_v3_cfg_t const enet_pads_control_efusa9x[] = {
	/* MDIO; on efusA9X both PHYs are on ENET1_MDIO bus  */
	IOMUX_PADS(PAD_ENET2_CRS__ENET1_MDIO | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET2_COL__ENET1_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL)),

	/* 25MHz base clock from CPU to both PHYs */
	IOMUX_PADS(PAD_ENET2_RX_CLK__ENET2_REF_CLK_25M | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL)),

	/* PHY interrupt; on efuA9X this is shared on both PHYs */
	IOMUX_PADS(PAD_ENET2_TX_CLK__GPIO2_IO_9 | MUX_PAD_CTRL(NO_PAD_CTRL)),

	/* Reset signal for both PHYs */
	IOMUX_PADS(PAD_ENET1_MDC__GPIO2_IO_2 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const enet_pads_control_cont1[] = {
	/* MDIO; on CONT1 all external PHYs are on ENET1_MDIO bus */
	IOMUX_PADS(PAD_ENET1_MDIO__ENET1_MDIO | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET1_MDC__ENET1_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL)),

	/* PHY interrupt lines, one for each external PHY */
	IOMUX_PADS(PAD_LCD1_DATA11__GPIO3_IO_12 | MUX_PAD_CTRL(NO_PAD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA13__GPIO3_IO_14 | MUX_PAD_CTRL(NO_PAD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA15__GPIO3_IO_16 | MUX_PAD_CTRL(NO_PAD_CTRL)),

	/* Reset signals, one for each external PHY */
	IOMUX_PADS(PAD_LCD1_DATA10__GPIO3_IO_11 | MUX_PAD_CTRL(NO_PAD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA12__GPIO3_IO_13 | MUX_PAD_CTRL(NO_PAD_CTRL)),
	IOMUX_PADS(PAD_LCD1_DATA14__GPIO3_IO_15 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const enet_pads_rgmii1[] = {
	/* FEC0 (ENET1) */
	IOMUX_PADS(PAD_RGMII1_TXC__ENET1_RGMII_TXC | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_TD0__ENET1_TX_DATA_0 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_TD1__ENET1_TX_DATA_1 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_TD2__ENET1_TX_DATA_2 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_TD3__ENET1_TX_DATA_3 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_TX_CTL__ENET1_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RXC__ENET1_RX_CLK | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RD0__ENET1_RX_DATA_0 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RD1__ENET1_RX_DATA_1 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RD2__ENET1_RX_DATA_2 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RD3__ENET1_RX_DATA_3 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RX_CTL__ENET1_RX_EN | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
};

static iomux_v3_cfg_t const enet_pads_rgmii2[] = {
	/* FEC1 (ENET2) */
	IOMUX_PADS(PAD_RGMII2_TXC__ENET2_RGMII_TXC | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_TD0__ENET2_TX_DATA_0 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_TD1__ENET2_TX_DATA_1 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_TD2__ENET2_TX_DATA_2 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_TD3__ENET2_TX_DATA_3 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_TX_CTL__ENET2_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RXC__ENET2_RX_CLK | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RD0__ENET2_RX_DATA_0 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RD1__ENET2_RX_DATA_1 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RD2__ENET2_RX_DATA_2 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RD3__ENET2_RX_DATA_3 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RX_CTL__ENET2_RX_EN | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
};

static iomux_v3_cfg_t const enet_pads_rmii1[] = {
	/* MDIO */
	IOMUX_PADS(PAD_ENET1_MDIO__ENET1_MDIO | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET1_MDC__ENET1_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL)),

	/* 50MHz base clock from CPU to PHY */
	IOMUX_PADS(PAD_GPIO1_IO05__ENET1_REF_CLK1 | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL)),

	/* FEC0 (ENET1) */
	IOMUX_PADS(PAD_RGMII1_TD0__ENET1_TX_DATA_0 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_TD1__ENET1_TX_DATA_1 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_TX_CTL__ENET1_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RXC__ENET1_RX_ER | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RD0__ENET1_RX_DATA_0 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RD1__ENET1_RX_DATA_1 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RX_CTL__ENET1_RX_EN | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),

	/* No interrupt on DP83848 PHY */

	/* Reset signal for PHY */
	IOMUX_PADS(PAD_ENET2_CRS__GPIO2_IO_7 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const enet_pads_rmii2[] = {
	/* MDIO */
	IOMUX_PADS(PAD_ENET1_CRS__ENET2_MDIO | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET1_COL__ENET2_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL)),

	/* 50MHz base clock from CPU to PHY */
	IOMUX_PADS(PAD_ENET2_TX_CLK__ENET2_REF_CLK2 | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL)),

	/* FEC1 (ENET2) */
	IOMUX_PADS(PAD_RGMII2_TD0__ENET2_TX_DATA_0 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_TD1__ENET2_TX_DATA_1 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_TX_CTL__ENET2_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RXC__ENET2_RX_ER | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RD0__ENET2_RX_DATA_0 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RD1__ENET2_RX_DATA_1 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RX_CTL__ENET2_RX_EN | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),

	/* No interrupt on DP83848 PHY */

	/* Reset signal for PHY */
	IOMUX_PADS(PAD_ENET2_CRS__GPIO2_IO_7 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const enet_pads_rmii1_vand3[] = {
	/* MDIO */
	IOMUX_PADS(PAD_ENET1_MDIO__ENET1_MDIO | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET1_MDC__ENET1_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL)),

	/* 50MHz base clock from CPU to PHY */
	IOMUX_PADS(PAD_ENET1_TX_CLK__ENET1_REF_CLK1 | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL)),

	/* FEC0 (ENET1) */
	IOMUX_PADS(PAD_RGMII1_TD0__ENET1_TX_DATA_0 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_TD1__ENET1_TX_DATA_1 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_TX_CTL__ENET1_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RXC__ENET1_RX_ER | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RD0__ENET1_RX_DATA_0 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RD1__ENET1_RX_DATA_1 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RX_CTL__ENET1_RX_EN | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),

	/* Interrupt signal for PHY */
	IOMUX_PADS(PAD_RGMII1_TD3__GPIO5_IO_9 | MUX_PAD_CTRL(NO_PAD_CTRL)),

	/* Reset signal for PHY */
	IOMUX_PADS(PAD_RGMII1_TD2__GPIO5_IO_8 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const enet_pads_rmii2_vand3[] = {
	/* MDIO */
	IOMUX_PADS(PAD_ENET1_CRS__ENET2_MDIO | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET1_COL__ENET2_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL)),

	/* 50MHz base clock from CPU to PHY */
	IOMUX_PADS(PAD_ENET2_TX_CLK__ENET2_REF_CLK2 | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL)),

	/* FEC1 (ENET2) */
	IOMUX_PADS(PAD_RGMII2_TD0__ENET2_TX_DATA_0 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_TD1__ENET2_TX_DATA_1 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_TX_CTL__ENET2_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RXC__ENET2_RX_ER | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RD0__ENET2_RX_DATA_0 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RD1__ENET2_RX_DATA_1 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RX_CTL__ENET2_RX_EN | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),

	/* Interrupt signal for PHY */
	IOMUX_PADS(PAD_RGMII2_TD3__GPIO5_IO_21 | MUX_PAD_CTRL(NO_PAD_CTRL)),

	/* Reset signal for PHY */
	IOMUX_PADS(PAD_RGMII2_TD2__GPIO5_IO_20 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};


/*
 * Allocate MII bus (if appropriate), find PHY and probe FEC. Besides the
 * error code as return value, also return pointer to new MII bus in *pbus.
 *
 * Remarks:
 * - If pbus == NULL, assume a PHY-less connection (no MII bus at all)
 * - If *pbus == NULL, allocate new MII bus before looking for PHY
 * - Otherwise use MII bus that is given in *pbus.
 */
int setup_fec(struct bd_info *bd, uint32_t base_addr, int eth_id,
	      enum xceiver_type xcv_type, struct mii_dev **pbus,
	      int bus_id, int phy_addr, phy_interface_t interface)
{
	struct phy_device *phydev = NULL;
	struct mii_dev *bus = NULL;
	int ret;

	fs_eth_set_ethaddr(eth_id);

	/*
	 * We can not use fecmxc_initialize_multi_type() because this would
	 * allocate one MII bus for each ethernet device. We have different
	 * configurations on some boards, e.g. only one MII bus for several
	 * PHYs. So the following code works rather similar to the code in
	 * fecmxc_initialize_multi_type(), but handles the MII bus separately
	 * from the FEC device.
	 */
	if (pbus) {
		bus = *pbus;

		/* Allocate MII bus, if we do not have one already */
		if (!bus) {
			bus = fec_get_miibus(base_addr, bus_id);
			if (!bus)
				return -ENOMEM;
		}

		/* Find PHY on the bus */
		phydev = phy_find_by_mask(bus, 1 << phy_addr, interface);
		if (!phydev) {
			free(bus);
			return -ENOMEM;
		}
	}

	ret = fec_probe(bd, eth_id, base_addr, bus, phydev, xcv_type);
	if (ret) {
		if (phydev)
			free(phydev);
		if (bus)
			free(bus);
		return ret;
	}

	if (pbus)
		*pbus = bus;

	return 0;
}

int board_eth_init(struct bd_info *bd)
{
	u32 gpr1;
	int ret = 0;
	struct iomuxc *iomux_regs = (struct iomuxc *)IOMUXC_BASE_ADDR;
	struct mii_dev *bus = NULL;
	unsigned int board_type = fs_board_get_type();
	unsigned int features2 = fs_board_get_nboot_args()->chFeatures2;
	int eth_id = 0;

	/*
	 * Set IOMUX for ports, enable clocks and reset PHYs. On i.MX6 SoloX,
	 * the ENET clock is ungated in enable_fec_anatop_clock().
	 */
	switch (board_type) {
	case BT_EFUSA9X:
	case BT_PCOREMX6SX:
		/* The 25 MHz reference clock is generated in the CPU and is an
		   output on pad ENET2_RX_CLK, i.e. CONFIG_FEC_MXC_25M_REF_CLK
		   must be set */
		if (features2 & FEAT2_ETH_A) {
			/* IOMUX for ENET1, use 1 GBit/s LAN on RGMII pins */
			SETUP_IOMUX_PADS(enet_pads_rgmii1);
			SETUP_IOMUX_PADS(enet_pads_control_efusa9x);

			/* ENET1_REF_CLK_25M output is not used, ENET1_TX_CLK
			   (125MHz) is generated in PHY and is an input */
			gpr1 = readl(&iomux_regs->gpr[1]);
			gpr1 &= ~IOMUX_GPR1_FEC1_MASK;
			writel(gpr1, &iomux_regs->gpr[1]);

			/* Activate ENET1 (FEC0) PLL */
			ret = enable_fec_anatop_clock(0, ENET_125MHZ);
			if (ret < 0)
				return ret;
		}

		if (features2 & FEAT2_ETH_B) {
			/* IOMUX for ENET2, use 1 GBit/s LAN on RGMII pins */
			SETUP_IOMUX_PADS(enet_pads_rgmii2);

			/* ENET2_REF_CLK_25M is an output, ENET2_TX_CLK
			   (125MHz) is generated in PHY and is an input */
			gpr1 = readl(&iomux_regs->gpr[1]);
			gpr1 &= ~IOMUX_GPR1_FEC2_MASK;
			writel(gpr1, &iomux_regs->gpr[1]);

			/* Activate ENET2 (FEC1) PLL */
			ret = enable_fec_anatop_clock(1, ENET_125MHZ);
			if (ret < 0)
				return ret;
		}

		/* Reset both PHYs, Atheros AR8035 needs at least 1ms after
		   clock is enabled */
		fs_board_issue_reset(1000, 1000, IMX_GPIO_NR(2, 2), ~0, ~0);

		/* Probe FEC ports, both PHYs on one MII bus */
		if (features2 & FEAT2_ETH_A)
			ret = setup_fec(bd, ENET_BASE_ADDR, eth_id++, RGMII,
					&bus, -1, 4,
					PHY_INTERFACE_MODE_RGMII_ID);
		if (!ret && (features2 & FEAT2_ETH_B))
			ret = setup_fec(bd, ENET2_BASE_ADDR, eth_id++, RGMII,
					&bus, -1, 5,
					PHY_INTERFACE_MODE_RGMII_ID);
		break;

	case BT_PICOCOMA9X:
	case BT_BEMA9X:
		if (features2 & FEAT2_ETH_A) {
			/* IOMUX for ENET1, use 100 MBit/s LAN on RGMII1 pins */
			SETUP_IOMUX_PADS(enet_pads_rmii1);

			/*
			 * ENET1 (FEC0) CLK is generated in CPU and is an
			 * output. Please note that the clock pin must have
			 * the SION flag set to feed back the clock to the
			 * internal MAC. This means we also have to set both
			 * ENET mux bits in gpr1. Bit 17 to generate the REF
			 * clock on the pin and bit 13 to get the generated
			 * clock from the PAD back into the MAC.
			 */
			gpr1 = readl(&iomux_regs->gpr[1]);
			gpr1 |= IOMUX_GPR1_FEC1_MASK;
			writel(gpr1, &iomux_regs->gpr[1]);

			/* Activate ENET1 (FEC0) PLL */
			ret = enable_fec_anatop_clock(0, ENET_50MHZ);
			if (ret < 0)
				return ret;
		}

		if (features2 & FEAT2_ETH_B) {
			/* IOMUX for ENET2, use 100 MBit/s LAN on RGMII2 pins */
			SETUP_IOMUX_PADS(enet_pads_rmii2);

			/*
			 * ENET2 (FEC1) CLK is generated in CPU and is an
			 * output. Please note that the clock pin must have
			 * the SION flag set to feed back the clock to the
			 * internal MAC. This means we also have to set both
			 * ENET mux bits in gpr1. Bit 18 to generate the REF
			 * clock on the pin and bit 14 to get the generated
			 * clock from the PAD back into the MAC.
			 */
			gpr1 = readl(&iomux_regs->gpr[1]);
			gpr1 |= IOMUX_GPR1_FEC2_MASK;
			writel(gpr1, &iomux_regs->gpr[1]);

			/* Activate ENET2 (FEC1) PLL */
			ret = enable_fec_anatop_clock(1, ENET_50MHZ);
			if (ret < 0)
				return ret;
		}

		/*
		 * Reset both PHYs. DP83484 needs at least 1us reset pulse
		 * width. After power on it needs min 167ms (after reset is
		 * deasserted) before the first MDIO access can be done. In a
		 * warm start, it only takes around 3us for this. As we do not
		 * know whether this is a cold or warm start, we must assume
		 * the worst case.
		 */
		fs_board_issue_reset(10, 170000, IMX_GPIO_NR(2, 7), ~0, ~0);

		/* Probe FEC ports, each PHY on its own MII bus */
		if (features2 & FEAT2_ETH_A)
			ret = setup_fec(bd, ENET_BASE_ADDR, eth_id++, RMII,
					&bus, 0, 1, PHY_INTERFACE_MODE_RMII);
		bus = NULL;
		if (!ret && (features2 & FEAT2_ETH_B))
			ret = setup_fec(bd, ENET2_BASE_ADDR, eth_id++, RMII,
					&bus, 1, 17, PHY_INTERFACE_MODE_RMII);
		break;

	case BT_VAND3:
		if (features2 & FEAT2_ETH_A) {
			/* IOMUX for ENET1, use 100 MBit/s LAN on RGMII1 pins */
			SETUP_IOMUX_PADS(enet_pads_rmii1_vand3);

			/*
			 * ENET1 (FEC0) CLK is generated in CPU and is an
			 * output. Please note that the clock pin must have
			 * the SION flag set to feed back the clock to the
			 * internal MAC. This means we also have to set both
			 * ENET mux bits in gpr1. Bit 17 to generate the REF
			 * clock on the pin and bit 13 to get the generated
			 * clock from the PAD back into the MAC.
			 */
			gpr1 = readl(&iomux_regs->gpr[1]);
			gpr1 |= IOMUX_GPR1_FEC1_MASK;
			writel(gpr1, &iomux_regs->gpr[1]);

			/* Activate ENET1 (FEC0) PLL */
			ret = enable_fec_anatop_clock(0, ENET_50MHZ);
			if (ret < 0)
				return ret;
		}

		if (features2 & FEAT2_ETH_B) {
			/* IOMUX for ENET2, use 100 MBit/s LAN on RGMII2 pins */
			SETUP_IOMUX_PADS(enet_pads_rmii2_vand3);

			/*
			 * ENET2 (FEC1) CLK is generated in CPU and is an
			 * output. Please note that the clock pin must have
			 * the SION flag set to feed back the clock to the
			 * internal MAC. This means we also have to set both
			 * ENET mux bits in gpr1. Bit 18 to generate the REF
			 * clock on the pin and bit 14 to get the generated
			 * clock from the PAD back into the MAC.
			 */
			gpr1 = readl(&iomux_regs->gpr[1]);
			gpr1 |= IOMUX_GPR1_FEC2_MASK;
			writel(gpr1, &iomux_regs->gpr[1]);

			/* Activate ENET2 (FEC1) PLL */
			ret = enable_fec_anatop_clock(1, ENET_50MHZ);
			if (ret < 0)
				return ret;
		}

		/*
		 * Up to two KSZ8081RNA PHYs: This PHY needs at least 500us
		 * reset pulse width and 100us delay before the first MDIO
		 * access can be done.
		 */
		fs_board_issue_reset(500, 100, IMX_GPIO_NR(5, 8), IMX_GPIO_NR(5, 20), ~0);

		/* Probe FEC ports, each PHY on its own MII bus */
		if (features2 & FEAT2_ETH_A)
			ret = setup_fec(bd, ENET_BASE_ADDR, eth_id++, RMII,
					&bus, 0, 0, PHY_INTERFACE_MODE_RMII);
		bus = NULL;
		if (!ret && (features2 & FEAT2_ETH_B))
			ret = setup_fec(bd, ENET2_BASE_ADDR, eth_id++, RMII,
					&bus, 1, 3, PHY_INTERFACE_MODE_RMII);
		break;

	case BT_CONT1:
		/*
		 * CONT1 uses a 5 port ethernet switch (SJA1105). Two ports
		 * are directly connected (MAC-MAC) to the RGMII ports of the
		 * i.MX6SX and do not need an MII bus. Port 3 goes to ENET1,
		 * port 4 to ENET2. The three other ports use an Atheros PHY
		 * each and go to the outside (ports 0+1 via RJ45 connectors,
		 * port 2 as part of the B2B connector). All three PHYs are on
		 * one MII bus (on ENET1). The 25 MHz reference clock for the
		 * SJA1105 is generated by an external crystal and is not
		 * visible and not required on the i.MX6SX.
		 */
		if (features2 & FEAT2_ETH_A) {
			/* IOMUX for ENET1, use 1 GBit/s LAN on RGMII pins */
			SETUP_IOMUX_PADS(enet_pads_rgmii1);
			SETUP_IOMUX_PADS(enet_pads_control_cont1);

			/* ENET1_TX_CLK (125MHz) is generated in SJA1105 and
			   is an input */
			gpr1 = readl(&iomux_regs->gpr[1]);
			gpr1 &= ~IOMUX_GPR1_FEC1_MASK;
			writel(gpr1, &iomux_regs->gpr[1]);

			/* Activate ENET1 (FEC0) PLL */
			ret = enable_fec_anatop_clock(0, ENET_125MHZ);
			if (ret < 0)
				return ret;
		}

		if (features2 & FEAT2_ETH_B) {
			/* IOMUX for ENET2, use 1 GBit/s LAN on RGMII pins */
			SETUP_IOMUX_PADS(enet_pads_rgmii2);

			/* ENET2_TX_CLK (125MHz) is generated in SJA1105 and
			   is an input */
			gpr1 = readl(&iomux_regs->gpr[1]);
			gpr1 &= ~IOMUX_GPR1_FEC2_MASK;
			writel(gpr1, &iomux_regs->gpr[1]);

			/* Activate ENET2 (FEC1) PLL */
			ret = enable_fec_anatop_clock(1, ENET_125MHZ);
			if (ret < 0)
				return 0;
		}

		/* Reset ext. PHYs, Atheros AR8035 needs at least 1ms */
		fs_board_issue_reset(1000, 1000, IMX_GPIO_NR(3, 11),
				     IMX_GPIO_NR(3, 13), IMX_GPIO_NR(3, 15));

		/* Probe FEC ports, direct connection no MII bus required */
		if (features2 & FEAT2_ETH_A)
			ret = setup_fec(bd, ENET_BASE_ADDR, eth_id++, RGMII,
					NULL, 0, 0, PHY_INTERFACE_MODE_NONE);
		bus = NULL;
		if (!ret && (features2 & FEAT2_ETH_B))
			ret = setup_fec(bd, ENET2_BASE_ADDR, eth_id++, RGMII,
					NULL, 0, 0, PHY_INTERFACE_MODE_NONE);
		if (!ret) {
			phy_interface_t interface = PHY_INTERFACE_MODE_RGMII;
			struct phy_device *phy3, *phy4, *phy5;

			bus = fec_get_miibus(ENET_BASE_ADDR, -1);
			if (!bus)
				return -ENOMEM;

			phy3 = phy_find_by_mask(bus, 1 << 4, interface);
			phy4 = phy_find_by_mask(bus, 1 << 5, interface);
			phy5 = phy_find_by_mask(bus, 1 << 6, interface);
			if (!phy3 || !phy4 || !phy5) {
				if (phy3)
					free(phy3);
				if (phy4)
					free(phy4);
				if (phy5)
					free(phy5);
				free(bus);
				ret = -ENOMEM;
			} else {
				phy_config(phy3);
				phy_config(phy4);
				phy_config(phy5);
			}
		}
#ifdef CONFIG_MXC_SPI
		if (!ret) {
			ret = sja1105_init();
			if (ret) {
				printf("SJA1105: Configuration failed"
				       " with error %d\n", ret);
			}
		}
#endif
		break;

	case BT_EFUSA9XR2:
	case BT_PCOREMX6SXR2:
		/* The 25 MHz reference clock is generated in the CPU and is an
		   output on pad ENET2_RX_CLK, i.e. CONFIG_FEC_MXC_25M_REF_CLK
		   must be set */
		if (features2 & FEAT2_ETH_A) {
			/* IOMUX for ENET1, use 1 GBit/s LAN on RGMII pins */
			SETUP_IOMUX_PADS(enet_pads_rgmii1);
			SETUP_IOMUX_PADS(enet_pads_control_efusa9x);

			/* ENET1_REF_CLK_25M output is not used, ENET1_TX_CLK
			   (125MHz) is generated in PHY and is an input */
			gpr1 = readl(&iomux_regs->gpr[1]);
			gpr1 &= ~IOMUX_GPR1_FEC1_MASK;
			writel(gpr1, &iomux_regs->gpr[1]);

			/* Activate ENET1 (FEC0) PLL */
			ret = enable_fec_anatop_clock(0, ENET_125MHZ);
			if (ret < 0)
				return ret;
		}

		if (features2 & FEAT2_ETH_B) {
			/* IOMUX for ENET2, use 1 GBit/s LAN on RGMII pins */
			SETUP_IOMUX_PADS(enet_pads_rgmii2);

			/* ENET2_REF_CLK_25M is an output, ENET2_TX_CLK
			   (125MHz) is generated in PHY and is an input */
			gpr1 = readl(&iomux_regs->gpr[1]);
			gpr1 &= ~IOMUX_GPR1_FEC2_MASK;
			writel(gpr1, &iomux_regs->gpr[1]);

			/* Activate ENET2 (FEC1) PLL */
			ret = enable_fec_anatop_clock(1, ENET_125MHZ);
			if (ret < 0)
				return ret;
		}

		/* Realtek RTL8211F(D): Assert reset for at least 10ms */
		fs_board_issue_reset(10000, 50000, IMX_GPIO_NR(2, 2), ~0, ~0);

		/* Probe FEC ports, both PHYs on one MII bus */
		if (features2 & FEAT2_ETH_A)
			ret = setup_fec(bd, ENET_BASE_ADDR, eth_id++, RGMII,
					&bus, -1, 4,
					PHY_INTERFACE_MODE_RGMII_ID);
		if (!ret && (features2 & FEAT2_ETH_B))
			ret = setup_fec(bd, ENET2_BASE_ADDR, eth_id++, RGMII,
					&bus, -1, 5,
					PHY_INTERFACE_MODE_RGMII_ID);
		break;

	default:
		return 0;
	}

	/* If WLAN is available, just set ethaddr variable */
	if (!ret && (features2 & FEAT2_WLAN))
		fs_eth_set_ethaddr(eth_id++);

	return ret;
}

#define MIIM_RTL8211F_PAGE_SELECT 0x1f
#define LED_MODE_B (1 << 15)
#define LED_LINK(X) (0x0b << (5*X))
#define LED_ACT(X) (0x10 << (5*X))

int board_phy_config(struct phy_device *phydev)
{
	unsigned int board_type = fs_board_get_type();
	unsigned int features2 = fs_board_get_nboot_args()->chFeatures2;

	if (phydev->drv->config)
		phydev->drv->config(phydev);

	/* Realtek needs special LED configuration */
	if (features2 & (FEAT2_ETH_A | FEAT2_ETH_B)) {
		switch (board_type) {
		case BT_EFUSA9XR2:
			phy_write(phydev, MDIO_DEVAD_NONE, MIIM_RTL8211F_PAGE_SELECT, 0xd04);
			phy_write(phydev, MDIO_DEVAD_NONE, 0x10, LED_MODE_B | LED_LINK(2) | LED_ACT(1));
			phy_write(phydev, MDIO_DEVAD_NONE, MIIM_RTL8211F_PAGE_SELECT, 0x0);
			break;
		case BT_PCOREMX6SXR2:
			phy_write(phydev, MDIO_DEVAD_NONE, MIIM_RTL8211F_PAGE_SELECT, 0xd04);
			phy_write(phydev, MDIO_DEVAD_NONE, 0x10, LED_MODE_B | LED_LINK(1) | LED_ACT(1));
			phy_write(phydev, MDIO_DEVAD_NONE, MIIM_RTL8211F_PAGE_SELECT, 0x0);
			break;
		}
	}

	return 0;
}
#endif /* CONFIG_CMD_NET */

#ifdef CONFIG_OF_BOARD_SETUP
/* Reserve a RAM memory region (Framebuffer, Cortex-M4)*/
static void fs_fdt_reserve_ram(void *fdt)
{
	DECLARE_GLOBAL_DATA_PTR;
	u32 size,base, end, vring_size, vring_base;
	u32 ram_base, ram_end;
	fdt32_t tmp[2];
	int offs, rm_offs;
	char name[30];

	/* Get the size to reserve from environment variable */
	size = env_get_hex("reserved_ram_size", 0);
	base = env_get_hex("reserved_ram_base", 0) & ~0xfffff;
	end = env_get_hex("reserved_ram_end", 0) & ~0xfffff;

	/* Round up to next MB boundary */
	size = (size + 0xfffff) & ~0xfffff;
	if (!size && !(base && end))
		return;
	if (!base && !end) {
		/* If just the size if given, place the reserved
		   memory at the end of the Cortex-M4 aliased DRAM
		   region ( first 265 MB) */
		end = gd->bd->bi_dram[0].start + M4_DRAM_MAX_CODE_SIZE;
		base = end - size;
	}
	else if (!base)
		base = end - size;
	else if (!end)
		end = base + size;

	/* Save 64 MB for the Linux kernel */
	ram_base = ((gd->bd->bi_dram[0].start + 0xfffff) & ~0xfffff) + (64 << 20);
	if (base < ram_base) {
		base = ram_base;
		printf("## ram_base is in the kernel memory area (first 64 MB)!\n"
				"   Moving ram_base to 0x%08x\n", base);
	}
	/* Make sure we stay in the maximum ram size available */
	ram_end = (gd->bd->bi_dram[0].start + gd->bd->bi_dram[0].size) & ~0xfffff;
	if (end > ram_end){
		end = ram_end;
		printf("## ram_end exceeds maxmimal available memory!\n"
				"   Moving ram_end to 0x%08x\n", end);
	}

	size = end-base;
	if (base > end){
		printf("## Invalid ram_size! Aborting!\n"
				"   ram_base: 0x%08x\n"
				"   ram_end:  0x%08x\n"
				"   ram_size: 0x%08x\n",base,end,size);
		return;
	}
	/* Get the reserved-memory node */
	rm_offs = fdt_path_offset(fdt, FDT_RES_MEM);
	if (rm_offs < 0)
		return;

	/* Create a node under reserved-memory (or update existing node) */
	snprintf(name, sizeof(name), "by-uboot@%08x", base);
	name[sizeof(name) - 1] = '\0';
	offs = fdt_add_subnode(fdt, rm_offs, name);
	if (offs == -FDT_ERR_EXISTS)
		offs = fdt_subnode_offset(fdt, rm_offs, name);
	if (offs >= 0) {
		printf("## Reserving RAM at 0x%08x, size 0x%08x\n", base, size);
		tmp[0] = cpu_to_fdt32(base);
		tmp[1] = cpu_to_fdt32(size);
		fs_fdt_set_val(fdt, offs, "reg", tmp, sizeof(tmp), 1);
		fdt_setprop(fdt, offs, "no-map", NULL, 0);
	}

	/* Let vring-buffer-addresses point to last 64K of this area */
	vring_size = env_get_hex("vring_size", 0);
	if (!vring_size)
	vring_size = RPMSG_SIZE;
	offs = fs_fdt_path_offset(fdt, FDT_RPMSG);
	if (offs < 0) {
		printf("   Trying legacy path\n");
		offs = fs_fdt_path_offset(fdt, FDT_RPMSG_LEGACY);
	}
	if (offs >= 0) {
		fdt32_t tmp[2];
		vring_base = base + size -vring_size;
		printf("## Reserving RPMSG vring-buffers at 0x%08x, size 0x%08x\n", vring_base, vring_size);
		tmp[0] = cpu_to_fdt32(vring_base);
		tmp[1] = cpu_to_fdt32(vring_size);

		fs_fdt_set_val(fdt, offs, "reg", tmp, sizeof(tmp), 1);
	}
}

/* Do all fixups that are done on both, U-Boot and Linux device tree */
static int do_fdt_board_setup_common(void *fdt)
{
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();
	unsigned int board_type = fs_board_get_type();
	unsigned int features = pargs->chFeatures2;

	/* Disable NAND node only for board type BT_PCOREMX6SX.
	 * These two board types can either have eMMC or NAND. EFUSA9X can have
	 * both, therefore we only disable the NAND node in case of PCOREMX6SX.
	 */
	if ((board_type == BT_PCOREMX6SX)||(board_type == BT_PCOREMX6SXR2)) {
		/* Disable NAND if it is not available */
		if ((features & FEAT2_EMMC))
			fs_fdt_enable(fdt, FDT_NAND, 0);
	}

	/* Disable eMMC if it is not available */
	if (!(features & FEAT2_EMMC))
		fs_fdt_enable(fdt, FDT_EMMC, 0);

	return 0;
}

/* Do any additional board-specific device tree modifications */
int ft_board_setup(void *fdt, struct bd_info *bd)
{
	int offs, err;
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();
	u32 val;

	printf("   Setting run-time properties\n");

	/* Optionally reserve some DDR RAM (e.g. Cortex-M4 RPMsg buffers) */
	fs_fdt_reserve_ram(fdt);

	/* Set ECC strength for NAND driver */
	offs = fs_fdt_path_offset(fdt, FDT_NAND);
	if (offs < 0) {
		printf("   Trying legacy path\n");
		offs = fs_fdt_path_offset(fdt, FDT_NAND_LEGACY);
	}

	if (offs >= 0) {
		fs_fdt_set_u32(fdt, offs, "fus,ecc_strength",
			       pargs->chECCtype, 1);
	}

	/* Set bdinfo entries */
	offs = fs_fdt_path_offset(fdt, "/bdinfo");
	if (offs >= 0) {
		int id = 0;

		/* Set common bdinfo entries */
		fs_fdt_set_bdinfo(fdt, offs);

		/* MAC addresses */
		if (pargs->chFeatures2 & FEAT2_ETH_A)
			fs_fdt_set_macaddr(fdt, offs, id++);
		if (pargs->chFeatures2 & FEAT2_ETH_B)
			fs_fdt_set_macaddr(fdt, offs, id++);
		/* WLAN MAC address only required on Silex based board revs */
		if ((pargs->chFeatures2 & FEAT2_WLAN)
		    && (((board_type == BT_EFUSA9X) && (board_rev >= 120))
			|| (board_type == BT_VAND3) || (board_type == BT_EFUSA9XR2)))
			fs_fdt_set_wlan_macaddr(fdt, offs, id++, 1);
	}

	/* Disable ethernet node(s) if feature is not available */
	if (!(pargs->chFeatures2 & FEAT2_ETH_A)) {
		err = fs_fdt_enable(fdt, FDT_ETH_A, 0);
		if(err) {
			printf("   Trying legacy path\n");
			fs_fdt_enable(fdt, FDT_ETH_A_LEGACY, 0);
		}
	}

	if (!(pargs->chFeatures2 & FEAT2_ETH_B)) {
		err = fs_fdt_enable(fdt, FDT_ETH_B, 0);
		if(err) {
			printf("   Trying legacy path\n");
			fs_fdt_enable(fdt, FDT_ETH_B_LEGACY, 0);
		}
	}

	/* Check if GPU is present */
	/* Disabled interfaces are in fuse bank 0, word 4 */
	if (!(fuse_read(0, 4, &val))) {
		if (val & GPU_DISABLE_MASK) {
			fs_fdt_enable(fdt, FDT_GPU, 0);
			offs = fs_fdt_path_offset(fdt, FDT_GPC);
			if (offs >= 0) {
				fs_fdt_set_val(fdt, offs, "no-gpu",
				NULL, 0, 1);
			}
		}
	}

	return do_fdt_board_setup_common(fdt);
}
#endif /* CONFIG_OF_BOARD_SETUP */

/* Board specific cleanup before Linux is started */
void board_preboot_os(void)
{
#ifdef CONFIG_VIDEO_MXS
	/* Switch off backlight and display voltages */
	/* ### TODO: In the future the display should pass smoothly to Linux,
	   then switching everything off should not be necessary anymore. */
	fs_disp_set_backlight_all(0);
	fs_disp_set_power_all(0);
#endif

	/* Shut down all ethernet PHYs (suspend mode); on CONT1, all PHYs are
	   external PHYs on the SJ1105 ethernet switch to the outside that
	   must remain active. */
	if (fs_board_get_type() != BT_CONT1)
		mdio_shutdown_all();
}
