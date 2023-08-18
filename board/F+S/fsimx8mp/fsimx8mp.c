/*
 * fsimx8mp.c
 *
 * (C) Copyright 2021
 * Patrik Jakob, F&S Elektronik Systeme GmbH, jakob@fs-net.de
 * Anatol Derksen, F&S Elektronik Systeme GmbH, derksen@fs-net.de
 * Philipp Gerbach, F&S Elektronik Systeme GmbH, gerbach@fs-net.de
 *
 * Board specific functions for F&S boards based on Freescale i.MX8MP CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <errno.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/io.h>
#include <env_internal.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm-generic/gpio.h>
#include <asm/arch/imx8mp_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/arch/clock.h>
#include <spl.h>
#include <asm/mach-imx/dma.h>
#include <power/pmic.h>
#ifdef CONFIG_USB_TCPC
#include "../common/tcpc.h"
#endif
#include <usb.h>
#include <dwc3-uboot.h>
#include <mmc.h>
#include <linux/delay.h>		/* udelay() */
#include <fdt_support.h>		/* fdt_getprop_u32_default_node() */
#include <hang.h>			/* hang() */
#include "../common/fs_fdt_common.h"	/* fs_fdt_set_val(), ... */
#include "../common/fs_board_common.h"	/* fs_board_*() */
#include "../common/fs_eth_common.h"	/* fs_eth_*() */
#include "../common/fs_image_common.h"	/* fs_image_*() */
#include "../common/fs_mmc_common.h"	/* fs_image_*() */
#include <imx_thermal.h> /* for temp ranges */

DECLARE_GLOBAL_DATA_PTR;

#define BT_PICOCOREMX8MP 	0
#define BT_PICOCOREMX8MPr2 	1
#define BT_ARMSTONEMX8MP 	2
#define BT_EFUSMX8MP 		3

#define FEAT_ETH_A 	(1<<0)	/* 0: no LAN0,  1: has LAN0 */
#define FEAT_ETH_B	(1<<1)	/* 0: no LAN1,  1: has LAN1 */
#define FEAT_DISP_A	(1<<2)	/* 0: MIPI-DSI, 1: LVDS lanes 0-3 */
#define FEAT_DISP_B	(1<<3)	/* 0: HDMI,     1: LVDS lanes 0-4 or (4-7 if DISP_A=1) */
#define FEAT_AUDIO 	(1<<4)	/* 0: no Audio, 1: Analog Audio Codec */
#define FEAT_WLAN	(1<<5)	/* 0: no WLAN,  1: has WLAN */
#define FEAT_EXT_RTC	(1<<6)	/* 0: internal RTC, 1: external RTC */
#define FEAT_NAND	(1<<7)	/* 0: no NAND,  1: has NAND */
#define FEAT_EMMC	(1<<8)	/* 0: no EMMC,  1: has EMMC */
#define FEAT_SEC_CHIP	(1<<9)	/* 0: no SE050,  1: has SE050 */
#define FEAT_EEPROM	(1<<10)	/* 0: no EEPROM,  1: has EEPROM */
#define FEAT_ADC	(1<<11)	/* 0: no ADC,  1: has ADC */
#define FEAT_DISP_RGB	(1<<12)	/* 0: no RGB Display,  1: has RGB Display */
#define FEAT_SD_A	(1<<13)	/* 0: no SD_A,  1: has SD_A */
#define FEAT_SD_B	(1<<14)	/* 0: no SD_B,  1: has SD_B */


#define FEAT_ETH_MASK 	(FEAT_ETH_A | FEAT_ETH_B)

