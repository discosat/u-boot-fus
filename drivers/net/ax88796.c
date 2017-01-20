/*
 * (c) 2017 Hartmut Keller, F&S Elektronik Systeme GmbH <keller@fs-net.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>			/* CONFIG_* */
#include <asm/io.h>			/* readw(), readb(), writeb(), ... */
#include <malloc.h>			/* free() */
#include <net.h>			/* struct eth_device, eth_register() */
#include <netdev.h>			/* ax88796_initialize() */

#ifdef CONFIG_PHYLIB
#include <miiphy.h>			/* mdio_alloc(), mdio_register() */
#include <phy.h>			/* phy_connect_dev(), phy_config() */
#endif

#define AX88796_DEV_NAME "AX88796"

/* AX88796 has RAM from 0x4000 to 0x7FFF, i.e. pages 0x40 to 0x80-1 */
#define TX_START	0x40		/* First page of TX buffer */
#define RX_START	(TX_START + 6)
#define RX_END		0x80

/* ------------------------------------------------------------------------ */

/* Page 0 register offsets */
#define AX_CR		0x00	/* Command register */
#define AX_PSTART	0x01	/* Receive ring start page */
#define AX_PSTOP	0x02	/* Receive ring stop page */
#define AX_BNDRY	0x03	/* Receive ring boundary page */
#define AX_TSR		0x04	/* Transmit status register (read) */
#define AX_TPSR		0x04	/* Transmit page start address (write) */
#define AX_NCR		0x05	/* Number of collisions (read) */
#define AX_TBCL		0x05	/* Transmit byte count [7:0] (write) */
#define AX_CURP		0x06	/* Current page (read-only) */
#define AX_TBCH		0x06	/* Transmit byte count [15:8] (write) */
#define AX_ISR		0x07	/* Interrupt status register */
#define AX_CRDA0	0x08	/* Current remote DMA address [7:0] (read) */
#define AX_RSAL		0x08	/* Remote DMA start address [7:0] (write) */
#define AX_CRDA1	0x09	/* Current remote DMA address [15:8] (read) */
#define AX_RSAH		0x09	/* Remote DMA start address [15:8] (write) */
#define AX_RBCL		0x0a	/* Remote DMA byte count [7:0] (write) */
#define AX_RBCH		0x0b	/* Remote DMA byte count (15:8] (write) */
#define AX_RSR		0x0c	/* Receive status register (read) */
#define AX_RCR		0x0c	/* Receive configuration register (write) */
#define AX_FER		0x0d	/* Number of frame alignment errors (read) */
#define AX_TCR		0x0d	/* Transmit configuration register (write) */
#define AX_CER		0x0e	/* Number of CRC errors (read) */
#define AX_DCR		0x0e	/* Data configuration register (write) */
#define AX_MISSED	0x0f	/* Number of missed frames (read) */
#define AX_IMR		0x0f	/* Interrupt mask register (write) */

/* Page 1 register offsets */
#define AX_P1_CR	0x00	/* Command register */
#define AX_P1_PAR0	0x01	/* Physical address 0 */
#define AX_P1_PAR1	0x02	/* Physical address 1 */
#define AX_P1_PAR2	0x03	/* Physical address 2 */
#define AX_P1_PAR3	0x04	/* Physical address 3 */
#define AX_P1_PAR4	0x05	/* Physical address 4 */
#define AX_P1_PAR5	0x06	/* Physical address 5 */
#define AX_P1_CURP	0x07	/* Current page (read/write) */
#define AX_P1_MAR0	0x08	/* Multicast address 0 */
#define AX_P1_MAR1	0x09	/* Multicast address 1 */
#define AX_P1_MAR2	0x0a	/* Multicast address 2 */
#define AX_P1_MAR3	0x0b	/* Multicast address 3 */
#define AX_P1_MAR4	0x0c	/* Multicast address 4 */
#define AX_P1_MAR5	0x0d	/* Multicast address 5 */
#define AX_P1_MAR6	0x0e	/* Multicast address 6 */
#define AX_P1_MAR7	0x0f	/* Multicast address 7 */

/* Registers of pages 2 and 3 are not needed and therefore not listed */

#define AX_DATA		0x10	/* Data port (8 or 16 bits) */
#define AX_MEMR		0x14	/* MII/EEPROM Management Register */

