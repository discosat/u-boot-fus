#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <stdio_dev.h>			  /* struct stdio_dev */
#include <post.h>

struct serial_device {
	/* Standard functions start(), stop(), tstc(), getc(), putc(), puts()
	   are defined in the embedded stdio_dev */
	struct stdio_dev dev;

	/* Additional serial specific functions and fields are defined here */
	void (*setbrg) (const struct serial_device *);
#if CONFIG_POST & CONFIG_SYS_POST_UART
	void (*loop) (const struct serial_device *, int);
#endif
	void *serpriv;

	/* The serial devices are stored in a ring structure. So the last
	   element points with its next pointer back again to the first
	   element and the first element points with its prev pointer back
	   again to the last element. The beginning of the ring is stored in
	   variable serial_devices. */
	struct serial_device *next;
	struct serial_device *prev;
};

extern struct serial_device serial_smc_device;
extern struct serial_device serial_scc_device;
extern struct serial_device *default_serial_console(void);

#if	defined(CONFIG_405GP) || defined(CONFIG_405CR) || \
	defined(CONFIG_405EP) || defined(CONFIG_405EZ) || \
	defined(CONFIG_405EX) || defined(CONFIG_440) || \
	defined(CONFIG_MB86R0x) || defined(CONFIG_MPC5xxx) || \
	defined(CONFIG_MPC83xx) || defined(CONFIG_MPC85xx) || \
	defined(CONFIG_MPC86xx) || defined(CONFIG_SYS_SC520) || \
	defined(CONFIG_TEGRA2)
extern struct serial_device serial0_device;
extern struct serial_device serial1_device;
#if defined(CONFIG_SYS_NS16550_SERIAL)
extern struct serial_device eserial1_device;
extern struct serial_device eserial2_device;
extern struct serial_device eserial3_device;
extern struct serial_device eserial4_device;
#endif /* CONFIG_SYS_NS16550_SERIAL */

#endif

#if defined(CONFIG_MPC512X)
extern struct serial_device serial1_device;
extern struct serial_device serial3_device;
extern struct serial_device serial4_device;
extern struct serial_device serial6_device;
#endif

#if defined(CONFIG_XILINX_UARTLITE)
extern struct serial_device uartlite_serial0_device;
extern struct serial_device uartlite_serial1_device;
extern struct serial_device uartlite_serial2_device;
extern struct serial_device uartlite_serial3_device;
#endif

#if defined(CONFIG_S3C2410)
extern struct serial_device s3c24xx_serial0_device;
extern struct serial_device s3c24xx_serial1_device;
extern struct serial_device s3c24xx_serial2_device;
#endif

#if defined(CONFIG_S5P)
extern void s5p_serial_register(int, const char *);
#endif

#if defined(CONFIG_OMAP3_ZOOM2)
extern struct serial_device zoom2_serial_device0;
extern struct serial_device zoom2_serial_device1;
extern struct serial_device zoom2_serial_device2;
extern struct serial_device zoom2_serial_device3;
#endif

extern struct serial_device serial_ffuart_device;
extern struct serial_device serial_btuart_device;
extern struct serial_device serial_stuart_device;

#if defined(CONFIG_SYS_BFIN_UART)
extern void serial_register_bfin_uart(void);
extern struct serial_device bfin_serial0_device;
extern struct serial_device bfin_serial1_device;
extern struct serial_device bfin_serial2_device;
extern struct serial_device bfin_serial3_device;
#endif

extern void serial_register(struct serial_device *);
extern void serial_initialize(void);
extern void serial_stdio_init(void);
extern int serial_assign(const char *name);
extern void serial_reinit_all(void);

/* For usbtty */
#ifdef CONFIG_USB_TTY

extern int usbtty_getc(void);
extern void usbtty_putc(const char c);
extern void usbtty_puts(const char *str);
extern int usbtty_tstc(void);

#else

/* stubs */
#define usbtty_getc() 0
#define usbtty_putc(a)
#define usbtty_puts(a)
#define usbtty_tstc() 0

#endif /* CONFIG_USB_TTY */

#if defined(CONFIG_MPC512X) &&  defined(CONFIG_SERIAL_MULTI)
extern struct stdio_dev *open_port(int num, int baudrate);
extern int close_port(int num);
extern int write_port(struct stdio_dev *port, char *buf);
extern int read_port(struct stdio_dev *port, char *buf, int size);
#endif

#endif

#ifdef CONFIG_SERIAL_SOFTWARE_FIFO
void	serial_buffered_init (void);
void	serial_buffered_putc (const struct stdio_dev *pdev, const char);
void	serial_buffered_puts (const struct stdio_dev *pdev, const char *);
int	serial_buffered_getc (const struct stdio_dev *pdev);
int	serial_buffered_tstc (const struct stdio_dev *pdev);
#endif /* CONFIG_SERIAL_SOFTWARE_FIFO */

/* arch/$(ARCH)/cpu/$(CPU)/$(SOC)/serial.c */
int	serial_init   (void);
void	serial_addr   (unsigned int);
void	serial_setbrg (void);
void	serial_putc_raw(const char);
int	serial_start  (const struct stdio_dev *);
void	serial_putc   (const struct stdio_dev *, const char);
void	serial_puts   (const struct stdio_dev *, const char *);
int	serial_getc   (const struct stdio_dev *);
int	serial_tstc   (const struct stdio_dev *);

void	_serial_setbrg (const int);
void	_serial_putc   (const char, const int);
void	_serial_putc_raw(const char, const int);
void	_serial_puts   (const char *, const int);
int	_serial_getc   (const int);
int	_serial_tstc   (const int);
