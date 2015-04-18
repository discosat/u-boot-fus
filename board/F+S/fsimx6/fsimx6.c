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

/* ------------------------------------------------------------------------- */

#define NBOOT_ARGS_BASE (PHYS_SDRAM + 0x00001000) /* Arguments from NBoot */
#define BOOT_PARAMS_BASE (PHYS_SDRAM + 0x100)	  /* Arguments to Linux */

#define BT_EFUSA9     0
#define BT_ARMSTONEA9 1
#define BT_PICOMODA9  2
#define BT_QBLISSA9   3

/* Features set in tag_fshwconfig.chFeature1 */
#define FEAT1_2NDCAN  (1<<1)		/* 0: 1x CAN, 1: 2x CAN */
#define FEAT1_2NDLAN  (1<<4)		/* 0: 1x LAN, 1: 2x LAN */

/* Features set in tag_fshwconfig.chFeature2 */
#define FEAT2_M4      (1<<0)		/* CPU has Cortex-M4 core */

#define ACTION_RECOVER 0x00000040	/* Start recovery instead of update */

#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)


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

#define USDHC_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_47K_UP  | PAD_CTL_SPEED_LOW |		\
	PAD_CTL_DSE_80ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)


struct board_info {
	char *name;			/* Device name */
	unsigned int mach_type;		/* Device machine ID */
	char *bootdelay;		/* Default value for bootdelay */
	char *updatecheck;		/* Default value for updatecheck */
	char *installcheck;		/* Default value for installcheck */
	char *recovercheck;		/* Default value for recovercheck */
	char *console;			/* Default variable for console */
	char *login;			/* Default variable for login */
	char *mtdparts;			/* Default variable for mtdparts */
	char *network;			/* Default variable for network */
	char *init;			/* Default variable for init */
	char *rootfs;			/* Default variable for rootfs */
	char *kernel;			/* Default variable for kernel */
	char *fdt;			/* Default variable for device tree */
	char *fsload;			/* Default variable for load command */
};

#if defined(CONFIG_MMC) && defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
#define UPDATE_DEF "mmc,usb"
#elif defined(CONFIG_MMC) && defined(CONFIG_FS_FAT)
#define UPDATE_DEF "mmc"
#elif defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
#define UPDATE_DEF "usb"
#else
#define UPDATE_DEF NULL
#endif


