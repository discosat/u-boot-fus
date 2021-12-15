// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 NXP
 */

#include <common.h>
#include <errno.h>
#include <linux/libfdt.h>
#include <fsl_esdhc.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/clock.h>
#include <asm/arch/sci/sci.h>
#include <asm/arch/imx8-pins.h>
#include <asm/arch/snvs_security_sc.h>
#include <asm/arch/iomux.h>
#include <asm/arch/sys_proto.h>
#include <imx8_hsio.h>
#include <usb.h>
#include <asm/mach-imx/video.h>
#include <power-domain.h>
#include <asm/arch/lpcg.h>
#include <bootm.h>

#include "../common/fs_fdt_common.h"	/* fs_fdt_set_val(), ... */
#include "../common/fs_board_common.h"	/* fs_board_*() */
//#include "../common/fs_eth_common.h"	/* fs_eth_*() */
#include "../common/fs_image_common.h"	/* fs_image_*() */

DECLARE_GLOBAL_DATA_PTR;

#define BT_EFUSMX8X 	0

/* Board features; these values can be resorted and redefined at will */
#define FEAT_ETH_A	(1<<0)
#define FEAT_ETH_B	(1<<1)
#define FEAT_ETH_A_PHY	(1<<2)
#define FEAT_ETH_B_PHY	(1<<3)
#define FEAT_NAND	(1<<4)
#define FEAT_EMMC	(1<<5)
#define FEAT_SGTL5000	(1<<6)
#define FEAT_WLAN	(1<<7)
#define FEAT_LVDS	(1<<8)
#define FEAT_MIPI_DSI	(1<<9)
#define FEAT_RTC85063	(1<<10)
#define FEAT_RTC85263	(1<<11)
#define FEAT_SEC_CHIP	(1<<12)
#define FEAT_CAN	(1<<13)
#define FEAT_EEPROM	(1<<14)

#define FEAT_ETH_MASK 	(FEAT_ETH_A | FEAT_ETH_B)

