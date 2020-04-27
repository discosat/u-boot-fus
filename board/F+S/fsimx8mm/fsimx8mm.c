/*
 * Copyright 2018 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <dm.h>
#include <errno.h>
#include <asm/io.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm-generic/gpio.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <asm/arch/imx8mm_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/gpio.h>
#include <i2c.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/clock.h>
#include <spl.h>
#include <asm/mach-imx/dma.h>
#include <power/bd71837.h>
#include <usb.h>
#include <sec_mipi_dsim.h>
#include <imx_mipi_dsi_bridge.h>
#include <mipi_dsi_panel.h>
#include <asm/mach-imx/video.h>
#include <serial.h>			/* get_serial_device() */
#include "../common/fs_fdt_common.h"	/* fs_fdt_set_val(), ... */
#include "../common/fs_board_common.h"	/* fs_board_*() */
#include <nand.h>


DECLARE_GLOBAL_DATA_PTR;

#define UART_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL1)
#define WDOG_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_ODE | PAD_CTL_PUE | PAD_CTL_PE)
#define ENET_PAD_CTRL ( \
		PAD_CTL_PUE |	\
		PAD_CTL_DSE6   | PAD_CTL_HYS)

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


/* Features set in fs_nboot_args.chFeature2 (available since NBoot VN27) */
#define FEAT2_ETH_A   (1<<0)		/* 0: no LAN0, 1; has LAN0 */
#define FEAT2_ETH_B   (1<<1)		/* 0: no LAN1, 1; has LAN1 */
#define FEAT2_EMMC    (1<<2)		/* 0: no eMMC, 1: has eMMC */
#define FEAT2_WLAN    (1<<3)		/* 0: no WLAN, 1: has WLAN */
#define FEAT2_HDMICAM (1<<4)		/* 0: LCD-RGB, 1: HDMI+CAM (PicoMOD) */
#define FEAT2_AUDIO   (1<<5)		/* 0: Codec onboard, 1: Codec extern */
#define FEAT2_SPEED   (1<<6)		/* 0: Full speed, 1: Limited speed */
#define FEAT2_ETH_MASK (FEAT2_ETH_A | FEAT2_ETH_B)

const struct fs_board_info board_info[1] = {
	{	/* 0 (BT_PICOCOREMX8MM) */
		.name = "PicoCoreMX8MM",
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_init",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
	},
};


static iomux_v3_cfg_t const uart_pads[] = {
	IMX8MM_PAD_UART1_RXD_UART1_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	IMX8MM_PAD_UART1_TXD_UART1_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

static iomux_v3_cfg_t const wdog_pads[] = {
	IMX8MM_PAD_GPIO1_IO02_WDOG1_WDOG_B  | MUX_PAD_CTRL(WDOG_PAD_CTRL),
};

#ifdef CONFIG_NAND_MXS
#define NAND_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL2 | PAD_CTL_HYS)
#define NAND_PAD_READY0_CTRL (PAD_CTL_DSE6 | PAD_CTL_FSEL2 | PAD_CTL_PUE)
static iomux_v3_cfg_t const gpmi_pads[] = {
	IMX8MM_PAD_NAND_ALE_RAWNAND_ALE | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_CE0_B_RAWNAND_CE0_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_CLE_RAWNAND_CLE | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA00_RAWNAND_DATA00 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA01_RAWNAND_DATA01 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA02_RAWNAND_DATA02 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA03_RAWNAND_DATA03 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA04_RAWNAND_DATA04 | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA05_RAWNAND_DATA05	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA06_RAWNAND_DATA06	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA07_RAWNAND_DATA07	| MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_RE_B_RAWNAND_RE_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_READY_B_RAWNAND_READY_B | MUX_PAD_CTRL(NAND_PAD_READY0_CTRL),
	IMX8MM_PAD_NAND_WE_B_RAWNAND_WE_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
	IMX8MM_PAD_NAND_WP_B_RAWNAND_WP_B | MUX_PAD_CTRL(NAND_PAD_CTRL),
};

static void setup_gpmi_nand(void)
{
	imx_iomux_v3_setup_multiple_pads(gpmi_pads, ARRAY_SIZE(gpmi_pads));
}
#endif

int board_early_init_f(void)
{
	struct wdog_regs *wdog = (struct wdog_regs*) WDOG1_BASE_ADDR;

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));

	set_wdog_reset(wdog);
	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));

