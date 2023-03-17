/*
 * fsimx6.c
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
#include <phy.h>
#endif
#include <serial.h>			/* struct serial_device */
#include <environment.h>

#ifdef CONFIG_FSL_ESDHC
#include <mmc.h>
#include <fsl_esdhc.h>			/* fsl_esdhc_initialize(), ... */
#include "../common/fs_mmc_common.h"	/* struct fs_mmc_cd, fs_mmc_*(), ... */
#endif

#ifdef CONFIG_LED_STATUS_CMD
#include <status_led.h>			/* led_id_t */
#endif

#include <i2c.h>
#include <asm/mach-imx/mxc_i2c.h>

#ifdef CONFIG_VIDEO_IPUV3
#include <asm/mach-imx/video.h>
#include "../common/fs_disp_common.h"	/* struct fs_disp_port, fs_disp_*() */
#endif

#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/iomux.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/crm_regs.h>		/* CCM_CCGR1, nandf clock settings */
#include <asm/arch/clock.h>		/* enable_fec_anatop_clock(), ... */

#include <linux/mtd/rawnand.h>		/* struct mtd_info, struct nand_chip */
#include <mtd/mxs_nand_fus.h>		/* struct mxs_nand_fus_platform_data */
#include <usb.h>			/* USB_INIT_HOST, USB_INIT_DEVICE */
#include "../common/fs_fdt_common.h"	/* fs_fdt_set_val(), ... */
#include "../common/fs_board_common.h"	/* fs_board_*() */
#include "../common/fs_usb_common.h"	/* struct fs_usb_port_cfg, fs_usb_*() */

/* ------------------------------------------------------------------------- */

#define BT_EFUSA9     0
#define BT_ARMSTONEA9 1
#define BT_PICOMODA9  2
#define BT_QBLISSA9   3
#define BT_ARMSTONEA9R2 4
#define BT_QBLISSA9R2 6
#define BT_NETDCUA9   7
#define BT_EFUSA9R2   8 /* in N-Boot number 29 */
#define BT_ARMSTONEA9R3 9 /* in N-Boot number 30 */
#define BT_ARMSTONEA9R4 10 /* in N-Boot number 31 */

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
#define FEAT2_DEFAULT FEAT2_ETH_A

/* Device tree paths */
#define FDT_NAND         "nand"
#define FDT_NAND_LEGACY  "/soc/gpmi-nand@00112000"
#define FDT_ETH_A        "ethernet0"
#define FDT_ETH_A_LEGACY "/soc/aips-bus@02100000/ethernet@02188000"

#define UART_PAD_CTRL  (PAD_CTL_PUS_100K_UP |			\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm |			\
	PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define ENET_PAD_CTRL ( \
		PAD_CTL_PKE | PAD_CTL_PUE | /*Kiepfer*/ 	\
		PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED |	\
		PAD_CTL_DSE_40ohm   | PAD_CTL_HYS)

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
#define USDHC_PWR_CTRL (USDHC_CD_CTRL | PAD_CTL_DSE_40ohm)

#define EIM_NO_PULL (PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm)
#define EIM_PULL_DOWN (EIM_NO_PULL | PAD_CTL_PKE | PAD_CTL_PUE)
#define EIM_PULL_UP (EIM_PULL_DOWN | PAD_CTL_PUS_100K_UP)

#define USB_ID_PAD_CTRL (PAD_CTL_PUS_47K_UP | PAD_CTL_SPEED_LOW | PAD_CTL_HYS)

#define LCD_CTRL PAD_CTL_DSE_120ohm

#define I2C_PAD_CTRL  (PAD_CTL_PUS_100K_UP |			\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm | PAD_CTL_HYS |	\
	PAD_CTL_ODE | PAD_CTL_SRE_FAST)

#define INSTALL_RAM "ram@10300000"
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

