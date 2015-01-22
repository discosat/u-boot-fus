/*
 * (C) Copyright 2014
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/arch/s3c64xx-regs.h>
#include <asm/arch/s3c64x0.h>
#include <asm/arch/mmc.h>		  /* s3c_mmc_init() */
#include <asm/errno.h>			  /* ENODEV */
#ifdef CONFIG_CMD_NET
#include <net.h>			  /* eth_init(), eth_halt() */
#include <netdev.h>			  /* ne2000_initialize() */
#endif
#ifdef CONFIG_CMD_LCD
#include <cmd_lcd.h>			  /* PON_*, POFF_* */
#endif
#include <serial.h>			  /* struct serial_device */

#include <nand.h>			/* nand_info[] */
#include <linux/mtd/nand.h>		/* struct mtd_info, struct nand_chip */
#include <mtd/s3c_nfc.h>		/* struct s3c_nfc_platform_data */

/* ------------------------------------------------------------------------- */

#define NBOOT_ARGS_BASE (PHYS_SDRAM_0 + 0x00001000) /* Arguments from NBoot */
#define BOOT_PARAMS_BASE (PHYS_SDRAM_0 + 0x100)	    /* Arguments to Linux */

#define BT_NETDCU12 0
#define BT_PICOMOD6 1
#define BT_PICOCOM3 2

#define FEAT_1RAM   (1<<4)		  /* 0: 2 RAM chips, 1: 1 RAM chip */
#define FEAT_CF     (1<<5)		  /* 0: No CF, 1: has CF */
#define FEAT_2NDLAN (1<<6)		  /* 0: 1x LAN, 1: 2x LAN */
#define FEAT_LAN    (1<<7)		  /* 0: No LAN, 1: 1x or 2x LAN */

#define ACTION_RECOVER 0x00000040	/* Start recovery instead of update */

#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)

struct board_info {
	char *name;			  /* Device name */
	unsigned int mach_type;		  /* Device machine ID */
	char *bootdelay;		  /* Default value for bootdelay */
	char *updatecheck;		  /* Default value for updatecheck */
	char *installcheck;		  /* Default value for installcheck */
	char *recovercheck;		  /* Default value for recovercheck */
	char *console;			  /* Default variable for console */
	char *login;			  /* Default variable for login */
	char *mtdparts;			  /* Default variable for mtdparts */
	char *network;			  /* Default variable for network */
	char *init;			  /* Default variable for init */
	char *rootfs;			  /* Default variable for rootfs */
	char *kernel;			  /* Default variable for kernel */
};

#if defined(CONFIG_MMC) && defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
#define UPDATE_DEF "mmc,usb"
#define UPDATE_PM6 "mmc1,mmc2,usb"
#elif defined(CONFIG_MMC) && defined(CONFIG_FS_FAT)
#define UPDATE_DEF "mmc"
#define UPDATE_PM6 "mmc1,mmc2"
#elif defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
#define UPDATE_DEF "usb"
#define UPDATE_PM6 "usb"
#else
#define UPDATE_DEF NULL
#endif

const struct board_info fs_board_info[8] = {
	{	/* 0 (BT_NETDCU12) */
		.name = "NetDCU12",
		.mach_type = MACH_TYPE_NETDCU12,
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = UPDATE_DEF,
		.recovercheck = UPDATE_DEF,
		.console = "_console_serial",
		.login = "_login_serial",
		.mtdparts = "_mtdparts_std",
		.network = "_network_off",
		.init = "_init_linuxrc",
		.rootfs = "_rootfs_ubifs",
		.kernel = "_kernel_nand",
	},
	{	/* 1 (BT_PICOMOD6) */
		.name = "PicoMOD6",
		.mach_type = MACH_TYPE_PICOMOD6,
		.bootdelay = "3",
		.updatecheck = UPDATE_PM6,
		.installcheck = UPDATE_PM6,
		.recovercheck = UPDATE_PM6,
		.console = "_console_serial",
		.login = "_login_serial",
		.mtdparts = "_mtdparts_std",
		.network = "_network_off",
		.init = "_init_linuxrc",
		.rootfs = "_rootfs_ubifs",
		.kernel = "_kernel_nand",
	},
	{	/* 2 (BT_PICOCOM3) */
		.name = "PicoCOM3",
		.mach_type = MACH_TYPE_PICOCOM3,
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = UPDATE_DEF,
		.recovercheck = UPDATE_DEF,
		.console = "_console_serial",
		.login = "_login_serial",
		.mtdparts = "_mtdparts_std",
		.network = "_network_off",
		.init = "_init_linuxrc",
		.rootfs = "_rootfs_ubifs",
		.kernel = "_kernel_nand",
	},
	{	/* 3 */
		.name = "Unknown",
	},
};