#define ENET_INPUT_PAD_CTRL	((SC_PAD_CONFIG_OD_IN << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_18V_10MA << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define ENET_NORMAL_PAD_CTRL	((SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_18V_10MA << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define GPIO_PAD_CTRL	((SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | \
			 (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
			 (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | \
			 (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define INSTALL_RAM "ram@83800000"
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
	{	/* 0 (BT_EFUSMX8X) */
		.name = "efusMX8X",
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
	{	/* (last) (unknown board) */
		.name = "unknown",
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

/* Parse the FDT of the BOARD-CFG in OCRAM and create binary info in OCRAM */
static void fs_spl_setup_cfg_info(void)
{
	void *fdt = fs_image_get_cfg_addr(false);
	int offs = fs_image_get_cfg_offs(fdt);
	int i;
	struct cfg_info *cfg = fs_board_get_cfg_info();
	const char *tmp;
	unsigned int features;

	memset(cfg, 0, sizeof(struct cfg_info));

	//###nbootargs.dwDbgSerPortPA = UART1_BASE_ADDR;

	tmp = fdt_getprop(fdt, offs, "board-name", NULL);
	for (i = 0; i < ARRAY_SIZE(board_info) - 1; i++) {
		if (!strcmp(tmp, board_info[i].name))
			break;
	}
	cfg->board_type = i;

	tmp = fdt_getprop(fdt, offs, "boot-dev", NULL);
	cfg->boot_dev = fs_board_get_boot_dev_from_name(tmp);

	cfg->board_rev = fdt_getprop_u32_default_node(fdt, offs, 0,
						      "board-rev", 100);
	cfg->dram_chips = fdt_getprop_u32_default_node(fdt, offs, 0,
						       "dram-chips", 1);
	cfg->dram_size = fdt_getprop_u32_default_node(fdt, offs, 0,
						      "dram-size", 0x400);

	features = 0;
	if (fdt_getprop(fdt, offs, "have-nand", NULL))
		features |= FEAT_NAND;
	if (fdt_getprop(fdt, offs, "have-emmc", NULL))
		features |= FEAT_EMMC;
	if (fdt_getprop(fdt, offs, "have-sgtl5000", NULL))
		features |= FEAT_SGTL5000;
	if (fdt_getprop(fdt, offs, "have-eth-phy", NULL)) {
		features |= FEAT_ETH_A;
		features |= FEAT_ETH_B;
	}
	if (fdt_getprop(fdt, offs, "have-wlan", NULL))
		features |= FEAT_WLAN;
	if (fdt_getprop(fdt, offs, "have-lvds", NULL))
		features |= FEAT_LVDS;
	if (fdt_getprop(fdt, offs, "have-mipi-dsi", NULL))
		features |= FEAT_MIPI_DSI;
	if (fdt_getprop(fdt, offs, "have-rtc-pcf85063", NULL))
		features |= FEAT_RTC85063;
	if (fdt_getprop(fdt, offs, "have-rtc-pcf85263", NULL))
		features |= FEAT_RTC85263;
	if (fdt_getprop(fdt, offs, "have-security", NULL))
		features |= FEAT_SEC_CHIP;
	if (fdt_getprop(fdt, offs, "have-can", NULL))
		features |= FEAT_CAN;
	if (fdt_getprop(fdt, offs, "have-eeprom", NULL))
		features |= FEAT_EEPROM;
	cfg->features = features;
}

int board_early_init_f(void)
{
	fs_spl_setup_cfg_info();

	return 0;
}

#if IS_ENABLED(CONFIG_DM_GPIO)
static void board_gpio_init(void)
{
	struct gpio_desc desc;
	int ret;

	ret = dm_gpio_lookup_name("gpio@1a_3", &desc);
	if (ret)
		return;

	ret = dm_gpio_request(&desc, "bb_per_rst_b");
	if (ret)
		return;

	dm_gpio_set_dir_flags(&desc, GPIOD_IS_OUT);
	dm_gpio_set_value(&desc, 0);
	udelay(50);
	dm_gpio_set_value(&desc, 1);
}
#else
static inline void board_gpio_init(void) {}
#endif

#if IS_ENABLED(CONFIG_FEC_MXC)
#include <miiphy.h>

#ifndef CONFIG_DM_ETH
static iomux_cfg_t pad_enet1[] = {
	SC_P_SPDIF0_TX | MUX_MODE_ALT(3) | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_SPDIF0_RX | MUX_MODE_ALT(3) | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ESAI0_TX3_RX2 | MUX_MODE_ALT(3) | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ESAI0_TX2_RX3 | MUX_MODE_ALT(3) | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ESAI0_TX1 | MUX_MODE_ALT(3) | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ESAI0_TX0 | MUX_MODE_ALT(3) | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ESAI0_SCKR | MUX_MODE_ALT(3) | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ESAI0_TX4_RX1 | MUX_MODE_ALT(3) | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ESAI0_TX5_RX0 | MUX_MODE_ALT(3) | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ESAI0_FST  | MUX_MODE_ALT(3) | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ESAI0_SCKT | MUX_MODE_ALT(3) | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ESAI0_FSR  | MUX_MODE_ALT(3) | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),

	/* Shared MDIO */
	SC_P_ENET0_MDC | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ENET0_MDIO | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
};

static iomux_cfg_t pad_enet0[] = {
	SC_P_ENET0_RGMII_RX_CTL | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ENET0_RGMII_RXD0 | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ENET0_RGMII_RXD1 | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ENET0_RGMII_RXD2 | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ENET0_RGMII_RXD3 | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ENET0_RGMII_RXC | MUX_PAD_CTRL(ENET_INPUT_PAD_CTRL),
	SC_P_ENET0_RGMII_TX_CTL | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ENET0_RGMII_TXD0 | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ENET0_RGMII_TXD1 | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ENET0_RGMII_TXD2 | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ENET0_RGMII_TXD3 | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ENET0_RGMII_TXC | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),

	/* Shared MDIO */
	SC_P_ENET0_MDC | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
	SC_P_ENET0_MDIO | MUX_PAD_CTRL(ENET_NORMAL_PAD_CTRL),
};

static void setup_iomux_fec(void)
{
	if (0 == CONFIG_FEC_ENET_DEV)
		imx8_iomux_setup_multiple_pads(pad_enet0, ARRAY_SIZE(pad_enet0));
	else
		imx8_iomux_setup_multiple_pads(pad_enet1, ARRAY_SIZE(pad_enet1));
}

static void enet_device_phy_reset(void)
{
	struct gpio_desc desc;
	int ret;

	/* The BB_PER_RST_B will reset the ENET1 PHY */
	if (0 == CONFIG_FEC_ENET_DEV) {
		ret = dm_gpio_lookup_name("gpio@1a_4", &desc);
		if (ret)
			return;

		ret = dm_gpio_request(&desc, "enet0_reset");
		if (ret)
			return;

		dm_gpio_set_dir_flags(&desc, GPIOD_IS_OUT);
		dm_gpio_set_value(&desc, 0);
		udelay(50);
		dm_gpio_set_value(&desc, 1);
	}

	/* The board has a long delay for this reset to become stable */
	mdelay(200);
}

int board_eth_init(bd_t *bis)
{
	int ret;
	struct power_domain pd;

	printf("[%s] %d\n", __func__, __LINE__);

	/* Reset ENET PHY */
	enet_device_phy_reset();

	if (CONFIG_FEC_ENET_DEV) {
		if (!power_domain_lookup_name("conn_enet1", &pd))
			power_domain_on(&pd);
	} else {
		if (!power_domain_lookup_name("conn_enet0", &pd))
			power_domain_on(&pd);
	}

	setup_iomux_fec();

	ret = fecmxc_initialize_multi(bis, CONFIG_FEC_ENET_DEV,
		CONFIG_FEC_MXC_PHYADDR, IMX_FEC_BASE);
	if (ret)
		printf("FEC1 MXC: %s:failed\n", __func__);

	return ret;
}
#endif

int board_phy_config(struct phy_device *phydev)
{
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x1f);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x8);

	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x00);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x82ee);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x05);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x100);

	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}
