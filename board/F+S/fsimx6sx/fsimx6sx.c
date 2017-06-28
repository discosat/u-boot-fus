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

#define BT_EFUSA9X    0
#define BT_PICOCOMA9X 1
#define BT_KEN116     2			/* Not supported in Linux */
#define BT_BEMA9X     3

/* Features set in tag_fshwconfig.chFeature2 (available since NBoot VN27) */
#define FEAT2_ETH_A   (1<<0)		/* 0: no LAN0, 1; has LAN0 */
#define FEAT2_ETH_B   (1<<1)		/* 0: no LAN1, 1; has LAN1 */
#define FEAT2_EMMC    (1<<2)		/* 0: no eMMC, 1: has eMMC */
#define FEAT2_WLAN    (1<<3)		/* 0: no WLAN, 1: has WLAN */
#define FEAT2_HDMICAM (1<<4)		/* 0: LCD-RGB, 1: HDMI+CAM (PicoMOD) */
#define FEAT2_ETH_MASK (FEAT2_ETH_A | FEAT2_ETH_B)

/* NBoot before VN27 did not report feature values; use reasonable defaults */
#define FEAT1_DEFAULT 0
#define FEAT2_DEFAULT (FEAT2_ETH_A | FEAT2_ETH_B)

#define ACTION_RECOVER 0x00000040	/* Start recovery instead of update */

#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)

/* Device tree paths */
#define FDT_NAND	"/soc/gpmi-nand@01806000"
#define FDT_ETH_A	"/soc/aips-bus@02100000/ethernet@02188000"
#define FDT_ETH_B	"/soc/aips-bus@02000000/ethernet@021b4000"

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

#define USDHC_PAD_CTRL (PAD_CTL_HYS | PAD_CTL_PUS_22K_UP |	\
	PAD_CTL_SPEED_LOW | PAD_CTL_DSE_80ohm | PAD_CTL_SRE_FAST)