const struct fs_board_info board_info[11] = {
	{	/* 0 (BT_EFUSA9) */
		.name = "efusA9",
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
	{	/* 1 (BT_ARMSTONEA9)*/
		.name = "armStoneA9",
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
	{	/* 2 (BT_PICOMODA9) */
		.name = "PicoMODA9",
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
	{	/* 3 (BT_QBLISSA9) */
		.name = "QBlissA9",
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
	{	/* 4 (BT_ARMSTONEA9R2) */
		.name = "armStoneA9r2",
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
	{	/* 6 (BT_QBLISSA9R2) */
		.name = "QBlissA9r2",
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
	{	/* 7 (BT_NETDCUA9) */
		.name = "NetDCUA9",
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
	{	/* 29 (BT_EFUSA9R2) */
		.name = "efusA9r2",
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
	{	/* 30 (BT_ARMSTONEA9R3) */
		.name = "armStoneA9r3",
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
	{	/* 31 (BT_ARMSTONEA9R4) */
		.name = "armStoneA9r4",
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

#ifdef CONFIG_VIDEO_IPUV3
static void setup_lcd_pads(int on);	/* Defined below */
#endif

/* Do some very early board specific setup */
int board_early_init_f(void)
{
#ifdef CONFIG_VIDEO_IPUV3
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
	return NAND_BOOT;
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
	if ((features2 & FEAT2_ETH_MASK) == FEAT2_ETH_MASK)
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

static iomux_v3_cfg_t const reset_pads[] = {
	IOMUX_PADS(PAD_ENET_RXD1__GPIO1_IO26 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const wlan_reset_pads[] = {
	IOMUX_PADS(PAD_ENET_RXD0__GPIO1_IO27 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

int board_init(void)
{
	unsigned int board_type = fs_board_get_type();

	/* Copy NBoot args to variables and prepare command prompt string */
	fs_board_init_common(&board_info[board_type]);

	if (board_type == BT_EFUSA9 || board_type == BT_EFUSA9R2) {
		/*
		 * efusA9/r2 has a generic RESET_OUT signal to reset some
		 * arbitrary hardware. This signal is available on the efus
		 * connector pin 14 and in turn on pin 8 of the SKIT feature
		 * connector. Because there might be some rather slow hardware
		 * involved, use a rather long low pulse of 1ms.
		 *
		 * FIXME: Should we do this somewhere else when we know the
		 * pulse time?
		 */
		SETUP_IOMUX_PADS(reset_pads);
		fs_board_issue_reset(1000, 0, IMX_GPIO_NR(1, 26), ~0, ~0);
	} else if (board_type == BT_ARMSTONEA9R2 || board_type == BT_ARMSTONEA9R4) {
		/* Reset uBlox WLAN/BT on armStoneA9r2 */
		SETUP_IOMUX_PADS(wlan_reset_pads);
		fs_board_issue_reset(1000, 0, IMX_GPIO_NR(1, 27), ~0, ~0);
	}

	return 0;
}


/* nand flash pads  */
static iomux_v3_cfg_t const nfc_pads[] = {
	IOMUX_PADS(PAD_NANDF_CLE__NAND_CLE | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NANDF_ALE__NAND_ALE | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NANDF_WP_B__NAND_WP_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NANDF_RB0__NAND_READY_B | MUX_PAD_CTRL(GPMI_PAD_CTRL0)),
	IOMUX_PADS(PAD_NANDF_CS0__NAND_CE0_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_SD4_CMD__NAND_RE_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_SD4_CLK__NAND_WE_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NANDF_D0__NAND_DATA00 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NANDF_D1__NAND_DATA01 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NANDF_D2__NAND_DATA02 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NANDF_D3__NAND_DATA03 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NANDF_D4__NAND_DATA04 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NANDF_D5__NAND_DATA05 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NANDF_D6__NAND_DATA06 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NANDF_D7__NAND_DATA07 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
};

/* Register NAND devices. We actually split the NAND into two virtual devices
   to allow different ECC strategies for NBoot and the rest. */
void board_nand_init(void)
{
	int reg;
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	struct mxs_nand_fus_platform_data pdata;

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

#ifdef CONFIG_FSL_ESDHC
/*
 * SD/MMC support.
 *
 *   Board         USDHC   CD-Pin                 Slot
 *   -----------------------------------------------------------------------
 *   QBlissA9:     USDHC3  NANDF_CS2 (GPIO6_IO15) Connector
 *        either:  USDHC1  -                      On-board (micro-SD)
 *            or: [USDHC1  GPIO_1 (GPIO1_IO01)    WLAN]
 *   -----------------------------------------------------------------------
 *   QBlissA9r2:   USDHC2  GPIO_4 (GPIO1_IO04)    Connector
 *                 USDHC3  -                      eMMC (8-Bit)
 *                [USDHC1  GPIO_1 (GPIO1_IO01)    WLAN]
 *   -----------------------------------------------------------------------
 *   armStoneA9:   USDHC3  NANDF_CS2 (GPIO6_IO15) On-board (micro-SD)
 *   -----------------------------------------------------------------------
 *   armStoneA9r2: USDHC2  GPIO_4 (GPIO1_IO04)    On-board (micro-SD)
 *                 USDHC3  -                      eMMC (8-Bit)
 *                [USDHC1  GPIO_1 (GPIO1_IO01)    WLAN]
 *   -----------------------------------------------------------------------
 *   efusA9:       USDHC2  GPIO_4 (GPIO1_IO04)    SD_B: Connector (SD)
 *                 USDHC1  GPIO_1 (GPIO1_IO01)    SD_A: Connector (Micro-SD)
 *                 USDHC3  -                      eMMC (8-Bit)
 *   -----------------------------------------------------------------------
 *   efusA9r2:     USDHC2  GPIO_4 (GPIO1_IO04)    SD_B: Connector (SD)
 *                 USDHC1  GPIO_1 (GPIO1_IO01)    SD_A: Connector (Micro-SD)
 *                 USDHC3  -                      eMMC (8-Bit)
 *   -----------------------------------------------------------------------
 *   PicoMODA9:    USDHC2  GPIO_4 (GPIO1_IO04)    Connector (SD)
 *        either:  USDHC1  -                      On-board (micro-SD)
 *            or:  USDHC1  -                      eMMC (4-Bit)
 *   -----------------------------------------------------------------------
 *   NetDCUA9:     USDHC1  GPIO_1 (GPIO1_IO01)    On-board (SD)
 *                 USDHC3  -                      eMMC (8-Bit)
 *   -----------------------------------------------------------------------
 *
 * Remark: The WP pin is ignored in U-Boot, also WLAN. QBlissA9r2 has an extra
 * GPIO to switch power for USDHC2 port.
 */

/* SD/MMC card pads definition, distinguish external from internal ports */
static iomux_v3_cfg_t const usdhc1_sd_pads_ext[] = {
	IOMUX_PADS(PAD_SD1_CLK__SD1_CLK | MUX_PAD_CTRL(USDHC_CLK_EXT)),
	IOMUX_PADS(PAD_SD1_CMD__SD1_CMD | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD1_DAT0__SD1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD1_DAT1__SD1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD1_DAT2__SD1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD1_DAT3__SD1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
};

static iomux_v3_cfg_t const usdhc1_sd_pads_int[] = {
	IOMUX_PADS(PAD_SD1_CLK__SD1_CLK | MUX_PAD_CTRL(USDHC_CLK_INT)),
	IOMUX_PADS(PAD_SD1_CMD__SD1_CMD | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD1_DAT0__SD1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD1_DAT1__SD1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD1_DAT2__SD1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD1_DAT3__SD1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_NANDF_D0__SD1_DATA4 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_NANDF_D1__SD1_DATA5 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_NANDF_D2__SD1_DATA6 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_NANDF_D3__SD1_DATA7 | MUX_PAD_CTRL(USDHC_PAD_INT)),
};

static iomux_v3_cfg_t const usdhc2_sd_pads_ext[] = {
	IOMUX_PADS(PAD_SD2_CLK__SD2_CLK    | MUX_PAD_CTRL(USDHC_CLK_EXT)),
	IOMUX_PADS(PAD_SD2_CMD__SD2_CMD    | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD2_DAT0__SD2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD2_DAT1__SD2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD2_DAT2__SD2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD2_DAT3__SD2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_NANDF_D4__SD2_DATA4 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_NANDF_D5__SD2_DATA5 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_NANDF_D6__SD2_DATA6 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_NANDF_D7__SD2_DATA7 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
};

static iomux_v3_cfg_t const usdhc3_sd_pads_ext[] = {
	IOMUX_PADS(PAD_SD3_CLK__SD3_CLK    | MUX_PAD_CTRL(USDHC_CLK_EXT)),
	IOMUX_PADS(PAD_SD3_CMD__SD3_CMD    | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD3_DAT0__SD3_DATA0 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD3_DAT1__SD3_DATA1 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD3_DAT2__SD3_DATA2 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD3_DAT3__SD3_DATA3 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
};

static iomux_v3_cfg_t const usdhc3_sd_pads_int[] = {
	IOMUX_PADS(PAD_SD3_CLK__SD3_CLK    | MUX_PAD_CTRL(USDHC_CLK_INT)),
	IOMUX_PADS(PAD_SD3_CMD__SD3_CMD    | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD3_RST__SD3_RESET  | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD3_DAT0__SD3_DATA0 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD3_DAT1__SD3_DATA1 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD3_DAT2__SD3_DATA2 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD3_DAT3__SD3_DATA3 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD3_DAT4__SD3_DATA4 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD3_DAT5__SD3_DATA5 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD3_DAT6__SD3_DATA6 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_SD3_DAT7__SD3_DATA7 | MUX_PAD_CTRL(USDHC_PAD_INT)),
};

/* CD on pad NANDF_CS2 */
static iomux_v3_cfg_t const cd_nandf_cs2[] = {
	IOMUX_PADS(PAD_NANDF_CS2__GPIO6_IO15 | MUX_PAD_CTRL(USDHC_CD_CTRL)),
};

/* CD on pad GPIO_1 */
static iomux_v3_cfg_t const cd_gpio_1[] = {
	IOMUX_PADS(PAD_GPIO_1__GPIO1_IO01  | MUX_PAD_CTRL(USDHC_CD_CTRL)),
};

/* CD on pad GPIO_4 */
static iomux_v3_cfg_t const cd_gpio_4[] = {
	IOMUX_PADS(PAD_GPIO_4__GPIO1_IO04  | MUX_PAD_CTRL(USDHC_CD_CTRL)),
};

/* PWR on QBlissA9r2 */
static iomux_v3_cfg_t const pwr_qblissa9r2[] = {
	IOMUX_PADS(PAD_EIM_DA14__GPIO3_IO14 | MUX_PAD_CTRL(USDHC_PWR_CTRL)),
};

/* Extended SDHC configuration. Pad count is without data signals, the data
   signal count will be added automatically according to bus_width. */
struct fus_sdhc_cfg {
	const iomux_v3_cfg_t *const pads;
	const u8 count;
	const u8 index;
	u16 cd_gpio;
	struct fsl_esdhc_cfg esdhc;
};

enum usdhc_pads {
	usdhc1_ext, usdhc1_int, usdhc2_ext, usdhc3_ext, usdhc3_int
};

static struct fs_mmc_cfg sdhc_cfg[] = {
		      /* pads,               count, USDHC# */
	[usdhc1_ext] = { usdhc1_sd_pads_ext, 2,     1 },
	[usdhc1_int] = { usdhc1_sd_pads_int, 2,     1 },
	[usdhc2_ext] = { usdhc2_sd_pads_ext, 2,     2 },
	[usdhc3_ext] = { usdhc3_sd_pads_ext, 2,     3 },
	[usdhc3_int] = { usdhc3_sd_pads_int, 3,     3 },
};

enum usdhc_cds {
	gpio1_io01, gpio1_io04, gpio6_io15
};

static const struct fs_mmc_cd sdhc_cd[] = {
		      /* pad,          gpio */
	[gpio1_io01] = { cd_gpio_1,    IMX_GPIO_NR(1, 1) },
	[gpio1_io04] = { cd_gpio_4,    IMX_GPIO_NR(1, 4) },
	[gpio6_io15] = { cd_nandf_cs2, IMX_GPIO_NR(6, 15) },
};

int board_mmc_init(bd_t *bd)
{
	int ret = 0;
	unsigned int board_type = fs_board_get_type();
	unsigned int features2 = fs_board_get_nboot_args()->chFeatures2;

	switch (board_type) {
	case BT_ARMSTONEA9:
	case BT_ARMSTONEA9R3:
		/* mmc0: USDHC3 (on-board micro SD slot), CD: GPIO6_IO15 */
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc3_ext],
				   &sdhc_cd[gpio6_io15]);
		break;

	case BT_ARMSTONEA9R2:
	case BT_ARMSTONEA9R4:
		/* mmc0: USDHC2 (on-board micro SD slot), CD: GPIO1_IO04 */
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc2_ext],
				   &sdhc_cd[gpio1_io04]);

		/* mmc1: USDHC3 (eMMC, if available), no CD */
		if (!ret && (features2 & FEAT2_EMMC))
			ret = fs_mmc_setup(bd, 8, &sdhc_cfg[usdhc3_int], NULL);
		break;

	case BT_NETDCUA9:
		/* mmc0: USDHC1 (on-board SD slot), CD: GPIO1_IO01 */
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc1_ext],
				   &sdhc_cd[gpio1_io01]);

		/* mmc1: USDHC3 (eMMC, if available), no CD */
		if (!ret && (features2 & FEAT2_EMMC))
			ret = fs_mmc_setup(bd, 8, &sdhc_cfg[usdhc3_int], NULL);
		break;

	case BT_QBLISSA9:
		/* mmc0: USDHC3 (ext. SD slot via connector), CD: GPIO6_IO15 */
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc3_ext],
				   &sdhc_cd[gpio6_io15]);

		/* mmc1: USDHC1: on-board micro SD slot (if available), no CD */
		if (!ret && !(features2 & FEAT2_WLAN))
			ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc1_ext], NULL);
		break;

	case BT_QBLISSA9R2:
		/* Enable port power for mmc0 (USDHC2) */
		SETUP_IOMUX_PADS(pwr_qblissa9r2);
		gpio_direction_output(IMX_GPIO_NR(3, 14), 0);

		/* mmc0: USDHC2: connector, CD: GPIO1_IO04 */
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc2_ext],
				   &sdhc_cd[gpio1_io04]);
		if (ret)
			break;

		/* mmc1: USDHC3: eMMC (if available), no CD */
		if (!ret && (features2 & FEAT2_EMMC))
			ret = fs_mmc_setup(bd, 8, &sdhc_cfg[usdhc3_int], NULL);
		break;

	case BT_PICOMODA9:
		/* mmc0: USDHC2 (ext. SD slot via connector), CD: GPIO1_IO04 */
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc2_ext],
				   &sdhc_cd[gpio1_io04]);

		/* mmc1: USDHC1 (on-board micro SD or on-board eMMC), no CD
		   Remark: eMMC also only uses 4 bits if NAND is present. */
		if (!ret && !(features2 & FEAT2_EMMC))
			ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc1_ext], NULL);
		if (!ret && (features2 & FEAT2_EMMC))
			ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc1_int], NULL);
		break;

	case BT_EFUSA9:
	case BT_EFUSA9R2:
		/* mmc0: USDHC2 (ext. SD slot, normal-size SD on efus SKIT) */
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc2_ext],
				   &sdhc_cd[gpio1_io04]);

		/* mmc1: USDHC1 (ext. SD slot, micro SD on efus SKIT) */
		if (!ret)
			ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc1_ext],
					   &sdhc_cd[gpio1_io01]);

		/* mmc2: USDHC3 (eMMC, if available), no CD */
		if (!ret && (features2 & FEAT2_EMMC))
			ret = fs_mmc_setup(bd, 8, &sdhc_cfg[usdhc3_int], NULL);
		break;

	default:
		return 0;		/* Neither SD card, nor eMMC */
	}

	return ret;
}
#endif /* CONFIG_FSL_ESDHC */

