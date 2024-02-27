/*
 * (C) Copyright 2018-2021 F&S Elektronik Systeme GmbH
 *
 * Board specific functions for F&S boards based on Freescale i.MX8MN CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <spl.h>
#include <hang.h>
#include <asm/io.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/arch/imx8mn_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/clock.h>
#include <fdt_support.h>		/* fdt_getprop_u32_default_node() */
#include <power/pmic.h>
#include <power/bd71837.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <init.h>			/* arch_cpu_init() */
#include <mmc.h>
#include <nand.h>
#include <asm/arch/ddr.h>

#include <asm/sections.h>
#include <sdp.h>

#include <asm/mach-imx/boot_mode.h>	/* BOOT_TYPE_* */
#include "../common/fs_image_common.h"	/* fs_image_*() */
#include "../common/fs_board_common.h"	/* fs_board_*() */
#include "../common/fs_mmc_common.h"	/* struct fs_mmc_cd, fs_mmc_*(), ... */

DECLARE_GLOBAL_DATA_PTR;

#define BT_PICOCOREMX8MN 0x0
#define BT_PICOCOREMX8MX 0x1

static const char *board_names[] = {
	"PicoCoreMX8MN-LPDDR4",
	"PicoCoreMX8MN-DDR3L",
	"(unknown)"
};

static unsigned int board_type;
static const char *board_name;
static const char *board_fdt;
static enum boot_device used_boot_dev;	/* Boot device used for NAND/MMC */
static bool boot_dev_init_done;
static unsigned int uboot_offs;
static unsigned int uboot_offs_redundant; /* offset of redundant UBoot */
static unsigned int uboot_part;
static unsigned int uboot_part_redundant; /* partition of redundant UBoot */
static bool secondary;			/* 0: primary, 1: secondary SPL */
static int uboot_try; /* 0: first try, 1: second try */

#ifdef CONFIG_POWER
#define I2C_PMIC_8MN	3
#define I2C_PMIC_8MX	0

#define I2C_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE | PAD_CTL_PE)
#define PC MUX_PAD_CTRL(I2C_PAD_CTRL)

struct i2c_pads_info i2c_pad_info_8mn = {
	.scl = {
		.i2c_mode = IMX8MN_PAD_I2C4_SCL__I2C4_SCL | PC,
		.gpio_mode = IMX8MN_PAD_I2C4_SCL__GPIO5_IO20 | PC,
		.gp = IMX_GPIO_NR(5, 20),
	},
	.sda = {
		.i2c_mode = IMX8MN_PAD_I2C4_SDA__I2C4_SDA | PC,
		.gpio_mode = IMX8MN_PAD_I2C4_SDA__GPIO5_IO21 | PC,
		.gp = IMX_GPIO_NR(5, 21),
	},
};

struct i2c_pads_info i2c_pad_info_8mx = {
	.scl = {
		.i2c_mode = IMX8MN_PAD_I2C1_SCL__I2C1_SCL | PC,
		.gpio_mode = IMX8MN_PAD_I2C1_SCL__GPIO5_IO14 | PC,
		.gp = IMX_GPIO_NR(5, 14),
	},
	.sda = {
		.i2c_mode = IMX8MN_PAD_I2C1_SDA__I2C1_SDA | PC,
		.gpio_mode = IMX8MN_PAD_I2C1_SDA__GPIO5_IO15 | PC,
		.gp = IMX_GPIO_NR(5, 15),
	},
};

int power_init_board(void)
{
	struct pmic *p;
	int ret;

	switch (board_type)
	{
	default:
	case BT_PICOCOREMX8MN:
		setup_i2c(I2C_PMIC_8MN, CONFIG_SYS_I2C_SPEED, 0x7f,
			  &i2c_pad_info_8mn);
		ret = power_bd71837_init(I2C_PMIC_8MN);
		break;
	case BT_PICOCOREMX8MX:
		setup_i2c(I2C_PMIC_8MX, CONFIG_SYS_I2C_SPEED, 0x7f,
			  &i2c_pad_info_8mx);
		ret = power_bd71837_init(I2C_PMIC_8MX);
		break;
	}

	if (ret)
		printf("power init failed");

	p = pmic_get("BD71837");

	pmic_probe(p);


	/* decrease RESET key long push time from the default 10s to 10ms */
	pmic_reg_write(p, BD718XX_PWRONCONFIG1, 0x0);

	/* unlock the PMIC regs */
	pmic_reg_write(p, BD718XX_REGLOCK, 0x1);

	/* increase VDD_SOC to typical value 0.85v before first DRAM access */
	pmic_reg_write(p, BD718XX_BUCK1_VOLT_RUN, 0x0f);

	switch (board_type)
	{
	case BT_PICOCOREMX8MN:
		/* increase VDD_DRAM to 0.975v f-*or 3Ghz DDR */
		pmic_reg_write(p, BD718XX_1ST_NODVS_BUCK_VOLT, 0x83);
		break;
	case BT_PICOCOREMX8MX:
		/* increase VDD_DRAM to 0.9v for 3Ghz DDR */
		pmic_reg_write(p, BD718XX_1ST_NODVS_BUCK_VOLT, 0x2);

		/* increase NVCC_DRAM_1V35 to 1.35v for DDR3L */
		pmic_reg_write(p, BD718XX_4TH_NODVS_BUCK_VOLT, 0x37);
		break;
	}

	/* lock the PMIC regs */
	pmic_reg_write(p, BD718XX_REGLOCK, 0x11);

	return 0;
}
#endif

