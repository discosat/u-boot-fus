/*
 * Copyright (c) 2013, Google Inc.
 *
 * Copyright (C) 2011
 * Corscience GmbH & Co. KG - Simon Schwarz <schwarz@corscience.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef ARM_BOOTM_H
#define ARM_BOOTM_H

extern void udc_disconnect(void);

#if defined(CONFIG_SETUP_MEMORY_TAGS) || \
		defined(CONFIG_CMDLINE_TAG) || \
		defined(CONFIG_INITRD_TAG) || \
		defined(CONFIG_SERIAL_TAG) || \
		defined(CONFIG_FSHWCONFIG_TAG) || \
		defined(CONFIG_FSM4CONFIG_TAG) || \
		defined(CONFIG_REVISION_TAG)
# define BOOTM_ENABLE_TAGS		1
#else
# define BOOTM_ENABLE_TAGS		0
#endif

#ifdef CONFIG_SETUP_MEMORY_TAGS
# define BOOTM_ENABLE_MEMORY_TAGS	1
#else
# define BOOTM_ENABLE_MEMORY_TAGS	0
#endif

#ifdef CONFIG_CMDLINE_TAG
 #define BOOTM_ENABLE_CMDLINE_TAG	1
#else
 #define BOOTM_ENABLE_CMDLINE_TAG	0
#endif

#ifdef CONFIG_INITRD_TAG
 #define BOOTM_ENABLE_INITRD_TAG	1
#else
 #define BOOTM_ENABLE_INITRD_TAG	0
#endif

#ifdef CONFIG_SERIAL_TAG
 #define BOOTM_ENABLE_SERIAL_TAG	1
void get_board_serial(struct tag_serialnr *serialnr);
#else
 #define BOOTM_ENABLE_SERIAL_TAG	0
static inline void get_board_serial(struct tag_serialnr *serialnr)
{
}
#endif

#ifdef CONFIG_REVISION_TAG
 #define BOOTM_ENABLE_REVISION_TAG	1
u32 get_board_rev(void);
#else
 #define BOOTM_ENABLE_REVISION_TAG	0
static inline u32 get_board_rev(void)
{
	return 0;
}
#endif

#ifdef CONFIG_FSHWCONFIG_TAG
 #define BOOTM_ENABLE_FSHWCONFIG_TAG	1
struct tag_fshwconfig *get_board_fshwconfig(void);
#else
 #define BOOTM_ENABLE_FSHWCONFIG_TAG	0
static inline struct tag_fshwconfig *get_board_fshwconfig(void)
{
	return NULL;
}
#endif

#ifdef CONFIG_FSM4CONFIG_TAG
 #define BOOTM_ENABLE_FSM4CONFIG_TAG	1
struct tag_fsm4config *get_board_fsm4config(void);
#else
 #define BOOTM_ENABLE_FSM4CONFIG_TAG	0
static inline struct tag_fsm4config *get_board_fsm4config(void)
{
	return NULL;
}
#endif

#endif