#ifdef CONFIG_NAND_MXS
	setup_gpmi_nand(); /* SPL will call the board_early_init_f */
#endif

	return 0;
}

/* Check board type */
int checkboard(void)
{
  struct fs_nboot_args *pargs = fs_board_get_nboot_args ();
  unsigned int board_type = fs_board_get_type ();
  unsigned int board_rev = fs_board_get_rev ();
  unsigned int features2;

  features2 = pargs->chFeatures2;

  printf ("Board: %s Rev %u.%02u (", board_info[board_type].name,
	  board_rev / 100, board_rev % 100);
  if ((features2 & FEAT2_ETH_MASK) == FEAT2_ETH_MASK)
    puts ("2x ");
  if (features2 & FEAT2_ETH_MASK)
    puts ("LAN, ");
  if (features2 & FEAT2_WLAN)
    puts ("WLAN, ");
  if (features2 & FEAT2_EMMC)
    puts ("eMMC, ");
  printf ("%dx DRAM)\n", pargs->dwNumDram);

  //fs_board_show_nboot_args(pargs);

  return 0;
}

#ifdef CONFIG_BOARD_POSTCLK_INIT
int board_postclk_init(void)
{
  /* TODO */
  return 0;
}
#endif

#ifdef CONFIG_OF_BOARD_SETUP
/* Do any additional board-specific device tree modifications */
int ft_board_setup(void *fdt, bd_t *bd)
{
  int offs;
  struct fs_nboot_args *pargs = fs_board_get_nboot_args ();

  /* Set bdinfo entries */
  offs = fs_fdt_path_offset (fdt, "/bdinfo");
  if (offs >= 0)
    {
      int id = 0;
      /* Set common bdinfo entries */
      fs_fdt_set_bdinfo (fdt, offs);

      /* MAC addresses */
      if (pargs->chFeatures2 & FEAT2_ETH_A)
	fs_fdt_set_macaddr (fdt, offs, id++);

      if (pargs->chFeatures2 & FEAT2_WLAN)
	fs_fdt_set_macaddr (fdt, offs, id++);
    }

  return 0;
}
#endif

#ifdef CONFIG_FEC_MXC

/* enet pads definition */
static iomux_v3_cfg_t const enet_pads_rgmii[] = {
    IMX8MM_PAD_ENET_MDIO_ENET1_MDIO | MUX_PAD_CTRL(ENET_PAD_CTRL),
    IMX8MM_PAD_ENET_MDC_ENET1_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL),
    IMX8MM_PAD_ENET_TXC_ENET1_RGMII_TXC | MUX_PAD_CTRL(ENET_PAD_CTRL),
    IMX8MM_PAD_ENET_TX_CTL_ENET1_RGMII_TX_CTL | MUX_PAD_CTRL(ENET_PAD_CTRL),
    IMX8MM_PAD_ENET_TD0_ENET1_RGMII_TD0 | MUX_PAD_CTRL(ENET_PAD_CTRL),
    IMX8MM_PAD_ENET_TD1_ENET1_RGMII_TD1 | MUX_PAD_CTRL(ENET_PAD_CTRL),
    IMX8MM_PAD_ENET_TD2_ENET1_RGMII_TD2 | MUX_PAD_CTRL(ENET_PAD_CTRL),
    IMX8MM_PAD_ENET_TD3_ENET1_RGMII_TD3 | MUX_PAD_CTRL(ENET_PAD_CTRL),
    IMX8MM_PAD_ENET_RXC_ENET1_RGMII_RXC | MUX_PAD_CTRL(ENET_PAD_CTRL),
    IMX8MM_PAD_ENET_RX_CTL_ENET1_RGMII_RX_CTL | MUX_PAD_CTRL(ENET_PAD_CTRL),
    IMX8MM_PAD_ENET_RD0_ENET1_RGMII_RD0 | MUX_PAD_CTRL(ENET_PAD_CTRL),
    IMX8MM_PAD_ENET_RD1_ENET1_RGMII_RD1 | MUX_PAD_CTRL(ENET_PAD_CTRL),
    IMX8MM_PAD_ENET_RD2_ENET1_RGMII_RD2 | MUX_PAD_CTRL(ENET_PAD_CTRL),
    IMX8MM_PAD_ENET_RD3_ENET1_RGMII_RD3 | MUX_PAD_CTRL(ENET_PAD_CTRL),

    /* Phy Interrupt */
    IMX8MM_PAD_GPIO1_IO04_GPIO1_IO4 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

