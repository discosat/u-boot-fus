/*
 * (C) Copyright 2000
 * Paolo Scaffardi, AIRVENT SAM s.p.a - RIMINI(ITALY), arsenio@tin.it
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <console.h>
#include <debug_uart.h>
#include <stdarg.h>
#include <malloc.h>
#include <os.h>
#include <serial.h>			  /* serial_*() */
#include <stdio_dev.h>
#include <exports.h>
#include <environment.h>
#include <watchdog.h>

DECLARE_GLOBAL_DATA_PTR;

static int on_console(const char *name, const char *value, enum env_op op,
	int flags)
{
	int console = -1;

	/* Check for console redirection */
	if (strcmp(name, "stdin") == 0)
		console = stdin;
	else if (strcmp(name, "stdout") == 0)
		console = stdout;
	else if (strcmp(name, "stderr") == 0)
		console = stderr;

	/* if not actually setting a console variable, we don't care */
	if (console == -1 || (gd->flags & GD_FLG_DEVINIT) == 0)
		return 0;

	switch (op) {
	case env_op_create:
	case env_op_overwrite:

#ifdef CONFIG_CONSOLE_MUX
		if (iomux_doenv(console, value))
			return 1;
#else
		/* Try assigning specified device */
		if (console_assign(console, value) < 0)
			return 1;
#endif /* CONFIG_CONSOLE_MUX */
		return 0;

	case env_op_delete:
		if ((flags & H_FORCE) == 0)
			printf("Can't delete \"%s\"\n", name);
		return 1;

	default:
		return 0;
	}
}
U_BOOT_ENV_CALLBACK(console, on_console);

#ifdef CONFIG_SILENT_CONSOLE
static int on_silent(const char *name, const char *value, enum env_op op,
	int flags)
{
#if !CONFIG_IS_ENABLED(CONFIG_SILENT_CONSOLE_UPDATE_ON_SET)
	if (flags & H_INTERACTIVE)
		return 0;
#endif
#if !CONFIG_IS_ENABLED(CONFIG_SILENT_CONSOLE_UPDATE_ON_RELOC)
	if ((flags & H_INTERACTIVE) == 0)
		return 0;
#endif

	if (value != NULL)
		gd->flags |= GD_FLG_SILENT;
	else
		gd->flags &= ~GD_FLG_SILENT;

	return 0;
}
U_BOOT_ENV_CALLBACK(silent, on_silent);
#endif

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
		gd->jt->getc = getc;
		gd->jt->tstc = tstc;
		break;
	case stdout:
		gd->jt->putc = putc;
		gd->jt->puts = puts;
		gd->jt->printf = printf;
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
	i = vscnprintf(printbuffer, sizeof(printbuffer), fmt, args);
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
	i = vscnprintf(printbuffer, sizeof(printbuffer), fmt, args);
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

#if CONFIG_IS_ENABLED(PRE_CONSOLE_BUFFER)
#define CIRC_BUF_IDX(idx) ((idx) % (unsigned long)CONFIG_PRE_CON_BUF_SZ)

static void pre_console_putc(const char c)
{
	char *buffer = (char *)CONFIG_PRE_CON_BUF_ADDR;

	buffer[CIRC_BUF_IDX(gd->precon_buf_idx++)] = c;
}

static void pre_console_puts(const char *s)
{
	while (*s)
		pre_console_putc(*s++);
}

static void print_pre_console_buffer(void)
{
	unsigned long i = 0;
	char *buffer = (char *)CONFIG_PRE_CON_BUF_ADDR;

	if (gd->precon_buf_idx > CONFIG_PRE_CON_BUF_SZ)
		i = gd->precon_buf_idx - CONFIG_PRE_CON_BUF_SZ;

	while (i < gd->precon_buf_idx)
		putc(buffer[CIRC_BUF_IDX(i++)]);
}
#else
static inline void pre_console_putc(const char c) {}
static inline void pre_console_puts(const char *s) {}
static inline void print_pre_console_buffer(void) {}
#endif

