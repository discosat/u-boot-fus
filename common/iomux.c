/*
 * (C) Copyright 2008
 * Gary Jennejohn, DENX Software Engineering GmbH, garyj@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <console.h>
#include <serial.h>
#include <malloc.h>

static struct stdio_dev *tstcdev;

static int console_tstc(int file)
{
	int ret;
	struct stdio_dev *pdev;

	disable_ctrlc(1);
	for (pdev = stdio_devices[file]; pdev; pdev = pdev->file_next[file]) {
		if (pdev->tstc != NULL) {
			ret = pdev->tstc(pdev);
			if (ret > 0) {
				tstcdev = pdev;
				disable_ctrlc(0);
				return ret;
			}
		}
	}
	disable_ctrlc(0);

	return 0;
}

int fgetc(int file)
{
	if (file >= MAX_FILES)
		return -1;

	/*
	 * Effectively poll for input wherever it may be available.
	 * No attempt is made to demultiplex multiple input sources.
	 */
	for (;;) {
		/*
		 * Upper layer may have already called ftstc() so
		 * check for that first.
		 */
		if (tstcdev != NULL) {
			int ret;

			ret = tstcdev->getc(tstcdev);
			tstcdev = NULL;
			return ret;
		}
		console_tstc(file);
#ifdef CONFIG_WATCHDOG
		/*
		 * If the watchdog must be rate-limited then it should
		 * already be handled in board-specific code.
		 */
		udelay(1);
#endif
	}
}

int ftstc(int file)
{
	if (file >= MAX_FILES)
		return -1;

	return console_tstc(file);
}

void fputc(int file, const char c)
{
	struct stdio_dev *pdev;

	if (file >= MAX_FILES)
		return;

	/* Put the character to all devices in the list of that file */
	for (pdev = stdio_devices[file]; pdev; pdev = pdev->file_next[file]) {
		if (pdev->putc != NULL)
			pdev->putc(pdev, c);
	}
}

void fputs(int file, const char *s)
{
	struct stdio_dev *pdev;

	if (file >= MAX_FILES)
		return;

	/* Put the string to all devices in the list of that file */
	for (pdev = stdio_devices[file]; pdev; pdev = pdev->file_next[file]) {
		if (pdev->puts != NULL)
			pdev->puts(pdev, s);
	}
}

void console_doenv(int file, struct stdio_dev *new_dev)
{
	struct stdio_dev *pdev;

	/* Clear the current lists for this file */
	for (pdev = stdio_devices[file]; pdev; pdev = pdev->next)
		pdev->file_next[file] = NULL;

	/* Set the new device for this file */
	stdio_devices[file] = new_dev;
}

/* This tries to preserve the old list if an error occurs. */
int iomux_doenv(int file, const char *arg)
{
	struct stdio_dev *pdev;
	struct stdio_dev *p;
	int i;
	int io_flag;

	switch (file) {
	case stdin:
		io_flag = DEV_FLAGS_INPUT;
		break;
	case stdout:
	case stderr:
		io_flag = DEV_FLAGS_OUTPUT;
		break;
	default:
		return -1;
	}

	/* Clear the current lists for stdin, stdout, stderr */
	for (i = 0; i < MAX_FILES; i++) {
		for (pdev = stdio_devices[i]; pdev; pdev = pdev->next)
			pdev->file_next[i] = NULL;
		stdio_devices[i] = NULL;
	}
	
	for (;;) {
		/* Skip empty arguments */
		while (*arg == ',')
			arg++;
		if (!*arg)
			break;

		/* Search if a device with this name exists; we can't use
		   strcmp() as the argument may also be delimited with ',' */
		for (pdev = stdio_get_list(); pdev; pdev = pdev->next) {
			const char *tmp;
			const char *name;
			char n, c;

			/* Check if device supports this transfer direction */
			if (!(pdev->flags & io_flag))
				continue;

			tmp = arg - 1;
			name = pdev->name;
			do {
				c = *(++tmp);
				if (c == ',')
					c = 0;
				n = *name++;
			} while ((c == n) && c && n);
			if (!c && !n) {
				arg = tmp;
				break;
			}
		}
		if (!pdev)
			continue;

		/* Check if device is already in list */
		for (p = stdio_devices[file]; p; p = p->file_next[file]) {
			if (p == pdev)
				break;
		}
		if (p)
			continue;

		/* Start the device; if this fails, don't use it */
		if (pdev->start) {
			if (pdev->start(pdev) < 0)
				continue;
		}

		/* Prepend to list for this file */
		pdev->next = stdio_devices[file];
		stdio_devices[file] = pdev;
	}

	return 0;
}
