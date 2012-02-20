/*
 * (C) Copyright 2000
 * Paolo Scaffardi, AIRVENT SAM s.p.a - RIMINI(ITALY), arsenio@tin.it
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <stdarg.h>
#include <malloc.h>
#include <serial.h>			  /* serial_*() */
#include <stdio_dev.h>
#include <exports.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_SYS_CONSOLE_IS_IN_ENV
/*
 * if overwrite_console returns 1, the stdin, stderr and stdout
 * are switched to the serial port, else the settings in the
 * environment are used
 */
#ifdef CONFIG_SYS_CONSOLE_OVERWRITE_ROUTINE
extern int overwrite_console(void);
#define OVERWRITE_CONSOLE overwrite_console()
#else
#define OVERWRITE_CONSOLE 0
#endif /* CONFIG_SYS_CONSOLE_OVERWRITE_ROUTINE */

#endif /* CONFIG_SYS_CONSOLE_IS_IN_ENV */

static int console_setfile(int file, struct stdio_dev *pdev)
{
	int error = 0;

	if (!pdev || file >= MAX_FILES)
		return -1;

	/* Start new device */
	if (pdev->start) {
		error = pdev->start(pdev);
		/* If it's not started dont use it */
		if (error < 0)
			return error;
	}

	/* Assign the new device (leaving the existing one started) */
	stdio_devices[file] = pdev;

	/*
	 * Update monitor functions
	 * (to use the console stuff by other applications)
	 */
	switch (file) {
	case stdin:
		gd->jt[XF_getc] = pdev->getc;
		gd->jt[XF_tstc] = pdev->tstc;
		break;
	case stdout:
		gd->jt[XF_putc] = pdev->putc;
		gd->jt[XF_puts] = pdev->puts;
		gd->jt[XF_printf] = printf;
		break;
	}

	return 0;
}

/** U-Boot INITIAL CONSOLE-NOT COMPATIBLE FUNCTIONS *************************/

int serial_printf(const char *fmt, ...)
{
	va_list args;
	uint i;
	char printbuffer[CONFIG_SYS_PBSIZE];

	va_start(args, fmt);

	/* For this to work, printbuffer must be larger than
	 * anything we ever want to print.
	 */
	i = vsprintf(printbuffer, fmt, args);
	va_end(args);

	serial_puts(NULL, printbuffer);
	return i;
}

#ifndef CONFIG_CONSOLE_MUX
/* Similar functions for I/O multiplexing are defined in iomux.c */
int fgetc(int file)
{
	struct stdio_dev *pdev;

	if (file >= MAX_FILES)
		return -1;

	pdev = stdio_devices[file];
	return pdev->getc(pdev);
}

int ftstc(int file)
{
	struct stdio_dev *pdev;

	if (file >= MAX_FILES)
		return -1;

	pdev = stdio_devices[file];
	return pdev->tstc(pdev);
}

void fputc(int file, const char c)
{
	struct stdio_dev *pdev;

	if (file >= MAX_FILES)
		return;

	pdev = stdio_devices[file];
	pdev->putc(pdev, c);
}

void fputs(int file, const char *s)
{
	struct stdio_dev *pdev;

	if (file >= MAX_FILES)
		return;
	pdev = stdio_devices[file];
	pdev->puts(pdev, s);
}

static inline void console_doenv(int file, struct stdio_dev *dev)
{
	console_setfile(file, dev);
}
#endif /* defined(CONFIG_CONSOLE_MUX) */


int fprintf(int file, const char *fmt, ...)
{
	va_list args;
	uint i;
	char printbuffer[CONFIG_SYS_PBSIZE];

	va_start(args, fmt);

	/* For this to work, printbuffer must be larger than
	 * anything we ever want to print.
	 */
	i = vsprintf(printbuffer, fmt, args);
	va_end(args);

	/* Send to desired file */
	fputs(file, printbuffer);
	return i;
}



/** U-Boot INITIAL CONSOLE-COMPATIBLE FUNCTION *****************************/

int getc(void)
{
#ifdef CONFIG_DISABLE_CONSOLE
	if (gd->flags & GD_FLG_DISABLE_CONSOLE)
		return 0;
#endif

	if (!gd->have_console)
		return 0;

	if (gd->flags & GD_FLG_DEVINIT) {
		/* Get from the standard input */
		return fgetc(stdin);
	}

	/* Send directly to the handler */
	return serial_getc(NULL);
}

