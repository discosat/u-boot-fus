/*
 * fsvybrid.c
 *
 * (C) Copyright 2015
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Board specific functions for F&S boards based on Freescale Vybrid CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/errno.h>			/* ENODEV */
#ifdef CONFIG_CMD_NET
#include <asm/fec.h>
#include <net.h>			/* eth_init(), eth_halt() */
#include <netdev.h>			/* ne2000_initialize() */
#endif
#ifdef CONFIG_CMD_LCD
#include <cmd_lcd.h>			/* PON_*, POFF_* */
#endif
#include <serial.h>			/* struct serial_device */

#ifdef CONFIG_GENERIC_MMC
#include <mmc.h>
#include <fsl_esdhc.h>			/* fsl_esdhc_initialize(), ... */
#endif

#ifdef CONFIG_CMD_LED
#include <status_led.h>			/* led_id_t */
#endif

#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/setup.h>			/* struct tag_fshwconfig, ... */
#include <asm/arch/vybrid-regs.h>	/* SCSCM_BASE_ADDR, ... */
#include <asm/arch/vybrid-pins.h>
#include <asm/arch/iomux.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/scsc_regs.h>		/* struct vybrid_scsc_reg */
#include <asm/arch/clock.h>		/* vybrid_get_esdhc_clk() */
#include <i2c.h>

#include <linux/mtd/nand.h>		/* struct mtd_info, struct nand_chip */
#include <mtd/fsl_nfc_fus.h>		/* struct fsl_nfc_fus_platform_data */
#include <usb/ehci-fsl.h>
#include <fdt_support.h>		/* do_fixup_by_path_u32(), ... */

#ifdef CONFIG_FSL_ESDHC
struct fsl_esdhc_cfg esdhc_cfg[] = {
	{
		.esdhc_base = ESDHC0_BASE_ADDR,
		.sdhc_clk = 0,
		.max_bus_width = 4,
	},
	{
		.esdhc_base = ESDHC1_BASE_ADDR,
		.sdhc_clk = 0,
		.max_bus_width = 4,
	},
};
#endif

/* ------------------------------------------------------------------------- */

#define NBOOT_ARGS_BASE (PHYS_SDRAM_0 + 0x00001000) /* Arguments from NBoot */
#define BOOT_PARAMS_BASE (PHYS_SDRAM_0 + 0x100)	    /* Arguments to Linux */

#define BT_ARMSTONEA5 0
#define BT_PICOCOMA5  1
#define BT_NETDCUA5   2
#define BT_PICOMODA5  4
#define BT_PICOMOD1_2 5
#define BT_AGATEWAY   6
#define BT_CUBEA5     7
#define BT_HGATEWAY   8

/* Features set in tag_fshwconfig.chFeature1 */
#define FEAT1_CPU400  (1<<0)		/* 0: 500 MHz, 1: 400 MHz CPU */
#define FEAT1_2NDCAN  (1<<1)		/* 0: 1x CAN, 1: 2x CAN */
#define FEAT1_2NDLAN  (1<<4)		/* 0: 1x LAN, 1: 2x LAN */

/* Features set in tag_fshwconfig.chFeature2 */
#define FEAT2_M4      (1<<0)		/* CPU has Cortex-M4 core */
#define FEAT2_L2      (1<<1)		/* CPU has Level 2 cache */
#define FEAT2_RMIICLK_CKO1 (1<<2)	/* RMIICLK (PTA6) 0: output, 1: input
					   CKO1 (PTB10) 0: unused, 1: output */

#define ACTION_RECOVER 0x00000040	/* Start recovery instead of update */

#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)

struct board_info {
	char *name;			/* Device name */
	unsigned int mach_type;		/* Device machine ID */
	char *bootdelay;		/* Default value for bootdelay */
	char *updatecheck;		/* Default value for updatecheck */
	char *installcheck;		/* Default value for installcheck */
	char *recovercheck;		/* Default value for recovercheck */
	char *earlyusbinit;		/* Default value for earlyusbinit */
	char *console;			/* Default variable for console */
	char *login;			/* Default variable for login */
	char *mtdparts;			/* Default variable for mtdparts */
	char *network;			/* Default variable for network */
	char *init;			/* Default variable for init */
	char *rootfs;			/* Default variable for rootfs */
	char *kernel;			/* Default variable for kernel */
	char *fdt;			/* Default variable for device tree */
};

#define INSTALL_RAM "ram@80300000"
#if defined(CONFIG_MMC) && defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
#define UPDATE_DEF "mmc,usb"
#define INSTALL_DEF INSTALL_RAM "," UPDATE_DEF
#elif defined(CONFIG_MMC) && defined(CONFIG_FS_FAT)
#define UPDATE_DEF "mmc"
#define INSTALL_DEF INSTALL_RAM "," UPDATE_DEF
#elif defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
#define UPDATE_DEF "usb"
#define INSTALL_DEF INSTALL_RAM "," UPDATE_DEF
#else
#define UPDATE_DEF NULL
#define INSTALL_DEF INSTALL_RAM
#endif
#if defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
#define EARLY_USB "1"
#else
#define EARLY_USB NULL
#endif