#define USDHC_CD_CTRL (PAD_CTL_PUS_47K_UP | PAD_CTL_SPEED_LOW | PAD_CTL_HYS)


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
	{	/* 0 (BT_EFUSA9X) */
		.name = "efusA9X",
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
	{	/* 1 (BT_PicoCOMA9X) */
		.name = "PicoCOMA9X",
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
	{	/* 2 (BT_KEN116) */
		.name = "KEN116",
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
	{	/* 3 (BT_BEMA9X) */
		.name = "BemA9X",
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
	{	/* 4 (unknown) */
		.name = "unknown",
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
	IOMUX_PADS(PAD_LCD1_ENABLE__GPIO3_IO_25 | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_HSYNC__GPIO3_IO_26  | MUX_PAD_CTRL(0x3010)),
	IOMUX_PADS(PAD_LCD1_VSYNC__GPIO3_IO_28  | MUX_PAD_CTRL(0x3010)),
};

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
	unsigned int boardtype = pargs->chBoardType - 8;
	unsigned int features2;

	/* NBoot versions before VN27 did not report feature values */
	if ((be32_to_cpu(pargs->dwNBOOT_VER) & 0xFFFF) < 0x3237) { /* "27" */
		pargs->chFeatures1 = FEAT1_DEFAULT;
		pargs->chFeatures2 = FEAT2_DEFAULT;
	}
	features2 = pargs->chFeatures2;

	printf("Board: %s Rev %u.%02u (", fs_board_info[boardtype].name,
	       pargs->chBoardRev / 100, pargs->chBoardRev % 100);
	if ((boardtype != BT_BEMA9X)
	    && ((features2 & FEAT2_ETH_MASK) == FEAT2_ETH_MASK))
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

/* Set the available RAM size. We have a memory bank starting at 0x80000000
   that can hold up to 2048MB of RAM. */
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
	IOMUX_PADS(PAD_ENET1_CRS__GPIO2_IO_1 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

int board_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	unsigned int board_type = pargs->chBoardType - 8;

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

	/* Reset board and SKIT hardware (PCIe, USB-Hub, WLAN, if available);
	   this is on pad ENET1_CRS (GPIO2_IO01); because there may be some
	   completely different hardware connected to this general RESETOUTn
	   pin, use a rather long low pulse of 100ms. */
	SETUP_IOMUX_PADS(reset_pads);
	gpio_direction_output(IMX_GPIO_NR(2, 1), 0);
	mdelay(100);
	gpio_set_value(IMX_GPIO_NR(2, 1), 1);

#if 0 //###
	printf("### PLL_ARM=0x%08x\n", readl(0x20c8000));
	printf("### PLL_USB1=0x%08x\n", readl(0x20c8010));
	printf("### PLL_USB2=0x%08x\n", readl(0x20c8020));
	printf("### PLL_SYS=0x%08x\n", readl(0x20c8030));
	printf("### PLL_AUDIO=0x%08x\n", readl(0x20c8070));
	printf("### PLL_VIDEO=0x%08x\n", readl(0x20c80A0));
	printf("### PLL_ENET=0x%08x\n", readl(0x20c80e0));

	enable_usb2_anatop_clock();
	printf("### PLL_USB2=0x%08x\n", readl(0x20c8020));
#endif

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
 *   efusA9X (Board Rev 1.1x):
 *                 USDHC2  GPIO1_IO06             SD_B: Connector (SD)
 *        either:  USDHC1  GPIO1_IO02             SD_A: Connector (Micro-SD)
 *            or: [USDHC1  GPIO1_IO02             WLAN]
 *                 USDHC4  -                      eMMC (8-Bit)
 *   efusA9X (Board Rev 1.2x):
 *                 USDHC2  GPIO1_IO06             SD_B: Connector (SD)
 *        either:  USDHC4  SD4_DATA7 (GPIO6_IO21) SD_A: Connector (Micro-SD)
 *            or:  USDHC4  -                      eMMC (8-Bit)
 *                [USDHC1  GPIO1_IO02             WLAN]
 *   -----------------------------------------------------------------------
 *   PicoCOMA9X:   USDHC2  -                      Connector (SD)
 *                 USDHC4 [KEY_COL2 (GPIO2_IO12)] eMMC (8-Bit)
 *   -----------------------------------------------------------------------
 *   BEMA9X:       USDHC2  -                      Connector (SD)
 *                [USDHC4  KEY_COL2 (GPIO2_IO12)  WLAN]
 *   -----------------------------------------------------------------------
 *
 * Remark: The WP pin is ignored in U-Boot, also WLAN
 */

/* Convert from struct fsl_esdhc_cfg to struct fus_sdhc_cfg */
#define to_fus_sdhc_cfg(x) container_of((x), struct fus_sdhc_cfg, esdhc)

/* SD/MMC card pads definition */
static iomux_v3_cfg_t const usdhc1_sd_pads[] = {
	IOMUX_PADS(PAD_SD1_CLK__USDHC1_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_CMD__USDHC1_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_DATA0__USDHC1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_DATA1__USDHC1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_DATA2__USDHC1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_DATA3__USDHC1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
};

static iomux_v3_cfg_t const usdhc2_sd_pads[] = {
	IOMUX_PADS(PAD_SD2_CLK__USDHC2_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD2_CMD__USDHC2_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD2_DATA0__USDHC2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD2_DATA1__USDHC2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD2_DATA2__USDHC2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD2_DATA3__USDHC2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
};

static iomux_v3_cfg_t const usdhc4_sd_pads[] = {
	IOMUX_PADS(PAD_SD4_CLK__USDHC4_CLK     | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD4_CMD__USDHC4_CMD     | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD4_RESET_B__USDHC4_RESET_B | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD4_DATA0__USDHC4_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD4_DATA1__USDHC4_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD4_DATA2__USDHC4_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD4_DATA3__USDHC4_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD4_DATA4__USDHC4_DATA4 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD4_DATA5__USDHC4_DATA5 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD4_DATA6__USDHC4_DATA6 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD4_DATA7__USDHC4_DATA7 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
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
	usdhc1, usdhc2, usdhc4
};

static struct fus_sdhc_cfg sdhc_cfg[] = {
	[usdhc1] = { usdhc1_sd_pads, 2, 1 }, /* pads, count, USDHC index */
	[usdhc2] = { usdhc2_sd_pads, 2, 2 },
	[usdhc4] = { usdhc4_sd_pads, 3, 4 },
};

struct fus_sdhc_cd {
	const iomux_v3_cfg_t *pad;
	unsigned int gpio;
};

enum usdhc_cds {
	gpio1_io02, gpio1_io06, gpio6_io21
};

struct fus_sdhc_cd sdhc_cd[] = {
	[gpio1_io02] = { cd_gpio1_io02, IMX_GPIO_NR(1, 2) }, /* pad, gpio */
	[gpio1_io06] = { cd_gpio1_io06, IMX_GPIO_NR(1, 6) },
	[gpio6_io21] = { cd_sd4_data7, IMX_GPIO_NR(6, 21) },
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
	case 4:
		cfg->esdhc.esdhc_base = USDHC4_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC4_CLK);
		ccgr6 |= (3 << 8);
		break;
	}
	writel(ccgr6, &mxc_ccm->CCGR6);

	return fsl_esdhc_initialize(bd, &cfg->esdhc);
}

int board_mmc_init(bd_t *bd)
{
	int ret = 0;

	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA9X:
		/* mmc0: USDHC2 (ext. SD slot, normal-size SD on efus SKIT) */
		ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc2], &sdhc_cd[gpio1_io06]);
		if (ret)
			break;

		/* mmc1 (ext. SD slot, micro SD on efus SKIT) */
		if (fs_nboot_args.chBoardRev < 120) {
			/* Board Rev 1.0x/1.1x: if no WLAN present: USDHC1 */
			if (!(fs_nboot_args.chFeatures2 & FEAT2_WLAN))
				ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc1],
						&sdhc_cd[gpio1_io02]);
		} else {
			/* Board Rev 1.2x: if no eMMC present: USDHC4 */
			if (!(fs_nboot_args.chFeatures2 & FEAT2_EMMC))
				ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc4],
						&sdhc_cd[gpio6_io21]);
		}

		/* mmc2: USDHC4 (eMMC, if available), no CD */
		if (!ret && (fs_nboot_args.chFeatures2 & FEAT2_EMMC))
			ret = setup_mmc(bd, 8, &sdhc_cfg[usdhc4], NULL);
		break;

	case BT_PICOCOMA9X:
		/* mmc0: USDHC2 (ext. SD slot via connector), no CD */
		ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc2], NULL);

		/* mmc1: USDHC4 (eMMC, if available), ignore CD */
		if (!ret && (fs_nboot_args.chFeatures2 & FEAT2_EMMC))
			ret = setup_mmc(bd, 8, &sdhc_cfg[usdhc4], NULL);
		break;

	case BT_BEMA9X:
		/* mmc0: USDHC2 (ext. SD slot via connector), no CD */
		ret = setup_mmc(bd, 4, &sdhc_cfg[usdhc2], NULL);
		break;

	default:
		return 0;		/* Neither SD card, nor eMMC */
	}

	return ret;
}
#endif