/* String used for system prompt */
char fs_sys_prompt[20];

/* Copy of the NBoot args */
struct tag_fshwconfig fs_nboot_args;


/*
 * Miscellaneous platform dependent initialisations. Boot Sequence:
 *
 * Nr. File             Function        Comment
 * -----------------------------------------------------------------
 *  1.  start.S         reset()                 Init CPU, invalidate cache
 *  2.  fss3c64xx.c     save_boot_params()      (unused)
 *  3.  lowlevel_init.S lowlevel_init()         Init clocks, etc.
 *  4.  board.c         board_init_f()          Init from flash (without RAM)
 *  5.  cpu_info.c      arch_cpu_init()         CPU type (PC100, PC110/PV210)
 *  6.  fss3c64xx.c     board_early_init_f()    (unused)
 *  7.  timer.c         timer_init()            Init timer for udelay()
 *  8.  env_nand.c      env_init()              Prepare to read env from NAND
 *  9.  board.c         init_baudrate()         Get the baudrate from env
 * 10.  serial.c        serial_init()           Start default serial port
 * 10a. fss3c64xx.c     default_serial_console() Get serial debug port
 * 11.  console.c       console_init_f()        Early console on serial port
 * 12.  board.c         display_banner()        Show U-Boot version
 * 13.  cpu_info.c      print_cpuinfo()         Show CPU type
 * 14.  fss3c64xx.c     checkboard()            Check NBoot params, show board
 * 15.  fss3c64xx.c     dram_init()             Set DRAM size to global data
 * 16.  cmd_lcd.c       lcd_setmem()            Reserve framebuffer region
 * 17.  fss3c64xx.c     dram_init_banksize()    Set ram banks to board desc.
 * 18.  board.c         display_dram_config()   Show DRAM info
 * 19.  lowlevel_init.S setup_mmu_table()       Init MMU table
 * 20.  start.S         relocate_code()         Relocate U-Boot to end of RAM,
 * 21.  board.c         board_init_r()          Init from RAM
 * 22.  cache.c         enable_caches()         Switch on caches
 * 23.  fss3c64xx.c     board_init()            Set boot params, config CS
 * 24.  serial.c        serial_initialize()     Register serial devices
 * 24a. fss3c64xx.c     board_serial_init()     (unused)
 * 25.  dlmalloc.c      mem_malloc_init()       Init heap for malloc()
 * 26.  nand.c          nand_init()             Scan NAND devices
 * 26a. s3c64xx.c       board_nand_init()       Set fss3c64xx NAND config
 * 26b. s3c64xx.c       board_nand_setup()      Set OOB layout and ECC mode
 * 26c. fss3c64xx.c     board_nand_setup_s3c()  Set OOB layout and ECC mode
 * 27.  mmc.c           mmc_initialize()        Scan MMC slots
 * 27a. fss3c64xx.c     board_mmc_init()        Set fss3c64xx MMC config
 * 28.  env_common.c    env_relocate()          Copy env to RAM
 * 29.  stdio_init.c    stdio_init()            Set I/O devices
 * 30.  cmd_lcd.c       drv_lcd_init()          Register LCD devices
 * 31.  serial.c        serial_stdio_init()     Register serial devices
 * 32.  exports.c       jumptable_init()        Table for exported functions
 * 33.  api.c           api_init()              (unused)
 * 34.  console.c       console_init_r()        Start full console
 * 35.  fss3c64xx.c     arch_misc_init()        (unused)
 * 36.  fss3c64xx.c     misc_init_r()           (unused)
 * 37.  interrupts.c    interrupt_init()        (unused)
 * 38.  interrupts.c    enable_interrupts()     (unused)
 * 39.  fss3c64xx.c     board_late_init()       (unused)
 * 40.  eth.c           eth_initialize()        Register eth devices
 * 40a. fss3c64xx.c     board_eth_init()        Set fss3c64xx eth config
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

/* Get the number of the debug port reported by NBoot */
static unsigned int get_debug_port(unsigned int dwDbgSerPortPA)
{
	unsigned int port = 4;
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
	int lancount = 0;

	if (pargs->chFeatures1 & FEAT_LAN)
		lancount = (pargs->chFeatures1 & FEAT_2NDLAN) ? 2 : 1;

	printf("Board: %s Rev %u.%02u (%dx DRAM, %dx LAN)\n",
	       fs_board_info[pargs->chBoardType].name,
	       pargs->chBoardRev / 100, pargs->chBoardRev % 100,
	       pargs->dwNumDram, lancount);

#if 0 //###
	printf("dwNumDram = 0x%08x\n", pargs->dwNumDram);
	printf("dwMemSize = 0x%08x\n", pargs->dwMemSize);
	printf("dwFlashSize = 0x%08x\n", pargs->dwFlashSize);
	printf("dwDbgSerPortPA = 0x%08x\n", pargs->dwDbgSerPortPA);
	printf("chBoardType = 0x%02x\n", pargs->chBoardType);
	printf("chBoardRev = 0x%02x\n", pargs->chBoardRev);
	printf("chFeatures1 = 0x%02x\n", pargs->chFeatures1);
#endif

	return 0;
}


