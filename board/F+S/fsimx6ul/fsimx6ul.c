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
#endif
#ifdef CONFIG_CMD_LCD
#include <cmd_lcd.h>			/* PON_*, POFF_* */
#endif
#include <serial.h>			/* struct serial_device */

#ifdef CONFIG_GENERIC_MMC
#include <mmc.h>
#include <fsl_esdhc.h>			/* fsl_esdhc_initialize(), ... */
#endif

#ifdef CONFIG_CMD_LED
#include <status_led.h>			/* led_id_t */
#endif

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
#include <malloc.h>			/* free() */
#include <fdt_support.h>		/* do_fixup_by_path_u32(), ... */

/* ------------------------------------------------------------------------- */

#define NBOOT_ARGS_BASE (PHYS_SDRAM + 0x00001000) /* Arguments from NBoot */
#define BOOT_PARAMS_BASE (PHYS_SDRAM + 0x100)	  /* Arguments to Linux */

#define BT_EFUSA7UL   0
#define BT_CUBEA7UL   1
#define BT_PICOCOM1_2 2
#define BT_CUBE2_0    3
#define BT_GAR1       4

/* Features set in tag_fshwconfig.chFeature2 (available since NBoot VN27) */
#define FEAT2_ETH_A   (1<<0)		/* 0: no LAN0, 1; has LAN0 */
#define FEAT2_ETH_B   (1<<1)		/* 0: no LAN1, 1; has LAN1 */
#define FEAT2_EMMC    (1<<2)		/* 0: no eMMC, 1: has eMMC */
#define FEAT2_WLAN    (1<<3)		/* 0: no WLAN, 1: has WLAN */
#define FEAT2_HDMICAM (1<<4)		/* 0: LCD-RGB, 1: HDMI+CAM (PicoMOD) */
#define FEAT2_ETH_MASK (FEAT2_ETH_A | FEAT2_ETH_B)

/* NBoot before VN27 did not report feature values; use reasonable defaults */
#define FEAT1_DEFAULT 0
#define FEAT2_DEFAULT (FEAT2_ETH_A | FEAT2_ETH_B | FEAT2_EMMC | FEAT2_WLAN)

#define ACTION_RECOVER 0x00000040	/* Start recovery instead of update */

#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)

/* Device tree paths */
#define FDT_NAND	"/soc/gpmi-nand@01806000"
#define FDT_ETH_A	"/soc/aips-bus@02100000/ethernet@02188000"
#define FDT_ETH_B	"/soc/aips-bus@02000000/ethernet@020b4000"

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

#define USDHC_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_47K_UP  | PAD_CTL_SPEED_MED |		\
	PAD_CTL_DSE_80ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define USDHC_CLK_CTRL (PAD_CTL_SPEED_MED |		\
	PAD_CTL_DSE_120ohm | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)


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

struct fus_sdhc_cfg {
	struct fsl_esdhc_cfg esdhc;
	unsigned int cd_gpio;
};

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