/* Command register - common to all pages */
#define AX_CR_STOP	0x01	/* Stop: software reset */
#define AX_CR_START	0x02	/* Start: initialize device */
#define AX_CR_TXPKT	0x04	/* Transmit packet */
#define AX_CR_RDMA	0x08	/* Remote read DMA (get data from device) */
#define AX_CR_WDMA	0x10	/* Remote write DMA (put data to device) */
#define AX_CR_NODMA	0x20	/* Remote (or no) DMA */
#define AX_CR_PAGE0	0x00	/* Register page select */
#define AX_CR_PAGE1	0x40
#define AX_CR_PAGE2	0x80
#define AX_CR_PAGE3	0xC0
#define AX_CR_PAGEMSK	0x3F	/* Used to mask out page bits */

/* Data configuration register */
#define AX_DCR_WTS	0x01	/* 1=16 bit word transfers */

#define AX_DCR_INIT	0x00

/* Interrupt status register */
#define AX_ISR_RxP	0x01	/* Packet received */
#define AX_ISR_TxP	0x02	/* Packet transmitted */
#define AX_ISR_RxE	0x04	/* Receive error */
#define AX_ISR_TxE	0x08	/* Transmit error */
#define AX_ISR_OFLW	0x10	/* Receive overflow */
#define AX_ISR_CNT	0x20	/* Tally counters need emptying */
#define AX_ISR_RDC	0x40	/* Remote DMA complete */
#define AX_ISR_RESET	0x80	/* Device has reset (shutdown, error) */

/* Interrupt mask register */
#define AX_IMR_RxP	0x01	/* Packet received */
#define AX_IMR_TxP	0x02	/* Packet transmitted */
#define AX_IMR_RxE	0x04	/* Receive error */
#define AX_IMR_TxE	0x08	/* Transmit error */
#define AX_IMR_OFLW	0x10	/* Receive overflow */
#define AX_IMR_CNT	0x20	/* Tall counters need emptying */
#define AX_IMR_RDC	0x40	/* Remote DMA complete */

#define AX_IMR_All	0x3F	/* Everything but remote DMA */

/* Receiver control register */
#define AX_RCR_SEP	0x01	/* Save bad(error) packets */
#define AX_RCR_AR	0x02	/* Accept runt packets */
#define AX_RCR_AB	0x04	/* Accept broadcast packets */
#define AX_RCR_AM	0x08	/* Accept multicast packets */
#define AX_RCR_PROM	0x10	/* Promiscuous mode */
#define AX_RCR_MON	0x20	/* Monitor mode - 1=accept no packets */

/* Receiver status register */
#define AX_RSR_RxP	0x01	/* Packet received */
#define AX_RSR_CRC	0x02	/* CRC error */
#define AX_RSR_FRAME	0x04	/* Frame alignment error */
#define AX_RSR_MISS	0x10	/* Missed packet */
#define AX_RSR_PHY	0x20	/* 0=pad match, 1=mad match */
#define AX_RSR_DIS	0x40	/* Receiver disabled */

/* Transmitter control register */
#define AX_TCR_NOCRC	0x01	/* 1=inhibit CRC */
#define AX_TCR_NORMAL	0x00	/* Normal transmitter operation */
#define AX_TCR_LOCAL	0x02	/* Internal NIC loopback */
#define AX_TCR_PHYLOOP	0x04	/* PHY loopback */
#define AX_TCR_RLO	0x20	/* Retry late collision packets */
#define AX_TCR_NOPAD	0x40	/* No automatic padding of small packets */
#define AX_TCR_FDU	0x80	/* Full duplex mode (only for ext. PHY) */

/* Transmit status register */
#define AX_TSR_TxP	0x01	/* Packet transmitted */
#define AX_TSR_COL	0x04	/* Collision (at least one) */
#define AX_TSR_ABT	0x08	/* Aborted because of too many collisions */
#define AX_TSR_OWC	0x80	/* Collision outside normal window */

/* MII/EEPROM management register */
#define AX_MEMR_MDC	0x01	/* MD clock */
#define AX_MEMR_DIRIN	0x02	/* Data direction in */
#define AX_MEMR_MDI	0x04	/* MD input */
#define AX_MEMR_MDO	0x08	/* MD output */
#define AX_MEMR_MDMASK	0x0F	/* Mask for all MDIO bits */
#define AX_MEMR_EECS	0x10	/* EEPROM chip select */
#define AX_MEMR_EEI	0x20	/* EEPROM input */
#define AX_MEMR_EEO	0x40	/* EEPROM output */
#define AX_MEMR_EECK	0x80	/* EEPROM clock */
#define AX_MEMR_EEMASK  0xF0	/* Mask for all EEPROM bits */

