/*
 * (C) Copyright 2011
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
#include <asm/errno.h>			  /* ENODEV */
#include <linux/mtd/nand.h>		  /* struct nand_ecclayout, ... */
#ifdef CONFIG_CMD_NET
#include <net.h>			  /* eth_init(), eth_halt() */
#include <netdev.h>			  /* ne2000_initialize() */
#endif
#ifdef CONFIG_CMD_LCD
#include <cmd_lcd.h>			  /* PON_*, POFF_* */
#endif
#include <serial.h>			  /* struct serial_device */

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

/* NBoot arguments that are passed from NBoot to us */
struct nboot_args
{
	unsigned int dwID;		  /* ARGS_ID */
	unsigned int dwSize;		  /* 16*4 */
	unsigned int dwNBOOT_VER;
	unsigned int dwMemSize;		  /* size of SDRAM in MB */
	unsigned int dwFlashSize;	  /* size of NAND flash in MB */
	unsigned int dwDbgSerPortPA;	  /* Phys addr of serial debug port */
	unsigned int dwNumDram;		  /* Installed memory chips */
	unsigned int dwAction;		  /* (unused in U-Boot) */
	unsigned int dwReserved1[3];
	unsigned char chBoardType;	  /* Board type (see above) */
	unsigned char chBoardRev;	  /* Board revision: major*100+minor */
	unsigned char chFeatures1;	  /* Board features (see above) */
	unsigned char chFeatures2;	  /* (unused on S3C6410) */
	unsigned int dwReserved2[4];
};

struct board_info {
	char *name;			  /* Device name */
	unsigned int mach_type;		  /* Device machine ID */
	char *updinstcheck;		  /* Default devices for upd/inst */
};

const struct board_info fs_board_info[8] = {
	{"NetDCU12", MACH_TYPE_NETDCU12, "mmc,usb"},        /* 0 */
	{"PicoMOD6", MACH_TYPE_PICOMOD6, "mmc0,mmc1,usb"},  /* 1 */
	{"PicoCOM3", MACH_TYPE_PICOCOM3, "mmc,usb"},        /* 2 */
	{"unknown",  0,                  NULL},             /* 3 */
};

/* String used for system prompt */
char fs_sys_prompt[20];

/* NAND blocksize determines environment and mtdparts settings */
unsigned int nand_blocksize;

/* Copy of the NBoot args */
struct nboot_args fs_nboot_args;

/* Serial devices, defined in serial driver */
extern struct serial_device s3c64xx_serial_device[];


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
 *          - eth_initialize() calls board_eth_init() here; we reset one or
 *            two AX88796 devices and register them with ne2000_initialize();
 *            this in turn calls eth_register(). Then we return.
 *          - eth_initialize() continues and lists all registered eth devices
 */

struct serial_device *default_serial_console(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	s3c64xx_uart *uart = s3c64xx_get_base_uart(0);
	s3c64xx_uart *dbg;
	struct nboot_args *pargs;
	int i;

	if (gd->flags & GD_FLG_RELOC)
		pargs = &fs_nboot_args;
	else
		pargs = (struct nboot_args *)NBOOT_ARGS_BASE;

	dbg = (s3c64xx_uart *)pargs->dwDbgSerPortPA;

	i = 4;
	do {
		i--;
		if (dbg == uart + i)
			break;
	} while (i);
	
	return s3c64xx_serial_device + i;
}

