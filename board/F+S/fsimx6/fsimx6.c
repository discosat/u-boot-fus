/*
 * (C) Copyright 2014
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
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
//#include <asm/arch/mx6-pins.h>
#include <asm/arch/mx6x_pins.h>
#include <asm/arch/iomux.h>
#include <asm/imx-common/iomux-v3.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/crm_regs.h> /* nandf clock settings */
#include <asm/arch/clock.h>
//#####include <asm/imx-common/boot_mode.h>

#include <linux/mtd/nand.h>		/* struct mtd_info, struct nand_chip */

#ifdef CONFIG_FSL_ESDHC
struct fsl_esdhc_cfg esdhc_cfg[] = {
	{USDHC1_BASE_ADDR},
	{USDHC2_BASE_ADDR},
	{USDHC3_BASE_ADDR},
	{USDHC4_BASE_ADDR},
};
#endif

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


#define UART_PAD_CTRL  (PAD_CTL_PUS_100K_UP |		\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm |			\
	PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define ENET_PAD_CTRL ( \
		PAD_CTL_PKE | PAD_CTL_PUE | /*Kiepfer*/ 	\
		PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED |	\
		PAD_CTL_DSE_40ohm   | PAD_CTL_HYS)

#define GPMI_PAD_CTRL0 (PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP)
#define GPMI_PAD_CTRL1 (PAD_CTL_DSE_40ohm | PAD_CTL_SPEED_MED | PAD_CTL_SRE_FAST)
#define GPMI_PAD_CTRL2 (GPMI_PAD_CTRL0 | GPMI_PAD_CTRL1)

#define USDHC_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |            \
	PAD_CTL_PUS_47K_UP  | PAD_CTL_SPEED_LOW |               \
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

