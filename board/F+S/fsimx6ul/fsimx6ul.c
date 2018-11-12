/*
 * fsimx6ul.c
 *
 * (C) Copyright 2015
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Board specific functions for F&S boards based on Freescale i.MX6 CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/errno.h>
#ifdef CONFIG_CMD_NET
#include <miiphy.h>
#include <netdev.h>
#include "../common/fs_eth_common.h"	/* fs_eth_*() */
#endif
#ifdef CONFIG_CMD_LCD
#include <cmd_lcd.h>			/* PON_*, POFF_* */
#endif
#include <serial.h>			/* struct serial_device */

#ifdef CONFIG_GENERIC_MMC
#include <mmc.h>
#include <fsl_esdhc.h>			/* fsl_esdhc_initialize(), ... */
#include "../common/fs_mmc_common.h"	/* struct fs_mmc_cd, fs_mmc_*(), ... */
#endif

#ifdef CONFIG_CMD_LED
#include <status_led.h>			/* led_id_t */
#endif

#ifdef CONFIG_VIDEO_MXS
#include <linux/fb.h>
#include <mxsfb.h>
#endif

#include <asm/imx-common/video.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/iomux.h>
#include <asm/imx-common/iomux-v3.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/crm_regs.h>		/* CCM_CCGR1, nandf clock settings */
#include <asm/arch/clock.h>		/* enable_fec_anatop_clock(), ... */

#include <linux/mtd/nand.h>		/* struct mtd_info, struct nand_chip */
#include <mtd/mxs_nand_fus.h>		/* struct mxs_nand_fus_platform_data */
#include <usb.h>			/* USB_INIT_HOST, USB_INIT_DEVICE */
#include <malloc.h>			/* free() */
#include <i2c.h>			/* i2c_reg_read/write(), ... */
#include <asm/imx-common/mxc_i2c.h>
#include <fdt_support.h>		/* do_fixup_by_path_u32(), ... */
#include "../common/fs_fdt_common.h"	/* fs_fdt_set_val(), ... */
#include "../common/fs_board_common.h"	/* fs_board_*() */
#include "../common/fs_usb_common.h"	/* struct fs_usb_port_cfg, fs_usb_*() */

/* ------------------------------------------------------------------------- */

#define BT_EFUSA7UL   0
#define BT_CUBEA7UL   1
#define BT_PICOCOM1_2 2
#define BT_CUBE2_0    3
#define BT_GAR1       4
#define BT_PICOCOMA7  5
#define BT_PCOREMX6UL 6

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
#define FEAT2_DEFAULT (FEAT2_ETH_A | FEAT2_ETH_B | FEAT2_EMMC | FEAT2_WLAN)

/* Maximum speed (in kHz) if FEAT2_SPEED is set */
#define SPEED_LIMIT	528000

/* Device tree paths */
#define FDT_NAND	"/soc/gpmi-nand@01806000"
#define FDT_ETH_A	"/soc/aips-bus@02100000/ethernet@02188000"
#define FDT_ETH_B	"/soc/aips-bus@02000000/ethernet@020b4000"
#define FDT_CPU0	"/cpus/cpu@0"

/* IO expander bits (on efusA9UL since board rev. 1.20) */
#define IOEXP_RESET_WLAN (1 << 0)
#define IOEXP_RESET_OUT  (1 << 1)
#define IOEXP_RESET_LVDS (1 << 2)
#define IOEXP_USB_H1_PWR (1 << 3)
#define IOEXP_ALL_RESETS (IOEXP_RESET_WLAN | IOEXP_RESET_OUT | IOEXP_RESET_LVDS)

#define UART_PAD_CTRL  (PAD_CTL_PUS_100K_UP |			\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm |			\
	PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define ENET_PAD_CTRL  (PAD_CTL_PUS_100K_UP | PAD_CTL_PUE |     \
        PAD_CTL_SPEED_HIGH   |                                   \
        PAD_CTL_DSE_48ohm   | PAD_CTL_SRE_FAST)

#define MDIO_PAD_CTRL  (PAD_CTL_PUS_100K_UP | PAD_CTL_PUE |     \
        PAD_CTL_DSE_48ohm   | PAD_CTL_SRE_FAST | PAD_CTL_ODE)

#define ENET_CLK_PAD_CTRL  (PAD_CTL_DSE_40ohm   | PAD_CTL_SRE_FAST)

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

#define USB_ID_PAD_CTRL (PAD_CTL_PUS_47K_UP | PAD_CTL_SPEED_LOW | PAD_CTL_HYS)

#define I2C_PAD_CTRL  (PAD_CTL_PUS_22K_UP | PAD_CTL_SPEED_LOW	\
	| PAD_CTL_DSE_40ohm | PAD_CTL_HYS | PAD_CTL_SRE_SLOW)

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
#if defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
#define EARLY_USB "1"
#else
#define EARLY_USB NULL
#endif

const struct fs_board_info board_info[8] = {
	{	/* 0 (BT_EFUSA7UL) */
		.name = "efusA7UL",
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.earlyusbinit = NULL,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_init",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
	},
	{	/* 1 (BT_CUBEA7UL) */
		.name = "CubeA7UL",
		.bootdelay = "3",
		.updatecheck = "TargetFS.ubi(ubi0:data)",
		.installcheck = INSTALL_RAM,
		.recovercheck = "TargetFS.ubi(ubi0:recovery)",
		.earlyusbinit = NULL,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_ubionly",
		.network = ".network_off",
		.init = ".init_init",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_ubifs",
		.fdt = ".fdt_ubifs",
	},
	{	/* 2 (BT_PICOCOM1_2) */
		.name = "PicoCOM1.2",
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.earlyusbinit = NULL,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_init",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
	},
	{	/* 3 (BT_CUBE2_0) */
		.name = "Cube2.0",
		.bootdelay = "3",
		.updatecheck = "TargetFS.ubi(ubi0:data)",
		.installcheck = INSTALL_RAM,
		.recovercheck = "TargetFS.ubi(ubi0:recovery)",
		.earlyusbinit = NULL,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_ubionly",
		.network = ".network_off",
		.init = ".init_init",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_ubifs",
		.fdt = ".fdt_ubifs",
	},
	{	/* 4 (BT_GAR1) */
		.name = "GAR1",
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.earlyusbinit = NULL,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_init",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
	},
	{	/* 5 (PICOCOMA7) */
		.name = "PicoCOMA7",
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.earlyusbinit = NULL,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_init",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
	},
	{	/* 6 (BT_PCOREMX6UL) */
		.name = "PicoCoreMX6UL",
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.earlyusbinit = NULL,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_init",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
	},
	{	/* 7 (unknown) */
		.name = "unknown",
	},
};

/* ---- Stage 'f': RAM not valid, variables can *not* be used yet ---------- */

/* Pads for 18-bit LCD interface */
static iomux_v3_cfg_t const lcd18_pads_low[] = {
	IOMUX_PADS(PAD_LCD_CLK__GPIO3_IO00    | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_ENABLE__GPIO3_IO01 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_HSYNC__GPIO3_IO02  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_VSYNC__GPIO3_IO03  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA00__GPIO3_IO05 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA01__GPIO3_IO06 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA02__GPIO3_IO07 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA03__GPIO3_IO08 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA04__GPIO3_IO09 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA05__GPIO3_IO10 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA06__GPIO3_IO11 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA07__GPIO3_IO12 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA08__GPIO3_IO13 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA09__GPIO3_IO14 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA10__GPIO3_IO15 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA11__GPIO3_IO16 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA12__GPIO3_IO17 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA13__GPIO3_IO18 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA14__GPIO3_IO19 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA15__GPIO3_IO20 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA16__GPIO3_IO21 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA17__GPIO3_IO22 | MUX_PAD_CTRL(0x3010)),
};

/* Pads for VLCD_ON and VCFL_ON: active high -> pull-down to switch off */
static iomux_v3_cfg_t const lcd_extra_pads_ull[] = {
	MX6ULL_PAD_SNVS_TAMPER4__GPIO5_IO04 | MUX_PAD_CTRL(0x3010),
	//MX6ULL_PAD_SNVS_TAMPER5__GPIO5_IO05 | MUX_PAD_CTRL(0x3010),
};

static iomux_v3_cfg_t const lcd_extra_pads_picocoma7ull[] = {
	IOMUX_PADS(PAD_LCD_DATA20__GPIO3_IO25 | MUX_PAD_CTRL(0x3010)),
	//MX6ULL_PAD_SNVS_TAMPER5__GPIO5_IO05 | MUX_PAD_CTRL(0x3010),
};