#endif

#define FDT_NAND        "nand"
#define FDT_EMMC        "emmc"
#define FDT_CMA         "/reserved-memory/linux,cma"
#define FDT_RTC85063    "rtcpcf85063"
#define FDT_RTC85263    "rtcpcf85263"
#define FDT_EEPROM      "eeprom"
#define FDT_CAN         "mcp2518fd"
#define FDT_SGTL5000    "sgtl5000"
#define FDT_I2C_SWITCH  "i2c4"
#define FDT_TEMP_ALERT   "/thermal-zones/cpu-thermal/trips/trip0"
#define FDT_TEMP_CRIT    "/thermal-zones/cpu-thermal/trips/trip1"
/* Do all fixups that are done on both, U-Boot and Linux device tree */
static int do_fdt_board_setup_common(void *fdt)
{
	unsigned int features = fs_board_get_features();

	/* Disable NAND if it is not available */
	if (!(features & FEAT_NAND))
		fs_fdt_enable(fdt, FDT_NAND, 0);

	/* Disable eMMC if it is not available */
	if (!(features & FEAT_EMMC))
		fs_fdt_enable(fdt, FDT_EMMC, 0);

	return 0;
}

/* Do any board-specific modifications on U-Boot device tree before starting */
int board_fix_fdt(void *fdt)
{
	/* Make some room in the FDT */
	fdt_shrink_to_minimum(fdt, 8192);

	return do_fdt_board_setup_common(fdt);
}

int checkboard(void)
{
#ifdef CONFIG_TARGET_IMX8DX_MEK
	puts("Board: iMX8DX MEK\n");
#else
	puts("Board: iMX8QXP MEK\n");
#endif

	build_info();
	print_bootinfo();

	return 0;
}

#ifdef CONFIG_FSL_HSIO

#define PCIE_PAD_CTRL	((SC_PAD_CONFIG_OD_IN << PADRING_CONFIG_SHIFT))
static iomux_cfg_t board_pcie_pins[] = {
	SC_P_PCIE_CTRL0_CLKREQ_B | MUX_MODE_ALT(0) | MUX_PAD_CTRL(PCIE_PAD_CTRL),
	SC_P_PCIE_CTRL0_WAKE_B | MUX_MODE_ALT(0) | MUX_PAD_CTRL(PCIE_PAD_CTRL),
	SC_P_PCIE_CTRL0_PERST_B | MUX_MODE_ALT(0) | MUX_PAD_CTRL(PCIE_PAD_CTRL),
};