#define FEC_RST_PAD IMX_GPIO_NR(1, 5)
static iomux_v3_cfg_t const fec1_rst_pads[] = {
    IMX8MM_PAD_GPIO1_IO05_GPIO1_IO5 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void setup_iomux_fec(void)
{
  imx_iomux_v3_setup_multiple_pads (enet_pads_rgmii,
				    ARRAY_SIZE (enet_pads_rgmii));

  imx_iomux_v3_setup_multiple_pads (fec1_rst_pads, ARRAY_SIZE (fec1_rst_pads));

  gpio_request (FEC_RST_PAD, "fec1_rst");
  gpio_direction_output (FEC_RST_PAD, 0);
  udelay (1000);
  gpio_direction_output (FEC_RST_PAD, 1);
}

static int setup_fec(void)
{
  struct iomuxc_gpr_base_regs *const iomuxc_gpr_regs =
      (struct iomuxc_gpr_base_regs*) IOMUXC_GPR_BASE_ADDR;

  setup_iomux_fec ();

  /* Use 125M anatop REF_CLK1 for ENET1, not from external */
  clrsetbits_le32 (&iomuxc_gpr_regs->gpr[1],
  IOMUXC_GPR_GPR1_GPR_ENET1_TX_CLK_SEL_SHIFT,
		   0);
  return set_clk_enet (ENET_125MHZ);
}

int board_phy_config(struct phy_device *phydev)
{
  /* enable rgmii rxc skew and phy mode select to RGMII copper */
  phy_write (phydev, MDIO_DEVAD_NONE, 0x1d, 0x1f);
  phy_write (phydev, MDIO_DEVAD_NONE, 0x1e, 0x8);

  phy_write (phydev, MDIO_DEVAD_NONE, 0x1d, 0x05);
  phy_write (phydev, MDIO_DEVAD_NONE, 0x1e, 0x100);

  if (phydev->drv->config)
    phydev->drv->config (phydev);
  return 0;
}
#endif

#define USB_GPIO_PAD_CTRL (PAD_CTL_HYS | PAD_CTL_DSE1)

#define USB_OTG_PWR_PAD IMX_GPIO_NR(1, 12)
static iomux_v3_cfg_t const usb_otg_pwr_pad =
		(IMX8MM_PAD_GPIO1_IO12_GPIO1_IO12 | MUX_PAD_CTRL(USB_GPIO_PAD_CTRL));

#define USB_HOST_PWR_PAD IMX_GPIO_NR(1, 14)
static iomux_v3_cfg_t const usb_host_pwr_pad
= (IMX8MM_PAD_GPIO1_IO14_GPIO1_IO14 | MUX_PAD_CTRL(USB_GPIO_PAD_CTRL));


int board_usb_init(int index, enum usb_init_type init)
{
  int ret = 0;

  debug("board_usb_init %d, type %d\n", index, init);

  switch(init)
  {
  case USB_INIT_HOST:
	  imx_iomux_v3_setup_pad(usb_host_pwr_pad);
	  gpio_request (USB_HOST_PWR_PAD, "USB_HOST_PWR");
	  /* vbus_detect for bddsi  */
	  gpio_direction_output (USB_HOST_PWR_PAD, 1);
	  break;
  case USB_INIT_DEVICE:
  default:
	  imx_iomux_v3_setup_pad(usb_otg_pwr_pad);
	  gpio_request (USB_HOST_PWR_PAD, "USB_OTG_PWR");
	  /* vbus_detect for bddsi  */
	  gpio_direction_output (USB_OTG_PWR_PAD, 1);
	  break;
  }

  imx8m_usb_power (index, true);

  return ret;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
  int ret = 0;

  debug("board_usb_cleanup %d, type %d\n", index, init);

  imx8m_usb_power (index, false);

  switch(init)
   {
   case USB_INIT_HOST:
 	  imx_iomux_v3_setup_pad(usb_host_pwr_pad);
 	  gpio_request (USB_HOST_PWR_PAD, "USB_HOST_PWR");
 	  /* vbus_detect for bddsi  */
 	  gpio_direction_output (USB_HOST_PWR_PAD, 0);
 	  break;
   case USB_INIT_DEVICE:
   default:
 	  imx_iomux_v3_setup_pad(usb_otg_pwr_pad);
 	  gpio_request (USB_HOST_PWR_PAD, "USB_OTG_PWR");
 	  /* vbus_detect for bddsi  */
 	  gpio_direction_output (USB_OTG_PWR_PAD, 0);
 	  break;
   }

  return ret;
}

static int pmic_init_board(void)
{
  struct udevice *bus, *pmic_dev;
  int i2c_bus = 3;
  int ret;
  uint8_t val[4] = { 0 };

#define BD71847_SLAVE_ADDR 0x4b

  ret = uclass_get_device_by_seq (UCLASS_I2C, i2c_bus, &bus);
  if (ret)
    {
      printf ("%s: No bus %d\n", __func__, i2c_bus);
      return ret;
    }

  ret = dm_i2c_probe (bus, BD71847_SLAVE_ADDR, 0, &pmic_dev);
  if (ret)
    {
      printf ("%s: Can't find device id=0x%x, on bus %d\n", __func__,
	      BD71847_SLAVE_ADDR, i2c_bus);
      return ret;
    }

  ret = dm_i2c_read (pmic_dev, 0x0, &val[0], 1);
  if (ret)
    {
      printf ("%s: Can't read chip rev. id=0x%x, on bus %d\n", __func__,
	      BD71847_SLAVE_ADDR, i2c_bus);
      return ret;
    }
  printf ("PMIC:  BD71847 Rev. 0x%x\n", val[0]);

  val[0] = 0;
  /* decrease RESET key long push time from the default 10s to 10ms */
  ret = dm_i2c_write (pmic_dev, BD71837_PWRONCONFIG1, &val[0], 1);
  if (ret)
    {
      printf ("%s: Can't decrease reset time %d.\n", __func__, ret);
      return ret;
    }

  val[0] = 1;

  /* unlock the PMIC regs */
  ret = dm_i2c_write (pmic_dev, BD71837_REGLOCK, &val[0], 1);
  if (ret)
    {
      printf ("%s: Can't write unlock device %d.\n", __func__, ret);
      return ret;
    }

  val[0] = 2;
  /* increase VDD_DRAM to 0.9v for 3Ghz DDR */
  ret = dm_i2c_write (pmic_dev, BD71837_BUCK5_VOLT, &val[0], 1);
  if (ret)
    {
      printf ("%s: Can't increase DRAM VDD %d.\n", __func__, ret);
      return ret;
    }

  val[0] = 0x11;
  /* lock the PMIC regs */
  ret = dm_i2c_write (pmic_dev, BD71837_REGLOCK, &val[0], 1);
  if (ret)
    {
      printf ("%s: Can't lock device %d\n", __func__, ret);
      return ret;
    }

  return 0;
}

int board_init(void)
{
  unsigned int board_type = fs_board_get_type ();

  /* Copy NBoot args to variables and prepare command prompt string */
  fs_board_init_common (&board_info[board_type]);

  pmic_init_board ();

	/*
	 * set rawnand root
	 * sys pll1 400M
	 */
	clock_enable(CCGR_RAWNAND, 0);
	clock_set_target_val(NAND_CLK_ROOT, CLK_ROOT_ON |
		CLK_ROOT_SOURCE_SEL(3) | CLK_ROOT_POST_DIV(CLK_ROOT_POST_DIV4)); /* 100M */
	clock_enable(CCGR_RAWNAND, 1);

#ifdef CONFIG_FEC_MXC
  setup_fec ();
#endif

  return 0;
}

int board_mmc_get_env_dev(int devno)
{
  return devno;
}

int mmc_map_to_kernel_blk(int devno)
{
  return devno + 1;
}

#define TC358764_ADDR 0xF

static int tc358764_i2c_reg_write(struct udevice *dev, uint addr, uint8_t *data, int length)
{
  int err;

  err = dm_i2c_write (dev, addr, data, length);
  return err;
}

static int tc358764_i2c_reg_read(struct udevice *dev, uint addr, uint8_t *data, int length)
{
  int err;

  err = dm_i2c_read (dev, addr, data, length);
  if (err)
    {
      return err;
    }
  return 0;
}

/* System registers */
#define SYS_RST			0x0504 /* System Reset */
#define SYS_ID			0x0580 /* System ID */

static int tc358764_init(void)
{
  struct udevice *bus = 0, *mipi2lvds_dev = 0;
  int i2c_bus = 3;
  int ret;
  uint8_t val[4] =
    { 0 };
  uint *uptr = (uint*) val;

  ret = uclass_get_device_by_seq (UCLASS_I2C, i2c_bus, &bus);
  if (ret)
    {
      printf ("%s: No bus %d\n", __func__, i2c_bus);
      return 1;
    }

  ret = dm_i2c_probe (bus, TC358764_ADDR, 0, &mipi2lvds_dev);
  if (ret)
    {
      printf ("%s: Can't find device id=0x%x, on bus %d, ret %d\n", __func__,
	      TC358764_ADDR, i2c_bus, ret);
      return 1;
    }

  /* offset */
  i2c_set_chip_offset_len (mipi2lvds_dev, 2);

  /* read chip/rev register with */
  tc358764_i2c_reg_read (mipi2lvds_dev, SYS_ID, val, sizeof(val));

  if (val[1] == 0x65)
    printf ("DSI2LVDS:  TC358764 Rev. 0x%x.\n", (uint8_t) (val[0] & 0xFF));
  else
    printf ("DSI2LVDS:  ID: 0x%x Rev. 0x%x.\n", (uint8_t) (val[1] & 0xFF),
	    (uint8_t) (val[0] & 0xFF));

  /* DSI Basic parameters. Have to be in LP mode...*/
#define PPI_TX_RX_TA 0x13C
  *uptr = 0x00010002; // BTA paramters
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_TX_RX_TA, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write PPI_TX_TA...\n", __func__);
      return 1;
    }

#define PPI_LPTXTIMCNT 0x114       
  *uptr = 0x00000001; // BTA paramters
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_LPTXTIMCNT, val,
				sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write PPI_LPTXTIMCNT...\n", __func__);
      return 1;
    }

