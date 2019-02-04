/*
 * (c) 2007 Sascha Hauer <s.hauer@pengutronix.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <watchdog.h>		        /* WATCHDOG_RESET() */
#include <asm/arch/imx-regs.h>		/* UART?_BASE */
#include <asm/arch/clock.h>		/* imx_get_uartclk() */
#include <dm/platform_data/serial_mxc.h>
#include <serial.h>			/* struct serial_device */
#include <linux/compiler.h>
#include <asm/io.h>			/* __raw_readl(), __raw_writel() */

/* Register definitions */
#define URXD  0x0  /* Receiver Register */
#define UTXD  0x40 /* Transmitter Register */
#define UCR1  0x80 /* Control Register 1 */
#define UCR2  0x84 /* Control Register 2 */
#define UCR3  0x88 /* Control Register 3 */
#define UCR4  0x8c /* Control Register 4 */
#define UFCR  0x90 /* FIFO Control Register */
#define USR1  0x94 /* Status Register 1 */
#define USR2  0x98 /* Status Register 2 */
#define UESC  0x9c /* Escape Character Register */
#define UTIM  0xa0 /* Escape Timer Register */
#define UBIR  0xa4 /* BRM Incremental Register */
#define UBMR  0xa8 /* BRM Modulator Register */
#define UBRC  0xac /* Baud Rate Count Register */
#define UTS   0xb4 /* UART Test Register (mx31) */