#define WDOG_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_ODE | PAD_CTL_PUE | PAD_CTL_PE)

static iomux_v3_cfg_t const wdog_pads[] = {
	IMX8MN_PAD_GPIO1_IO02__WDOG1_WDOG_B  | MUX_PAD_CTRL(WDOG_PAD_CTRL),
};

static void wdog_init(void)
{
	struct wdog_regs *wdog = (struct wdog_regs*) WDOG1_BASE_ADDR;

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));

	set_wdog_reset(wdog);
}

#define UART_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL1)

static iomux_v3_cfg_t const uart_pads_mn[] = {
	MUX_PAD_CTRL(UART_PAD_CTRL) | IMX8MN_PAD_UART1_RXD__UART1_DCE_RX,
	MUX_PAD_CTRL(UART_PAD_CTRL) | IMX8MN_PAD_UART1_TXD__UART1_DCE_TX,
};

static iomux_v3_cfg_t const uart_pads_mx[] = {
	MUX_PAD_CTRL(UART_PAD_CTRL) | IMX8MN_PAD_SAI2_RXC__UART1_DCE_RX,
	MUX_PAD_CTRL(UART_PAD_CTRL) | IMX8MN_PAD_SAI2_RXFS__UART1_DCE_TX,
};

/* Setup and start serial debug port */
static void config_uart(int board_type)
{
	switch (board_type)
	{
	default:
	case BT_PICOCOREMX8MN:
		/* Setup UART1 on UART1 pads */
		imx_iomux_v3_setup_multiple_pads(uart_pads_mn,
						 ARRAY_SIZE(uart_pads_mn));
		init_uart_clk(0);
		break;

	case BT_PICOCOREMX8MX:
		/* Setup UART1 on SAI2 pads */
		imx_iomux_v3_setup_multiple_pads(uart_pads_mx,
						 ARRAY_SIZE(uart_pads_mx));
		init_uart_clk(0);
		break;
	}

	preloader_console_init();
}

#ifdef CONFIG_NAND_MXS
/* Pad settings if using NAND flash */
#define NAND_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL2 | PAD_CTL_HYS)
#define NAND_PAD_READY0_CTRL (PAD_CTL_DSE6 | PAD_CTL_FSEL2 | PAD_CTL_PUE)

static iomux_v3_cfg_t const nand_pads[] = {
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MN_PAD_NAND_ALE__RAWNAND_ALE,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MN_PAD_NAND_CE0_B__RAWNAND_CE0_B,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MN_PAD_NAND_CLE__RAWNAND_CLE,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MN_PAD_NAND_DATA00__RAWNAND_DATA00,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MN_PAD_NAND_DATA01__RAWNAND_DATA01,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MN_PAD_NAND_DATA02__RAWNAND_DATA02,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MN_PAD_NAND_DATA03__RAWNAND_DATA03,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MN_PAD_NAND_DATA04__RAWNAND_DATA04,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MN_PAD_NAND_DATA05__RAWNAND_DATA05,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MN_PAD_NAND_DATA06__RAWNAND_DATA06,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MN_PAD_NAND_DATA07__RAWNAND_DATA07,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MN_PAD_NAND_RE_B__RAWNAND_RE_B,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MN_PAD_NAND_WE_B__RAWNAND_WE_B,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MN_PAD_NAND_WP_B__RAWNAND_WP_B,
	MUX_PAD_CTRL(NAND_PAD_READY0_CTRL)
				    | IMX8MN_PAD_NAND_READY_B__RAWNAND_READY_B,
};

#endif /* CONFIG_NAND_MXS */

#ifdef CONFIG_MMC
/* Pad settings if using eMMC */
#define USDHC_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE \
			 |PAD_CTL_PE | PAD_CTL_FSEL2)
