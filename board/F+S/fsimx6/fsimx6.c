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
#include <asm/errno.h>
#ifdef CONFIG_CMD_NET
#include <miiphy.h>
#include <netdev.h>
#endif
#ifdef CONFIG_CMD_LCD
#include <cmd_lcd.h>			/* PON_*, POFF_* */
#endif
#include <serial.h>			/* struct serial_device */
#include <version.h>			/* version_string[] */

#ifdef CONFIG_GENERIC_MMC
#include <mmc.h>
#include <fsl_esdhc.h>			/* fsl_esdhc_initialize(), ... */
#endif

#ifdef CONFIG_CMD_LED
#include <status_led.h>			/* led_id_t */
#endif

#include <i2c.h>
#include <asm/imx-common/mxc_i2c.h>

#include <asm/imx-common/video.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/setup.h>			/* struct tag_fshwconfig, ... */
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
#include <fdt_support.h>		/* do_fixup_by_path_u32(), ... */

/* ------------------------------------------------------------------------- */

/* Addresses of arguments coming from NBoot and going to Linux */
#define NBOOT_ARGS_BASE (CONFIG_SYS_SDRAM_BASE + 0x00001000)
#define BOOT_PARAMS_BASE (CONFIG_SYS_SDRAM_BASE + 0x100)

#define BT_EFUSA9     0
#define BT_ARMSTONEA9 1
#define BT_PICOMODA9  2
#define BT_QBLISSA9   3
#define BT_ARMSTONEA9R2 4
#define BT_QBLISSA9R2 6
#define BT_NETDCUA9   7

/* Features set in tag_fshwconfig.chFeature2 (available since NBoot VN27) */
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

#define ACTION_RECOVER 0x00000040	/* Start recovery instead of update */

#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)

/* Device tree paths */
#define FDT_NAND	"/soc/gpmi-nand@00112000"
#define FDT_ETH_A	"/soc/aips-bus@02100000/ethernet@02188000"

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

struct board_info {
	char *name;			/* Device name */
	char *bootdelay;		/* Default value for bootdelay */
	char *updatecheck;		/* Default value for updatecheck */
	char *installcheck;		/* Default value for installcheck */
	char *recovercheck;		/* Default value for recovercheck */
	char *earlyusbinit;		/* Default value for earlyusbinit */
	char *console;			/* Default variable for console */
	char *login;			/* Default variable for login */
	char *mtdparts;			/* Default variable for mtdparts */
	char *network;			/* Default variable for network */
	char *init;			/* Default variable for init */
	char *rootfs;			/* Default variable for rootfs */
	char *kernel;			/* Default variable for kernel */
	char *fdt;			/* Default variable for device tree */
};

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
#if defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
#define EARLY_USB "1"
#else
#define EARLY_USB NULL
#endif

