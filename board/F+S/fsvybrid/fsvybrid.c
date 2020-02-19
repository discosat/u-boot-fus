/*
 * fsvybrid.c
 *
 * (C) Copyright 2015
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Board specific functions for F&S boards based on Freescale Vybrid CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#ifdef CONFIG_CMD_NET
#include <asm/fec.h>
#include <net.h>			/* eth_init(), eth_halt() */
#include <miiphy.h>
#include <netdev.h>			/* ne2000_initialize() */
#endif
#ifdef CONFIG_CMD_LCD
#include <cmd_lcd.h>			/* PON_*, POFF_* */
#endif
#include <serial.h>			/* struct serial_device */

#ifdef CONFIG_FSL_ESDHC
#include <mmc.h>
#include <fsl_esdhc.h>			/* fsl_esdhc_initialize(), ... */
#endif

#ifdef CONFIG_LED_STATUS_CMD
#include <status_led.h>			/* led_id_t */
#endif

#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/vybrid-regs.h>	/* SCSCM_BASE_ADDR, ... */
#include <asm/arch/vybrid-pins.h>
#include <asm/arch/iomux.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/scsc_regs.h>		/* struct vybrid_scsc_reg */
#include <asm/arch/clock.h>		/* vybrid_get_esdhc_clk() */
#include <i2c.h>

#include <linux/mtd/rawnand.h>		/* struct mtd_info, struct nand_chip */
#include <mtd/fsl_nfc_fus.h>		/* struct fsl_nfc_fus_platform_data */
#include <fdt_support.h>		/* do_fixup_by_path_u32(), ... */
#include "../common/fs_fdt_common.h"	/* fs_fdt_set_val(), ... */
#include "../common/fs_board_common.h"	/* fs_board_*() */

/* ------------------------------------------------------------------------- */

#define BT_ARMSTONEA5 0
#define BT_PICOCOMA5  1
#define BT_NETDCUA5   2
#define BT_PICOMODA5  4
#define BT_PICOMOD1_2 5
#define BT_AGATEWAY   6
#define BT_CUBEA5     7
#define BT_HGATEWAY   8

/* Features set in fs_nboot_args.chFeature1 */
#define FEAT1_CPU400  (1<<0)		/* 0: 500 MHz, 1: 400 MHz CPU */
#define FEAT1_2NDCAN  (1<<1)		/* 0: 1x CAN, 1: 2x CAN */
#define FEAT1_2NDLAN  (1<<4)		/* 0: 1x LAN, 1: 2x LAN */

/* Features set in fs_nboot_args.chFeature2 */
#define FEAT2_M4      (1<<0)		/* CPU has Cortex-M4 core */
#define FEAT2_L2      (1<<1)		/* CPU has Level 2 cache */
#define FEAT2_RMIICLK_CKO1 (1<<2)	/* RMIICLK (PTA6) 0: output, 1: input
					   CKO1 (PTB10) 0: unused, 1: output */
/* Device tree paths */
#define FDT_NAND	"/soc/aips-bus@40080000/nand@400e0000"
#define FDT_ETH_A	"/soc/aips-bus@40080000/ethernet@400d0000"
#define FDT_ETH_B	"/soc/aips-bus@40080000/ethernet@400d1000"

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