const struct board_info fs_board_info[16] = {
	{	/* 0 (BT_ARMSTONEA5) */
		.name = "armStoneA5",
		.mach_type = MACH_TYPE_ARMSTONEA5,
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.earlyusbinit = NULL,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_linuxrc",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
	},
	{	/* 1 (BT_PICOCOMA5)*/
		.name = "PicoCOMA5",
		.mach_type = MACH_TYPE_PICOCOMA5,
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.earlyusbinit = NULL,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_linuxrc",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
	},
	{	/* 2 (BT_NETDCUA5) */
		.name = "NetDCUA5",
		.mach_type = MACH_TYPE_NETDCUA5,
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = INSTALL_DEF,
		.recovercheck = UPDATE_DEF,
		.earlyusbinit = NULL,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_linuxrc",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
	},
	{	/* 3 */
		.name = "Unknown",
	},
	{	/* 4 (BT_PICOMODA5) */
		.name = "PicoMODA5",
		.mach_type = 0,		/*####not yet registered*/
	},
	{	/* 5 (BT_PICOMOD1_2) */
		.name = "PicoMOD1.2",
		.mach_type = 0,		/*####not yet registered*/
	},
	{	/* 6 (BT_AGATEWAY) */
		.name = "AGATEWAY",
		.mach_type = MACH_TYPE_AGATEWAY,
		.bootdelay = "0",
		.updatecheck = "TargetFS.ubi(ubi0:data)",
		.installcheck = INSTALL_RAM,
		.recovercheck = "TargetFS.ubi(ubi0:recovery)",
		.earlyusbinit = NULL,
//###		.console = ".console_serial",
		.console = ".console_none",
//###		.login = ".login_serial",
		.login = ".login_none",
		.mtdparts = ".mtdparts_ubionly",
		.network = ".network_off",
		.init = ".init_linuxrc",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_ubifs",
		.fdt = ".fdt_nand",
	},
	{	/* 7 (BT_CUBEA5) */
		.name = "CUBEA5",
		.mach_type = MACH_TYPE_CUBEA5,
		.bootdelay = "0",
		.updatecheck = "TargetFS.ubi(ubi0:data)",
		.installcheck = INSTALL_RAM,
		.recovercheck = "TargetFS.ubi(ubi0:recovery)",
		.earlyusbinit = NULL,
		.console = ".console_none",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_ubionly",
		.network = ".network_off",
		.init = ".init_linuxrc",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_ubifs",
		.fdt = ".fdt_nand",
	},
	{	/* 8 (BT_HGATEWAY) */
		.name = "HGATEWAY",
		.mach_type = MACH_TYPE_HGATEWAY,
		.bootdelay = "0",
		.updatecheck = "TargetFS.ubi(ubi0:data)",
		.installcheck = INSTALL_RAM,
		.recovercheck = "TargetFS.ubi(ubi0:recovery)",
		.earlyusbinit = NULL,
//###		.console = ".console_serial",
		.console = ".console_none",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_ubionly",
		.network = ".network_off",
		.init = ".init_linuxrc",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_ubifs",
		.fdt = ".fdt_nand",
	},
	{	/* 9 */
		.name = "Unknown",
	},
	{	/* 10 */
		.name = "Unknown",
	},
	{	/* 11 */
		.name = "Unknown",
	},
	{	/* 12 */
		.name = "Unknown",
	},
	{	/* 13 */
		.name = "Unknown",
	},
	{	/* 14 */
		.name = "Unknown",
	},
	{	/* 15 */
		.name = "Unknown",
	},
};

/* String used for system prompt */
static char fs_sys_prompt[20];

/* Copy of the NBoot args, split into hwconfig and m4config */
struct tag_fshwconfig fs_nboot_args;
struct tag_fsm4config fs_m4_args;


/*
 * Miscellaneous platform dependent initialisations. Boot Sequence:
 *
 * Nr. File             Function        Comment
 * -----------------------------------------------------------------
 *  1.  start.S         reset()                 Init CPU, invalidate cache
 *  2.  fsvybrid.c      save_boot_params()      (unused)
 *  3.  lowlevel_init.S lowlevel_init()         Init clocks, etc.
 *  4.  board.c         board_init_f()          Init from flash (without RAM)
 *  5.  cpu_info.c      arch_cpu_init()         CPU type (PC100, PC110/PV210)
 *  6.  fsvybrid.c      board_early_init_f()    Set up NAND flash ###
 *  7.  timer.c         timer_init()            Init timer for udelay()
 *  8.  env_nand.c      env_init()              Prepare to read env from NAND
 *  9.  board.c         init_baudrate()         Get the baudrate from env
 * 10.  serial.c        serial_init()           Start default serial port
 * 10a. fsvybrid.c      default_serial_console() Get serial debug port
 * 11.  console.c       console_init_f()        Early console on serial port
 * 12.  board.c         display_banner()        Show U-Boot version
 * 13.  cpu_info.c      print_cpuinfo()         Show CPU type
 * 14.  fsvybrid.c      checkboard()            Check NBoot params, show board
 * 15.  fsvybrid.c      dram_init()             Set DRAM size to global data
 * 16.  cmd_lcd.c       lcd_setmem()            Reserve framebuffer region
 * 17.  fsvybrid.c      dram_init_banksize()    Set ram banks to board desc.
 * 18.  board.c         display_dram_config()   Show DRAM info
 * 19.  lowlevel_init.S setup_mmu_table()       Init MMU table
 * 20.  start.S         relocate_code()         Relocate U-Boot to end of RAM,
 * 21.  board.c         board_init_r()          Init from RAM
 * 22.  cache.c         enable_caches()         Switch on caches
 * 23.  fsvybrid.c      board_init()            Set boot params, config CS
 * 24.  serial.c        serial_initialize()     Register serial devices
 * 24a. fsvybrid.c      board_serial_init()     (unused)
 * 25.  dlmalloc.c      mem_malloc_init()       Init heap for malloc()
 * 26.  nand.c          nand_init()             Scan NAND devices
 * 26a. fsl_nfc_fus.c   board_nand_init()       Set fsvybrid NAND config
 * 26b. fsl_nfc_fus.c   board_nand_setup()      Set OOB layout and ECC mode
 * 26c. fsvybrid.c      board_nand_setup_vybrid()  Set OOB layout and ECC mode
 * 27.  mmc.c           mmc_initialize()        Scan MMC slots
 * 27a. fsvybrid.c      board_mmc_init()        Set fss5pv210 MMC config
 * 28.  env_common.c    env_relocate()          Copy env to RAM
 * 29.  stdio_init.c    stdio_init()            Set I/O devices
 * 30.  cmd_lcd.c       drv_lcd_init()          Register LCD devices
 * 31.  serial.c        serial_stdio_init()     Register serial devices
 * 32.  exports.c       jumptable_init()        Table for exported functions
 * 33.  api.c           api_init()              (unused)
 * 34.  console.c       console_init_r()        Start full console
 * 35.  fsvybrid.c      arch_misc_init()        (unused)
 * 36.  fsvybrid.c      misc_init_r()           (unused)
 * 37.  interrupts.c    interrupt_init()        (unused)
 * 38.  interrupts.c    enable_interrupts()     (unused)
 * 39.  fsvybrid.c      board_late_init()       Set additional environment
 * 40.  eth.c           eth_initialize()        Register eth devices
 * 40a. fsvybrid.c      board_eth_init()        Set fss5pv210 eth config
 * 41.  cmd_source.c    update_script()         Run update script
 * 42.  main.c          main_loop()             Handle user commands
 *
 * The basic idea is to call some protocol specific driver_init() function in
 * board.c. This calls a board_driver_init() function here where all required
 * GPIOs and other initializations for this device are done. Then a device
 * specific init function is called that registers the appropriate devices at
 * the protocol driver. Then the local function returns and the registered
 * devices can be listed.
 *
 * Example: - board.c calls eth_initialize()
 *	    - eth_initialize() calls board_eth_init() here; we reset one or
 *	      two AX88796 devices and register them with ne2000_initialize();
 *	      this in turn calls eth_register(). Then we return.
 *	    - eth_initialize() continues and lists all registered eth devices
 */