/* UART Control Register Bit Fields.*/
#define  URXD_CHARRDY    (1<<15)
#define  URXD_ERR        (1<<14)
#define  URXD_OVRRUN     (1<<13)
#define  URXD_FRMERR     (1<<12)
#define  URXD_BRK        (1<<11)
#define  URXD_PRERR      (1<<10)
#define  URXD_RX_DATA    (0xFF)
#define  UCR1_ADEN       (1<<15) /* Auto dectect interrupt */
#define  UCR1_ADBR       (1<<14) /* Auto detect baud rate */
#define  UCR1_TRDYEN     (1<<13) /* Transmitter ready interrupt enable */
#define  UCR1_IDEN       (1<<12) /* Idle condition interrupt */
#define  UCR1_RRDYEN     (1<<9)	 /* Recv ready interrupt enable */
#define  UCR1_RDMAEN     (1<<8)	 /* Recv ready DMA enable */
#define  UCR1_IREN       (1<<7)	 /* Infrared interface enable */
#define  UCR1_TXMPTYEN   (1<<6)	 /* Transimitter empty interrupt enable */
#define  UCR1_RTSDEN     (1<<5)	 /* RTS delta interrupt enable */
#define  UCR1_SNDBRK     (1<<4)	 /* Send break */
#define  UCR1_TDMAEN     (1<<3)	 /* Transmitter ready DMA enable */
#define  UCR1_UARTCLKEN  (1<<2)	 /* UART clock enabled */
#define  UCR1_DOZE       (1<<1)	 /* Doze */
#define  UCR1_UARTEN     (1<<0)	 /* UART enabled */
#define  UCR2_ESCI	 (1<<15) /* Escape seq interrupt enable */
#define  UCR2_IRTS	 (1<<14) /* Ignore RTS pin */
#define  UCR2_CTSC	 (1<<13) /* CTS pin control */
#define  UCR2_CTS        (1<<12) /* Clear to send */
#define  UCR2_ESCEN      (1<<11) /* Escape enable */
#define  UCR2_PREN       (1<<8)  /* Parity enable */
#define  UCR2_PROE       (1<<7)  /* Parity odd/even */
#define  UCR2_STPB       (1<<6)	 /* Stop */
#define  UCR2_WS         (1<<5)	 /* Word size */
#define  UCR2_RTSEN      (1<<4)	 /* Request to send interrupt enable */
#define  UCR2_TXEN       (1<<2)	 /* Transmitter enabled */
#define  UCR2_RXEN       (1<<1)	 /* Receiver enabled */
#define  UCR2_SRST	 (1<<0)	 /* SW reset */
#define  UCR3_DTREN	 (1<<13) /* DTR interrupt enable */
#define  UCR3_PARERREN   (1<<12) /* Parity enable */
#define  UCR3_FRAERREN   (1<<11) /* Frame error interrupt enable */
#define  UCR3_DSR        (1<<10) /* Data set ready */
#define  UCR3_DCD        (1<<9)  /* Data carrier detect */
#define  UCR3_RI         (1<<8)  /* Ring indicator */
#define  UCR3_ADNIMP     (1<<7)  /* Autobaud Detection Not Improved */
#define  UCR3_RXDSEN	 (1<<6)  /* Receive status interrupt enable */
#define  UCR3_AIRINTEN   (1<<5)  /* Async IR wake interrupt enable */
#define  UCR3_AWAKEN	 (1<<4)  /* Async wake interrupt enable */
#define  UCR3_REF25	 (1<<3)  /* Ref freq 25 MHz */
#define  UCR3_REF30	 (1<<2)  /* Ref Freq 30 MHz */
#define  UCR3_INVT	 (1<<1)  /* Inverted Infrared transmission */
#define  UCR3_BPEN	 (1<<0)  /* Preset registers enable */
#define  UCR4_CTSTL_32   (32<<10) /* CTS trigger level (32 chars) */
#define  UCR4_INVR	 (1<<9)  /* Inverted infrared reception */
#define  UCR4_ENIRI	 (1<<8)  /* Serial infrared interrupt enable */
#define  UCR4_WKEN	 (1<<7)  /* Wake interrupt enable */
#define  UCR4_REF16	 (1<<6)  /* Ref freq 16 MHz */
#define  UCR4_IRSC	 (1<<5)  /* IR special case */
#define  UCR4_TCEN	 (1<<3)  /* Transmit complete interrupt enable */
#define  UCR4_BKEN	 (1<<2)  /* Break condition interrupt enable */
#define  UCR4_OREN	 (1<<1)  /* Receiver overrun interrupt enable */
#define  UCR4_DREN	 (1<<0)  /* Recv data ready interrupt enable */
#define  UFCR_RXTL_SHF   0       /* Receiver trigger level shift */
#define  UFCR_RFDIV      (7<<7)  /* Reference freq divider mask */
#define  UFCR_RFDIV_SHF  7       /* Reference freq divider shift */
#define  UFCR_DCEDTE	 (1<<6)  /* DTE mode select */
#define  UFCR_TXTL_SHF   10      /* Transmitter trigger level shift */
#define  USR1_PARITYERR  (1<<15) /* Parity error interrupt flag */
#define  USR1_RTSS	 (1<<14) /* RTS pin status */
#define  USR1_TRDY	 (1<<13) /* Transmitter ready interrupt/dma flag */
#define  USR1_RTSD	 (1<<12) /* RTS delta */
#define  USR1_ESCF	 (1<<11) /* Escape seq interrupt flag */
#define  USR1_FRAMERR    (1<<10) /* Frame error interrupt flag */
#define  USR1_RRDY       (1<<9)	 /* Receiver ready interrupt/dma flag */
#define  USR1_TIMEOUT    (1<<7)	 /* Receive timeout interrupt status */
#define  USR1_RXDS	 (1<<6)	 /* Receiver idle interrupt flag */
#define  USR1_AIRINT	 (1<<5)	 /* Async IR wake interrupt flag */
#define  USR1_AWAKE	 (1<<4)	 /* Aysnc wake interrupt flag */
#define  USR2_ADET	 (1<<15) /* Auto baud rate detect complete */
#define  USR2_TXFE	 (1<<14) /* Transmit buffer FIFO empty */
#define  USR2_DTRF	 (1<<13) /* DTR edge interrupt flag */
#define  USR2_IDLE	 (1<<12) /* Idle condition */
#define  USR2_IRINT	 (1<<8)	 /* Serial infrared interrupt flag */
#define  USR2_WAKE	 (1<<7)	 /* Wake */
#define  USR2_RTSF	 (1<<4)	 /* RTS edge interrupt flag */
#define  USR2_TXDC	 (1<<3)	 /* Transmitter complete */
#define  USR2_BRCD	 (1<<2)	 /* Break condition */
#define  USR2_ORE        (1<<1)	 /* Overrun error */
#define  USR2_RDR        (1<<0)	 /* Recv data ready */
#define  UTS_FRCPERR	 (1<<13) /* Force parity error */
#define  UTS_LOOP        (1<<12) /* Loop tx and rx */
#define  UTS_TXEMPTY	 (1<<6)	 /* TxFIFO empty */
#define  UTS_RXEMPTY	 (1<<5)	 /* RxFIFO empty */
#define  UTS_TXFULL	 (1<<4)	 /* TxFIFO full */
#define  UTS_RXFULL	 (1<<3)	 /* RxFIFO full */
#define  UTS_SOFTRST	 (1<<0)	 /* Software reset */

