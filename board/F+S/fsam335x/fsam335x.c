/*
 * fsam335x.c
 *
 * (C) Copyright 2015
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Board specific functions for F&S boards based on TI Sitara AM335x CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * ### TODO ###
 * - Check if NAND write_page() function actually returns the bitflip count
 * - Use a function board_nand_init() and CONFIG_SYS_NAND_SELF_INIT to
 *   register the NAND devices. Use the skip region to handle/avoid accesses
 *   to NBoot region.
 * - Implement F&S NAND ECC strategy with Block Refresh.
 * - Check USB host, it is really slow (670KB/s, should be up to 20MB/s).
 * - Check OPP settings. Are we really using full throttle?
 */

#include <common.h>
#include <errno.h>
#include <serial.h>			/* struct serial_device */

#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/setup.h>			/* struct tag_fshwconfig, ... */
#include <asm/arch/omap.h>
#include <asm/arch/cpu.h>
#include <asm/arch/hardware.h>		/* CTRL_DEVICE_BASE, ... */
//#include <asm/arch/ddr_defs.h>
#include <asm/arch/clock.h>		/* MODULE_CLKCTRL_* */
#include <asm/arch/mux.h>
#include <asm/arch/gpio.h>
//#include <asm/arch/mmc_host_def.h>
#include <asm/arch/sys_proto.h>		/* gpmc_init() */
//#include <asm/arch/mem.h>
//#include <asm/emif.h>
//#include <i2c.h>
#include <miiphy.h>			/* PHY_INTERFACE_MODE_MII */
#include <cpsw.h>
//#include <power/tps65217.h>
//#include <power/tps65910.h>
#include <linux/mtd/nand.h>		/* struct mtd_info, struct nand_chip */


/* ------------------------------------------------------------------------- */

#define NBOOT_ARGS_BASE (PHYS_SDRAM_0 + 0x00001000) /* Arguments from NBoot */
#define BOOT_PARAMS_BASE (PHYS_SDRAM_0 + 0x100)	    /* Arguments to Linux */

#define BT_PICOCOMA8 0

/* Features set in tag_fshwconfig.chFeature1 */
#define FEAT1_2NDCAN  (1<<1)		/* 0: 1x CAN, 1: 2x CAN */

/* Features set in tag_fshwconfig.chFeature2 */
#define FEAT2_L2      (1<<1)		/* CPU has Level 2 cache */

#define ACTION_RECOVER 0x00000040	/* Start recovery instead of update */

#define CLKCTRL_FULLY_FUNCTIONAL \
	(MODULE_CLKCTRL_IDLEST_FULLY_FUNCTIONAL << MODULE_CLKCTRL_IDLEST_SHIFT)
#define CLKCTRL_ENABLE \
	(MODULE_CLKCTRL_MODULEMODE_SW_EXPLICIT_EN << MODULE_CLKCTRL_MODULEMODE_SHIFT)
#define CM_PER_CPGMAC0_CLKCTRL (CM_PER + 0x14)
#define CM_PER_USB0_CLKCTRL (CM_PER + 0x1c)
#define CM_PER_GPIO3_CLKCTRL (CM_PER + 0x84)
#define CM_PER_CPSW_CLKSTCTRL (CM_PER + 0x144)
#define CM_CLKDCOLDO_DPLL_PER (CM_WKUP + 0x7c)

#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)