#define WDOG_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_ODE | PAD_CTL_PUE | PAD_CTL_PE)

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
	{	/* 0 (BT_PICOCOREMX8MP) */
		.name = "PicoCoreMX8MP",
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
	{	/* 1 (BT_PICOCOREMX8MPr2) */
		.name = "PicoCoreMX8MPr2",
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
	{	/* 2 (BT_ARMSTONEMX8MP) */
		.name = "armStoneMX8MP",
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
	{	/* 3 (BT_EFUSMX8MP) */
		.name = "efusMX8MP",
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

/* Parse the FDT of the BOARD-CFG in OCRAM and create binary info in OCRAM */
static void fs_setup_cfg_info(void)
{
	void *fdt;
	int offs;
	int i;
	struct cfg_info *info;
	const char *tmp;
	unsigned int features;

	/*
	 * If the BOARD-CFG cannot be found in OCRAM or it is corrupted, this
	 * is fatal. However no output is possible this early, so simply stop.
	 * If the BOARD-CFG is not at the expected location in OCRAM but is
	 * found somewhere else, output a warning later in board_late_init().
	 */
	if (!fs_image_find_cfg_in_ocram())
		hang();

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

	features = 0;
	if (fdt_getprop(fdt, offs, "have-nand", NULL))
		features |= FEAT_NAND;
	if (fdt_getprop(fdt, offs, "have-emmc", NULL))
		features |= FEAT_EMMC;
	if (fdt_getprop(fdt, offs, "have-audio", NULL))
		features |= FEAT_AUDIO;
	if (fdt_getprop(fdt, offs, "have-eth-phy", NULL)) {
		features |= FEAT_ETH_A;
		features |= FEAT_ETH_B;
	}
	if(fdt_getprop(fdt, offs, "have-eth-phy-a", NULL)){
		features |= FEAT_ETH_A;
	}
	if(fdt_getprop(fdt, offs, "have-eth-phy-b", NULL)){
		features |= FEAT_ETH_B;
	}
	if (fdt_getprop(fdt, offs, "have-wlan", NULL))
		features |= FEAT_WLAN;
	if (fdt_getprop(fdt, offs, "have-mipi-dsi", NULL))
		features |= FEAT_DISP_A;
	if (fdt_getprop(fdt, offs, "have-hdmi", NULL))
		features |= FEAT_DISP_B;
	if (fdt_getprop(fdt, offs, "have-ext-rtc", NULL))
		features |= FEAT_EXT_RTC;
	if (fdt_getprop(fdt, offs, "have-security", NULL))
		features |= FEAT_SEC_CHIP;
	if (fdt_getprop(fdt, offs, "have-eeprom", NULL))
		features |= FEAT_EEPROM;
	if (fdt_getprop(fdt, offs, "have-adc", NULL))
		features |= FEAT_ADC;
	if (fdt_getprop(fdt, offs, "have-mipi-to-rgb", NULL))
		features |= FEAT_DISP_RGB;
	if (fdt_getprop(fdt, offs, "have-sd-a", NULL))
		features |= FEAT_SD_A;
	if (fdt_getprop(fdt, offs, "have-sd-b", NULL))
		features |= FEAT_SD_B;
	info->features = features;
}

static iomux_v3_cfg_t const wdog_pads[] = {
	MX8MP_PAD_GPIO1_IO02__WDOG1_WDOG_B  | MUX_PAD_CTRL(WDOG_PAD_CTRL),
};

/* Do some very early board specific setup */
int board_early_init_f(void)
{
	struct wdog_regs *wdog = (struct wdog_regs *)WDOG1_BASE_ADDR;

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));

	set_wdog_reset(wdog);

	fs_setup_cfg_info();

	return 0;
}

/* Return the HW partition where U-Boot environment is on eMMC */
unsigned int mmc_get_env_part(struct mmc *mmc)
{
	unsigned int boot_part;

	boot_part = (mmc->part_config >> 3) & PART_ACCESS_MASK;
	if (boot_part == 7)
		boot_part = 0;

	return boot_part;
}

/* Return the appropriate environment depending on the fused boot device */
enum env_location env_get_location(enum env_operation op, int prio)
{
	enum env_location env_loc = ENVL_UNKNOWN;

	if (prio == 0) {
		switch (fs_board_get_boot_dev())
		{
		case FLEXSPI_BOOT:
			env_loc = ENVL_SPI_FLASH;
			break;
		case MMC1_BOOT:
		case MMC3_BOOT:
			env_loc = ENVL_MMC;
			break;
		default:
#if defined(CONFIG_ENV_IS_NOWHERE)
			env_loc = ENVL_NOWHERE;
#endif
			break;
		}
	}

	return env_loc;
}

#ifdef CONFIG_OF_BOARD_SETUP
#define FDT_LDB_LVDS0	"ldb/lvds-channel@0"
#define FDT_LDB_LVDS1	"ldb/lvds-channel@1"
#define FDT_CPU_TEMP_ALERT	"/thermal-zones/cpu-thermal/trips/trip0"
#define FDT_CPU_TEMP_CRIT	"/thermal-zones/cpu-thermal/trips/trip1"
#define FDT_SOC_TEMP_ALERT	"/thermal-zones/soc-thermal/trips/trip0"
#define FDT_SOC_TEMP_CRIT	"/thermal-zones/soc-thermal/trips/trip1"

/* Do all fixups that are done on both, U-Boot and Linux device tree */
static int do_fdt_board_setup_common(void *fdt)
{
	unsigned int features = fs_board_get_features();

	/* Disable eMMC if it is not available */
	if (!(features & FEAT_EMMC))
		fs_fdt_enable(fdt, "emmc", 0);

	/* Disable eqos node if it is not available */
	if (!(features & FEAT_ETH_A)) {
		fs_fdt_enable(fdt, "ethernet0", 0);
	}

	/* Disable fec node if it is not available */
	if (!(features & FEAT_ETH_B)) {
		fs_fdt_enable(fdt, "ethernet1", 0);
	}

	switch (fs_board_get_type()) {
		case BT_PICOCOREMX8MP:
		case BT_PICOCOREMX8MPr2:
		case BT_ARMSTONEMX8MP:
			break;
		case BT_EFUSMX8MP:
			/* Disable eeprom node if it is not available */
			if (!(features & FEAT_ADC)) {
				fs_fdt_enable(fdt, "adc", 0);
			}
			/* Disable eeprom node if it is not available */
			if (!(features & FEAT_EEPROM)) {
				fs_fdt_enable(fdt, "eeprom", 0);
			}
			/* Disable eeprom node if it is not available */
			if (!(features & FEAT_SD_B)) {
				fs_fdt_enable(fdt, "sd_b", 0);
			}
			/* Disable rgb-bridge node if it is not available */
			if (!(features & FEAT_DISP_RGB)) {
				fs_fdt_enable(fdt, "rgb_bridge", 0);
				/* disable mipi_dsi */
				fs_fdt_enable(fdt, "mipi_dsi", 0);
			}
			break;
		default:
			break;
		}

#if 0 // TODO:
	/* Disable security node if it is not available */
	if (!(features & FEAT_SEC_CHIP)) {
		fs_fdt_enable(fdt, "security", 0);
	}
#endif

	return 0;
}

/* Do any board-specific modifications on U-Boot device tree before starting */
int board_fix_fdt(void *fdt)
{
	unsigned int features = fs_board_get_features();

	/* Make some room in the FDT */
	fdt_shrink_to_minimum(fdt, 8192);

	/* Disable SPI NAND if it is not available
	 * U-Boot: specific alias name [spi0]
	 * */
	if (!(features & FEAT_NAND))
		fs_fdt_enable(fdt, "spi0", 0);

	/* Disable sd_a if WLAN is available
	 * U-Boot: support not available
	 * */
	if (features & FEAT_WLAN) {
		char* usdhc_name = "";
		switch (fs_board_get_type()) {
		case BT_PICOCOREMX8MP:
		case BT_PICOCOREMX8MPr2:
		case BT_ARMSTONEMX8MP:
			break;
		case BT_EFUSMX8MP:
			/* get sd_(x) interface name name for wlan */
			usdhc_name = "sd_a";
			break;
		default:
			break;
		}
		fs_fdt_enable(fdt, usdhc_name, 0);

	}

	return do_fdt_board_setup_common(fdt);
}

/* Do any additional board-specific modifications on Linux device tree */
int ft_board_setup(void *fdt, struct bd_info *bd)
{
	int offs;
	unsigned int features = fs_board_get_features();
	int minc, maxc;
	int id = 0;
	__maybe_unused uint32_t temp_range;

	/* get CPU temp grade from the fuses */
	temp_range = get_cpu_temp_grade(&minc, &maxc);

#if 0 /* TODO: need to rework */
	if (temp_range == TEMP_COMMERCIAL){
		/* no wlan abailable */
		fs_fdt_enable(fdt, "wlan-reset", 0);
		/* no eeprom available */
		fs_fdt_enable(fdt, "eeprom", 0);
		/* no MIPI_CSI2 available */
		fs_fdt_enable(fdt, "mipi_csi_1", 0);
		/* disable image sensing interface for MIPI_CSI2 */
		fs_fdt_enable(fdt, "isi_1", 0);
		/* no VPU */
		fs_fdt_enable(fdt, "vpu_g1", 0);
		fs_fdt_enable(fdt, "vpu_g2", 0);
		fs_fdt_enable(fdt, "vpu_vc8000e", 0);
		/* no ISP */
		fs_fdt_enable(fdt, "isp_0", 0);
		fs_fdt_enable(fdt, "isp_1", 0);
		/* no NPU */
		fs_fdt_enable(fdt, "vipsi", 0);
	}
#endif

#if 0 // TODO:
	/* Display A/B options */
	/* -------------------------------------
	 *                 |    0      |   1   |
	 * -------------------------------------
	 * DISP_A (DSI_A): | MIPI_DSI1 | LVDS0 |
	 * -------------------------------------
	 * DISP_B (DSI_B): |   HDMI    | LVDS1 |
	 * -------------------------------------
	 *
	 * in nboot configuration are display features inverted.
	 * */

	if (!(features & FEAT_DISP_A)) {
		/* disable mipi_dsi1 */
		fs_fdt_enable(fdt, "mipi_dsi", 0);

		/* enable LVDS0 */
		fs_fdt_enable(fdt, "lcdif2", 1);
		fs_fdt_enable(fdt, "ldb_phy", 1);
		fs_fdt_enable(fdt, "ldb", 1);
		/* TODO: use default LVDS1.
		 * 8 lanes mode is currently not working
		 * */
		//fs_fdt_enable(fdt, FDT_LDB_LVDS0, 0);
	}

	if (!(features & FEAT_DISP_B)) {
		/* disable HDMI */
		fs_fdt_enable(fdt, "irqsteer_hdmi", 0);
		fs_fdt_enable(fdt, "hdmimix_clk", 0);
		fs_fdt_enable(fdt, "hdmimix_reset", 0);
		fs_fdt_enable(fdt, "hdmi_pavi", 0);
		fs_fdt_enable(fdt, "hdmi", 0);
		fs_fdt_enable(fdt, "hdmiphy", 0);
		fs_fdt_enable(fdt, "lcdif3", 0);

		/* enable LVDS1 */
		fs_fdt_enable(fdt, "lcdif2", 1);
		fs_fdt_enable(fdt, "ldb_phy", 1);
		fs_fdt_enable(fdt, "ldb", 1);
		fs_fdt_enable(fdt, FDT_LDB_LVDS1, 1);

	}
#endif

	/* Disable SGTL5000 if it is not available */
	if (!(features & FEAT_AUDIO)) {
		/* disable all sgtl5000 regulators */
		fs_fdt_enable(fdt, "/regulators/reg_sgtl5000_vdda", 0);
		fs_fdt_enable(fdt, "/regulators/reg_sgtl5000_vddio", 0);
		fs_fdt_enable(fdt, "/regulators/reg_sgtl5000_vddd", 0);
		/* disable i2c sgtl5000 */
		fs_fdt_enable(fdt, "sgtl5000", 0);
		/* disable sgtl5000 platform driver */
		fs_fdt_enable(fdt, "sound-sgtl5000", 0);
	}

	/* The following stuff is only set in Linux device tree */
	/* Disable RTC85263 if it is not available */
	if (!(features & FEAT_EXT_RTC)) {
		fs_fdt_enable(fdt, "rtcpcf85263", 0);
		/* enable internal RTC */
		fs_fdt_enable(fdt, "snvs_rtc", 1);
	}

	/* Disable flexspi node if it is not available */
	if (!(features & FEAT_NAND)) {
		fs_fdt_enable(fdt, "flexspi", 0);
	}

	/* Disable wlan node if it is not available
	 * Default in u-boot is disabled.
	 * */
	if (!(features & FEAT_WLAN)) {
		int nodeoffset;
		char* usdhc_name = "";

		/* delete any existing wlan sub-node in sd_(x) interface */
		nodeoffset = fdt_path_offset(fdt, "wlan");
		fdt_del_node(fdt, nodeoffset);

		switch (fs_board_get_type()) {
			case BT_PICOCOREMX8MP:
			case BT_PICOCOREMX8MPr2:
			case BT_ARMSTONEMX8MP:
				break;
			case BT_EFUSMX8MP:
				/* get sd_(x) interface name name for wlan */
				usdhc_name = "sd_a";
				break;
			default:
				break;
			}

		if (!strcmp(usdhc_name, "sd_a") || !strcmp(usdhc_name, "sd_b") ||
				!strcmp(usdhc_name, "sd_c")) {
			nodeoffset = fdt_path_offset(fdt, usdhc_name);
			/* delete properties for wlan */
			fdt_delprop(fdt, nodeoffset, "mmc-pwrseq");
			fdt_delprop(fdt, nodeoffset, "non-removable");
			fdt_delprop(fdt, nodeoffset, "pm-ignore-notify");
			fdt_delprop(fdt, nodeoffset, "cap-power-off-card");
			fdt_delprop(fdt, nodeoffset, "keep-power-in-suspend");
		}
	}

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
	}

	/* Sanity check for get_cpu_temp_grade() */
	if ((minc > -500) && maxc < 500) {
		u32 tmp_val;

		tmp_val = (maxc - 10) * 1000;
		offs = fs_fdt_path_offset(fdt, FDT_CPU_TEMP_ALERT);
		fs_fdt_set_u32(fdt, offs, "temperature", tmp_val, 1);
		offs = fs_fdt_path_offset(fdt, FDT_SOC_TEMP_ALERT);
		fs_fdt_set_u32(fdt, offs, "temperature", tmp_val, 1);

		tmp_val = maxc * 1000;
		offs = fs_fdt_path_offset(fdt, FDT_CPU_TEMP_CRIT);
		fs_fdt_set_u32(fdt, offs, "temperature", tmp_val, 1);
		offs = fs_fdt_path_offset(fdt, FDT_SOC_TEMP_CRIT);
		fs_fdt_set_u32(fdt, offs, "temperature", tmp_val, 1);
	} else {
		printf("## Wrong cpu temp grade values read! Keeping defaults from device tree\n");
	}

	return do_fdt_board_setup_common(fdt);
}
#endif

void fs_ethaddr_init(void)
{
	unsigned int features2 = fs_board_get_features();
	int eth_id = 0;

	/* Set MAC addresses as environment variables */
	switch (fs_board_get_type())
	{
	case BT_PICOCOREMX8MP:
	case BT_PICOCOREMX8MPr2:
	case BT_ARMSTONEMX8MP:
	case BT_EFUSMX8MP:
		if (features2 & FEAT_ETH_A) {
			fs_eth_set_ethaddr(eth_id++);
		}
		if (features2 & FEAT_ETH_B) {
			fs_eth_set_ethaddr(eth_id++);
		}
		break;
	default:
		break;
	}
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

	printf ("%dx DRAM)\n", fs_board_get_cfg_info()->dram_chips);

	return 0;
}

#ifdef CONFIG_FEC_MXC
static int setup_fec(void)
{
	struct iomuxc_gpr_base_regs *gpr =
		(struct iomuxc_gpr_base_regs *)IOMUXC_GPR_BASE_ADDR;

	/* Enable RGMII TX clk output */
	setbits_le32(&gpr->gpr[1], BIT(22));

	return 0;
}
#endif

#ifdef CONFIG_DWC_ETH_QOS
static int setup_eqos(void)
{
	struct iomuxc_gpr_base_regs *gpr =
		(struct iomuxc_gpr_base_regs *)IOMUXC_GPR_BASE_ADDR;

	/* set INTF as RGMII, enable RGMII TXC clock */
	clrsetbits_le32(&gpr->gpr[1],
			IOMUXC_GPR_GPR1_GPR_ENET_QOS_INTF_SEL_MASK, BIT(16));
	setbits_le32(&gpr->gpr[1], BIT(19) | BIT(21));

	return set_clk_eqos(ENET_125MHZ);
}
#endif

#if defined(CONFIG_FEC_MXC) || defined(CONFIG_DWC_ETH_QOS)
int board_phy_config(struct phy_device *phydev)
{
	if (phydev->drv->config)
		phydev->drv->config(phydev);
	return 0;
}
#endif

#ifdef CONFIG_USB_TCPC
struct tcpc_port port1;

static int setup_pd_switch(uint8_t i2c_bus, uint8_t addr)
{
	struct udevice *bus;
	struct udevice *i2c_dev = NULL;
	int ret;
	uint8_t valb;

	ret = uclass_get_device_by_seq(UCLASS_I2C, i2c_bus, &bus);
	if (ret) {
		printf("%s: Can't find bus\n", __func__);
		return -EINVAL;
	}

	ret = dm_i2c_probe(bus, addr, 0, &i2c_dev);
	if (ret) {
		printf("%s: Can't find device id=0x%x\n",
			__func__, addr);
		return -ENODEV;
	}

	ret = dm_i2c_read(i2c_dev, 0xB, &valb, 1);
	if (ret) {
		printf("%s dm_i2c_read failed, err %d\n", __func__, ret);
		return -EIO;
	}
	valb |= 0x4; /* Set DB_EXIT to exit dead battery mode */
	ret = dm_i2c_write(i2c_dev, 0xB, (const uint8_t *)&valb, 1);
	if (ret) {
		printf("%s dm_i2c_write failed, err %d\n", __func__, ret);
		return -EIO;
	}

	/* Set OVP threshold to 23V */
	valb = 0x6;
	ret = dm_i2c_write(i2c_dev, 0x8, (const uint8_t *)&valb, 1);
	if (ret) {
		printf("%s dm_i2c_write failed, err %d\n", __func__, ret);
		return -EIO;
	}

	return 0;
}

int pd_switch_snk_enable(struct tcpc_port *port)
{
	if (port == &port1) {
		return setup_pd_switch(port->cfg.i2c_bus, port->cfg.addr);
	} else
		return -EINVAL;
}

struct tcpc_port_config port1_config = {
	.i2c_bus = 2, /*i2c3*/
	.addr = 0x52,
	.port_type = TYPEC_PORT_UFP,
	.max_snk_mv = 20000,
	.max_snk_ma = 3000,
	.max_snk_mw = 45000,
	.op_snk_mv = 15000,
	.switch_setup_func = &pd_switch_snk_enable,
	.disable_pd = true,
};

#define USB_TYPEC_SEL IMX_GPIO_NR(1, 9)
#define USB_TYPEC_EN IMX_GPIO_NR(1, 5)

static iomux_v3_cfg_t ss_mux_gpio[] = {
	MX8MP_PAD_GPIO1_IO09__GPIO1_IO09 | MUX_PAD_CTRL(NO_PAD_CTRL),
	MX8MP_PAD_GPIO1_IO05__GPIO1_IO05 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

void ss_mux_select(enum typec_cc_polarity pol)
{
	if (pol == TYPEC_POLARITY_CC1)
		gpio_direction_output(USB_TYPEC_SEL, 0);
	else
		gpio_direction_output(USB_TYPEC_SEL, 1);
}

static int setup_typec(void)
{
	int ret = 0;
	unsigned int board_type = fs_board_get_type();

	/* efusmx8mp does not support typec */
	if(board_type == BT_EFUSMX8MP) {
		port1.i2c_dev = NULL;
		return ret;
	}

	imx_iomux_v3_setup_multiple_pads(ss_mux_gpio, ARRAY_SIZE(ss_mux_gpio));
	gpio_request(USB_TYPEC_SEL, "typec_sel");
	gpio_request(USB_TYPEC_EN, "typec_en");
	gpio_direction_output(USB_TYPEC_EN, 0);

	switch(board_type) {
	default:
	case BT_PICOCOREMX8MP:
	case BT_PICOCOREMX8MPr2:
		port1_config.i2c_bus = 2; /* i2c3 */
		break;
	case BT_ARMSTONEMX8MP:
		port1_config.i2c_bus = 0; /* i2c1 */
		break;
	}

	ret = tcpc_init(&port1, port1_config, &ss_mux_select);
	if (ret) {
		debug("%s: tcpc port init failed, err=%d\n",
		       __func__, ret);
		port1.i2c_dev = NULL;
	}
	return ret;
}
#endif

#ifdef CONFIG_USB_DWC3

#define USB_PHY_CTRL0			0xF0040
#define USB_PHY_CTRL0_REF_SSP_EN	BIT(2)

#define USB_PHY_CTRL1			0xF0044
#define USB_PHY_CTRL1_RESET		BIT(0)
#define USB_PHY_CTRL1_COMMONONN		BIT(1)
#define USB_PHY_CTRL1_ATERESET		BIT(3)
#define USB_PHY_CTRL1_VDATSRCENB0	BIT(19)
#define USB_PHY_CTRL1_VDATDETENB0	BIT(20)

#define USB_PHY_CTRL2			0xF0048
#define USB_PHY_CTRL2_TXENABLEN0	BIT(8)

#define USB_PHY_CTRL6			0xF0058

#define HSIO_GPR_BASE                               (0x32F10000U)
#define HSIO_GPR_REG_0                              (HSIO_GPR_BASE)
#define HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN_SHIFT    (1)
#define HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN          (0x1U << HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN_SHIFT)


static struct dwc3_device dwc3_device_data = {
	.maximum_speed = USB_SPEED_SUPER,
	.base = USB2_BASE_ADDR,
	.dr_mode = USB_DR_MODE_PERIPHERAL,
	.index = 1,
	.power_down_scale = 2,
};

int usb_gadget_handle_interrupts(int index)
{
	dwc3_uboot_handle_interrupt(index);
	return 0;
}

static void dwc3_nxp_usb_phy_init(struct dwc3_device *dwc3)
{
	u32 RegData;

	/* enable usb clock via hsio gpr */
	RegData = readl(HSIO_GPR_REG_0);
	RegData |= HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN;
	writel(RegData, HSIO_GPR_REG_0);

	/* USB3.0 PHY signal fsel for 100M ref */
	RegData = readl(dwc3->base + USB_PHY_CTRL0);
	RegData = (RegData & 0xfffff81f) | (0x2a<<5);
	writel(RegData, dwc3->base + USB_PHY_CTRL0);

	RegData = readl(dwc3->base + USB_PHY_CTRL6);
	RegData &=~0x1;
	writel(RegData, dwc3->base + USB_PHY_CTRL6);

	RegData = readl(dwc3->base + USB_PHY_CTRL1);
	RegData &= ~(USB_PHY_CTRL1_VDATSRCENB0 | USB_PHY_CTRL1_VDATDETENB0 |
			USB_PHY_CTRL1_COMMONONN);
	RegData |= USB_PHY_CTRL1_RESET | USB_PHY_CTRL1_ATERESET;
	writel(RegData, dwc3->base + USB_PHY_CTRL1);

	RegData = readl(dwc3->base + USB_PHY_CTRL0);
	RegData |= USB_PHY_CTRL0_REF_SSP_EN;
	writel(RegData, dwc3->base + USB_PHY_CTRL0);

	RegData = readl(dwc3->base + USB_PHY_CTRL2);
	RegData |= USB_PHY_CTRL2_TXENABLEN0;
	writel(RegData, dwc3->base + USB_PHY_CTRL2);

	RegData = readl(dwc3->base + USB_PHY_CTRL1);
	RegData &= ~(USB_PHY_CTRL1_RESET | USB_PHY_CTRL1_ATERESET);
	writel(RegData, dwc3->base + USB_PHY_CTRL1);
}
#endif

#if defined(CONFIG_USB_DWC3) || defined(CONFIG_USB_XHCI_IMX8M)
#define USB1_PWR_EN IMX_GPIO_NR(1, 12)
#define USB1_RESET IMX_GPIO_NR(1, 6)
int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;
	unsigned int board_type = fs_board_get_type();

	debug("USB%d: %s init.\n", index, (init)?"otg":"host");

	if (index == 0 && init == USB_INIT_DEVICE)
		/* usb host only */
		return ret;

	imx8m_usb_power(index, true);

	if (index == 1 && init == USB_INIT_DEVICE) {
#ifdef CONFIG_USB_TCPC
		if(port1.i2c_dev)
			tcpc_setup_ufp_mode(&port1);
#endif
		dwc3_nxp_usb_phy_init(&dwc3_device_data);
		return dwc3_uboot_init(&dwc3_device_data);
	} else if (index == 1 && init == USB_INIT_HOST) {
#ifdef CONFIG_USB_TCPC
		if(port1.i2c_dev) {
			/*
			 * first check upstream facing port (ufp)
			 * for device
			 * */
			ret = tcpc_setup_ufp_mode(&port1);
			if(ret)
				/*
				 * second check downstream facing port (dfp)
				 * for usb host
				 * */
				ret = tcpc_setup_dfp_mode(&port1);
		}
#endif
		return ret;
	} else if (index == 0 && init == USB_INIT_HOST) {

		if(board_type == BT_ARMSTONEMX8MP) {
			/* Set reset pin to high */
			gpio_request(USB1_RESET, "usb1_reset");
			gpio_direction_output(USB1_RESET, 0);
			udelay(100);
			gpio_direction_output(USB1_RESET, 1);
		}
		/* Enable host power */
		gpio_request(USB1_PWR_EN, "usb1_pwr");
		gpio_direction_output(USB1_PWR_EN, 1);
	}

	return ret;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;

	if (index == 0 && init == USB_INIT_DEVICE)
		/* usb host only */
		return 0;

	debug("USB%d: %s cleanup.\n", index, (init)?"otg":"host");
	if (index == 1 && init == USB_INIT_DEVICE) {
		dwc3_uboot_exit(index);
	} else if (index == 1 && init == USB_INIT_HOST) {
#ifdef CONFIG_USB_TCPC
		if(port1.i2c_dev)
			ret = tcpc_disable_src_vbus(&port1);
#endif
	} else if (index == 0 && init == USB_INIT_HOST) {
		/* Disable host power */
		gpio_direction_output(USB1_PWR_EN, 0);
	}

	imx8m_usb_power(index, false);

	return ret;
}

#ifdef CONFIG_USB_TCPC
/* Not used so far */
int board_typec_get_mode(int index)
{
	int ret = 0;
	enum typec_cc_polarity pol;
	enum typec_cc_state state;

	if (index == 1) {
		tcpc_setup_ufp_mode(&port1);

		ret = tcpc_get_cc_status(&port1, &pol, &state);
		if (!ret) {
			if (state == TYPEC_STATE_SRC_RD_RA || state == TYPEC_STATE_SRC_RD)
				return USB_INIT_HOST;
		}

		return USB_INIT_DEVICE;
	} else {
		return USB_INIT_HOST;
	}
}
#endif
#endif

#ifdef CONFIG_DM_VIDEO
#define FSL_SIP_GPC			0xC2000000
#define FSL_SIP_CONFIG_GPC_PM_DOMAIN	0x3
#define DISPMIX				13
#define MIPI				15
#endif

int board_init(void)
{
	unsigned int board_type = fs_board_get_type();

	/* Copy NBoot args to variables and prepare command prompt string */
	fs_board_init_common(&board_info[board_type]);

#ifdef CONFIG_USB_TCPC
	setup_typec();
#endif

#ifdef CONFIG_FEC_MXC
	setup_fec();
#endif

#ifdef CONFIG_DWC_ETH_QOS
	/* clock, pin, gpr */
	setup_eqos();
#endif

#if defined(CONFIG_USB_DWC3) || defined(CONFIG_USB_XHCI_IMX8M)
	init_usb_clk();
#endif

#ifdef CONFIG_DM_VIDEO
	/* enable the dispmix & mipi phy power domain */
	call_imx_sip(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, DISPMIX, true, 0);
	call_imx_sip(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, MIPI, true, 0);
#endif

	return 0;
}

/*TODO: video support is not available for now.
 * first disable bl_on and vlcd_on
 * */
#define BL_ON_PAD IMX_GPIO_NR(5, 3)
static iomux_v3_cfg_t const bl_on_pads[] = {
	MX8MP_PAD_SPDIF_TX__GPIO5_IO03 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

#define VLCD_ON_PAD IMX_GPIO_NR(5, 2)
static iomux_v3_cfg_t const vlcd_on_pads[] = {
	MX8MP_PAD_SAI3_MCLK__GPIO5_IO02 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

int board_late_init(void)
{
	/* Set up all board specific variables */
	fs_board_late_init_common("ttymxc");

	/* Set mac addresses for corresponding boards */
	fs_ethaddr_init();

	/*TODO: video support is not available for now. */
	imx_iomux_v3_setup_multiple_pads (bl_on_pads, ARRAY_SIZE (bl_on_pads));
	/* backlight off */
	gpio_request (BL_ON_PAD, "BL_ON");
	gpio_direction_output (BL_ON_PAD, 0);

	imx_iomux_v3_setup_multiple_pads (vlcd_on_pads, ARRAY_SIZE (vlcd_on_pads));
	/* vlcd_on off */
	gpio_request (VLCD_ON_PAD, "VLCD_ON");
	gpio_direction_output (VLCD_ON_PAD, 0);

	return 0;
}

#ifdef CONFIG_IMX_BOOTAUX
ulong board_get_usable_ram_top(ulong total_size)
{
	/* Reserve 16M memory used by M core vring/buffer, which begins at 16MB before optee */
	if (rom_pointer[1])
		return gd->ram_top - SZ_16M;

	return gd->ram_top;
}
#endif

/* Set base address depends on board type.
 * Override function from serial_mxc.c
 * */
ulong board_serial_base(void)
{
	switch (fs_board_get_type())
	{
	case BT_EFUSMX8MP:
		return UART1_BASE;
	case BT_PICOCOREMX8MP:
	case BT_PICOCOREMX8MPr2:
	case BT_ARMSTONEMX8MP:
	default:
		break;
	}
	return UART2_BASE;
}

#ifdef CONFIG_FSL_FASTBOOT
#ifdef CONFIG_ANDROID_RECOVERY
int is_recovery_key_pressing(void)
{
	return 0; /*TODO*/
}
#endif /*CONFIG_ANDROID_RECOVERY*/
#endif /*CONFIG_FSL_FASTBOOT*/

#ifdef CONFIG_ANDROID_SUPPORT
bool is_power_key_pressed(void) {
	return (bool)(!!(readl(SNVS_HPSR) & (0x1 << 6)));
}
#endif

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