static void imx8qxp_hsio_initialize(void)
{
	struct power_domain pd;
	int ret;

	if (!power_domain_lookup_name("hsio_pcie1", &pd)) {
		ret = power_domain_on(&pd);
		if (ret)
			printf("hsio_pcie1 Power up failed! (error = %d)\n", ret);
	}

	if (!power_domain_lookup_name("hsio_gpio", &pd)) {
		ret = power_domain_on(&pd);
		if (ret)
			 printf("hsio_gpio Power up failed! (error = %d)\n", ret);
	}

	lpcg_all_clock_on(HSIO_PCIE_X1_LPCG);
	lpcg_all_clock_on(HSIO_PHY_X1_LPCG);
	lpcg_all_clock_on(HSIO_PHY_X1_CRR1_LPCG);
	lpcg_all_clock_on(HSIO_PCIE_X1_CRR3_LPCG);
	lpcg_all_clock_on(HSIO_MISC_LPCG);
	lpcg_all_clock_on(HSIO_GPIO_LPCG);

	imx8_iomux_setup_multiple_pads(board_pcie_pins, ARRAY_SIZE(board_pcie_pins));
}

void pci_init_board(void)
{
	imx8qxp_hsio_initialize();

	/* test the 1 lane mode of the PCIe A controller */
	mx8qxp_pcie_init();
}

#endif

#ifdef CONFIG_USB

#if defined(CONFIG_USB_CDNS3_GADGET)

#ifdef CONFIG_USB_TCPC
#define USB_TYPEC_SEL IMX_GPIO_NR(5, 9)
static iomux_cfg_t ss_mux_gpio[] = {
	SC_P_ENET0_REFCLK_125M_25M | MUX_MODE_ALT(4) | MUX_PAD_CTRL(GPIO_PAD_CTRL),
};

struct tcpc_port port;
struct tcpc_port_config port_config = {
	.i2c_bus = 1,
	.addr = 0x50,
	.port_type = TYPEC_PORT_DFP,
};

void ss_mux_select(enum typec_cc_polarity pol)
{
	if (pol == TYPEC_POLARITY_CC1)
		gpio_direction_output(USB_TYPEC_SEL, 0);
	else
		gpio_direction_output(USB_TYPEC_SEL, 1);
}

static void setup_typec(void)
{
	int ret;
	struct gpio_desc typec_en_desc;

	imx8_iomux_setup_multiple_pads(ss_mux_gpio, ARRAY_SIZE(ss_mux_gpio));
	gpio_request(USB_TYPEC_SEL, "typec_sel");

	ret = dm_gpio_lookup_name("gpio@1a_7", &typec_en_desc);
	if (ret) {
		printf("%s lookup gpio@1a_7 failed ret = %d\n", __func__, ret);
		return;
	}

	ret = dm_gpio_request(&typec_en_desc, "typec_en");
	if (ret) {
		printf("%s request typec_en failed ret = %d\n", __func__, ret);
		return;
	}

	/* Enable SS MUX */
	dm_gpio_set_dir_flags(&typec_en_desc, GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);

	tcpc_init(&port, port_config, &ss_mux_select);
}
#endif

int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;

	if (index == 1) {
		if (init == USB_INIT_HOST) {
#ifdef CONFIG_USB_TCPC
			ret = tcpc_setup_dfp_mode(&port);
#endif
#ifdef CONFIG_USB_CDNS3_GADGET
		} else {
#ifdef CONFIG_USB_TCPC
			ret = tcpc_setup_ufp_mode(&port);
			printf("%d setufp mode %d\n", index, ret);
#endif
#endif
		}
	}

	return ret;

}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;

	if (index == 1) {
		if (init == USB_INIT_HOST) {
#ifdef CONFIG_USB_TCPC
			ret = tcpc_disable_src_vbus(&port);
#endif
		}
	}

	return ret;
}
#else