int tstc(void)
{
#ifdef CONFIG_DISABLE_CONSOLE
	if (gd->flags & GD_FLG_DISABLE_CONSOLE)
		return 0;
#endif

	if (!gd->have_console)
		return 0;

	if (gd->flags & GD_FLG_DEVINIT) {
		/* Test the standard input */
		return ftstc(stdin);
	}

	/* Send directly to the handler */
	return serial_tstc(NULL);
}

#if defined(CONFIG_PRE_CONSOLE_BUFFER) || defined(CONFIG_PRE_CONSOLE_PUTC)
#define CIRC_BUF_IDX(idx) ((idx) % (unsigned long)CONFIG_PRE_CON_BUF_SZ)

static void pre_console_putc(const char c)
{
#ifdef CONFIG_PRE_CONSOLE_BUFFER
	char *buffer = (char *)CONFIG_PRE_CON_BUF_ADDR;

	buffer[CIRC_BUF_IDX(gd->precon_buf_idx++)] = c;
#endif
#ifdef CONFIG_PRE_CONSOLE_PUTC
	board_pre_console_putc(c);
#endif
}

static void pre_console_puts(const char *s)
{
	while (*s)
		pre_console_putc(*s++);
}

static void print_pre_console_buffer(void)
{
#ifdef CONFIG_PRE_CONSOLE_BUFFER
	unsigned long i = 0;
	char *buffer = (char *)CONFIG_PRE_CON_BUF_ADDR;

	if (gd->precon_buf_idx > CONFIG_PRE_CON_BUF_SZ)
		i = gd->precon_buf_idx - CONFIG_PRE_CON_BUF_SZ;

	while (i < gd->precon_buf_idx)
		putc(buffer[CIRC_BUF_IDX(i++)]);
#endif
}

#else
static inline void pre_console_putc(const char c) {}
static inline void pre_console_puts(const char *s) {}
static inline void print_pre_console_buffer(void) {}
#endif

void putc(const char c)
{
#ifdef CONFIG_SILENT_CONSOLE
	if (gd->flags & GD_FLG_SILENT)
		return;
#endif

#ifdef CONFIG_DISABLE_CONSOLE
	if (gd->flags & GD_FLG_DISABLE_CONSOLE)
		return;
#endif

	if (!gd->have_console)
		pre_console_putc(c);
	else if (gd->flags & GD_FLG_DEVINIT)
		fputc(stdout, c);	  /* Send to the standard output */
	else
		serial_putc(NULL, c);	  /* Send directly to the handler */
}

void puts(const char *s)
{
#ifdef CONFIG_SILENT_CONSOLE
	if (gd->flags & GD_FLG_SILENT)
		return;
#endif

#ifdef CONFIG_DISABLE_CONSOLE
	if (gd->flags & GD_FLG_DISABLE_CONSOLE)
		return;
#endif

	if (!gd->have_console)
		pre_console_puts(s);
	else if (gd->flags & GD_FLG_DEVINIT)
		fputs(stdout, s);	  /* Send to the standard output */
	else
		serial_puts(NULL, s);	  /* Send directly to the handler */
}

int printf(const char *fmt, ...)
{
	va_list args;
	uint i;
	char printbuffer[CONFIG_SYS_PBSIZE];

#ifndef CONFIG_PRE_CONSOLE_BUFFER
	if (!gd->have_console)
		return 0;
#endif

	va_start(args, fmt);

	/* For this to work, printbuffer must be larger than
	 * anything we ever want to print.
	 */
	i = vsprintf(printbuffer, fmt, args);
	va_end(args);

	/* Print the string */
	puts(printbuffer);

	return i;
}

int vprintf(const char *fmt, va_list args)
{
	uint i;
	char printbuffer[CONFIG_SYS_PBSIZE];

#ifndef CONFIG_PRE_CONSOLE_BUFFER
	if (!gd->have_console)
		return 0;
#endif

	/* For this to work, printbuffer must be larger than
	 * anything we ever want to print.
	 */
	i = vsprintf(printbuffer, fmt, args);

	/* Print the string */
	puts(printbuffer);
	return i;
}

/* test if ctrl-c was pressed */
static int ctrlc_disabled = 0;	/* see disable_ctrl() */
static int ctrlc_was_pressed = 0;
int ctrlc(void)
{
	if (!ctrlc_disabled && gd->have_console) {
		if (tstc()) {
			switch (getc()) {
			case 0x03:		/* ^C - Control C */
				ctrlc_was_pressed = 1;
				return 1;
			default:
				break;
			}
		}
	}
	return 0;
}