static iomux_v3_cfg_t const lcd_extra_pads_ul[] = {
	MX6UL_PAD_SNVS_TAMPER4__GPIO5_IO04 | MUX_PAD_CTRL(0x3010),
	//MX6UL_PAD_SNVS_TAMPER5__GPIO5_IO05 | MUX_PAD_CTRL(0x3010),
};


/* DVS on efusA7UL (since board rev 1.10), PicoCOM1.2 (since rev 1.00) and PicoCOMA7 */
static iomux_v3_cfg_t const dvs[] = {
	IOMUX_PADS(PAD_NAND_DQS__GPIO4_IO16 | MUX_PAD_CTRL(0x3010)),
};

/* GAR1 power off leds */
static iomux_v3_cfg_t const gar1_led_pads[] = {
	IOMUX_PADS(PAD_LCD_DATA00__GPIO3_IO05 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA01__GPIO3_IO06 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA02__GPIO3_IO07 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD_DATA03__GPIO3_IO08 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_GPIO1_IO04__GPIO1_IO04 | MUX_PAD_CTRL(0x0)),
	IOMUX_PADS(PAD_GPIO1_IO05__GPIO1_IO05 | MUX_PAD_CTRL(0x0)),
	IOMUX_PADS(PAD_GPIO1_IO08__GPIO1_IO08 | MUX_PAD_CTRL(0x0)),
	IOMUX_PADS(PAD_GPIO1_IO09__GPIO1_IO09 | MUX_PAD_CTRL(0x0)),
};

int board_early_init_f(void)
{
	/*
	 * Set pull-down resistors on display signals; some displays do not
	 * like high level on data signals when VLCD is not applied yet.
	 *
	 * FIXME: This should actually only happen if display is really in
	 * use, i.e. if device tree activates lcd. However we do not know this
	 * at this point of time.
	 */
	switch (fs_board_get_type())
	{
	case BT_PICOCOM1_2:		/* No LCD, just DVS */
		SETUP_IOMUX_PADS(dvs);
		break;

	case BT_CUBEA7UL:		/* No LCD, no DVS */
	case BT_CUBE2_0:
	default:
		break;

	case BT_GAR1:			/* No LCD, but init other GPIOs */
		SETUP_IOMUX_PADS(gar1_led_pads);
		gpio_direction_input(IMX_GPIO_NR(3, 5));
		gpio_direction_input(IMX_GPIO_NR(3, 6));
		gpio_direction_input(IMX_GPIO_NR(3, 7));
		gpio_direction_input(IMX_GPIO_NR(3, 8));
		gpio_direction_input(IMX_GPIO_NR(1, 4));
		gpio_direction_input(IMX_GPIO_NR(1, 5));
		gpio_direction_input(IMX_GPIO_NR(1, 8));
		gpio_direction_input(IMX_GPIO_NR(1, 9));
		break;

	case BT_EFUSA7UL:		/* 18-bit LCD and DVS */
		SETUP_IOMUX_PADS(lcd18_pads_low);
		if (is_cpu_type(MXC_CPU_MX6ULL))
			SETUP_IOMUX_PADS(lcd_extra_pads_ull);
		else
			SETUP_IOMUX_PADS(lcd_extra_pads_ul);
		SETUP_IOMUX_PADS(dvs);
		break;
	case BT_PICOCOMA7:
		SETUP_IOMUX_PADS(lcd18_pads_low);
		SETUP_IOMUX_PADS(lcd_extra_pads_picocoma7ull);
		SETUP_IOMUX_PADS(dvs);
		break;

	}

	return 0;
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

static iomux_v3_cfg_t const i2c_pads_ul[] = {
	/* SCL */
	MX6UL_PAD_SNVS_TAMPER9__GPIO5_IO09 | MUX_PAD_CTRL(I2C_PAD_CTRL),
	/* SDA */
	MX6UL_PAD_SNVS_TAMPER8__GPIO5_IO08 | MUX_PAD_CTRL(I2C_PAD_CTRL),
};

static iomux_v3_cfg_t const i2c_pads_ull[] = {
	/* SCL */
	MX6ULL_PAD_SNVS_TAMPER9__GPIO5_IO09 | MUX_PAD_CTRL(I2C_PAD_CTRL),
	/* SDA */
	MX6ULL_PAD_SNVS_TAMPER8__GPIO5_IO08 | MUX_PAD_CTRL(I2C_PAD_CTRL),
};

/* Set up I2C signals */
void i2c_init_board(void)
{
	if (is_cpu_type(MXC_CPU_MX6ULL))
		SETUP_IOMUX_PADS(i2c_pads_ull);
	else{
		SETUP_IOMUX_PADS(i2c_pads_ul);
	}
}

/* ---- Stage 'r': RAM valid, U-Boot relocated, variables can be used ------ */

int board_init(void)
{
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();

	/* Copy NBoot args to variables and prepare command prompt string */
	fs_board_init_common(&board_info[board_type]);

	/*
	 * REMARK:
	 * efusA7UL has a generic RESETOUTn signal to reset on-board WLAN
	 * (only board revisions before 1.20), both ethernet PHYs and that is
	 * also available on the efus connector pin 14 and in turn on pin 8 of
	 * the SKIT feature connector. Because ethernet PHYs have to be reset
	 * when the PHY clock is already active, this signal is triggered as
	 * part of the ethernet initialization in board_eth_init(), not here.
	 *
	 * Starting with board revision 1.20, efusA7UL has an external GPIO
	 * expander via I2C and RESET_WLANn, RESETOUTn and RESET_LVDSn can now
	 * be switched indiviually. In additon, USB_H1_PWR can now be switched,
	 * too.
	 *
	 * A similar issue exists for PicoCOM1.2. RESETOUTn is also triggered
	 * in board_eth_init().
	 */
	if ((board_type == BT_EFUSA7UL) && (board_rev >= 120)) {
		uint8_t val;

		/*
		 * Set all IO expander port bits to output, assert RESET_WLANn,
		 * RESET_LVDSn, RESETOUTn and de-assert them after 1ms again.
		 *
		 * ### TODO: Implement switching of USB_H1_PWR. For now it is
		 * simply set to "on", like on previous board revisions.
		 */
		i2c_set_bus_num(0);
		val = ~IOEXP_ALL_RESETS;
		i2c_reg_write(0x41, 1, val);
		i2c_reg_write(0x41, 2, 0);
		i2c_reg_write(0x41, 3, 0);
		udelay(1000);
		val |= IOEXP_ALL_RESETS;
		i2c_reg_write(0x41, 1, val);
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

#ifdef CONFIG_GENERIC_MMC
/*
 * SD/MMC support.
 *
 *   Board          USDHC   CD-Pin                 Slot
 *   -----------------------------------------------------------------------
 *   efusA7UL (for CD signal see below):
 *        either:   USDHC2  UART1_RTS (GPIO1_IO19) SD_B: Connector (SD)
 *            or:   USDHC2  -                      eMMC (8-Bit)
 *        either:   USDHC1  UART1_RTS (GPIO1_IO19) SD_A: Connector (Micro-SD)
 *            or:  [USDHC1  UART1_RTS (GPIO1_IO19) WLAN]
 *   -----------------------------------------------------------------------
 *   PicoCOM1.2:    USDHC2 [GPIO1_IO19]            Connector (SD)
 *                 [USDHC1  GPIO1_IO03             WLAN]
 *   -----------------------------------------------------------------------
 *   PicoCOMA7:     USDHC1 [GPIO1_IO19]            Connector (SD)
 *   -----------------------------------------------------------------------
 *   PicoCoreMX6UL: USDHC1  GPIO1_IO19             SD_A: Connector (Micro-SD)
 *   -----------------------------------------------------------------------
 *   GAR1:         (no SD/MMC)
 *   -----------------------------------------------------------------------
 *   CubeA7UL:     [USDHC1  -                      WLAN]
 *   -----------------------------------------------------------------------
 *   Cube2.0:      [USDHC1  -                      WLAN]
 *   -----------------------------------------------------------------------
 *
 * Remark: The WP pin is ignored in U-Boot, also WLAN
 *
 * On efusA7UL board rev 1.00, CD/WP pins are only available for SD_A, and
 * only if WLAN is not equipped. If WLAN is equipped, CD/WP signals are not
 * available at all. On newer board revisions, CD/WP are also used for SD_A if
 * WLAN is not equipped. And if both, WLAN and eMMC are equipped, CD/WP
 * signals are still not available at all. But in the case of WLAN equipped
 * and eMMC not equipped, CD/WP are connected be SD_B instead of SD_A.
 */

/* SD/MMC card pads definition, distinguish external from internal ports */
static iomux_v3_cfg_t const usdhc1_sd_pads_ext_rst[] = {
	IOMUX_PADS(PAD_GPIO1_IO09__USDHC1_RESET_B|MUX_PAD_CTRL(USDHC_CLK_INT)),
	IOMUX_PADS(PAD_SD1_CMD__USDHC1_CMD | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD1_CLK__USDHC1_CLK | MUX_PAD_CTRL(USDHC_CLK_EXT)),
	IOMUX_PADS(PAD_SD1_DATA0__USDHC1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD1_DATA1__USDHC1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD1_DATA2__USDHC1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_SD1_DATA3__USDHC1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
};

static iomux_v3_cfg_t const usdhc2_sd_pads_ext[] = {
	IOMUX_PADS(PAD_LCD_DATA18__USDHC2_CMD | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_LCD_DATA19__USDHC2_CLK | MUX_PAD_CTRL(USDHC_CLK_EXT)),
	IOMUX_PADS(PAD_LCD_DATA20__USDHC2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_LCD_DATA21__USDHC2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_LCD_DATA22__USDHC2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
	IOMUX_PADS(PAD_LCD_DATA23__USDHC2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_EXT)),
};

static iomux_v3_cfg_t const usdhc2_sd_pads_int[] = {
	IOMUX_PADS(PAD_LCD_DATA18__USDHC2_CMD | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_LCD_DATA19__USDHC2_CLK | MUX_PAD_CTRL(USDHC_CLK_INT)),
	IOMUX_PADS(PAD_GPIO1_IO09__USDHC2_RESET_B|MUX_PAD_CTRL(USDHC_CLK_INT)),
	IOMUX_PADS(PAD_LCD_DATA20__USDHC2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_LCD_DATA21__USDHC2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_LCD_DATA22__USDHC2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_LCD_DATA23__USDHC2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_NAND_DATA04__USDHC2_DATA4| MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_NAND_DATA05__USDHC2_DATA5| MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_NAND_DATA06__USDHC2_DATA6| MUX_PAD_CTRL(USDHC_PAD_INT)),
	IOMUX_PADS(PAD_NAND_DATA07__USDHC2_DATA7| MUX_PAD_CTRL(USDHC_PAD_INT)),
};

/* CD on pad UART1_RTS */
static iomux_v3_cfg_t const cd_uart1_rts[] = {
	IOMUX_PADS(PAD_UART1_RTS_B__GPIO1_IO19 | MUX_PAD_CTRL(USDHC_CD_CTRL)),
};

enum usdhc_pads {
	usdhc1_ext_rst, usdhc1_ext, usdhc2_ext, usdhc2_int
};

static struct fs_mmc_cfg sdhc_cfg[] = {
		          /* pads,                       count, USDHC# */
	[usdhc1_ext_rst] = { usdhc1_sd_pads_ext_rst,     3,     1 },
	[usdhc1_ext]     = { &usdhc1_sd_pads_ext_rst[1], 2,     1 },
	[usdhc2_ext]     = { usdhc2_sd_pads_ext,         2,     2 },
	[usdhc2_int]     = { usdhc2_sd_pads_int,         3,     2 },
};

enum usdhc_cds {
	gpio1_io19
};

static const struct fs_mmc_cd sdhc_cd[] = {
		      /* pad,          gpio */
	[gpio1_io19] = { cd_uart1_rts, IMX_GPIO_NR(1, 19) },
};

int board_mmc_init(bd_t *bd)
{
	int ret = 0;
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();
	unsigned int features2 = fs_board_get_nboot_args()->chFeatures2;

	switch (board_type) {
	case BT_EFUSA7UL:
		/*
		 * If no eMMC is equipped, port SD_B can be used as mmc0
		 * (USDHC2, ext. SD slot, normal-size SD on efus SKIT); may
		 * use CD on GPIO1_IO19 if SD_A is occupied by WLAN and board
		 * revision is at least 1.10.
		 */
		if (!(features2 & FEAT2_EMMC)) {
			const struct fs_mmc_cd *cd = NULL;

			if ((features2 & FEAT2_WLAN) && (board_rev >= 110))
				cd = &sdhc_cd[gpio1_io19];
			ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc2_ext], cd);
		}
		/*
		 * If no WLAN is equipped, port SD_A with CD on GPIO1_IO19 can
		 * be used (USDHC1, ext. SD slot, micro SD on efus SKIT). This
		 * is either mmc1 if SD_B is available, or mmc0 if not.
		 */
		if (!ret && !(features2 & FEAT2_WLAN))
			ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc1_ext],
					   &sdhc_cd[gpio1_io19]);
		/*
		 * If eMMC is equipped, add it as last mmc port, which is
		 * either mmc1 if SD_A is available, or mmc0 if not. Use as
		 * last mmc, because eMMC is least suited as a source for
		 * update/install, which is by default searched for on mmc0.
		 */
#ifdef CONFIG_CMD_NAND
		/* If NAND is equipped, eMMC can only use buswidth 4 */
		if (!ret && (features2 & FEAT2_EMMC))
			ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc2_int], NULL);