const struct board_info fs_board_info[8] = {
	{	/* 0 (BT_EFUSA9) */
		.name = "efusA9",
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
	{	/* 1 (BT_ARMSTONEA9)*/
		.name = "armStoneA9",
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
	{	/* 2 (BT_PICOMODA9) */
		.name = "PicoMODA9",
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
	{	/* 3 (BT_QBLISSA9) */
		.name = "QBlissA9",
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
	{	/* 4 (BT_ARMSTONEA9R2) */
		.name = "armStoneA9r2",
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
	{	/* 5 (unknown) */
		.name = "unknown",
	},
	{	/* 6 (BT_QBLISSA9R2) */
		.name = "QBlissA9r2",
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
	{	/* 7 (BT_NETDCUA9) */
		.name = "NetDCUA9",
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
};

/* String used for system prompt */
static char fs_sys_prompt[20];

/* Copy of the NBoot args, split into hwconfig and m4config */
static struct tag_fshwconfig fs_nboot_args;
static struct tag_fsm4config fs_m4_args;

/* Get the number of the debug port reported by NBoot */
static unsigned int get_debug_port(unsigned int dwDbgSerPortPA)
{
	unsigned int port = 6;
	struct serial_device *sdev;

	do {
		sdev = get_serial_device(--port);
		if (sdev && sdev->dev.priv == (void *)dwDbgSerPortPA)
			return port;
	} while (port);

	return CONFIG_SYS_UART_PORT;
}

struct serial_device *default_serial_console(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs;

	/* As long as GD_FLG_RELOC is not set, we can not access fs_nboot_args
	   and therefore have to use the NBoot args at NBOOT_ARGS_BASE.
	   However GD_FLG_RELOC may be set before the NBoot arguments are
	   copied from NBOOT_ARGS_BASE to fs_nboot_args (see board_init()
	   below). But then at least the .bss section and therefore
	   fs_nboot_args is cleared. So if fs_nboot_args.dwDbgSerPortPA is 0,
	   the structure is not yet copied and we still have to look at
	   NBOOT_ARGS_BASE. Otherwise we can (and must) use fs_nboot_args. */
	if ((gd->flags & GD_FLG_RELOC) && fs_nboot_args.dwDbgSerPortPA)
		pargs = &fs_nboot_args;
	else
		pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;

	return get_serial_device(get_debug_port(pargs->dwDbgSerPortPA));
}

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

/* Additional pads for 24-bit LCD interface */
static iomux_v3_cfg_t const lcd24_pads_low[] = {
	IOMUX_PADS(PAD_DISP0_DAT18__GPIO5_IO12  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT19__GPIO5_IO13  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT20__GPIO5_IO14  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT21__GPIO5_IO15  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT22__GPIO5_IO16  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_DISP0_DAT23__GPIO5_IO17  | MUX_PAD_CTRL(0x3010)),
};

/* Additional pads for VLCD_ON and VCFL_ON */
static iomux_v3_cfg_t const lcd_extra_pads[] = {
	/* Signals are active high -> pull-down to switch off */
	IOMUX_PADS(PAD_SD4_DAT3__GPIO2_IO11 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_SD4_DAT0__GPIO2_IO08 | MUX_PAD_CTRL(0x3010)),
};


/* Do some very early board specific setup */
int board_early_init_f(void)
{
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;

	/*
	 * Set pull-down resistors on display signals; some displays do not
	 * like high level on data signals when VLCD is not applied yet.
	 *
	 * FIXME: This should actually only happen if display is really in
	 * use, i.e. if device tree activates lcd. However we do not know this
	 * at this point of time.
	 */
	switch (pargs->chBoardType)
	{
	case BT_ARMSTONEA9R2:		/* Boards without LCD interface */
	case BT_QBLISSA9:
	case BT_QBLISSA9R2:
		SETUP_IOMUX_PADS(lcd_extra_pads);
		break;

	case BT_NETDCUA9:		/* Boards with 24-bit LCD interface */
		SETUP_IOMUX_PADS(lcd24_pads_low);
		/* No break, fall through to default case */
	case BT_EFUSA9:
	case BT_ARMSTONEA9:
	case BT_PICOMODA9:
	default:			/* Boards with 18-bit LCD interface */
		SETUP_IOMUX_PADS(lcd18_pads_low);
		SETUP_IOMUX_PADS(lcd_extra_pads);
		break;
	}

	return 0;
}

/* Check board type */
int checkboard(void)
{
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	unsigned int boardtype = pargs->chBoardType;
	unsigned int features2;

	/* NBoot versions before VN27 did not report feature values */
	if ((be32_to_cpu(pargs->dwNBOOT_VER) & 0xFFFF) < 0x3237) { /* "27" */
		pargs->chFeatures1 = FEAT1_DEFAULT;
		pargs->chFeatures2 = FEAT2_DEFAULT;
	}
	features2 = pargs->chFeatures2;

	printf("Board: %s Rev %u.%02u (", fs_board_info[boardtype].name,
	       pargs->chBoardRev / 100, pargs->chBoardRev % 100);
	if ((features2 & FEAT2_ETH_MASK) == FEAT2_ETH_MASK)
		puts("2x ");
	if (features2 & FEAT2_ETH_MASK)
		puts("LAN, ");
	if (features2 & FEAT2_WLAN)
		puts("WLAN, ");
	if (features2 & FEAT2_EMMC)
		puts("eMMC, ");
	printf("%dx DRAM)\n", pargs->dwNumDram);

#if 0 //###
	printf("dwNumDram = 0x%08x\n", pargs->dwNumDram);
	printf("dwMemSize = 0x%08x\n", pargs->dwMemSize);
	printf("dwFlashSize = 0x%08x\n", pargs->dwFlashSize);
	printf("dwDbgSerPortPA = 0x%08x\n", pargs->dwDbgSerPortPA);
	printf("chBoardType = 0x%02x\n", pargs->chBoardType);
	printf("chBoardRev = 0x%02x\n", pargs->chBoardRev);
	printf("chFeatures1 = 0x%02x\n", pargs->chFeatures1);
	printf("chFeatures2 = 0x%02x\n", pargs->chFeatures2);
#endif

	return 0;
}

/* Set the available RAM size. We have a memory bank starting at 0x10000000
   that can hold up to 3840MB of RAM. */
int dram_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs;

	pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	gd->ram_size = pargs->dwMemSize << 20;
	gd->ram_base = CONFIG_SYS_SDRAM_BASE;

	return 0;
}

/* Now RAM is valid, U-Boot is relocated. From now on we can use variables */

/* Issue reset signal on up to three pads (~0: pad unused) */
void issue_reset(unsigned int active_us, unsigned int delay_us,
		 unsigned int pad0, unsigned int pad1, unsigned int pad2)
{
	/* Assert reset */
	gpio_direction_output(pad0, 0);
	if (pad1 != ~0)
		gpio_direction_output(pad1, 0);
	if (pad2 != ~0)
		gpio_direction_output(pad2, 0);

	/* Delay for the active pulse time */
	udelay(active_us);

	/* De-assert reset */
	gpio_set_value(pad0, 1);
	if (pad1 != ~0)
		gpio_set_value(pad1, 1);
	if (pad2 != ~0)
		gpio_set_value(pad2, 1);

	/* Delay some more time if requested */
	if (delay_us)
		udelay(delay_us);
}

static iomux_v3_cfg_t const reset_pads[] = {
	IOMUX_PADS(PAD_ENET_RXD1__GPIO1_IO26 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

int board_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	unsigned int board_type = pargs->chBoardType;

	/* Save a copy of the NBoot args */
	memcpy(&fs_nboot_args, pargs, sizeof(struct tag_fshwconfig));
	fs_nboot_args.dwSize = sizeof(struct tag_fshwconfig);
	memcpy(&fs_m4_args, pargs+1, sizeof(struct tag_fsm4config));
	fs_m4_args.dwSize = sizeof(struct tag_fsm4config);

	gd->bd->bi_arch_number = 0xFFFFFFFF;
	gd->bd->bi_boot_params = BOOT_PARAMS_BASE;

	/* Prepare the command prompt */
	sprintf(fs_sys_prompt, "%s # ", fs_board_info[board_type].name);

	/*
	 * efusA9 has a generic RESET_OUT signal to reset some arbitrary
	 * hardware. This signal is available on the efus connector pin 14 and
	 * in turn on pin 8 of the SKIT feature connector. Because there might
	 * be some rather slow hardware involved, use a rather long low pulse
	 * of 1ms.
	 *
	 * FIXME: Should we do this somewhere else when we know the pulse time?
	 */
	if (board_type == BT_EFUSA9) {
		SETUP_IOMUX_PADS(reset_pads);
		issue_reset(1000, 0, IMX_GPIO_NR(1, 26), ~0, ~0);
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
	pdata.ecc_strength = fs_nboot_args.chECCtype;
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

void board_nand_state(struct mtd_info *mtd, unsigned int state)
{
	/* Save state to pass it to Linux later */
	fs_nboot_args.chECCstate |= (unsigned char)state;
}

size_t get_env_size(void)
{
	return ENV_SIZE_DEF_LARGE;
}

size_t get_env_range(void)
{
	return ENV_RANGE_DEF_LARGE;
}

size_t get_env_offset(void)
{
	return ENV_OFFSET_DEF_LARGE;
}

#ifdef CONFIG_CMD_UPDATE
enum update_action board_check_for_recover(void)
{
	char *recover_gpio;

	/* On some platforms, the check for recovery is already done in NBoot.
	   Then the ACTION_RECOVER bit in the dwAction value is set. */
	if (fs_nboot_args.dwAction & ACTION_RECOVER)
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
	recover_gpio = getenv("recovergpio");
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
#endif

#ifdef CONFIG_GENERIC_MMC
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

/* Convert from struct fsl_esdhc_cfg to struct fus_sdhc_cfg */
#define to_fus_sdhc_cfg(x) container_of((x), struct fus_sdhc_cfg, esdhc)

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

static struct fus_sdhc_cfg sdhc_cfg[] = {
	[usdhc1_ext] = { usdhc1_sd_pads_ext, 2, 1 }, /* pads, count, USDHC# */
	[usdhc1_int] = { usdhc1_sd_pads_int, 2, 1 },
	[usdhc2_ext] = { usdhc2_sd_pads_ext, 2, 2 },
	[usdhc3_ext] = { usdhc3_sd_pads_ext, 2, 3 },
	[usdhc3_int] = { usdhc3_sd_pads_int, 3, 3 },
};

struct fus_sdhc_cd {
	const iomux_v3_cfg_t *pad;
	unsigned int gpio;
};

enum usdhc_cds {
	gpio1_io01, gpio1_io04, gpio6_io15
};

struct fus_sdhc_cd sdhc_cd[] = {
	[gpio1_io01] = { cd_gpio_1, IMX_GPIO_NR(1, 1) }, /* pad, gpio */
	[gpio1_io04] = { cd_gpio_4, IMX_GPIO_NR(1, 4) },
	[gpio6_io15] = { cd_nandf_cs2, IMX_GPIO_NR(6, 15) },
};

int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *fsl_cfg = mmc->priv;
	struct fus_sdhc_cfg *fus_cfg = to_fus_sdhc_cfg(fsl_cfg);
	u16 cd_gpio = fus_cfg->cd_gpio;

	if (cd_gpio == (u16)~0)
		return 1;		/* No CD, assume card is present */

	/* Return CD signal (active low) */
	return !gpio_get_value(cd_gpio);
}

static int setup_mmc(bd_t *bd, u8 bus_width, struct fus_sdhc_cfg *cfg,
		     struct fus_sdhc_cd *cd)
{
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	u32 ccgr6;

	/* Set CD pin configuration, activate GPIO for CD (if appropriate) */
	if (!cd)
		cfg->cd_gpio = ~0;
	else {
		cfg->cd_gpio = cd->gpio;
		imx_iomux_v3_setup_multiple_pads(cd->pad, 1);
		gpio_direction_input(cd->gpio);
	}

	/* Set DAT, CLK, CMD and RST pin configurations */
	cfg->esdhc.max_bus_width = bus_width;
	imx_iomux_v3_setup_multiple_pads(cfg->pads, cfg->count + bus_width);

	/* Get clock speed and ungate appropriate USDHC clock */
	ccgr6 = readl(&mxc_ccm->CCGR6);
	switch (cfg->index) {
	default:
	case 1:
		cfg->esdhc.esdhc_base = USDHC1_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);
		ccgr6 |= (3 << 2);
		break;
	case 2:
		cfg->esdhc.esdhc_base = USDHC2_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC2_CLK);
		ccgr6 |= (3 << 4);
		break;
	case 3:
		cfg->esdhc.esdhc_base = USDHC3_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
		ccgr6 |= (3 << 6);
		break;
	}
	writel(ccgr6, &mxc_ccm->CCGR6);

	return fsl_esdhc_initialize(bd, &cfg->esdhc);
}

int board_mmc_init(bd_t *bd)
{
	int ret = 0;

	switch (fs_nboot_args.chBoardType) {
	case BT_ARMSTONEA9:
		/* mmc0: USDHC3 (on-board micro SD slot), CD: GPIO6_IO15 */
		ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc3_ext],
				&sdhc_cd[gpio6_io15]);
		break;

	case BT_ARMSTONEA9R2:
		/* mmc0: USDHC2 (on-board micro SD slot), CD: GPIO1_IO04 */
		ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc2_ext],
				&sdhc_cd[gpio1_io04]);

		/* mmc1: USDHC3 (eMMC, if available), no CD */
		if (!ret && (fs_nboot_args.chFeatures2 & FEAT2_EMMC))
			ret = setup_mmc(bd, 8, &sdhc_cfg[usdhc3_int], NULL);
		break;

	case BT_NETDCUA9:
		/* mmc0: USDHC1 (on-board SD slot), CD: GPIO1_IO01 */
		ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc1_ext],
				&sdhc_cd[gpio1_io01]);

		/* mmc1: USDHC3 (eMMC, if available), no CD */
		if (!ret && (fs_nboot_args.chFeatures2 & FEAT2_EMMC))
			ret = setup_mmc(bd, 8, &sdhc_cfg[usdhc3_int], NULL);
		break;

	case BT_QBLISSA9:
		/* mmc0: USDHC3 (ext. SD slot via connector), CD: GPIO6_IO15 */
		ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc3_ext],
				&sdhc_cd[gpio6_io15]);

		/* mmc1: USDHC1: on-board micro SD slot (if available), no CD */
		if (!ret && !(fs_nboot_args.chFeatures2 & FEAT2_WLAN))
			ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc1_ext], NULL);
		break;

	case BT_QBLISSA9R2:
		/* Enable port power for mmc0 (USDHC2) */
		SETUP_IOMUX_PADS(pwr_qblissa9r2);
		gpio_direction_output(IMX_GPIO_NR(3, 14), 0);

		/* mmc0: USDHC2: connector, CD: GPIO1_IO04 */
		ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc2_ext],
				&sdhc_cd[gpio1_io04]);
		if (ret)
			break;

		/* mmc1: USDHC3: eMMC (if available), no CD */
		if (!ret && (fs_nboot_args.chFeatures2 & FEAT2_EMMC))
			ret = setup_mmc(bd, 8, &sdhc_cfg[usdhc3_int], NULL);
		break;

	case BT_PICOMODA9:
		/* mmc0: USDHC2 (ext. SD slot via connector), CD: GPIO1_IO04 */
		ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc2_ext],
				&sdhc_cd[gpio1_io04]);

		/* mmc1: USDHC1 (on-board micro SD or on-board eMMC), no CD
		   Remark: eMMC also only uses 4 bits if NAND is present. */
		if (!ret && !(fs_nboot_args.chFeatures2 & FEAT2_EMMC))
			ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc1_ext], NULL);
		if (!ret && (fs_nboot_args.chFeatures2 & FEAT2_EMMC))
			ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc1_int], NULL);
		break;

	case BT_EFUSA9:
		/* mmc0: USDHC2 (ext. SD slot, normal-size SD on efus SKIT) */
		ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc2_ext],
				&sdhc_cd[gpio1_io04]);

		/* mmc1: USDHC1 (ext. SD slot, micro SD on efus SKIT) */
		if (!ret)
			ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc1_ext],
					&sdhc_cd[gpio1_io01]);

		/* mmc2: USDHC3 (eMMC, if available), no CD */
		if (!ret && (fs_nboot_args.chFeatures2 & FEAT2_EMMC))
			ret = setup_mmc(bd, 8, &sdhc_cfg[usdhc3_int], NULL);
		break;

	default:
		return 0;		/* Neither SD card, nor eMMC */
	}

	return ret;
}
#endif

