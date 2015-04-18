/*
 * National Semiconductor PHY drivers
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * Copyright 2010-2011 Freescale Semiconductor, Inc.
 * author Andy Fleming
 */
#include <phy.h>

/* NatSemi DP83630 */

#define DP83630_PHY_PAGESEL_REG		0x13
#define DP83630_PHY_PTP_COC_REG		0x14
#define DP83630_PHY_PTP_CLKOUT_EN	(1<<15)
#define DP83630_PHY_RBR_REG		0x17

static int dp83630_config(struct phy_device *phydev)
{
	int ptp_coc_reg;

	phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, BMCR_RESET);
	phy_write(phydev, MDIO_DEVAD_NONE, DP83630_PHY_PAGESEL_REG, 0x6);
	ptp_coc_reg = phy_read(phydev, MDIO_DEVAD_NONE,
			       DP83630_PHY_PTP_COC_REG);
	ptp_coc_reg &= ~DP83630_PHY_PTP_CLKOUT_EN;
	phy_write(phydev, MDIO_DEVAD_NONE, DP83630_PHY_PTP_COC_REG,
		  ptp_coc_reg);
	phy_write(phydev, MDIO_DEVAD_NONE, DP83630_PHY_PAGESEL_REG, 0);

	genphy_config_aneg(phydev);

	return 0;
}

static struct phy_driver DP83630_driver = {
	.name = "NatSemi DP83630",
	.uid = 0x20005ce1,
	.mask = 0xfffffff0,
	.features = PHY_BASIC_FEATURES,
	.config = &dp83630_config,
	.startup = &genphy_startup,
	.shutdown = &genphy_shutdown,
};


/* DP83865 Link and Auto-Neg Status Register */
#define MIIM_DP83865_LANR      0x11
#define MIIM_DP83865_SPD_MASK  0x0018
#define MIIM_DP83865_SPD_1000  0x0010
#define MIIM_DP83865_SPD_100   0x0008
#define MIIM_DP83865_DPX_FULL  0x0002


/* NatSemi DP83865 */
static int dp83865_config(struct phy_device *phydev)
{
	phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, BMCR_RESET);
	genphy_config_aneg(phydev);

	return 0;
}

static int dp83865_parse_status(struct phy_device *phydev)
{
	int mii_reg;

	mii_reg = phy_read(phydev, MDIO_DEVAD_NONE, MIIM_DP83865_LANR);

	switch (mii_reg & MIIM_DP83865_SPD_MASK) {

	case MIIM_DP83865_SPD_1000:
		phydev->speed = SPEED_1000;
		break;

	case MIIM_DP83865_SPD_100:
		phydev->speed = SPEED_100;
		break;

	default:
		phydev->speed = SPEED_10;
		break;

	}

	if (mii_reg & MIIM_DP83865_DPX_FULL)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;

	return 0;
}

static int dp83865_startup(struct phy_device *phydev)
{
	genphy_update_link(phydev);
	dp83865_parse_status(phydev);

	return 0;
}


static struct phy_driver DP83865_driver = {
	.name = "NatSemi DP83865",
	.uid = 0x20005c70,
	.mask = 0xfffffff0,
	.features = PHY_GBIT_FEATURES,
	.config = &dp83865_config,
	.startup = &dp83865_startup,
	.shutdown = &genphy_shutdown,
};


/* NatSemi DP83484 */
#define MIIM_DP83484_RBR 0x17
#define MIIM_DP83484_PHYCR 0x19

static int dp83484_config(struct phy_device *phydev)
{
	int reg;

#if 0
	/* Switch to RMII rev. 1.0 (instead of rev. 1.2) */
	reg = phy_read(phydev, MDIO_DEVAD_NONE, MIIM_DP83484_RBR);
	reg |= 0x10;
	phy_write(phydev, MDIO_DEVAD_NONE, MIIM_DP83484_RBR, reg);
#endif

	/* LED Mode 2: Show ethernet activity on LINK LED */
	reg = phy_read(phydev, MDIO_DEVAD_NONE, MIIM_DP83484_PHYCR);
	reg &= ~0x20;
	phy_write(phydev, MDIO_DEVAD_NONE, MIIM_DP83484_PHYCR, reg);

	genphy_config(phydev);

	return 0;
}

static struct phy_driver DP83484_driver = {
	.name = "NatSemi DP83484",
	.uid = 0x20005c90,
	.mask = 0xfffffff0,
	.features = PHY_BASIC_FEATURES,
	.config = &dp83484_config,
	.startup = &genphy_startup,
	.shutdown = &genphy_shutdown,
};

int phy_natsemi_init(void)
{
	phy_register(&DP83630_driver);
	phy_register(&DP83865_driver);
	phy_register(&DP83484_driver);

	return 0;
}