void putc(const char c)
{
#ifdef CONFIG_SANDBOX
	/* sandbox can send characters to stdout before it has a console */
	if (!gd || !(gd->flags & GD_FLG_SERIAL_READY)) {
		os_putc(c);
		return;
	}
#endif
#ifdef CONFIG_DEBUG_UART
	/* if we don't have a console yet, use the debug UART */
	if (!gd || !(gd->flags & GD_FLG_SERIAL_READY)) {
		printch(c);
		return;
	}
#endif
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
#ifdef CONFIG_SANDBOX
	if (!gd || !(gd->flags & GD_FLG_SERIAL_READY)) {
		os_puts(s);
		return;
	}
#endif
#ifdef CONFIG_DEBUG_UART
	if (!gd || !(gd->flags & GD_FLG_SERIAL_READY)) {
		while (*s) {
			int ch = *s++;

			printch(ch);
		}
		return;
	}
#endif
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

/* test if ctrl-c was pressed */
static int ctrlc_disabled = 0;	/* see disable_ctrl() */
static int ctrlc_was_pressed = 0;
int ctrlc(void)
{
#ifndef CONFIG_SANDBOX
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
#endif

	return 0;
}
/* Reads user's confirmation.
   Returns 1 if user's input is "y", "Y", "yes" or "YES"
*/
int confirm_yesno(void)
{
	int i;
	char str_input[5];

	/* Flush input */
	while (tstc())
		getc();
	i = 0;
	while (i < sizeof(str_input)) {
		str_input[i] = getc();
		putc(str_input[i]);
		if (str_input[i] == '\r')
			break;
		i++;
	}
	putc('\n');
	if (strncmp(str_input, "y\r", 2) == 0 ||
	    strncmp(str_input, "Y\r", 2) == 0 ||
	    strncmp(str_input, "yes\r", 4) == 0 ||
	    strncmp(str_input, "YES\r", 4) == 0)
		return 1;
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

/** U-Boot INIT FUNCTIONS *************************************************/

static struct stdio_dev *search_device(int flags, const char *name)
{
	struct stdio_dev *pdev;

	pdev = stdio_get_by_name(name);
#ifdef CONFIG_VIDCONSOLE_AS_LCD
	if (!pdev && !strcmp(name, "lcd"))
		pdev = stdio_get_by_name("vidconsole");
#endif

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

static void console_update_silent(void)
{
#ifdef CONFIG_SILENT_CONSOLE
	if (getenv("silent") != NULL)
		gd->flags |= GD_FLG_SILENT;
	else
		gd->flags &= ~GD_FLG_SILENT;
#endif
}

/* Called before relocation - use serial functions */
int console_init_f(void)
{
	gd->have_console = 1;

	console_update_silent();

	print_pre_console_buffer();

	return 0;
}

void stdio_print_current_devices(void)
{
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

	/* set default handlers at first */
	gd->jt->getc = getc;
	gd->jt->tstc = tstc;
	gd->jt->putc = putc;
	gd->jt->puts = puts;
	gd->jt->printf = printf;

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

#ifndef CONFIG_SYS_CONSOLE_INFO_QUIET
	stdio_print_current_devices();
#endif /* CONFIG_SYS_CONSOLE_INFO_QUIET */
#ifdef CONFIG_VIDCONSOLE_AS_LCD
	if (strstr(stdoutname, "lcd"))
		printf("Warning: Please change 'lcd' to 'vidconsole' in stdout/stderr environment vars\n");
#endif

#ifdef CONFIG_SYS_CONSOLE_ENV_OVERWRITE
	/* set the environment variables (will overwrite previous env settings) */
	for (i = 0; i < 3; i++) {
		setenv(stdio_names[i], stdio_devices[i]->name);
	}
#endif /* CONFIG_SYS_CONSOLE_ENV_OVERWRITE */

	gd->flags |= GD_FLG_DEVINIT;	/* device initialization completed */

	return 0;
}

#else /* CONFIG_SYS_CONSOLE_IS_IN_ENV */

/* Called after the relocation - use desired console functions */
int console_init_r(void)
{
	struct stdio_dev *inputdev = NULL, *outputdev = NULL;
	int i;
	struct stdio_dev *dev;

	console_update_silent();

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

#ifndef CONFIG_SYS_CONSOLE_INFO_QUIET
	stdio_print_current_devices();
#endif /* CONFIG_SYS_CONSOLE_INFO_QUIET */

	/* Setting environment variables */
	for (i = 0; i < 3; i++) {
		setenv(stdio_names[i], stdio_devices[i]->name);
	}

	gd->flags |= GD_FLG_DEVINIT;	/* device initialization completed */

	return 0;
}

#endif /* CONFIG_SYS_CONSOLE_IS_IN_ENV */