/* Receive header status */
#define RCV_HDR_GOOD	0x01	/* Good packet */
#define RCV_HDR_CRCE	0x02	/* CRC error */
#define RCV_HDR_AE	0x04	/* Alignment error */
#define RCV_HDR_MIIE	0x08	/* MII error */
#define RCV_HDR_RUNT	0x10	/* Runt packet */
#define RCV_HDR_MB	0x20	/* Multicast or broadcast packet */

#define IEEE_8023_MAX_FRAME	1518	/* Largest possible ethernet frame */
#define IEEE_8023_MIN_FRAME	64	/* Smallest possible ethernet frame */

#define RX_BUFFER_SIZE	(IEEE_8023_MAX_FRAME + 18)

/* Timeout for transmission (seconds) */
#define TX_TIMEOUT	(5 * CONFIG_SYS_HZ)

/* Macros to read/write register */
#define AX_IN(ax, reg) ax->in(ax->base_addr, reg)
#define AX_OUT(ax, reg, val) ax->out(ax->base_addr, reg, val)

/* Driver specific information */
struct ax88796_priv_data {
	void *base_addr;		/* Registers */
	int rx_next;			/* First free Rx page */
	int mode;			/* Bus width, data port width */
	bool tx_running;		/* Transmission in progress */
	u8 tsr;				/* Transmit status */

	/* Function to read from a register */
	u8 (*in)(void *base, unsigned int reg);

	/* Function to write to a register */
	void (*out)(void *base, unsigned int reg, u8 val);

	/* Function to read data from data port */
	void (*data_in)(struct ax88796_priv_data *ax, u8 *buf, int len);

	/* Function to write data to data port */
	void (*data_out)(struct ax88796_priv_data *ax, u8 *buf, int len,
			 int pad_len);

#ifdef CONFIG_PHYLIB
	struct phy_device *phydev;	/* PHY access */
#endif
	u8 pbuf[RX_BUFFER_SIZE];	/* Receive buffer */
};

/* --------------------- EEPROM ACCESS ------------------------------------ */

#ifdef CONFIG_DRIVER_AX88796_EEPROM

/* EEPROM commands */
#define MAC_EEP_EWEN	4
#define MAC_EEP_EWDS	4
#define MAC_EEP_WRITE	5
#define MAC_EEP_READ	6
#define MAC_EEP_ERASE	7

#define EEP_DELAY	1000

/* Receive some bits from EEPROM */
static u16 ax88796_eep_receive(struct ax88796_priv_data *ax,
			       int bitcount, u8 memr)
{
	u16 bits = 0;
	u8 tmp;

	do {
		AX_OUT(ax, AX_MEMR, memr);
		udelay(EEP_DELAY);
		tmp = AX_IN(ax, AX_MEMR);
		AX_OUT(ax, AX_MEMR, memr | AX_MEMR_EECK);
		udelay(EEP_DELAY);
		bits <<= 1;
		if (tmp & AX_MEMR_EEO)
			bits |= 1;
	} while (--bitcount);

	return bits;
}

/* Send some bits to EEPROM */
static void ax88796_eep_send(struct ax88796_priv_data *ax,
			     u32 bits, int bitcount, u8 memr)
{
	do {
		if (bits & (1 << --bitcount))
			memr |= AX_MEMR_EEI;
		else
			memr &= ~AX_MEMR_EEI;
		AX_OUT(ax, AX_MEMR, memr);
		udelay(EEP_DELAY);
		AX_OUT(ax, AX_MEMR, memr | AX_MEMR_EECK);
		udelay(EEP_DELAY);
	} while (bitcount);
}

/* Read network address from EEPROM */
int ax88796_get_prom(u8 *mac_addr, struct ax88796_priv_data *ax)
{
	u32 addr;
	u16 bits;
	u8 memr;

	memr = AX_IN(ax, AX_MEMR) & ~AX_MEMR_EEMASK;

	for (addr = 0; addr < 2; addr++) {
		/* Assert chip select */
		AX_OUT(ax, AX_MEMR, memr | AX_MEMR_EECS);
		udelay(EEP_DELAY);

		/* Send: dummy bit '0', EEP_READ '110' , address (8 bits) */
		bits = (MAC_EEP_READ << 8) | addr;
		ax88796_eep_send(ax, bits, 12, memr | AX_MEMR_EECS);

		/* Receive: dummy bit '0', value (16 bits) */
		bits = ax88796_eep_receive(ax, 17, memr | AX_MEMR_EECS);
		*mac_addr++ = (uchar)bits;
		*mac_addr++ = (bits >> 8);

		/* De-assert chip select */
		AX_OUT(ax, AX_MEMR, memr);
		udelay(EEP_DELAY);
	}

	return 0;
}