#define USDHC_GPIO_PAD_CTRL (PAD_CTL_HYS | PAD_CTL_DSE1)

static iomux_v3_cfg_t const emmc_pads[] = {
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MN_PAD_NAND_WE_B__USDHC3_CLK,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MN_PAD_NAND_WP_B__USDHC3_CMD,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MN_PAD_NAND_DATA04__USDHC3_DATA0,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MN_PAD_NAND_DATA05__USDHC3_DATA1,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MN_PAD_NAND_DATA06__USDHC3_DATA2,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MN_PAD_NAND_DATA07__USDHC3_DATA3,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MN_PAD_NAND_RE_B__USDHC3_DATA4,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MN_PAD_NAND_CE2_B__USDHC3_DATA5,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MN_PAD_NAND_CE3_B__USDHC3_DATA6,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MN_PAD_NAND_CLE__USDHC3_DATA7,
//###	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MN_PAD_NAND_CE1_B__USDHC3_STROBE,
	MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL) | IMX8MN_PAD_NAND_CE1_B__GPIO3_IO2,
	/* IMX8MN_PAD_NAND_READY_B__USDHC3_RESET_B */
	MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL) | IMX8MN_PAD_NAND_READY_B__GPIO3_IO16,
};

int board_mmc_getcd(struct mmc *mmc)
{
	return 1;			/* eMMC always present */
}

int board_mmc_init(struct bd_info *bd)
{
	struct fsl_esdhc_cfg esdhc;

	switch (used_boot_dev) {
	case MMC1_BOOT:
		esdhc.esdhc_base = USDHC1_BASE_ADDR;
		esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);
		break;

	case MMC2_BOOT:
		esdhc.esdhc_base = USDHC2_BASE_ADDR;
		esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC2_CLK);
		break;

	case MMC3_BOOT:
		esdhc.esdhc_base = USDHC3_BASE_ADDR;
		esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
		break;
	default:
		return -ENODEV;
	}

	return fsl_esdhc_initialize(bd, &esdhc);
}
#endif /* CONFIG_MMC */

/* Configure (and optionally start) the given boot device */
static int fs_spl_init_boot_dev(enum boot_device boot_dev, const char *type)
{
	if (boot_dev_init_done)
		return 0;

	used_boot_dev = boot_dev;
	switch (boot_dev) {
#ifdef CONFIG_NAND_MXS
	case NAND_BOOT:
		imx_iomux_v3_setup_multiple_pads(nand_pads, ARRAY_SIZE(nand_pads));
		init_nand_clk();
		nand_init();
		break;
#endif
#ifdef CONFIG_MMC
	case MMC3_BOOT:
		imx_iomux_v3_setup_multiple_pads(emmc_pads, ARRAY_SIZE(emmc_pads));
		init_clk_usdhc(2);
		mmc_initialize(NULL);
		break;
		//### TODO: Also have setups for MMC1_BOOT and MMC2_BOOT
#endif
	case USB_BOOT:
		return -ENODEV;

	default:
		printf("Can not handle %s boot device %s\n", type,
		       fs_board_get_name_from_boot_dev(boot_dev));
		return -ENODEV;
	}

	boot_dev_init_done = true;
	return 0;
}

#define OPEN_DRAIN_PAD_CTRL (PAD_CTL_DSE6 | PAD_CTL_ODE)

static iomux_v3_cfg_t const lvds_rst_8mn_120_pads =
	IMX8MN_PAD_GPIO1_IO13__GPIO1_IO13  | MUX_PAD_CTRL(OPEN_DRAIN_PAD_CTRL);

static iomux_v3_cfg_t const lvds_rst_8mn_130_pads =
	IMX8MN_PAD_SAI3_TXFS__GPIO4_IO31  | MUX_PAD_CTRL(OPEN_DRAIN_PAD_CTRL);

static iomux_v3_cfg_t const lvds_rst_8mx_110_pads =
	IMX8MN_PAD_GPIO1_IO08__GPIO1_IO8  | MUX_PAD_CTRL(OPEN_DRAIN_PAD_CTRL);

static void fs_board_early_init(void)
{
	switch (board_type)
	{
	case BT_PICOCOREMX8MN:
		if (fs_image_get_board_rev() < 130)
			imx_iomux_v3_setup_pad(lvds_rst_8mn_120_pads);
		else
			imx_iomux_v3_setup_pad(lvds_rst_8mn_130_pads);
		break;
	case BT_PICOCOREMX8MX:
			imx_iomux_v3_setup_pad(lvds_rst_8mx_110_pads);
		break;
	default:
		break;
	}
}