const struct board_info fs_board_info[8] = {
	{	/* 0 (BT_EFUSA9) */
		.name = "efusA9",
		.mach_type = MACH_TYPE_EFUSA9,
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = UPDATE_DEF,
		.recovercheck = UPDATE_DEF,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_init",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
		.fsload = ".fsload_fat",
	},
	{	/* 1 (BT_ARMSTONEA9)*/
		.name = "armStoneA9",
		.mach_type = MACH_TYPE_ARMSTONEA9,
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = UPDATE_DEF,
		.recovercheck = UPDATE_DEF,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_init",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
		.fsload = ".fsload_fat",
	},
	{	/* 2 (BT_PICOMODA9) */
		.name = "PicoMODA9",
		.mach_type = MACH_TYPE_PICOMODA9,
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = UPDATE_DEF,
		.recovercheck = UPDATE_DEF,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_init",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
		.fsload = ".fsload_fat",
	},
	{	/* 3 (BT_QBLISSA9) */
		.name = "QBlissA9",
		.mach_type = MACH_TYPE_QBLISSA9,
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = UPDATE_DEF,
		.recovercheck = UPDATE_DEF,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_init",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
		.fsload = ".fsload_fat",
	},
	{	/* 4 (unknown) */
		.name = "unknown",
		.mach_type = 0,
	},
	{	/* 5 (unknown) */
		.name = "unknown",
		.mach_type = 0,
	},
	{	/* 6 (unknown) */
		.name = "unknown",
		.mach_type = 0,
	},
	{	/* 7 (unknown) */
		.name = "unknown",
		.mach_type = 0,
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

/* Check board type */
int checkboard(void)
{
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	int nLAN;
	int nCAN;

	nLAN = pargs->chFeatures1 & FEAT1_2NDLAN ? 2 : 1;
	nCAN = pargs->chFeatures1 & FEAT1_2NDCAN ? 2 : 1;

	printf("Board: %s Rev %u.%02u (%dx DRAM, %dx LAN, %dx CAN)\n",
	       fs_board_info[pargs->chBoardType].name,
	       pargs->chBoardRev / 100, pargs->chBoardRev % 100,
	       pargs->dwNumDram, nLAN, nCAN);

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

	gd->bd->bi_arch_number = fs_board_info[board_type].mach_type;
	gd->bd->bi_boot_params = BOOT_PARAMS_BASE;

	/* Prepare the command prompt */
	sprintf(fs_sys_prompt, "%s # ", fs_board_info[board_type].name);

	return 0;
}


/* nand flash pads  */
static iomux_v3_cfg_t const nfc_pads_q[] = {
	MX6Q_PAD_NANDF_CLE__NAND_CLE	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_ALE__NAND_ALE	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6Q_PAD_NANDF_WP_B__NAND_WP_B	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_RB0__NAND_READY_B	| MUX_PAD_CTRL(GPMI_PAD_CTRL0),
	MX6Q_PAD_NANDF_CS0__NAND_CE0_B	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_SD4_CMD__NAND_RE_B	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_SD4_CLK__NAND_WE_B	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D0__NAND_DATA00	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D1__NAND_DATA01	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D2__NAND_DATA02	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D3__NAND_DATA03	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D4__NAND_DATA04	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D5__NAND_DATA05	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D6__NAND_DATA06	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D7__NAND_DATA07	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
};

static iomux_v3_cfg_t const nfc_pads_dl[] = {
	MX6DL_PAD_NANDF_CLE__NAND_CLE	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6DL_PAD_NANDF_ALE__NAND_ALE	| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6DL_PAD_NANDF_WP_B__NAND_WP_B	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6DL_PAD_NANDF_RB0__NAND_READY_B	| MUX_PAD_CTRL(GPMI_PAD_CTRL0),
	MX6DL_PAD_NANDF_CS0__NAND_CE0_B	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6DL_PAD_SD4_CMD__NAND_RE_B	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6DL_PAD_SD4_CLK__NAND_WE_B	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6DL_PAD_NANDF_D0__NAND_DATA00	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6DL_PAD_NANDF_D1__NAND_DATA01	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6DL_PAD_NANDF_D2__NAND_DATA02	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6DL_PAD_NANDF_D3__NAND_DATA03	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6DL_PAD_NANDF_D4__NAND_DATA04	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6DL_PAD_NANDF_D5__NAND_DATA05	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6DL_PAD_NANDF_D6__NAND_DATA06	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6DL_PAD_NANDF_D7__NAND_DATA07	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
};

/* Register NAND devices. We actually split the NAND into two virtual devices
   to allow different ECC strategies for NBoot and the rest. */
void board_nand_init(void)
{
	int reg;
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	iomux_v3_cfg_t const *nfc_pads;
	unsigned nfc_pad_count;
	struct mxs_nand_fus_platform_data pdata;

	if (is_cpu_type(MXC_CPU_MX6Q)) {
		nfc_pads = nfc_pads_q;
		nfc_pad_count = ARRAY_SIZE(nfc_pads_q);
	} else {
		nfc_pads = nfc_pads_dl;
		nfc_pad_count = ARRAY_SIZE(nfc_pads_dl);
	}

	/* config gpmi nand iomux */
	imx_iomux_v3_setup_multiple_pads(nfc_pads, nfc_pad_count);

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
static iomux_v3_cfg_t const usdhc1_pads_q[] = {
	MX6Q_PAD_SD1_CLK__SD1_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD1_CMD__SD1_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD1_DAT0__SD1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD1_DAT1__SD1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD1_DAT2__SD1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD1_DAT3__SD1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc1_pads_dl[] = {
	MX6DL_PAD_SD1_CLK__SD1_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_SD1_CMD__SD1_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_SD1_DAT0__SD1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_SD1_DAT1__SD1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_SD1_DAT2__SD1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_SD1_DAT3__SD1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc2_pads_q[] = {
	MX6Q_PAD_SD2_CLK__SD2_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD2_CMD__SD2_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD2_DAT0__SD2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD2_DAT1__SD2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD2_DAT2__SD2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD2_DAT3__SD2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_GPIO_2__GPIO1_IO02 | MUX_PAD_CTRL(NO_PAD_CTRL),	/* WP */
	MX6Q_PAD_GPIO_4__GPIO1_IO04 | MUX_PAD_CTRL(NO_PAD_CTRL),	/* CD */
};

static iomux_v3_cfg_t const usdhc2_pads_dl[] = {
	MX6DL_PAD_SD2_CLK__SD2_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_SD2_CMD__SD2_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_SD2_DAT0__SD2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_SD2_DAT1__SD2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_SD2_DAT2__SD2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_SD2_DAT3__SD2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_GPIO_2__GPIO1_IO02 | MUX_PAD_CTRL(NO_PAD_CTRL),	/* WP */
	MX6DL_PAD_GPIO_4__GPIO1_IO04 | MUX_PAD_CTRL(NO_PAD_CTRL),	/* CD */
//###	MX6DL_PAD_SD3_RST__SD3_RST | MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

/* SD/MMC card pads definition */
static iomux_v3_cfg_t const usdhc3_pads_q[] = {
	MX6Q_PAD_SD3_CLK__SD3_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD3_CMD__SD3_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD3_DAT0__SD3_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD3_DAT1__SD3_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD3_DAT2__SD3_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD3_DAT3__SD3_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_NANDF_CS2__GPIO6_IO15 | MUX_PAD_CTRL(NO_PAD_CTRL),	/* CD */
//###	MX6Q_PAD_SD3_RST__SD3_RST | MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc3_pads_dl[] = {
	MX6DL_PAD_SD3_CLK__SD3_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_SD3_CMD__SD3_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_SD3_DAT0__SD3_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_SD3_DAT1__SD3_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_SD3_DAT2__SD3_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_SD3_DAT3__SD3_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6DL_PAD_NANDF_CS2__GPIO6_IO15 | MUX_PAD_CTRL(NO_PAD_CTRL),	/* CD */
//###	MX6DL_PAD_SD3_RST__SD3_RST | MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

struct fsl_esdhc_cfg esdhc_cfg[] = {
	{USDHC1_BASE_ADDR, MXC_ESDHC_CLK},
	{USDHC2_BASE_ADDR, MXC_ESDHC2_CLK},
	{USDHC3_BASE_ADDR, MXC_ESDHC3_CLK},
	{USDHC4_BASE_ADDR, MXC_ESDHC4_CLK},
};

int board_mmc_getcd(struct mmc *mmc)
{
	switch (fs_nboot_args.chBoardType) {
	case BT_ARMSTONEA9:
		return !gpio_get_value(IMX_GPIO_NR(6, 15));

	case BT_PICOMODA9:
		if (mmc->block_dev.dev == 0)
			/* External SD card slot has Card Detect (CD) */
			return !gpio_get_value(IMX_GPIO_NR(1, 4));
		else
			/* On-board SD card slot has no Card Detect (CD) */
			return 1;	/* Assume card is present */

	default:
		return 1;		/* Assume card is present */
	}
}

int board_mmc_init(bd_t *bis)
{
	int index;
	iomux_v3_cfg_t const *usdhc_pads;
	unsigned usdhc_pad_count;
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	u32 ccgr6;
	unsigned int gpio_cd = 0xFFFFFFFF;
	int ret;

	ccgr6 = readl(&mxc_ccm->CCGR6);

	/* Configure first SD card slot */
	switch (fs_nboot_args.chBoardType) {
	case BT_QBLISSA9:
		/* USDHC3: ext. SD slot (connector), Write Protect (WP) on
		   SD2_DAT2 (ignored), Card Detect (CD) on NANDF_CS2 pin
		   (GPIO6_IO15), Power on SD3_RST (GPIO7_IO08); this slot has
		   8 data lines, but we only use four lines for SD card;
		   however if this is used for EMMC, we need to add support */
		if (is_cpu_type(MXC_CPU_MX6Q)) {
			usdhc_pads = usdhc3_pads_q;
			usdhc_pad_count = ARRAY_SIZE(usdhc3_pads_q);
		} else {
			usdhc_pads = usdhc3_pads_dl;
			usdhc_pad_count = ARRAY_SIZE(usdhc3_pads_dl);
		}
		gpio_cd = IMX_GPIO_NR(6, 15);
		ccgr6 |= (3 << 6);
		index = 2;
		break;

	case BT_ARMSTONEA9:
		/* USDHC3: on-board micro SD slot, Card Detect (CD) on
		   NANDF_CS2 pin (GPIO6_IO15) */
		if (is_cpu_type(MXC_CPU_MX6Q)) {
			usdhc_pads = usdhc3_pads_q;
			usdhc_pad_count = ARRAY_SIZE(usdhc3_pads_q);
		} else {
			usdhc_pads = usdhc3_pads_dl;
			usdhc_pad_count = ARRAY_SIZE(usdhc3_pads_dl);
		}
		gpio_cd = IMX_GPIO_NR(6, 15);
		ccgr6 |= (3 << 6);
		index = 2;
		break;

	case BT_PICOMODA9:
		/* USDHC2: ext. SD slot (connector), Write Protect (WP) on
		   GPIO_2 (ignored), Card Detect (CD) on GPIO_4 (GPIO1_IO4) */
	case BT_EFUSA9:
		/* USDHC2: ext. SD slot (connector, normal-size SD slot on
		   efus SKIT), Write Protect (WP) on GPIO_2 (ignored), Card
		   Detect (CD) on GPIO_4 (GPIO1_IO4) */
		if (is_cpu_type(MXC_CPU_MX6Q)) {
			usdhc_pads = usdhc2_pads_q;
			usdhc_pad_count = ARRAY_SIZE(usdhc2_pads_q);
		} else {
			usdhc_pads = usdhc2_pads_dl;
			usdhc_pad_count = ARRAY_SIZE(usdhc2_pads_dl);
		}
		gpio_cd = IMX_GPIO_NR(1, 4);
		ccgr6 |= (3 << 4);
		index = 1;
		break;

	default:
		return 0;		/* Unknown device */
	}

	imx_iomux_v3_setup_multiple_pads(usdhc_pads, usdhc_pad_count);
	if (gpio_cd != 0xFFFFFFFF)
		gpio_direction_input(gpio_cd);
	writel(ccgr6, &mxc_ccm->CCGR6);

	esdhc_cfg[index].sdhc_clk = mxc_get_clock(esdhc_cfg[index].sdhc_clk);
	ret = fsl_esdhc_initialize(bis, &esdhc_cfg[index]);
	if (ret)
		return ret;
	
	/* Configure second SD card slot (if available) */
	switch (fs_nboot_args.chBoardType) {
	case BT_QBLISSA9:
	case BT_PICOMODA9:
		/* USDHC1: This on-board micro SD slot is only available if
		   WLAN is not mounted. It has no Card Detect (CD) signal. */
		//### TODO: Define and set feature bit for SD slot in NBoot
		if (0) /*(fs_nboot_args.chFeatures1 & FEAT1_SDSLOT)*/ {
			gpio_cd = 0xFFFFFFFF;
			goto usdhc1;
		}
		return 0; 		/* No more SD card slots */

	case BT_EFUSA9:
		/* USDHC1: ext. SD slot (connector, micro SD slot on efus
		   SKIT), Write Protect (WP) on DIO_PIN4 (ignored), Card
		   Detect (CD) on GPIO_1 (GPIO1_IO1) */
		gpio_cd = IMX_GPIO_NR(1, 1);
	usdhc1:
		if (is_cpu_type(MXC_CPU_MX6Q)) {
			usdhc_pads = usdhc1_pads_q;
			usdhc_pad_count = ARRAY_SIZE(usdhc1_pads_q);
		} else {
			usdhc_pads = usdhc1_pads_dl;
			usdhc_pad_count = ARRAY_SIZE(usdhc1_pads_dl);
		}
		ccgr6 |= (3 << 2);
		index = 0;
		break;

	default:
		return 0; 		/* No more SD card slots */
	}

	imx_iomux_v3_setup_multiple_pads(usdhc_pads, usdhc_pad_count);
	if (gpio_cd != 0xFFFFFFFF)
		gpio_direction_input(gpio_cd);
	writel(ccgr6, &mxc_ccm->CCGR6);

	esdhc_cfg[index].sdhc_clk = mxc_get_clock(esdhc_cfg[index].sdhc_clk);
	ret = fsl_esdhc_initialize(bis, &esdhc_cfg[index]);
	if (ret)
		return ret;

	/* ### TODO: efusA9 has a third SDIO slot on USDHC3 with 8 data lines,
	   for on-board EMMC. This may also optionally be used for WLAN or
	   other RF modems in the future. */

	return 0;
}
#endif

#ifdef CONFIG_USB_EHCI_MX6
/* USB Hub Reset-Pin (armStoneA9, QBlissA9) */
static iomux_v3_cfg_t const usb_hub_pads_q[] = {
        MX6Q_PAD_GPIO_17__GPIO7_IO12 | MUX_PAD_CTRL(NO_PAD_CTRL),
};
static iomux_v3_cfg_t const usb_hub_pads_dl[] = {
        MX6DL_PAD_GPIO_17__GPIO7_IO12 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

/* USB Host power (efusA9) */
static iomux_v3_cfg_t const usb_pwr_pads_q[] = {
        MX6Q_PAD_EIM_D31__GPIO3_IO31 | MUX_PAD_CTRL(NO_PAD_CTRL),
};
static iomux_v3_cfg_t const usb_pwr_pads_dl[] = {
        MX6DL_PAD_EIM_D31__GPIO3_IO31 | MUX_PAD_CTRL(NO_PAD_CTRL),
};


int board_ehci_hcd_init(int port)
{
	iomux_v3_cfg_t const *usb_pads;
	unsigned usb_pad_count;

	if (port != 1)
		return 0;

	switch (fs_nboot_args.chBoardType) {
	case BT_ARMSTONEA9:
	case BT_QBLISSA9:
		if (is_cpu_type(MXC_CPU_MX6Q)) {
			usb_pads = usb_hub_pads_q;
			usb_pad_count = ARRAY_SIZE(usb_hub_pads_q);
		} else {
			usb_pads = usb_hub_pads_dl;
			usb_pad_count = ARRAY_SIZE(usb_hub_pads_dl);
		}
		imx_iomux_v3_setup_multiple_pads(usb_pads, usb_pad_count);

		/* Reset USB hub */
		gpio_direction_output(IMX_GPIO_NR(7, 12), 0);
		mdelay(2);
		gpio_set_value(IMX_GPIO_NR(7, 12), 1);
		break;

	case BT_EFUSA9:
#if 0
		if (is_cpu_type(MXC_CPU_MX6Q)) {
			usb_pads = usb_pwr_pads_q;
			usb_pad_count = ARRAY_SIZE(usb_pwr_pads_q);
		} else {
			usb_pads = usb_pwr_pads_dl;
			usb_pad_count = ARRAY_SIZE(usb_pwr_pads_dl);
		}
		imx_iomux_v3_setup_multiple_pads(usb_pads, usb_pad_count);

		/* Enable USB Host power */
		gpio_direction_output(IMX_GPIO_NR(3, 31), 1);
#endif
		break;

	default:
		break;
	}

        return 0;
}

int board_ehci_power(int port, int on)
{
	iomux_v3_cfg_t const *usb_pads;
	unsigned usb_pad_count;

	if (port != 1)
		return 0;

	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA9:
		if (is_cpu_type(MXC_CPU_MX6Q)) {
			usb_pads = usb_pwr_pads_q;
			usb_pad_count = ARRAY_SIZE(usb_pwr_pads_q);
		} else {
			usb_pads = usb_pwr_pads_dl;
			usb_pad_count = ARRAY_SIZE(usb_pwr_pads_dl);
		}
		imx_iomux_v3_setup_multiple_pads(usb_pads, usb_pad_count);

		/* Enable USB Host power */
		gpio_direction_output(IMX_GPIO_NR(3, 31), on);
		break;

	default:
		break;
	}

	return 0;
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
	setup_var("mtdids", MTDIDS_DEFAULT, 0);
	setup_var("partition", MTDPART_DEFAULT, 0);
	setup_var("mode", CONFIG_MODE, 0);

	/* Set some variables by runnning another variable */
	setup_var("console", bi->console, 1);
	setup_var("login", bi->login, 1);
	setup_var("mtdparts", bi->mtdparts, 1);
	setup_var("fsload", bi->fsload, 1);
	setup_var("network", bi->network, 1);
	setup_var("init", bi->init, 1);
	setup_var("rootfs", bi->rootfs, 1);
	setup_var("kernel", bi->kernel, 1);
	setup_var("fdt", bi->fdt, 1);
	setup_var("bootargs", "set_bootargs", 1);

	return 0;
}
#endif

#ifdef CONFIG_CMD_NET
/* enet pads definition */
static iomux_v3_cfg_t const enet_pads_rgmii_q[] = {
	MX6Q_PAD_ENET_MDIO__ENET_MDIO	 	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_ENET_MDC__ENET_MDC  		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_ENET_REF_CLK__ENET_TX_CLK	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_TXC__RGMII_TXC		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_TD0__RGMII_TD0		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_TD1__RGMII_TD1		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_TD2__RGMII_TD2		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_TD3__RGMII_TD3		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_TX_CTL__RGMII_TX_CTL	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_RXC__RGMII_RXC		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_RD0__RGMII_RD0		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_RD1__RGMII_RD1		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_RD2__RGMII_RD2		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_RD3__RGMII_RD3		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_RX_CTL__RGMII_RX_CTL	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	/* Phy Interrupt */
	MX6Q_PAD_GPIO_19__GPIO4_IO05		| MUX_PAD_CTRL(NO_PAD_CTRL),
	/* Phy Reset */
	MX6Q_PAD_ENET_CRS_DV__GPIO1_IO25	| MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const enet_pads_rgmii_dl[] = {
	MX6DL_PAD_ENET_MDIO__ENET_MDIO	 	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_ENET_MDC__ENET_MDC  		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_ENET_REF_CLK__ENET_TX_CLK	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_RGMII_TXC__RGMII_TXC		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_RGMII_TD0__RGMII_TD0		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_RGMII_TD1__RGMII_TD1		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_RGMII_TD2__RGMII_TD2		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_RGMII_TD3__RGMII_TD3		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_RGMII_TX_CTL__RGMII_TX_CTL	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_RGMII_RXC__RGMII_RXC		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_RGMII_RD0__RGMII_RD0		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_RGMII_RD1__RGMII_RD1		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_RGMII_RD2__RGMII_RD2		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_RGMII_RD3__RGMII_RD3		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_RGMII_RX_CTL__RGMII_RX_CTL	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	/* Phy Interrupt */
	MX6DL_PAD_GPIO_19__GPIO4_IO05		| MUX_PAD_CTRL(NO_PAD_CTRL),
	/* Phy Reset */
	MX6DL_PAD_ENET_CRS_DV__GPIO1_IO25	| MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const enet_pads_rmii_q[] = {
	MX6Q_PAD_ENET_MDIO__ENET_MDIO	 	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_ENET_MDC__ENET_MDC  		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_ENET_CRS_DV__ENET_RX_EN	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_ENET_RX_ER__ENET_RX_ER	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_ENET_TX_EN__ENET_TX_EN	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_ENET_RXD0__ENET_RX_DATA0	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_ENET_RXD1__ENET_RX_DATA1	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_ENET_TXD0__ENET_TX_DATA0	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_ENET_TXD1__ENET_TX_DATA1	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_TX_CTL__ENET_REF_CLK	| MUX_PAD_CTRL(ENET_PAD_CTRL),

	/* Phy Reset */
	MX6Q_PAD_SD4_DAT2__GPIO2_IO10		| MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const enet_pads_rmii_dl[] = {
	MX6DL_PAD_ENET_MDIO__ENET_MDIO	 	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_ENET_MDC__ENET_MDC  		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_ENET_CRS_DV__ENET_RX_EN	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_ENET_RX_ER__ENET_RX_ER	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_ENET_TX_EN__ENET_TX_EN	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_ENET_RXD0__ENET_RX_DATA0	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_ENET_RXD1__ENET_RX_DATA1	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_ENET_TXD0__ENET_TX_DATA0	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_ENET_TXD1__ENET_TX_DATA1	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6DL_PAD_RGMII_TX_CTL__ENET_REF_CLK	| MUX_PAD_CTRL(NO_PAD_CTRL),

	/* Phy Reset */
	MX6DL_PAD_SD4_DAT2__GPIO2_IO10		| MUX_PAD_CTRL(NO_PAD_CTRL),
};

int board_eth_init(bd_t *bis)
{
	u32 ccgr1, gpr1;
	int ret;
	iomux_v3_cfg_t const *enet_pads;
	unsigned enet_pad_count;
	int phy_addr;
	enum xceiver_type xcv_type;
	enum enet_freq freq; 
	struct iomuxc *iomux_regs = (struct iomuxc *)IOMUXC_BASE_ADDR;

	/* Set the IOMUX for ENET */
	switch (fs_nboot_args.chBoardType) {
	case BT_PICOMODA9:
		/* Use 100 MBit/s LAN on RMII pins */
		if (is_cpu_type(MXC_CPU_MX6Q)) {
			enet_pads = enet_pads_rmii_q;
			enet_pad_count = ARRAY_SIZE(enet_pads_rmii_q);
		} else {
			enet_pads = enet_pads_rmii_dl;
			enet_pad_count = ARRAY_SIZE(enet_pads_rmii_dl);
		}
		/* ENET CLK is generated in i.MX& and is an output */
		gpr1 = readl(&iomux_regs->gpr[1]);
		gpr1 |= IOMUXC_GPR1_ENET_CLK_SEL_MASK;
		writel(gpr1, &iomux_regs->gpr[1]);

		freq = ENET_50MHz;
		break;

	default:
		/* Use 1 GBit/s LAN on RGMII pins */
		if (is_cpu_type(MXC_CPU_MX6Q)) {
			enet_pads = enet_pads_rgmii_q;
			enet_pad_count = ARRAY_SIZE(enet_pads_rgmii_q);
		} else {
			enet_pads = enet_pads_rgmii_dl;
			enet_pad_count = ARRAY_SIZE(enet_pads_rgmii_dl);
		}
		/* ENET CLK is generated in PHY and is an input */
		gpr1 = readl(&iomux_regs->gpr[1]);
		gpr1 |= IOMUXC_GPR1_ENET_CLK_SEL_MASK;
		writel(gpr1, &iomux_regs->gpr[1]);

		freq = ENET_25MHz;
		break;
	}

	/* Activate ENET PLL */
	ret = enable_fec_anatop_clock(freq);
	if (ret < 0)
		return ret;

	/* Enable ENET clock in clock gating register 1 */
	ccgr1 = readl(CCM_CCGR1);
	writel(ccgr1 | MXC_CCM_CCGR1_ENET_CLK_ENABLE_MASK, CCM_CCGR1);

	imx_iomux_v3_setup_multiple_pads(enet_pads, enet_pad_count);

	/* Reset the PHY */
	switch (fs_nboot_args.chBoardType) {
	case BT_PICOMODA9:
		/* DP83484 PHY: This PHY needs at least 1 us reset pulse width
		   (GPIO_2_10). After power on it needs min 167 ms (after
		   reset is deasserted) before the first MDIO access can be
		   done. In a warm start, it only takes around 3 for this. As
		   we do not know whether this is a cold or warm start, we
		   must assume the worst case. */
		gpio_direction_output(IMX_GPIO_NR(2, 10), 0);
		udelay(10);
		gpio_set_value(IMX_GPIO_NR(2, 10), 1);
		mdelay(170);
		phy_addr = 1;
		xcv_type = RMII;
		break;

	default:
		/* Atheros AR8035: reset phy (GPIO_1_25) for at least 0.5 ms */
		gpio_direction_output(IMX_GPIO_NR(1, 25), 0);
		udelay(500);
		gpio_set_value(IMX_GPIO_NR(1, 25), 1);
		phy_addr = 4;
		xcv_type = RGMII;
		break;
	}

	return fecmxc_initialize_multi_type(bis, -1, phy_addr, IMX_FEC_BASE,
					    xcv_type);
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

/* Return the board revision; this is called when Linux is started and the
   value is passed to Linux */
unsigned int get_board_rev(void)
{
	return fs_nboot_args.chBoardRev;
}

/* Return a pointer to the hardware configuration; this is called when Linux
   is started and the structure is passed to Linux */
struct tag_fshwconfig *get_board_fshwconfig(void)
{
	return &fs_nboot_args;
}

/* Return a pointer to the M4 image and configuration; this is called when
   Linux is started and the structure is passed to Linux */
struct tag_fsm4config *get_board_fsm4config(void)
{
	return &fs_m4_args;
}