#else
		/* If no NAND is equipped, four additional data lines
		   are available and eMMC can use buswidth 8 */
		if (!ret && (features2 & FEAT2_EMMC))
			ret = fs_mmc_setup(bd, 8, &sdhc_cfg[usdhc2_int], NULL);
#endif
		break;

	case BT_PICOCOM1_2:
		/* mmc0: USDHC2 (ext. SD slot via connector), no CD.
		   Actually the port has a CD if UART1 does not use RTS/CTS,
		   but as we do not know this for sure, skip CD here and
		   assume "always present". */
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc2_ext], NULL);
		break;

	case BT_PICOCOMA7:
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc1_ext], NULL);
		break;

	case BT_PCOREMX6UL:
		/* mmc0: USDHC1 (ext. micro SD slot via connector) */
		ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc1_ext_rst], 
				   &sdhc_cd[gpio1_io19]);
		break;

	default:
		return 0;		/* Neither SD card, nor eMMC */
	}

	return ret;
}
#endif


#ifdef CONFIG_VIDEO_MXS
/*
 * Display initialization sequence:
 *
 * 1.0   board.c: board_init_r(); calls stdio_init();
 * 2.0   stdio.c: stdio_init(); calls drv_video_init();
 *
 * 3.1   cfb_console.c: drv_video_init(); calls board_video_skip();
 *       board_video_skip(); is overriden in our boardfile fsimx6ul.c.
 *       - The original file comes from video.c.
 *       - if this returns non-zero, the display will not be started.
 *
 * 3.11  board_video_skip(); calls
 *       parse_display_params(struct display_params *display, const char *s);
 *       - function parses the dispmode and disppara env vars set in u-boot.
 *
 * 3.12  fsimx6ul.c: board_video_skip(); calls mxs_lcd_panel_setup();
 * 3.121 mxsfb.c: mxs_lcd_panel_setup(struct fb_videomode mode, int bpp,
 *       uint32_t base_addr); calls nothing.
 *       - Map struct, bpp and base_addr to class
 *
 * 3.13  fsimx6ul.c: board_video_skip(); calls enable_lcdif_clock();
 * 3.131 clock.c: enable_lcdif_clock(uint32_t base_addr = LCDIF_BASE_ADDR);
 *       calls nothing.
 *       - clear the pre-mux clock in CSCDR2 register.
 *       - Enable the LCDIF pix clock in CCGR2 and CCGR3 Register.
 *
 * 3.2   cfb_console.c: drv_video_init(); calls video_init();
 * 3.21  cfb_console.c: video_init(); calls video_hw_init();
 *       - Parse display parameters again.
 *       - Allocate, wipe and start the framebuffer.
 *
 * 3.31  mxsfb.b: video_hw_init(); calls mxs_lcd_init();
 *       - Set Databus Width.
 *
 * 3.32  mxsfb.c: mxs_lcd_init(GraphicDevice *panel,
 *       struct ctfb_res_modes *mode, int bpp); calls mxs_set_lcdclock();
 *       - Start lcdif clock.
 *
 * 3.321 clock.c: mxs_set_lcdclk(uint32_t base_addr, uint32_t freq);
 *       calls enable_pll_video();
 *       - Power up PLL5 video.
 *       - Set div, num and denom.
 *       - Set PLL Lock.
 *
 * 3.34  mxsfb.c: mxs_lcd_init(); calls mxs_reset_block();
 * 3.35  misc.c: mxs_reset_block(struct mxs_register_32 *reg);
 *       calls minor functions
 *
 * 4.0   board.c: board_init_r(); calls board_late_init();
 * 4.1   fsimx6ul.c: board_late_init(); calls enable_displays();
 *       - Enables backlight voltage and sets backlight brightness (PWM)
 *         for all active displays
 */