const struct fs_board_info board_info[16] = {
	{	/* 0 (BT_ARMSTONEA5) */
		.name = "armStoneA5",
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_linuxrc",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
	},
	{	/* 1 (BT_PICOCOMA5)*/
		.name = "PicoCOMA5",
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_linuxrc",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
	},
	{	/* 2 (BT_NETDCUA5) */
		.name = "NetDCUA5",
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_linuxrc",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
	},
	{	/* 3 */
		.name = "Unknown",
	},
	{	/* 4 (BT_PICOMODA5) */
		.name = "PicoMODA5",
	},
	{	/* 5 (BT_PICOMOD1_2) */
		.name = "PicoMOD1.2",
	},
	{	/* 6 (BT_AGATEWAY) */
		.name = "AGATEWAY",
		.bootdelay = "0",
		.updatecheck = "TargetFS.ubi(ubi0:data)",
		.installcheck = INSTALL_RAM,
		.recovercheck = "TargetFS.ubi(ubi0:recovery)",
//###		.console = ".console_serial",
		.console = ".console_none",
//###		.login = ".login_serial",
		.login = ".login_none",
		.mtdparts = ".mtdparts_ubionly",
		.network = ".network_off",
		.init = ".init_linuxrc",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_ubifs",
		.fdt = ".fdt_ubifs",
	},
	{	/* 7 (BT_CUBEA5) */
		.name = "CUBEA5",
		.bootdelay = "0",
		.updatecheck = "TargetFS.ubi(ubi0:data)",
		.installcheck = INSTALL_RAM,
		.recovercheck = "TargetFS.ubi(ubi0:recovery)",
		.console = ".console_none",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_ubionly",
		.network = ".network_off",
		.init = ".init_linuxrc",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_ubifs",
		.fdt = ".fdt_ubifs",
	},
	{	/* 8 (BT_HGATEWAY) */
		.name = "HGATEWAY",
		.bootdelay = "0",
		.updatecheck = "TargetFS.ubi(ubi0:data)",
		.installcheck = INSTALL_RAM,
		.recovercheck = "TargetFS.ubi(ubi0:recovery)",
//###		.console = ".console_serial",
		.console = ".console_none",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_ubionly",
		.network = ".network_off",
		.init = ".init_linuxrc",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_ubifs",
		.fdt = ".fdt_ubifs",
	},
	{	/* 9 */
		.name = "Unknown",
	},
	{	/* 10 */
		.name = "Unknown",
	},
	{	/* 11 */
		.name = "Unknown",
	},
	{	/* 12 */
		.name = "Unknown",
	},
	{	/* 13 */
		.name = "Unknown",
	},
	{	/* 14 */
		.name = "Unknown",
	},
	{	/* 15 */
		.name = "Unknown",
	},
};

/* ---- Stage 'f': RAM not valid, variables can *not* be used yet ---------- */

#ifdef CONFIG_NAND_FSL_NFC
static void setup_iomux_nfc(void)
{
	__raw_writel(0x002038df, IOMUXC_PAD_063);
	__raw_writel(0x002038df, IOMUXC_PAD_064);
	__raw_writel(0x002038df, IOMUXC_PAD_065);
	__raw_writel(0x002038df, IOMUXC_PAD_066);
	__raw_writel(0x002038df, IOMUXC_PAD_067);
	__raw_writel(0x002038df, IOMUXC_PAD_068);
	__raw_writel(0x002038df, IOMUXC_PAD_069);
	__raw_writel(0x002038df, IOMUXC_PAD_070);
	__raw_writel(0x002038df, IOMUXC_PAD_071);
	__raw_writel(0x002038df, IOMUXC_PAD_072);
	__raw_writel(0x002038df, IOMUXC_PAD_073);
	__raw_writel(0x002038df, IOMUXC_PAD_074);
	__raw_writel(0x002038df, IOMUXC_PAD_075);
	__raw_writel(0x002038df, IOMUXC_PAD_076);
	__raw_writel(0x002038df, IOMUXC_PAD_077);
	__raw_writel(0x002038df, IOMUXC_PAD_078);

	__raw_writel(0x005038d2, IOMUXC_PAD_094);
	__raw_writel(0x005038d2, IOMUXC_PAD_095);
	__raw_writel(0x006038d2, IOMUXC_PAD_097);
	__raw_writel(0x005038dd, IOMUXC_PAD_099);
	__raw_writel(0x006038d2, IOMUXC_PAD_100);
	__raw_writel(0x006038d2, IOMUXC_PAD_101);
}
#endif

int board_early_init_f(void)
{
#ifdef CONFIG_NAND_FSL_NFC
	setup_iomux_nfc();		/* Setup NAND flash controller */
#endif
	return 0;
}