#define PPI_D0S_CLRSIPOCOUNT 0x164      
  *uptr = 0x00000002; // BTA paramters
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_D0S_CLRSIPOCOUNT, val,
				sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write PPI_D0S_CLRSIPOCOUNT...\n", __func__);
      return 1;
    }
#define PPI_D1S_CLRSIPOCOUNT 0x168 
  *uptr = 0x00000002; // BTA paramters
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_D1S_CLRSIPOCOUNT, val,
				sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write PPI_D1S_CLRSIPOCOUNT...\n", __func__);
      return 1;
    }
#define PPI_D2S_CLRSIPOCOUNT 0x16C
  *uptr = 0x00000002; // BTA paramters
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_D2S_CLRSIPOCOUNT, val,
				sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write PPI_D2S_CLRSIPOCOUNT...\n", __func__);
      return 1;
    }
#define PPI_D3S_CLRSIPOCOUNT 0x170
  *uptr = 0x00000002; //4; // BTA paramters
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_D3S_CLRSIPOCOUNT, val,
				sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write PPI_D3S_CLRSIPOCOUNT...\n", __func__);
      return 1;
    }
#define PPI_LANEENABLE 0x134
  *uptr = 0x0000001F; // BTA paramters
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_LANEENABLE, val,
				sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write PPI_LANEENABLE...\n", __func__);
      return 1;
    }