int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;

	if (index == 0) {
		if (init == USB_INIT_DEVICE) {
#if !CONFIG_IS_ENABLED(DM_USB_GADGET) && !CONFIG_IS_ENABLED(DM_USB)
			ret = sc_pm_set_resource_power_mode(-1, SC_R_USB_0, SC_PM_PW_MODE_ON);
			if (ret != SC_ERR_NONE)
				printf("conn_usb0 Power up failed! (error = %d)\n", ret);

			ret = sc_pm_set_resource_power_mode(-1, SC_R_USB_0_PHY, SC_PM_PW_MODE_ON);
			if (ret != SC_ERR_NONE)
				printf("conn_usb0_phy Power up failed! (error = %d)\n", ret);
#endif
		}
	}
	return ret;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;

	if (index == 0) {
		if (init == USB_INIT_DEVICE) {
#if !CONFIG_IS_ENABLED(DM_USB_GADGET) && !CONFIG_IS_ENABLED(DM_USB)
			ret = sc_pm_set_resource_power_mode(-1, SC_R_USB_0_PHY, SC_PM_PW_MODE_OFF);
			if (ret != SC_ERR_NONE)
				printf("conn_usb0_phy Power down failed! (error = %d)\n", ret);

			ret = sc_pm_set_resource_power_mode(-1, SC_R_USB_0, SC_PM_PW_MODE_OFF);
			if (ret != SC_ERR_NONE)
				printf("conn_usb0 Power down failed! (error = %d)\n", ret);
#endif
		}
	}
	return ret;
}
#endif
#endif

int board_init(void)
{
	board_gpio_init();

#if defined(CONFIG_USB) && defined(CONFIG_USB_TCPC)
	setup_typec();
#endif

#ifdef CONFIG_SNVS_SEC_SC_AUTO
	{
		int ret = snvs_security_sc_init();

		if (ret)
			return ret;
	}
#endif

	return 0;
}

void board_quiesce_devices(void)
{
	const char *power_on_devices[] = {
		"dma_lpuart2",
		"PD_UART2_RX",
		"PD_UART2_TX",

		/* HIFI DSP boot */
		"audio_sai0",
		"audio_ocram",
	};

	power_off_pd_devices(power_on_devices, ARRAY_SIZE(power_on_devices));
}

/*
 * Board specific reset that is system reset.
 */
void reset_cpu(ulong addr)
{
	sc_pm_reboot(-1, SC_PM_RESET_TYPE_COLD);
	while(1);

}

#ifdef CONFIG_OF_BOARD_SETUP
int ft_board_setup(void *blob, bd_t *bd)
{
	return 0;
}
#endif

static int check_mmc_autodetect(void)
{
	char *autodetect_str = env_get("mmcautodetect");

	if ((autodetect_str != NULL) &&
		(strcmp(autodetect_str, "yes") == 0)) {
		return 1;
	}

	return 0;
}

/* This should be defined for each board */
__weak int mmc_map_to_kernel_blk(int dev_no)
{
	return dev_no;
}

void board_late_mmc_env_init(void)
{
	char cmd[32];
	char mmcblk[32];
	u32 dev_no = mmc_get_env_dev();

	if (!check_mmc_autodetect())
		return;

	env_set_ulong("mmcdev", dev_no);

	/* Set mmcblk env */
	sprintf(mmcblk, "/dev/mmcblk%dp2 rootwait rw",
		mmc_map_to_kernel_blk(dev_no));
	env_set("mmcroot", mmcblk);

	sprintf(cmd, "mmc dev %d", dev_no);
	run_command(cmd, 0);
}

int board_late_init(void)
{
	char *fdt_file;
	bool m4_boot;

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	env_set("board_name", "MEK");
#ifdef CONFIG_TARGET_IMX8DX_MEK
	env_set("board_rev", "iMX8DX");
#else
	env_set("board_rev", "iMX8QXP");
#endif
#endif

	env_set("sec_boot", "no");
#ifdef CONFIG_AHAB_BOOT
	env_set("sec_boot", "yes");
#endif

	fdt_file = env_get("fdt_file");
	m4_boot = check_m4_parts_boot();

	if (fdt_file && !strcmp(fdt_file, "undefined")) {
		if (m4_boot)
			env_set("fdt_file", "efusmx8xrpmsg.dtb");
		else
			env_set("fdt_file", "efusmx8x.dtb");
	}

#ifdef CONFIG_ENV_IS_IN_MMC
	board_late_mmc_env_init();
#endif

	return 0;
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
	sc_bool_t status = SC_FALSE;

	sc_misc_get_button_status(-1, &status);
	return (bool)status;
}
#endif