/* pass 1 to disable ctrlc() checking, 0 to enable.
 * returns previous state
 */
int disable_ctrlc(int disable)
{
	int prev = ctrlc_disabled;	/* save previous state */

	ctrlc_disabled = disable;
	return prev;
}

int had_ctrlc (void)
{
	return ctrlc_was_pressed;
}

void clear_ctrlc(void)
{
	ctrlc_was_pressed = 0;
}

#ifdef CONFIG_MODEM_SUPPORT_DEBUG
char	screen[1024];
char *cursor = screen;
int once = 0;
inline void dbg(const char *fmt, ...)
{
	va_list	args;
	uint	i;
	char	printbuffer[CONFIG_SYS_PBSIZE];

	if (!once) {
		memset(screen, 0, sizeof(screen));
		once++;
	}

	va_start(args, fmt);

	/* For this to work, printbuffer must be larger than
	 * anything we ever want to print.
	 */
	i = vsprintf(printbuffer, fmt, args);
	va_end(args);

	if ((screen + sizeof(screen) - 1 - cursor)
	    < strlen(printbuffer) + 1) {
		memset(screen, 0, sizeof(screen));
		cursor = screen;
	}
	sprintf(cursor, printbuffer);
	cursor += strlen(printbuffer);

}
#else
inline void dbg(const char *fmt, ...)
{
}
#endif

/** U-Boot INIT FUNCTIONS *************************************************/

static struct stdio_dev *search_device(int flags, const char *name)
{
	struct stdio_dev *pdev;

	pdev = stdio_get_by_name(name);

	if (pdev && (pdev->flags & flags))
		return pdev;

	return NULL;
}

int console_assign(int file, const char *devname)
{
	int flag;
	struct stdio_dev *pdev;

	/* Check for valid file */
	switch (file) {
	case stdin:
		flag = DEV_FLAGS_INPUT;
		break;
	case stdout:
	case stderr:
		flag = DEV_FLAGS_OUTPUT;
		break;
	default:
		return -1;
	}

	/* Check for valid device name */

	pdev = search_device(flag, devname);
	if (pdev)
		return console_setfile(file, pdev);

	return -1;
}

/* Called before relocation - use serial functions */
int console_init_f(void)
{
	gd->have_console = 1;

#ifdef CONFIG_SILENT_CONSOLE
	if (getenv("silent") != NULL)
		gd->flags |= GD_FLG_SILENT;
#endif

	print_pre_console_buffer();

	return 0;
}

void stdio_print_current_devices(void)
{
#ifndef CONFIG_SYS_CONSOLE_INFO_QUIET
	/* Print information */
	puts("In:    ");
	if (stdio_devices[stdin] == NULL) {
		puts("No input devices available!\n");
	} else {
#ifdef CONFIG_CONSOLE_MUX
		struct stdio_dev *p;

		for (p = stdio_devices[stdin]; p; p = p->file_next[stdin])
			printf("%s ", p->name);
#else
		printf("%s\n", stdio_devices[stdin]->name);
#endif
	}

	puts("Out:   ");
	if (stdio_devices[stdout] == NULL) {
		puts("No output devices available!\n");
	} else {
#ifdef CONFIG_CONSOLE_MUX
		struct stdio_dev *p;

		for (p = stdio_devices[stdout]; p; p = p->file_next[stdout])
			printf("%s ", p->name);
#else
		printf("%s\n", stdio_devices[stdout]->name);
#endif
	}

	puts("Err:   ");
	if (stdio_devices[stderr] == NULL) {
		puts("No error devices available!\n");
	} else {
#ifdef CONFIG_CONSOLE_MUX
		struct stdio_dev *p;

		for (p = stdio_devices[stderr]; p; p = p->file_next[stderr])
			printf("%s ", p->name);
#else
		printf("%s\n", stdio_devices[stderr]->name);
#endif
	}
#endif /* CONFIG_SYS_CONSOLE_INFO_QUIET */
}