#define DSI_LANEENABLE 0x210
  *uptr = 0x0000001F; // BTA paramters
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, DSI_LANEENABLE, val,
				sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write DSI_LANEENABLE...\n", __func__);
      return 1;
    }
#define PPI_SARTPPI 0x104
  *uptr = 0x00000001; // BTA paramters
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, PPI_SARTPPI, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write PPI_SARTPPI...\n", __func__);
      return 1;
    }

#define DSI_SARTPPI 0x204
  *uptr = 0x00000001; // BTA paramters
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, DSI_SARTPPI, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write DSI_SARTPPI...\n", __func__);
      return 1;
    }

  /* Timing and mode setting */
#define VPCTRL 0x450
  *uptr = 0x03F40120; // BTA paramters
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, VPCTRL, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write VPCTRL...\n", __func__);
      return 1;
    }

#define HTIM1 0x454
  *uptr = 0x002C0002;
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, HTIM1, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write HTIM1...\n", __func__);
      return 1;
    }

#define HTIM2 0x458
  *uptr = 0x00040320;
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, HTIM2, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write VPCTRL...\n", __func__);
      return 1;
    }

#define VTIM1 0x45C
  *uptr = 0x00150002;
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, VTIM1, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write VTIM1...\n", __func__);
      return 1;
    }