/* Check board type */
int checkboard(void)
{
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();
	int nLAN;
	int nCAN;

	switch (board_type) {
	case BT_CUBEA5:
		nLAN = 0;
		break;
	default:
		nLAN = pargs->chFeatures1 & FEAT1_2NDLAN ? 2 : 1;
		break;
	}

	switch (board_type) {
	case BT_CUBEA5:
	case BT_AGATEWAY:
	case BT_HGATEWAY:
		nCAN = 0;
		break;
	default:
		nCAN = pargs->chFeatures1 & FEAT1_2NDCAN ? 2 : 1;
		break;
	}

	printf("Board: %s Rev %u.%02u (%d MHz, %dx DRAM, %dx LAN, %dx CAN)\n",
	       board_info[board_type].name, board_rev / 100, board_rev % 100,
	       pargs->chFeatures1 & FEAT1_CPU400 ? 400 : 500,
	       pargs->dwNumDram, nLAN, nCAN);

	//fs_board_show_nboot_args(pargs);

	return 0;
}

/* ---- Stage 'r': RAM valid, U-Boot relocated, variables can be used ------ */

int board_init(void)
{
	struct vybrid_scsc_reg *scsc;
	u32 temp;

	/* Copy NBoot args to variables and prepare command prompt string */
	fs_board_init_common(&board_info[fs_board_get_type()]);

#if 0
	__led_init(0, 0); //###
	__led_init(1, 0); //###
#endif

	/* The internal clock experiences significant drift so we must use the
	   external oscillator in order to maintain correct time in the
	   hwclock */
	scsc = (struct vybrid_scsc_reg *)SCSCM_BASE_ADDR;
	temp = __raw_readl(&scsc->sosc_ctr);
	temp |= VYBRID_SCSC_SICR_CTR_SOSC_EN;
	__raw_writel(temp, &scsc->sosc_ctr);

	return 0;
}

/* Register NAND devices. We actually split the NAND into two virtual devices
   to allow different ECC strategies for NBoot and the rest. */
void board_nand_init(void)
{
	struct fsl_nfc_fus_platform_data pdata;

	/* The first device skips the NBoot region (2 blocks) to protect it
	   from inadvertent erasure. The skipped region can not be written
	   and is always read as 0xFF. */
	pdata.options = NAND_BBT_SCAN2NDPAGE;
	pdata.t_wb = 0;
	pdata.eccmode = fs_board_get_nboot_args()->chECCtype;
	pdata.skipblocks = 2;
	pdata.flags = 0;
#ifdef CONFIG_NAND_REFRESH
	pdata.backup_sblock = CONFIG_SYS_NAND_BACKUP_START_BLOCK;
	pdata.backup_eblock = CONFIG_SYS_NAND_BACKUP_END_BLOCK;
#endif
	vybrid_nand_register(0, &pdata);

#if CONFIG_SYS_MAX_NAND_DEVICE > 1
	/* The second device just consists of the NBoot region (2 blocks) and
	   is software write-protected by default. It uses a different ECC
	   strategy. ### TODO ### In fact we actually need special code to
	   store the NBoot image. */
	pdata.options |= NAND_SW_WRITE_PROTECT;
	pdata.eccmode = VYBRID_NFC_ECCMODE_32BIT;
	pdata.flags = VYBRID_NFC_SKIP_INVERSE;
#ifdef CONFIG_NAND_REFRESH
	pdata.backupstart = 0;
	pdata.backupend = 0;
#endif
	vybrid_nand_register(0, &pdata);
#endif
}

#ifdef CONFIG_FSL_ESDHC
struct fsl_esdhc_cfg esdhc_cfg[] = {
	{
		.esdhc_base = ESDHC0_BASE_ADDR,
		.sdhc_clk = 0,
		.max_bus_width = 4,
	},
	{
		.esdhc_base = ESDHC1_BASE_ADDR,
		.sdhc_clk = 0,
		.max_bus_width = 4,
	},
};

int board_mmc_getcd(struct mmc *mmc)
{
	u32 val;

	switch (fs_board_get_type()) {
	case BT_AGATEWAY:		/* PAD51 = GPIO1, Bit 19 */
		val = __raw_readl(0x400FF050) & (1 << 19);
		break;

	case BT_ARMSTONEA5:
	case BT_NETDCUA5:		/* PAD134 = GPIO4, Bit 6 */
		/* #### Check if NetDCUA5 is working ### */
		val = __raw_readl(0x400FF110) & (1 << 6);
		break;

	default:
		return 1;		/* Assume card present */
	}

	return (val == 0);
}