#ifdef CONFIG_NAND_FSL_NFC
static void setup_iomux_nfc(void)
{
	__raw_writel(0x002038df, IOMUXC_PAD_063);
	__raw_writel(0x002038df, IOMUXC_PAD_064);
	__raw_writel(0x002038df, IOMUXC_PAD_065);
	__raw_writel(0x002038df, IOMUXC_PAD_066);
	__raw_writel(0x002038df, IOMUXC_PAD_067);
	__raw_writel(0x002038df, IOMUXC_PAD_068);
	__raw_writel(0x002038df, IOMUXC_PAD_069);
	__raw_writel(0x002038df, IOMUXC_PAD_070);
	__raw_writel(0x002038df, IOMUXC_PAD_071);
	__raw_writel(0x002038df, IOMUXC_PAD_072);
	__raw_writel(0x002038df, IOMUXC_PAD_073);
	__raw_writel(0x002038df, IOMUXC_PAD_074);
	__raw_writel(0x002038df, IOMUXC_PAD_075);
	__raw_writel(0x002038df, IOMUXC_PAD_076);
	__raw_writel(0x002038df, IOMUXC_PAD_077);
	__raw_writel(0x002038df, IOMUXC_PAD_078);

	__raw_writel(0x005038d2, IOMUXC_PAD_094);
	__raw_writel(0x005038d2, IOMUXC_PAD_095);
	__raw_writel(0x006038d2, IOMUXC_PAD_097);
	__raw_writel(0x005038dd, IOMUXC_PAD_099);
	__raw_writel(0x006038d2, IOMUXC_PAD_100);
	__raw_writel(0x006038d2, IOMUXC_PAD_101);
}
#endif

int board_early_init_f(void)
{
#ifdef CONFIG_NAND_FSL_NFC
	setup_iomux_nfc();		/* Setup NAND flash controller */
#endif
	return 0;
}

/* Get the number of the debug port reported by NBoot */
static unsigned int get_debug_port(unsigned int dwDbgSerPortPA)
{
	unsigned int port = 6;
	struct serial_device *sdev;

	do {
		sdev = get_serial_device(--port);
		if (sdev && sdev->dev.priv == (void *)dwDbgSerPortPA)
			return port;
	} while (port);

	return CONFIG_SYS_UART_PORT;
}

struct serial_device *default_serial_console(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs;

	/* As long as GD_FLG_RELOC is not set, we can not access fs_nboot_args
	   and therefore have to use the NBoot args at NBOOT_ARGS_BASE.
	   However GD_FLG_RELOC may be set before the NBoot arguments are
	   copied from NBOOT_ARGS_BASE to fs_nboot_args (see board_init()
	   below). But then at least the .bss section and therefore
	   fs_nboot_args is cleared. So if fs_nboot_args.dwDbgSerPortPA is 0,
	   the structure is not yet copied and we still have to look at
	   NBOOT_ARGS_BASE. Otherwise we can (and must) use fs_nboot_args. */
	if ((gd->flags & GD_FLG_RELOC) && fs_nboot_args.dwDbgSerPortPA)
		pargs = &fs_nboot_args;
	else
		pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;

	return get_serial_device(get_debug_port(pargs->dwDbgSerPortPA));
}

/* Check board type */
int checkboard(void)
{
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	int nLAN;
	int nCAN;

	switch (pargs->chBoardType) {
	case BT_CUBEA5:
		nLAN = 0;
		break;
	default:
		nLAN = pargs->chFeatures1 & FEAT1_2NDLAN ? 2 : 1;
		break;
	}

	switch (pargs->chBoardType) {
	case BT_CUBEA5:
	case BT_AGATEWAY:
	case BT_HGATEWAY:
		nCAN = 0;
		break;
	default:
		nCAN = pargs->chFeatures1 & FEAT1_2NDCAN ? 2 : 1;
		break;
	}

	printf("Board: %s Rev %u.%02u (%d MHz, %dx DRAM, %dx LAN, %dx CAN)\n",
	       fs_board_info[pargs->chBoardType].name,
	       pargs->chBoardRev / 100, pargs->chBoardRev % 100,
	       pargs->chFeatures1 & FEAT1_CPU400 ? 400 : 500,
	       pargs->dwNumDram, nLAN, nCAN);

#if 0 //###
	printf("dwNumDram = 0x%08x\n", pargs->dwNumDram);
	printf("dwMemSize = 0x%08x\n", pargs->dwMemSize);
	printf("dwFlashSize = 0x%08x\n", pargs->dwFlashSize);
	printf("dwDbgSerPortPA = 0x%08x\n", pargs->dwDbgSerPortPA);
	printf("chBoardType = 0x%02x\n", pargs->chBoardType);
	printf("chBoardRev = 0x%02x\n", pargs->chBoardRev);
	printf("chFeatures1 = 0x%02x\n", pargs->chFeatures1);
	printf("chFeatures2 = 0x%02x\n", pargs->chFeatures2);
#endif

	return 0;
}

/* Set the available RAM size. We have a memory bank starting at 0x80000000
   that can hold up to 1536MB of RAM. However up to now we only have 256MB or
   512MB on F&S Vybrid boards. */
int dram_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs;

	pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	gd->ram_size = pargs->dwMemSize << 20;
	gd->ram_base = PHYS_SDRAM_0;

	return 0;
}

/* Now RAM is valid, U-Boot is relocated. From now on we can use variables */
int board_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	unsigned int board_type = pargs->chBoardType;
	u32 temp;
	struct vybrid_scsc_reg *scsc;

	/* Save a copy of the NBoot args */
	memcpy(&fs_nboot_args, pargs, sizeof(struct tag_fshwconfig));
	fs_nboot_args.dwSize = sizeof(struct tag_fshwconfig);
	memcpy(&fs_m4_args, pargs+1, sizeof(struct tag_fsm4config));
	fs_m4_args.dwSize = sizeof(struct tag_fsm4config);

	gd->bd->bi_arch_number = fs_board_info[board_type].mach_type;
	gd->bd->bi_boot_params = BOOT_PARAMS_BASE;

	/* Prepare the command prompt */
	sprintf(fs_sys_prompt, "%s # ", fs_board_info[board_type].name);

#if 0
	__led_init(0, 0); //###
	__led_init(1, 0); //###