void mmc_get_parts()
{
	if(uboot_offs == uboot_offs_redundant){
		uboot_part = 1;
		uboot_part_redundant = 2;
	}
	else{
		uboot_part = 0;
		uboot_part_redundant = 0;
	}
}

/* Do the basic board setup when we have our final BOARD-CFG */
static void basic_init(const char *layout_name)
{
	void *fdt = fs_image_get_cfg_fdt();
	int offs = fs_image_get_board_cfg_offs(fdt);
	int rev_offs = fs_image_get_board_rev_subnode(fdt, offs);
	int i;
	char c;
	int index;
	const char *boot_dev_name;
	enum boot_device boot_dev;

	board_name = fs_image_getprop(fdt, offs, rev_offs, "board-name", NULL);
	for (i = 0; i < ARRAY_SIZE(board_names); i++) {
		if (!strcmp(board_name, board_names[i]))
			break;
	}
	board_type = i;

	/*
	 * If an fdt-name is not given, use board name in lower case. Please
	 * note that this name is only used for the U-Boot device tree. The
	 * Linux device tree name is defined by executing U-Boot's environment
	 * variable set_bootfdt.
	 */
	board_fdt = fs_image_getprop(fdt, offs, rev_offs, "board-fdt", NULL);
	if (!board_fdt) {
		static char board_name_lc[32];

		i = 0;
		do {
			c = board_name[i];
			if ((c >= 'A') && (c <= 'Z'))
				c += 'a' - 'A';
			board_name_lc[i++] = c;
		} while (c);

		board_fdt = (const char *)&board_name_lc[0];
	}

	config_uart(board_type);
	if (secondary)
		puts("Warning! Running secondary SPL, please check if"
		     " primary SPL is damaged.\n");

	boot_dev_name = fs_image_getprop(fdt, offs, rev_offs, "boot-dev", NULL);
	boot_dev = fs_board_get_boot_dev_from_name(boot_dev_name);

	printf("BOARD-ID: %s\n", fs_image_get_board_id());

	/* Get U-Boot offset; not necessary in SDP mode */
	if (layout_name) {
		int layout;
#ifdef CONFIG_FS_UPDATE_SUPPORT
		index = 0;		/* ### TODO: Select slot A or B */
#else
		index = 0;
#endif
		offs = fs_image_get_nboot_info_offs(fdt);
		layout = fdt_subnode_offset(fdt, offs, layout_name);
		uboot_offs = fdt_getprop_u32_default_node(fdt, layout, index,
							  "uboot-start", 0);
		uboot_offs_redundant = fdt_getprop_u32_default_node(
					fdt, layout, index + 1,"uboot-start",
								uboot_offs);

		/* Only relevant for mmc, but does no harm for Nand */
		mmc_get_parts();
	}

	/* We need to have the boot device pads active when starting U-Boot */
	fs_spl_init_boot_dev(boot_dev, "BOARD-CFG");

	fs_board_early_init();
	power_init_board();
}

int spl_mmc_emmc_boot_partition(struct mmc *mmc)
{
	int part = 0;

	if(uboot_try == 0)
		part = uboot_part;
	else
		part = uboot_part_redundant;

	return part;
}

int check_if_secondary()
{
	uint32_t * csf_addr = (uint32_t *)*(uint32_t**)(CONFIG_SPL_TEXT_BASE - 0x28);
	uint32_t * copy_addr = csf_addr - 1;
	int copy_val = *copy_addr;
	return copy_val;
}

void board_init_f(ulong dummy)
{
	int ret;
	enum boot_device boot_dev;

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	arch_cpu_init();

	wdog_init();
	timer_init();

#if 0
	/*
	 * Enable this to have early debug output before BOARD-CFG is loaded
	 * You have to provide the board type, we do not know it yet
	 */
	config_uart(BT_PICOCOREMX8MX);
	puts("Hello\n");
#endif

	/* Init malloc_f pool and boot stages */
	ret = spl_init();
	if (ret) {
		debug("spl_init() failed: %d\n", ret);
		hang();
	}
	enable_tzc380();

#if 0
	// ### TODO: How do we determine this on i.MX8MN?
	{
		struct src *src;
		/* Determine if we are running on primary or secondary SPL */
		src = (struct src *)SRC_BASE_ADDR;
		if (readl(&src->gpr10) & (1 << 30))
			secondary = true;
	}
#endif
	secondary = check_if_secondary();

	/* Try loading from the current boot dev. If this fails, try USB. */
	boot_dev = get_boot_device();

	if (boot_dev != USB_BOOT) {
		if (fs_spl_init_boot_dev(boot_dev, "current")
		    || fs_image_load_system(boot_dev, secondary, basic_init))
			boot_dev = USB_BOOT;
	}
	if (boot_dev == USB_BOOT) {
		bool need_cfg = true;

		/* Try loading a BOARD-CFG from the fused boot device first */
		boot_dev = fs_board_get_boot_dev_from_fuses();
		if (!fs_spl_init_boot_dev(boot_dev, "fused")
		    && !fs_image_load_system(boot_dev, secondary, NULL))
			need_cfg = false;

		/* Load the system from USB with Serial Download Protocol */
		fs_image_all_sdp(need_cfg, basic_init);
	}

	/* If running on secondary SPL, mark BOARD-CFG to pass info to U-Boot */
	if (secondary)
		fs_image_mark_secondary();

	/* At this point we have a valid system configuration */
	board_init_r(NULL, 0);
}

