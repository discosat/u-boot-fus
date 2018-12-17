/*
 * (C) Copyright 2009
 * Stefano Babic, DENX Software Engineering, sbabic@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __ASM_ARCH_CLOCK_H
#define __ASM_ARCH_CLOCK_H

#include <common.h>

#ifdef CONFIG_SYS_MX6_HCLK
#define MXC_HCLK	CONFIG_SYS_MX6_HCLK
#else
#define MXC_HCLK	24000000
#endif

#ifdef CONFIG_SYS_MX6_CLK32
#define MXC_CLK32	CONFIG_SYS_MX6_CLK32
#else
#define MXC_CLK32	32768
#endif

enum pll_clocks {
	PLL_SYS,	/* System PLL */
	PLL_BUS,	/* System Bus PLL*/
	PLL_USBOTG,	/* OTG USB PLL */
	PLL_ENET,	/* ENET PLL */
	PLL_AUDIO,	/* AUDIO PLL */
	PLL_VIDEO,	/* AUDIO PLL */
	PLL_USB2,	/* USB Host PLL */
};

enum mxc_clock {
	MXC_ARM_CLK = 0,
	MXC_PER_CLK,
	MXC_AHB_CLK,
	MXC_IPG_CLK,
	MXC_IPG_PERCLK,
	MXC_UART_CLK,
	MXC_CSPI_CLK,
	MXC_AXI_CLK,
	MXC_EMI_SLOW_CLK,
	MXC_DDR_CLK,
	MXC_ESDHC_CLK,
	MXC_ESDHC2_CLK,
	MXC_ESDHC3_CLK,
	MXC_ESDHC4_CLK,
	MXC_SATA_CLK,
	MXC_NFC_CLK,
	MXC_I2C_CLK,
};

enum ldb_di_clock {
	MXC_PLL5_CLK = 0,
	MXC_PLL2_PFD0_CLK,
	MXC_PLL2_PFD2_CLK,
	MXC_MMDC_CH1_CLK,
	MXC_PLL3_SW_CLK,
};

enum enet_freq {
	ENET_25MHZ,
	ENET_50MHZ,
	ENET_100MHZ,
	ENET_125MHZ,
};

u32 imx_get_uartclk(void);
u32 imx_get_fecclk(void);
unsigned int mxc_get_ldb_clock(int channel);
unsigned int mxc_get_ipu_clock(int ipu);
unsigned int mxc_get_ipu_di_clock(int ipu, int di);
unsigned int mxc_get_clock(enum mxc_clock clk);
void setup_gpmi_io_clk(u32 cfg);
void hab_caam_clock_enable(unsigned char enable);
void enable_ocotp_clk(unsigned char enable);
void enable_usboh3_clk(unsigned char enable);
void enable_uart_clk(unsigned char enable);
int enable_usdhc_clk(unsigned char enable, unsigned bus_num);
int enable_sata_clock(void);
void disable_sata_clock(void);
int enable_pcie_clock(void);
int enable_i2c_clk(unsigned char enable, unsigned i2c_num);
int enable_spi_clk(unsigned char enable, unsigned spi_num);
void enable_ldb_di_clk(int channel);
int enable_fec_anatop_clock(int fec_id, enum enet_freq freq);
void enable_enet_clk(unsigned char enable);
void enable_qspi_clk(int qspi_num);
void enable_thermal_clk(void);

/* Display related clock handling */
unsigned int mxs_get_ldb_clock(int channel);
unsigned int mxs_get_lcdif_clock(int lcdif);
void mxs_enable_lcdif_clk(unsigned int base_addr);
int mxs_config_lcdif_clk(unsigned int base_addr, unsigned int freq_khz);
int mxs_config_lvds_clk(unsigned int base_addr, unsigned int freq_khz);
unsigned int ipuv3_get_ldb_clock(int channel);
unsigned int ipuv3_get_ipu_di_clock(int ipu, int di);
unsigned int ipuv3_get_ipu_clock(int ipu);
void ipuv3_enable_ipu_clk(int ipu);
int ipuv3_config_lcd_di_clk(u32 ipu, u32 di);
int ipuv3_config_lvds_clk(unsigned int ipu, unsigned int di,
			  unsigned int freq_khz, unsigned int split);
int enable_video_pll(void);
void disable_video_pll(void);
int setup_video_pll(u32 freq_khz);
int freq_is_accurate(unsigned int freq_is, unsigned int freq_target);

#endif /* __ASM_ARCH_CLOCK_H */
