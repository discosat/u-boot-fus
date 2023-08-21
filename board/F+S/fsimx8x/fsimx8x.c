// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 NXP
 */

#include <common.h>
#include <errno.h>
#include <linux/libfdt.h>
#include <fsl_esdhc.h>
#include <asm/io.h>
#include <env_internal.h>
#include <asm/gpio.h>
#include <asm/arch/clock.h>
#include <asm/arch/sci/sci.h>
#include <asm/arch/imx8-pins.h>
#include <asm/arch/snvs_security_sc.h>
#include <asm/arch/iomux.h>
#include <asm/arch/sys_proto.h>
#include <usb.h>
#include <asm/mach-imx/video.h>
#include <hang.h>			/* hang() */
#include <power-domain.h>
#include <asm/arch/lpcg.h>
#include <bootm.h>
#include <phy.h>

#include "../common/fs_fdt_common.h"	/* fs_fdt_set_val(), ... */
#include "../common/fs_board_common.h"	/* fs_board_*() */
#include "../common/fs_eth_common.h"	/* fs_eth_*() */
#include "../common/fs_image_common.h"	/* fs_image_*() */
#include "../common/tcpc.h"

#include <power/regulator.h>

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
	u32 flags = 0;

	/*
	 * If the BOARD-CFG cannot be found in OCRAM or it is corrupted, this
	 * is fatal. However no output is possible this early, so simply stop.
	 * If the BOARD-CFG is not at the expected location in OCRAM but is
	 * found somewhere else, output a warning later in board_late_init().
	 */
	if (!fs_image_find_cfg_in_ocram())
		hang();

	/*
	 * The flag if running from Primary or Secondary SPL is misusing a
	 * byte in the BOARD-CFG in OCRAM, so we have to remove this before
	 * validating the BOARD-CFG.
	 */
	if (fs_image_is_secondary())
		flags |= CI_FLAGS_SECONDARY;

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
	info->flags = flags;

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
	info->features = features;
}

/* Do some very early board specific setup */
int board_early_init_f(void)
{
	fs_setup_cfg_info();

	return 0;
}

/* Return the appropriate environment depending on the fused boot device */
enum env_location env_get_location(enum env_operation op, int prio)
{
	if (prio == 0) {
		switch (fs_board_get_boot_dev()) {
		case FLEXSPI_BOOT:
			return ENVL_SPI_FLASH;
		case MMC1_BOOT:
			return ENVL_MMC;
		default:
			break;
		}
	}

	return ENVL_UNKNOWN;
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
	if (features & FEAT_NAND)
		puts("NAND, ");

	printf ("%dx DRAM)\n", fs_board_get_cfg_info()->dram_chips);
#if 0
#ifdef CONFIG_TARGET_IMX8DX_MEK
	puts("Board: iMX8DX MEK\n");
#else
	puts("Board: iMX8QXP MEK\n");
#endif

	build_info();
	print_bootinfo();
#endif

	return 0;
}

/* ---- Stage 'r': RAM valid, U-Boot relocated, variables can be used ------ */
#ifdef CONFIG_USB_TCPC
#define  USB_INIT_UNKNOWN (USB_INIT_DEVICE + 1)
static int setup_typec(void);
#endif
void fs_ethaddr_init(void);