#endif

	/* The internal clock experiences significant drift so we must use the
	   external oscillator in order to maintain correct time in the
	   hwclock */
	scsc = (struct vybrid_scsc_reg *)SCSCM_BASE_ADDR;
	temp = __raw_readl(&scsc->sosc_ctr);
	temp |= VYBRID_SCSC_SICR_CTR_SOSC_EN;
	__raw_writel(temp, &scsc->sosc_ctr);

	return 0;
}

/* Register NAND devices. We actually split the NAND into two virtual devices
   to allow different ECC strategies for NBoot and the rest. */
void board_nand_init(void)
{
	struct fsl_nfc_fus_platform_data pdata;

	/* The first device skips the NBoot region (2 blocks) to protect it
	   from inadvertent erasure. The skipped region can not be written
	   and is always read as 0xFF. */
	pdata.options = NAND_BBT_SCAN2NDPAGE;
	pdata.t_wb = 0;
	pdata.eccmode = fs_nboot_args.chECCtype;
	pdata.skipblocks = 2;
	pdata.flags = 0;
#ifdef CONFIG_NAND_REFRESH
	pdata.backup_sblock = CONFIG_SYS_NAND_BACKUP_START_BLOCK;
	pdata.backup_eblock = CONFIG_SYS_NAND_BACKUP_END_BLOCK;
#endif
	vybrid_nand_register(0, &pdata);

#if CONFIG_SYS_MAX_NAND_DEVICE > 1
	/* The second device just consists of the NBoot region (2 blocks) and
	   is software write-protected by default. It uses a different ECC
	   strategy. ### TODO ### In fact we actually need special code to
	   store the NBoot image. */
	pdata.options |= NAND_SW_WRITE_PROTECT;
	pdata.eccmode = VYBRID_NFC_ECCMODE_32BIT;
	pdata.flags = VYBRID_NFC_SKIP_INVERSE;
#ifdef CONFIG_NAND_REFRESH
	pdata.backupstart = 0;
	pdata.backupend = 0;
#endif
	vybrid_nand_register(0, &pdata);
#endif
}

void board_nand_state(struct mtd_info *mtd, unsigned int state)
{
	/* Save state to pass it to Linux later */
	fs_nboot_args.chECCstate |= (unsigned char)state;
}

size_t get_env_size(void)
{
	return ENV_SIZE_DEF_LARGE;
}

size_t get_env_range(void)
{
	return ENV_RANGE_DEF_LARGE;
}

size_t get_env_offset(void)
{
	return ENV_OFFSET_DEF_LARGE;
}

#ifdef CONFIG_CMD_UPDATE
enum update_action board_check_for_recover(void)
{
	char *recover_gpio;

	/* On some platforms, the check for recovery is already done in NBoot.
	   Then the ACTION_RECOVER bit in the dwAction value is set. */
	if (fs_nboot_args.dwAction & ACTION_RECOVER)
		return UPDATE_ACTION_RECOVER;

	/*
	 * If a recover GPIO is defined, check if it is in active state. The
	 * variable contains the number of a gpio, followed by an optional '-'
	 * or '_', followed by an optional "high" or "low" for active high or
	 * active low signal. Actually only the first character is checked,
	 * 'h' and 'H' mean "high", everything else is taken for "low".
	 * Default is active low.
	 *
	 * Examples:
	 *    123_high  GPIO #123, active high
	 *    65-low    GPIO #65, active low
	 *    13        GPIO #13, active low
	 *    0x1fh     GPIO #31, active high (this shows why a dash or
	 *              underscore before "high" or "low" makes sense)
	 * 
	 * Remark:
	 * We do not have any clue here what the GPIO represents and therefore
	 * we do not assume any pad settings. So for example if the GPIO
	 * represents a button that is floating in the released state, an
	 * external pull-up or pull-down must be used to avoid unintentionally
	 * detecting the active state.
	 */
	recover_gpio = getenv("recovergpio");
	if (recover_gpio) {
		char *endp;
		int active_state = 0;
		unsigned int gpio = simple_strtoul(recover_gpio, &endp, 0);

		if (endp != recover_gpio) {
			char c = *endp;

			if ((c == '-') || (c == '_'))
				c = *(++endp);
			if ((c == 'h') || (c == 'H'))
				active_state = 1;
			if (!gpio_direction_input(gpio)
			    && (gpio_get_value(gpio) == active_state))
				return UPDATE_ACTION_RECOVER;
		}
	}

	return UPDATE_ACTION_UPDATE;
}
#endif

#ifdef CONFIG_GENERIC_MMC
int board_mmc_getcd(struct mmc *mmc)
{
	u32 val;

	switch (fs_nboot_args.chBoardType) {
	case BT_AGATEWAY:		/* PAD51 = GPIO1, Bit 19 */
		val = __raw_readl(0x400FF050) & (1 << 19);
		break;

	case BT_ARMSTONEA5:
	case BT_NETDCUA5:		/* PAD134 = GPIO4, Bit 6 */
		/* #### Check if NetDCUA5 is working ### */
		val = __raw_readl(0x400FF110) & (1 << 6);
		break;

	default:
		return 1;		/* Assume card present */
	}

	return (val == 0);
}

#define MVF600_GPIO_SDHC_CD \
	(PAD_CTL_SPEED_HIGH | PAD_CTL_DSE_20ohm | PAD_CTL_IBE_ENABLE)
int board_mmc_init(bd_t *bis)
{
	int index;
	u32 val;

	switch (fs_nboot_args.chBoardType) {
	case BT_AGATEWAY:
		__raw_writel(MVF600_GPIO_SDHC_CD, IOMUXC_PAD_051);
		index = 0;
		break;

	case BT_ARMSTONEA5:
	case BT_NETDCUA5:
		__raw_writel(MVF600_GPIO_GENERAL_CTRL | PAD_CTL_IBE_ENABLE,
			     IOMUXC_PAD_134);
		/* fall through to default */
	default:
		index = 1;
		break;
	}

	val = MVF600_SDHC_PAD_CTRL | PAD_CTL_MODE_ALT5;
	if (!index) {				   /* ESDHC0 */
		__raw_writel(val, IOMUXC_PAD_045); /* CLK */
		__raw_writel(val, IOMUXC_PAD_046); /* CMD */
		__raw_writel(val, IOMUXC_PAD_047); /* DAT0 */
		__raw_writel(val, IOMUXC_PAD_048); /* DAT1 */
		__raw_writel(val, IOMUXC_PAD_049); /* DAT2 */
		__raw_writel(val, IOMUXC_PAD_050); /* DAT3 */
	} else {				   /* ESDHC1 */
		__raw_writel(val, IOMUXC_PAD_014); /* CLK */
		__raw_writel(val, IOMUXC_PAD_015); /* CMD */
		__raw_writel(val, IOMUXC_PAD_016); /* DAT0 */
		__raw_writel(val, IOMUXC_PAD_017); /* DAT1 */
		__raw_writel(val, IOMUXC_PAD_018); /* DAT2 */
		__raw_writel(val, IOMUXC_PAD_019); /* DAT3 */
	}

	esdhc_cfg[index].sdhc_clk = vybrid_get_esdhc_clk(index);
	return fsl_esdhc_initialize(bis, &esdhc_cfg[index]);
}
#endif