#if defined(CONFIG_MMC) && defined(CONFIG_USB_STORAGE) && defined(CONFIG_CMD_FAT)
#define UPDATE_DEF "mmc,usb"
#elif defined(CONFIG_MMC) && defined(CONFIG_CMD_FAT)
#define UPDATE_DEF "mmc"
#elif defined(CONFIG_USB_STORAGE) && defined(CONFIG_CMD_FAT)
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
		.console = "_console_serial",
		.login = "_login_serial",
		.mtdparts = "_mtdparts_std",
		.network = "_network_off",
		.init = "_init_linuxrc",
		.rootfs = "_rootfs_ubifs",
		.kernel = "_kernel_nand",
		.fdt = "_fdt_nand",
		.fsload = "_fsload_fat",
	},
	{	/* 1 (BT_ARMSTONEA9)*/
		.name = "armStoneA9",
		.mach_type = MACH_TYPE_ARMSTONEA9,
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = UPDATE_DEF,
		.recovercheck = UPDATE_DEF,
		.console = "_console_serial",
		.login = "_login_serial",
		.mtdparts = "_mtdparts_std",
		.network = "_network_off",
		.init = "_init_linuxrc",
		.rootfs = "_rootfs_ubifs",
		.kernel = "_kernel_nand",
		.fdt = "_fdt_nand",
		.fsload = "_fsload_fat",
	},
	{	/* 2 (BT_PICOMODA9) */
		.name = "PicoMODA9",
		.mach_type = MACH_TYPE_PICOMODA9,
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = UPDATE_DEF,
		.recovercheck = UPDATE_DEF,
		.console = "_console_serial",
		.login = "_login_serial",
		.mtdparts = "_mtdparts_std",
		.network = "_network_off",
		.init = "_init_linuxrc",
		.rootfs = "_rootfs_ubifs",
		.kernel = "_kernel_nand",
		.fdt = "_fdt_nand",
		.fsload = "_fsload_fat",
	},
	{	/* 3 (BT_QBLISSA9) */
		.name = "QBlissA9",
		.mach_type = MACH_TYPE_QBLISSA9,
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = UPDATE_DEF,
		.recovercheck = UPDATE_DEF,
		.console = "_console_serial",
		.login = "_login_serial",
		.mtdparts = "_mtdparts_std",
		.network = "_network_off",
		.init = "_init_linuxrc",
		.rootfs = "_rootfs_ubifs",
		.kernel = "_kernel_nand",
		.fdt = "_fdt_nand",
		.fsload = "_fsload_fat",
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

/* nand flash pads  */
iomux_v3_cfg_t /*const###*/ nfc_pads[] = {
	MX6Q_PAD_NANDF_CLE__RAWNAND_CLE		| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_ALE__RAWNAND_ALE		| MUX_PAD_CTRL(NO_PAD_CTRL),
	MX6Q_PAD_NANDF_WP_B__RAWNAND_RESETN	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_RB0__RAWNAND_READY0	| MUX_PAD_CTRL(GPMI_PAD_CTRL0),
	MX6Q_PAD_NANDF_CS0__RAWNAND_CE0N	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_SD4_CMD__RAWNAND_RDN		| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_SD4_CLK__RAWNAND_WRN		| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D0__RAWNAND_D0	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D1__RAWNAND_D1	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D2__RAWNAND_D2	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D3__RAWNAND_D3	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D4__RAWNAND_D4	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D5__RAWNAND_D5	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D6__RAWNAND_D6	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6Q_PAD_NANDF_D7__RAWNAND_D7	| MUX_PAD_CTRL(GPMI_PAD_CTRL2),
};

/* enet pads definition */
iomux_v3_cfg_t /*const###*/ enet_pads[] = {
	MX6Q_PAD_ENET_MDIO__ENET_MDIO	 	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_ENET_MDC__ENET_MDC  		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_ENET_REF_CLK__ENET_TX_CLK	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_TXC__ENET_RGMII_TXC	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_TD0__ENET_RGMII_TD0	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_TD1__ENET_RGMII_TD1	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_TD2__ENET_RGMII_TD2	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_TD3__ENET_RGMII_TD3	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_TX_CTL__RGMII_TX_CTL	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_RXC__ENET_RGMII_RXC	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_RD0__ENET_RGMII_RD0	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_RD1__ENET_RGMII_RD1	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_RD2__ENET_RGMII_RD2	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_RD3__ENET_RGMII_RD3	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6Q_PAD_RGMII_RX_CTL__RGMII_RX_CTL	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	/* Phy Interrupt */
	MX6Q_PAD_GPIO_19__GPIO_4_5		| MUX_PAD_CTRL(NO_PAD_CTRL),
	/* Phy Reset */
	MX6Q_PAD_ENET_CRS_DV__GPIO_1_25		| MUX_PAD_CTRL(NO_PAD_CTRL),
};

/* SD/MMC card pads definition */
iomux_v3_cfg_t /*const###*/ usdhc3_pads[] = {
	MX6Q_PAD_SD3_CLK__USDHC3_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD3_CMD__USDHC3_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD3_DAT0__USDHC3_DAT0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD3_DAT1__USDHC3_DAT1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD3_DAT2__USDHC3_DAT2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_SD3_DAT3__USDHC3_DAT3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6Q_PAD_NANDF_CS2__GPIO_6_15 | MUX_PAD_CTRL(NO_PAD_CTRL),    /* CD */
//###	MX6Q_PAD_SD3_RST__USDHC3_RST | MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

/* USB Host pads definition */
iomux_v3_cfg_t const usb_pads[] = {
        MX6Q_PAD_GPIO_17__GPIO_7_12 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void setup_gpmi_nand(void)
{
	int reg;
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

	/* config gpmi nand iomux */
	imx_iomux_v3_setup_multiple_pads(nfc_pads,
					 ARRAY_SIZE(nfc_pads));

	/* config gpmi and bch clock to 11Mhz*/
	reg = readl(&mxc_ccm->cs2cdr);
	reg &= 0xF800FFFF;
	reg |= 0x01E40000;
	writel(reg, &mxc_ccm->cs2cdr);

	/* enable gpmi and bch clock gating */

	reg = readl(&mxc_ccm->CCGR4);
	reg |= 0xFF003000;
	writel(reg, &mxc_ccm->CCGR4);

	/* enable apbh clock gating */
	reg = readl(&mxc_ccm->CCGR0);
	reg |= 0x0030;
	writel(reg, &mxc_ccm->CCGR0);
}

int board_early_init_f(void)
{
//###	setup_gpmi_nand();

	return 0;
}

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

/* Register NAND devices. We actually split the NAND into two virtual devices
   to allow different ECC strategies for NBoot and the rest. */
void board_nand_init(void)
{
	//### TODO ###
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
int board_mmc_getcd(struct mmc *mmc)
{
	switch (fs_nboot_args.chBoardType) {
	case BT_ARMSTONEA9:
		return 1;		/* Assume card present */

	default:
		return 1;		/* Assume card present */
	}
}

int board_mmc_init(bd_t *bis)
{
	int index;

	switch (fs_nboot_args.chBoardType) {
	case BT_ARMSTONEA9:
		/* Card Detect (CD) on NANDF_CS2 pin */
		index = 2;		/* USDHC3 */
		break;

	default:
		// ### TODO: other boards, on USDHC1 and USHCD2 also init CD
		index = 2;
		break;
	}

	if (index == 2) {
		imx_iomux_v3_setup_multiple_pads(usdhc3_pads,
						 ARRAY_SIZE(usdhc3_pads));
	}

	return fsl_esdhc_initialize(bis, &esdhc_cfg[index]);
}
#endif

#ifdef CONFIG_USB_EHCI_MX6
int board_ehci_hcd_init(int port)
{
        imx_iomux_v3_setup_multiple_pads(usb_pads, ARRAY_SIZE(usb_pads));

        /* Reset USB hub */
        gpio_direction_output(IMX_GPIO_NR(7, 12), 0);
        mdelay(2);
        gpio_set_value(IMX_GPIO_NR(7, 12), 1);

        return 0;
}
#endif

#ifdef CONFIG_BOARD_LATE_INIT
void setup_var(const char *varname, const char *content, int runvar)
{
	/* If variable does not contain string "undef", do not change it */
	if (strcmp(getenv(varname), "undef"))
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
static void setup_iomux_enet(void) {
	imx_iomux_v3_setup_multiple_pads(enet_pads, ARRAY_SIZE(enet_pads));

	switch (fs_nboot_args.chBoardType) {
	case BT_PICOMODA9:
		/* reset phy (GPIO_2_10) for at least 10ms */
		gpio_direction_output(IMX_GPIO_NR(2, 10), 0);
		udelay(10000);
		gpio_set_value(IMX_GPIO_NR(2, 10), 1);
		break;

	default:
		/* reset phy (GPIO_1_25) for at least 0.5 ms */
		gpio_direction_output(IMX_GPIO_NR(1, 25), 0);
		udelay(500);
		gpio_set_value(IMX_GPIO_NR(1, 25), 1);
		break;
	}
}

int board_eth_init(bd_t *bis)
{
	setup_iomux_enet();

	return cpu_eth_init(bis);
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