/* Set the available RAM size */
int dram_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	unsigned int ram_size = pargs->dwMemSize << 20;

	gd->ram_size = ram_size;
	gd->ram_base = PHYS_SDRAM_0;

	return 0;
}

/* Set the RAM banksizes; we use only one bank in all cases */
void dram_init_banksize(void)
{
	DECLARE_GLOBAL_DATA_PTR;

	gd->bd->bi_dram[0].start = gd->ram_base;
	gd->bd->bi_dram[0].size = gd->ram_size;
}


/* Now RAM is valid, U-Boot is relocated. From now on we can use variables */
int board_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	unsigned int board_type = pargs->chBoardType;

	/* Save a copy of the NBoot args */
	memcpy(&fs_nboot_args, pargs, sizeof(struct tag_fshwconfig));

	gd->bd->bi_arch_number = fs_board_info[board_type].mach_type;
	gd->bd->bi_boot_params = BOOT_PARAMS_BASE;

	/* Prepare the command prompt */
	sprintf(fs_sys_prompt, "%s # ", fs_board_info[board_type].name);

#if 1
	icache_enable();
	/* We already set up the MMU pagetable and could switch on the data
	   cache, but then the USB driver will not work. The idea is to add
	   another mapping for the RAM without caching and the USB driver
	   should use this. Maybe other drivers don't work too? */
//####	dcache_enable();
#endif
	return 0;
}


#if 0
/* Register only those serial ports that are actually available on the board */
int board_serial_init(void)
{
	serial_register(get_serial_device(0));
	serial_register(get_serial_device(1));
	serial_register(get_serial_device(2));
	serial_register(get_serial_device(3));

	return 0;
}
#endif

/* Register NAND devices. We actually split the NAND into two virtual devices
   to allow different ECC strategies for NBoot and the rest. */