static iomux_v3_cfg_t const lcd18_pads[] = {
	IOMUX_PADS(PAD_LCD_CLK__GPIO3_IO00 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_HSYNC__GPIO3_IO02 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_VSYNC__GPIO3_IO03 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_ENABLE__GPIO3_IO01 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA00__GPIO3_IO05  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA01__GPIO3_IO06  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA02__GPIO3_IO07  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA03__GPIO3_IO08  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA04__GPIO3_IO09  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA05__GPIO3_IO10  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA06__GPIO3_IO11  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA07__GPIO3_IO12  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA08__GPIO3_IO13  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA09__GPIO3_IO14  | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA10__GPIO3_IO15 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA11__GPIO3_IO16 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA12__GPIO3_IO17 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA13__GPIO3_IO18 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA14__GPIO3_IO19 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA15__GPIO3_IO20 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA16__GPIO3_IO21 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA17__GPIO3_IO22 | MUX_PAD_CTRL(LCD_CTRL)),
};

I2C_PADS(efusa7ul,						\
	 PAD_GPIO1_IO02__I2C1_SCL | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 PAD_GPIO1_IO02__GPIO1_IO02 |  MUX_PAD_CTRL(I2C_PAD_CTRL),\
	 IMX_GPIO_NR(1, 2),					\
	 PAD_GPIO1_IO01__I2C2_SDA | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 PAD_GPIO1_IO01__GPIO1_IO01 | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 IMX_GPIO_NR(1, 1));

/* Available display ports */
#define DISP_PORT_LCD	(1 << 0)
#define DISP_PORT_HDMI	(1 << 1)
#define DISP_PORT_LVDS0	(1 << 2)
#define DISP_PORT_LVDS1	(1 << 3)

/* Extra LVDS settings */
#define LDB_FLAGS_SPLIT	(1 << 0)	/* 0: 1ch display, 1: 2ch display */
#define LDB_FLAGS_DUAL	(1 << 1)	/* 0: one display, 1: two displays */
#define LDB_FLAGS_24BPP	(1 << 2)	/* 0: 18 bpp, 1: 24 bpp */
#define LDB_FLAGS_JEIDA	(1 << 3)	/* 0: 24 bpp SPWG, 1: 24 bpp JEIDA */

/* Extra display parameters that are not part of fb_videomode */
struct fb_extra {
	unsigned int port;
	unsigned int ldb_flags;
};

struct display_params {
	struct fb_videomode mode;
	struct fb_extra extra;
};

struct param_choice {
	const char *name;
	unsigned int val;
};

/* This is a set of used display ports */
unsigned int used_ports;

struct display_params display;

const struct param_choice disp_ports[] = {
	{"lcd", DISP_PORT_LCD},
	{"hdmi", DISP_PORT_LCD},
	{"lvds0", DISP_PORT_LVDS0},
	{"lvds1", DISP_PORT_LVDS1},
};

/*
 * Have a small display data base.
 *
 * drivers/video/mxcfb.h defines additional values to set DE polarity and
 * clock sensitivity. However these values are not valid in Linux and the file
 * can not easily be included here. So we (F&S) misuse some existing defines
 * from include/linux/fb.h to handle these two cases. This may change in the
 * future.
 *
 *   FB_SYNC_COMP_HIGH_ACT: DE signal active high
 *   FB_SYNC_ON_GREEN:      Latch on rising edge of pixel clock
 */

const struct fb_videomode const display_db[] = {
	{
		.name           = "EDT-ET070080DH6",
		.refresh        = 60,
		.xres           = 800,
		.yres           = 480,
		.pixclock       = 30066, // picosceconds
		.left_margin    = 88,
		.right_margin   = 40,
		.upper_margin   = 33,
		.lower_margin   = 10,
		.hsync_len      = 128,
		.vsync_len      = 2,
		.sync           = FB_SYNC_ON_GREEN | FB_SYNC_COMP_HIGH_ACT,
		.vmode          = FB_VMODE_NONINTERLACED
	},
#if 0
	{
		.name           = "HDMI",
		.refresh        = 60,
		.xres           = 640,
		.yres           = 480,
		.pixclock       = 39721,
		.left_margin    = 48,
		.right_margin   = 16,
		.upper_margin   = 33,
		.lower_margin   = 10,
		.hsync_len      = 96,
		.vsync_len      = 2,
		.sync           = 0,
		.vmode          = FB_VMODE_NONINTERLACED
	},
#endif
};

/* Always use serial for U-Boot console */
int overwrite_console(void)
{
	return 1;
}

/* Enable backlight power and set brightness via I2C on RGB adapter */
static void enable_i2c_backlight(int on)
{
	u8 val;
#if 0
	printf("### ID = 0x%x\n", i2c_reg_read(0x60, 0));
#endif

	/*
	 * Talk to the PCA9632 via I2C, this is a 4 channel LED driver
	 *  Channel 0: Used as GPIO to switch backlight power
	 *  Channel 1: Used as PWM to set backlight brightness
	 *  Channel 2: Used as GPIO to set display rotation
	 *  Channel 3: Unused
	 * Channels use inverted logic, i.e. ON=0, OFF=1, and the higher the
	 * PWM value, the lower the duty cycle
	 */
	i2c_reg_write(0x60, 0x0, 0x0);	/* Normal mode, no auto-increment */
	i2c_reg_write(0x60, 0x1, 0x5);	/* Grp dimming, no inv., Totem pole */
	i2c_reg_write(0x60, 0x3, 0xf0);	/* PWM duty cycle for Channel 1 */
	if (on)
		val = 0x18;		/* CH2: ON=0, CH1: PWM, CH0: OFF=1 */
	else
		val = 0x11;		/* CH2: ON=0, CH1: OFF=1, CH0: ON=0 */
	i2c_reg_write(0x60, 0x8, val);
}

/* Enable VLCD, configure pads if required */
static void prepare_displays(void)
{
	unsigned int board_type = fs_board_get_type();

	/* Switch off display power on RGB adapter */
	switch (board_type) {
	case BT_EFUSA7UL:
		setup_i2c(1, CONFIG_SYS_I2C_SPEED, 0x60, I2C_PADS_INFO(efusa7ul));
		i2c_set_bus_num(1);
		enable_i2c_backlight(0);
		break;
	default:
		break;
	}

	/* Enable VLCD */
	if (used_ports & (DISP_PORT_LCD | DISP_PORT_LVDS0 | DISP_PORT_LVDS1)){
		switch (board_type) {
		case BT_EFUSA7UL:
			gpio_direction_output(IMX_GPIO_NR(3, 19), 1);
			mdelay(10);
			break;
		default:
			break;
		}
	}

	if (used_ports & DISP_PORT_LCD) {
		switch (board_type) {
		case BT_EFUSA7UL:		/* 18-bit LCD interface */
		default:
			SETUP_IOMUX_PADS(lcd18_pads);
			break;
		}
	}
}

/* Enable backlight power depending on the used display ports */
static void enable_displays(void)
{
	unsigned int board_type = fs_board_get_type();

	if (used_ports & DISP_PORT_LCD) {
		switch (board_type) {
		case BT_EFUSA7UL:
			i2c_set_bus_num(1);
			enable_i2c_backlight(1);
			break;
		default:
			break;
		}
	}

	if (used_ports & DISP_PORT_HDMI) {
		// ### TODO
	}
}

static int parse_choice_param(const char *param, unsigned *val, const char **s,
			      const struct param_choice *choice, unsigned count)
{
	int len;
	const char *p = *s;
	unsigned int i;
	const struct param_choice *entry;
	char c;

	/* Check for parameter name match */
	len = strlen(param);
	if (strncmp(param, p, len))
		return 0;
	p += len;
	if (*p++ != '=')
		return 0;

	/* Search for a matching choice */
	for (i = 0, entry = choice; i < count; i++, entry++) {
		len = strlen(entry->name);
		if (!strncmp(p, entry->name, len)) {
			c = p[len];
			if (c == ',')
				len++;
			else if (c != '\0')
				continue;
			*s = p + len;
			*val = entry->val;
			return 0;
		}
	}

	/* No matching choice found */
	return -1;
}

/* Parse <param>=<uint>; returns 1 for match, 0 for no match, -1 for error */
static int parse_uint_param(const char *param, unsigned int *val,
			     const char **s)
{
	int len = strlen(param);
	const char *p = *s;
	char *endp;
	unsigned int tmp;

	if (strncmp(param, p, len))
		return 0;
	p += len;
	if (*p++ != '=')
		return 0;
	tmp = simple_strtoul(p, &endp, 0);
	if (endp == p)
		return -1;
	if (*endp == ',')
		endp++;
	else if (*endp != '\0')
		return -1;
	*s = endp;
	*val = tmp;

	return 1;
}

