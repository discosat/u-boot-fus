/*
 * (C) Copyright 2011 Freescale Semiconductor, Inc
 * Andy Fleming
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * MDIO Commands
 */

#include <common.h>
#include <command.h>
#include <miiphy.h>
#include <phy.h>

static int mdio_write_ranges(struct phy_device *phydev, struct mii_dev *bus,
			     int addrlo,
			     int addrhi, int devadlo, int devadhi,
			     int reglo, int reghi, unsigned short data,
			     int extended)
{
	int addr, devad, reg;
	int err;

	for (addr = addrlo; addr <= addrhi; addr++) {
		for (devad = devadlo; devad <= devadhi; devad++) {
			for (reg = reglo; reg <= reghi; reg++) {
				if (!extended)
					err = bus->write(bus, addr, devad,
							 reg, data);
				else
					err = phydev->drv->writeext(phydev,
							addr, devad, reg, data);

				if (err)
					return err;
			}
		}
	}

	return 0;
}

static int mdio_read_ranges(struct phy_device *phydev, struct mii_dev *bus,
			    int addrlo,
			    int addrhi, int devadlo, int devadhi,
			    int reglo, int reghi, int extended)
{
	int addr, devad, reg;

	printf("Reading from bus %s\n", bus->name);
	for (addr = addrlo; addr <= addrhi; addr++) {
		printf("PHY at address 0x%02x:\n", addr);

		for (devad = devadlo; devad <= devadhi; devad++) {
			for (reg = reglo; reg <= reghi; reg++) {
				int val;

				if (!extended)
					val = bus->read(bus, addr, devad, reg);
				else
					val = phydev->drv->readext(phydev, addr,
						devad, reg);

				if (val < 0)
					return val;

				putc(' ');
				if (devad >= 0)
					printf("0x%02x.", devad);

				printf("0x%02x: 0x%x\n", reg, val & 0xffff);
			}
		}
	}

	return 0;
}

static int extract_range(char *input, int *plo, int *phi)
{
	char *end;
	*plo = simple_strtol(input, &end, 16);
	if (end == input)
		return CMD_RET_USAGE;
	if ((*plo < 0) || (*plo > 31))
		return CMD_RET_FAILURE;

	if ((*end == '-') && *(++end))
		*phi = simple_strtol(end, NULL, 16);
	else if (*end == '\0')
		*phi = *plo;
	else
		return CMD_RET_USAGE;

	if ((*phi < 0) || (*phi > 31))
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

/* The register will be in the form [a[-b].]x[-y] */
static int extract_reg_range(char *input, int *devadlo, int *devadhi,
			     int *reglo, int *reghi)
{
	char *regstr;

	/* use strrchr to find the last string after a '.' */
	regstr = strrchr(input, '.');

	/* If it exists, extract the devad(s) */
	if (regstr) {
		char devadstr[32];
		int ret;

		strncpy(devadstr, input, regstr - input);
		devadstr[regstr - input] = '\0';

		ret = extract_range(devadstr, devadlo, devadhi);
		if (ret)
			return ret;

		regstr++;
	} else {
		/* Otherwise, we have no devad, and we just got regs */
		*devadlo = *devadhi = MDIO_DEVAD_NONE;

		regstr = input;
	}

	return extract_range(regstr, reglo, reghi);
}

/* ---------------------------------------------------------------- */
static int do_mdio(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char *op;
	int addrlo = 0, addrhi = 0, reglo = 0, reghi = 0;
	int devadlo = 0, devadhi = 0;
	unsigned short data = 0;
	struct mii_dev *bus;
	struct phy_device *phydev = NULL;
	int extended = 0;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	op = argv[1];

	switch (op[0]) {
	case 'l':
		mdio_list_devices();

		return 0;

	case 'w':
		if (argc > 1)
			data = simple_strtoul(argv[--argc], NULL, 16);
		/* Fall through to case 'r' */
	case 'r':
		if ((argc < 4) || (argc > 5))
			return CMD_RET_USAGE;

		ret = extract_reg_range(argv[--argc], &devadlo, &devadhi,
					&reglo, &reghi);
		if (ret)
			return ret;

		if (argc == 4) {
			/* MII bus plus PHY address */
			bus = miiphy_get_dev_by_name(argv[2]);
			if (!bus) {
				printf("No such MII bus: %s\n", argv[2]);
				return CMD_RET_FAILURE;
			}
			ret = extract_range(argv[3], &addrlo, &addrhi);
		} else {		/* argc == 3 */
			/* Ethernet name or just PHY address on current bus */
			phydev = mdio_phydev_for_ethname(argv[2]);
			if (phydev) {
				bus = phydev->bus;
				addrlo = phydev->addr;
				addrhi = addrlo;
			} else {
				bus = mdio_get_current_dev();
				ret = extract_range(argv[2], &addrlo, &addrhi);
			}
		}
		if (ret)
			return ret;
		break;

	default:
		return CMD_RET_USAGE;
	}

	/* Only 'r' or 'w' here */
	if (op[1] == 'x') {
		if (!phydev || !phydev->drv ||
		    (!phydev->drv->writeext && (op[0] == 'w')) ||
		    (!phydev->drv->readext && (op[0] == 'r'))) {
			puts("PHY does not have extended functions\n");
			return CMD_RET_FAILURE;
		}
		extended = 1;
	}

	/* Save the chosen bus */
	miiphy_set_current_dev(bus->name);

	if (op[0] == 'w') {
		ret = mdio_write_ranges(phydev, bus, addrlo, addrhi, devadlo,
					devadhi, reglo, reghi, data, extended);
	} else {
		ret = mdio_read_ranges(phydev, bus, addrlo, addrhi, devadlo,
				       devadhi, reglo, reghi, extended);
	}
	if (ret) {
		printf("Error %d\n", ret);
		return CMD_RET_FAILURE;
	}

	return 0;
}

/***************************************************/

U_BOOT_CMD(
	mdio,	6,	1,	do_mdio,
	"MDIO utility commands",
	"list			- List MDIO buses\n"
	"mdio read <phydev> [<devad>.]<reg> - "
		"read PHY's register at <devad>.<reg>\n"
	"mdio write <phydev> [<devad>.]<reg> <data> - "
		"write PHY's register at <devad>.<reg>\n"
	"mdio rx <phydev> [<devad>.]<reg> - "
		"read PHY's extended register at <devad>.<reg>\n"
	"mdio wx <phydev> [<devad>.]<reg> <data> - "
		"write PHY's extended register at <devad>.<reg>\n"
	"<phydev> may be:\n"
	"   <busname>  <addr>\n"
	"   <addr>\n"
	"   <eth name>\n"
	"<addr> <devad>, and <reg> may be ranges, e.g. 1-5.4-0x1f.\n"
);