#define MVF600_GPIO_SDHC_CD \
	(PAD_CTL_SPEED_HIGH | PAD_CTL_DSE_20ohm | PAD_CTL_IBE_ENABLE)
int board_mmc_init(bd_t *bis)
{
	int index;
	u32 val;

	switch (fs_board_get_type()) {
	case BT_AGATEWAY:
		__raw_writel(MVF600_GPIO_SDHC_CD, IOMUXC_PAD_051);
		index = 0;
		break;

	case BT_ARMSTONEA5:
	case BT_NETDCUA5:
		__raw_writel(MVF600_GPIO_GENERAL_CTRL | PAD_CTL_IBE_ENABLE,
			     IOMUXC_PAD_134);
		/* fall through to default */
	default:
		index = 1;
		break;
	}

	val = MVF600_SDHC_PAD_CTRL | PAD_CTL_MODE_ALT5;
	if (!index) {				   /* ESDHC0 */
		__raw_writel(val, IOMUXC_PAD_045); /* CLK */
		__raw_writel(val, IOMUXC_PAD_046); /* CMD */
		__raw_writel(val, IOMUXC_PAD_047); /* DAT0 */
		__raw_writel(val, IOMUXC_PAD_048); /* DAT1 */
		__raw_writel(val, IOMUXC_PAD_049); /* DAT2 */
		__raw_writel(val, IOMUXC_PAD_050); /* DAT3 */
	} else {				   /* ESDHC1 */
		__raw_writel(val, IOMUXC_PAD_014); /* CLK */
		__raw_writel(val, IOMUXC_PAD_015); /* CMD */
		__raw_writel(val, IOMUXC_PAD_016); /* DAT0 */
		__raw_writel(val, IOMUXC_PAD_017); /* DAT1 */
		__raw_writel(val, IOMUXC_PAD_018); /* DAT2 */
		__raw_writel(val, IOMUXC_PAD_019); /* DAT3 */
	}

	esdhc_cfg[index].sdhc_clk = vybrid_get_esdhc_clk(index);
	return fsl_esdhc_initialize(bis, &esdhc_cfg[index]);
}
#endif /* CONFIG_FSL_ESDHC */

#ifdef CONFIG_USB_EHCI_VYBRID
int board_ehci_hcd_init(int port)
{
	if (!port) {
		/* Configure USB0_PWR (PTE5) as GPIO output and set to 1 by
		   writing to GPIO3_PSOR[14] */
		__raw_writel(MVF600_GPIO_GENERAL_CTRL | PAD_CTL_OBE_ENABLE,
			     IOMUXC_PAD_110);
		__raw_writel(1 << (110 & 0x1F), 0x400ff0c4);
	} else {
		/* Configure USB1_PWR (PTE6) as GPIO output and set to 1 by
		   writing to GPIO3_PSOR[15] */
		__raw_writel(MVF600_GPIO_GENERAL_CTRL | PAD_CTL_OBE_ENABLE,
			     IOMUXC_PAD_111);
		__raw_writel(1 << (111 & 0x1F), 0x400ff0c4);
	}

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
	/* Set up all board specific variables */
	fs_board_late_init_common();

	return 0;
}
#endif /* CONFIG_BOARD_LATE_INIT */