int board_init(void)
{
	unsigned int board_type = fs_board_get_type();

	/* Prepare command prompt string */
	fs_board_init_common(&board_info[board_type]);

#ifdef CONFIG_USB_TCPC
	setup_typec();
#endif

#ifdef CONFIG_IMX_SNVS_SEC_SC_AUTO
	{
		int ret = snvs_security_sc_init();

		if (ret)
			return ret;
	}
#endif

/* TODO KM: Is this generally a better way to initialize all the fixed GPIOs? */

	/* Enable regulators defined in device tree */
	regulators_enable_boot_on(false);

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

#ifdef CONFIG_USB_TCPC
struct tcpc_port port;

struct tcpc_port_config port_config = {
	.i2c_bus = 1,
	.addr = 0x50,
	.port_type = TYPEC_PORT_DFP,
};

#define USB_TYPEC_SEL IMX_GPIO_NR(5, 9)
static iomux_cfg_t ss_mux_gpio[] = {
	SC_P_ENET0_REFCLK_125M_25M | MUX_MODE_ALT(4) | MUX_PAD_CTRL(GPIO_PAD_CTRL),
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

int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;

	if (index == 1) {
		if (init == USB_INIT_HOST) {
			ret = tcpc_setup_dfp_mode(&port);
		} else {
			ret = tcpc_setup_ufp_mode(&port);
			printf("%d setufp mode %d\n", index, ret);
		}
	}

	return ret;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;

	if (index == 1) {
		if (init == USB_INIT_HOST) {
			ret = tcpc_disable_src_vbus(&port);
		}
	}

	return ret;
}
#else
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
 *    efusMX8X            GPIO4_3 (GPIO4_IO03)(*)  USB_OTG1_ID
 *
 * (*) Signal on SKIT is active low, usually USB_OTG_PWR is active high
 *
 * USB1 is a host-only port (USB_H1). It is used on all boards. Some boards
 * may have an additional USB hub with a reset signal connected to this port.
 *
 *    Board               USB_H1_PWR               Hub Reset
 *    -------------------------------------------------------------------------
 *    efusMX8X            USB_A_PWR (HUB)          PMIC_POR
  *
 * (*) Signal on SKIT is active low, usually USB_HOST_PWR is active high
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
int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;
#if 0
	if (index == 0) {
		if (init == USB_INIT_DEVICE) {
			ret = sc_pm_set_resource_power_mode(-1, SC_R_USB_0, SC_PM_PW_MODE_ON);
			if (ret != SC_ERR_NONE)
				printf("conn_usb0 Power up failed! (error = %d)\n", ret);

			ret = sc_pm_set_resource_power_mode(-1, SC_R_USB_0_PHY, SC_PM_PW_MODE_ON);
			if (ret != SC_ERR_NONE)
				printf("conn_usb0_phy Power up failed! (error = %d)\n", ret);
		}
	}

	if (index == 1) {
		if (init == USB_INIT_HOST) {
			ret = sc_pm_set_resource_power_mode(-1, SC_R_USB_2, SC_PM_PW_MODE_ON);
			if (ret != SC_ERR_NONE)
				printf("conn_usb2 Power up failed! (error = %d)\n", ret);

			ret = sc_pm_set_resource_power_mode(-1, SC_R_USB_2_PHY, SC_PM_PW_MODE_ON);
			if (ret != SC_ERR_NONE)
				printf("conn_usb2_phy Power up failed! (error = %d)\n", ret);
		}
	}
#endif
	return ret;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;
#if 0
	if (index == 0) {
		if (init == USB_INIT_DEVICE) {
			ret = sc_pm_set_resource_power_mode(-1, SC_R_USB_0_PHY, SC_PM_PW_MODE_OFF);
			if (ret != SC_ERR_NONE)
				printf("conn_usb0_phy Power down failed! (error = %d)\n", ret);

			ret = sc_pm_set_resource_power_mode(-1, SC_R_USB_0, SC_PM_PW_MODE_OFF);
			if (ret != SC_ERR_NONE)
				printf("conn_usb0 Power down failed! (error = %d)\n", ret);
		}
	}

	if (index == 1) {
		if (init == USB_INIT_HOST) {
			ret = sc_pm_set_resource_power_mode(-1, SC_R_USB_2_PHY, SC_PM_PW_MODE_OFF);
			if (ret != SC_ERR_NONE)
				printf("conn_usb2_phy Power down failed! (error = %d)\n", ret);

			ret = sc_pm_set_resource_power_mode(-1, SC_R_USB_2, SC_PM_PW_MODE_OFF);
			if (ret != SC_ERR_NONE)
				printf("conn_usb2 Power down failed! (error = %d)\n", ret);
		}
	}
#endif
	return ret;
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
	/* Remove 'fdtcontroladdr' env. because we are using
	 * compiled-in version. In this case it is not possible
	 * to use this env. as saved in NAND flash. (s. readme for fdt control)
	 */
	env_set("fdtcontroladdr", "");

	env_set("sec_boot", "no");
#ifdef CONFIG_AHAB_BOOT
	env_set("sec_boot", "yes");
#endif

	/* Set up all board specific variables */
	fs_board_late_init_common("ttyLP");

#if 1
#ifdef CONFIG_ENV_IS_IN_MMC
	board_late_mmc_env_init();
#endif
#endif

	/* Set mac addresses for corresponding boards */
	fs_ethaddr_init();
	return 0;
}
#endif /* CONFIG_BOARD_LATE_INIT */

