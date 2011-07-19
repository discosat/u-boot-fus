#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <devices.h>			  /* device_t */

#define NAMESIZE 16
#define CTLRSIZE 8

struct serial_device {
	char name[NAMESIZE];
	char ctlr[CTLRSIZE];

	int  (*init) (void);
	void (*setbrg) (void);
	int (*getc) (void);
	int (*tstc) (void);
	void (*putc) (const char c);
	void (*puts) (const char *s);

	struct serial_device *next;
};

extern struct serial_device serial_smc_device;
extern struct serial_device serial_scc_device;
extern struct serial_device * default_serial_console (void);

#if defined(CONFIG_405GP) || defined(CONFIG_405CR) || defined(CONFIG_440) || \
    defined(CONFIG_405EP) || defined(CONFIG_405EZ) || defined(CONFIG_405EX) || \
    defined(CONFIG_MPC5xxx)
extern struct serial_device serial0_device;
extern struct serial_device serial1_device;
#if defined(CFG_NS16550_SERIAL)
extern struct serial_device eserial1_device;
extern struct serial_device eserial2_device;
extern struct serial_device eserial3_device;
extern struct serial_device eserial4_device;
#endif /* CFG_NS16550_SERIAL */

#endif

#if defined(CONFIG_S3C2410)
extern struct serial_device s3c24xx_serial0_device;
extern struct serial_device s3c24xx_serial1_device;
extern struct serial_device s3c24xx_serial2_device;
#endif

extern struct serial_device serial_ffuart_device;
extern struct serial_device serial_btuart_device;
extern struct serial_device serial_stuart_device;

extern void serial_initialize(void);
extern void serial_devices_init(void);
extern int serial_assign(char * name);
extern void serial_reinit_all(void);

#endif

#ifdef CONFIG_SERIAL_SOFTWARE_FIFO
void	serial_buffered_init (void);
void	serial_buffered_putc (const device_t *pdev, const char);
void	serial_buffered_puts (const device_t *pdev, const char *);
int	serial_buffered_getc (const device_t *pdev);
int	serial_buffered_tstc (const device_t *pdev);
#endif /* CONFIG_SERIAL_SOFTWARE_FIFO */

/* $(CPU)/serial.c */
int	serial_init   (void);
void	serial_addr   (unsigned int);
void	serial_setbrg (void);
void	serial_putc_raw(const char);
void	serial_putc   (const device_t *, const char);
void	serial_puts   (const device_t *, const char *);
int	serial_getc   (const device_t *);
int	serial_tstc   (const device_t *);

void	_serial_setbrg (const int);
void	_serial_putc   (const char, const int);
void	_serial_putc_raw(const char, const int);
void	_serial_puts   (const char *, const int);
int	_serial_getc   (const int);
int	_serial_tstc   (const int);