#ifdef CONFIG_CMD_NET
static void fecpin_config(uint32_t enet_addr)
{
	/*
	 * Configure as ethernet. There is a hardware bug on Vybrid when
	 * RMIICLK (PTA6) is used as RMII clock output. Then outgoing data
	 * changes value on the wrong edge, i.e. when it is latched in the
	 * PHY. Unfortunately we have some board revisions where this
	 * configuration is used. To reduce the risk that the PHY latches the
	 * wrong data, we set the edge speed of the data signals to low and of
	 * the clock signal to high. This gains about 2ns difference but is
	 * unstable and does not work with all PHYs.
	 *
	 * In our newer board revisions we either use an external oscillator
	 * (AGATEWAY) or we have looped back CKO1 (PTB10) to RMIICKL (PTA6)
	 * and output the RMII clock on CKO1. Then PTA6 is a clock input and
	 * everything works as expected.
	 *
	 * The drive strength values below guarantee very stable results, but
	 * if EMC conformance requires, they can be reduced even more:
	 * 0x00100042 for outputs, 0x00100001 for inputs and 0x00100043 for
	 * mixed inputs/outputs.
	 */
	if (enet_addr == MACNET0_BASE_ADDR) {
		__raw_writel(0x001000c2, IOMUXC_PAD_045);	/*MDC*/
		__raw_writel(0x001000c3, IOMUXC_PAD_046);	/*MDIO*/
		__raw_writel(0x001000c1, IOMUXC_PAD_047);	/*RxDV*/
		__raw_writel(0x001000c1, IOMUXC_PAD_048);	/*RxD1*/
		__raw_writel(0x001000c1, IOMUXC_PAD_049);	/*RxD0*/
		__raw_writel(0x001000c1, IOMUXC_PAD_050);	/*RxER*/
		__raw_writel(0x001000c2, IOMUXC_PAD_051);	/*TxD1*/
		__raw_writel(0x001000c2, IOMUXC_PAD_052);	/*TxD0*/
		__raw_writel(0x001000c2, IOMUXC_PAD_053);	/*TxEn*/
	} else if (enet_addr == MACNET1_BASE_ADDR) {
		__raw_writel(0x001000c2, IOMUXC_PAD_054);	/*MDC*/
		__raw_writel(0x001000c3, IOMUXC_PAD_055);	/*MDIO*/
		__raw_writel(0x001000c1, IOMUXC_PAD_056);	/*RxDV*/
		__raw_writel(0x001000c1, IOMUXC_PAD_057);	/*RxD1*/
		__raw_writel(0x001000c1, IOMUXC_PAD_058);	/*RxD0*/
		__raw_writel(0x001000c1, IOMUXC_PAD_059);	/*RxER*/
		__raw_writel(0x001000c2, IOMUXC_PAD_060);	/*TxD1*/
		__raw_writel(0x001000c2, IOMUXC_PAD_061);	/*TxD0*/
		__raw_writel(0x001000c2, IOMUXC_PAD_062);	/*TxEn*/
	}
}