#define VTIM2 0x460
  *uptr = 0x000B01E0;
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, VTIM2, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write VTIM2...\n", __func__);
      return 1;
    }
#define VFUEN 0x464
  *uptr = 0x00000001;
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, VFUEN, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write VFUEN...\n", __func__);
      return 1;
    }
#define LVPHY0 0x4A0
  *uptr = 0x0044802D;
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVPHY0, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write LVPHY0...\n", __func__);
      return 1;
    }
  udelay (100);

  *uptr = 0x0004802D;
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVPHY0, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write LVPHY0...\n", __func__);
      return 1;
    }
#define SYSRST 0x504
  *uptr = 0x00000004;
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, SYSRST, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write SYSRST...\n", __func__);
      return 1;
    }

#define LVMX0003 0x0480
  *uptr = 0x03020100;
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVMX0003, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write LVMX0003...\n", __func__);
      return 1;
    }

#define LVMX0407 0x0484
  *uptr = 0x08050704;
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVMX0407, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write LVMX0407...\n", __func__);
      return 1;
    }

#define LVMX0811 0x0488
  *uptr = 0x0F0E0A09;
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVMX0811, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write LVMX0811...\n", __func__);
      return 1;
    }

#define LVMX1215 0x048C
  *uptr = 0x100D0C0B;
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVMX1215, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write LVMX1215...\n", __func__);
      return 1;
    }

#define LVMX1619 0x0490
  *uptr = 0x12111716;
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVMX1619, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write LVMX1619...\n", __func__);
      return 1;
    }

#define LVMX2023 0x0494
  *uptr = 0x1B151413;
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVMX2023, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write LVMX2023...\n", __func__);
      return 1;
    }

#define LVMX2427 0x0498
  *uptr = 0x061A1918;

  ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVMX2427, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write LVMX2427...\n", __func__);
      return 1;
    }

  /* LVDS enable */
#define LVCFG 0x49C
  *uptr = 0x00000031;
  ret = tc358764_i2c_reg_write (mipi2lvds_dev, LVCFG, val, sizeof(val));
  if (ret)
    {
      printf ("%s: Can't write LVCFG...\n", __func__);
      return 1;
    }

  return 0;
}

#ifdef CONFIG_VIDEO_MXS


#ifdef CONFIG_IMX_SEC_MIPI_DSI
static const struct sec_mipi_dsim_plat_data imx8mm_mipi_dsim_plat_data = {
	.version	= 0x1060200,
	.max_data_lanes = 4,
	.max_data_rate  = 1500000000ULL,
	.reg_base = MIPI_DSI_BASE_ADDR,
	.gpr_base = CSI_BASE_ADDR + 0x8000,
};

#define DISPLAY_MIX_SFT_RSTN_CSR		0x00
#define DISPLAY_MIX_CLK_EN_CSR		0x04

   /* 'DISP_MIX_SFT_RSTN_CSR' bit fields */
#define BUS_RSTN_BLK_SYNC_SFT_EN	BIT(6)

   /* 'DISP_MIX_CLK_EN_CSR' bit fields */
#define LCDIF_PIXEL_CLK_SFT_EN		BIT(7)
#define LCDIF_APB_CLK_SFT_EN		BIT(6)