static int parse_display_params(struct display_params *display, const char *s)
{
	int err = 0;
	unsigned int tmp = ~0;
	const char *start;

	if (!s || !*s)
		return 0;

	/* Parse given string for paramters */
	while (*s) {
		start = s;

		/* Values from dispmode */
		err = parse_uint_param("clk", &tmp, &s);
		if (err < 0)
			break;
		if (err > 0)
			display->mode.pixclock = KHZ2PICOS(tmp/1000);
		err = parse_uint_param("rate", &display->mode.refresh, &s);
		if (err < 0)
			break;
		err = parse_uint_param("hres", &display->mode.xres, &s);
		if (err < 0)
			break;
		err = parse_uint_param("vres", &display->mode.yres, &s);
		if (err < 0)
			break;
		err = parse_uint_param("hfp", &display->mode.right_margin, &s);
		if (err < 0)
			break;
		err = parse_uint_param("hbp", &display->mode.left_margin, &s);
		if (err < 0)
			break;
		err = parse_uint_param("vfp", &display->mode.lower_margin, &s);
		if (err < 0)
			break;
		err = parse_uint_param("vbp", &display->mode.upper_margin, &s);
		if (err < 0)
			break;
		err = parse_uint_param("hsw", &display->mode.hsync_len, &s);
		if (err < 0)
			break;
		err = parse_uint_param("vsw", &display->mode.vsync_len, &s);
		if (err < 0)
			break;
		err = parse_uint_param("hsp", &tmp, &s);
		if (err < 0)
			break;
		if (err > 0) {
			if (tmp)
				display->mode.sync |= FB_SYNC_HOR_HIGH_ACT;
			else
				display->mode.sync &= ~FB_SYNC_HOR_HIGH_ACT;
		}
		err = parse_uint_param("vsp", &tmp, &s);
		if (err < 0)
			break;
		if (err > 0) {
			if (tmp)
				display->mode.sync |= FB_SYNC_VERT_HIGH_ACT;
			else
				display->mode.sync &= ~FB_SYNC_VERT_HIGH_ACT;
		}
		err = parse_uint_param("dep", &tmp, &s);
		if (err < 0)
			break;
		if (err > 0) {
			if (tmp)	/* DE active high */
				display->mode.sync |= FB_SYNC_COMP_HIGH_ACT;
			else		/* DE active low */
				display->mode.sync &= ~FB_SYNC_COMP_HIGH_ACT;
		}
		err = parse_uint_param("clkp", &tmp, &s);
		if (err < 0)
			break;
		if (err > 0) {
			if (tmp)	/* Latch on rising edge */
				display->mode.sync |= FB_SYNC_ON_GREEN;
			else		/* Latch on falling edge */
				display->mode.sync &= ~FB_SYNC_ON_GREEN;
		}
		err = parse_uint_param("il", &tmp, &s);
		if (err < 0)
			break;
		if (err > 0) {
			if (tmp)
				display->mode.vmode &= ~FB_VMODE_INTERLACED;
			else
				display->mode.vmode |= FB_VMODE_INTERLACED;
		}

		/* Values from disppara */
		err = parse_choice_param("port", &display->extra.port, &s,
					 disp_ports, ARRAY_SIZE(disp_ports));
		if (err < 0)
			break;

		err = parse_uint_param("split", &tmp, &s);
		if (err < 0)
			break;
		if (err > 0) {
			if (tmp == 1)
				display->extra.ldb_flags |= LDB_FLAGS_SPLIT;
			else
				display->extra.ldb_flags &= ~LDB_FLAGS_SPLIT;
		}

		err = parse_uint_param("dual", &tmp, &s);
		if (err < 0)
			break;
		if (err > 0) {
			if (tmp == 1)
				display->extra.ldb_flags |= LDB_FLAGS_DUAL;
			else
				display->extra.ldb_flags &= ~LDB_FLAGS_DUAL;
		}

		err = parse_uint_param("bw", &tmp, &s);
		if (err < 0)
			break;
		if (err > 0) {
			if ((tmp == 24) || (tmp == 1))
				display->extra.ldb_flags |= LDB_FLAGS_24BPP;
			else
				display->extra.ldb_flags &= ~LDB_FLAGS_24BPP;
		}

		err = parse_uint_param("jeida", &tmp, &s);
		if (err < 0)
			break;
		if (err > 0) {
			if ((tmp == 24) || (tmp == 1))
				display->extra.ldb_flags |= LDB_FLAGS_JEIDA;
			else
				display->extra.ldb_flags &= ~LDB_FLAGS_JEIDA;
		}

		err = 0;
		if (s == start)
			break;		/* No progress during this loop */
	}

	/* Complain if we did not consume the whole string */
	if (*s)
		err = -1;
	if (err < 0)
		printf("Error parsing display parameters\n"
		       "Remaining string: %s\n", s);
	return err;
}

int board_video_skip(void)
{
	int i;
	unsigned int freq;
	const char *panel = getenv("disppanel");
	const char *mode = getenv("dispmode");
	unsigned int board_type = fs_board_get_type();

	if (!panel)
		return 1;
	/* Look for panel in display database */
	for (i = 0; i <  ARRAY_SIZE(display_db); i++) {
		if (!strcmp(panel, display_db[i].name))
			break;
	}
	if ((i >= ARRAY_SIZE(display_db))) {
		if (!mode) {
			printf("Display panel %s not found.\n"
			       "For a user-defined panel set variable"
			       " 'disppanel' with appropriate timings\n",
			       panel);
			return 1;
		}
		/* Use first entry for default parameters */
		i = 0;
	}

	/* Take mode parameters from display database */
	display.mode = display_db[i];

	/* Init extra parameters to default */
	switch (board_type) {
	case BT_EFUSA7UL:
	default:
		display.extra.port = DISP_PORT_LVDS0;
		break;
	}


	/* Parse mode parameters to override defaults */
	if ((parse_display_params(&display, mode) < 0)
	    || (parse_display_params(&display, getenv("disppara")) < 0))
	    return 1;

	/*
	 * If pixelclock is given, compute frame rate. If pixelclock is
	 * missing, compute it from frame rate. If frame rate is also missing,
	 * assume 60 fps.
	 */

	freq = (display.mode.xres + display.mode.left_margin
		+ display.mode.right_margin + display.mode.hsync_len)
		* (display.mode.yres + display.mode.upper_margin
		   + display.mode.lower_margin + display.mode.vsync_len);

	if (display.mode.pixclock) {
		display.mode.refresh =
			(PICOS2KHZ(display.mode.pixclock) * 1000 + freq/2)/freq;
	} else {
		if (!display.mode.refresh)
			display.mode.refresh = 60;
		display.mode.pixclock =
			KHZ2PICOS(freq * display.mode.refresh / 1000);
	}

	/*
	 * Initialize display clock and register display settings with IPU
	 * driver. The real initialization takes place when this function
	 * returns.
	 */

	freq = PICOS2KHZ(display.mode.pixclock) * 1000;

	switch (display.extra.port) {

	case DISP_PORT_LCD:
		mxs_lcd_panel_setup(display.mode, 18, LCDIF1_BASE_ADDR);
		enable_lcdif_clock(LCDIF1_BASE_ADDR);
		break;

	case DISP_PORT_HDMI:
		puts("### HDMI support not yet implemented\n");
		return 1;
	}
	used_ports |= display.extra.port;

	printf("Disp.: %s (%ux%u)\n", panel, display.mode.xres,
	       display.mode.yres);
#if 0
	show_dispmode(&display.mode);
	show_disppara(&display.extra);
#endif

	/* Enable VLCD */

	prepare_displays();

	return 0;
}