/* Read a MAC address from OTP memory */
int get_otp_mac(unsigned long otp_addr, uchar *enetaddr)
{
	u32 val;
	static const uchar empty1[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	static const uchar empty2[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	/*
	 * Read a MAC address from OTP memory on Vybrid; it is stored in the
	 * following order:
	 *
	 *   Byte 1 in mac_h[7:0]
	 *   Byte 2 in mac_h[15:8]
	 *   Byte 3 in mac_h[23:16]
	 *   Byte 4 in mac_h[31:24]
	 *   Byte 5 in mac_l[23:16]
	 *   Byte 6 in mac_l[31:24]
	 *
	 * Please note that this layout is different to i.MX6.
	 *
	 * The MAC address itself can be empty (all six bytes zero) or erased
	 * (all six bytes 0xFF). In this case the whole address is ignored.
	 *
	 * In addition to the address itself, there may be a count stored in
	 * mac_l[15:8].
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
	enetaddr[0] = val & 0xFF;
	enetaddr[1] = (val >> 8) & 0xFF;
	enetaddr[2] = (val >> 16) & 0xFF;
	enetaddr[3] = val >> 24;

	val = __raw_readl(otp_addr + 0x10);
	enetaddr[4] = (val >> 16) & 0xFF;
	enetaddr[5] = val >> 24;

	if (!memcmp(enetaddr, empty1, 6) || !memcmp(enetaddr, empty2, 6))
		return 0;

	val >>= 8;
	val &= 0xFF;
	if (val == 0xFF)
		val = 0;

	return (int)(val + 1);
}


/* Set the ethaddr environment variable according to index */
void fs_eth_set_ethaddr(int index)
{
	uchar enetaddr[6];
	int count, i;
	int offs = index;

	/* Try to fulfil the request in the following order:
	 *   1. From environment variable
	 *   2. MAC0 from OTP
	 *   3. MAC1 from OTP
	 *   4. CONFIG_ETHADDR_BASE
	 */
	if (eth_env_get_enetaddr_by_index("eth", index, enetaddr))
		return;

	count = get_otp_mac(OTP_BASE_ADDR + 0x620, enetaddr);
	if (count <= offs) {
		offs -= count;
		count = get_otp_mac(OTP_BASE_ADDR + 0x640, enetaddr);
		if (count <= offs) {
			offs -= count;
			eth_parse_enetaddr(CONFIG_ETHADDR_BASE, enetaddr);
		}
	}

	i = 6;
	do {
		offs += (int)enetaddr[--i];
		enetaddr[i] = offs & 0xFF;
		offs >>= 8;
	} while (i);

	eth_env_set_enetaddr_by_index("eth", index, enetaddr);
}

/* Initialize ethernet by registering the available FEC devices */
int board_eth_init(bd_t *bis)
{
	int ret;
	int id;
	int phy_addr;
	uint32_t enet_addr;
	unsigned int board_type = fs_board_get_type();
	unsigned int board_rev = fs_board_get_rev();
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();


	/* CUBEA5 has not ethernet at all, do not even configure PHY clock */
	if (board_type == BT_CUBEA5) {
		fs_eth_set_ethaddr(0);	/* MAC for WLAN */
		return 0;
	}

	/* Configure ethernet PHY clock depending on board type and revision */
	if ((board_type == BT_AGATEWAY) && (board_rev >= 110)) {
		/* Starting with board Rev 1.10, AGATEWAY has an external
		   oscillator and needs RMIICLK (PTA6) as input */
		__raw_writel(0x00203191, IOMUXC_PAD_000);
	} else {
#if 1
		/* Use PLL for RMII clock */
		struct clkctl *ccm = (struct clkctl *)CCM_BASE_ADDR;
		u32 temp;

		/* Configure PLL5 main clock for RMII clock. */
		temp = 0x2001;		/* ANADIG_PLL5_CTRL: Enable, 50MHz */
		__raw_writel(temp, 0x400500E0);
		if (pargs->chFeatures2 & FEAT2_RMIICLK_CKO1) {
			/* We have a board revision with a direct connection
			   between PTB10 and PTA6, so we will use CKO1 (PTB10)
			   to output the PLL5 clock signal and use RMIICLK
			   (PTA6) as input. */
			temp = __raw_readl(&ccm->ccosr);
			temp &= ~(0x7FF << 0);	/* PLL5 output on CKO1 */
			temp |= (0x04 << 0) | (0 << 6) | (1 << 10);
			__raw_writel(temp, &ccm->ccosr);
			temp = __raw_readl(&ccm->cscmr2);
			temp |= (0<<4);		/* Use RMII clock as input */
			__raw_writel(temp, &ccm->cscmr2);
			/* See commment above about drive strength */
			__raw_writel(0x006000c2, IOMUXC_PAD_032);
			__raw_writel(0x002000c1, IOMUXC_PAD_000);
		} else {
			/* We do not have a connection between PTB10 and PTA6
			   and we also don't have an external oscillator. We
			   must use RMIICLK (PTA6) as RMII clock output. This
			   is not stable and may not work with all PHYs! See
			   the comment in function fecpin_setclear(). */
			temp = __raw_readl(&ccm->cscmr2);
			temp |= (2<<4);		/* Use PLL5 for RMII */
			__raw_writel(temp, &ccm->cscmr2);
			temp = __raw_readl(&ccm->cscdr1);
			temp |= (1<<24);	/* Enable RMII clock output */
			__raw_writel(temp, &ccm->cscdr1);
			__raw_writel(0x00103942, IOMUXC_PAD_000);
		}
#else
		/* We have an external oscillator for RMII clock, configure
		   RMIICLK (PTA6) as input */
		__raw_writel(0x00203191, IOMUXC_PAD_000);
#endif /* CONFIG_FS_VYBRID_PLL_ETH */
	}

	/* Get info on first ethernet port; AGATEWAY and HGATEWAY always only
	   have one port which is actually FEC1! */
	fs_eth_set_ethaddr(0);
	phy_addr = 0;
	enet_addr = MACNET0_BASE_ADDR;
	id = -1;
	switch (board_type) {
	case BT_PICOCOMA5:
		phy_addr = 1;
		/* Fall through to case BT_ARMSTONEA5 */
	case BT_ARMSTONEA5:
	case BT_NETDCUA5:
		if (pargs->chFeatures1 & FEAT1_2NDLAN)
			id = 0;
		break;
	case BT_AGATEWAY:
	case BT_HGATEWAY:
		enet_addr = MACNET1_BASE_ADDR;
		break;
	}

	/* Configure pads for first port */
	fecpin_config(enet_addr);

	/* Probe first PHY and ethernet port */
	ret = fecmxc_initialize_multi_type(bis, id, phy_addr, enet_addr, RMII);
	if (ret)
		return ret;

	if (board_type == BT_AGATEWAY) {
		fs_eth_set_ethaddr(1);	/* MAC for WLAN */
		return 0;
	}

	if (!(pargs->chFeatures1 & FEAT1_2NDLAN))
		return 0;

	/* Get info on second ethernet port */
	fs_eth_set_ethaddr(1);
	switch (board_type) {
	case BT_PICOCOMA5:
		phy_addr = 1;
		break;
	case BT_ARMSTONEA5:
	case BT_NETDCUA5:
		phy_addr = 0;
		break;
	}
	enet_addr = MACNET1_BASE_ADDR;

	/* Configure pads for second port */
	fecpin_config(enet_addr);

	/* Probe second PHY and ethernet port */
	return fecmxc_initialize_multi_type(bis, 1, phy_addr, enet_addr, RMII);
}
#endif /* CONFIG_CMD_NET */

/* Get board revision */
unsigned int get_board_rev(void)
{
	return fs_board_get_rev();
}

#ifdef CONFIG_LED_STATUS_CMD
/* We have LEDs on PTC30 (Pad 103) and PTC31 (Pad 104); on CUBEA5 and
   AGATEWAY, the logic is inverted.
   On HGATEWAY, we allow using the RGB LED PTD25 (red, Pad 69) and PTD24
   (blue, Pad 70) as status LEDs.
*/
#if 0
void __led_init(led_id_t mask, int state)
{
	printf("### __led_init()\n");
	if ((mask > 1) || (fs_board_get_type() != BT_HGATEWAY))
		return;
	__raw_writel(0x00000142, mask ? 0x40048118 : 0x40048114);
	__led_set(mask, state);
}
#endif
void __led_set(led_id_t mask, int state)
{
	unsigned long reg;
	unsigned int board_type = fs_board_get_type();

	if (mask > 1)
		return;

	if ((board_type == BT_CUBEA5) || (board_type == BT_AGATEWAY))
		state = !state;

	if (board_type == BT_HGATEWAY) {
		/* Write to GPIO2_PSOR or GPIO2_PCOR */
		mask += 5;
		reg = 0x400ff084;
	} else {
		/* Write to GPIO3_PSOR or GPIO3_PCOR */
		mask += 7;
		reg = 0x400ff0c4;
	}
	__raw_writel(1 << mask, state ? reg : (reg + 4));
}

void __led_toggle(led_id_t mask)
{
	unsigned long reg;
	unsigned int board_type = fs_board_get_type();

	if (mask > 1)
		return;

	if (board_type == BT_HGATEWAY) {
		/* Write to GPIO2_PTOR */
		mask += 5;
		reg = 0x400ff08c;
	} else {
		/* Write to GPIO3_PTOR */
		mask += 7;
		reg = 0x400ff0cc;
	}
	__raw_writel(1 << mask, reg);
}
#endif /* CONFIG_LED_STATUS_CMD */

#ifdef CONFIG_CMD_LCD
// ####TODO
void s3c64xx_lcd_board_init(void)
{
	/* Setup GPF15 to output 0 (backlight intensity 0) */
	__REG(GPFDAT) &= ~(0x1<<15);
	__REG(GPFCON) = (__REG(GPFCON) & ~(0x3<<30)) | (0x1<<30);
	__REG(GPFPUD) &= ~(0x3<<30);

	/* Setup GPK[3] to output 1 (Buffer enable off), GPK[2] to output 1
	   (Display enable off), GPK[1] to output 0 (VCFL off), GPK[0] to
	   output 0 (VLCD off), no pull-up/down */
	__REG(GPKDAT) = (__REG(GPKDAT) & ~(0xf<<0)) | (0xc<<0);
	__REG(GPKCON0) = (__REG(GPKCON0) & ~(0xFFFF<<0)) | (0x1111<<0);
	__REG(GPKPUD) &= ~(0xFF<<0);
}

void s3c64xx_lcd_board_enable(int index)
{
	switch (index) {
	case PON_LOGIC:			  /* Activate VLCD */
		__REG(GPKDAT) |= (1<<0);
		break;

	case PON_DISP:			  /* Activate Display Enable signal */
		__REG(GPKDAT) &= ~(1<<2);
		break;

	case PON_CONTR:			  /* Activate signal buffers */
		__REG(GPKDAT) &= ~(1<<3);
		break;

	case PON_PWM:			  /* Activate VEEK*/
		__REG(GPFDAT) |= (0x1<<15); /* full intensity
					       #### TODO: actual PWM value */
		break;

	case PON_BL:			  /* Activate VCFL */
		__REG(GPKDAT) |= (1<<1);
		break;

	default:
		break;
	}
}

void s3c64xx_lcd_board_disable(int index)
{
	switch (index) {
	case POFF_BL:			  /* Deactivate VCFL */
		__REG(GPKDAT) &= ~(1<<1);
		break;

	case POFF_PWM:			  /* Deactivate VEEK*/
		__REG(GPFDAT) &= ~(0x1<<15);
		break;

	case POFF_CONTR:		  /* Deactivate signal buffers */
		__REG(GPKDAT) |= (1<<3);
		break;

	case POFF_DISP:			  /* Deactivate Display Enable signal */
		__REG(GPKDAT) |= (1<<2);
		break;

	case POFF_LOGIC:		  /* Deactivate VLCD */
		__REG(GPKDAT) &= ~(1<<0);
		break;

	default:
		break;
	}
}
#endif

#ifdef CONFIG_OF_BOARD_SETUP
/* Do any additional board-specific device tree modifications */
int ft_board_setup(void *fdt, bd_t *bd)
{
	int offs;
	struct fs_nboot_args *pargs = fs_board_get_nboot_args();

	printf("   Setting run-time properties\n");

	/* Set ECC mode for NAND driver */
	offs = fs_fdt_path_offset(fdt, FDT_NAND);
	if (offs >= 0)
		fs_fdt_set_u32(fdt, offs, "fus,ecc_mode", pargs->chECCtype, 1);

	/* Set bdinfo entries */
	offs = fs_fdt_path_offset(fdt, "/bdinfo");
	if (offs >= 0) {
		int id = 0;

		/* Set common bdinfo entries */
		fs_fdt_set_bdinfo(fdt, offs);

		/* MAC addresses */
		if (fs_board_get_type() == BT_CUBEA5)
			fs_fdt_set_wlan_macaddr(fdt, offs, id++, 0);
		else
			fs_fdt_set_macaddr(fdt, offs, id++);
		if (pargs->chFeatures1 & FEAT1_2NDLAN)
			fs_fdt_set_macaddr(fdt, offs, id++);
	}

	/* Disable second ethernet node if feature is not available */
	if (!(pargs->chFeatures1 & FEAT1_2NDLAN))
		fs_fdt_enable(fdt, FDT_ETH_B, 0);

	return 0;
}
#endif /* CONFIG_OF_BOARD_SETUP */

/* Board specific cleanup before Linux is started */
void board_preboot_os(void)
{
#if 0
	/* Switch off backlight and display voltages */
	/* ### TODO: In the future the display should pass smoothly to Linux,
	   then switching everything off should not be necessary anymore. */
	fs_disp_set_backlight_all(0);
	fs_disp_set_power_all(0);
#endif

	/* Shut down all ethernet PHYs (suspend mode) */
	mdio_shutdown_all();
}