#ifdef CONFIG_VIDEO_IPUV3
/*
 * Possible display configurations
 *
 *   Board          LCD      LVDS0    LVDS1    HDMI
 *   -------------------------------------------------------------
 *   efusA9         18 bit*  24 bit   24 bit   24 bit
 *   efusA9r2       18 bit*  24 bit   24 bit   24 bit
 *   armStoneA9     18 bit*  24 bit   24 bit   24 bit
 *   armStoneA9r2   -        24 bit*  24 bit   24 bit
 *   QBlissA9       -        24 bit*  24 bit   24 bit
 *   QBlissA9r2     -        24 bit*  24 bit   24 bit
 *   NetDCUA9       24 bit*  24 bit   -        -
 *   PicoMODA9      18 bit   24 bit*  -        24 bit
 *
 * The entry marked with * is the default port.
 *
 * Display initialization sequence:
 *
 *  1. board_r.c: board_init_r() calls stdio_init().
 *  2. stdio.c: stdio_init() calls drv_video_init().
 *  3. cfb_console.c: drv_video_init() calls board_video_skip(); if this
 *     returns non-zero, the display will not be started.
 *  4. fsimx6.c: board_video_skip(): Call fs_disp_register(). This checks for
 *     display parameters and activates the display. To handle board specific
 *     stuff, it will call callback functions board_display_set_backlight(),
 *     board_display_set_power() and board_display_start() here. The latter
 *     will initialize the clocks and call ipuv3_fb_init() to store mode and
 *     pixfmt to be used later.
 *  5. cfb_console.c: drv_video_init() calls video_init().
 *  6. cfb_console.c: video_init() calls video_hw_init().
 *  7. mxc_ipuv3_fb.c: video_hw_init() calls ipu_probe().
 *  8. ipu_common.c: ipu_probe() initializes a minimal IPU clock tree.
 *  9. mxc_ipuv3_fb.c: video_hw_init() calls mxcfb_probe().
 * 10. mxc_ipuv3_fb.c: mxcfb_probe() initializes framebuffer, using the values
 *     stored in step 4 above.
 * 11. cfb_console.c: video_init() clears framebuffer and calls video_logo().
 * 12. cfb_console.c: video_logo() draws either the console logo and the welcome
 *     message, or if environment variable splashimage is set, the splash
 *     screen.
 * 13. cfb_console.c: drv_video_init() registers the console as stdio device.
 * 14. board_r.c: board_init_r() calls board_late_init().
 * 15. fsimx6.c: board_late_init() calls fs_board_set_backlight_all() to
 *     enable all active displays.
 */

/* Pads for 18-bit LCD interface */
static iomux_v3_cfg_t const lcd18_pads_low[] = {
	IOMUX_PADS(PAD_DI0_DISP_CLK__GPIO4_IO16 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DI0_PIN2__GPIO4_IO18     | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DI0_PIN3__GPIO4_IO19     | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DI0_PIN15__GPIO4_IO17    | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT0__GPIO4_IO21   | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT1__GPIO4_IO22   | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT2__GPIO4_IO23   | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT3__GPIO4_IO24   | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT4__GPIO4_IO25   | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT5__GPIO4_IO26   | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT6__GPIO4_IO27   | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT7__GPIO4_IO28   | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT8__GPIO4_IO29   | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT9__GPIO4_IO30   | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT10__GPIO4_IO31  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT11__GPIO5_IO05  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT12__GPIO5_IO06  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT13__GPIO5_IO07  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT14__GPIO5_IO08  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT15__GPIO5_IO09  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT16__GPIO5_IO10  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT17__GPIO5_IO11  | MUX_PAD_CTRL(0x3010)),
};

static iomux_v3_cfg_t const lcd18_pads_active[] = {
	IOMUX_PADS(PAD_DI0_DISP_CLK__IPU1_DI0_DISP_CLK | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DI0_PIN2__IPU1_DI0_PIN02       | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DI0_PIN3__IPU1_DI0_PIN03       | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DI0_PIN15__IPU1_DI0_PIN15      | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT0__IPU1_DISP0_DATA00  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT1__IPU1_DISP0_DATA01  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT2__IPU1_DISP0_DATA02  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT3__IPU1_DISP0_DATA03  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT4__IPU1_DISP0_DATA04  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT5__IPU1_DISP0_DATA05  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT6__IPU1_DISP0_DATA06  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT7__IPU1_DISP0_DATA07  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT8__IPU1_DISP0_DATA08  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT9__IPU1_DISP0_DATA09  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT10__IPU1_DISP0_DATA10 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT11__IPU1_DISP0_DATA11 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT12__IPU1_DISP0_DATA12 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT13__IPU1_DISP0_DATA13 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT14__IPU1_DISP0_DATA14 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT15__IPU1_DISP0_DATA15 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT16__IPU1_DISP0_DATA16 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT17__IPU1_DISP0_DATA17 | MUX_PAD_CTRL(LCD_CTRL)),
};