struct board_info {
	char *name;			  /* Device name */
	unsigned int mach_type;		  /* Device machine ID */
	char *bootdelay;		  /* Default value for bootdelay */
	char *updatecheck;		  /* Default value for updatecheck */
	char *installcheck;		  /* Default value for installcheck */
	char *recovercheck;		  /* Default value for recovercheck */
	char *console;			  /* Default variable for console */
	char *login;			  /* Default variable for login */
	char *mtdparts;			  /* Default variable for mtdparts */
	char *network;			  /* Default variable for network */
	char *init;			  /* Default variable for init */
	char *rootfs;			  /* Default variable for rootfs */
	char *kernel;			  /* Default variable for kernel */
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

const struct board_info fs_board_info[4] = {
	{	/* 0 (BT_PICOCOMA8) */
		"PicoCOMA8",
		MACH_TYPE_AM335XEVM,	/* ### TODO: Register own ID */
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
	},
	{	/* 1 */
		.name = "Unknown",
		.mach_type = 0,		/*####not yet registered*/
	},
	{	/* 2 */
		.name = "Unknown",
		.mach_type = 0,		/*####not yet registered*/
	},
	{	/* 3 */
		.name = "Unknown",
		.mach_type = 0,		/*####not yet registered*/
	},
};

/* String used for system prompt */
char fs_sys_prompt[20];

/* Copy of the NBoot args */
struct tag_fshwconfig fs_nboot_args;

static struct ctrl_dev *cdev = (struct ctrl_dev *)CTRL_DEVICE_BASE;


//---------------- TI-Code - Anfang -------------------------------

#if 0

DECLARE_GLOBAL_DATA_PTR;

/* GPIO that controls power to DDR on EVM-SK */
#define GPIO_DDR_VTT_EN		7


/*
 * Read header information from EEPROM into global structure.
 */
static int read_eeprom(struct am335x_baseboard_id *header)
{
	/* Check if baseboard eeprom is available */
	if (i2c_probe(CONFIG_SYS_I2C_EEPROM_ADDR)) {
		puts("Could not probe the EEPROM; something fundamentally "
			"wrong on the I2C bus.\n");
		return -ENODEV;
	}

	/* read the eeprom using i2c */
	if (i2c_read(CONFIG_SYS_I2C_EEPROM_ADDR, 0, 2, (uchar *)header,
		     sizeof(struct am335x_baseboard_id))) {
		puts("Could not read the EEPROM; something fundamentally"
			" wrong on the I2C bus.\n");
		return -EIO;
	}

	if (header->magic != 0xEE3355AA) {
		/*
		 * read the eeprom using i2c again,
		 * but use only a 1 byte address
		 */
		if (i2c_read(CONFIG_SYS_I2C_EEPROM_ADDR, 0, 1, (uchar *)header,
			     sizeof(struct am335x_baseboard_id))) {
			puts("Could not read the EEPROM; something "
				"fundamentally wrong on the I2C bus.\n");
			return -EIO;
		}

		if (header->magic != 0xEE3355AA) {
			printf("Incorrect magic number (0x%x) in EEPROM\n",
					header->magic);
			return -EINVAL;
		}
	}

	return 0;
}

#define OSC	(V_OSCK/1000000)
const struct dpll_params dpll_ddr = {
		266, OSC-1, 1, -1, -1, -1, -1};
const struct dpll_params dpll_ddr_evm_sk = {
		303, OSC-1, 1, -1, -1, -1, -1};
const struct dpll_params dpll_ddr_bone_black = {
		400, OSC-1, 1, -1, -1, -1, -1};

void am33xx_spl_board_init(void)
{
	struct am335x_baseboard_id header;
	int mpu_vdd;

	if (read_eeprom(&header) < 0)
		puts("Could not get board ID.\n");

	/* Get the frequency */
	dpll_mpu_opp100.m = am335x_get_efuse_mpu_max_freq(cdev);

#if 0 //####
	if (board_is_bone(&header) || board_is_bone_lt(&header)) {
		/* BeagleBone PMIC Code */
		int usb_cur_lim;

		/*
		 * Only perform PMIC configurations if board rev > A1
		 * on Beaglebone White
		 */
		if (board_is_bone(&header) && !strncmp(header.version,
						       "00A1", 4))
			return;

		if (i2c_probe(TPS65217_CHIP_PM))
			return;

		/*
		 * On Beaglebone White we need to ensure we have AC power
		 * before increasing the frequency.
		 */
		if (board_is_bone(&header)) {
			uchar pmic_status_reg;
			if (tps65217_reg_read(TPS65217_STATUS,
					      &pmic_status_reg))
				return;
			if (!(pmic_status_reg & TPS65217_PWR_SRC_AC_BITMASK)) {
				puts("No AC power, disabling frequency switch\n");
				return;
			}
		}

		/*
		 * Override what we have detected since we know if we have
		 * a Beaglebone Black it supports 1GHz.
		 */
		if (board_is_bone_lt(&header))
			dpll_mpu_opp100.m = MPUPLL_M_1000;

		/*
		 * Increase USB current limit to 1300mA or 1800mA and set
		 * the MPU voltage controller as needed.
		 */
		if (dpll_mpu_opp100.m == MPUPLL_M_1000) {
			usb_cur_lim = TPS65217_USB_INPUT_CUR_LIMIT_1800MA;
			mpu_vdd = TPS65217_DCDC_VOLT_SEL_1325MV;
		} else {
			usb_cur_lim = TPS65217_USB_INPUT_CUR_LIMIT_1300MA;
			mpu_vdd = TPS65217_DCDC_VOLT_SEL_1275MV;
		}

		if (tps65217_reg_write(TPS65217_PROT_LEVEL_NONE,
				       TPS65217_POWER_PATH,
				       usb_cur_lim,
				       TPS65217_USB_INPUT_CUR_LIMIT_MASK))
			puts("tps65217_reg_write failure\n");

		/* Set DCDC3 (CORE) voltage to 1.125V */
		if (tps65217_voltage_update(TPS65217_DEFDCDC3,
					    TPS65217_DCDC_VOLT_SEL_1125MV)) {
			puts("tps65217_voltage_update failure\n");
			return;
		}

		/* Set CORE Frequencies to OPP100 */
		do_setup_dpll(&dpll_core_regs, &dpll_core_opp100);

		/* Set DCDC2 (MPU) voltage */
		if (tps65217_voltage_update(TPS65217_DEFDCDC2, mpu_vdd)) {
			puts("tps65217_voltage_update failure\n");
			return;
		}

		/*
		 * Set LDO3, LDO4 output voltage to 3.3V for Beaglebone.
		 * Set LDO3 to 1.8V and LDO4 to 3.3V for Beaglebone Black.
		 */
		if (board_is_bone(&header)) {
			if (tps65217_reg_write(TPS65217_PROT_LEVEL_2,
					       TPS65217_DEFLS1,
					       TPS65217_LDO_VOLTAGE_OUT_3_3,
					       TPS65217_LDO_MASK))
				puts("tps65217_reg_write failure\n");
		} else {
			if (tps65217_reg_write(TPS65217_PROT_LEVEL_2,
					       TPS65217_DEFLS1,
					       TPS65217_LDO_VOLTAGE_OUT_1_8,
					       TPS65217_LDO_MASK))
				puts("tps65217_reg_write failure\n");
		}

		if (tps65217_reg_write(TPS65217_PROT_LEVEL_2,
				       TPS65217_DEFLS2,
				       TPS65217_LDO_VOLTAGE_OUT_3_3,
				       TPS65217_LDO_MASK))
			puts("tps65217_reg_write failure\n");
	} else {
		int sil_rev;

		/*
		 * The GP EVM, IDK and EVM SK use a TPS65910 PMIC.  For all
		 * MPU frequencies we support we use a CORE voltage of
		 * 1.1375V.  For MPU voltage we need to switch based on
		 * the frequency we are running at.
		 */
		if (i2c_probe(TPS65910_CTRL_I2C_ADDR))
			return;

		/*
		 * Depending on MPU clock and PG we will need a different
		 * VDD to drive at that speed.
		 */
		sil_rev = readl(&cdev->deviceid) >> 28;
		mpu_vdd = am335x_get_tps65910_mpu_vdd(sil_rev,
						      dpll_mpu_opp100.m);

		/* Tell the TPS65910 to use i2c */
		tps65910_set_i2c_control();

		/* First update MPU voltage. */
		if (tps65910_voltage_update(MPU, mpu_vdd))
			return;

		/* Second, update the CORE voltage. */
		if (tps65910_voltage_update(CORE, TPS65910_OP_REG_SEL_1_1_3))
			return;

		/* Set CORE Frequencies to OPP100 */
		do_setup_dpll(&dpll_core_regs, &dpll_core_opp100);
	}

	/* Set MPU Frequency to what we detected now that voltages are set */
	do_setup_dpll(&dpll_mpu_regs, &dpll_mpu_opp100);
#endif
}


static struct module_pin_mux uart0_pin_mux[] = {
	{OFFSET(uart0_rxd), (MODE(0) | PULLUP_EN | RXACTIVE)},	/* UART0_RXD */
	{OFFSET(uart0_txd), (MODE(0) | PULLUDEN)},		/* UART0_TXD */
	{-1},
};

static struct module_pin_mux uart1_pin_mux[] = {
	{OFFSET(uart1_rxd), (MODE(0) | PULLUP_EN | RXACTIVE)},	/* UART1_RXD */
	{OFFSET(uart1_txd), (MODE(0) | PULLUDEN)},		/* UART1_TXD */
	{-1},
};

static struct module_pin_mux uart2_pin_mux[] = {
	{OFFSET(spi0_sclk), (MODE(1) | PULLUP_EN | RXACTIVE)},	/* UART2_RXD */
	{OFFSET(spi0_d0), (MODE(1) | PULLUDEN)},		/* UART2_TXD */
	{-1},
};

static struct module_pin_mux uart3_pin_mux[] = {
	{OFFSET(spi0_cs1), (MODE(1) | PULLUP_EN | RXACTIVE)},	/* UART3_RXD */
	{OFFSET(ecap0_in_pwm0_out), (MODE(1) | PULLUDEN)},	/* UART3_TXD */
	{-1},
};

static struct module_pin_mux uart4_pin_mux[] = {
	{OFFSET(gpmc_wait0), (MODE(6) | PULLUP_EN | RXACTIVE)},	/* UART4_RXD */
	{OFFSET(gpmc_wpn), (MODE(6) | PULLUDEN)},		/* UART4_TXD */
	{-1},
};

static struct module_pin_mux uart5_pin_mux[] = {
	{OFFSET(lcd_data9), (MODE(4) | PULLUP_EN | RXACTIVE)},	/* UART5_RXD */
	{OFFSET(lcd_data8), (MODE(4) | PULLUDEN)},		/* UART5_TXD */
	{-1},
};

static struct module_pin_mux mmc0_pin_mux[] = {
	{OFFSET(mmc0_dat3), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_DAT3 */
	{OFFSET(mmc0_dat2), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_DAT2 */
	{OFFSET(mmc0_dat1), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_DAT1 */
	{OFFSET(mmc0_dat0), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_DAT0 */
	{OFFSET(mmc0_clk), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_CLK */
	{OFFSET(mmc0_cmd), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_CMD */
	{OFFSET(mcasp0_aclkr), (MODE(4) | RXACTIVE)},		/* MMC0_WP */
	{OFFSET(spi0_cs1), (MODE(5) | RXACTIVE | PULLUP_EN)},	/* MMC0_CD */
	{-1},
};

static struct module_pin_mux mmc0_no_cd_pin_mux[] = {
	{OFFSET(mmc0_dat3), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_DAT3 */
	{OFFSET(mmc0_dat2), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_DAT2 */
	{OFFSET(mmc0_dat1), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_DAT1 */
	{OFFSET(mmc0_dat0), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_DAT0 */
	{OFFSET(mmc0_clk), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_CLK */
	{OFFSET(mmc0_cmd), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_CMD */
	{OFFSET(mcasp0_aclkr), (MODE(4) | RXACTIVE)},		/* MMC0_WP */
	{-1},
};

static struct module_pin_mux mmc0_pin_mux_sk_evm[] = {
	{OFFSET(mmc0_dat3), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_DAT3 */
	{OFFSET(mmc0_dat2), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_DAT2 */
	{OFFSET(mmc0_dat1), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_DAT1 */
	{OFFSET(mmc0_dat0), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_DAT0 */
	{OFFSET(mmc0_clk), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_CLK */
	{OFFSET(mmc0_cmd), (MODE(0) | RXACTIVE | PULLUP_EN)},	/* MMC0_CMD */
	{OFFSET(spi0_cs1), (MODE(5) | RXACTIVE | PULLUP_EN)},	/* MMC0_CD */
	{-1},
};

static struct module_pin_mux mmc1_pin_mux[] = {
	{OFFSET(gpmc_ad3), (MODE(1) | RXACTIVE | PULLUP_EN)},	/* MMC1_DAT3 */
	{OFFSET(gpmc_ad2), (MODE(1) | RXACTIVE | PULLUP_EN)},	/* MMC1_DAT2 */
	{OFFSET(gpmc_ad1), (MODE(1) | RXACTIVE | PULLUP_EN)},	/* MMC1_DAT1 */
	{OFFSET(gpmc_ad0), (MODE(1) | RXACTIVE | PULLUP_EN)},	/* MMC1_DAT0 */
	{OFFSET(gpmc_csn1), (MODE(2) | RXACTIVE | PULLUP_EN)},	/* MMC1_CLK */
	{OFFSET(gpmc_csn2), (MODE(2) | RXACTIVE | PULLUP_EN)},	/* MMC1_CMD */
	{OFFSET(gpmc_csn0), (MODE(7) | RXACTIVE | PULLUP_EN)},	/* MMC1_WP */
	{OFFSET(gpmc_advn_ale), (MODE(7) | RXACTIVE | PULLUP_EN)},	/* MMC1_CD */
	{-1},
};

static struct module_pin_mux gpio0_7_pin_mux[] = {
	{OFFSET(ecap0_in_pwm0_out), (MODE(7) | PULLUDEN)},	/* GPIO0_7 */
	{-1},
};

static struct module_pin_mux rgmii1_pin_mux[] = {
	{OFFSET(mii1_txen), MODE(2)},			/* RGMII1_TCTL */
	{OFFSET(mii1_rxdv), MODE(2) | RXACTIVE},	/* RGMII1_RCTL */
	{OFFSET(mii1_txd3), MODE(2)},			/* RGMII1_TD3 */
	{OFFSET(mii1_txd2), MODE(2)},			/* RGMII1_TD2 */
	{OFFSET(mii1_txd1), MODE(2)},			/* RGMII1_TD1 */
	{OFFSET(mii1_txd0), MODE(2)},			/* RGMII1_TD0 */
	{OFFSET(mii1_txclk), MODE(2)},			/* RGMII1_TCLK */
	{OFFSET(mii1_rxclk), MODE(2) | RXACTIVE},	/* RGMII1_RCLK */
	{OFFSET(mii1_rxd3), MODE(2) | RXACTIVE},	/* RGMII1_RD3 */
	{OFFSET(mii1_rxd2), MODE(2) | RXACTIVE},	/* RGMII1_RD2 */
	{OFFSET(mii1_rxd1), MODE(2) | RXACTIVE},	/* RGMII1_RD1 */
	{OFFSET(mii1_rxd0), MODE(2) | RXACTIVE},	/* RGMII1_RD0 */
	{OFFSET(mdio_data), MODE(0) | RXACTIVE | PULLUP_EN},/* MDIO_DATA */
	{OFFSET(mdio_clk), MODE(0) | PULLUP_EN},	/* MDIO_CLK */
	{-1},
};

static struct module_pin_mux mii1_pin_mux[] = {
	{OFFSET(mii1_rxerr), MODE(0) | RXACTIVE},	/* MII1_RXERR */
	{OFFSET(mii1_txen), MODE(0)},			/* MII1_TXEN */
	{OFFSET(mii1_rxdv), MODE(0) | RXACTIVE},	/* MII1_RXDV */
	{OFFSET(mii1_txd3), MODE(0)},			/* MII1_TXD3 */
	{OFFSET(mii1_txd2), MODE(0)},			/* MII1_TXD2 */
	{OFFSET(mii1_txd1), MODE(0)},			/* MII1_TXD1 */
	{OFFSET(mii1_txd0), MODE(0)},			/* MII1_TXD0 */
	{OFFSET(mii1_txclk), MODE(0) | RXACTIVE},	/* MII1_TXCLK */
	{OFFSET(mii1_rxclk), MODE(0) | RXACTIVE},	/* MII1_RXCLK */
	{OFFSET(mii1_rxd3), MODE(0) | RXACTIVE},	/* MII1_RXD3 */
	{OFFSET(mii1_rxd2), MODE(0) | RXACTIVE},	/* MII1_RXD2 */
	{OFFSET(mii1_rxd1), MODE(0) | RXACTIVE},	/* MII1_RXD1 */
	{OFFSET(mii1_rxd0), MODE(0) | RXACTIVE},	/* MII1_RXD0 */
	{OFFSET(mdio_data), MODE(0) | RXACTIVE | PULLUP_EN}, /* MDIO_DATA */
	{OFFSET(mdio_clk), MODE(0) | PULLUP_EN},	/* MDIO_CLK */
	{-1},
};

static struct module_pin_mux nand_pin_mux[] = {
	{OFFSET(gpmc_ad0), (MODE(0) | PULLUP_EN | RXACTIVE)},	/* NAND AD0 */
	{OFFSET(gpmc_ad1), (MODE(0) | PULLUP_EN | RXACTIVE)},	/* NAND AD1 */
	{OFFSET(gpmc_ad2), (MODE(0) | PULLUP_EN | RXACTIVE)},	/* NAND AD2 */
	{OFFSET(gpmc_ad3), (MODE(0) | PULLUP_EN | RXACTIVE)},	/* NAND AD3 */
	{OFFSET(gpmc_ad4), (MODE(0) | PULLUP_EN | RXACTIVE)},	/* NAND AD4 */
	{OFFSET(gpmc_ad5), (MODE(0) | PULLUP_EN | RXACTIVE)},	/* NAND AD5 */
	{OFFSET(gpmc_ad6), (MODE(0) | PULLUP_EN | RXACTIVE)},	/* NAND AD6 */
	{OFFSET(gpmc_ad7), (MODE(0) | PULLUP_EN | RXACTIVE)},	/* NAND AD7 */
	{OFFSET(gpmc_wait0), (MODE(0) | RXACTIVE | PULLUP_EN)}, /* NAND WAIT */
	{OFFSET(gpmc_wpn), (MODE(7) | PULLUP_EN | RXACTIVE)},	/* NAND_WPN */
	{OFFSET(gpmc_csn0), (MODE(0) | PULLUDEN)},	/* NAND_CS0 */
	{OFFSET(gpmc_advn_ale), (MODE(0) | PULLUDEN)}, /* NAND_ADV_ALE */
	{OFFSET(gpmc_oen_ren), (MODE(0) | PULLUDEN)},	/* NAND_OE */
	{OFFSET(gpmc_wen), (MODE(0) | PULLUDEN)},	/* NAND_WEN */
	{OFFSET(gpmc_be0n_cle), (MODE(0) | PULLUDEN)},	/* NAND_BE_CLE */
	{-1},
};

static struct module_pin_mux i2c0_pin_mux[] = {
	{OFFSET(i2c0_sda), (MODE(0) | RXACTIVE |
			PULLUDEN | SLEWCTRL)}, /* I2C_DATA */
	{OFFSET(i2c0_scl), (MODE(0) | RXACTIVE |
			PULLUDEN | SLEWCTRL)}, /* I2C_SCLK */
	{-1},
};

static struct module_pin_mux i2c1_pin_mux[] = {
	{OFFSET(spi0_d1), (MODE(2) | RXACTIVE |
			PULLUDEN | SLEWCTRL)},	/* I2C_DATA */
	{OFFSET(spi0_cs0), (MODE(2) | RXACTIVE |
			PULLUDEN | SLEWCTRL)},	/* I2C_SCLK */
	{-1},
};

static struct module_pin_mux spi0_pin_mux[] = {
	{OFFSET(spi0_sclk), (MODE(0) | RXACTIVE | PULLUDEN)},	/* SPI0_SCLK */
	{OFFSET(spi0_d0), (MODE(0) | RXACTIVE |
			PULLUDEN | PULLUP_EN)},			/* SPI0_D0 */
	{OFFSET(spi0_d1), (MODE(0) | RXACTIVE | PULLUDEN)},	/* SPI0_D1 */
	{OFFSET(spi0_cs0), (MODE(0) | RXACTIVE |
			PULLUDEN | PULLUP_EN)},			/* SPI0_CS0 */
	{-1},
};

void enable_uart0_pin_mux(void)
{
	configure_module_pin_mux(uart0_pin_mux);
}

void enable_uart1_pin_mux(void)
{
	configure_module_pin_mux(uart1_pin_mux);
}

void enable_uart2_pin_mux(void)
{
	configure_module_pin_mux(uart2_pin_mux);
}

void enable_uart3_pin_mux(void)
{
	configure_module_pin_mux(uart3_pin_mux);
}

void enable_uart4_pin_mux(void)
{
	configure_module_pin_mux(uart4_pin_mux);
}

void enable_uart5_pin_mux(void)
{
	configure_module_pin_mux(uart5_pin_mux);
}

void enable_i2c0_pin_mux(void)
{
	configure_module_pin_mux(i2c0_pin_mux);
}

void set_uart_mux_conf(void)
{
#ifdef CONFIG_SERIAL1
	enable_uart0_pin_mux();
#endif /* CONFIG_SERIAL1 */
#ifdef CONFIG_SERIAL2
	enable_uart1_pin_mux();
#endif /* CONFIG_SERIAL2 */
#ifdef CONFIG_SERIAL3
	enable_uart2_pin_mux();
#endif /* CONFIG_SERIAL3 */
#ifdef CONFIG_SERIAL4
	enable_uart3_pin_mux();
#endif /* CONFIG_SERIAL4 */
#ifdef CONFIG_SERIAL5
	enable_uart4_pin_mux();
#endif /* CONFIG_SERIAL5 */
#ifdef CONFIG_SERIAL6
	enable_uart5_pin_mux();
#endif /* CONFIG_SERIAL6 */
}


void enable_board_pin_mux(struct am335x_baseboard_id *header)
{
	/* Do board-specific muxes. */
	if (board_is_bone(header)) {
		/* Beaglebone pinmux */
		configure_module_pin_mux(i2c1_pin_mux);
		configure_module_pin_mux(mii1_pin_mux);
		configure_module_pin_mux(mmc0_pin_mux);
#ifndef CONFIG_NOR
		configure_module_pin_mux(mmc1_pin_mux);
#endif
#if defined(CONFIG_NOR) && !defined(CONFIG_NOR_BOOT)
		configure_module_pin_mux(bone_norcape_pin_mux);
#endif
	} else if (board_is_gp_evm(header)) {
		/* General Purpose EVM */
		unsigned short profile = detect_daughter_board_profile();
		configure_module_pin_mux(rgmii1_pin_mux);
		configure_module_pin_mux(mmc0_pin_mux);
		/* In profile #2 i2c1 and spi0 conflict. */
		if (profile & ~PROFILE_2)
			configure_module_pin_mux(i2c1_pin_mux);
		/* Profiles 2 & 3 don't have NAND */
		if (profile & ~(PROFILE_2 | PROFILE_3))
			configure_module_pin_mux(nand_pin_mux);
		else if (profile == PROFILE_2) {
			configure_module_pin_mux(mmc1_pin_mux);
			configure_module_pin_mux(spi0_pin_mux);
		}
	} else if (board_is_idk(header)) {
		/*
		 * Industrial Motor Control (IDK)
		 * note: IDK console is on UART3 by default.
		 *       So u-boot mus be build with CONFIG_SERIAL4 and
		 *       CONFIG_CONS_INDEX=4
		 */
		configure_module_pin_mux(mii1_pin_mux);
		configure_module_pin_mux(mmc0_no_cd_pin_mux);
	} else if (board_is_evm_sk(header)) {
		/* Starter Kit EVM */
		configure_module_pin_mux(i2c1_pin_mux);
		configure_module_pin_mux(gpio0_7_pin_mux);
		configure_module_pin_mux(rgmii1_pin_mux);
		configure_module_pin_mux(mmc0_pin_mux_sk_evm);
	} else if (board_is_bone_lt(header)) {
		/* Beaglebone LT pinmux */
		configure_module_pin_mux(i2c1_pin_mux);
		configure_module_pin_mux(mii1_pin_mux);
		configure_module_pin_mux(mmc0_pin_mux);
		configure_module_pin_mux(mmc1_pin_mux);
	} else {
		puts("Unknown board, cannot configure pinmux.");
		hang();
	}
}

void set_mux_conf_regs(void)
{
	__maybe_unused struct am335x_baseboard_id header;

	if (read_eeprom(&header) < 0)
		puts("Could not get board ID.\n");

	enable_board_pin_mux(&header);
}

void sdram_init(void)
{
}

#endif //0####

//---------------- TI-Code - Ende -------------------------------

/* Enable a CM_<clock_domain>_<module>_CLKCTRL clock */
static void enable_module_clock(unsigned long clkctrl)
{
	u32 reg;

	if ((__raw_readl(clkctrl) & MODULE_CLKCTRL_IDLEST_MASK)
	    == CLKCTRL_FULLY_FUNCTIONAL)
		return;

	__raw_writel(CLKCTRL_ENABLE, clkctrl);
	do {
		reg = __raw_readl(CM_PER_USB0_CLKCTRL);
		reg &= MODULE_CLKCTRL_IDLEST_MASK;
	} while (reg != CLKCTRL_FULLY_FUNCTIONAL);
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

	printf("Board: %s Rev %u.%02u (%dx DRAM)\n",
	       fs_board_info[pargs->chBoardType].name,
	       pargs->chBoardRev / 100, pargs->chBoardRev % 100,
	       pargs->dwNumDram);

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
   that can hold up to 1GB of RAM. However up to now we only have 256MB on
   F&S TI boards. */
int dram_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs;

	pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	gd->ram_size = pargs->dwMemSize << 20;
	gd->ram_base = PHYS_SDRAM_0;

	return 0;
}

#if 0
static struct module_pin_mux gpio3_14_pin_mux[] = {
	{OFFSET(mcasp0_aclkx), (MODE(7) | PULLUDEN)},	/* GPIO3_14 */
	{-1},
};
#endif

/* Now RAM is valid, U-Boot is relocated. From now on we can use variables */
int board_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	unsigned int board_type = pargs->chBoardType;
	u32 reg;

	/* Save a copy of the NBoot args */
	memcpy(&fs_nboot_args, pargs, sizeof(struct tag_fshwconfig));
	fs_nboot_args.dwSize = sizeof(struct tag_fshwconfig);

	gd->bd->bi_arch_number = fs_board_info[board_type].mach_type;
	gd->bd->bi_boot_params = BOOT_PARAMS_BASE;

	/* Prepare the command prompt */
	sprintf(fs_sys_prompt, "%s # ", fs_board_info[board_type].name);

	/* Init GPMC for NAND */
	gpmc_init();

	/* Activate USB clocks */
	enable_module_clock(CM_PER_USB0_CLKCTRL);
	reg = __raw_readl(CM_CLKDCOLDO_DPLL_PER);
	reg |= (1 << 8);
	__raw_writel(reg, CM_CLKDCOLDO_DPLL_PER);

#if 0
	/* If we need a GPIO for test purposes, use GPIO3_14 on pin 12 (J10
	   pin 2 of SKIT). */
	enable_module_clock(CM_PER_GPIO3_CLKCTRL);
	configure_module_pin_mux(gpio3_14_pin_mux);
        gpio_request(3*32+14, "test_out");
        gpio_direction_output(3*32+14, 1);
	printf("### High\n");
	msleep(2000);
	gpio_set_value(3*32+14, 0);
	printf("### Low\n");
	msleep(2000);
	gpio_set_value(3*32+14, 1);
#endif

	return 0;
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
	   dwAction value. If a special button is pressed for a defined time
	   when power is supplied, the system should be reset to the default
	   state, i.e. perform a complete recovery. The button is detected in
	   NBoot, but recovery takes place in U-Boot. Currently this is not
	   defined for PicoCOMA8, but who knows, maybe this will change in the
	   future, then we do not need to change anything here. */
	if (fs_nboot_args.dwAction & ACTION_RECOVER)
		return UPDATE_ACTION_RECOVER;
	return UPDATE_ACTION_UPDATE;
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
	setup_var("network", bi->network, 1);
	setup_var("init", bi->init, 1);
	setup_var("rootfs", bi->rootfs, 1);
	setup_var("kernel", bi->kernel, 1);
	setup_var("bootargs", "set_bootargs", 1);

	return 0;
}
#endif




#ifdef CONFIG_CMD_NET
static void cpsw_control(int enabled)
{
	/* VTP can be added here */

	return;
}

static struct cpsw_slave_data cpsw_slaves[] = {
	{
		.slave_reg_ofs	= 0x208,
		.sliver_reg_ofs	= 0xd80,
		.phy_addr	= 1,
	},
#if 0
	{
		.slave_reg_ofs	= 0x308,
		.sliver_reg_ofs	= 0xdc0,
		.phy_addr	= 1,
	},
#endif
};

static struct cpsw_platform_data cpsw_data = {
	.mdio_base		= CPSW_MDIO_BASE,
	.cpsw_base		= CPSW_BASE,
	.mdio_div		= 0xff,
	.channels		= 8,
	.cpdma_reg_ofs		= 0x800,
	.slaves			= 1,
	.slave_data		= cpsw_slaves,
	.ale_reg_ofs		= 0xd00,
	.ale_entries		= 1024,
	.host_port_reg_ofs	= 0x108,
	.hw_stats_reg_ofs	= 0x900,
	.bd_ram_ofs		= 0x2000,
	.mac_control		= (1 << 5),
	.control		= cpsw_control,
	.host_port_num		= 0,
	.version		= CPSW_CTRL_VERSION_2,
};

static struct module_pin_mux mii1_pin_mux[] = {
	{OFFSET(mii1_crs), MODE(1) | RXACTIVE},		/* RMII1_CRS_DV */
	{OFFSET(rmii1_refclk), MODE(0) | RXACTIVE},	/* RMII1_REFCLK */
	{OFFSET(mii1_rxerr), MODE(1) | RXACTIVE},	/* RMII1_RXERR */
	{OFFSET(mii1_txen), MODE(1)},			/* RMII1_TXEN */
	//{OFFSET(mii1_rxdv), MODE(0) | RXACTIVE},	/* MII1_RXDV */
	//{OFFSET(mii1_txd3), MODE(0)},			/* MII1_TXD3 */
	//{OFFSET(mii1_txd2), MODE(0)},			/* MII1_TXD2 */
	{OFFSET(mii1_txd1), MODE(1)},			/* RMII1_TXD1 */
	{OFFSET(mii1_txd0), MODE(1)},			/* RMII1_TXD0 */
	//{OFFSET(mii1_txclk), MODE(0) | RXACTIVE},	/* MII1_TXCLK */
	//{OFFSET(mii1_rxclk), MODE(0) | RXACTIVE},	/* MII1_RXCLK */
	//{OFFSET(mii1_rxd3), MODE(0) | RXACTIVE},	/* MII1_RXD3 */
	//{OFFSET(mii1_rxd2), MODE(0) | RXACTIVE},	/* MII1_RXD2 */
	{OFFSET(mii1_rxd1), MODE(1) | RXACTIVE},	/* RMII1_RXD1 */
	{OFFSET(mii1_rxd0), MODE(1) | RXACTIVE},	/* RMII1_RXD0 */
	{OFFSET(mdio_data), MODE(0) | RXACTIVE | PULLUP_EN}, /* MDIO_DATA */
	{OFFSET(mdio_clk), MODE(0) | PULLUP_EN},	/* MDIO_CLK */
	{-1},
};

int board_eth_init(bd_t *bis)
{
	int rv, n = 0;
#if 0
	uint8_t mac_addr[6];
	uint32_t mac_hi, mac_lo;

	/* try reading mac address from efuse */
	mac_lo = readl(&cdev->macid0l);
	mac_hi = readl(&cdev->macid0h);
	mac_addr[0] = mac_hi & 0xFF;
	mac_addr[1] = (mac_hi & 0xFF00) >> 8;
	mac_addr[2] = (mac_hi & 0xFF0000) >> 16;
	mac_addr[3] = (mac_hi & 0xFF000000) >> 24;
	mac_addr[4] = mac_lo & 0xFF;
	mac_addr[5] = (mac_lo & 0xFF00) >> 8;

	if (!getenv("ethaddr")) {
		printf("<ethaddr> not set. Validating first E-fuse MAC\n");

		if (is_valid_ether_addr(mac_addr))
			eth_setenv_enetaddr("ethaddr", mac_addr);
	}

	mac_lo = readl(&cdev->macid1l);
	mac_hi = readl(&cdev->macid1h);
	mac_addr[0] = mac_hi & 0xFF;
	mac_addr[1] = (mac_hi & 0xFF00) >> 8;
	mac_addr[2] = (mac_hi & 0xFF0000) >> 16;
	mac_addr[3] = (mac_hi & 0xFF000000) >> 24;
	mac_addr[4] = mac_lo & 0xFF;
	mac_addr[5] = (mac_lo & 0xFF00) >> 8;

	if (!getenv("eth1addr")) {
		if (is_valid_ether_addr(mac_addr))
			eth_setenv_enetaddr("eth1addr", mac_addr);
	}
#endif

	/* Configure ethernet for RMII */
	configure_module_pin_mux(mii1_pin_mux);

	/* Enable the ethernet clock */
	enable_module_clock(CM_PER_CPGMAC0_CLKCTRL);

	/* Enable the CPSW clock */
	if ((__raw_readl(CM_PER_CPSW_CLKSTCTRL) & (1 << 4)) == 0) {
		__raw_writel(2 << 0, CM_PER_CPSW_CLKSTCTRL);
		while ((__raw_readl(CM_PER_CPSW_CLKSTCTRL) & (1 << 4)) == 0)
			;
	}

	/* Soft-reset CPSW_SS */
	//printf("### Soft-reset CPSW_SS\n");
	__raw_writel(1, 0x4a100008);
	while (__raw_readl(0x4a100008) & 1)
		;

	/* Soft-reset CPSW_WR */
	//printf("### Soft-reset CPSW_WR\n");
	__raw_writel(1, 0x4a101204);
	while (__raw_readl(0x4a101204) & 1)
		;

	/* Soft-reset CPGMAC_SL1 */
	//printf("### Soft-reset CPGMAC_SL1\n");
	__raw_writel(1, 0x4a100d8c);
	while (__raw_readl(0x4a100d8c) & 1)
		;

	/* Soft-reset CPDMA */
	//printf("### Soft-reset CPDMA\n");
	__raw_writel(1, 0x4a10081c);
	while (__raw_readl(0x4a10081c) & 1)
		;

	writel(RMII_MODE_ENABLE | RMII_CHIPCKL_ENABLE, &cdev->miisel);
	cpsw_slaves[0].phy_if = PHY_INTERFACE_MODE_MII;

	rv = cpsw_register(&cpsw_data);
	if (rv < 0)
		printf("Error %d registering CPSW switch\n", rv);
	else
		n += rv;

#if 0
	/*
	 *
	 * CPSW RGMII Internal Delay Mode is not supported in all PVT
	 * operating points.  So we must set the TX clock delay feature
	 * in the AR8051 PHY.  Since we only support a single ethernet
	 * device in U-Boot, we only do this for the first instance.
	 */
#define AR8051_PHY_DEBUG_ADDR_REG	0x1d
#define AR8051_PHY_DEBUG_DATA_REG	0x1e
#define AR8051_DEBUG_RGMII_CLK_DLY_REG	0x5
#define AR8051_RGMII_TX_CLK_DLY		0x100

	if (board_is_evm_sk(&header) || board_is_gp_evm(&header)) {
		const char *devname;
		devname = miiphy_get_current_dev();

		miiphy_write(devname, 0x0, AR8051_PHY_DEBUG_ADDR_REG,
				AR8051_DEBUG_RGMII_CLK_DLY_REG);
		miiphy_write(devname, 0x0, AR8051_PHY_DEBUG_DATA_REG,
				AR8051_RGMII_TX_CLK_DLY);
	}
#endif

	return n;
}
#endif

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