#ifdef CONFIG_USB_EHCI_MX6
/* USB Host power (efusA9X) on GPIO_12 (GPIO1_IO12) */
static iomux_v3_cfg_t const usb_pwr_pads[] = {
//###	IOMUX_PADS(PAD_GPIO1_IO12__GPIO1_IO_12 | MUX_PAD_CTRL(NO_PAD_CTRL)),
	IOMUX_PADS(PAD_GPIO1_IO12__USB_OTG2_PWR | MUX_PAD_CTRL(NO_PAD_CTRL)),
//###	IOMUX_PADS(PAD_GPIO1_IO13__ANATOP_OTG2_ID | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

#define USB_OTHERREGS_OFFSET	0x800
#define UCTRL_PWR_POL		(1 << 9)

int board_ehci_hcd_init(int port)
{
	u32 *usbnc_usb_ctrl;

	if (port > 1)
		return 0;

	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA9X:
	case BT_PICOCOMA9X:
	case BT_BEMA9X:
		SETUP_IOMUX_PADS(usb_pwr_pads);
#if 0
		/* Enable USB Host power */
		gpio_direction_output(IMX_GPIO_NR(1, 12), 1);
#endif
		break;

	default:
		break;
	}

#if 1 //###
	usbnc_usb_ctrl = (u32 *)(USB_BASE_ADDR + USB_OTHERREGS_OFFSET +
				 port * 4);

	/* Set Power polarity */
	setbits_le32(usbnc_usb_ctrl, UCTRL_PWR_POL);
#endif //###

        return 0;
}

#if 0 //###
int board_ehci_power(int port, int on)
{
	if (port != 1)
		return 0;

	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA9X:
	case BT_BEMA9X:
		SETUP_IOMUX_PADS(usb_pwr_pads);

		/* Enable USB Host power */
		gpio_direction_output(IMX_GPIO_NR(1, 12), on);

		break;

	default:
		break;
	}

	return 0;
}
#endif