#ifdef CONFIG_SYS_CONSOLE_IS_IN_ENV
/* Called after the relocation - use desired console functions */
int console_init_r(void)
{
	char *stdinname, *stdoutname, *stderrname;
	struct stdio_dev *inputdev = NULL, *outputdev = NULL, *errdev = NULL;
#ifdef CONFIG_SYS_CONSOLE_ENV_OVERWRITE
	int i;
#endif /* CONFIG_SYS_CONSOLE_ENV_OVERWRITE */
#ifdef CONFIG_CONSOLE_MUX
#endif

	/* set default handlers at first */
	gd->jt[XF_getc] = serial_getc;
	gd->jt[XF_tstc] = serial_tstc;
	gd->jt[XF_putc] = serial_putc;
	gd->jt[XF_puts] = serial_puts;
	gd->jt[XF_printf] = serial_printf;

	/* stdin stdout and stderr are in environment */
	/* scan for it */
	stdinname  = getenv("stdin");
	stdoutname = getenv("stdout");
	stderrname = getenv("stderr");

	if (OVERWRITE_CONSOLE == 0) {	/* if not overwritten by config switch */
#ifdef CONFIG_CONSOLE_MUX
		int iomux_err = 0;

		iomux_err = iomux_doenv(stdin, stdinname);
		iomux_err += iomux_doenv(stdout, stdoutname);
		iomux_err += iomux_doenv(stderr, stderrname);
		if (!iomux_err)
			/* Successful, so skip all the code below. */
			goto done;
#endif
		inputdev  = search_device(DEV_FLAGS_INPUT,  stdinname);
		outputdev = search_device(DEV_FLAGS_OUTPUT, stdoutname);
		errdev    = search_device(DEV_FLAGS_OUTPUT, stderrname);
	}
	/* if the devices are overwritten or not found, use default device */
	if (inputdev == NULL) {
		inputdev  = search_device(DEV_FLAGS_INPUT,  "serial");
	}
	if (outputdev == NULL) {
		outputdev = search_device(DEV_FLAGS_OUTPUT, "serial");
	}
	if (errdev == NULL) {
		errdev    = search_device(DEV_FLAGS_OUTPUT, "serial");
	}
	/* Initializes output console first */
	if (outputdev != NULL) {
		/* need to set a console if not done above. */
		console_doenv(stdout, outputdev);
	}
	if (errdev != NULL) {
		/* need to set a console if not done above. */
		console_doenv(stderr, errdev);
	}
	if (inputdev != NULL) {
		/* need to set a console if not done above. */
		console_doenv(stdin, inputdev);
	}

#ifdef CONFIG_CONSOLE_MUX
done:
#endif

	gd->flags |= GD_FLG_DEVINIT;	/* device initialization completed */

	stdio_print_current_devices();

#ifdef CONFIG_SYS_CONSOLE_ENV_OVERWRITE
	/* set the environment variables (will overwrite previous env settings) */
	for (i = 0; i < 3; i++) {
		setenv(stdio_names[i], stdio_devices[i]->name);
	}
#endif /* CONFIG_SYS_CONSOLE_ENV_OVERWRITE */

	return 0;
}

#else /* CONFIG_SYS_CONSOLE_IS_IN_ENV */

/* Called after the relocation - use desired console functions */
int console_init_r(void)
{
	struct stdio_dev *inputdev = NULL, *outputdev = NULL;
	int i;
	struct stdio_dev *dev;

#ifdef CONFIG_SPLASH_SCREEN
	/*
	 * suppress all output if splash screen is enabled and we have
	 * a bmp to display. We redirect the output from frame buffer
	 * console to serial console in this case or suppress it if
	 * "silent" mode was requested.
	 */
	if (getenv("splashimage") != NULL) {
		if (!(gd->flags & GD_FLG_SILENT))
			outputdev = search_device (DEV_FLAGS_OUTPUT, "serial");
	}
#endif

	/* Scan devices looking for input and output devices */
	for (dev = stdio_get_list(); dev; dev = dev->next) {
		if ((dev->flags & DEV_FLAGS_INPUT) && (inputdev == NULL)) {
			inputdev = dev;
		}
		if ((dev->flags & DEV_FLAGS_OUTPUT) && (outputdev == NULL)) {
			outputdev = dev;
		}
		if(inputdev && outputdev)
			break;
	}

	/* Initializes output console first */
	if (outputdev != NULL) {
		console_setfile(stdout, outputdev);
		console_setfile(stderr, outputdev);
	}

	/* Initializes input console */
	if (inputdev != NULL)
		console_setfile(stdin, inputdev);

	gd->flags |= GD_FLG_DEVINIT;	/* device initialization completed */

	stdio_print_current_devices();

	/* Setting environment variables */
	for (i = 0; i < 3; i++) {
		setenv(stdio_names[i], stdio_devices[i]->name);
	}

	return 0;
}

#endif /* CONFIG_SYS_CONSOLE_IS_IN_ENV */