#ifdef CONFIG_VIDEO_IPUV3
/*
 * Display initialization sequence:
 *
 *  1. board.c: board_init_r() calls stdio_init()
 *  2. stdio.c: stdio_init() calls drv_video_init()
 *  3. cfb_console.c: drv_video_init() calls board_video_skip(); if this
 *     returns non-zero, the display will not be started
 *  4. Here: board_video_skip(): Check all disp* variables for panel settings,
 *     returns 1 on error or if no panel was given, which means do not start
 *     display driver. If display is valid, calls one of config_lvds_clk() and
 *     config_lcd_di_clk() to initialize display clocks.
 * 5a. clock.c: config_lvds_clk(): Sets clock parent for LDB_DI clock to PLL5
 *     or PLL2PFD0, sets given frequency and sets IPU_DI clock parent to LDB_D.
 * 5b. clock.c: config_lcd_di_clk(): Sets IPU_DI clock to premuxed IPU_DI clock
 *     and in turn the parent of this to PLL2PFD2 (396MHz)
 *  6: Here: board_skip_video() calls ipuv3_fb_init()
 *  7: mxc_ipuv3_fb.c: ipuv3_fb_init(): Store the given values for later use
 *  8. Here: board_skip_video() calls prepare_displays()
 *  9. Here: prepare_displays() enables the voltage for the LCD logic, in case
 *     of LCD also configures LCD pads
 * 10. cfb_console.c: drv_video_init() calls video_init()
 * 11. cfb_console.c: video_init() calls video_hw_init()
 * 12. mxc_ipuv3_fb.c: video_hw_init() calls ipu_probe()
 * 13. ipu_common.c: ipu_probe() initializes a minimal IPU clock tree
 * 14. mxc_ipuv3_fb.c: video_hw_init() calls mxcfb_probe()
 * 15. mxc_ipuv3_fb.c: mxcfb_probe() initializes framebuffer, using the values
 *     stored in step 7 above
 * 16. cfb_console.c: video_init() clears the framebuffer and calls video_logo()
 * 17. cfb_console.c: video_logo() draws either the console logo and the welcome
 *     message, or if environment variable splashimage is set, the splash
 *     screen. In this case function splash_screen_prepare() is called first.
 *     We use this function here to run variable splashprepare that should load
 *     the image to the address given in splashimage.
 * 18. cfb_console.c: drv_video_init() registers the console as stdio device,
 *     but only if board_cfb_skip() returns 0. This function could be used
 *     here to suppress the console function, e.g. if only a splash image
 *     should be shown without the option for text output.
 * 19. board.c: board_init_r() calls board_late_init()
 * 20. Here: board_late_init() calls enable_displays()
 * 21. Here: enable_displays() enables backlight voltage and sets backlight
 *     brightness (PWM) for all active displays
 */