#ifdef CONFIG_USB_EHCI_VYBRID
int board_ehci_hcd_init(int port)
{
	if (!port) {
		/* Configure USB0_PWR (PTE5) as GPIO output and set to 1 by
		   writing to GPIO3_PSOR[14] */
		__raw_writel(MVF600_GPIO_GENERAL_CTRL | PAD_CTL_OBE_ENABLE,
			     IOMUXC_PAD_110);
		__raw_writel(1 << (110 & 0x1F), 0x400ff0c4);
	} else {
		/* Configure USB1_PWR (PTE6) as GPIO output and set to 1 by
		   writing to GPIO3_PSOR[15] */
		__raw_writel(MVF600_GPIO_GENERAL_CTRL | PAD_CTL_OBE_ENABLE,
			     IOMUXC_PAD_111);
		__raw_writel(1 << (111 & 0x1F), 0x400ff0c4);
	}

        return 0;
}
#endif

#ifdef CONFIG_BOARD_LATE_INIT
void setup_var(const char *varname, const char *content, int runvar)
{
	char *envvar = getenv(varname);

	/* If variable is not set or does not contain string "undef", do not
	   change it */
	if (!envvar || strcmp(envvar, "undef"))
		return;

	/* Either set variable directly with value ... */
	if (!runvar) {
		setenv(varname, content);
		return;
	}

	/* ... or set variable by running the variable with name in content */
	content = getenv(content);
	if (content)
		run_command(content, 0);
}

/* Use this slot to init some final things before the network is started. We
   set up some environment variables for things that are board dependent and
   can't be defined as a fix value in fsvybrid.h. As an unset value is valid
   for some of these variables, we check for the special value "undef". Any
   of these variables that holds this value will be replaced with the
   board-specific value. */
int board_late_init(void)
{
	unsigned int boardtype = fs_nboot_args.chBoardType;
	const struct board_info *bi = &fs_board_info[boardtype];
	const char *envvar;

	/* Set sercon variable if not already set */
	envvar = getenv("sercon");
	if (!envvar || !strcmp(envvar, "undef")) {
		char sercon[DEV_NAME_SIZE];

		sprintf(sercon, "%s%c", CONFIG_SYS_SERCON_NAME,
			'0' + get_debug_port(fs_nboot_args.dwDbgSerPortPA));
		setenv("sercon", sercon);
	}

	/* Set platform variable if not already set */
	envvar = getenv("platform");
	if (!envvar || !strcmp(envvar, "undef")) {
		char lcasename[20];
		char *p = bi->name;
		char *l = lcasename;
		char c;

		do {
			c = *p++;
			if ((c >= 'A') && (c <= 'Z'))
				c += 'a' - 'A';
			*l++ = c;
		} while (c);

		setenv("platform", lcasename);
	}

	/* Set some variables with a direct value */
	setup_var("bootdelay", bi->bootdelay, 0);
	setup_var("updatecheck", bi->updatecheck, 0);
	setup_var("installcheck", bi->installcheck, 0);
	setup_var("recovercheck", bi->recovercheck, 0);
	setup_var("earlyusbinit", bi->earlyusbinit, 0);
	setup_var("mtdids", MTDIDS_DEFAULT, 0);
	setup_var("partition", MTDPART_DEFAULT, 0);
	setup_var("mode", CONFIG_MODE, 0);

	/* Set some variables by runnning another variable */
	setup_var("console", bi->console, 1);
	setup_var("login", bi->login, 1);
	setup_var("mtdparts", bi->mtdparts, 1);
	setup_var("network", bi->network, 1);
	setup_var("init", bi->init, 1);
	setup_var("rootfs", bi->rootfs, 1);
	setup_var("kernel", bi->kernel, 1);
	setup_var("bootfdt", "set_bootfdt", 1);
	setup_var("fdt", bi->fdt, 1);
	setup_var("bootargs", "set_bootargs", 1);

	return 0;
}
#endif


#ifdef CONFIG_CMD_NET
static void fecpin_config(uint32_t enet_addr)
{
	/*
	 * Configure as ethernet. There is a hardware bug on Vybrid when
	 * RMIICLK (PTA6) is used as RMII clock output. Then outgoing data
	 * changes value on the wrong edge, i.e. when it is latched in the
	 * PHY. Unfortunately we have some board revisions where this
	 * configuration is used. To reduce the risk that the PHY latches the
	 * wrong data, we set the edge speed of the data signals to low and of
	 * the clock signal to high. This gains about 2ns difference but is
	 * unstable and does not work with all PHYs.
	 *
	 * In our newer board revisions we either use an external oscillator
	 * (AGATEWAY) or we have looped back CKO1 (PTB10) to RMIICKL (PTA6)
	 * and output the RMII clock on CKO1. Then PTA6 is a clock input and
	 * everything works as expected.
	 *
	 * The drive strength values below guarantee very stable results, but
	 * if EMC conformance requires, they can be reduced even more:
	 * 0x00100042 for outputs, 0x00100001 for inputs and 0x00100043 for
	 * mixed inputs/outputs.
	 */
	if (enet_addr == MACNET0_BASE_ADDR) {
		__raw_writel(0x001000c2, IOMUXC_PAD_045);	/*MDC*/
		__raw_writel(0x001000c3, IOMUXC_PAD_046);	/*MDIO*/
		__raw_writel(0x001000c1, IOMUXC_PAD_047);	/*RxDV*/
		__raw_writel(0x001000c1, IOMUXC_PAD_048);	/*RxD1*/
		__raw_writel(0x001000c1, IOMUXC_PAD_049);	/*RxD0*/
		__raw_writel(0x001000c1, IOMUXC_PAD_050);	/*RxER*/
		__raw_writel(0x001000c2, IOMUXC_PAD_051);	/*TxD1*/
		__raw_writel(0x001000c2, IOMUXC_PAD_052);	/*TxD0*/
		__raw_writel(0x001000c2, IOMUXC_PAD_053);	/*TxEn*/
	} else if (enet_addr == MACNET1_BASE_ADDR) {
		__raw_writel(0x001000c2, IOMUXC_PAD_054);	/*MDC*/
		__raw_writel(0x001000c3, IOMUXC_PAD_055);	/*MDIO*/
		__raw_writel(0x001000c1, IOMUXC_PAD_056);	/*RxDV*/
		__raw_writel(0x001000c1, IOMUXC_PAD_057);	/*RxD1*/
		__raw_writel(0x001000c1, IOMUXC_PAD_058);	/*RxD0*/
		__raw_writel(0x001000c1, IOMUXC_PAD_059);	/*RxER*/
		__raw_writel(0x001000c2, IOMUXC_PAD_060);	/*TxD1*/
		__raw_writel(0x001000c2, IOMUXC_PAD_061);	/*TxD0*/
		__raw_writel(0x001000c2, IOMUXC_PAD_062);	/*TxEn*/
	}
}