void disp_mix_bus_rstn_reset(ulong gpr_base, bool reset)
{
  if (!reset)
    /* release reset */
    setbits_le32 (gpr_base + DISPLAY_MIX_SFT_RSTN_CSR,
		  BUS_RSTN_BLK_SYNC_SFT_EN);
  else
    /* hold reset */
    clrbits_le32 (gpr_base + DISPLAY_MIX_SFT_RSTN_CSR,
		  BUS_RSTN_BLK_SYNC_SFT_EN);
}

void disp_mix_lcdif_clks_enable(ulong gpr_base, bool enable)
{
  if (enable)
    /* enable lcdif clks */
    setbits_le32 (gpr_base + DISPLAY_MIX_CLK_EN_CSR,
		  LCDIF_PIXEL_CLK_SFT_EN | LCDIF_APB_CLK_SFT_EN);
  else
    /* disable lcdif clks */
    clrbits_le32 (gpr_base + DISPLAY_MIX_CLK_EN_CSR,
		  LCDIF_PIXEL_CLK_SFT_EN | LCDIF_APB_CLK_SFT_EN);
}

struct mipi_dsi_client_dev tc358764_dev = {
	.channel	= 0,
	.lanes = 4,
	.format  = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_VIDEO_AUTO_VERT,
	.name = "TC358764",
};

struct mipi_dsi_client_dev g050tan01_dev = {
	.channel	= 0,
	.lanes = 4,
	.format  = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO
                | MIPI_DSI_MODE_EOT_PACKET | MIPI_DSI_MODE_VIDEO_SYNC_PULSE,
};

#define FSL_SIP_GPC			0xC2000000
#define FSL_SIP_CONFIG_GPC_PM_DOMAIN	0x3
#define DISPMIX				9
#define MIPI				10