#endif /* CONFIG_DRIVER_AX88796_EEPROM */

/* --------------------- PHY ACCESS --------------------------------------- */

#ifdef CONFIG_PHYLIB

/* Send some bits to PHY */
static void ax88796_phy_send(struct ax88796_priv_data *ax,
			     u32 bits, int bitcount, u8 memr)
{
	/* udelay(1) is the minimum delay we have, so we get ~500 kHz speed */
	do {
		if (bits & (1 << --bitcount))
			memr |= AX_MEMR_MDO;
		else
			memr &= ~AX_MEMR_MDO;
		AX_OUT(ax, AX_MEMR, memr);
		udelay(1);
		AX_OUT(ax, AX_MEMR, memr | AX_MEMR_MDC);
		udelay(1);
	} while (bitcount);
}

/* Read from a PHY register */
static int ax88796_phy_read(struct mii_dev *bus, int phy_addr, int dev_addr,
			    int reg_addr)
{
	int i;
	u8 memr, tmp;
	u32 bits;
	struct ax88796_priv_data *ax = bus->priv;

	bits = (0x6 << 10);
	bits |= (phy_addr & 0x1F) << 5;
	bits |= (reg_addr & 0x1F) << 0;

	memr = AX_IN(ax, AX_MEMR) & ~AX_MEMR_MDMASK;

	/* Send 32 bit preamble (all '1's) */
	ax88796_phy_send(ax, 0xFFFFFFFF, 32, memr);

	/* Send start '01', read '10', phy_addr (5 bits), reg_addr (5 bits) */
	ax88796_phy_send(ax, bits, 14, memr);

	/* Read TA ('Z0') and value (16 bits) */
	for (i = 0; i < 18; i++) {
		AX_OUT(ax, AX_MEMR, memr | AX_MEMR_DIRIN);
		udelay(1);
		tmp = AX_IN(ax, AX_MEMR);
		AX_OUT(ax, AX_MEMR, memr | AX_MEMR_DIRIN | AX_MEMR_MDC);
		udelay(1);
		bits <<= 1;
		if (tmp & AX_MEMR_MDI)
			bits |= 1;
	}

	return (u16)bits;
}

/* Write to a PHY register */
static int ax88796_phy_write(struct mii_dev *bus, int phy_addr, int dev_addr,
			     int reg_addr, u16 val)
{
	u8 memr;
	u32 bits;
	struct ax88796_priv_data *ax = bus->priv;

	bits = 0x50020000;
	bits |= (phy_addr & 0x1F) << 23;
	bits |= (reg_addr & 0x1F) << 18;
	bits |= val;

	memr = AX_IN(ax, AX_MEMR) & ~AX_MEMR_MDMASK;

	/* Send 32 bit preamble (all '1's) */
	ax88796_phy_send(ax, 0xFFFFFFFF, 32, memr);

	/* Send start '01', write '01", phy_addr (5 bits), reg_addr (5 bits),
	   TA '10', value (16 bits) */
	ax88796_phy_send(ax, bits, 32, memr);

	return 0;
}

#endif /* CONFIG_PHYLIB */

/* --------------------- ETHERNET ACCESS ---------------------------------- */

/* Read register value when buswidth is 8 */
static u8 ax88796_in8(void *base_addr, unsigned int reg)
{
	return readb(base_addr + reg);
}

/* Write register value when buswidth is 8 */
static void ax88796_out8(void *base_addr, unsigned int reg, u8 val)
{
	writeb(val, base_addr + reg);
}

/* Read register value when buswidth is 16 */
static u8 ax88796_in16(void *base_addr, unsigned int reg)
{
	return (u8)readw(base_addr + (reg << 1));
}

/* Write register value when buswidth is 16 */
static void ax88796_out16(void *base_addr, unsigned int reg, u8 val)
{
	writew(val, base_addr + (reg << 1));
}

/* Read data when data port is 8 bits wide (buswidth 8 or 16) */
static void ax88796_data_in8(struct ax88796_priv_data *ax, u8 *buf, int len)
{
	int i;

	for (i = 0; i < len; i++)
		*buf++ = AX_IN(ax, AX_DATA);
}