void board_nand_init(void)
{
	struct s3c_nfc_platform_data pdata;

	/* The first device skips the NBoot region (2 blocks) to protect it
	   from inadvertent erasure. The skipped region can not be written
	   and is always read as 0xFF. */
	pdata.options = NAND_BBT_SCAN2NDPAGE;
	pdata.t_wb = 0;
	pdata.eccmode = S3C_NFC_ECCMODE_1BIT;
	pdata.skipblocks = 2;
	pdata.flags = 0;

	s3c_nand_register(0, &pdata);

#if CONFIG_SYS_MAX_NAND_DEVICE > 1
	/* The second device just consists of the NBoot region (2 blocks) and
	   is software write-protected by default. It uses a different ECC
	   strategy. */
	pdata.options |= NAND_SW_WRITE_PROTECT | NAND_NO_BADBLOCK;
	pdata.eccmode = S3C_NFC_ECCMODE_8BIT;
	pdata.flags = S3C_NFC_SKIP_INVERSE;

	s3c_nand_register(0, &pdata);
#endif
}

static inline int is_large_block_nand(void)
{
	return (nand_info[0].erasesize > 16*1024);
}

size_t get_env_size(void)
{
	return is_large_block_nand()
		? ENV_SIZE_DEF_LARGE : ENV_SIZE_DEF_SMALL;
}

size_t get_env_range(void)
{
	return is_large_block_nand()
		? ENV_RANGE_DEF_LARGE : ENV_RANGE_DEF_SMALL;
}

size_t get_env_offset(void)
{
	return is_large_block_nand()
		? ENV_OFFSET_DEF_LARGE : ENV_OFFSET_DEF_SMALL;
}

#ifdef CONFIG_CMD_UPDATE
enum update_action board_check_for_recover(void)
{
	/* If the board should do an automatic recovery is given in the
	   dwAction value. Currently this is only defined for CUBEA5 and
	   AGATEWAY. If a special button is pressed for a defined time
	   when power is supplied, the system should be reset to the default
	   state, i.e. perform a complete recovery. The button is detected in
	   NBoot, but recovery takes place in U-Boot. */
	if (fs_nboot_args.dwAction & ACTION_RECOVER)
		return UPDATE_ACTION_RECOVER;
	return UPDATE_ACTION_UPDATE;
}
#endif