/* Run variable splashprepare to load bitmap image for splash */
int splash_screen_prepare(void)
{
	char *prep;

	prep = getenv("splashprepare");
	if (prep)
		run_command(prep, 0);

	return 0;
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
 *    Board           USB_OTG1_PWR                 USB_OTG1_ID
 *    ----------------------------------------------------------------------
 *    efusA7UL        GPIO1_IO04(*)                GPIO1_IO00
 *    PicoCOM1.2      ENET2_RX_DATA0 (GPIO2_IO08)  -
 *    PicoCOMA7       (only device mode possible)
 *    PicoCoreMX6UL   SNVS_TAMPER3 (GPIO5_IO03)(*) GPIO1_IO00
 *    CubeA7UL/2.0    (Port is power supply, only device mode possible)
 *    GAR1            (always on)                  GPIO1_IO00
 *
 * (*) Signal on SKIT is active low, usually USB_OTG1_PWR is active high
 *
 * USB1 is OTG2 port that is only used as host port at F&S. It is used on all
 * boards. Some boards may have an additional USB hub with a reset signal
 * connected to this port.
 *
 *    Board           VBUS PWR                     Hub Reset
 *    -------------------------------------------------------------------------
 *    efusA7UL        (always on)                  (Hub on SKIT, no reset line)
 *    PicoCOM1.2      ENET2_TX_DATA1 (GPIO2_IO12)  (no Hub)
 *    PicoCOMA7	      UART4_TX_DATA                (no Hub)
 *    PicoCoreMX6UL   SNVS_TAMPER2 (GPIO5_IO02)    (no Hub)
 *    CubeA7UL/2.0    GPIO1_IO02                   (no Hub)
 *    GAR1            SD1_DATA1 (GPIO2_IO19)       (no Hub)
 *
 * The polarity for the host VBUS power can be set with environment variable
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
static iomux_v3_cfg_t const usb_otg1_id_pad[] = {
	IOMUX_PADS(PAD_GPIO1_IO00__GPIO1_IO00 | MUX_PAD_CTRL(USB_ID_PAD_CTRL)),
};

/* Some boards can switch the USB OTG power when configured as host */
static iomux_v3_cfg_t const usb_otg1_pwr_pad_efusa7ul[] = {
#ifdef CONFIG_FS_USB_PWR_USBNC
	IOMUX_PADS(PAD_GPIO1_IO04__USB_OTG1_PWR | MUX_PAD_CTRL(NO_PAD_CTRL))
#else
	IOMUX_PADS(PAD_GPIO1_IO04__GPIO1_IO04 | MUX_PAD_CTRL(NO_PAD_CTRL))
#endif
};

static iomux_v3_cfg_t const usb_otg1_pwr_pad_picocom1_2[] = {
#ifdef CONFIG_FS_USB_PWR_USBNC
	IOMUX_PADS(PAD_ENET2_RX_DATA0__USB_OTG1_PWR | MUX_PAD_CTRL(NO_PAD_CTRL))
#else
	IOMUX_PADS(PAD_ENET2_RX_DATA0__GPIO2_IO08 | MUX_PAD_CTRL(NO_PAD_CTRL))
#endif
};

/* On PicoCoreMX6UL, power switching by GPIO only; pins differ on UL and ULL */
static iomux_v3_cfg_t const usb_otg1_pwr_pad_pcoremx6ul[] = {
	MX6UL_PAD_SNVS_TAMPER2__GPIO5_IO02 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const usb_otg1_pwr_pad_pcoremx6ull[] = {
	MX6ULL_PAD_SNVS_TAMPER2__GPIO5_IO02 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

/* Some boards can switch the USB Host power (USB_OTG2_PWR) */
static iomux_v3_cfg_t const usb_otg2_pwr_pad_picocom1_2[] = {
#ifdef CONFIG_FS_USB_PWR_USBNC
	IOMUX_PADS(PAD_ENET2_TX_DATA1__USB_OTG2_PWR | MUX_PAD_CTRL(NO_PAD_CTRL))
#else
	IOMUX_PADS(PAD_ENET2_TX_DATA1__GPIO2_IO12 | MUX_PAD_CTRL(NO_PAD_CTRL))
#endif
};
static iomux_v3_cfg_t const usb_otg2_pwr_pad_picocoma7[] = {
		IOMUX_PADS(PAD_UART4_TX_DATA__GPIO1_IO28 | MUX_PAD_CTRL(NO_PAD_CTRL))
};
static iomux_v3_cfg_t const usb_otg2_pwr_pad_cube[] = {
#ifdef CONFIG_FS_USB_PWR_USBNC
	IOMUX_PADS(PAD_GPIO1_IO02__USB_OTG2_PWR | MUX_PAD_CTRL(NO_PAD_CTRL))
#else
	IOMUX_PADS(PAD_GPIO1_IO02__GPIO1_IO02 | MUX_PAD_CTRL(NO_PAD_CTRL))
#endif
};

static iomux_v3_cfg_t const usb_otg2_pwr_pad_gar1[] = {
#ifdef CONFIG_FS_USB_PWR_USBNC
	IOMUX_PADS(PAD_SD1_DATA1__USB_OTG2_PWR | MUX_PAD_CTRL(NO_PAD_CTRL))
#else
	IOMUX_PADS(PAD_SD1_DATA1__GPIO2_IO19 | MUX_PAD_CTRL(NO_PAD_CTRL))
#endif
};

/* On PicoCoreMX6UL, power switching by GPIO only; pins differ on UL and ULL */
static iomux_v3_cfg_t const usb_otg2_pwr_pad_pcoremx6ul[] = {
	MX6UL_PAD_SNVS_TAMPER2__GPIO5_IO02 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const usb_otg2_pwr_pad_pcoremx6ull[] = {
	MX6ULL_PAD_SNVS_TAMPER2__GPIO5_IO02 | MUX_PAD_CTRL(NO_PAD_CTRL),
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

	if (index == 0) {		/* USB0: OTG1, DEVICE, optional HOST */
		switch (board_type) {
		/* These boards support optional HOST function on this port */
		case BT_EFUSA7UL:	/* PWR active low, ID available */
			cfg.pwr_pad = usb_otg1_pwr_pad_efusa7ul;
#ifndef CONFIG_FS_USB_PWR_USBNC
			cfg.pwr_gpio = IMX_GPIO_NR(1, 4);
#endif
			cfg.pwr_pol = 1;
			cfg.id_pad = usb_otg1_id_pad;
			cfg.id_gpio = IMX_GPIO_NR(1, 0);
			break;
		case BT_PICOCOM1_2:	/* PWR active high, no ID */
			cfg.pwr_pad = usb_otg1_pwr_pad_picocom1_2;
#ifndef CONFIG_FS_USB_PWR_USBNC
			cfg.pwr_gpio = IMX_GPIO_NR(2, 8);
#endif
			break;
		case BT_PCOREMX6UL:	/* PWR active high???, ID available */
			if (is_cpu_type(MXC_CPU_MX6ULL))
				cfg.pwr_pad = usb_otg1_pwr_pad_pcoremx6ull;
			else
				cfg.pwr_pad = usb_otg1_pwr_pad_pcoremx6ul;
			cfg.pwr_gpio = IMX_GPIO_NR(5, 3); /* GPIO only */
			cfg.pwr_pol = 1;
			cfg.id_pad = usb_otg1_id_pad;
			cfg.id_gpio = IMX_GPIO_NR(1, 0);
			break;
		case BT_GAR1:		/* PWR always on, ID available */
			cfg.id_pad = usb_otg1_id_pad;
			cfg.id_gpio = IMX_GPIO_NR(1, 0);
			break;

		/* These boards have only DEVICE function on this port */
		case BT_CUBEA7UL:
		case BT_CUBE2_0:
		default:
			cfg.mode = FS_USB_DEVICE;
			break;
		}
	} else {			/* USB1: OTG2, HOST only */
		cfg.mode = FS_USB_HOST;

		switch (board_type) {
		/* These boards can switch host power, PWR is active high */
		case BT_PICOCOM1_2:
			cfg.pwr_pad = usb_otg2_pwr_pad_picocom1_2;
#ifndef CONFIG_FS_USB_PWR_USBNC
			cfg.pwr_gpio = IMX_GPIO_NR(2, 12);
#endif
			break;
		case BT_PICOCOMA7:
			cfg.pwr_pad = usb_otg2_pwr_pad_picocoma7;
#ifndef CONFIG_FS_USB_PWR_USBNC
			cfg.pwr_gpio = IMX_GPIO_NR(1, 28);
#endif
			break;
		case BT_CUBEA7UL:
		case BT_CUBE2_0:
			cfg.pwr_pad = usb_otg2_pwr_pad_cube;
#ifndef CONFIG_FS_USB_PWR_USBNC
			cfg.pwr_gpio = IMX_GPIO_NR(1, 2);
#endif
			break;
		case BT_GAR1:
			cfg.pwr_pad = usb_otg2_pwr_pad_gar1;
#ifndef CONFIG_FS_USB_PWR_USBNC
			cfg.pwr_gpio = IMX_GPIO_NR(2, 19);
#endif
			break;
		case BT_PCOREMX6UL:
			if (is_cpu_type(MXC_CPU_MX6ULL))
				cfg.pwr_pad = usb_otg2_pwr_pad_pcoremx6ull;
			else
				cfg.pwr_pad = usb_otg2_pwr_pad_pcoremx6ul;
			cfg.pwr_gpio = IMX_GPIO_NR(5, 2); /* GPIO only */
			break;

		/* ### TODO: Starting with board rev 1.2, efusA7UL can switch
		   power via I2C I/O-Expander ### */
		/* These boards can not switch host power, it is always on */
		case BT_EFUSA7UL:
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
	fs_board_late_init_common();

#ifdef CONFIG_VIDEO_MXS
	/* Enable backlight for displays */
	enable_displays();
#endif

	return 0;
}
#endif /* CONFIG_BOARD_LATE_INIT */

#ifdef CONFIG_CMD_NET
/* MDIO on ENET1, used if either only ENET1 port or both ports are in use */
static iomux_v3_cfg_t const enet1_pads_mdio[] = {
	IOMUX_PADS(PAD_GPIO1_IO06__ENET1_MDIO | MUX_PAD_CTRL(MDIO_PAD_CTRL)),
	IOMUX_PADS(PAD_GPIO1_IO07__ENET1_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL)),
};

/* MDIO on ENET2, used if only ENET2 port is in use */
static iomux_v3_cfg_t const enet2_pads_mdio[] = {
	IOMUX_PADS(PAD_GPIO1_IO06__ENET2_MDIO | MUX_PAD_CTRL(MDIO_PAD_CTRL)),
	IOMUX_PADS(PAD_GPIO1_IO07__ENET2_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL)),
};

/* FEC0 (ENET1); 100 MBit/s on RMII, reference clock goes from CPU to PHY */
static iomux_v3_cfg_t const enet1_pads_rmii[] = {
	IOMUX_PADS(PAD_ENET1_RX_EN__ENET1_RX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET1_RX_ER__ENET1_RX_ER | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET1_TX_EN__ENET1_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET1_RX_DATA0__ENET1_RDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET1_RX_DATA1__ENET1_RDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET1_TX_DATA0__ENET1_TDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET1_TX_DATA1__ENET1_TDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET1_TX_CLK__ENET1_REF_CLK1 | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL)),
};

/* FEC1 (ENET2); 100 MBit/s on RMII, reference clock goes from CPU to PHY */
static iomux_v3_cfg_t const enet2_pads_rmii[] = {
	IOMUX_PADS(PAD_ENET2_RX_EN__ENET2_RX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET2_RX_ER__ENET2_RX_ER | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET2_TX_EN__ENET2_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET2_RX_DATA0__ENET2_RDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET2_RX_DATA1__ENET2_RDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET2_TX_DATA0__ENET2_TDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET2_TX_DATA1__ENET2_TDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET2_TX_CLK__ENET2_REF_CLK2 | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL)),

};

/* Additional pins required to reset the PHY(s). Please note that on efusA7UL
   before board revision 1.20, this is actually the generic RESETOUTn signal! */
static iomux_v3_cfg_t const enet_pads_reset_efus_picocom_ull[] = {
	MX6ULL_PAD_BOOT_MODE1__GPIO5_IO11 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const enet_pads_reset_efus_picocom_ul[] = {
	MX6UL_PAD_BOOT_MODE1__GPIO5_IO11 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const enet_pads_reset_picocoma7[] = {
	MX6UL_PAD_SNVS_TAMPER5__GPIO5_IO05 | MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6UL_PAD_SNVS_TAMPER6__GPIO5_IO06  | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const enet_pads_reset_gar1[] = {
	IOMUX_PADS(PAD_CSI_MCLK__GPIO4_IO17 | MUX_PAD_CTRL(NO_PAD_CTRL)),
	IOMUX_PADS(PAD_CSI_PIXCLK__GPIO4_IO18 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

int board_eth_init(bd_t *bis)
{
	u32 gpr1;
	int ret;
	int phy_addr_a, phy_addr_b;
	enum xceiver_type xcv_type;
	phy_interface_t interface = PHY_INTERFACE_MODE_RMII;
	struct iomuxc *iomux_regs = (struct iomuxc *)IOMUXC_BASE_ADDR;
	struct mii_dev *bus = NULL;
	struct phy_device *phydev;
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();
	unsigned int features2 = fs_board_get_nboot_args()->chFeatures2;
	int id = 0;

	/* Ungate ENET clock, this is a common clock for both ports */
	if (features2 & (FEAT2_ETH_A | FEAT2_ETH_B))
		enable_enet_clk(1);

	/*
	 * Set IOMUX for ports, enable clocks. Both PHYs were already reset
	 * via RESETOUTn in board_init().
	 */
	switch (board_type) {
	default:
		if (features2 & FEAT2_ETH_A) {
			/* IOMUX for ENET1, use 100 MBit/s LAN on RMII pins */
			SETUP_IOMUX_PADS(enet1_pads_rmii);
			SETUP_IOMUX_PADS(enet1_pads_mdio);

			/* ENET1 CLK is generated in i.MX6UL and is output */
			gpr1 = readl(&iomux_regs->gpr[1]);
			gpr1 |= IOMUXC_GPR1_ENET1_CLK_SEL;
			writel(gpr1, &iomux_regs->gpr[1]);

			/* Activate ENET1 PLL */
			ret = enable_fec_anatop_clock(0, ENET_50MHZ);
			if (ret < 0)
				return ret;
		}

		if (features2 & FEAT2_ETH_B) {
			/* Set the IOMUX for ENET2, use 100 MBit/s LAN on RMII
			   pins. If both ports are in use, MDIO was already
			   set above. Otherwise we will use MDIO via ENET2 to
			   avoid having to activate the clock for ENET1. */
			SETUP_IOMUX_PADS(enet2_pads_rmii);
			if (!(features2 & FEAT2_ETH_A))
				SETUP_IOMUX_PADS(enet2_pads_mdio);

			/* ENET2 CLK is generated in i.MX6UL and is output */
			gpr1 = readl(&iomux_regs->gpr[1]);
			gpr1 |= IOMUXC_GPR1_ENET2_CLK_SEL;
			writel(gpr1, &iomux_regs->gpr[1]);

			/* Activate ENET2 PLL */
			ret = enable_fec_anatop_clock(1, ENET_50MHZ);
			if (ret < 0)
				return 0;
		}

		if ((board_type == BT_PICOCOM1_2)
		    || (board_type == BT_PICOCOMA7) || (board_type == BT_GAR1))
			phy_addr_a = 1;
		else
			phy_addr_a = 0;

		if (board_type == BT_GAR1)
			phy_addr_b = 17;
		else
			phy_addr_b = 3;

		xcv_type = RMII;
		break;
	}

	/* Reset the PHY(s) */
	switch (board_type) {
	case BT_EFUSA7UL:
		/*
		 * Up to two KSZ8081RNA PHYs: This PHY needs at least 500us
		 * reset pulse width and 100us delay before the first MDIO
		 * access can be done.
		 *
		 * ATTENTION:
		 * On efusA7UL before board revision 1.20, this is the generic
		 * RESETOUTn signal that is also available on pin 14 of the
		 * efus connector. There it also resets WLAN. And because the
		 * WLAN there needs at least 100ms, we must increase the reset
		 * pulse time in this case.
		 * On efusA7UL since board revision 1.20, this only resets the
		 * ethernet PHYs. WLAN reset, RESETOUTn and other power signals
		 * are handled by an additional I2C IO expander.
		 */
		if (is_cpu_type(MXC_CPU_MX6ULL))
			SETUP_IOMUX_PADS(enet_pads_reset_efus_picocom_ull);
		else
			SETUP_IOMUX_PADS(enet_pads_reset_efus_picocom_ul);
		fs_board_issue_reset((board_rev < 120) ? 100000 : 500,
				     100, IMX_GPIO_NR(5, 11), ~0, ~0);
		break;
	case BT_PICOCOM1_2:
		/*
		 * DP83484 PHY: This PHY needs at least 1us reset pulse
		 * width. After power on it needs min 167ms (after reset is
		 * deasserted) before the first MDIO access can be done. In a
		 * warm start, it only takes around 3us for this. As we do not
		 * know whether this is a cold or warm start, we must assume
		 * the worst case.
		 *
		 * ATTENTION:
		 * On PicoCOM1.2, this is the generic RESETOUTn signal that
		 * also resets WLAN. But WLAN needs only ~100ms, so no need to
		 * further increase the reset pulse time.
		 */
		if (is_cpu_type(MXC_CPU_MX6ULL))
			SETUP_IOMUX_PADS(enet_pads_reset_efus_picocom_ull);
		else
			SETUP_IOMUX_PADS(enet_pads_reset_efus_picocom_ul);
		fs_board_issue_reset(10, 170000, IMX_GPIO_NR(5, 11), ~0, ~0);
		break;

	case BT_PICOCOMA7:
		/* Two DP83484 PHYs with separate reset signals; see comment
		   above for timing considerations */
		SETUP_IOMUX_PADS(enet_pads_reset_picocoma7);
		fs_board_issue_reset(10, 170000, IMX_GPIO_NR(5, 5),
				     IMX_GPIO_NR(5, 6), ~0);
		break;

	case BT_GAR1:
		/* Two DP83484 PHYs with separate reset signals; see comment
		   above for timing considerations */
		SETUP_IOMUX_PADS(enet_pads_reset_gar1);
		fs_board_issue_reset(10, 170000, IMX_GPIO_NR(4, 17),
				     IMX_GPIO_NR(4, 18), ~0);
		break;

	case BT_PCOREMX6UL:
		/*
		 * Up to two KSZ8081RNA PHYs: This PHY needs at least 500us
		 * reset pulse width and 100us delay before the first MDIO
		 * access can be done.
		 *
		 * On PicoCoreMX6UL reset signal is used for ethernet PHYs only.
		 * Having the distinction for ULL and UL we are prepared for
		 * possible PicoCoreMX6UL with UL CPU in future.
		 */
		if (is_cpu_type(MXC_CPU_MX6ULL))
			SETUP_IOMUX_PADS(enet_pads_reset_efus_picocom_ull);
		else
			SETUP_IOMUX_PADS(enet_pads_reset_efus_picocom_ul);
			fs_board_issue_reset(500, 100, IMX_GPIO_NR(5, 11),
					     ~0, ~0);
		break;

	default:
		break;
	}

	/* Probe first PHY and activate first ethernet port */
	if (features2 & FEAT2_ETH_A) {
		fs_eth_set_ethaddr(id);

		/*
		 * We can not use fecmxc_initialize_multi_type() because this
		 * would allocate one MII bus for each ethernet device. But on
		 * efusA7UL we only need one MII bus in total for both ports.
		 * So the following code works rather similar to the code in
		 * fecmxc_initialize_multi_type(), but uses just one bus on
		 * efusA7UL.
		 */
		bus = fec_get_miibus(ENET_BASE_ADDR, -1);
		if (!bus)
			return -ENOMEM;

		phydev = phy_find_by_mask(bus, 1 << phy_addr_a, interface);
		if (!phydev) {
			free(bus);
			return -ENOMEM;
		}

		ret = fec_probe(bis, id, ENET_BASE_ADDR, bus, phydev, xcv_type);
		if (ret) {
			free(phydev);
			free(bus);
			return ret;
		}
		id++;
	}

	/* Probe second PHY and activate second ethernet port. */
	if (features2 & FEAT2_ETH_B) {
		fs_eth_set_ethaddr(id);

		/* If ENET1 is not in use, we must get our MDIO bus now */
		if (!bus) {
			bus = fec_get_miibus(ENET2_BASE_ADDR, -1);
			if (!bus)
				return -ENOMEM;
		}

		phydev = phy_find_by_mask(bus, 1 << phy_addr_b, interface);
		if (!phydev) {
			if (!(features2 & FEAT2_ETH_A))
				free(bus);
			return -ENOMEM;
		}

		/* Probe the second ethernet port */
		ret = fec_probe(bis, id, ENET2_BASE_ADDR, bus, phydev,
				xcv_type);
		if (ret) {
			free(phydev);
			/* If this is the only port, return with error */
			if (!(features2 & FEAT2_ETH_A))
				free(bus);
			return ret;
		}
		id++;
	}

	/* If WLAN is available, just set ethaddr variable */
	if (features2 & FEAT2_WLAN)
		fs_eth_set_ethaddr(id++);

	return 0;
}
#endif /* CONFIG_CMD_NET */

#ifdef CONFIG_CMD_LED
/*
 * Boards       STA1           STA2         Active
 * ------------------------------------------------------------------------
 * efusA7UL     -              -            -
 * PicoCOM1.2   GPIO5_IO00     GPIO5_IO01   high
 * PicoCOMA7	GPIO5_IO00	   GPIO5_IO01   high
 * CubeA7UL     GPIO2_IO05     GPIO2_IO06   low
 * Cube2.0      GPIO2_IO05     GPIO2_IO06   low
 */
static unsigned int led_value;

static unsigned int get_led_gpio(led_id_t id, int val)
{
	unsigned int gpio;
	unsigned int board_type = fs_board_get_type();

	if (val)
		led_value |= (1 << id);
	else
		led_value &= ~(1 << id);

	switch (board_type) {
	case BT_PICOCOM1_2:
	case BT_PICOCOMA7:
		gpio = (id ? IMX_GPIO_NR(5, 1) : IMX_GPIO_NR(5, 0));
		break;

	default:			/* CubeA7UL, Cube2.0 */
		gpio = (id ? IMX_GPIO_NR(2, 5) : IMX_GPIO_NR(2, 6));
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
	int val = (board_type != BT_PICOCOM1_2);

	gpio_direction_output(get_led_gpio(0, val), val);
	gpio_direction_output(get_led_gpio(1, val), val);
}

void __led_set(led_id_t id, int val)
{
	unsigned int board_type = fs_board_get_type();

	if (id > 1)
		return;

	if (board_type != BT_PICOCOM1_2)
		val = !val;

	gpio_set_value(get_led_gpio(id, val), val);
}

void __led_toggle(led_id_t id)
{
	int val;

	if (id > 1)
		return;

	val = !((led_value >> id) & 1);
	gpio_set_value(get_led_gpio(id, val), val);
}
#endif /* CONFIG_CMD_LED */

#ifdef CONFIG_OF_BOARD_SETUP
/* Remove operation points for speeds > SPEED_LIMIT */
static void fs_fdt_limit_speed(void *fdt, int offs, char *name)
{
	const fdt32_t *val;
	fdt32_t *new;
	int src = 0, dest = 0, len, need_update = 0;

	/* Get existing points, each point is a cell pair <freq> <voltage> */
	val = fdt_getprop(fdt, offs, name, &len);
	if (!val || !len || (len % (2 * sizeof(fdt32_t))))
		return;

	/* Copy only values that do not exceed speed limit */
	new = (fdt32_t *)malloc(len);
	if (!new)
		return;
	len /= sizeof(fdt32_t);
	do {
		if (fdt32_to_cpu(val[src]) > SPEED_LIMIT)
			need_update = 1;
		else {
			new[dest++] = val[src];		/* freq */
			new[dest++] = val[src + 1];	/* voltage */
		}
		src += 2;
	} while (src < len);

	/* If there were changes, replace property with new cell array */
	if (need_update)
		fs_fdt_set_val(fdt, offs, name, new, dest*sizeof(fdt32_t), 1);

	free(new);
}

/* Do any additional board-specific device tree modifications */
void ft_board_setup(void *fdt, bd_t *bd)
{
	int offs;
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();

	printf("   Setting run-time properties\n");

	/* Set ECC strength for NAND driver */
	offs = fs_fdt_path_offset(fdt, FDT_NAND);
	if (offs >= 0) {
		fs_fdt_set_u32(fdt, offs, "fus,ecc_strength",
			       pargs->chECCtype, 1);
	}

	/* Remove operation points > 528 MHz if speed should be limited */
	if (pargs->chFeatures2 & FEAT2_SPEED) {
		offs = fs_fdt_path_offset(fdt, FDT_CPU0);
		if (offs >= 0) {
			fs_fdt_limit_speed(fdt, offs, "operating-points");
			fs_fdt_limit_speed(fdt, offs,
					   "fsl,soc-operating-points");
		}
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
		if (pargs->chFeatures2 & FEAT2_WLAN) {
			if ((board_type == BT_EFUSA7UL) && (board_rev >= 120))
				fs_fdt_set_wlan_macaddr(fdt, offs, id++, 1);
			else if (board_type == BT_CUBE2_0)
				fs_fdt_set_wlan_macaddr(fdt, offs, id++, 0);
		}
	}

	/* Disable ethernet node(s) if feature is not available */
	if (!(pargs->chFeatures2 & FEAT2_ETH_A))
		fs_fdt_enable(fdt, FDT_ETH_A, 0);
	if (!(pargs->chFeatures2 & FEAT2_ETH_B))
		fs_fdt_enable(fdt, FDT_ETH_B, 0);
}
#endif /* CONFIG_OF_BOARD_SETUP */

/* Board specific cleanup before Linux is started */
void board_preboot_os(void)
{
	/* Shut down all ethernet PHYs (suspend mode) */
	mdio_shutdown_all();
}