/* Check board type */
int checkboard(void)
{
	struct nboot_args *pargs = (struct nboot_args *)NBOOT_ARGS_BASE;
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
	struct nboot_args *pargs = (struct nboot_args *)NBOOT_ARGS_BASE;
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
	struct nboot_args *pargs = (struct nboot_args *)NBOOT_ARGS_BASE;
	unsigned int board_type = pargs->chBoardType;

	/* Save a copy of the NBoot args */
	memcpy(&fs_nboot_args, pargs, sizeof(struct nboot_args));

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
/* Register the available serial ports on this board */
int board_serial_init(void)
{
	s5p_serial_register(0, "fs_ser0");
	s5p_serial_register(1, "fs_ser1");
	s5p_serial_register(2, "fs_ser2");
	s5p_serial_register(3, "fs_ser3");

	return 0;
}
#endif


int board_nand_setup_s3c(struct mtd_info *mtd, struct nand_chip *nand, int id)
{
	/* Now that we know the NAND type, save the blocksize for functions
	   get_env_offset(), get_env_size() and get_env_range(). We also would
	   like to set the MTD partition layout now, but as the environment is
	   not yet loaded, we do this later in board_late_init(). */
	nand_blocksize = mtd->erasesize;

	/* NBoot is two blocks in size, independent of the block size. */
	switch (id) {
	case 0:
		/* nand0: everything but NBoot, use 1-bit ECC */
		mtd->size -= 2*mtd->erasesize;
		mtd->skip = 2*mtd->erasesize;
		break;

	case 1:
		/* nand1: only NBoot, use 8-bit ECC, software wtrite
		   protection and mark device as not using bad block markers */
		mtd->size = 2*mtd->erasesize;
		nand->ecc.mode = -8;
		nand->options |= NAND_SW_WRITE_PROTECT | NAND_NO_BADBLOCK;
		break;

	default:
		return -ENODEV;
	}

	return 0;
}


size_t get_env_size(void)
{
	return (nand_blocksize > 16*1024)
		? ENV_SIZE_DEF_LARGE : ENV_SIZE_DEF_SMALL;
}

size_t get_env_range(void)
{
	return (nand_blocksize > 16*1024)
		? ENV_RANGE_DEF_LARGE : ENV_RANGE_DEF_SMALL;
}

size_t get_env_offset(void)
{
	return (nand_blocksize > 16*1024)
		? ENV_OFFSET_DEF_LARGE : ENV_OFFSET_DEF_SMALL;
}

#ifdef CONFIG_GENERIC_MMC
int board_mmc_init(bd_t *bis)
{
	struct s5pc110_gpio *gpio =
		(struct s5pc110_gpio *)samsung_get_base_gpio();
	struct s5pc110_clock *clock =
		(struct s5pc110_clock *)samsung_get_base_clock();
	unsigned int div;

	/* We want to use MCLK (667 MHz) as MMC0 clock source */
	writel((readl(&clock->src4) & ~(15 << 0)) | (6 << 0), &clock->src4);

	/* MMC clock must not exceed 52 MHz, the MMC driver expects it to be
	   between 40 an 50 MHz. So we'll compute a divider that generates a
	   value below 50 MHz; the result would have to be rounded up, i.e. we
	   would have to add 1. However we also have to subtract 1 again when
	   we store div in the CLK_DIV4 register, so we can skip this +1-1. */
	div = get_pll_clk(MPLL)/49999999;
	if (div > 15)
		div = 15;
	writel((readl(&clock->div4) & ~(15 << 0)) | (div << 0), &clock->div4);

	/* All clocks are enabled in CLK_SRC_MASK0 and CLK_GATE_IP2 after reset
	   and NBoot does not change this; so MMC0 clock should be enabled and
	   there is no need to set anything there */

	/* Configure GPG0 pins for MMC/SD */
	s5p_gpio_cfg_pin(&gpio->g0, 0, 2);
	s5p_gpio_cfg_pin(&gpio->g0, 1, 2);
	s5p_gpio_cfg_pin(&gpio->g0, 2, 2);
	s5p_gpio_cfg_pin(&gpio->g0, 3, 2);
	s5p_gpio_cfg_pin(&gpio->g0, 4, 2);
	s5p_gpio_cfg_pin(&gpio->g0, 5, 2);
	s5p_gpio_cfg_pin(&gpio->g0, 6, 2);

	/* We have mmc0 with 4 bit bus width */
	return s5p_mmc_init(0, 4);
}
#endif


#ifdef CONFIG_MMC
void mmc_s3c64xx_board_power(unsigned int channel)
{
	switch (channel) {
	case 0:
		/* Set GPN6 (GPIO6) as output low for SD card power, disable
		   any pull-up/down */
		GPNDAT_REG &= ~0x40;
		GPNCON_REG = (GPNCON_REG & ~0x3000) | 0x1000;
		GPNPUD_REG &= ~0x3000;
		break;

	case 1:
		/* There is no power switch on this slot, power is always on. */
		break;

	case 2:
		/* Not used */
		break;
	}
}
#endif

const char *board_get_mtdparts_default(void)
{
	return (nand_blocksize > 16*1024)
		? MTDPARTS_DEF_LARGE : MTDPARTS_DEF_SMALL;
}

#ifdef CONFIG_BOARD_LATE_INIT
/* Use this slot to init some final things before the network is started. We
   set up some environment variables for things that are board dependent and
   can't be defined as a fix value in fss3c64xx.h. */
int board_late_init(void)
{
	unsigned int boardtype = fs_nboot_args.chBoardType;

	/* Set sercon variable if not already set */
	if (!getenv("sercon")) {
		s3c64xx_uart *uart;
		s3c64xx_uart *dbg;
		int i;
		char sercon[DEV_NAME_SIZE];

		uart = s3c64xx_get_base_uart(0);
		dbg = (s3c64xx_uart *)fs_nboot_args.dwDbgSerPortPA;

		i = 4;
		do {
			i--;
			if (dbg == uart + i)
				break;
		} while (i);
		sprintf(sercon, "%s%c", CONFIG_SYS_SERCON_NAME, '0'+i);
		setenv("sercon", sercon);
	}

	/* Set mtdids, mtdparts and partition if not already set */
	if (!getenv("mtdids"))
		setenv("mtdids", MTDIDS_DEFAULT);
	if (!getenv("mtdparts"))
		setenv("mtdparts", board_get_mtdparts_default());
	if (!getenv("partition"))
		setenv("partition", MTDPART_DEFAULT);

	/* installcheck and updatecheck are allowed to be empty, so we can't
	   check for empty here. On the other hand they depend on the board,
	   so we can't define them as fix value. The trick that we do here is
	   that they are set to the string "default" in the default
	   environment and then we replace this string with the board specific
	   value here */
	if (strcmp(getenv("installcheck"), "default") == 0)
	    setenv("installcheck", fs_board_info[boardtype].updinstcheck);
	if (strcmp(getenv("updatecheck"), "default") == 0)
	    setenv("updatecheck", fs_board_info[boardtype].updinstcheck);

	/* If bootargs is not set, run variable bootubi as default setting */
	if (!getenv("bootargs")) {
		char *s;
		s = getenv("bootubi");
		if (s)
			run_command(s, 0);
	}

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