#ifdef CONFIG_GENERIC_MMC
int board_mmc_init(bd_t *bis)
{
	unsigned int div;
	unsigned int boardtype = fs_nboot_args.chBoardType;
	int ret;
	unsigned int base_clock;

	/* Activate 48MHz clock, this is not only required for USB OTG, but
	   also for MMC */
	OTHERS_REG |= 1<<16;		  /* Enable USB signal */
	__REG(S3C_OTG_PHYPWR) = 0x00;
	__REG(S3C_OTG_PHYCTRL) = 0x00;
	__REG(S3C_OTG_RSTCON) = 1;
	udelay(10);
	__REG(S3C_OTG_RSTCON) = 0;

	/* We want to use EPLL (84,67MHz) as MMC0..MMC2 clock source:
	   CLK_SRC[23:22]: MMC2_SEL
	   CLK_SRC[21:20]: MMC1_SEL
	   CLK_SRC[19:18]: MMC0_SEL
	     00: MOUT_EPLL, 01: DOUT_MPLL, 10: FIN_EPLL, 11: 27MHz */
	CLK_SRC_REG = (CLK_SRC_REG & ~(0x3F << 18)) | (0 << 18);

	/* MMC clock must not exceed 52 MHz, the MMC driver expects it to be
	   between 40 and 50 MHz. So we'll compute a divider that generates a
	   value below 50 MHz; the result would have to be rounded up, i.e. we
	   would have to add 1. However we also have to subtract 1 again when
	   we store div in the CLK_DIV4 register, so we can skip this +1-1.
	   CLK_DIV1[11:8]: MMC2_RATIO
	   CLK_DIV1[7:4]:  MMC1_RATIO
	   CLK_DIV1[3:0]:  MMC0_RATIO */
	base_clock = get_UCLK();
	div = base_clock/49999999;
	if (div > 15)
		div = 15;
	base_clock /= div + 1;
	div |= (div << 4) | (div << 8);
	CLK_DIV1_REG = (CLK_DIV1_REG & ~(0xFFF << 0)) | (div << 0);

	/* All clocks are enabled in HCLK_GATE and SCLK_GATE after reset and
	   NBoot does not change this; so all MMC clocks should be enabled and
	   there is no need to set anything there */

	/* We have MMC0 in any case: it is on-board on NetDCU12 and external on
	   PicoMOD6 and PicoCOM3. Pins in use:
	   GPG[6]: CD, GPG[5:2]: DATA[3:0], GPG[1]: CMD, GPG[0]: CLK
	   NetDCU12: GPL[9]: Power
	   PicoMOD6: GPN[6]: Power
	   PicoCOM3: No Power pin */
	GPGCON_REG = 0x02222222;
	GPGPUD_REG = 0;
	switch (boardtype) {
	case BT_NETDCU12:
		GPLDAT_REG &= ~(1 << 9);
		GPLCON1_REG = (GPLCON1_REG & ~(0xF << 4)) | (0x1 << 4);
		GPLPUD_REG &= ~(0x3 << 18);
		break;

	case BT_PICOMOD6:
		GPNDAT_REG &= ~(1 << 6);
		GPNCON_REG = (GPNCON_REG & (0x3 << 12)) | (0x1 << 12);
		GPNPUD_REG &= ~(0x3 << 12);
		break;

	default:
		break;
	}

	/* Add MMC0 (4 bit bus width) */
	ret = s3c_sdhci_init(ELFIN_HSMMC_0_BASE + (0x100000 * 0),
			     base_clock, base_clock/256, 0);

	if (!ret && (boardtype == BT_PICOMOD6)) {
		/* On PicoMOD6, we also have MMC1 (on-board Micro-SD slot).
		   This port is theoretically capable of 1, 4, and 8 bit bus
		   width, but our slot uses only 4 bits, the upper 4 bits are
		   fixed to high. Pins in use:
		   GPH[5:2]: DATA[3:0], GPH[1]: CMD, GPH[0]: CLK
		   There is no CD and no power pin associated with this port */
		GPHCON0_REG = 0x22222222;
		GPHCON1_REG = 0x22;
		GPHPUD_REG = 0;

		/* ADD MMC1 (4 bit bus width) */
		ret = s3c_sdhci_init(ELFIN_HSMMC_0_BASE + (0x100000 * 1),
			     base_clock, base_clock/256, 0);
	}

	return ret;
}
#endif