#ifndef CONFIG_DM_SERIAL

/* Register definitions */
#define URXD  0x0  /* Receiver Register */
#define UTXD  0x40 /* Transmitter Register */
#define UCR1  0x80 /* Control Register 1 */
#define UCR2  0x84 /* Control Register 2 */
#define UCR3  0x88 /* Control Register 3 */
#define UCR4  0x8c /* Control Register 4 */
#define UFCR  0x90 /* FIFO Control Register */
#define USR1  0x94 /* Status Register 1 */
#define USR2  0x98 /* Status Register 2 */
#define UESC  0x9c /* Escape Character Register */
#define UTIM  0xa0 /* Escape Timer Register */
#define UBIR  0xa4 /* BRM Incremental Register */
#define UBMR  0xa8 /* BRM Modulator Register */
#define UBRC  0xac /* Baud Rate Count Register */
#define UTS   0xb4 /* UART Test Register (mx31) */

#define TXTL  2 /* reset default */
#define RXTL  1 /* reset default */
#define RFDIV 4 /* divide input clock by 2 */

static void mxc_serial_setbrg(const struct serial_device *sdev)
{
	DECLARE_GLOBAL_DATA_PTR;
	u32 clk = imx_get_uartclk();
	void *uart_phys = sdev->dev.priv;

	__raw_writel((RFDIV << UFCR_RFDIV_SHF) | (TXTL << UFCR_TXTL_SHF)
		     | (RXTL << UFCR_RXTL_SHF), uart_phys + UFCR);
	__raw_writel(0xF, uart_phys + UBIR);
	__raw_writel(clk / (2 * gd->baudrate), uart_phys + UBMR);

}

/*
 * Initialise the serial port with the given baudrate. The settings
 * are always 8 data bits, no parity, 1 stop bit, no start bits.
 *
 */
static int mxc_serial_start(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = to_serial_device(pdev);
	void *uart_phys = sdev->dev.priv;

	__raw_writel(0x0, uart_phys + UCR1);
	__raw_writel(0x0, uart_phys + UCR2);

	while (!(__raw_readl(uart_phys + UCR2) & UCR2_SRST))
		; /* Do nothing */

	__raw_writel(0x0704 | UCR3_ADNIMP, uart_phys + UCR3);
	__raw_writel(0x8000, uart_phys + UCR4);
	__raw_writel(0x002B, uart_phys + UESC);
	__raw_writel(0x0, uart_phys + UTIM);

	__raw_writel(0x0, uart_phys + UTS);

	mxc_serial_setbrg(sdev);

	__raw_writel(UCR2_WS | UCR2_IRTS | UCR2_RXEN | UCR2_TXEN | UCR2_SRST,
		     uart_phys + UCR2);

	__raw_writel(UCR1_UARTEN, uart_phys + UCR1);

	return 0;
}