/* Write data when data port is 8 bits wide (buswidth 8 or 16) */
static void ax88796_data_out8(struct ax88796_priv_data *ax, u8 *buf, int len,
			      int pad_len)
{
	int i;

	for (i = 0; i < len; i++)
		AX_OUT(ax, AX_DATA, *buf++);	/* Send data bytes */
	for (; i < pad_len; i++)
		AX_OUT(ax, AX_DATA, 0);		/* Send padding bytes */
}

/* Read data when data port is 16 bits wide (buswidth 16) */
static void ax88796_data_in16(struct ax88796_priv_data *ax, u8 *buf, int len)
{
	void *base_addr = ax->base_addr;
	u16 *data = (u16 *)buf;
	int i;

	for (i = 0; i < len; i += 2)
		*data++ = readw(base_addr + (AX_DATA << 1));
}

/* Write data when data port is 16 bits wide (buswidth 16) */
static void ax88796_data_out16(struct ax88796_priv_data *ax, u8 *buf, int len,
			       int pad_len)
{
	void *base_addr = ax->base_addr;
	u16 *data = (u16 *)buf;
	int i;

	/* Write data bytes */
	for (i = 0; i + 1 < len; i += 2)
		writew(*data++, base_addr + (AX_DATA << 1));

	/* There may be one word with a data byte and a padding byte */
	if (i < len) {
		writew(buf[i], base_addr + (AX_DATA << 1));
		i += 2;
	}

	/* Write padding bytes */
	while (i < pad_len) {
		writew(0, base_addr + (AX_DATA << 1));
		i += 2;
	}
}

/* Read data of received packet from AX88796 and pass to upper layer */
static void ax88796_RxEvent(struct eth_device *dev)
{
	struct ax88796_priv_data *ax = dev->priv;
	u8 rcv_hdr[4];
	int len, pkt, cur;

	AX_IN(ax, AX_RSR);		/* FIXME: Should we evaluate RSR? */
	while (1) {
		/*
		 * Read incoming packet header; different to regular NE2000
		 * devices, the AX88796 duplicates the CURP register for
		 * reading in page 0, so no need to switch to page 1 here.
		 */
		cur = AX_IN(ax, AX_CURP);
		pkt = AX_IN(ax, AX_BNDRY);

		pkt += 1;
		if (pkt == RX_END)
			pkt = RX_START;

		if (pkt == cur) {
			break;
		}
		AX_OUT(ax, AX_RBCL, sizeof(rcv_hdr));
		AX_OUT(ax, AX_RBCH, 0);
		AX_OUT(ax, AX_RSAL, 0);
		AX_OUT(ax, AX_RSAH, pkt);
		if (ax->rx_next == pkt) {
			/* Update boundary pointer */
			if (cur == RX_START)
				AX_OUT(ax, AX_BNDRY, RX_END - 1);
			else
				AX_OUT(ax, AX_BNDRY, cur - 1);
			return;
		}
		ax->rx_next = pkt;
		AX_OUT(ax, AX_ISR, AX_ISR_RDC); /* Clear end of DMA */
		AX_OUT(ax, AX_CR, AX_CR_RDMA | AX_CR_START);

		/* Read header data */
		ax->data_in(ax, rcv_hdr, 4);

		if (!(rcv_hdr[0] & RCV_HDR_GOOD))
			printf("Bad packet: Status=0x%02x\n", rcv_hdr[0]);

		len = ((rcv_hdr[3] << 8) | rcv_hdr[2]) - sizeof(rcv_hdr);
		if (len > RX_BUFFER_SIZE)
			printf("AX88796: packet too big (len=%d)\n", len);

		/* Read incoming packet data */
		AX_OUT(ax, AX_CR, AX_CR_PAGE0 | AX_CR_NODMA | AX_CR_START);
		AX_OUT(ax, AX_RBCL, len & 0xFF);
		AX_OUT(ax, AX_RBCH, len >> 8);
		AX_OUT(ax, AX_RSAL, 4);		/* Skip header */
		AX_OUT(ax, AX_RSAH, ax->rx_next);
		AX_OUT(ax, AX_ISR, AX_ISR_RDC); /* Clear end of DMA */
		AX_OUT(ax, AX_CR, AX_CR_RDMA | AX_CR_START);

		/* Actually transfer data */
		ax->data_in(ax, ax->pbuf, len);

		/* Update boundary pointer */
		if (rcv_hdr[1] == RX_START)
			AX_OUT(ax, AX_BNDRY, RX_END - 1);
		else
			AX_OUT(ax, AX_BNDRY, rcv_hdr[1] - 1);

		/* Then pass frame to the upper layer */
		NetReceive(ax->pbuf, len);
	}
}