#ifdef CONFIG_BOARD_LATE_INIT
void setup_var(const char *varname, const char *content, int runvar)
{
	/* If variable does not contain string "undef", do not change it */
	if (strcmp(getenv(varname), "undef"))
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
   can't be defined as a fix value in fss3c64xx.h. */
int board_late_init(void)
{
	unsigned int boardtype = fs_nboot_args.chBoardType;
	const struct board_info *bi = &fs_board_info[boardtype];

	/* Set sercon variable if not already set */
	if (strcmp(getenv("sercon"), "undef") == 0) {
		char sercon[DEV_NAME_SIZE];

		sprintf(sercon, "%s%c", CONFIG_SYS_SERCON_NAME,
			'0' + get_debug_port(fs_nboot_args.dwDbgSerPortPA));
		setenv("sercon", sercon);
	}

	/* Set platform variable if not already set */
	if (strcmp(getenv("platform"), "undef") == 0) {
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
	setup_var("mtdids", MTDIDS_DEFAULT, 0);
	setup_var("_mtdparts_std", is_large_block_nand() ? MTDPARTS_STD_LARGE : MTDPARTS_STD_SMALL, 0);
#ifdef CONFIG_CMD_UBI
	setup_var("_mtdparts_ubionly", is_large_block_nand() ? MTDPARTS_UBIONLY_LARGE : MTDPARTS_UBIONLY_SMALL, 0);
#endif
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
	setup_var("bootargs", "set_bootargs", 1);

	return 0;
}
#endif


#ifdef CONFIG_CMD_NET
/* Register the available ethernet controller(s) */
int board_eth_init(bd_t *bis)
{
	/* If we don't have LAN at all, we are done */
	if (!(fs_nboot_args.chFeatures1 & FEAT_LAN))
		return 0;

	/* Reset AX88796 ethernet chip (toggle nRST line for min 200us). If
	   there are two ethernet chips, reset both at the same time. */
	if (fs_nboot_args.chBoardType == BT_NETDCU12) {
		/* NetDCU12: Reset is on GPL4 for LAN0 and GPL5 for LAN1 */
		GPNDAT_REG &= ~0x00000030;
		udelay(200);
		GPNDAT_REG |= 0x00000030;
	} else {
		/* PicoMOD6/PicoCOM3: Reset is on GPK4 (always only one LAN) */
		GPKDAT_REG &= ~0x00000010;
		udelay(200);
		GPKDAT_REG |= 0x00000010;
	}

	/* ### Muss hier Pull-Up für LAN0 gesetzt werden? */

	/* Activate ethernet 0 */
	if (ne2000_initialize(0, CONFIG_DRIVER_NE2000_BASE) < 0)
		return -1;

	/* Activate ethernet 1 only if reported as available by NBoot */
	if (fs_nboot_args.chFeatures1 & FEAT_2NDLAN) {
		/* ### Muss hier Pull-Up für LAN1 gesetzt werden? */
		ne2000_initialize(1, CONFIG_DRIVER_NE2000_BASE2);
	}

	return 0;
}
#endif /* CONFIG_CMD_NET */


#if 0//#### MMU-Test mit 1:1-Mapping
#ifdef CONFIG_ENABLE_MMU
ulong virt_to_phy_fss3c64xx(ulong addr)
{
	if ((0xc0000000 <= addr) && (addr < 0xc8000000))
		return (addr - 0xc0000000 + 0x50000000);
	else
		printf("do not support this address : %08lx\n", addr);

	return addr;
}
#endif
#endif //0####


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


#ifdef CONFIG_CMD_LCD
void s3c64xx_lcd_board_init(void)
{
	/* Setup GPF15 to output 0 (backlight intensity 0) */
	__REG(GPFDAT) &= ~(0x1<<15);
	__REG(GPFCON) = (__REG(GPFCON) & ~(0x3<<30)) | (0x1<<30);
	__REG(GPFPUD) &= ~(0x3<<30);

	if (fs_nboot_args.chBoardType == BT_PICOCOM3) {
		/* PicoCOM3: Setup GPK[2] to output 1 (Display enable off),
		   GPK[1] to output 1 (VCFL off), GPK[0] to output 1 (VLCD
		   off), no pull-up/down */
		__REG(GPKDAT) = (__REG(GPKDAT) & ~(0x7<<0)) | (0x7<<0);
		__REG(GPKCON0) = (__REG(GPKCON0) & ~(0xFFF<<0)) | (0x111<<0);
		__REG(GPKPUD) &= ~(0x3F<<0);
	} else {
		/* PicoMOD6: Setup GPK[3] to output 1 (Buffer enable off),
		   GPK[2] to output 1 (Display enable off), GPK[1] to output 0
		   (VCFL off), GPK[0] to output 0 (VLCD off), no
		   pull-up/down */
		__REG(GPKDAT) = (__REG(GPKDAT) & ~(0xf<<0)) | (0xc<<0);
		__REG(GPKCON0) = (__REG(GPKCON0) & ~(0xFFFF<<0)) | (0x1111<<0);
		__REG(GPKPUD) &= ~(0xFF<<0);
	}
}

void s3c64xx_lcd_board_enable(int index)
{
	switch (index) {
	case PON_LOGIC:			  /* Activate VLCD */
		/* PicoCOM3 Starterkit uses different logic */
		if (fs_nboot_args.chBoardType == BT_PICOCOM3)
			__REG(GPKDAT) &= ~(1<<0); /* PicoCOM3 */
		else
			__REG(GPKDAT) |= (1<<0);  /* PicoMOD6 */
		break;

	case PON_DISP:			  /* Activate Display Enable signal */
		__REG(GPKDAT) &= ~(1<<2);
		break;

	case PON_CONTR:			  /* Activate signal buffers */
		/* There is no buffer enable signal on PicoCOM3 */
		if (fs_nboot_args.chBoardType != BT_PICOCOM3)
			__REG(GPKDAT) &= ~(1<<3);
		break;

	case PON_PWM:			  /* Activate VEEK*/
		__REG(GPFDAT) |= (0x1<<15); /* full intensity
					       #### TODO: actual PWM value */
		break;

	case PON_BL:			  /* Activate VCFL */
		/* PicoCOM3 Starterkit uses different logic */
		if (fs_nboot_args.chBoardType == BT_PICOCOM3)
			__REG(GPKDAT) &= ~(1<<1); /* PicoCOM3 */
		else
			__REG(GPKDAT) |= (1<<1);  /* PicoMOD6 */
		break;

	default:
		break;
	}
}

void s3c64xx_lcd_board_disable(int index)
{
	switch (index) {
	case POFF_BL:			  /* Deactivate VCFL */
		/* PicoCOM3 Starterkit uses different logic */
		if (fs_nboot_args.chBoardType == BT_PICOCOM3)
			__REG(GPKDAT) |= (1<<1);  /* PicoCOM3 */
		else
			__REG(GPKDAT) &= ~(1<<1); /* PicoMOD6 */
		break;

	case POFF_PWM:			  /* Deactivate VEEK*/
		__REG(GPFDAT) &= ~(0x1<<15);
		break;

	case POFF_CONTR:		  /* Deactivate signal buffers */
		/* There is no buffer enable signal on PicoCOM3 */
		if (fs_nboot_args.chBoardType != BT_PICOCOM3)
			__REG(GPKDAT) |= (1<<3);
		break;

	case POFF_DISP:			  /* Deactivate Display Enable signal */
		__REG(GPKDAT) |= (1<<2);
		break;

	case POFF_LOGIC:		  /* Deactivate VLCD */
		/* PicoCOM3 Starterkit uses different logic */
		if (fs_nboot_args.chBoardType == BT_PICOCOM3)
			__REG(GPKDAT) |= (1<<0);  /* PicoCOM3 */
		else
			__REG(GPKDAT) &= ~(1<<0); /* PicoMOD6 */
		break;

	default:
		break;
	}
}
#endif

#ifdef CONFIG_USB_OHCI_NEW
int usb_board_init(void)
{
	/* Clock source 48MHz for USB */
	CLK_SRC_REG &= ~(3<<5);

	/* Activate USB Host clock */
	HCLK_GATE_REG |=  (1<<29);

	/* Activate USB OTG clock */
	HCLK_GATE_REG |= (1<<20);

	/* Set OTG special flag */
	OTHERS_REG |= 1<<16;

	/* Power up OTG PHY */
	__REG(S3C_OTG_PHYPWR) = 0;

	/* Use UTMI interface of OTG or serial 1 interface of USB1.1 host */
	__REG(S3C_OTG_PHYCTRL) = 0x10; /* 0x50 for serial 1, or 0x10 for OTG */

	/* Reset PHY */
	__REG(S3C_OTG_RSTCON) = 1;
	udelay(50);
	__REG(S3C_OTG_RSTCON) = 0;
	udelay(50);

	/* Switch PWR1 on */
	GPKDAT_REG |= (1<<7);		  /* Out 1 */
	GPKCON0_REG = (GPKCON0_REG & ~0xf0000000) | 0x10000000; /* Output */
	GPKPUD_REG &= 0xffff3fff;	  /* No Pullup/down */

	/* Let the power voltage settle */
	udelay(10000);
	//udelay(50000);

	return 0;
}

int usb_board_init_fail(void)
{
	return 0;
}

int usb_board_stop(void)
{
	GPKDAT_REG &= ~(1<<7);		  /* Out 0 */

	return 0;
}
#endif