static void mxc_ll_putc(void *uart_phys, const char c)
{
	/* If \n, do \r first */
	if (c == '\n')
		mxc_ll_putc(uart_phys, '\r');

	/* wait for room in the tx FIFO */
	while (!(__raw_readl(uart_phys + UTS) & UTS_TXEMPTY))
		WATCHDOG_RESET();

	/* Send character */
	__raw_writel(c, uart_phys + UTXD);
}

/*
 * Output a single byte to the serial port.
 */
static void mxc_serial_putc(const struct stdio_dev *pdev, const char c)
{
	struct serial_device *sdev = to_serial_device(pdev);
	void *uart_phys = sdev->dev.priv;

	mxc_ll_putc(uart_phys, c);
}

/*
 * Output a string to the serial port.
 */
static void mxc_serial_puts(const struct stdio_dev *pdev, const char *s)
{
	struct serial_device *sdev = to_serial_device(pdev);
	void *uart_phys = sdev->dev.priv;

	while (*s)
		mxc_ll_putc(uart_phys, *s++);
}


/*
 * Read a single byte from the serial port. 
 */
static int mxc_serial_getc(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = to_serial_device(pdev);
	void *uart_phys = sdev->dev.priv;

	/* Wait for character to arrive */
	while (__raw_readl(uart_phys + UTS) & UTS_RXEMPTY)
		WATCHDOG_RESET();

	/* mask out status from upper word */
	return (__raw_readl(uart_phys + URXD) & URXD_RX_DATA);
}

/*
 * Test whether a character is in the RX buffer
 */
static int mxc_serial_tstc(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = to_serial_device(pdev);
	void *uart_phys = sdev->dev.priv;

	/* If receive fifo is empty, return false */
	if (__raw_readl(uart_phys + UTS) & UTS_RXEMPTY)
		return 0;

	return 1;
}


#define INIT_MXC_SERIAL(_addr, _name, _hwname) {	\
	{       /* stdio_dev part */		\
		.name = _name,			\
		.hwname = _hwname,		\
		.flags = DEV_FLAGS_INPUT | DEV_FLAGS_OUTPUT, \
		.start = mxc_serial_start,	\
		.stop = NULL,			\
		.getc = mxc_serial_getc,	\
		.tstc =	mxc_serial_tstc,	\
		.putc = mxc_serial_putc,	\
		.puts = mxc_serial_puts,	\
		.priv = (void *)_addr,	\
	},					\
	.setbrg = mxc_serial_setbrg,		\
}

struct serial_device mxc_serial_device[] = {
	INIT_MXC_SERIAL(UART1_BASE, CONFIG_SYS_SERCON_NAME "0", "mxc_uart0"),
	INIT_MXC_SERIAL(UART2_BASE, CONFIG_SYS_SERCON_NAME "1", "mxc_uart1"),
	INIT_MXC_SERIAL(UART3_BASE, CONFIG_SYS_SERCON_NAME "2", "mxc_uart2"),
	INIT_MXC_SERIAL(UART4_BASE, CONFIG_SYS_SERCON_NAME "3", "mxc_uart3"),
	INIT_MXC_SERIAL(UART5_BASE, CONFIG_SYS_SERCON_NAME "4", "mxc_uart4"),
};

/* Get pointer to n-th serial device */
struct serial_device *get_serial_device(unsigned int n)
{
	if (n < 5)
		return &mxc_serial_device[n];

	return NULL;
}

/* Register all serial ports; if you only want to register a subset, implement
   function board_serial_init() and call serial_register() there. */
void mxc_serial_initialize(void)
{
	serial_register(&mxc_serial_device[0]);
	serial_register(&mxc_serial_device[1]);
	serial_register(&mxc_serial_device[2]);
	serial_register(&mxc_serial_device[3]);
	serial_register(&mxc_serial_device[4]);
}
#endif

#ifdef CONFIG_DM_SERIAL

struct mxc_uart {
	u32 rxd;
	u32 spare0[15];

	u32 txd;
	u32 spare1[15];

	u32 cr1;
	u32 cr2;
	u32 cr3;
	u32 cr4;

	u32 fcr;
	u32 sr1;
	u32 sr2;
	u32 esc;