/* Handle overflow condition:  Switch to loopcak and fetch packets */
static void ax88796_Overflow(struct eth_device *dev)
{
	struct ax88796_priv_data *ax = dev->priv;
	u8 isr;

	/* Issue a stop command and wait 1.6ms for it to complete. */
	AX_OUT(ax, AX_CR, AX_CR_STOP | AX_CR_NODMA);
	udelay(1600);

	/* Clear the remote byte counter registers. */
	AX_OUT(ax, AX_RBCL, 0);
	AX_OUT(ax, AX_RBCH, 0);

	/* Enter loopback mode while we clear the buffer. */
	AX_OUT(ax, AX_TCR, AX_TCR_LOCAL);
	AX_OUT(ax, AX_CR, AX_CR_START | AX_CR_NODMA);

	/*
	 * Read in as many packets as we can and acknowledge any receive
	 * interrupts. Since the buffer has overflowed, a receive event of
	 * some kind will have occured.
	 */
	ax88796_RxEvent(dev);
	AX_OUT(ax, AX_ISR, AX_ISR_RxP | AX_ISR_RxE);

	/* Clear the overflow condition and leave loopback mode. */
	AX_OUT(ax, AX_ISR, AX_ISR_OFLW);
	AX_OUT(ax, AX_TCR, AX_TCR_NORMAL);

	/*
	 * If a transmit command was issued, but no transmit event has occured,
	 * restart it here.
	 */
	isr = AX_IN(ax, AX_ISR);
	if (ax->tx_running && !(isr & (AX_ISR_TxP | AX_ISR_TxE))) {
		AX_OUT(ax, AX_CR, AX_CR_NODMA | AX_CR_TXPKT | AX_CR_START);
	}
}

/* Handle any events that are signalled in ISR */
static void ax88796_poll(struct eth_device *dev)
{
	struct ax88796_priv_data *ax = dev->priv;
	u8 isr;

	AX_OUT(ax, AX_CR, AX_CR_NODMA | AX_CR_PAGE0 | AX_CR_START);
	while (1) {
		isr = AX_IN(ax, AX_ISR);
		if (isr == 0)
			break;

		/*
		 * The CNT interrupt triggers when the MSB of one of the error
		 * counters is set. We don't much care about these counters, but
		 * we should read their values to reset them.
		 */
		if (isr & AX_ISR_CNT) {
			/* Read the tally counters to clear them. */
			AX_IN(ax, AX_FER);
			AX_IN(ax, AX_CER);
			AX_IN(ax, AX_MISSED);
			AX_OUT(ax, AX_ISR, AX_ISR_CNT);
			isr &= ~AX_ISR_CNT;
		}
		if (isr & AX_ISR_OFLW) {
			/*
			 * Check for overflow. It's a special case, since
			 * there's a particular procedure that must be
			 * followed to get back into a running state.
			 */
			ax88796_Overflow(dev);
		} else {
			/*
			 * Other kinds of interrupts can be acknowledged
			 * simply by clearing the relevant bits of the ISR. Do
			 * that now, then handle the interrupts we care about.
			 */
			AX_OUT(ax, AX_ISR, isr);	/* Clear set bits */

			/*
			 * Check for tx_running on TX event since these may
			 * happen spuriously it seems.
			 */
			if (isr & (AX_ISR_TxP|AX_ISR_TxE) && ax->tx_running) {
				/* Save transmission status and signal done */
				ax->tsr = AX_IN(ax, AX_TSR);
				ax->tx_running = false;
			}
			if (isr & (AX_ISR_RxP|AX_ISR_RxE)) {
				ax88796_RxEvent(dev);
			}
		}
	}
}