int board_usb_phy_mode(int port)
{
	if (port == 0)
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
/* enet pads definition */
static iomux_v3_cfg_t const enet_pads_rgmii1_mdio_irq_reset[] = {
	/* MDIO */
	IOMUX_PADS(PAD_ENET2_CRS__ENET1_MDIO | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET2_COL__ENET1_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL)),

	/* 25MHz base clock from CPU to both PHYs */
	IOMUX_PADS(PAD_ENET2_RX_CLK__ENET2_REF_CLK_25M | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL)),

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

	/* PHY interrupt; on efuA9X this is shared on both PHYs */
	IOMUX_PADS(PAD_ENET2_TX_CLK__GPIO2_IO_9 | MUX_PAD_CTRL(NO_PAD_CTRL)),

	/* Reset signal for both PHYs */
	IOMUX_PADS(PAD_ENET1_MDC__GPIO2_IO_2 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

static iomux_v3_cfg_t const enet_pads_rgmii2[] = {
	/* FEC1 (ENET2); MDIO is handled over FEC0 */
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
	int mii_bus_addr = -1;
	enum xceiver_type xcv_type = RGMII;
	phy_interface_t interface = PHY_INTERFACE_MODE_RGMII;
	struct iomuxc *iomux_regs = (struct iomuxc *)IOMUXC_BASE_ADDR;
	struct mii_dev *bus = NULL;
	struct phy_device *phydev;
	unsigned int features2 = fs_nboot_args.chFeatures2;
	int id = 0;

	/*
	 * Set IOMUX for ports, enable clocks and reset PHYs. On i.MX6 SoloX,
	 * the ENET clock is ungated in enable_fec_anatop_clock().
	 */
	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA9X:
		/* The 25 MHz reference clock is generated in the CPU and is an
		   output on pad ENET2_RX_CLK, i.e. CONFIG_FEC_MXC_25M_REF_CLK
		   must be set */
		if (features2 & FEAT2_ETH_A) {
			/* IOMUX for ENET1, use 1 GBit/s LAN on RGMII pins */
			SETUP_IOMUX_PADS(enet_pads_rgmii1_mdio_irq_reset);

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
				return 0;
		}

		/* Reset both PHYs (GPIO2_IO2), Atheros AR8035 needs at least
		   0.5 ms */
		gpio_direction_output(IMX_GPIO_NR(2, 2), 0);
		udelay(500);
		gpio_set_value(IMX_GPIO_NR(2, 2), 1);

		phy_addr_a = 4;
		phy_addr_b = 5;
		break;


	case BT_PICOCOMA9X:
		mii_bus_addr = 0;
		/* Fall through to case BT_BEMA9X */
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

		if (features2 & FEAT2_ETH_A) {
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
		 * Reset both PHYs. DP83484 needs at least 1 us reset pulse
		 * width (GPIO2_IO7). After power on it needs min 167 ms
		 * (after reset is deasserted) before the first MDIO access
		 * can be done. In a warm start, it only takes around 3 for
		 * this. As we do not know whether this is a cold or warm
		 * start, we must assume the worst case.
		 */
		gpio_direction_output(IMX_GPIO_NR(2, 7), 0);
		udelay(10);
		gpio_set_value(IMX_GPIO_NR(2, 7), 1);
		mdelay(170);

		interface = PHY_INTERFACE_MODE_RMII;
		xcv_type = RMII;
		phy_addr_a = 1;
		phy_addr_b = 17;
		break;

	default:
		return 0;
	}

#if 0
	/*
	 * ### If CONFIG_FEC_MXC_25M_REF_CLK is set, this is automatically
	 * done in enable_fec_anatop_clock(); we keep it here as reference
	 * code in case additional board layouts need a different setting and
	 * we can not set CONFIG_FEC_MXC_25M_REF_CLK globally in fsimx6sx.h
	 * anymore. Then we can set it here depending on board type.
	 */
	{
		u32 reg;
		struct anatop_regs *anatop =
			(struct anatop_regs *)ANATOP_BASE_ADDR;

		reg = readl(&anatop->pll_enet);
		reg |= BM_ANADIG_PLL_ENET_REF_25M_ENABLE;
		writel(reg, &anatop->pll_enet);
	}
#endif

	/* Probe first PHY and activate first ethernet port */
	if (features2 & FEAT2_ETH_A) {
		set_fs_ethaddr(id);

		/*
		 * We can not use fecmxc_initialize_multi_type() because this
		 * would allocate one MII bus for each ethernet device. But on
		 * efusA9X we only need one MII bus in total for both ports.
		 * So the following code works rather similar to the code in
		 * fecmxc_initialize_multi_type(), but uses just one bus on
		 * efusA9X.
		 */
		bus = fec_get_miibus(ENET_BASE_ADDR, mii_bus_addr);
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

		/* Get the second MII bus if the PHYs are on different buses */
		if (mii_bus_addr != -1) {
			mii_bus_addr++;
			bus = fec_get_miibus(ENET2_BASE_ADDR, mii_bus_addr);
			if (!bus)
				return 0;
		}

		phydev = phy_find_by_mask(bus, 1 << phy_addr_b, interface);
		if (!phydev) {
			if (mii_bus_addr != -1)
				free(bus);
			return -ENOMEM;
		}

		ret = fec_probe(bis, id, ENET2_BASE_ADDR, bus, phydev,
				xcv_type);
		if (ret) {
			free(phydev);
			if (mii_bus_addr != -1)
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
	if (!(fs_nboot_args.chFeatures2 & FEAT2_ETH_B))
		fus_fdt_enable(fdt, FDT_ETH_B, 0);
}
#endif /* CONFIG_OF_BOARD_SETUP */