/* Read a MAC address from OTP memory */
int get_otp_mac(unsigned long otp_addr, uchar *enetaddr)
{
	u32 val;
	static const uchar empty1[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	static const uchar empty2[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	/*
	 * Read a MAC address from OTP memory on Vybrid; it is stored in the
	 * following order:
	 *
	 *   Byte 1 in mac_h[7:0]
	 *   Byte 2 in mac_h[15:8]
	 *   Byte 3 in mac_h[23:16]
	 *   Byte 4 in mac_h[31:24]
	 *   Byte 5 in mac_l[23:16]
	 *   Byte 6 in mac_l[31:24]
	 *
	 * Please note that this layout is different to i.MX6.
	 *
	 * The MAC address itself can be empty (all six bytes zero) or erased
	 * (all six bytes 0xFF). In this case the whole address is ignored.
	 *
	 * In addition to the address itself, there may be a count stored in
	 * mac_l[15:8].
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
	enetaddr[0] = val & 0xFF;
	enetaddr[1] = (val >> 8) & 0xFF;
	enetaddr[2] = (val >> 16) & 0xFF;
	enetaddr[3] = val >> 24;

	val = __raw_readl(otp_addr + 0x10);
	enetaddr[4] = (val >> 16) & 0xFF;
	enetaddr[5] = val >> 24;

	if (!memcmp(enetaddr, empty1, 6) || !memcmp(enetaddr, empty2, 6))
		return 0;

	val >>= 8;
	val &= 0xFF;
	if (val == 0xFF)
		val = 0;

	return (int)(val + 1);
}


/* Set the ethaddr environment variable according to index */
void set_fs_ethaddr(int index)
{
	uchar enetaddr[6];
	int count, i;
	int offs = index;

	/* Try to fulfil the request in the following order:
	 *   1. From environment variable
	 *   2. MAC0 from OTP
	 *   3. MAC1 from OTP
	 *   4. CONFIG_ETHADDR_BASE
	 */
	if (eth_getenv_enetaddr_by_index("eth", index, enetaddr))
		return;

	count = get_otp_mac(OTP_BASE_ADDR + 0x620, enetaddr);
	if (count <= offs) {
		offs -= count;
		count = get_otp_mac(OTP_BASE_ADDR + 0x640, enetaddr);
		if (count <= offs) {
			offs -= count;
			eth_parse_enetaddr(MK_STR(CONFIG_ETHADDR_BASE),
					   enetaddr);
		}
	}

	i = 6;
	do {
		offs += (int)enetaddr[--i];
		enetaddr[i] = offs & 0xFF;
		offs >>= 8;
	} while (i);

	eth_setenv_enetaddr_by_index("eth", index, enetaddr);
}

/* Initialize ethernet by registering the available FEC devices */
int board_eth_init(bd_t *bis)
{
	int ret;
	int id;
	int phy_addr;
	uint32_t enet_addr;
	u8 chBoardType = fs_nboot_args.chBoardType;

	/* CUBEA5 has not ethernet at all, do not even configure PHY clock */
	if (chBoardType == BT_CUBEA5)
		return 0;

	/* Configure ethernet PHY clock depending on board type and revision */
	if ((chBoardType == BT_AGATEWAY) && (fs_nboot_args.chBoardRev >= 110)) {
		/* Starting with board Rev 1.10, AGATEWAY has an external
		   oscillator and needs RMIICLK (PTA6) as input */
		__raw_writel(0x00203191, IOMUXC_PAD_000);
	} else {
#ifdef CONFIG_FS_VYBRID_PLL_ETH
		struct clkctl *ccm = (struct clkctl *)CCM_BASE_ADDR;
		u32 temp;

		/* Configure PLL5 main clock for RMII clock. */
		temp = 0x2001;		/* ANADIG_PLL5_CTRL: Enable, 50MHz */
		__raw_writel(temp, 0x400500E0);
		if (fs_nboot_args.chFeatures2 & FEAT2_RMIICLK_CKO1) {
			/* We have a board revision with a direct connection
			   between PTB10 and PTA6, so we will use CKO1 (PTB10)
			   to output the PLL5 clock signal and use RMIICLK
			   (PTA6) as input. */
			temp = __raw_readl(&ccm->ccosr);
			temp &= ~(0x7FF << 0);	/* PLL5 output on CKO1 */
			temp |= (0x04 << 0) | (0 << 6) | (1 << 10);
			__raw_writel(temp, &ccm->ccosr);
			temp = __raw_readl(&ccm->cscmr2);
			temp |= (0<<4);		/* Use RMII clock as input */
			__raw_writel(temp, &ccm->cscmr2);
			/* See commment above about drive strength */
			__raw_writel(0x006000c2, IOMUXC_PAD_032);
			__raw_writel(0x002000c1, IOMUXC_PAD_000);
		} else {
			/* We do not have a connection between PTB10 and PTA6
			   and we also don't have an external oscillator. We
			   must use RMIICLK (PTA6) as RMII clock output. This
			   is not stable and may not work with all PHYs! See
			   the comment in function fecpin_setclear(). */
			temp = __raw_readl(&ccm->cscmr2);
			temp |= (2<<4);		/* Use PLL5 for RMII */
			__raw_writel(temp, &ccm->cscmr2);
			temp = __raw_readl(&ccm->cscdr1);
			temp |= (1<<24);	/* Enable RMII clock output */
			__raw_writel(temp, &ccm->cscdr1);
			__raw_writel(0x00103942, IOMUXC_PAD_000);
		}
#else
		/* We have an external oscillator for RMII clock, configure
		   RMIICLK (PTA6) as input */
		__raw_writel(0x00203191, IOMUXC_PAD_000);
#endif /* CONFIG_FS_VYBRID_PLL_ETH */
	}

	/* Get info on first ethernet port; AGATEWAY and HGATEWAY always only
	   have one port which is actually FEC1! */
	set_fs_ethaddr(0);
	phy_addr = 0;
	enet_addr = MACNET0_BASE_ADDR;
	id = -1;
	switch (fs_nboot_args.chBoardType) {
	case BT_PICOCOMA5:
		phy_addr = 1;
		/* Fall through to case BT_ARMSTONEA5 */
	case BT_ARMSTONEA5:
	case BT_NETDCUA5:
		if (fs_nboot_args.chFeatures1 & FEAT1_2NDLAN)
			id = 0;
		break;
	case BT_AGATEWAY:
	case BT_HGATEWAY:
		id = -1;
		enet_addr = MACNET1_BASE_ADDR;
		break;
	}

	/* Configure pads for first port */
	fecpin_config(enet_addr);

	/* Probe first PHY and ethernet port */
	ret = fecmxc_initialize_multi_type(bis, id, phy_addr, enet_addr, RMII);
	if (ret || !(fs_nboot_args.chFeatures1 & FEAT1_2NDLAN))
		return ret;

	/* Get info on second ethernet port */
	set_fs_ethaddr(1);
	switch (fs_nboot_args.chBoardType) {
	case BT_PICOCOMA5:
		phy_addr = 1;
		break;
	case BT_ARMSTONEA5:
	case BT_NETDCUA5:
		phy_addr = 0;
		break;
	}
	enet_addr = MACNET1_BASE_ADDR;

	/* Configure pads for second port */
	fecpin_config(enet_addr);

	/* Probe second PHY and ethernet port */
	return fecmxc_initialize_multi_type(bis, 1, phy_addr, enet_addr, RMII);
}
#endif /* CONFIG_CMD_NET */

/* Return the board name; we have different boards that use this file, so we
   can not define the board name with CONFIG_SYS_BOARDNAME */
char *get_board_name(void)
{
	return fs_board_info[fs_nboot_args.chBoardType].name;
}


/* Return the system prompt; we can not define it with CONFIG_SYS_PROMPT
   because we want to include the board name, which is variable (see above) */
char *get_sys_prompt(void)
{
	return fs_sys_prompt;
}

/* Return the board revision; this is called when Linux is started and the
   value is passed to Linux */
unsigned int get_board_rev(void)
{
	return fs_nboot_args.chBoardRev;
}

/* Return a pointer to the hardware configuration; this is called when Linux
   is started and the structure is passed to Linux */
struct tag_fshwconfig *get_board_fshwconfig(void)
{
	return &fs_nboot_args;
}

/* Return a pointer to the M4 image and configuration; this is called when
   Linux is started and the structure is passed to Linux */
struct tag_fsm4config *get_board_fsm4config(void)
{
	return &fs_m4_args;
}

#ifdef CONFIG_CMD_LED
/* We have LEDs on PTC30 (Pad 103) and PTC31 (Pad 104); on CUBEA5 and
   AGATEWAY, the logic is inverted.
   On HGATEWAY, we allow using the RGB LED PTD25 (red, Pad 69) and PTD24
   (blue, Pad 70) as status LEDs.
*/
#if 0
void __led_init(led_id_t mask, int state)
{
	printf("### __led_init()\n");
	if ((mask > 1) || (fs_nboot_args.chBoardType != BT_HGATEWAY))
		return;
	__raw_writel(0x00000142, mask ? 0x40048118 : 0x40048114);
	__led_set(mask, state);
}
#endif
void __led_set(led_id_t mask, int state)
{
	unsigned long reg;

	if (mask > 1)
		return;

	if ((fs_nboot_args.chBoardType == BT_CUBEA5)
	    || (fs_nboot_args.chBoardType == BT_AGATEWAY))
		state = !state;

	if (fs_nboot_args.chBoardType == BT_HGATEWAY) {
		/* Write to GPIO2_PSOR or GPIO2_PCOR */
		mask += 5;
		reg = 0x400ff084;
	} else {
		/* Write to GPIO3_PSOR or GPIO3_PCOR */
		mask += 7;
		reg = 0x400ff0c4;
	}
	__raw_writel(1 << mask, state ? reg : (reg + 4));
}

void __led_toggle(led_id_t mask)
{
	unsigned long reg;

	if (mask > 1)
		return;

	if (fs_nboot_args.chBoardType == BT_HGATEWAY) {
		/* Write to GPIO2_PTOR */
		mask += 5;
		reg = 0x400ff08c;
	} else {
		/* Write to GPIO3_PTOR */
		mask += 7;
		reg = 0x400ff0cc;
	}
	__raw_writel(1 << mask, reg);
}
#endif /* CONFIG_CMD_LED */

#ifdef CONFIG_CMD_LCD
// ####TODO
void s3c64xx_lcd_board_init(void)
{
	/* Setup GPF15 to output 0 (backlight intensity 0) */
	__REG(GPFDAT) &= ~(0x1<<15);
	__REG(GPFCON) = (__REG(GPFCON) & ~(0x3<<30)) | (0x1<<30);
	__REG(GPFPUD) &= ~(0x3<<30);

	/* Setup GPK[3] to output 1 (Buffer enable off), GPK[2] to output 1
	   (Display enable off), GPK[1] to output 0 (VCFL off), GPK[0] to
	   output 0 (VLCD off), no pull-up/down */
	__REG(GPKDAT) = (__REG(GPKDAT) & ~(0xf<<0)) | (0xc<<0);
	__REG(GPKCON0) = (__REG(GPKCON0) & ~(0xFFFF<<0)) | (0x1111<<0);
	__REG(GPKPUD) &= ~(0xFF<<0);
}

void s3c64xx_lcd_board_enable(int index)
{
	switch (index) {
	case PON_LOGIC:			  /* Activate VLCD */
		__REG(GPKDAT) |= (1<<0);
		break;

	case PON_DISP:			  /* Activate Display Enable signal */
		__REG(GPKDAT) &= ~(1<<2);
		break;

	case PON_CONTR:			  /* Activate signal buffers */
		__REG(GPKDAT) &= ~(1<<3);
		break;

	case PON_PWM:			  /* Activate VEEK*/
		__REG(GPFDAT) |= (0x1<<15); /* full intensity
					       #### TODO: actual PWM value */
		break;

	case PON_BL:			  /* Activate VCFL */
		__REG(GPKDAT) |= (1<<1);
		break;

	default:
		break;
	}
}

void s3c64xx_lcd_board_disable(int index)
{
	switch (index) {
	case POFF_BL:			  /* Deactivate VCFL */
		__REG(GPKDAT) &= ~(1<<1);
		break;

	case POFF_PWM:			  /* Deactivate VEEK*/
		__REG(GPFDAT) &= ~(0x1<<15);
		break;

	case POFF_CONTR:		  /* Deactivate signal buffers */
		__REG(GPKDAT) |= (1<<3);
		break;

	case POFF_DISP:			  /* Deactivate Display Enable signal */
		__REG(GPKDAT) |= (1<<2);
		break;

	case POFF_LOGIC:		  /* Deactivate VLCD */
		__REG(GPKDAT) &= ~(1<<0);
		break;

	default:
		break;
	}
}
#endif

#ifdef CONFIG_OF_BOARD_SETUP
/* Set a generic value, if it was not already set in the device tree */
static void fus_fdt_set_val(void *fdt, int offs, const char *name,
			    const void *val, int len)
{
	int err;

	/* Warn if property already exists in device tree */
	if (fdt_get_property(fdt, offs, name, NULL) != NULL) {
		printf("## Keeping property %s/%s from device tree!\n",
		       fdt_get_name(fdt, offs, NULL), name);
	}

	err = fdt_setprop(fdt, offs, name, val, len);
	if (err) {
		printf("## Unable to update property %s/%s: err=%s\n",
		       fdt_get_name(fdt, offs, NULL), name, fdt_strerror(err));
	}
}

/* Set a string value */
static void fus_fdt_set_string(void *fdt, int offs, const char *name,
			       const char *str)
{
	fus_fdt_set_val(fdt, offs, name, str, strlen(str) + 1);
}

/* Set a u32 value as a string (usually for bdinfo) */
static void fus_fdt_set_u32str(void *fdt, int offs, const char *name, u32 val)
{
	char str[12];

	sprintf(str, "%u", val);
	fus_fdt_set_string(fdt, offs, name, str);
}
#if 0
/* Set a u32 value */
static void fus_fdt_set_u32(void *fdt, int offs, const char *name, u32 val)
{
	fdt32_t tmp = cpu_to_fdt32(val);

	fus_fdt_set_val(fdt, offs, name, &tmp, sizeof(tmp));
}

/* Set ethernet MAC address aa:bb:cc:dd:ee:ff for given index */
static void fus_fdt_set_macaddr(void *fdt, int offs, int id)
{
	uchar enetaddr[6];
	char name[10];
	char str[20];

	if (eth_getenv_enetaddr_by_index("eth", id, enetaddr)) {
		sprintf(name, "MAC%d", id);
		sprintf(str, "%pM", enetaddr);
		fus_fdt_set_string(fdt, offs, name, str);
	}
}
#endif
/* If environment variable exists, set a string property with the same name */
static void fus_fdt_set_getenv(void *fdt, int offs, const char *name)
{
	const char *str;

	str = getenv(name);
	if (str)
		fus_fdt_set_string(fdt, offs, name, str);
}

/* Open a node, warn if the node does not exist */
static int fus_fdt_path_offset(void *fdt, const char *path)
{
	int offs;

	offs = fdt_path_offset(fdt, path);
	if (offs < 0) {
		printf("## Can not access node %s: err=%s\n",
		       path, fdt_strerror(offs));
	}

	return offs;
}
#if 0
/* Enable or disable node given by path, overwrite any existing status value */
static void fus_fdt_enable(void *fdt, const char *path, int enable)
{
	int offs, err, len;
	const void *val;
	char *str = enable ? "okay" : "disabled";

	offs = fdt_path_offset(fdt, path);
	if (offs < 0)
		return;

	/* Do not change if status already exists and has this value */
	val = fdt_getprop(fdt, offs, "status", &len);
	if (val && len && !strcmp(val, str))
		return;

	/* No, set new value */
	err = fdt_setprop_string(fdt, offs, "status", str);
	if (err) {
		printf("## Can not set status of node %s: err=%s\n",
		       path, fdt_strerror(err));
	}
}
#endif
/* Do any additional board-specific device tree modifications */
void ft_board_setup(void *fdt, bd_t *bd)
{
	int offs;

	printf("   Setting run-time properties\n");
#if 0
	/* Set ECC strength for NAND driver */
	offs = fus_fdt_path_offset(fdt, FDT_NAND);
	if (offs >= 0) {
		fus_fdt_set_u32(fdt, offs, "fus,ecc_strength",
				fs_nboot_args.chECCtype);
	}
#endif
	/* Set bdinfo entries */
	offs = fus_fdt_path_offset(fdt, "/bdinfo");
	if (offs >= 0) {
		int id = 0;
		char rev[6];

		/* NAND info, names and features */
		fus_fdt_set_u32str(fdt, offs, "ecc_strength",
				   fs_nboot_args.chECCtype);
		fus_fdt_set_u32str(fdt, offs, "nand_state",
				   fs_nboot_args.chECCstate);
		fus_fdt_set_string(fdt, offs, "board_name", get_board_name());
		sprintf(rev, "%d.%02d", fs_nboot_args.chBoardRev / 100,
			fs_nboot_args.chBoardRev % 100);
		fus_fdt_set_string(fdt, offs, "board_revision", rev);
		fus_fdt_set_getenv(fdt, offs, "platform");
		fus_fdt_set_getenv(fdt, offs, "arch");
		fus_fdt_set_u32str(fdt, offs, "features1",
				   fs_nboot_args.chFeatures1);
		fus_fdt_set_u32str(fdt, offs, "features2",
				   fs_nboot_args.chFeatures2);
#if 0
		/* MAC addresses */
		if (fs_nboot_args.chFeatures2 & FEAT2_ETH_A)
			fus_fdt_set_macaddr(fdt, offs, id++);
		if (fs_nboot_args.chFeatures2 & FEAT2_ETH_B)
			fus_fdt_set_macaddr(fdt, offs, id++);
#endif
	}
#if 0
	/* Disable ethernet node(s) if feature is not available */
	if (!(fs_nboot_args.chFeatures2 & FEAT2_ETH_A))
		fus_fdt_enable(fdt, FDT_ETH_A, 0);
	if (!(fs_nboot_args.chFeatures2 & FEAT2_ETH_B))
		fus_fdt_enable(fdt, FDT_ETH_B, 0);
#endif
}
#endif /* CONFIG_OF_BOARD_SETUP */