const struct board_info fs_board_info[8] = {
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
	{	/* 5 (unknown) */
		.name = "unknown",
	},
	{	/* 6 (unknown) */
		.name = "unknown",
	},
	{	/* 7 (unknown) */
		.name = "unknown",
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
static iomux_v3_cfg_t const lcd18_pads[] = {
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
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	unsigned int board_type = pargs->chBoardType - 16;
	/*
	 * Set pull-down resistors on display signals; some displays do not
	 * like high level on data signals when VLCD is not applied yet.
	 *
	 * FIXME: This should actually only happen if display is really in
	 * use, i.e. if device tree activates lcd. However we do not know this
	 * at this point of time.
	 */
	switch (board_type)
	{
	case BT_PICOCOM1_2:		/* Boards without LCD interface */
	case BT_CUBEA7UL:
	case BT_CUBE2_0:
		break;

	case BT_GAR1:
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

	default:			/* Boards with 18-bit LCD interface */
		SETUP_IOMUX_PADS(lcd18_pads);
		break;
	}

	return 0;
}

/* Check board type */
int checkboard(void)
{
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	unsigned int boardtype = pargs->chBoardType - 16;
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
   that can hold up to 3840MB of RAM. However up to now we only have 256MB or
   512MB on F&S i.MX6 boards. */
int dram_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs;

	pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	gd->ram_size = pargs->dwMemSize << 20;
	gd->ram_base = PHYS_SDRAM;

	return 0;
}

/* Now RAM is valid, U-Boot is relocated. From now on we can use variables */

static iomux_v3_cfg_t const reset_pads[] = {
	IOMUX_PADS(PAD_BOOT_MODE1__GPIO5_IO11 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

int board_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	unsigned int board_type = pargs->chBoardType - 16;

	/* Save a copy of the NBoot args */
	memcpy(&fs_nboot_args, pargs, sizeof(struct tag_fshwconfig));
	fs_nboot_args.chBoardType = board_type;
	fs_nboot_args.dwSize = sizeof(struct tag_fshwconfig);
	memcpy(&fs_m4_args, pargs+1, sizeof(struct tag_fsm4config));
	fs_m4_args.dwSize = sizeof(struct tag_fsm4config);

	gd->bd->bi_arch_number = 0xFFFFFFFF;
	gd->bd->bi_boot_params = BOOT_PARAMS_BASE;

	/* Prepare the command prompt */
	sprintf(fs_sys_prompt, "%s # ", fs_board_info[board_type].name);

	if (fs_nboot_args.chBoardType ==  BT_EFUSA7UL) {
		/* Reset board and SKIT hardware like ETH PHY, PCIe, USB-Hub, WLAN (if
		available). This is on pad BOOT_MODE1 (GPIO5_IO10). Because there
		may be some completely different hardware connected to this general
		RESETOUTn pin, use a rather long low pulse of 100ms. */
		SETUP_IOMUX_PADS(reset_pads);
		gpio_direction_output(IMX_GPIO_NR(5, 11), 0);
		mdelay(100);
		gpio_set_value(IMX_GPIO_NR(5, 11), 1);
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
	/* If the board should do an automatic recovery is given in the
	   dwAction value. Currently this is only defined for CUBEA5, AGATEWAY
	   and HGATEWAY. If a special button is pressed for a defined time
	   when power is supplied, the system should be reset to the default
	   state, i.e. perform a complete recovery. The button is detected in
	   NBoot, but recovery takes place in U-Boot. */
	if (fs_nboot_args.dwAction & ACTION_RECOVER)
		return UPDATE_ACTION_RECOVER;
	return UPDATE_ACTION_UPDATE;
}
#endif

#ifdef CONFIG_GENERIC_MMC
static iomux_v3_cfg_t const usdhc1_pads[] = {
	IOMUX_PADS(PAD_SD1_CMD__USDHC1_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_CLK__USDHC1_CLK | MUX_PAD_CTRL(USDHC_CLK_CTRL)),
	IOMUX_PADS(PAD_SD1_DATA0__USDHC1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_DATA1__USDHC1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_DATA2__USDHC1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_DATA3__USDHC1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
};

static iomux_v3_cfg_t const usdhc2_pads[] = {
	IOMUX_PADS(PAD_LCD_DATA18__USDHC2_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA19__USDHC2_CLK | MUX_PAD_CTRL(USDHC_CLK_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA20__USDHC2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA21__USDHC2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA22__USDHC2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_LCD_DATA23__USDHC2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
};

static iomux_v3_cfg_t const sdhc_cd_wp_pads[] = {
	/* WP */
	IOMUX_PADS(PAD_UART1_CTS_B__GPIO1_IO18 | MUX_PAD_CTRL(NO_PAD_CTRL)),
	/* CD */
	IOMUX_PADS(PAD_UART1_RTS_B__GPIO1_IO19 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

struct fus_sdhc_cfg sdhc_cfg[2];

int board_mmc_getcd(struct mmc *mmc)
{
	struct fus_sdhc_cfg *cfg = &sdhc_cfg[mmc->block_dev.dev];

	if (cfg->cd_gpio == 0xFFFFFFFF)
		return 1;		/* No CD, assume card is present */

	/* Return CD signal (active low) */
	return !gpio_get_value(cfg->cd_gpio);
}

static int setup_mmc(bd_t *bis, struct fus_sdhc_cfg *cfg, unsigned cd_gpio,
		     int usdhc_index, u8 max_bus_width)
{
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	u32 ccgr6;

	/* Set CD pin configuration, activate GPIO for CD (if appropriate) */
	cfg->cd_gpio = cd_gpio;
	if (cd_gpio != 0xFFFFFFFF)
		gpio_direction_input(cd_gpio);

	/* Ungate USDHC clock and configure port */
	cfg->esdhc.max_bus_width = max_bus_width;
	ccgr6 = readl(&mxc_ccm->CCGR6);
	switch (usdhc_index) {
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
	}
	writel(ccgr6, &mxc_ccm->CCGR6);

	return fsl_esdhc_initialize(bis, &cfg->esdhc);
}

int board_mmc_init(bd_t *bis)
{
	int ret = 0;
	unsigned int cd_gpio = 0xFFFFFFFF;
	struct fus_sdhc_cfg *cfg = &sdhc_cfg[0];

	/* Configure first SD card slot (if available) */
	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA7UL:
		/*
		 * On efusA7UL, USDHC2 is either used for internal eMMC or
		 * provides the ext. SD_B on the connector (normal-size SD
		 * slot on efus SKIT). Write Protect (WP) is on GPIO1_IO18
		 * (ignored), Card Detect (CD) is on GPIO1_IO19.
		 *
		 * On board rev 1.00, the CD/WP pins are only available for
		 * SD_A. But on newer board revisions, they can be either used
		 * for SD_A or SD_B. Here we usually use them for SD_B because
		 * the normal-size slot on the SKIT actually has CD and WP
		 * while the Micro SD slot on SD_A only has CD. But if eMMC is
		 * equipped, we pass them on to SD_A. If SD_A is also not
		 * available because WLAN is equipped, we do not activate any
		 * CD/WP at all.
		 */
		if (fs_nboot_args.chFeatures2 & FEAT2_EMMC)
			break;		/* configure eMMC later as last MMC */
		if (fs_nboot_args.chBoardRev >= 110) {
			cd_gpio = IMX_GPIO_NR(1, 19);
			SETUP_IOMUX_PADS(sdhc_cd_wp_pads);
		}
		SETUP_IOMUX_PADS(usdhc2_pads);
		ret = setup_mmc(bis, cfg++, cd_gpio, 2, 4);
		break;
	case BT_PICOCOM1_2:
			/*
			 * On efusA7UL, USDHC1 is either used for on-board WLAN or
			 * provides ext. SD_B on the connector (Micro SD slot on efus
			 * SKIT). We can only use CD/WP if not already used by SD_B
			 * above (cd_gpio is still 0xFFFFFFFF).
			 */
		   SETUP_IOMUX_PADS(usdhc2_pads);
		   		ret=setup_mmc(bis,cfg++,cd_gpio, 2, 4);   		   		
			break;

	default:
		return 0;		/* Unknown device */
	}

	if (ret)
		return ret;

	/* Configure second SD card slot (if available) */
	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA7UL:
		/*
		 * On efusA7UL, USDHC1 is either used for on-board WLAN or
		 * provides ext. SD_B on the connector (Micro SD slot on efus
		 * SKIT). We can only use CD/WP if not already used by SD_B
		 * above (cd_gpio is still 0xFFFFFFFF).
		 */
		if (fs_nboot_args.chFeatures2 & FEAT2_WLAN)
			break;
		if (cd_gpio == 0xFFFFFFFF) {
			cd_gpio = IMX_GPIO_NR(1, 19);
			SETUP_IOMUX_PADS(sdhc_cd_wp_pads);
		}
		SETUP_IOMUX_PADS(usdhc1_pads);
		ret = setup_mmc(bis, cfg++, cd_gpio, 1, 4);
		break;

	default:
		return 0; 		/* No more SD card slots */
	}

	if (ret)
		return ret;

	/* Configure eMMC (if available) ### TODO: eMMC does not work yet */
	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA7UL:
		if (!(fs_nboot_args.chFeatures2 & FEAT2_EMMC))
			break;
		/* On efusA7UL, eMMC is on USDHC2 */
		SETUP_IOMUX_PADS(usdhc2_pads);
		/* ### TODO: if no NAND is used, eMMC may take four additional
		   signals from NAND and use buswidth 8 */
		ret = setup_mmc(bis, cfg++, 0xFFFFFFFF, 2, 4);
		break;

	default:
		return 0; 		/* No more SD card slots */
	}

	return ret;
}
#endif

#ifdef CONFIG_USB_EHCI_MX6
/* USB Host power (PicoCOM1_2) */
static iomux_v3_cfg_t const usb_pwr_pad_picocom1_2[] = {
	IOMUX_PADS(PAD_ENET2_TX_DATA1__USB_OTG2_PWR | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

/* USB Host power (GAR1) */
static iomux_v3_cfg_t const usb_pwr_pad_gar1[] = {
	IOMUX_PADS(PAD_SD1_DATA1__USB_OTG2_PWR | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

#define USB_OTHERREGS_OFFSET	0x800
#define UCTRL_PWR_POL		(1 << 9)

int board_ehci_hcd_init(int port)
{
	u32 *usbnc_usb_ctrl;

	if (port > 1)
		return 0;

	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA7UL:
		/* On efusA7UL, USB host power is fixed to 3.3V */
		break;

	case BT_PICOCOM1_2:
		/* Power on pad ENET2_TX_DATA1 */
		SETUP_IOMUX_PADS(usb_pwr_pad_picocom1_2);
		break;

	case BT_GAR1:
		/* Power on pad SD1_DATA1 */
		SETUP_IOMUX_PADS(usb_pwr_pad_gar1);
		break;

	default:
		break;
	}

	/* Set power polarity to active high */
	usbnc_usb_ctrl = (u32 *)(USB_BASE_ADDR + USB_OTHERREGS_OFFSET +
				 port * 4);
	setbits_le32(usbnc_usb_ctrl, UCTRL_PWR_POL);

        return 0;
}

/* Check if port is Host or Device */
int board_usb_phy_mode(int port)
{
	if (port == 0 && fs_nboot_args.chBoardType != BT_GAR1)
		return USB_INIT_DEVICE;

	return USB_INIT_HOST;
}
#endif

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

	/* Set sercon variable if not already set */
	if (strcmp(getenv("sercon"), "undef") == 0) {
		char sercon[DEV_NAME_SIZE];

		sprintf(sercon, "%s%c", CONFIG_SYS_SERCON_NAME,
			'0' + get_debug_port(fs_nboot_args.dwDbgSerPortPA));
		setenv("sercon", sercon);
	}

	/* Set platform variable if not already set */
	if (strcmp(getenv("platform"), "undef") == 0) {
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

	return 0;
}
#endif

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

/* Additional pins required for ethernet */
static iomux_v3_cfg_t const enet_pads_extra[] = {
	/* PHY interrupt; on efusA7UL this is shared on both PHYs and even the
	   PCA8565 RTC uses the same interrupt line! */
	IOMUX_PADS(PAD_LCD_RESET__GPIO3_IO04 | MUX_PAD_CTRL(NO_PAD_CTRL)),

	/* On efusA7UL, there is no dedicated PHY reset. Reset is done by the
	   global RESETOUTn signal that we trigger in board_init() */
};

/* Additional pins required for ethernet */
static iomux_v3_cfg_t const enet_pads_reset[] = {
	/* Phy Reset */
	IOMUX_PADS(PAD_CSI_MCLK__GPIO4_IO17 | MUX_PAD_CTRL(NO_PAD_CTRL)),
	IOMUX_PADS(PAD_CSI_PIXCLK__GPIO4_IO18 | MUX_PAD_CTRL(NO_PAD_CTRL)),
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
	int phy_addr_a, phy_addr_b;
	int reset_gpio, reset_gpio_2;
	enum xceiver_type xcv_type;
	phy_interface_t interface = PHY_INTERFACE_MODE_RMII;
	struct iomuxc *iomux_regs = (struct iomuxc *)IOMUXC_BASE_ADDR;
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	struct mii_dev *bus = NULL;
	struct phy_device *phydev;
	unsigned int features2 = fs_nboot_args.chFeatures2;
	int id = 0;

	/* Ungate ENET clock, this is a common clock for both ports */
	if (features2 & (FEAT2_ETH_A | FEAT2_ETH_B))
		writel(readl(&mxc_ccm->CCGR3) | 0x30, &mxc_ccm->CCGR3);

	/*
	 * Set IOMUX for ports, enable clocks. Both PHYs were already reset
	 * via RESETOUTn in board_init().
	 */
	switch (fs_nboot_args.chBoardType) {
	default:
		if (features2 & FEAT2_ETH_A) {
			/* IOMUX for ENET1, use 100 MBit/s LAN on RMII pins */
			SETUP_IOMUX_PADS(enet1_pads_rmii);
			SETUP_IOMUX_PADS(enet1_pads_mdio);

			/* ENET1 CLK is generated in i.MX6UL and is output */
			gpr1 = readl(&iomux_regs->gpr[1]);
			gpr1 |= IOMUXC_GPR1_ENET1_CLK_SEL_MASK;
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
			gpr1 |= IOMUXC_GPR1_ENET2_CLK_SEL_MASK;
			writel(gpr1, &iomux_regs->gpr[1]);

			/* Activate ENET2 PLL */
			ret = enable_fec_anatop_clock(1, ENET_50MHZ);
			if (ret < 0)
				return 0;
		}

		if (fs_nboot_args.chBoardType == BT_PICOCOM1_2 ||
				fs_nboot_args.chBoardType == BT_GAR1) {
			phy_addr_a = 1;
		}
		else
			phy_addr_a = 0;

		if (fs_nboot_args.chBoardType == BT_GAR1)
			phy_addr_b = 17;
		else
			phy_addr_b = 3;

		xcv_type = RMII;
		break;
	}

	/* Reset the PHY */
	switch (fs_nboot_args.chBoardType) {
	case BT_PICOCOM1_2:
		/*
		 * DP83484 PHY: This PHY needs at least 1 us reset
		 * pulse width (GPIO_2_10). After power on it needs
		 * min 167 ms (after reset is deasserted) before the
		 * first MDIO access can be done. In a warm start, it
		 * only takes around 3 for this. As we do not know
		 * whether this is a cold or warm start, we must
		 * assume the worst case.
		 */
		reset_gpio = IMX_GPIO_NR(5, 11);
		gpio_direction_output(reset_gpio, 0);
		udelay(10);
		gpio_set_value(reset_gpio, 1);
		mdelay(170);
		break;
	case BT_GAR1:
		/*
		 * DP83484 PHY: This PHY needs at least 1 us reset
		 * pulse width (GPIO_4_17, GPIO_4_18). After power on it needs
		 * min 167 ms (after reset is deasserted) before the
		 * first MDIO access can be done. In a warm start, it
		 * only takes around 3 for this. As we do not know
		 * whether this is a cold or warm start, we must
		 * assume the worst case.
		 */
		SETUP_IOMUX_PADS(enet_pads_reset);
		reset_gpio = IMX_GPIO_NR(4, 17);
		reset_gpio_2 = IMX_GPIO_NR(4, 18);
		gpio_direction_output(reset_gpio, 0);
		gpio_direction_output(reset_gpio_2, 0);
		udelay(10);
		gpio_set_value(reset_gpio, 1);
		gpio_set_value(reset_gpio_2, 1);
		mdelay(170);
		break;
	default:
		break;
	}

	/* Probe first PHY and activate first ethernet port */
	if (features2 & FEAT2_ETH_A) {
		set_fs_ethaddr(id);

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
		set_fs_ethaddr(id);
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
 * Boards       STA1           STA2         Active
 * ------------------------------------------------------------------------
 * efusA7UL     -              -            -
 * PicoCOM1.2   GPIO5_IO00     GPIO5_IO01   high
 * CubeA7UL     GPIO2_IO05     GPIO2_IO06   low
 * Cube2.0      GPIO2_IO05     GPIO2_IO06   low
 */
static unsigned int led_value;

static unsigned int get_led_gpio(struct tag_fshwconfig *pargs, led_id_t id,
				 int val)
{
	unsigned int gpio;

	if (val)
		led_value |= (1 << id);
	else
		led_value &= ~(1 << id);

	switch (pargs->chBoardType) {
	case BT_PICOCOM1_2:
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
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	unsigned int board_type = pargs->chBoardType - 16;
	int val = (board_type != BT_PICOCOM1_2);

	gpio_direction_output(get_led_gpio(pargs, 0, val), val);
	gpio_direction_output(get_led_gpio(pargs, 1, val), val);
}

void __led_set(led_id_t id, int val)
{
	struct tag_fshwconfig *pargs = &fs_nboot_args;

	if (id > 1)
		return;

	if (pargs->chBoardType != BT_PICOCOM1_2)
		val = !val;

	gpio_set_value(get_led_gpio(pargs, id, val), val);
}

void __led_toggle(led_id_t id)
{
	struct tag_fshwconfig *pargs = &fs_nboot_args;
	int val;

	if (id > 1)
		return;

	val = !((led_value >> id) & 1);
	gpio_set_value(get_led_gpio(pargs, id, val), val);
}
#endif /* CONFIG_CMD_LED */

#ifdef CONFIG_OF_BOARD_SETUP
/* Set a generic value, if it was not already set in the device tree */
static void fus_fdt_set_val(void *fdt, int offs, const char *name,
			    const void *val, int len)
{
	int err;

	/* Warn if property already exists in device tree */
	if (fdt_get_property(fdt, offs, name, NULL) != NULL) {
		printf("## Keeping property %s/%s from device tree!\n",
		       fdt_get_name(fdt, offs, NULL), name);
	}

	err = fdt_setprop(fdt, offs, name, val, len);
	if (err) {
		printf("## Unable to update property %s/%s: err=%s\n",
		       fdt_get_name(fdt, offs, NULL), name, fdt_strerror(err));
	}
}

/* Set a string value */
static void fus_fdt_set_string(void *fdt, int offs, const char *name,
			       const char *str)
{
	fus_fdt_set_val(fdt, offs, name, str, strlen(str) + 1);
}

/* Set a u32 value as a string (usually for bdinfo) */
static void fus_fdt_set_u32str(void *fdt, int offs, const char *name, u32 val)
{
	char str[12];

	sprintf(str, "%u", val);
	fus_fdt_set_string(fdt, offs, name, str);
}

/* Set a u32 value */
static void fus_fdt_set_u32(void *fdt, int offs, const char *name, u32 val)
{
	fdt32_t tmp = cpu_to_fdt32(val);

	fus_fdt_set_val(fdt, offs, name, &tmp, sizeof(tmp));
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
		fus_fdt_set_string(fdt, offs, name, str);
	}
}

/* If environment variable exists, set a string property with the same name */
static void fus_fdt_set_getenv(void *fdt, int offs, const char *name)
{
	const char *str;

	str = getenv(name);
	if (str)
		fus_fdt_set_string(fdt, offs, name, str);
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
				fs_nboot_args.chECCtype);
	}

	/* Set bdinfo entries */
	offs = fus_fdt_path_offset(fdt, "/bdinfo");
	if (offs >= 0) {
		int id = 0;
		char rev[6];

		/* NAND info, names and features */
		fus_fdt_set_u32str(fdt, offs, "ecc_strength",
				   fs_nboot_args.chECCtype);
		fus_fdt_set_u32str(fdt, offs, "nand_state",
				   fs_nboot_args.chECCstate);
		fus_fdt_set_string(fdt, offs, "board_name", get_board_name());
		sprintf(rev, "%d.%02d", fs_nboot_args.chBoardRev / 100,
			fs_nboot_args.chBoardRev % 100);
		fus_fdt_set_string(fdt, offs, "board_revision", rev);
		fus_fdt_set_getenv(fdt, offs, "platform");
		fus_fdt_set_getenv(fdt, offs, "arch");
		fus_fdt_set_u32str(fdt, offs, "features1",
				   fs_nboot_args.chFeatures1);
		fus_fdt_set_u32str(fdt, offs, "features2",
				   fs_nboot_args.chFeatures2);

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
	if (!(fs_nboot_args.chFeatures2 & FEAT2_ETH_B))
		fus_fdt_enable(fdt, FDT_ETH_B, 0);
}
#endif /* CONFIG_OF_BOARD_SETUP */