/* Send a packet */
static int ax88796_send(struct eth_device *dev, void *packet, int length)
{
	struct ax88796_priv_data *ax = dev->priv;
	u8 *data = (u8 *)packet;
	int pkt_len, isr;
	unsigned int start;

	pkt_len = length;
	if (pkt_len < IEEE_8023_MIN_FRAME)
		pkt_len = IEEE_8023_MIN_FRAME;

	AX_OUT(ax, AX_ISR, AX_ISR_RDC);	/* Clear end of DMA */

	/* Send data to device buffer(s) */
	AX_OUT(ax, AX_RSAL, 0);
	AX_OUT(ax, AX_RSAH, TX_START);
	AX_OUT(ax, AX_RBCL, pkt_len & 0xFF);
	AX_OUT(ax, AX_RBCH, pkt_len >> 8);
	AX_OUT(ax, AX_CR, AX_CR_WDMA | AX_CR_START);

	/* Put data into buffer, add padding if required */
	ax->data_out(ax, data, length, pkt_len);

	/* Wait for DMA to complete */
	do {
		isr = AX_IN(ax, AX_ISR);
	} while ((isr & AX_ISR_RDC) == 0);

	/* Then disable DMA */
	AX_OUT(ax, AX_CR, AX_CR_PAGE0 | AX_CR_NODMA | AX_CR_START);

	/* Start transmit */
	AX_OUT(ax, AX_ISR, (AX_ISR_TxP | AX_ISR_TxE));
	AX_OUT(ax, AX_CR, AX_CR_PAGE0 | AX_CR_NODMA | AX_CR_START);
	AX_OUT(ax, AX_TBCL, pkt_len & 0xFF);
	AX_OUT(ax, AX_TBCH, pkt_len >> 8);
	AX_OUT(ax, AX_TPSR, TX_START);
	AX_OUT(ax, AX_CR, AX_CR_NODMA | AX_CR_TXPKT | AX_CR_START);
	ax->tx_running = true;

	/* Wait for packet to transmit */
	start = get_timer(0);
	do {
		ax88796_poll(dev);
		if (!ax->tx_running) {
			if (ax->tsr & AX_TSR_TxP)
				return 0;
			printf("Transmission error, TSR=0x%02x\n", ax->tsr);
			return -1;
		}
	} while (get_timer(start) < TX_TIMEOUT);

	printf("Transmission timeout\n");

	return -1;
}

/* Check if something was received */
static int ax88796_recv(struct eth_device *dev)
{
	ax88796_poll(dev);
	return 0;
}

/* Start up hardware, prepare to sedn/receive packets */
static int ax88796_init(struct eth_device *dev, bd_t *bd)
{
	struct ax88796_priv_data *ax = dev->priv;

	if (!ax->base_addr)
		return -1;		  	/* No device found */

	ax->tx_running = false;
	ax->rx_next = RX_START - 1;

	AX_OUT(ax, AX_CR, AX_CR_PAGE0 | AX_CR_NODMA | AX_CR_STOP); /* Stop */
	AX_OUT(ax, AX_DCR, AX_DCR_INIT);
	AX_OUT(ax, AX_RBCH, 0);			/* Remote byte count */
	AX_OUT(ax, AX_RBCL, 0);
	AX_OUT(ax, AX_RCR, AX_RCR_MON);		/* Accept no packets */
	AX_OUT(ax, AX_TCR, AX_TCR_LOCAL);	/* Transmitter off */
	AX_OUT(ax, AX_TPSR, TX_START);		/* Transmitter start page */
	AX_OUT(ax, AX_PSTART, RX_START);	/* Receive ring start page */
	AX_OUT(ax, AX_BNDRY, RX_END - 1);	/* Receive ring boundary */
	AX_OUT(ax, AX_PSTOP, RX_END);		/* Receive ring end page */
	if (ax->mode == AX88796_MODE_BUS16_DP16)
		AX_OUT(ax, AX_DCR, AX_DCR_WTS);	/* Word transfers */

#ifdef CONFIG_PHYLIB
	/* Start up the PHY */
	if (phy_startup(ax->phydev)) {
		printf("Could not initialize PHY %s\n", ax->phydev->dev->name);
		return -1;
	}
#endif

	AX_OUT(ax, AX_ISR, 0xFF);		/* Clear pending interrupts */
	AX_OUT(ax, AX_IMR, AX_IMR_All);		/* Enable all interrupts */
	AX_OUT(ax, AX_CR, AX_CR_NODMA | AX_CR_PAGE1 | AX_CR_STOP);
	AX_OUT(ax, AX_P1_CURP, RX_START);	/* Current page */

	/* Enable and start device */
	AX_OUT(ax, AX_CR, AX_CR_PAGE0 | AX_CR_NODMA | AX_CR_START);
	AX_OUT(ax, AX_TCR, AX_TCR_NORMAL);	/* Normal transmit operations */
	AX_OUT(ax, AX_RCR, AX_RCR_AB);		/* Accept broadcast, no errors,
						   no multicast */
	return 0;
}