static iomux_v3_cfg_t const lcd18_pads[] = {
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
static iomux_v3_cfg_t const lcd24_pads[] = {
	IOMUX_PADS(PAD_DISP0_DAT18__IPU1_DISP0_DATA18 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT19__IPU1_DISP0_DATA19 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT20__IPU1_DISP0_DATA20 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT21__IPU1_DISP0_DATA21 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT22__IPU1_DISP0_DATA22 | MUX_PAD_CTRL(LCD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT23__IPU1_DISP0_DATA23 | MUX_PAD_CTRL(LCD_CTRL)),
};

/* Use I2C2 to talk to RGB adapter on armStoneA9 */
I2C_PADS(armstonea9,						\
	 PAD_GPIO_3__I2C3_SCL | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 PAD_GPIO_3__GPIO1_IO03 |  MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 IMX_GPIO_NR(1, 3),					\
	 PAD_GPIO_16__I2C3_SDA | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 PAD_GPIO_16__GPIO7_IO11 | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 IMX_GPIO_NR(7, 11));

/* Use I2C1 to talk to RGB adapter on efusA9 */
I2C_PADS(efusa9,						\
	 PAD_KEY_COL3__I2C2_SCL | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 PAD_KEY_COL3__GPIO4_IO12 |  MUX_PAD_CTRL(I2C_PAD_CTRL),\
	 IMX_GPIO_NR(4, 12),					\
	 PAD_KEY_ROW3__I2C2_SDA | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 PAD_KEY_ROW3__GPIO4_IO13 | MUX_PAD_CTRL(I2C_PAD_CTRL),	\
	 IMX_GPIO_NR(4, 13));

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
		.pixclock       = 30066, // picoseconds
		.left_margin    = 88,
		.right_margin   = 40,
		.upper_margin   = 33,
		.lower_margin   = 10,
		.hsync_len      = 128,
		.vsync_len      = 2,
		.sync           = FB_SYNC_ON_GREEN | FB_SYNC_COMP_HIGH_ACT,
		.vmode          = FB_VMODE_NONINTERLACED
	},
	{
		.name           = "ChiMei-G070Y2-L01",
		.refresh        = 60,
		.xres           = 800,
		.yres           = 480,
		.pixclock       = 33500, // picoseconds
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
	 * Talk to the PCA9632 via I2C, this is a 4 channel LED driver.
	 *  Channel 0: Used as GPIO to switch backlight power
	 *  Channel 1: Used as PWM to set backlight brightness
	 *  Channel 2: Used as GPIO to set display rotation
	 *  Channel 3: Unused
	 * Channels use inverted logic, i.e. ON outputs 0, OFF outputs 1, and
	 * the higher the PWM value, the lower the duty cycle.
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
	/* Switch off display power on RGB adapter */
	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA9:
		setup_i2c(1, CONFIG_SYS_I2C_SPEED, 0x60, I2C_PADS_INFO(efusa9));
		i2c_set_bus_num(1);
		enable_i2c_backlight(0);
		break;
	case BT_ARMSTONEA9:
		setup_i2c(2, CONFIG_SYS_I2C_SPEED, 0x60, I2C_PADS_INFO(armstonea9));
		i2c_set_bus_num(2);
		enable_i2c_backlight(0);
		break;
	default:
		break;
	}

	/* Enable VLCD */
	if (used_ports & (DISP_PORT_LCD | DISP_PORT_LVDS0 | DISP_PORT_LVDS1))
		gpio_direction_output(IMX_GPIO_NR(2, 11), 1);
	mdelay(1);

	if (used_ports & DISP_PORT_LCD) {
		switch (fs_nboot_args.chBoardType) {
		case BT_ARMSTONEA9R2:	/* No LCD interface */
		case BT_QBLISSA9:
		case BT_QBLISSA9R2:
			break;

		case BT_NETDCUA9:	/* 24-bit LCD interface */
			SETUP_IOMUX_PADS(lcd24_pads);
			/* No break, fall through to default case */
		case BT_EFUSA9:		/* 18-bit LCD interface */
		case BT_ARMSTONEA9:
		case BT_PICOMODA9:
		default:
			SETUP_IOMUX_PADS(lcd18_pads);
			break;
		}
	}
}

/* Enable backlight power depending on the used display ports */
static void enable_backlight(int on)
{
	if (used_ports & (DISP_PORT_LVDS0 | DISP_PORT_LVDS1)) {
		/* Switch VCFL */
		gpio_direction_output(IMX_GPIO_NR(2, 8), on);
		mdelay(1);
		// ### TODO: Set PWM
	}

	if (used_ports & DISP_PORT_LCD) {
		switch (fs_nboot_args.chBoardType) {
		case BT_EFUSA9:
			i2c_set_bus_num(1);
			enable_i2c_backlight(on);
			break;
		case BT_ARMSTONEA9:
			i2c_set_bus_num(2);
			enable_i2c_backlight(on);
			break;
		case BT_QBLISSA9:
		case BT_QBLISSA9R2:
		case BT_NETDCUA9:
		case BT_PICOMODA9:
			/* Enable VCFL */
			gpio_direction_output(IMX_GPIO_NR(2, 8), on);
			mdelay(1);
			// ### TODO: Set PWM
			// ### TODO: Set LCD_DEN on NetDCUA9/PICOMODA9
			// ### TODO: Set LCD_ENA on PicoMODA9
			break;
		case BT_ARMSTONEA9R2:
		default:
			break;
		}
	}

	if (used_ports & DISP_PORT_HDMI) {
		// ### TODO
	}
}

static void enable_lvds(int disp, int channel,
			const struct display_params *params)
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
	mask |= bit_mapping | data_width;
	gpr2 &= ~mask;

	if (!(params->mode.sync & FB_SYNC_VERT_HIGH_ACT))
		gpr2 |= vs_polarity;
	if (params->extra.ldb_flags & LDB_FLAGS_JEIDA)
		gpr2 |= bit_mapping;
	if (params->extra.ldb_flags & LDB_FLAGS_24BPP)
		gpr2 |= data_width;
	gpr2 |= enable;

	writel(gpr2, &iomux->gpr[2]);
}

#if 0 //###
static const char *show_choice(unsigned int val,
			       const struct param_choice *choice,
			       unsigned count)
{
	unsigned int i;
	const struct param_choice *entry;

	for (i = 0, entry = choice; i < count; i++, entry++) {
		if (entry->val == val)
			return entry->name;
	}

	return "???";
}

static void show_disppara(const struct fb_extra *extra)
{
	printf("disppara: port=%s,split=%d,dual=%d,bw=%d,jeida=%d\n",
	       show_choice(extra->port, disp_ports, ARRAY_SIZE(disp_ports)),
	       (extra->ldb_flags & LDB_FLAGS_SPLIT) ? 1 : 0,
	       (extra->ldb_flags & LDB_FLAGS_DUAL) ? 1 : 0,
	       (extra->ldb_flags & LDB_FLAGS_24BPP) ? 24 : 18,
	       (extra->ldb_flags & LDB_FLAGS_JEIDA) ? 1 : 0);
}

static void show_dispmode(const struct fb_videomode *mode)
{
	printf("dispmode: clk=%lu,rate=%u,hres=%u,vres=%u,hfp=%u,hbp=%u,vfp=%u,"
	       "vbp=%u,hsw=%u,vsw=%u,hsp=%u,vsp=%u,dep=%u,clkp=%u,il=%u\n",
	       PICOS2KHZ(mode->pixclock)*1000, mode->refresh, mode->xres,
	       mode->yres, mode->right_margin, mode->left_margin,
	       mode->lower_margin, mode->upper_margin, mode->hsync_len,
	       mode->vsync_len,
	       (mode->sync & FB_SYNC_HOR_HIGH_ACT) ? 1 : 0,
	       (mode->sync & FB_SYNC_VERT_HIGH_ACT) ? 1 : 0,
	       (mode->sync & FB_SYNC_COMP_HIGH_ACT) ? 1 : 0,
	       (mode->sync & FB_SYNC_ON_GREEN) ? 1 : 0,
	       (mode->vmode & FB_VMODE_INTERLACED) ? 1 : 0);
}
#endif //###

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
	int pixfmt;
	unsigned int freq;
	const char *panel = getenv("disppanel");
	const char *mode = getenv("dispmode");

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
	switch (fs_nboot_args.chBoardType) {
	case BT_QBLISSA9:
	case BT_QBLISSA9R2:
	case BT_ARMSTONEA9R2:
	case BT_PICOMODA9:
		display.extra.port = DISP_PORT_LVDS0;
		break;
	default:
		display.extra.port = DISP_PORT_LCD;
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

	/* The LCD port on efusA9 has swapped red and blue signals */
	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA9:
		if (display.extra.port == DISP_PORT_LCD) {
			pixfmt = IPU_PIX_FMT_BGR666;
			break;
		}
		/* Fall through to default */
	default:
		pixfmt = IPU_PIX_FMT_RGB666;
		break;
	}

	/*
	 * Initialize display clock and register display settings with IPU
	 * driver. The real initialization takes place when this function
	 * returns.
	 */
	freq = PICOS2KHZ(display.mode.pixclock) * 1000;
	switch (display.extra.port) {
	case DISP_PORT_LVDS0:
		config_lvds_clk(1, 0, freq * 7, 0);
		enable_ldb_di_clk(0);
		enable_lvds(0, 0, &display);
		ipuv3_fb_init(&display.mode, 0, pixfmt);
		break;

	case DISP_PORT_LVDS1:
		config_lvds_clk(1, 1, freq * 7, 0);
		enable_ldb_di_clk(1);
		enable_lvds(1, 1, &display);
		ipuv3_fb_init(&display.mode, 1, pixfmt);
		break;

	case DISP_PORT_LCD:
		config_lcd_di_clk(1, 0);
		ipuv3_fb_init(&display.mode, 0, pixfmt);
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

	enable_ipu_clock(1);

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
 * We have two versions of the code here: If you set USE_USBNC_PWR, then the
 * power pads will be configured as dedicated function. board_ehci_power() is
 * only required for PicoMODA9 then, where USB power must always be handled by
 * a GPIO. If you do not define USE_USBNC_PWR, then all power pads will be
 * configured as GPIO and function board_ehci_power() will switch VBUS power
 * manually for all boards.
 */
#define USE_USBNC_PWR

#define USB_OTHERREGS_OFFSET	0x800
#define UCTRL_PWR_POL		(1 << 9)
#define UCTRL_OVER_CUR_DIS	(1 << 7)

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
#ifdef USE_USBNC_PWR
	IOMUX_PADS(PAD_EIM_D22__USB_OTG_PWR | MUX_PAD_CTRL(NO_PAD_CTRL)),
#else
	IOMUX_PADS(PAD_EIM_D22__GPIO3_IO22 | MUX_PAD_CTRL(NO_PAD_CTRL)),
#endif
};