/* Additional pads for 24-bit LCD interface */
static iomux_v3_cfg_t const lcd24_pads_low[] = {
	IOMUX_PADS(PAD_DISP0_DAT18__GPIO5_IO12  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT19__GPIO5_IO13  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT20__GPIO5_IO14  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT21__GPIO5_IO15  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT22__GPIO5_IO16  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT23__GPIO5_IO17  | MUX_PAD_CTRL(0x3010)),
};

static iomux_v3_cfg_t const lcd24_pads_active[] = {
	IOMUX_PADS(PAD_DISP0_DAT18__IPU1_DISP0_DATA18 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT19__IPU1_DISP0_DATA19 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT20__IPU1_DISP0_DATA20 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT21__IPU1_DISP0_DATA21 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT22__IPU1_DISP0_DATA22 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT23__IPU1_DISP0_DATA23 | MUX_PAD_CTRL(LCD_CTRL)),
};

/* Additional pads for VLCD_ON, VCFL_ON and BKLT_PWM */
static iomux_v3_cfg_t const lcd_extra_pads[] = {
	/* Signals are active high -> pull-down to switch off */
	IOMUX_PADS(PAD_SD4_DAT3__GPIO2_IO11 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_SD4_DAT0__GPIO2_IO08 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_SD4_DAT1__GPIO2_IO09 | MUX_PAD_CTRL(0x3010)),
};

/* Additional pads for ENA and DEN */
static iomux_v3_cfg_t const lcd_extra_pads_pmoda9[] = {
	/* Signals are active low -> pull-up to switch off */
	IOMUX_PADS(PAD_SD4_DAT4__GPIO2_IO12 | MUX_PAD_CTRL(0xb010)),
	IOMUX_PADS(PAD_SD4_DAT5__GPIO2_IO13 | MUX_PAD_CTRL(0xb010)),
	/* Signals acre connected to LCD pins*/
	IOMUX_PADS(PAD_CSI0_PIXCLK__GPIO5_IO18  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_CSI0_MCLK__GPIO5_IO19    | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_CSI0_DATA_EN__GPIO5_IO20 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_CSI0_VSYNC__GPIO5_IO21   | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_CSI0_DAT12__GPIO5_IO30 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_CSI0_DAT13__GPIO5_IO31 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_CSI0_DAT14__GPIO6_IO00 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_CSI0_DAT15__GPIO6_IO01 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_CSI0_DAT16__GPIO6_IO02 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_CSI0_DAT17__GPIO6_IO03 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_CSI0_DAT18__GPIO6_IO04 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_CSI0_DAT19__GPIO6_IO05 | MUX_PAD_CTRL(0x3010)),
};

/* Use I2C2 to talk to RGB adapter on armStoneA9 */
I2C_PADS(armstonea9,						\
	 PAD_GPIO_3__I2C3_SCL | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 PAD_GPIO_3__GPIO1_IO03 |  MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 IMX_GPIO_NR(1, 3),					\
	 PAD_GPIO_16__I2C3_SDA | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 PAD_GPIO_16__GPIO7_IO11 | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 IMX_GPIO_NR(7, 11));

/* Use I2C1 to talk to RGB adapter on efusA9/r2 */
I2C_PADS(efusa9,						\
	 PAD_KEY_COL3__I2C2_SCL | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 PAD_KEY_COL3__GPIO4_IO12 |  MUX_PAD_CTRL(I2C_PAD_CTRL),\
	 IMX_GPIO_NR(4, 12),					\
	 PAD_KEY_ROW3__I2C2_SDA | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 PAD_KEY_ROW3__GPIO4_IO13 | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 IMX_GPIO_NR(4, 13));

enum display_port_index {
	port_lcd, port_lvds0, port_lvds1, port_hdmi
};

/* Define possible displays ports; LVDS ports may have additional settings */
#define FS_DISP_FLAGS_LVDS						\
	(FS_DISP_FLAGS_LVDS_2CH | FS_DISP_FLAGS_LVDS_DUP		\
	 | FS_DISP_FLAGS_LVDS_24BPP | FS_DISP_FLAGS_LVDS_JEIDA)

static const struct fs_display_port display_ports[CONFIG_FS_DISP_COUNT] = {
	[port_lcd] =   { "lcd",   FS_DISP_FLAGS_LVDS_BL_INV },
	[port_lvds0] = { "lvds0", FS_DISP_FLAGS_LVDS | FS_DISP_FLAGS_LVDS_BL_INV },
	[port_lvds1] = { "lvds1", FS_DISP_FLAGS_LVDS | FS_DISP_FLAGS_LVDS_BL_INV },
	[port_hdmi] =  { "hdmi",  0 }
};

static void setup_lcd_pads(int on)
{
	int board_type = fs_board_get_type();

	switch (board_type)
	{
	case BT_NETDCUA9:		/* Boards with 24-bit LCD interface */
		if (on)
			SETUP_IOMUX_PADS(lcd24_pads_active);
		else
			SETUP_IOMUX_PADS(lcd24_pads_low);
		/* No break, fall through to default case */
	case BT_EFUSA9:
	case BT_EFUSA9R2:
	case BT_ARMSTONEA9:
	case BT_ARMSTONEA9R3:
	case BT_PICOMODA9:
	default:			/* Boards with 18-bit LCD interface */
		if (on)
			SETUP_IOMUX_PADS(lcd18_pads_active);
		else
			SETUP_IOMUX_PADS(lcd18_pads_low);
		/* No break, fall through to case BT_ARMSTONEA9R2 */
	case BT_ARMSTONEA9R2:		/* Boards without LCD interface */
	case BT_ARMSTONEA9R4:
	case BT_QBLISSA9:
	case BT_QBLISSA9R2:
		SETUP_IOMUX_PADS(lcd_extra_pads);
		if (board_type == BT_PICOMODA9)
			SETUP_IOMUX_PADS(lcd_extra_pads_pmoda9);
		break;
	}
}