/* Shut down interface, any ongoing transfer is stopped */
static void ax88796_halt(struct eth_device *dev)
{
	struct ax88796_priv_data *ax = dev->priv;

	AX_OUT(ax, AX_CR, AX_CR_PAGE0 | AX_CR_NODMA | AX_CR_STOP);
	AX_OUT(ax, AX_ISR, 0xFF);		/* Clear pending interrupts */
	AX_OUT(ax, AX_IMR, 0x00);		/* Disable all interrupts */
}

/* Set network address */
int ax88796_write_hwaddr(struct eth_device *dev)
{
	struct ax88796_priv_data *ax = dev->priv;
	int i;

	if (!ax->base_addr)
		return -1;

	/* Stop transfer, select register page 1 */
	AX_OUT(ax, AX_CR, AX_CR_NODMA | AX_CR_PAGE1 | AX_CR_STOP);

	/* Set ethernet address */
	for (i = 0; i < 6; i++)
		AX_OUT(ax, AX_P1_PAR0+i, dev->enetaddr[i]);

	/* Select register page 0 again */
	AX_OUT(ax, AX_CR, AX_CR_NODMA | AX_CR_PAGE0);

	return 0;
}

/* Register an AX88796 ethernet device (including PHY) */
int ax88796_initialize(int dev_id, uint32_t base_addr, int mode)
{
	struct eth_device *dev;
	struct ax88796_priv_data *ax;
#ifdef CONFIG_PHYLIB
	struct mii_dev *bus = NULL;
	struct phy_device *phydev;
#endif

	/* Allocate device */
	dev = malloc(sizeof(*dev));
	if (!dev)
		goto edev_error;
	memset(dev, 0, sizeof(*dev));

	/* Allocate private data structure */
	ax = malloc(sizeof(struct ax88796_priv_data));
	if (!ax)
		goto ax_error;

	/* Set up private data structure */
	ax->base_addr = (void *)base_addr;
	ax->mode = mode;
	if (mode == AX88796_MODE_BUS8_DP8) {
		ax->in = ax88796_in8;
		ax->out = ax88796_out8;
	} else {
		ax->in = ax88796_in16;
		ax->out = ax88796_out16;
	}
	if (mode == AX88796_MODE_BUS16_DP16) {
		ax->data_in = ax88796_data_in16;
		ax->data_out = ax88796_data_out16;
	} else {
		ax->data_in = ax88796_data_in8;
		ax->data_out = ax88796_data_out8;
	}

#ifdef CONFIG_DRIVER_AX88796_EEPROM
	/* Get hardware MAC address from PROM */
	if (ax88796_get_prom(dev->enetaddr, (void *)base_addr))
		goto eeprom_error;
#endif

	/* Set up device structure */
	dev->priv = ax;
	dev->iobase = (int)base_addr;
	dev->init = ax88796_init;
	dev->halt = ax88796_halt;
	dev->send = ax88796_send;
	dev->recv = ax88796_recv;
	dev->write_hwaddr = ax88796_write_hwaddr;
	if (dev_id < 0)
		strcpy(dev->name, AX88796_DEV_NAME);
	else
		sprintf(dev->name, AX88796_DEV_NAME "-%d", dev_id);

#ifdef CONFIG_PHYLIB
	bus = mdio_alloc();
	if (!bus)
		goto mdio_alloc_error;

	bus->read = ax88796_phy_read;
	bus->write = ax88796_phy_write;
	bus->priv = ax;
	strcpy(bus->name, dev->name);
	
	if (mdio_register(bus))
		goto mdio_register_error;

	/* The AX88796 PHY is always on phy address 0x10 */
	phydev = phy_find_by_mask(bus, 1 << 0x10, PHY_INTERFACE_MODE_RMII);
	if (!phydev)
		goto phydev_error;

	phy_connect_dev(phydev, dev);
	phy_config(phydev);
	ax->phydev = phydev;
#endif

	/* Register the ethernet device */
	return eth_register(dev);

#ifdef CONFIG_PHYLIB
phydev_error:
mdio_register_error:
	free(bus);
mdio_alloc_error:
#endif
#ifdef CONFIG_DRIVER_AX88796_EEPROM
eeprom_error:
#endif
	free(ax);
ax_error:
	free(dev);
edev_error:

	return -1;
}