	u32 tim;
	u32 bir;
	u32 bmr;
	u32 brc;

	u32 onems;
	u32 ts;
};

int mxc_serial_setbrg(struct udevice *dev, int baudrate)
{
	struct mxc_serial_platdata *plat = dev->platdata;
	struct mxc_uart *const uart = plat->reg;
	u32 clk = imx_get_uartclk();
	u32 tmp;

	tmp = 4 << UFCR_RFDIV_SHF;
	if (plat->use_dte)
		tmp |= UFCR_DCEDTE;
	writel(tmp, &uart->fcr);

	writel(0xf, &uart->bir);
	writel(clk / (2 * baudrate), &uart->bmr);

	writel(UCR2_WS | UCR2_IRTS | UCR2_RXEN | UCR2_TXEN | UCR2_SRST,
	       &uart->cr2);
	writel(UCR1_UARTEN, &uart->cr1);

	return 0;
}

static int mxc_serial_probe(struct udevice *dev)
{
	struct mxc_serial_platdata *plat = dev->platdata;
	struct mxc_uart *const uart = plat->reg;

	writel(0, &uart->cr1);
	writel(0, &uart->cr2);
	while (!(readl(&uart->cr2) & UCR2_SRST));
	writel(0x704 | UCR3_ADNIMP, &uart->cr3);
	writel(0x8000, &uart->cr4);
	writel(0x2b, &uart->esc);
	writel(0, &uart->tim);
	writel(0, &uart->ts);

	return 0;
}

static int mxc_serial_getc(struct udevice *dev)
{
	struct mxc_serial_platdata *plat = dev->platdata;
	struct mxc_uart *const uart = plat->reg;

	if (readl(&uart->ts) & UTS_RXEMPTY)
		return -EAGAIN;

	return readl(&uart->rxd) & URXD_RX_DATA;
}

static int mxc_serial_putc(struct udevice *dev, const char ch)
{
	struct mxc_serial_platdata *plat = dev->platdata;
	struct mxc_uart *const uart = plat->reg;

	if (!(readl(&uart->ts) & UTS_TXEMPTY))
		return -EAGAIN;

	writel(ch, &uart->txd);

	return 0;
}

static int mxc_serial_pending(struct udevice *dev, bool input)
{
	struct mxc_serial_platdata *plat = dev->platdata;
	struct mxc_uart *const uart = plat->reg;
	uint32_t sr2 = readl(&uart->sr2);

	if (input)
		return sr2 & USR2_RDR ? 1 : 0;
	else
		return sr2 & USR2_TXDC ? 0 : 1;
}

static const struct dm_serial_ops mxc_serial_ops = {
	.putc = mxc_serial_putc,
	.pending = mxc_serial_pending,
	.getc = mxc_serial_getc,
	.setbrg = mxc_serial_setbrg,
};

#if CONFIG_IS_ENABLED(OF_CONTROL)
static int mxc_serial_ofdata_to_platdata(struct udevice *dev)
{
	struct mxc_serial_platdata *plat = dev->platdata;
	fdt_addr_t addr;

	addr = devfdt_get_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	plat->reg = (struct mxc_uart *)addr;

	plat->use_dte = fdtdec_get_bool(gd->fdt_blob, dev_of_offset(dev),
					"fsl,dte-mode");
	return 0;
}

static const struct udevice_id mxc_serial_ids[] = {
	{ .compatible = "fsl,imx6ul-uart" },
	{ .compatible = "fsl,imx7d-uart" },
	{ }
};
#endif

U_BOOT_DRIVER(serial_mxc) = {
	.name	= "serial_mxc",
	.id	= UCLASS_SERIAL,
#if CONFIG_IS_ENABLED(OF_CONTROL)
	.of_match = mxc_serial_ids,
	.ofdata_to_platdata = mxc_serial_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct mxc_serial_platdata),
#endif
	.probe = mxc_serial_probe,
	.ops	= &mxc_serial_ops,
	.flags = DM_FLAG_PRE_RELOC,
};
#endif