/* Enable or disable display voltage VLCD; if LCD, also switch LCD pads */
void board_display_set_power(int port, int on)
{
	static unsigned int vlcd_users;

	if (port == port_hdmi)
		return;

	/* Switch on when first user enables and off when last user disables */
	if (!on) {
		vlcd_users &= ~(1 << port);
		if (port == port_lcd)
			setup_lcd_pads(0);
	}
	if (!vlcd_users) {
		gpio_direction_output(IMX_GPIO_NR(2, 11), on);
		if (on)
			mdelay(1);
		if(fs_board_get_type() == BT_PICOMODA9) {
			/* ENA -> low active */
			gpio_direction_output(IMX_GPIO_NR(2, 13), !on);
			mdelay(1);
			/* DEN -> low active */
			gpio_direction_output(IMX_GPIO_NR(2, 12), !on);
			mdelay(1);
			// ### TODO: Set LCD_DEN on NetDCUA9
		}
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

	switch (port) {
	case port_lcd:
		switch (fs_board_get_type()) {
		case BT_EFUSA9:
		case BT_EFUSA9R2:
			if (!i2c_init) {
				setup_i2c(1, CONFIG_SYS_I2C_SPEED, 0x60,
					  I2C_PADS_INFO(efusa9));
				i2c_init = 1;
			}
			fs_disp_set_i2c_backlight(1, on);
			break;
		case BT_ARMSTONEA9:
		case BT_ARMSTONEA9R3:
			if (!i2c_init) {
				setup_i2c(2, CONFIG_SYS_I2C_SPEED, 0x60,
					  I2C_PADS_INFO(armstonea9));
				i2c_init = 1;
			}
			fs_disp_set_i2c_backlight(2, on);
			break;
		case BT_NETDCUA9:
		case BT_PICOMODA9:
			fs_disp_set_vcfl(port, on, IMX_GPIO_NR(2, 8));
			fs_disp_set_bklt_pwm(port, on, IMX_GPIO_NR(2, 9));
			break;
		default:
			break;
		}
		break;

	case port_lvds0:
	case port_lvds1:
		fs_disp_set_vcfl(port, on, IMX_GPIO_NR(2, 8));
		fs_disp_set_bklt_pwm(port, on, IMX_GPIO_NR(2, 9));
		break;
	case port_hdmi:
		break;
	}
}

/* If UART is sending data, wait until done */
static void wait_for_uart(void)
{
#if 0
	//### TODO. For this to work we need to add serial_stop() to serial.c
	//### and mxc_serial_stop() to serial_mxc.c. In the latter, wait for
	//### end of transmission.
	serial_stop();
	serial_init();
#endif
}

/* Activate LVDS channel ### TODO: Handle 2CH and CLONE */
static void config_lvds(int disp, int channel, unsigned int flags,
			const struct fb_videomode *mode)
{
	struct iomuxc *iomux = (struct iomuxc *)IOMUXC_BASE_ADDR;
	u32 gpr2 = readl(&iomux->gpr[2]);
	u32 vs_polarity;
	u32 bit_mapping;
	u32 data_width;
	u32 enable;
	u32 mask;

	if (disp == 1)
		vs_polarity = IOMUXC_GPR2_DI1_VS_POLARITY_MASK;
	else
		vs_polarity = IOMUXC_GPR2_DI0_VS_POLARITY_MASK;
	if (channel == 1) {
		if (disp == 1)
			enable = IOMUXC_GPR2_LVDS_CH1_MODE_ENABLED_DI1;
		else
			enable = IOMUXC_GPR2_LVDS_CH1_MODE_ENABLED_DI0;
		bit_mapping = IOMUXC_GPR2_BIT_MAPPING_CH1_MASK;
		data_width = IOMUXC_GPR2_DATA_WIDTH_CH1_MASK;
		mask = IOMUXC_GPR2_LVDS_CH1_MODE_MASK;
	} else {
		if (disp == 1)
			enable = IOMUXC_GPR2_LVDS_CH0_MODE_ENABLED_DI1;
		else
			enable = IOMUXC_GPR2_LVDS_CH0_MODE_ENABLED_DI0;
		bit_mapping = IOMUXC_GPR2_BIT_MAPPING_CH0_MASK;
		data_width = IOMUXC_GPR2_DATA_WIDTH_CH0_MASK;
		mask = IOMUXC_GPR2_LVDS_CH0_MODE_MASK;
	}
	mask |= bit_mapping | data_width | vs_polarity;
	gpr2 &= ~mask;

	if (!(mode->sync & FB_SYNC_VERT_HIGH_ACT))
		gpr2 |= vs_polarity;
	if (flags & FS_DISP_FLAGS_LVDS_JEIDA)
		gpr2 |= bit_mapping;
	if (flags & FS_DISP_FLAGS_LVDS_24BPP)
		gpr2 |= data_width;
	gpr2 |= enable;

	writel(gpr2, &iomux->gpr[2]);
}

/* Set display clocks and register pixel format, resolution and timings */
int board_display_start(int port, unsigned flags, struct fb_videomode *mode)
{
	unsigned int freq_khz;
	int pixfmt = IPU_PIX_FMT_RGB666;
	/*
	 * Initialize display clock and register display settings with IPU
	 * display driver. The real initialization takes place later in
	 * function cfb_console.c: video_init().
	 *
	 * Remark: In config_lvds_clk(), the clock for UART may be switched
	 * off for a short period of time. This can result in some damaged
	 * characters if the UART is still transmitting some characters from
	 * the FIFO. Therefore wait for the end of the UART transmission first.
	 */
	freq_khz = PICOS2KHZ(mode->pixclock);
	if (flags & FS_DISP_FLAGS_LVDS_24BPP)
		pixfmt = IPU_PIX_FMT_RGB24;

	switch (port) {
	case port_lcd:
		/* The LCD port on efusA9/r2 has swapped red and blue signals */
		/* The LCD port on netdcuA9 need RGB24 */
		if (fs_board_get_type() == BT_EFUSA9 || fs_board_get_type() == BT_EFUSA9R2)
			pixfmt = IPU_PIX_FMT_BGR666;
		else if (fs_board_get_type() == BT_NETDCUA9)
			pixfmt = IPU_PIX_FMT_RGB24;
		ipuv3_config_lcd_di_clk(1, 0);
		ipuv3_fb_init(mode, 0, pixfmt);
		break;

	case port_lvds0:
		wait_for_uart();
		ipuv3_config_lvds_clk(1, 0, freq_khz, 0);
		enable_ldb_di_clk(0);
		config_lvds(0, 0, flags, mode);
		ipuv3_fb_init(mode, 0, pixfmt);
		break;

	case port_lvds1:
		wait_for_uart();
		ipuv3_config_lvds_clk(1, 1, freq_khz, 0);
		enable_ldb_di_clk(1);
		config_lvds(1, 1, flags, mode);
		ipuv3_fb_init(mode, 1, pixfmt);
		break;

	case port_hdmi:
		puts("### HDMI support not yet implemented\n");
		return 1;
	}

	ipuv3_enable_ipu_clk(1);

	return 0;
}

/* Init display or return 1 to skip display activation */
int board_video_skip(void)
{
	int default_port;
	unsigned int valid_mask;
	unsigned int board_type = fs_board_get_type();

	/* Determine possible displays and default port */
	switch (board_type) {
	case BT_EFUSA9:
	case BT_ARMSTONEA9:
	case BT_ARMSTONEA9R3:
	case BT_EFUSA9R2:
		valid_mask = (1 << port_lcd) | (1 << port_lvds0)
			| (1 << port_lvds1) | (1 << port_hdmi);
		default_port = port_lcd;
		break;

	case BT_QBLISSA9:
	case BT_QBLISSA9R2:
	case BT_ARMSTONEA9R2:
	case BT_ARMSTONEA9R4:
		valid_mask = (1 << port_lvds0) | (1 << port_lvds1)
			| (1 << port_hdmi);
		default_port = port_lvds0;
		break;

	case BT_NETDCUA9:
		valid_mask = (1 << port_lcd) | (1 << port_lvds0);
		default_port = port_lcd;
		break;

	case BT_PICOMODA9:
		valid_mask = (1 << port_lcd) | (1 << port_lvds0)
			| (1 << port_hdmi);
		default_port = port_lvds0;
		break;

	default:			/* No displays */
		return 1;
	}

	/*
	 * Check if user has defined a display. If not, return 1, i.e. skip
	 * display activation. If yes, activate display.
	 */
	return fs_disp_register(display_ports, valid_mask, default_port);
}
#endif /* CONFIG_VIDEO_IPUV3 */

#ifdef CONFIG_USB_EHCI_MX6
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
 *    efusA9              EIM_D22 (GPIO3_IO22)(*)  ENET_RX_ER (GPIO1_IO24)
 *    efusA9r2            EIM_D22 (GPIO3_IO22)(*)  ENET_RX_ER (GPIO1_IO24)
 *    armStoneA9          (always on)              ENET_RX_ER (GPIO1_IO24)
 *    armStoneA9r2        EIM_D22 (GPIO3_IO22)     ENET_RX_ER (GPIO1_IO24)
 *    QBlissA9  Rev<1.30  (always on)              ENET_RX_ER (GPIO1_IO24)
 *              other     EIM_D22 (GPIO3_IO22)(*)  ENET_RX_ER (GPIO1_IO24)
 *    QBlissA9r2          EIM_D22 (GPIO3_IO22)(*)  ENET_RX_ER (GPIO1_IO24)
 *    PicoMODA9           GPIO_8 (GPIO1_IO08)      GPIO_1 (GPIO1_IO01)(fix)
 *    NetDCUA9            GPIO_9 (GPIO1_IO09)      (not available)
 *
 * (*) Signal on SKIT is active low, usually USB_OTG_PWR is active high
 *
 * USB1 is a host-only port (USB_H1). It is used on all boards. Some boards
 * may have an additional USB hub with a reset signal connected to this port.
 *
 *    Board               USB_H1_PWR               Hub Reset
 *    -------------------------------------------------------------------------
 *    efusA9              EIM_D31 (GPIO3_IO31)     (Hub on SKIT, no reset line)
 *    efusA9r2            EIM_D31 (GPIO3_IO31)     (Hub on SKIT, no reset line)
 *    armStoneA9          (always on)              GPIO_17 (GPIO7_IO12)
 *    armStoneA9r2        (always on)              EIM_EB1 (GPIO2_IO29)
 *    QBlissA9            (always on)              GPIO_17 (GPIO7_IO12)
 *    QBlissA9r2          (always on)              EIM_DA15 (GPIO3_IO15)
 *    PicoMODA9 Rev<1.10  GPIO_17 (GPIO7_IO12)     (Hub on SKIT, no reset line)
 *              other     GPIO_5 (GPIO1_IO05)      (Hub on SKIT, no reset line)
 *    NetDCUA9            GPIO_0 (GPIO1_IO00)      (no Hub)
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
#define USE_USBNC_PWR

/* Some boards have access to the USB_OTG_ID pin to check host/device mode */
static iomux_v3_cfg_t const usb_otg_id_pad_picomoda9[] = {
	IOMUX_PADS(PAD_GPIO_1__GPIO1_IO01 | MUX_PAD_CTRL(USB_ID_PAD_CTRL)),
};

static iomux_v3_cfg_t const usb_otg_id_pad_other[] = {
	IOMUX_PADS(PAD_ENET_RX_ER__GPIO1_IO24 | MUX_PAD_CTRL(USB_ID_PAD_CTRL)),
};

/* Some boards can switch the USB OTG power when configured as host */
static iomux_v3_cfg_t const usb_otg_pwr_pad_picomoda9[] = {
	IOMUX_PADS(PAD_GPIO_8__GPIO1_IO08 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const usb_otg_pwr_pad_netdcua9[] = {
	IOMUX_PADS(PAD_GPIO_9__GPIO1_IO09 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const usb_otg_pwr_pad_other[] = {
#ifdef CONFIG_FS_USB_PWR_USBNC
	IOMUX_PADS(PAD_EIM_D22__USB_OTG_PWR | MUX_PAD_CTRL(NO_PAD_CTRL)),
#else
	IOMUX_PADS(PAD_EIM_D22__GPIO3_IO22 | MUX_PAD_CTRL(NO_PAD_CTRL)),
#endif
};

/* Some boards can switch the USB Host power */
static iomux_v3_cfg_t const usb_h1_pwr_pad_efusa9[] = {
#ifdef CONFIG_FS_USB_PWR_USBNC
	IOMUX_PADS(PAD_EIM_D31__USB_H1_PWR | MUX_PAD_CTRL(NO_PAD_CTRL)),
#else
	IOMUX_PADS(PAD_EIM_D31__GPIO3_IO31 | MUX_PAD_CTRL(NO_PAD_CTRL)),
#endif
};

static iomux_v3_cfg_t const usb_h1_pwr_pad_picomoda9_REV100[] = {
	IOMUX_PADS(PAD_GPIO_17__GPIO7_IO12 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const usb_h1_pwr_pad_picomoda9[] = {
	IOMUX_PADS(PAD_GPIO_5__GPIO1_IO05 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const usb_h1_pwr_pad_netdcua9[] = {
#ifdef CONFIG_FS_USB_PWR_USBNC
	IOMUX_PADS(PAD_GPIO_0__USB_H1_PWR | MUX_PAD_CTRL(NO_PAD_CTRL)),
#else
	IOMUX_PADS(PAD_GPIO_0__GPIO1_IO00 | MUX_PAD_CTRL(NO_PAD_CTRL)),
#endif
};

/* Some boards have an on-board USB hub with a reset signal */
static iomux_v3_cfg_t const usb_hub_reset_pad_armstonea9_qblissa9[] = {
	IOMUX_PADS(PAD_GPIO_17__GPIO7_IO12 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const usb_hub_reset_pad_armstonea9r2[] = {
	IOMUX_PADS(PAD_EIM_EB1__GPIO2_IO29 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const usb_hub_reset_pad_qblissa9r2[] = {
	IOMUX_PADS(PAD_EIM_DA15__GPIO3_IO15 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

/* Init one USB port */
int board_ehci_hcd_init(int index)
{
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();
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

	if (index == 0) {		/* USB0: OTG1, DEVICE, optional HOST */
		switch (board_type) {
		/* These boards support optional HOST function on this port */
		case BT_PICOMODA9:	/* PWR active high, ID available */
			cfg.pwr_pad = usb_otg_pwr_pad_picomoda9;
			cfg.pwr_gpio = IMX_GPIO_NR(1, 8); /* GPIO only */
			cfg.id_pad = usb_otg_id_pad_picomoda9;
			cfg.id_gpio = IMX_GPIO_NR(1, 1);
			break;
		case BT_EFUSA9:
		case BT_ARMSTONEA9R2:
		case BT_ARMSTONEA9R4:
		case BT_QBLISSA9R2:
		case BT_EFUSA9R2:	/* PWR active low, ID available */
			cfg.pwr_pol = 1;
			cfg.pwr_pad = usb_otg_pwr_pad_other;
#ifndef CONFIG_FS_USB_PWR_USBNC
			cfg.pwr_gpio = IMX_GPIO_NR(3, 22);
#endif
			cfg.id_pad = usb_otg_id_pad_other;
			cfg.id_gpio = IMX_GPIO_NR(1, 24);
			break;
		case BT_QBLISSA9:	/* PWR active low, ID available */
			if (board_rev >= 130) {
				/* PWR always on before board revision 1.30 */
				cfg.pwr_pol = 1;
				cfg.pwr_pad = usb_otg_pwr_pad_other;
#ifndef CONFIG_FS_USB_PWR_USBNC
				cfg.pwr_gpio = IMX_GPIO_NR(3, 22);
#endif
			}
			cfg.id_pad = usb_otg_id_pad_other;
			cfg.id_gpio = IMX_GPIO_NR(1, 24);
			break;
		case BT_ARMSTONEA9:	/* PWR always on, ID available */
		case BT_ARMSTONEA9R3:
			cfg.id_pad = usb_otg_id_pad_other;
			cfg.id_gpio = IMX_GPIO_NR(1, 24);
			break;
		case BT_NETDCUA9:	/* PWR active high, no ID */
			cfg.pwr_pad = usb_otg_pwr_pad_netdcua9;
			cfg.pwr_gpio = IMX_GPIO_NR(1, 9); /* GPIO only */
			break;

		/* These boards have only DEVICE function on this port */
		default:
			cfg.mode = FS_USB_DEVICE;
			break;
		}
	} else {			/* USB1: OTG2, HOST only */
		cfg.mode = FS_USB_HOST;

		switch (board_type) {
		case BT_PICOMODA9:	/* PWR active high, no USB hub */
			if (board_rev < 110) {
				cfg.pwr_pad = usb_h1_pwr_pad_picomoda9_REV100;
				cfg.pwr_gpio = IMX_GPIO_NR(7, 12);
			} else {
				cfg.pwr_pad = usb_h1_pwr_pad_picomoda9;
				cfg.pwr_gpio = IMX_GPIO_NR(1, 5);
			}
			break;
		case BT_EFUSA9:
		case BT_EFUSA9R2:	/* PWR active high, no RESET for hub */
			cfg.pwr_pad = usb_h1_pwr_pad_efusa9;
#ifndef CONFIG_FS_USB_PWR_USBNC
			cfg.pwr_gpio = IMX_GPIO_NR(3, 31);
#endif
			break;
		case BT_ARMSTONEA9:
		case BT_ARMSTONEA9R3:
		case BT_QBLISSA9:	/* PWR always on, RESET for USB hub */
			cfg.reset_pad = usb_hub_reset_pad_armstonea9_qblissa9;
			cfg.reset_gpio = IMX_GPIO_NR(7, 12);
			break;
		case BT_ARMSTONEA9R2:	/* PWR always on, RESET for USB hub */
		case BT_ARMSTONEA9R4:
			cfg.reset_pad = usb_hub_reset_pad_armstonea9r2;
			cfg.reset_gpio = IMX_GPIO_NR(2, 29);
			break;
		case BT_QBLISSA9R2:	/* PWR always on, RESET for USB hub */
			cfg.reset_pad = usb_hub_reset_pad_qblissa9r2;
			cfg.reset_gpio = IMX_GPIO_NR(3, 15);
			break;
		case BT_NETDCUA9:	/* PWR active high, no USB hub */
			cfg.pwr_pad = usb_h1_pwr_pad_netdcua9;
#ifndef CONFIG_FS_USB_PWR_USBNC
			cfg.pwr_gpio = IMX_GPIO_NR(1, 0);
#endif
			break;
		default:		/* PWR always on, no USB hub */
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

#ifdef CONFIG_VIDEO_IPUV3
	/* Enable backlight for displays */
	fs_disp_set_backlight_all(1);
#endif

	return 0;
}
#endif /* CONFIG_BOARD_LATE_INIT */

#ifdef CONFIG_CMD_NET
static iomux_v3_cfg_t const eim_pads_eth_b[] = {
	/* AX88796B Ethernet 2 */
	IOMUX_PADS(PAD_EIM_OE__EIM_OE_B | MUX_PAD_CTRL(EIM_NO_PULL)),
	IOMUX_PADS(PAD_EIM_CS1__EIM_CS1_B | MUX_PAD_CTRL(EIM_NO_PULL)),
	IOMUX_PADS(PAD_EIM_RW__EIM_RW | MUX_PAD_CTRL(EIM_NO_PULL)),
	/* AX88796B Ethernet 2 IRQ */
	IOMUX_PADS(PAD_EIM_DA14__GPIO3_IO14 | MUX_PAD_CTRL(EIM_PULL_DOWN)),
	IOMUX_PADS(PAD_EIM_DA15__GPIO3_IO15 | MUX_PAD_CTRL(EIM_PULL_DOWN)),
	/* AX88796B Ethernet 2 RESET */
	IOMUX_PADS(PAD_GPIO_3__GPIO1_IO03 | MUX_PAD_CTRL(EIM_PULL_UP)),
	/* AX88796B Ethernet 2 - EIM_A */
	IOMUX_PADS(PAD_EIM_DA0__EIM_AD00 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_DA1__EIM_AD01 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_DA2__EIM_AD02 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_DA3__EIM_AD03 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_DA4__EIM_AD04 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_DA12__EIM_AD12 | MUX_PAD_CTRL(EIM_PULL_UP)),
	/* AX88796B Ethernet 2 - EIM_D & PIFDATA */
	IOMUX_PADS(PAD_EIM_D16__EIM_DATA16 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_D17__EIM_DATA17 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_D18__EIM_DATA18 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_D19__EIM_DATA19 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_D20__EIM_DATA20 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_D21__EIM_DATA21 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_D22__EIM_DATA22 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_D23__EIM_DATA23 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_D24__EIM_DATA24 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_D25__EIM_DATA25 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_D26__EIM_DATA26 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_D27__EIM_DATA27 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_D28__EIM_DATA28 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_D29__EIM_DATA29 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_D30__EIM_DATA30 | MUX_PAD_CTRL(EIM_PULL_UP)),
	IOMUX_PADS(PAD_EIM_D31__EIM_DATA31 | MUX_PAD_CTRL(EIM_PULL_UP)),
};

/* The second ethernet controller is attached via EIM */
void setup_weim(bd_t *bis)
{
	struct weim *weim = (struct weim *)WEIM_BASE_ADDR;
	struct iomuxc *iomux_regs = (struct iomuxc *)IOMUXC_BASE_ADDR;
	u32 gpr1;

	SETUP_IOMUX_PADS(eim_pads_eth_b);

	/* Enable EIM clock */
	enable_eim_clk(1);

	/*
	 * Set EIM chip select configuration:
	 *
	 *   CS0: 0x08000000 - 0x08FFFFFF (64MB)
	 *   CS1: 0x0C000000 - 0x0FFFFFFF (64MB)
	 *   CS2, CS3: unused
	 *
	 * As these are three bits per chip select, use octal numbers!
	 */
	gpr1 = readl(&iomux_regs->gpr[1]);
	gpr1 &= ~07777;
	gpr1 |= 00033;
	writel(gpr1, &iomux_regs->gpr[1]);

	/* AX88796B is connected to CS1 of EIM */
	writel(0x00020001, &weim->cs1gcr1);
	writel(0x00000000, &weim->cs1gcr2);
	writel(0x16000202, &weim->cs1rcr1);
	writel(0x00000002, &weim->cs1rcr2);
	writel(0x16002082, &weim->cs1wcr1);
	writel(0x00000000, &weim->cs1wcr2);
}

/* enet pads definition */
static iomux_v3_cfg_t const enet_pads_rgmii[] = {
	IOMUX_PADS(PAD_ENET_MDIO__ENET_MDIO | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_MDC__ENET_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_REF_CLK__ENET_TX_CLK | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_TXC__RGMII_TXC | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_TD0__RGMII_TD0 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_TD1__RGMII_TD1 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_TD2__RGMII_TD2 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_TD3__RGMII_TD3 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_TX_CTL__RGMII_TX_CTL | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_RXC__RGMII_RXC | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_RD0__RGMII_RD0 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_RD1__RGMII_RD1 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_RD2__RGMII_RD2 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_RD3__RGMII_RD3 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_RX_CTL__RGMII_RX_CTL | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	/* Phy Interrupt */
	IOMUX_PADS(PAD_GPIO_19__GPIO4_IO05 | MUX_PAD_CTRL(NO_PAD_CTRL)),
	/* Phy Reset */
	IOMUX_PADS(PAD_ENET_CRS_DV__GPIO1_IO25 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const enet_pads_rmii_picomoda9[] = {
	IOMUX_PADS(PAD_ENET_MDIO__ENET_MDIO | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_MDC__ENET_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_CRS_DV__ENET_RX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_RX_ER__ENET_RX_ER | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_TX_EN__ENET_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_RXD0__ENET_RX_DATA0 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_RXD1__ENET_RX_DATA1 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_TXD0__ENET_TX_DATA0 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_TXD1__ENET_TX_DATA1 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_TX_CTL__ENET_REF_CLK | MUX_PAD_CTRL(ENET_PAD_CTRL)),

	/* Phy Reset */
	IOMUX_PADS(PAD_SD4_DAT2__GPIO2_IO10 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const enet_pads_rmii_netdcua9[] = {
	IOMUX_PADS(PAD_ENET_MDIO__ENET_MDIO | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_MDC__ENET_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_CRS_DV__ENET_RX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_RX_ER__ENET_RX_ER | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_TX_EN__ENET_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_RXD0__ENET_RX_DATA0 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_RXD1__ENET_RX_DATA1 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_TXD0__ENET_TX_DATA0 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET_TXD1__ENET_TX_DATA1 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII_TX_CTL__ENET_REF_CLK | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_GPIO_16__ENET_REF_CLK | MUX_PAD_CTRL(ENET_PAD_CTRL)),

	/* Phy Reset */
	IOMUX_PADS(PAD_GPIO_2__GPIO1_IO02 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

int board_eth_init(bd_t *bis)
{
	u32 gpr1;
	int ret;
	int phy_addr;
	int reset_gpio;
	enum xceiver_type xcv_type;
	enum enet_freq freq;
	phy_interface_t if_mode = PHY_INTERFACE_MODE_RGMII;
	struct iomuxc *iomux_regs = (struct iomuxc *)IOMUXC_BASE_ADDR;
	int id = 0;
	unsigned int board_type = fs_board_get_type();
	unsigned int features2 = fs_board_get_nboot_args()->chFeatures2;

	/* Activate on-chip ethernet port (FEC) */
	if (features2 & FEAT2_ETH_A) {
		fs_eth_set_ethaddr(id);

		switch (board_type) {
		case BT_PICOMODA9:
		case BT_NETDCUA9:
			/* Use 100 MBit/s LAN on RMII pins */
			if (board_type == BT_PICOMODA9)
				SETUP_IOMUX_PADS(enet_pads_rmii_picomoda9);
			else
				SETUP_IOMUX_PADS(enet_pads_rmii_netdcua9);

			/* ENET CLK is generated in i.MX6 and is an output */
			gpr1 = readl(&iomux_regs->gpr[1]);
			gpr1 |= IOMUXC_GPR1_ENET_CLK_SEL_MASK;
			writel(gpr1, &iomux_regs->gpr[1]);

			freq = ENET_50MHZ;
			break;

		default:
			/* Use 1 GBit/s LAN on RGMII pins */
			SETUP_IOMUX_PADS(enet_pads_rgmii);

			/* ENET CLK is generated in PHY and is an input */
			gpr1 = readl(&iomux_regs->gpr[1]);
			gpr1 &= ~IOMUXC_GPR1_ENET_CLK_SEL_MASK;
			writel(gpr1, &iomux_regs->gpr[1]);

			freq = ENET_25MHZ;
			break;
		}

		/* Activate ENET PLL */
		ret = enable_fec_anatop_clock(0, freq);
		if (ret < 0)
			return ret;

		/* Enable ENET clock */
		enable_enet_clk(1);

		/* Reset the PHY */
		switch (board_type) {
		case BT_PICOMODA9:
		case BT_NETDCUA9:
			/*
			 * DP83484: This PHY needs at least 1us reset pulse
			 * width. After power on it needs min 167ms (after
			 * reset is deasserted) before the first MDIO access
			 * can be done. In a warm start, it only takes around
			 * 3us for this. As we do not know whether this is a
			 * cold or warm start, we must assume the worst case.
			 */
			if (board_type == BT_PICOMODA9)
				reset_gpio = IMX_GPIO_NR(2, 10);
			else
				reset_gpio = IMX_GPIO_NR(1, 2);
			fs_board_issue_reset(10, 170000, reset_gpio, ~0, ~0);
			phy_addr = 1;
			xcv_type = RMII;
			break;
		case BT_EFUSA9R2:
		case BT_ARMSTONEA9R3:
		case BT_ARMSTONEA9R4:
			/* Realtek RTL8211F(D): Assert reset for at least 10ms */
			fs_board_issue_reset(10000, 50000, IMX_GPIO_NR(1, 25),
					     ~0,~0);
			phy_addr = 4;
			xcv_type = RGMII;
			if_mode = PHY_INTERFACE_MODE_RGMII_ID;
			break;
		default:
			/* Atheros AR8035: Assert reset for at least 1ms */
			fs_board_issue_reset(1000, 1000, IMX_GPIO_NR(1, 25),
					     ~0,~0);
			phy_addr = 4;
			xcv_type = RGMII;
			break;
		}

		ret = fecmxc_initialize_multi_type_if_mode(bis, -1, phy_addr,
						   ENET_BASE_ADDR, xcv_type, if_mode);
		if (ret < 0)
			return ret;

		id++;
	}

	/* If available, activate external ethernet port (AX88796B) */
	if (features2 & FEAT2_ETH_B) {
		/* AX88796B is connected via EIM */
		setup_weim(bis);

		/* Reset AX88796B, on NetDCUA9 */
		fs_board_issue_reset(200, 1000, IMX_GPIO_NR(1, 3), ~0, ~0);

		/* Initialize AX88796B */
		ret = ax88796_initialize(-1, CONFIG_DRIVER_AX88796_BASE,
					 AX88796_MODE_BUS16_DP16);

		fs_eth_set_ethaddr(id++);
	}

	/* If WLAN is available, just set ethaddr variable */
	if (features2 & FEAT2_WLAN)
		fs_eth_set_ethaddr(id++);

	return 0;
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
		case BT_EFUSA9R2:
		case BT_ARMSTONEA9R4:
			phy_write(phydev, MDIO_DEVAD_NONE, MIIM_RTL8211F_PAGE_SELECT, 0xd04);
			phy_write(phydev, MDIO_DEVAD_NONE, 0x10, LED_MODE_B | LED_LINK(2) | LED_ACT(1));
			phy_write(phydev, MDIO_DEVAD_NONE, MIIM_RTL8211F_PAGE_SELECT, 0x0);
			break;
		case BT_ARMSTONEA9R3:
			phy_write(phydev, MDIO_DEVAD_NONE, MIIM_RTL8211F_PAGE_SELECT, 0xd04);
			phy_write(phydev, MDIO_DEVAD_NONE, 0x10, LED_MODE_B | LED_LINK(0) | LED_ACT(1));
			phy_write(phydev, MDIO_DEVAD_NONE, MIIM_RTL8211F_PAGE_SELECT, 0x0);
			break;
		}
	}

	return 0;
}
#endif /* CONFIG_CMD_NET */

#ifdef CONFIG_LED_STATUS_CMD
/*
 * Boards                             STA1           STA2         Active
 * ------------------------------------------------------------------------
 * PicoMODA9 (Rev 1.00)  extern       GPIO5_IO04     GPIO5_IO02   low
 * PicoMODA9 (newer)     extern       GPIO7_IO12     GPIO7_IO13   low
 *                       on-board     GPIO5_IO28     GPIO5_IO29   low
 * armStoneA9, QBlissA9               GPIO4_IO06     GPIO4_IO07   high
 * efusA9/r2,armStoneA9r2,QBlissA9r2  GPIO7_IO12     GPIO7_IO13   high
 */
static unsigned int led_value;

static unsigned int get_led_gpio(led_id_t id, int val, int index)
{
	unsigned int gpio;
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();

	if (val)
		led_value |= (1 << id);
	else
		led_value &= ~(1 << id);

	switch (board_type) {
	case BT_PICOMODA9:
		if (board_rev == 100)
			gpio = (id ? IMX_GPIO_NR(5, 2) : IMX_GPIO_NR(5, 4));
		else if (!index)
			gpio = (id ? IMX_GPIO_NR(7, 13) : IMX_GPIO_NR(7, 12));
		else
			gpio = (id ? IMX_GPIO_NR(5, 29) : IMX_GPIO_NR(5, 28));
		break;

	case BT_ARMSTONEA9:
	case BT_ARMSTONEA9R3:
	case BT_QBLISSA9:
		gpio = (id ? IMX_GPIO_NR(4, 7) : IMX_GPIO_NR(4, 6));
		break;

	case BT_EFUSA9:
	case BT_NETDCUA9:
	case BT_ARMSTONEA9R2:
	case BT_ARMSTONEA9R4:
	case BT_QBLISSA9R2:
	case BT_EFUSA9R2:
	default:			/* efusA9/r2, armStoneA9r2, NetDCUA9 */
		gpio = (id ? IMX_GPIO_NR(7, 13) : IMX_GPIO_NR(7, 12));
		break;
	}

	return gpio;
}

void coloured_LED_init(void)
{
	/* This is called after code relocation in arch/arm/lib/crt0.S but
	   before board_init_r() and therefore before the NBoot args struct is
	   copied to fs_nboot_args in board_init(). So variables in RAM are OK
	   (like led_value above), but no fs_nboot_args yet. */
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();
	int val = (board_type == BT_PICOMODA9);

	if (val && (board_rev > 100)) {
		gpio_direction_output(get_led_gpio(0, val, 1), val);
		gpio_direction_output(get_led_gpio(1, val, 1), val);
	}
	gpio_direction_output(get_led_gpio(0, val, 0), val);
	gpio_direction_output(get_led_gpio(1, val, 0), val);
}

void __led_set(led_id_t id, int val)
{
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();

	if (id > 1)
		return;

	if (board_type == BT_PICOMODA9) {
		val = !val;
		if (board_rev > 100)
			gpio_set_value(get_led_gpio(id, val, 1), val);
	}

	gpio_set_value(get_led_gpio(id, val, 0), val);
}

void __led_toggle(led_id_t id)
{
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();
	int val;

	if (id > 1)
		return;

	val = !((led_value >> id) & 1);
	if ((board_type == BT_PICOMODA9) && (board_rev > 100))
		gpio_set_value(get_led_gpio(id, val, 1), val);
	gpio_set_value(get_led_gpio(id, val, 0), val);
}
#endif /* CONFIG_LED_STATUS_CMD */

#ifdef CONFIG_OF_BOARD_SETUP
/* Do any additional board-specific device tree modifications */
int ft_board_setup(void *fdt, bd_t *bd)
{
	int offs,err;
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();

	printf("   Setting run-time properties\n");

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
		if (pargs->chFeatures2 & FEAT2_WLAN)
			fs_fdt_set_macaddr(fdt, offs, id++);
	}

	/* Disable ethernet node(s) if feature is not available */
	if (!(pargs->chFeatures2 & FEAT2_ETH_A)){
		err = fs_fdt_enable(fdt, FDT_ETH_A, 0);
		if(err){
			printf("   Trying legacy path\n");
			fs_fdt_enable(fdt, FDT_ETH_A_LEGACY, 0);
		}
	}
#if 0
	if (!(pargs->chFeatures2 & FEAT2_ETH_B))
		fs_fdt_enable(fdt, FDT_ETH_B, 0);
#endif

	return 0;
}
#endif /* CONFIG_OF_BOARD_SETUP */

/* Board specific cleanup before Linux is started */
void board_preboot_os(void)
{
#ifdef CONFIG_VIDEO_IPUV3
	/* Switch off backlight and display voltages, IPU: arch_preboot_os() */
	/* ### TODO: In the future the display should pass smoothly to Linux,
	   then switching everything off should not be necessary anymore. */
	fs_disp_set_backlight_all(0);
	fs_disp_set_power_all(0);
#endif

	/* Shut down all ethernet PHYs (suspend mode) */
	mdio_shutdown_all();
}
