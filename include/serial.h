#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <stdio_dev.h>			  /* struct stdio_dev */
#include <post.h>

#define to_serial_device(x) container_of((x), struct serial_device, dev)

struct serial_device {
	/* Standard functions start(), stop(), tstc(), getc(), putc(), puts()
	   are defined in the embedded stdio_dev */
	struct stdio_dev dev;

	/* Additional serial specific functions and fields are defined here */
	void (*setbrg) (const struct serial_device *);
#if CONFIG_POST & CONFIG_SYS_POST_UART
	void (*loop) (const struct serial_device *, int);
#endif

	/* The serial devices are stored in a ring structure. So the last
	   element points with its next pointer back again to the first
	   element and the first element points with its prev pointer back
	   again to the last element. The beginning of the ring is stored in
	   variable serial_devices. */
	struct serial_device *next;
	struct serial_device *prev;
};

void default_serial_puts(const struct stdio_dev *pdev, const char *s);

extern struct serial_device serial_smc_device;
extern struct serial_device serial_scc_device;
extern struct serial_device *default_serial_console(void);

#if	defined(CONFIG_405GP) || defined(CONFIG_405CR) || \
	defined(CONFIG_405EP) || defined(CONFIG_405EZ) || \
	defined(CONFIG_405EX) || defined(CONFIG_440) || \
	defined(CONFIG_MB86R0x) || defined(CONFIG_MPC5xxx) || \
	defined(CONFIG_MPC83xx) || defined(CONFIG_MPC85xx) || \
	defined(CONFIG_MPC86xx) || defined(CONFIG_SYS_SC520) || \
	defined(CONFIG_TEGRA) || defined(CONFIG_SYS_COREBOOT) || \
	defined(CONFIG_MICROBLAZE)
extern struct serial_device serial0_device;
extern struct serial_device serial1_device;
#endif

extern void serial_register(struct serial_device *);
extern void serial_initialize(void);
extern void serial_stdio_init(void);
extern int serial_assign(const char *name);
extern void serial_reinit_all(void);
extern struct serial_device *get_serial_device(unsigned int);

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

#if defined(CONFIG_MPC512X)
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