/* BL_ON */
#define BL_ON_PAD IMX_GPIO_NR(5, 3)
static iomux_v3_cfg_t const bl_on_pad =
	IMX8MN_PAD_SPDIF_TX__GPIO5_IO3 | MUX_PAD_CTRL(NO_PAD_CTRL);

void spl_board_init(void)
{
	imx_iomux_v3_setup_pad(bl_on_pad);
	/* backlight off*/
	gpio_request(BL_ON_PAD, "BL_ON");
	gpio_direction_output(BL_ON_PAD, 0);

#ifndef CONFIG_SPL_USB_SDP_SUPPORT
	/* Serial download mode */
	if (is_usb_boot()) {
		puts("Back to ROM, SDP\n");
		restore_boot_params();
	}
#endif
	debug("Normal Boot\n");
}

/* Return the offset where U-Boot starts in NAND */
uint32_t spl_nand_get_uboot_raw_page(void)
{
	return uboot_offs;
}

/* Return the sector number where U-Boot starts in eMMC (User HW partition) */
unsigned long spl_mmc_get_uboot_raw_sector(struct mmc *mmc)
{
	int offs;

	if(uboot_try == 0)
		offs = uboot_offs / 512;
	else
		offs = uboot_offs_redundant / 512;

	uboot_try++;
	return offs;
}

/*
 * Provide our own boot order, which in fact has just one entry: the current
 * boot device. Unfortunately the regular SPL infrastructure of U-Boot uses an
 * own set of boot devices (defined in arch/arm/include/asm/spl.h) that
 * differs from NXP's definitions in arch/arm/include/asm/mach-imx/boot_mode.h.
 * Here only BOOT_DEVICE_MMC1, BOOT_DEVICE_MMC2 and BOOT_DEVICE_MMC2_2 are
 * known as MMC devices. These values do not actually refer to a hardware
 * device of this number, they simply mean the MMC index. So BOOT_DEVICE_MMC1
 * is the first MMC device with index 0 and both BOOT_DEVICE_MMC2 as well as
 * BOOT_DEVICE_MMC2_2 point to the second MMC device with index 1. See
 * spl_mmc_get_device_index() in common/spl/spl_mmc.c. So at some point, the
 * NXP boot device has to be converted to U-Boot's device. This happens in
 * spl_boot_device() in arch/arm/mach-imx/spl.c.
 *
 * Unfortunately this mapping there does not fit our needs. We only have one
 * MMC device active in SPL, and this is the boot device. It will always be on
 * index 0, no matter what USDHC port we actually boot from. So we always have
 * to map to BOOT_DEVICE_MMC1. By overwriting the weak board_boot_order() in
 * common/spl/spl.c, we can change this to our needs.
 */
void board_boot_order(u32 *spl_boot_list)
{
	switch (get_boot_device()) {
	case MMC1_BOOT:
	case MMC2_BOOT:
	case MMC3_BOOT:
		spl_boot_list[0] = BOOT_DEVICE_MMC1;
		break;
	case NAND_BOOT:
		spl_boot_list[0] = BOOT_DEVICE_NAND;
		break;
	case USB_BOOT:
		spl_boot_list[0] = BOOT_DEVICE_BOARD;
		break;
	default:
		spl_boot_list[0] = BOOT_DEVICE_NONE;
		break;
	}
}

#ifdef CONFIG_SPL_LOAD_FIT
/*
 * This function is called for each appended device tree. If we signal a match
 * (return value 0), the referenced device tree (and only this) is loaded
 * behind U-Boot. So from the view of U-Boot, it always has the right device
 * tree when starting. See doc/README.multi-dtb-fit for details.
 */
int board_fit_config_name_match(const char *name)
{
	return strcmp(name, board_fdt);
}
#endif