#define BL_ON_PAD IMX_GPIO_NR(5, 3)
static iomux_v3_cfg_t const bl_on_pads[] = {
	IMX8MM_PAD_SPDIF_TX_GPIO5_IO3 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

#define VLCD_ON_PAD IMX_GPIO_NR(4, 28)
static iomux_v3_cfg_t const vlcd_on_pads[] = {
	IMX8MM_PAD_SAI3_RXFS_GPIO4_IO28 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

#define LVDS_RST_PAD IMX_GPIO_NR(1, 13)
static iomux_v3_cfg_t const lvds_rst_pads[] = {
	IMX8MM_PAD_GPIO1_IO13_GPIO1_IO13 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

void enable_tc358764(struct display_info_t const *dev)
{
  int ret = 0;

  mxs_set_lcdclk(dev->bus, PICOS2KHZ(dev->mode.pixclock));

  clock_set_target_val (IPP_DO_CLKO2, CLK_ROOT_ON
	  | CLK_ROOT_SOURCE_SEL(1) | CLK_ROOT_POST_DIV(CLK_ROOT_POST_DIV6));

  imx_iomux_v3_setup_multiple_pads (lvds_rst_pads, ARRAY_SIZE (lvds_rst_pads));

  gpio_request (LVDS_RST_PAD, "LVDS_RST");
  gpio_direction_output (LVDS_RST_PAD, 0);
  /* period of reset signal > 50 ns */
  udelay (5);
  gpio_direction_output (LVDS_RST_PAD, 1);
  udelay (500);

  /* enable the dispmix & mipi phy power domain */
  call_imx_sip (FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, DISPMIX, true, 0);
  call_imx_sip (FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, MIPI, true, 0);

  /* Put lcdif out of reset */
  disp_mix_bus_rstn_reset (imx8mm_mipi_dsim_plat_data.gpr_base, false);
  disp_mix_lcdif_clks_enable (imx8mm_mipi_dsim_plat_data.gpr_base, true);

  /* Setup mipi dsim */
  ret = sec_mipi_dsim_setup (&imx8mm_mipi_dsim_plat_data);

  if (ret)
    return;

  ret = imx_mipi_dsi_bridge_attach (&tc358764_dev); /* attach tc358764 device */
}

int detect_tc358764(struct display_info_t const *dev)
{
  struct fs_nboot_args *pargs = fs_board_get_nboot_args();
  unsigned int features2;

  features2 = pargs->chFeatures2;

  /* if LVDS controller is equipped  */
  if((features2 >> 7) & 1) {
    return 1;
  }

  return 0;
}

int detect_mipi_disp(struct display_info_t const *dev)
{
  struct fs_nboot_args *pargs = fs_board_get_nboot_args();
  unsigned int features2;

  features2 = pargs->chFeatures2;
  
  /* if LVDS controller is equipped  */
  if((features2 >> 7) & 1) {
    return 0;
  }

  return 1;
}

void enable_mipi_disp(struct display_info_t const *dev)
{
  /* enable the dispmix & mipi phy power domain */
  call_imx_sip (FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, DISPMIX, true, 0);
  call_imx_sip (FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, MIPI, true, 0);

  /* Put lcdif out of reset */
  disp_mix_bus_rstn_reset (imx8mm_mipi_dsim_plat_data.gpr_base, false);
  disp_mix_lcdif_clks_enable (imx8mm_mipi_dsim_plat_data.gpr_base, true);

  /* Setup mipi dsim */
  sec_mipi_dsim_setup (&imx8mm_mipi_dsim_plat_data);

  nt35521_init ();
  g050tan01_dev.name = displays[0].mode.name;
  imx_mipi_dsi_bridge_attach (&g050tan01_dev); /* attach g050tan01 device */
}

void board_quiesce_devices(void)
{
  gpio_request (IMX_GPIO_NR(1, 13), "DSI EN");
  gpio_direction_output (IMX_GPIO_NR(1, 13), 0);
}

#endif // end of mipi


struct display_info_t const displays[] = {
{
	.bus = LCDIF_BASE_ADDR,
	.addr = 0,
	.pixfmt = 24,
	.detect = detect_mipi_disp,
	.enable	= enable_mipi_disp,
	.mode	= {
		.name			= "NT35521_OLED",
		.refresh		= 60,
		.xres			= 720,
		.yres			= 1280,
		.pixclock		= 12830, // 10^12/freq
		.left_margin	= 72,
		.right_margin	= 56,
		.hsync_len		= 128,
		.upper_margin	= 38,
		.lower_margin	= 3,
		.vsync_len		= 10,
		.sync			= FB_SYNC_EXT,
		.vmode			= FB_VMODE_NONINTERLACED
	}
},
{
	.bus = LCDIF_BASE_ADDR,
	.addr = 0,
	.pixfmt = 24,
	.detect = detect_tc358764,
	.enable	= enable_tc358764,
	.mode	= {
		.name			= "TC358764",
		.refresh		= 60,
		.xres			= 800,
		.yres			= 480,
		.pixclock		= 29850, // 10^12/freq 
		.left_margin	= 10,
		.right_margin	= 37,
		.hsync_len	= 128,
		.upper_margin	= 23,
		.lower_margin	= 11,
		.vsync_len		= 2,
		.sync			= FB_SYNC_EXT,
		.vmode			= FB_VMODE_NONINTERLACED
	}
},
};
size_t display_count = ARRAY_SIZE(displays);
#endif

void board_late_mmc_env_init(void)
{
}

int board_late_init(void)
{
  /* Remove 'fdtcontroladdr' env. because we are using
   * compiled-in version. In this case it is not possible
   * to use this env. as saved in NAND flash. (s. readme for fdt control)
   *  */
  env_set("fdtcontroladdr", "");
  /* Set up all board specific variables */
  fs_board_late_init_common ();

#ifdef CONFIG_ENV_IS_IN_MMC
	board_late_mmc_env_init();
#endif

  imx_iomux_v3_setup_multiple_pads (vlcd_on_pads, ARRAY_SIZE (bl_on_pads));
  /* backlight off */
  gpio_request (BL_ON_PAD, "BL_ON");
  gpio_direction_output (BL_ON_PAD, 0);

  imx_iomux_v3_setup_multiple_pads (vlcd_on_pads, ARRAY_SIZE (vlcd_on_pads));

  if(detect_tc358764(0))
  {
	  /* initialize TC358764 over I2C */
	  if(tc358764_init())
		/* error case... */
		return 0;
  }

  /* set vlcd on*/
  gpio_request (VLCD_ON_PAD, "VLCD_ON");
  gpio_direction_output (VLCD_ON_PAD, 0);
  /* backlight on */
  gpio_direction_output (BL_ON_PAD, 1);

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