#ifdef CONFIG_FEC_MXC

void fs_ethaddr_init(void)
{
	unsigned int features = fs_board_get_features();
	int eth_id = 0;

	if (features & FEAT_ETH_A)
		fs_eth_set_ethaddr(eth_id++);
	if (features & FEAT_ETH_B)
		fs_eth_set_ethaddr(eth_id++);
	/* All fsimx8mn boards have a WLAN module
	 * which have an integrated mac address. So we don´t
	 * have to set an own mac address for the module.
	 */
//	if (features & FEAT_WLAN)
//		fs_eth_set_ethaddr(eth_id++);
}

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
#endif /* CONFIG_FEC_MXC */

#define FDT_NAND        "nand"
#define FDT_EMMC        "emmc"
#define FDT_CMA         "/reserved-memory/linux,cma"
#define FDT_RTC85063    "rtcpcf85063"
#define FDT_RTC85263    "rtcpcf85263"
#define FDT_EEPROM      "eeprom"
#define FDT_SGTL5000    "sgtl5000"
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

/* Do any additional board-specific modifications on Linux device tree */
int ft_board_setup(void *fdt, struct bd_info *bd)
{
	int offs;
	unsigned int features = fs_board_get_features();

	int id = 0;

	/* The following stuff is only set in Linux device tree */
	/* Disable RTC85063 if it is not available */
	if (!(features & FEAT_RTC85063))
		fs_fdt_enable(fdt, FDT_RTC85063, 0);

	/* Disable RTC85263 if it is not available */
	if (!(features & FEAT_RTC85263))
		fs_fdt_enable(fdt, FDT_RTC85263, 0);

	/* Disable EEPROM if it is not available */
	if (!(features & FEAT_EEPROM))
		fs_fdt_enable(fdt, FDT_EEPROM, 0);

	/* Disable SGTL5000 if it is not available */
	if (!(features & FEAT_SGTL5000))
		fs_fdt_enable(fdt, FDT_SGTL5000, 0);

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
		/* All fsimx8x boards have a WLAN module
		 * which have an integrated mac address. So we don´t
		 * have to set an own mac address for the module.
		 */
//		if (features & FEAT_WLAN)
//			fs_fdt_set_macaddr(fdt, offs, id++);
	}

	/* Set linux,cma size depending on RAM size. Default is 320MB. */
	offs = fs_fdt_path_offset(fdt, FDT_CMA);
	if (fdt_get_property(fdt, offs, "no-uboot-override", NULL) == NULL) {
		unsigned int dram_size = fs_board_get_cfg_info()->dram_size;
		if ((dram_size == 1023) || (dram_size == 1024)) {
			fdt32_t tmp[2];
			tmp[0] = cpu_to_fdt32(0x0);
			tmp[1] = cpu_to_fdt32(0x28000000);
			fs_fdt_set_val(fdt, offs, "size", tmp, sizeof(tmp), 1);
		}
	}

	return do_fdt_board_setup_common(fdt);
}

int mmc_map_to_kernel_blk(int devno)
{
	return devno + 1;
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

	imx8_power_off_pd_devices(power_on_devices, ARRAY_SIZE(power_on_devices));
}

/*
 * Board specific reset that is system reset.
 */
void reset_cpu(ulong addr)
{
	sc_pm_reboot(-1, SC_PM_RESET_TYPE_COLD);
	while(1);
}

static int check_mmc_autodetect(void)
{
	char *autodetect_str = env_get("mmcautodetect");

	if ((autodetect_str != NULL) &&
		(strcmp(autodetect_str, "yes") == 0)) {
		return 1;
	}

	return 0;
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

#ifdef CONFIG_BOARD_POSTCLK_INIT
int board_postclk_init(void)
{
	/* TODO */
	return 0;
}
#endif /* CONFIG_BOARD_POSTCLK_INIT */