/* Some boards can switch the USB Host power */
static iomux_v3_cfg_t const usb_h1_pwr_pad_efusa9[] = {
#ifdef USE_USBNC_PWR
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
#ifdef USE_USBNC_PWR
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

struct fs_usb_port_cfg {
	int mode;			/* USB_INIT_DEVICE or USB_INIT_HOST */
	int pwr_pol;			/* 0 = active high, 1 = active low */
	int pwr_gpio;			/* GPIO number to switch power */
	const char *pwr_name;		/* "usb?pwr" */
	const char *mode_name;		/* "usb?mode" */
};

static struct fs_usb_port_cfg usb_port_cfg[2] = {
	{
		.mode = USB_INIT_DEVICE, /* USB OTG port */
		.pwr_pol = 0,
		.pwr_gpio = ~0,
		.pwr_name = "usb0pwr",
		.mode_name = "usb0mode",
	},
	{
		.mode = USB_INIT_HOST,	/* USB H1 port */
		.pwr_pol = 0,
		.pwr_gpio = ~0,
		.pwr_name = "usb1pwr",
		.mode_name = "usb1mode",
	},
};

/* Setup pad for reset signal and issue 2ms reset for USB hub */
static void fs_usb_reset_hub(iomux_v3_cfg_t const *reset_pad,
			     unsigned int reset_gpio)
{
	if (reset_pad)
		imx_iomux_v3_setup_multiple_pads(reset_pad, 1);
	if (reset_gpio != ~0)
		issue_reset(2000, 0, reset_gpio, ~0, ~0);
}

/* Check if OTG port should be started as host or device */
static int fs_usb_get_otg_mode(iomux_v3_cfg_t const *id_pad, unsigned id_gpio,
			       const char *mode_name, int default_mode)
{
	const char *envvar = getenv(mode_name);

	if (!envvar)
		return default_mode;
	if (!strcmp(envvar, "peripheral") || !strcmp(envvar, "device"))
		return USB_INIT_DEVICE;	/* Requested by user as device */
	if (!strcmp(envvar, "host"))
		return USB_INIT_HOST;	/* Requested by user as host */
	if (strcmp(envvar, "otg"))
		return default_mode;	/* Unknown mode setting */

	/* "OTG" mode, check ID pin to decide */
	if (!id_pad || (id_gpio == ~0))
		return default_mode;	/* No ID pad available */

	/* ID pad must be set up as GPIO with an internal pull-up */
	imx_iomux_v3_setup_multiple_pads(id_pad, 1);
	gpio_direction_input(id_gpio);
	udelay(100);			/* Let voltage settle */
	if (gpio_get_value(id_gpio))
		return USB_INIT_DEVICE;	/* ID pulled up: device mode */

	return USB_INIT_HOST;		/* ID connected with GND: host mode */
}

/* Determine USB host power polarity */
static int fs_usb_get_pwr_pol(const char *pwr_name, int default_pol)
{
	const char *envvar = getenv(pwr_name);

	if (!envvar)
		return default_pol;

	/* Skip optional prefix "active", "active-" or "active_" */
	if (!strncmp(envvar, "active", 6)) {
		envvar += 6;
		if ((*envvar == '-') || (*envvar == '_'))
			envvar++;
	}

	if (!strcmp(envvar, "high"))
		return 0;
	if (!strcmp(envvar, "low"))
		return 1;

	return default_pol;
}

/* Set up power pad and polarity; if GPIO, switch off for now */
static void fs_usb_config_pwr(iomux_v3_cfg_t const *pwr_pad, unsigned pwr_gpio,
			      int port, int pol)
{
	/* Configure pad */
	if (pwr_pad)
		imx_iomux_v3_setup_multiple_pads(pwr_pad, 1);

	/* Use as GPIO or set power polarity in USB controller */
	if (pwr_gpio != ~0)
		gpio_direction_output(pwr_gpio, pol);
#ifdef USE_USBNC_PWR
	else {
		u32 *usbnc_usb_ctrl;
		u32 val;

		usbnc_usb_ctrl = (u32 *)(USB_BASE_ADDR + USB_OTHERREGS_OFFSET +
					 port * 4);
		val = readl(usbnc_usb_ctrl);
		if (pol)
			val &= ~UCTRL_PWR_POL;
		else
			val |= UCTRL_PWR_POL;

		/* Disable over-current detection */
		val |= UCTRL_OVER_CUR_DIS;
		writel(val, usbnc_usb_ctrl);
	}
#endif
}

/* Check if port is Host or Device */
int board_usb_phy_mode(int port)
{
	if (port > 1)
		return USB_INIT_HOST;	/* Unknown port */

	return usb_port_cfg[port].mode;
}

/* Switch VBUS power via GPIO */
int board_ehci_power(int port, int on)
{
	struct fs_usb_port_cfg *port_cfg;

	if (port > 1)
		return 0;		/* Unknown port */

	port_cfg = &usb_port_cfg[port];
	if (port_cfg->mode != USB_INIT_HOST)
		return 0;		/* Port not in host mode */

	if (port_cfg->pwr_gpio == ~0)
		return 0;		/* PWR not handled by GPIO */

	if (port_cfg->pwr_pol)
		on = !on;		/* Invert polarity */

	gpio_set_value(port_cfg->pwr_gpio, on);

	return 0;
}

/* Init one USB port */
int board_ehci_hcd_init(int port)
{
	iomux_v3_cfg_t const *pwr_pad, *reset_pad, *id_pad;
	unsigned int pwr_gpio, reset_gpio, id_gpio;
	int pwr_pol;
	struct fs_usb_port_cfg *port_cfg;

	if (port > 1)
		return 0;		/* Unknown port */

	port_cfg = &usb_port_cfg[port];

	/* Default settings, board specific code below will override */
	reset_pad = NULL;
	reset_gpio = ~0;
	pwr_pad = NULL;
	pwr_gpio = ~0;
	pwr_pol = 0;

	/* Determine host power pad and USB hub reset pad (if applicable) */
	if (port == 0) {
		/* Handle USB OTG port (USB0); Step 1: check OTG mode */
		id_pad = NULL;
		id_gpio = ~0;

		switch (fs_nboot_args.chBoardType) {
		case BT_PICOMODA9:
			id_pad = usb_otg_id_pad_picomoda9;
			id_gpio = IMX_GPIO_NR(1, 1);
			break;

		case BT_EFUSA9:
		case BT_ARMSTONEA9:
		case BT_ARMSTONEA9R2:
		case BT_QBLISSA9:
		case BT_QBLISSA9R2:
			id_pad = usb_otg_id_pad_other;
			id_gpio = IMX_GPIO_NR(1, 24);
			break;

		case BT_NETDCUA9:
			/* No ID pad available */
			break;

		default:
			/* No host mode available */
			port_cfg->mode = USB_INIT_DEVICE;
			return 0;
		}
		port_cfg->mode = fs_usb_get_otg_mode(id_pad, id_gpio,
						     port_cfg->mode_name,
						     USB_INIT_DEVICE);
		if (port_cfg->mode != USB_INIT_HOST)
			return 0;	/* OTG port not in host mode */

		/* Step 2: determine host power pad */
		switch (fs_nboot_args.chBoardType) {
		case BT_QBLISSA9:
			/* No USB_OTG_PWR before board revision 1.30 */
			if (fs_nboot_args.chBoardRev < 130)
				break;
			/* Fall through to case BT_QBLISSA9R2 */
		case BT_QBLISSA9R2:
		case BT_EFUSA9:
		case BT_ARMSTONEA9R2:
			/* OTG host power on EIM_D22, active low */
			pwr_pol = 1;
			pwr_pad = usb_otg_pwr_pad_other;
#ifndef USE_USBNC_PWR
			pwr_gpio = IMX_GPIO_NR(3, 22);
#endif
			break;

		case BT_PICOMODA9:
			/* OTG host power on GPIO_8 (only GPIO) */
			pwr_pad = usb_otg_pwr_pad_picomoda9;
			pwr_gpio = IMX_GPIO_NR(1, 8);
			break;

		case BT_NETDCUA9:
			/* OTG host power on GPIO_9 (only GPIO, active high) */
			pwr_pad = usb_otg_pwr_pad_netdcua9;
			pwr_gpio = IMX_GPIO_NR(1, 9);
			break;

		case BT_ARMSTONEA9:
		default:
			/* No USB_OTG_PWR */
			break;
		}
	} else {
		/* Handle USB H1 port (USB1) */
		switch (fs_nboot_args.chBoardType) {
		case BT_EFUSA9:
			/* No reset for USB hub, USB power on EIM_D31 */
			pwr_pad = usb_h1_pwr_pad_efusa9;
#ifndef USE_USBNC_PWR
			pwr_gpio = IMX_GPIO_NR(3, 31);
#endif
			break;

		case BT_ARMSTONEA9:
		case BT_QBLISSA9:
			/* Hub reset on GPIO_17, USB host power is always on */
			reset_pad = usb_hub_reset_pad_armstonea9_qblissa9;
			reset_gpio = IMX_GPIO_NR(7, 12);
			break;

		case BT_ARMSTONEA9R2:
			/* Hub reset on EIM_EB1, USB host power is always on */
			reset_pad = usb_hub_reset_pad_armstonea9r2;
			reset_gpio = IMX_GPIO_NR(2, 29);
			break;

		case BT_QBLISSA9R2:
			/* Hub reset on EIM_DA15, USB host power always on */
			reset_pad = usb_hub_reset_pad_armstonea9r2;
			reset_gpio = IMX_GPIO_NR(3, 15);
			break;

		case BT_PICOMODA9:
			/* No USB hub, USB host power on GPIO_17 or GPIO_5 */
			if (fs_nboot_args.chBoardRev < 110) {
				pwr_pad = usb_h1_pwr_pad_picomoda9_REV100;
				pwr_gpio = IMX_GPIO_NR(7, 12);
			} else {
				pwr_pad = usb_h1_pwr_pad_picomoda9;
				pwr_gpio = IMX_GPIO_NR(1, 5);
			}
			break;

		case BT_NETDCUA9:
			/* No USB hub, USB host power on GPIO_0 */
			pwr_pad = usb_h1_pwr_pad_netdcua9;
#ifndef USE_USBNC_PWR
			pwr_gpio = IMX_GPIO_NR(1, 0);
#endif
			break;

		default:
			break;
		}
	}

	/* Reset a hub, if present */
	fs_usb_reset_hub(reset_pad, reset_gpio);

	/* Set up the host power pad incl. polarity */
	port_cfg->pwr_gpio = pwr_gpio;
	port_cfg->pwr_pol = fs_usb_get_pwr_pol(port_cfg->pwr_name, pwr_pol);
	fs_usb_config_pwr(pwr_pad, pwr_gpio, port, port_cfg->pwr_pol);

	return 0;
}
#endif /* CONFIG_USB_EHCI_MX6 */

#ifdef CONFIG_BOARD_LATE_INIT
void setup_var(const char *varname, const char *content, int runvar)
{
	char *envvar = getenv(varname);

	/* If variable is not set or does not contain string "undef", do not
	   change it */
	if (!envvar || strcmp(envvar, "undef"))
		return;

	/* Either set variable directly with value ... */
	if (!runvar) {
		setenv(varname, content);
		return;
	}

	/* ... or set variable by running the variable with name in content */
	content = getenv(content);
	if (content)
		run_command(content, 0);
}

/* Use this slot to init some final things before the network is started. We
   set up some environment variables for things that are board dependent and
   can't be defined as a fix value in fsvybrid.h. As an unset value is valid
   for some of these variables, we check for the special value "undef". Any
   of these variables that holds this value will be replaced with the
   board-specific value. */
int board_late_init(void)
{
	unsigned int boardtype = fs_nboot_args.chBoardType;
	const struct board_info *bi = &fs_board_info[boardtype];
	const char *envvar;

	/* Set sercon variable if not already set */
	envvar = getenv("sercon");
	if (!envvar || !strcmp(envvar, "undef")) {
		char sercon[DEV_NAME_SIZE];

		sprintf(sercon, "%s%c", CONFIG_SYS_SERCON_NAME,
			'0' + get_debug_port(fs_nboot_args.dwDbgSerPortPA));
		setenv("sercon", sercon);
	}

	/* Set platform variable if not already set */
	envvar = getenv("platform");
	if (!envvar || !strcmp(envvar, "undef")) {
		char lcasename[20];
		char *p = bi->name;
		char *l = lcasename;
		char c;

		do {
			c = *p++;
			if ((c >= 'A') && (c <= 'Z'))
				c += 'a' - 'A';
			*l++ = c;
		} while (c);

		if (is_cpu_type(MXC_CPU_MX6SOLO) ||is_cpu_type(MXC_CPU_MX6DL))
			sprintf(lcasename, "%sdl", lcasename);
		else if (is_cpu_type(MXC_CPU_MX6D) || is_cpu_type(MXC_CPU_MX6Q))
			sprintf(lcasename, "%sq", lcasename);

		setenv("platform", lcasename);
	}

	/* Set some variables with a direct value */
	setup_var("bootdelay", bi->bootdelay, 0);
	setup_var("updatecheck", bi->updatecheck, 0);
	setup_var("installcheck", bi->installcheck, 0);
	setup_var("recovercheck", bi->recovercheck, 0);
	setup_var("earlyusbinit", bi->earlyusbinit, 0);
	setup_var("mtdids", MTDIDS_DEFAULT, 0);
	setup_var("partition", MTDPART_DEFAULT, 0);
	setup_var("mode", CONFIG_MODE, 0);

	/* Set some variables by runnning another variable */
	setup_var("console", bi->console, 1);
	setup_var("login", bi->login, 1);
	setup_var("mtdparts", bi->mtdparts, 1);
	setup_var("network", bi->network, 1);
	setup_var("init", bi->init, 1);
	setup_var("rootfs", bi->rootfs, 1);
	setup_var("kernel", bi->kernel, 1);
	setup_var("bootfdt", "set_bootfdt", 1);
	setup_var("fdt", bi->fdt, 1);
	setup_var("bootargs", "set_bootargs", 1);

#ifdef CONFIG_VIDEO_IPUV3
	/* Enable backlight for displays */
	enable_backlight(1);
#endif

	return 0;
}
#endif

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
	u32 ccgr6, gpr1;

	SETUP_IOMUX_PADS(eim_pads_eth_b);

	/* Enable EIM clock */
	ccgr6 = readl(CCM_CCGR6);
	writel(ccgr6 | MXC_CCM_CCGR6_EMI_SLOW_MASK, CCM_CCGR6);

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

/* Read a MAC address from OTP memory */
int get_otp_mac(void *otp_addr, uchar *enetaddr)
{
	u32 val;
	static const uchar empty1[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	static const uchar empty2[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	/*
	 * Read a MAC address from OTP memory on i.MX6; it is stored in the
	 * following order:
	 *
	 *   Byte 1 in mac_h[31:24]
	 *   Byte 2 in mac_h[23:16]
	 *   Byte 3 in mac_h[15:8]
	 *   Byte 4 in mac_h[7:0]
	 *   Byte 5 in mac_l[31:24]
	 *   Byte 6 in mac_l[23:16]
	 *
	 * Please note that this layout is different to Vybrid.
	 *
	 * The MAC address itself can be empty (all six bytes zero) or erased
	 * (all six bytes 0xFF). In this case the whole address is ignored.
	 *
	 * In addition to the address itself, there may be a count stored in
	 * mac_l[7:0].
	 *
	 *   count=0: only the address itself
	 *   count=1: the address itself and the next address
	 *   count=2: the address itself and the next two addresses
	 *   etc.
	 *
	 * count=0xFF is a special case (erased count) and must be handled
	 * like count=0. The count is only valid if the MAC address itself is
	 * valid (not all zeroes and not all 0xFF).
	 */
	val = __raw_readl(otp_addr);
	enetaddr[0] = val >> 24;
	enetaddr[1] = (val >> 16) & 0xFF;
	enetaddr[2] = (val >> 8) & 0xFF;
	enetaddr[3] = val & 0xFF;

	val = __raw_readl(otp_addr + 0x10);
	enetaddr[4] = val >> 24;
	enetaddr[5] = (val >> 16) & 0xFF;

	if (!memcmp(enetaddr, empty1, 6) || !memcmp(enetaddr, empty2, 6))
		return 0;

	val &= 0xFF;
	if (val == 0xFF)
		val = 0;

	return (int)(val + 1);
}


/* Set the ethaddr environment variable according to index */
void set_fs_ethaddr(int index)
{
	struct ocotp_regs *ocotp = (struct ocotp_regs *)OCOTP_BASE_ADDR;
	struct fuse_bank *bank = &ocotp->bank[4];
	uchar enetaddr[6];
	int count, i;
	int offs = index;

	/*
	 * Try to fulfil the request in the following order:
	 *   1. From environment variable
	 *   2. MAC0 from OTP
	 *   3. CONFIG_ETHADDR_BASE
	 */
	if (eth_getenv_enetaddr_by_index("eth", index, enetaddr))
		return;

	count = get_otp_mac(&bank->fuse_regs[8], enetaddr);
	if (count <= offs) {
		offs -= count;
		eth_parse_enetaddr(MK_STR(CONFIG_ETHADDR_BASE), enetaddr);
	}

	i = 6;
	do {
		offs += (int)enetaddr[--i];
		enetaddr[i] = offs & 0xFF;
		offs >>= 8;
	} while (i);

	eth_setenv_enetaddr_by_index("eth", index, enetaddr);
}

int board_eth_init(bd_t *bis)
{
	u32 gpr1;
	int ret;
	int phy_addr;
	int reset_gpio;
	enum xceiver_type xcv_type;
	enum enet_freq freq;
	struct iomuxc *iomux_regs = (struct iomuxc *)IOMUXC_BASE_ADDR;
	int id = 0;

	/* Activate on-chip ethernet port (FEC) */
	if (fs_nboot_args.chFeatures2 & FEAT2_ETH_A) {
		set_fs_ethaddr(id);

		switch (fs_nboot_args.chBoardType) {
		case BT_PICOMODA9:
		case BT_NETDCUA9:
			/* Use 100 MBit/s LAN on RMII pins */
			if (fs_nboot_args.chBoardType == BT_PICOMODA9)
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
		switch (fs_nboot_args.chBoardType) {
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
			if (fs_nboot_args.chBoardType == BT_PICOMODA9)
				reset_gpio = IMX_GPIO_NR(2, 10);
			else
				reset_gpio = IMX_GPIO_NR(1, 2);
			issue_reset(10, 170000, reset_gpio, ~0, ~0);
			phy_addr = 1;
			xcv_type = RMII;
			break;

		default:
			/* Atheros AR8035: Assert reset for at least 1ms */
			issue_reset(1000, 0, IMX_GPIO_NR(1, 25), ~0,~0);
			phy_addr = 4;
			xcv_type = RGMII;
			break;
		}

		ret = fecmxc_initialize_multi_type(bis, -1, phy_addr,
						   ENET_BASE_ADDR, xcv_type);
		if (ret < 0)
			return ret;

		id++;
	}

	/* If available, activate external ethernet port (AX88796B) */
	if (fs_nboot_args.chFeatures2 & FEAT2_ETH_B) {
		/* AX88796B is connected via EIM */
		setup_weim(bis);

		/* Reset AX88796B, on NetDCUA9 */
		issue_reset(200, 0, IMX_GPIO_NR(1, 3), ~0, ~0);

		/* Initialize AX88796B */
		ret = ax88796_initialize(-1, CONFIG_DRIVER_AX88796_BASE,
					 AX88796_MODE_BUS16_DP16);

		set_fs_ethaddr(id++);
	}

	/* If WLAN is available, just set ethaddr variable */
	if (fs_nboot_args.chFeatures2 & FEAT2_WLAN)
		set_fs_ethaddr(id++);

	return 0;
}
#endif /* CONFIG_CMD_NET */

/* Return the board name; we have different boards that use this file, so we
   can not define the board name with CONFIG_SYS_BOARDNAME */
char *get_board_name(void)
{
	return fs_board_info[fs_nboot_args.chBoardType].name;
}


/* Return the system prompt; we can not define it with CONFIG_SYS_PROMPT
   because we want to include the board name, which is variable (see above) */
char *get_sys_prompt(void)
{
	return fs_sys_prompt;
}

#ifdef CONFIG_CMD_LED
/*
 * Boards                             STA1           STA2         Active
 * ------------------------------------------------------------------------
 * PicoMODA9 (Rev 1.00)  extern       GPIO5_IO04     GPIO5_IO02   low
 * PicoMODA9 (newer)     extern       GPIO7_IO12     GPIO7_IO13   low
 *                       on-board     GPIO5_IO28     GPIO5_IO29   low
 * armStoneA9, QBlissA9               GPIO4_IO06     GPIO4_IO07   high
 * efusA9, armStoneA9r2, QBlissA9r2   GPIO7_IO12     GPIO7_IO13   high
 */
static unsigned int led_value;

static unsigned int get_led_gpio(struct tag_fshwconfig *pargs, led_id_t id,
				 int val, int index)
{
	unsigned int gpio;

	if (val)
		led_value |= (1 << id);
	else
		led_value &= ~(1 << id);

	switch (pargs->chBoardType) {
	case BT_PICOMODA9:
		if (pargs->chBoardRev == 100)
			gpio = (id ? IMX_GPIO_NR(5, 2) : IMX_GPIO_NR(5, 4));
		else if (!index)
			gpio = (id ? IMX_GPIO_NR(7, 13) : IMX_GPIO_NR(7, 12));
		else
			gpio = (id ? IMX_GPIO_NR(5, 29) : IMX_GPIO_NR(5, 28));
		break;

	case BT_ARMSTONEA9:
	case BT_QBLISSA9:
		gpio = (id ? IMX_GPIO_NR(4, 7) : IMX_GPIO_NR(4, 6));
		break;

	case BT_EFUSA9:
	case BT_NETDCUA9:
	case BT_ARMSTONEA9R2:
	case BT_QBLISSA9R2:
	default:			/* efusA9, armStoneA9r2, NetDCUA9 */
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
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	int val = (pargs->chBoardType == BT_PICOMODA9);

	if (val && (pargs->chBoardRev > 100)) {
		gpio_direction_output(get_led_gpio(pargs, 0, val, 1), val);
		gpio_direction_output(get_led_gpio(pargs, 1, val, 1), val);
	}
	gpio_direction_output(get_led_gpio(pargs, 0, val, 0), val);
	gpio_direction_output(get_led_gpio(pargs, 1, val, 0), val);
}

void __led_set(led_id_t id, int val)
{
	struct tag_fshwconfig *pargs = &fs_nboot_args;

	if (id > 1)
		return;

	if (pargs->chBoardType == BT_PICOMODA9) {
		val = !val;
		if (pargs->chBoardRev > 100)
			gpio_set_value(get_led_gpio(pargs, id, val, 1), val);
	}

	gpio_set_value(get_led_gpio(pargs, id, val, 0), val);
}

void __led_toggle(led_id_t id)
{
	struct tag_fshwconfig *pargs = &fs_nboot_args;
	int val;

	if (id > 1)
		return;

	val = !((led_value >> id) & 1);
	if ((pargs->chBoardType == BT_PICOMODA9) && (pargs->chBoardRev > 100))
		gpio_set_value(get_led_gpio(pargs, id, val, 1), val);
	gpio_set_value(get_led_gpio(pargs, id, val, 0), val);
}
#endif /* CONFIG_CMD_LED */

#ifdef CONFIG_OF_BOARD_SETUP
/* Set a generic value, if it was not already set in the device tree */
static void fus_fdt_set_val(void *fdt, int offs, const char *name,
			    const void *val, int len, int force)
{
	int err;

	/* Warn if property already exists in device tree */
	if (fdt_get_property(fdt, offs, name, NULL) != NULL) {
		printf("## %s property %s/%s from device tree!\n",
		       force ? "Overwriting": "Keeping",
		       fdt_get_name(fdt, offs, NULL), name);
		if (!force)
			return;
	}

	err = fdt_setprop(fdt, offs, name, val, len);
	if (err) {
		printf("## Unable to update property %s/%s: err=%s\n",
		       fdt_get_name(fdt, offs, NULL), name, fdt_strerror(err));
	}
}

/* Set a string value */
static void fus_fdt_set_string(void *fdt, int offs, const char *name,
			       const char *str, int force)
{
	fus_fdt_set_val(fdt, offs, name, str, strlen(str) + 1, force);
}

/* Set a u32 value as a string (usually for bdinfo) */
static void fus_fdt_set_u32str(void *fdt, int offs, const char *name,
			       u32 val, int force)
{
	char str[12];

	sprintf(str, "%u", val);
	fus_fdt_set_string(fdt, offs, name, str, force);
}

/* Set a u32 value */
static void fus_fdt_set_u32(void *fdt, int offs, const char *name,
			    u32 val, int force)
{
	fdt32_t tmp = cpu_to_fdt32(val);

	fus_fdt_set_val(fdt, offs, name, &tmp, sizeof(tmp), force);
}

/* Set ethernet MAC address aa:bb:cc:dd:ee:ff for given index */
static void fus_fdt_set_macaddr(void *fdt, int offs, int id)
{
	uchar enetaddr[6];
	char name[10];
	char str[20];

	if (eth_getenv_enetaddr_by_index("eth", id, enetaddr)) {
		sprintf(name, "MAC%d", id);
		sprintf(str, "%pM", enetaddr);
		fus_fdt_set_string(fdt, offs, name, str, 1);
	}
}

/* If environment variable exists, set a string property with the same name */
static void fus_fdt_set_getenv(void *fdt, int offs, const char *name, int force)
{
	const char *str;

	str = getenv(name);
	if (str)
		fus_fdt_set_string(fdt, offs, name, str, force);
}

/* Open a node, warn if the node does not exist */
static int fus_fdt_path_offset(void *fdt, const char *path)
{
	int offs;

	offs = fdt_path_offset(fdt, path);
	if (offs < 0) {
		printf("## Can not access node %s: err=%s\n",
		       path, fdt_strerror(offs));
	}

	return offs;
}

/* Enable or disable node given by path, overwrite any existing status value */
static void fus_fdt_enable(void *fdt, const char *path, int enable)
{
	int offs, err, len;
	const void *val;
	char *str = enable ? "okay" : "disabled";

	offs = fdt_path_offset(fdt, path);
	if (offs < 0)
		return;

	/* Do not change if status already exists and has this value */
	val = fdt_getprop(fdt, offs, "status", &len);
	if (val && len && !strcmp(val, str))
		return;

	/* No, set new value */
	err = fdt_setprop_string(fdt, offs, "status", str);
	if (err) {
		printf("## Can not set status of node %s: err=%s\n",
		       path, fdt_strerror(err));
	}
}

/* Do any additional board-specific device tree modifications */
void ft_board_setup(void *fdt, bd_t *bd)
{
	int offs;

	printf("   Setting run-time properties\n");

	/* Set ECC strength for NAND driver */
	offs = fus_fdt_path_offset(fdt, FDT_NAND);
	if (offs >= 0) {
		fus_fdt_set_u32(fdt, offs, "fus,ecc_strength",
				fs_nboot_args.chECCtype, 1);
	}

	/* Set bdinfo entries */
	offs = fus_fdt_path_offset(fdt, "/bdinfo");
	if (offs >= 0) {
		int id = 0;
		char rev[6];

		/* NAND info, names and features */
		fus_fdt_set_u32str(fdt, offs, "ecc_strength",
				   fs_nboot_args.chECCtype, 1);
		fus_fdt_set_u32str(fdt, offs, "nand_state",
				   fs_nboot_args.chECCstate, 1);
		fus_fdt_set_string(fdt, offs, "board_name",
				   get_board_name(), 0);
		sprintf(rev, "%d.%02d", fs_nboot_args.chBoardRev / 100,
			fs_nboot_args.chBoardRev % 100);
		fus_fdt_set_string(fdt, offs, "board_revision", rev, 1);
		fus_fdt_set_getenv(fdt, offs, "platform", 0);
		fus_fdt_set_getenv(fdt, offs, "arch", 1);
		fus_fdt_set_u32str(fdt, offs, "features1",
				   fs_nboot_args.chFeatures1, 1);
		fus_fdt_set_u32str(fdt, offs, "features2",
				   fs_nboot_args.chFeatures2, 1);
		fus_fdt_set_string(fdt, offs, "reset_cause",
				   get_reset_cause(), 1);
		memcpy(rev, &fs_nboot_args.dwNBOOT_VER, 4);
		rev[4] = 0;
		fus_fdt_set_string(fdt, offs, "nboot_version", rev, 1);
		fus_fdt_set_string(fdt, offs, "u-boot_version",
				   version_string, 1);

		/* MAC addresses */
		if (fs_nboot_args.chFeatures2 & FEAT2_ETH_A)
			fus_fdt_set_macaddr(fdt, offs, id++);
		if (fs_nboot_args.chFeatures2 & FEAT2_ETH_B)
			fus_fdt_set_macaddr(fdt, offs, id++);
		if (fs_nboot_args.chFeatures2 & FEAT2_WLAN)
			fus_fdt_set_macaddr(fdt, offs, id++);
	}

	/* Disable ethernet node(s) if feature is not available */
	if (!(fs_nboot_args.chFeatures2 & FEAT2_ETH_A))
		fus_fdt_enable(fdt, FDT_ETH_A, 0);
#if 0
	if (!(fs_nboot_args.chFeatures2 & FEAT2_ETH_B))
		fus_fdt_enable(fdt, FDT_ETH_B, 0);
#endif
}
#endif /* CONFIG_OF_BOARD_SETUP */

/* Board specific cleanup before Linux is started */
void board_preboot_os(void)
{
	/* Switch off display and backlight voltages */
	enable_backlight(0);

	/* Shut down all ethernet PHYs (suspend mode) */
	mdio_shutdown_all();
}
