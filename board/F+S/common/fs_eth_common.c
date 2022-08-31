/*
 * fs_eth_common.c
 *
 * (C) Copyright 2018
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Common ETH code used on F&S boards
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>

#ifdef CONFIG_CMD_NET

#include <common.h>			/* Types */
#include <net.h>			/* eth_env_get_enetaddr_by_index() */
#include <asm/io.h>			/* __raw_readl() */

/* Read a MAC address from OTP memory */
static int get_otp_mac(void *otp_addr, uchar *enetaddr)
{
	u32 val;
	static const uchar empty1[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	static const uchar empty2[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	/*
	 * Read a MAC address from OTP memory on i.MX6; it is stored in the
	 * following order:
	 *
	 *   Byte 1 in mac_h[31:24]
	 *   Byte 2 in mac_h[23:16]
	 *   Byte 3 in mac_h[15:8]
	 *   Byte 4 in mac_h[7:0]
	 *   Byte 5 in mac_l[31:24]
	 *   Byte 6 in mac_l[23:16]
	 *
	 * Please note that this layout is different to Vybrid.
	 *
	 * The MAC address itself can be empty (all six bytes zero) or erased
	 * (all six bytes 0xFF). In this case the whole address is ignored.
	 *
	 * In addition to the address itself, there may be a count stored in
	 * mac_l[7:0].
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
	enetaddr[0] = val >> 24;
	enetaddr[1] = (val >> 16) & 0xFF;
	enetaddr[2] = (val >> 8) & 0xFF;
	enetaddr[3] = val & 0xFF;

	val = __raw_readl(otp_addr + 0x10);
	enetaddr[4] = val >> 24;
	enetaddr[5] = (val >> 16) & 0xFF;

	if (!memcmp(enetaddr, empty1, 6) || !memcmp(enetaddr, empty2, 6))
		return 0;

	val &= 0xFF;
	if (val == 0xFF)
		val = 0;

	return (int)(val + 1);
}


/* Set the ethaddr environment variable according to index */
void fs_eth_set_ethaddr(int index)
{
	struct ocotp_regs *ocotp = (struct ocotp_regs *)OCOTP_BASE_ADDR;
#if defined(CONFIG_ARCH_IMX8M)
	struct fuse_bank *bank = &ocotp->bank[9];
#else
	struct fuse_bank *bank = &ocotp->bank[4];
#endif
	uchar enetaddr[6];
	int count, i;
	int offs = index;

	/*
	 * Try to fulfil the request in the following order:
	 *   1. From environment variable
	 *   2. MAC0 from OTP
	 *   3. CONFIG_ETHADDR_BASE
	 */
	if (eth_env_get_enetaddr_by_index("eth", index, enetaddr))
		return;

	/* To understand how the offsets comes about, here is a small
	 * description.
	 * Every word between one bank has an offset of 0x10.
	 * For i.MX6* there are 8 words each bank.
	 * For i.MX8M* there are 4 words each bank.
	 * For each word there are 4 32-bit values. But only the first
	 * 32-bit entry holds the value, the other 32-bit entrys are reserved.
	 * For example we need word 2, we need fuse_regs[8] because
	 * word 0 is fuse_regs[0], word 1 is fuse_regs[4] and word 2 is
	 * fuse_regs[8].
	 */
#if defined(CONFIG_ARCH_IMX8M)
	count = get_otp_mac(&bank->fuse_regs[0], enetaddr);
#else
	count = get_otp_mac(&bank->fuse_regs[8], enetaddr);
#endif
	if (count <= offs) {
		offs -= count;
		eth_parse_enetaddr(CONFIG_ETHADDR_BASE, enetaddr);
	}

	i = 6;
	do {
		offs += (int)enetaddr[--i];
		enetaddr[i] = offs & 0xFF;
		offs >>= 8;
	} while (i);

	eth_env_set_enetaddr_by_index("eth", index, enetaddr);
}

#endif /* CONFIG_CMD_NET */
