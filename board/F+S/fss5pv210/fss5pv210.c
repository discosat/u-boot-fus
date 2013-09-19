/*
 * (C) Copyright 2013
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
#include <linux/mtd/nand.h>		  /* struct nand_ecclayout, ... */
#include <asm/errno.h>			  /* ENODEV */
#ifdef CONFIG_CMD_NET
#include <net.h>			  /* eth_init(), eth_halt() */
#include <netdev.h>			  /* ne2000_initialize() */
#endif
#ifdef CONFIG_CMD_LCD
#include <cmd_lcd.h>			  /* PON_*, POFF_* */
#endif
#include <serial.h>			  /* struct serial_device */
#include <asm/arch/uart.h>		  /* samsung_get_base_uart(), ... */
#include <asm/arch/cpu.h>		  /* samsung_get_base_gpio() */
#include <asm/gpio.h>			  /* gpio_set_value(), ... */
#ifdef CONFIG_GENERIC_MMC
#include <sdhci.h>			  /* SDHCI_QUIRK_* */
#include <asm/arch/mmc.h>		  /* s5p_mmc_init() */
#include <asm/arch/clock.h>		  /* struct s5pc110_clock */
#include <asm/arch/clk.h>		  /* get_pll_clk(), MPLL */
#endif

/* ------------------------------------------------------------------------- */

#define NBOOT_PASSWDLEN 8
#define NBOOT_ARGS_BASE (PHYS_SDRAM_1 + 0x00000800) /* Arguments from NBoot */
#define BOOT_PARAMS_BASE (PHYS_SDRAM_1 + 0x100)	    /* Arguments to Linux */

#define BT_ARMSTONEA8 0
#define BT_QBLISSA8   2
#define BT_NETDCU14   4
#define BT_PICOMOD7A  6
#define BT_EASYSOM1   7

#define FEAT_CPU800   (1<<0)		  /* 0: 1000 MHz, 1: 800 MHz CPU */
#define FEAT_NOUART2  (1<<2)		  /* 0: UART2, 1: no UART2 (NetDCU14) */
#define FEAT_DIGITALRGB (1<<3)		  /* 0: LVDS, 1: RGB (NetDCU14) */
#define FEAT_2NDLAN   (1<<4)		  /* 0: 1x LAN, 1: 2x LAN */


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
	unsigned int dwCompat;		  /* (unused in U-Boot) */
	char chPassword[NBOOT_PASSWDLEN]; /* (unused in U-Boot) */
	unsigned char chBoardType;
	unsigned char chBoardRev;
	unsigned char chFeatures1;
	unsigned char chFeatures2;
	unsigned int dwReserved[4];
};

struct board_info {
	char *name;			  /* Device name */
	unsigned int mach_type;		  /* Device machine ID */
	char *updinstcheck;		  /* Default devices for upd/inst */
};

const struct board_info fs_board_info[8] = {
	{"armStoneA8", MACH_TYPE_ARMSTONEA8, "mmc,usb"},	/* 0 */
	{"???",	       0,		     NULL},		/* 1 */
	{"QBlissA8B",  MACH_TYPE_QBLISSA8B,  "mmc,usb"},	/* 2 */
	{"???",	       0,		     NULL},		/* 3 */
	{"NetDCU14",   MACH_TYPE_NETDCU14,   "mmc,usb"},	/* 4 */
	{"???",	       0,		     NULL},		/* 5 */
	{"PicoMOD7A",  MACH_TYPE_PICOMOD7,   "mmc0,mmc1, usb"}, /* 6 */
	{"EASYsom1",   MACH_TYPE_EASYSOM1,   "mmc,usb"},	/* 7 */
};

/* String used for system prompt */
char fs_sys_prompt[20];

/* NAND blocksize determines environment and mtdparts settings */
unsigned int nand_blocksize;

/* Copy of the NBoot args */
struct nboot_args fs_nboot_args;


/*
 * Miscellaneous platform dependent initialisations. Boot Sequence:
 *
 * Nr. File             Function        Comment
 * -----------------------------------------------------------------
 *  1.  start.S         reset()                 Init CPU, invalidate cache
 *  2.  fss5pv210.c     save_boot_params()      (unused)
 *  3.  lowlevel_init.S lowlevel_init()         Init clocks, etc.
 *  4.  board.c         board_init_f()          Init from flash (without RAM)
 *  5.  cpu_info.c      arch_cpu_init()         CPU type (PC100, PC110/PV210)
 *  6.  fss5pv210.c     board_early_init_f()    (unused)
 *  7.  timer.c         timer_init()            Init timer for udelay()
 *  8.  env_nand.c      env_init()              Prepare to read env from NAND
 *  9.  board.c         init_baudrate()         Get the baudrate from env
 * 10.  serial.c        serial_init()           Start default serial port
 * 10a. fss5pv210.c     default_serial_console() Get serial debug port
 * 11.  console.c       console_init_f()        Early console on serial port
 * 12.  board.c         display_banner()        Show U-Boot version
 * 13.  cpu_info.c      print_cpuinfo()         Show CPU type
 * 14.  fss5pv210.c     checkboard()            Check NBoot params, show board
 * 15.  fss5pv210.c     dram_init()             Set DRAM size to global data
 * 16.  cmd_lcd.c       lcd_setmem()            Reserve framebuffer region
 * 17.  fss5pv210.c     dram_init_banksize()    Set ram banks to board desc.
 * 18.  board.c         display_dram_config()   Show DRAM info
 * 19.  lowlevel_init.S setup_mmu_table()       Init MMU table
 * 20.  start.S         relocate_code()         Relocate U-Boot to end of RAM,
 * 21.  board.c         board_init_r()          Init from RAM
 * 22.  cache.c         enable_caches()         Switch on caches
 * 23.  fss5pv210.c     board_init()            Set boot params, config CS
 * 24.  serial.c        serial_initialize()     Register serial devices
 * 24a. fss5pv210.c     board_serial_init()     (unused)
 * 25.  dlmalloc.c      mem_malloc_init()       Init heap for malloc()
 * 26.  nand.c          nand_init()             Scan NAND devices
 * 26a. s5p_nand.c      board_nand_init()       Set fss5pv210 NAND config
 * 26b. s5p_nand.c      board_nand_setup()      Set OOB layout and ECC mode
 * 26c. fss5pv210.c     board_nand_setup_s3c()  Set OOB layout and ECC mode
 * 27.  mmc.c           mmc_initialize()        Scan MMC slots
 * 27a. fss5pv210.c     board_mmc_init()        Set fss5pv210 MMC config
 * 28.  env_common.c    env_relocate()          Copy env to RAM
 * 29.  stdio_init.c    stdio_init()            Set I/O devices
 * 30.  cmd_lcd.c       drv_lcd_init()          Register LCD devices
 * 31.  serial.c        serial_stdio_init()     Register serial devices
 * 32.  exports.c       jumptable_init()        Table for exported functions
 * 33.  api.c           api_init()              (unused)
 * 34.  console.c       console_init_r()        Start full console
 * 35.  fss5pv210.c     arch_misc_init()        (unused)
 * 36.  fss5pv210.c     misc_init_r()           (unused)
 * 37.  interrupts.c    interrupt_init()        (unused)
 * 38.  interrupts.c    enable_interrupts()     (unused)
 * 39.  fss5pv210.c     board_late_init()       Set additional environment
 * 40.  eth.c           eth_initialize()        Register eth devices
 * 40a. fss5pv210.c     board_eth_init()        Set fss5pv210 eth config
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
	struct nboot_args *pargs;

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
		pargs = (struct nboot_args *)NBOOT_ARGS_BASE;

	return get_serial_device(get_debug_port(pargs->dwDbgSerPortPA));
}

/* Check board type */
int checkboard(void)
{
	struct nboot_args *pargs = (struct nboot_args *)NBOOT_ARGS_BASE;

	printf("Board: %s Rev %u.%02u (%dx DRAM, %dx LAN, %d MHz)\n",
	       fs_board_info[pargs->chBoardType].name,
	       pargs->chBoardRev / 100, pargs->chBoardRev % 100,
	       pargs->dwNumDram, pargs->chFeatures1 & FEAT_2NDLAN ? 2 : 1,
	       pargs->chFeatures1 & FEAT_CPU800 ? 800 : 1000);

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


/* Set the available RAM size. Basically we have a memory bank starting at
   0x20000000 and a memory bank starting at 0x40000000. If only one bank is in
   use, it is the one starting at 0x40000000! If both banks are used, the code
   below assumes that they hold RAM of the same size, e.g. 128MB + 128MB,
   256MB + 256MB, *not* 128MB + 256MB. The most common configurations are
   0MB + 256MB and 256MB + 256MB.

   But we have a problem: in the common case, where the bank at 0x20000000 is
   smaller than 512MB, there is a gap in the physical address space between
   the two banks. Then we use a trick: in these cases, the RAM is also
   mirrored at higher addresses. This means instead of 0x20000000 we can use
   the highest of these addresses as start adddress and thus get a uniform
   address space without gap to the next bank.

   Example: Let's assume 128MB + 128MB of RAM. The first 128MB are mirrored at
   0x28000000, 0x30000000 and 0x38000000. So we change our starting address
   for the first bank from 0x20000000 to 0x38000000. Then we have a uniform
   address space without gap from 0x38000000 to 0x48000000. This is how we
   forward the bank information to Linux.

   Disadvantage: Depending on the RAM configuration, we have a varying base
   address of the U-Boot memory. We could solve this if we use the MMU. Then
   we can map the first bank to 0x20000000 and the second bank immediately
   behind the first bank. However then there are other problems. */
int dram_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct nboot_args *pargs = (struct nboot_args *)NBOOT_ARGS_BASE;
	unsigned int ram_size = pargs->dwMemSize << 20;

	gd->ram_size = ram_size;
	gd->ram_base = PHYS_SDRAM_1;
	if (pargs->dwNumDram > 2)
		gd->ram_base -= ram_size/2;

	return 0;
}


/* Set the RAM banksizes */
void dram_init_banksize(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct nboot_args *pargs = (struct nboot_args *)NBOOT_ARGS_BASE;
	unsigned int total_size = pargs->dwMemSize << 20;
	unsigned int bank_size = total_size;

	if (pargs->dwNumDram > 2)
		bank_size >>= 1;

	/* We always report two banks, even if we only have one bank; the size
	   of the second bank is zero then */
	gd->bd->bi_dram[0].start = gd->ram_base;
	gd->bd->bi_dram[0].size = bank_size;
	gd->bd->bi_dram[1].start = gd->ram_base + bank_size;
	gd->bd->bi_dram[1].size = total_size - bank_size;
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

#if 0
	/* ### Audio subsystem: set audio_clock to EPLL */
	*(volatile unsigned *)0xEEE10000 |= 1;
	printf("### ASS_CLK_SRC %08x, ASS_CLK_DIV %08x, ASS_CLK_GATE %08x\n",
	       *(volatile unsigned *)0xEEE10000,
	       *(volatile unsigned *)0xEEE10004,
	       *(volatile unsigned *)0xEEE10008);
#endif

#if 0 //###
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
/* Register only those serial ports that are actually available on the board,
   i.e. UART0-2 on armStoneA8, UART0-3 on NetDCU14 and PicoMOD7A */
int board_serial_init(void)
{
	serial_register(get_serial_device(0));
	serial_register(get_serial_device(1));
	serial_register(get_serial_device(2));
	if (fs_nboot_args.chBoardType != BT_ARMSTONEA8)
		serial_register(get_serial_device(3));

	return 0;
}
#endif

int board_nand_setup_s5p(struct mtd_info *mtd, struct nand_chip *nand, int id)
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
	unsigned int base_clock;

	/* We want to use MCLK (667 MHz) as MMC0 clock source */
	writel((readl(&clock->src4) & ~(15 << 0)) | (6 << 0), &clock->src4);

	/* MMC clock must not exceed 52 MHz, the MMC driver expects it to be
	   between 40 and 50 MHz. So we'll compute a divider that generates a
	   value below 50 MHz; the result would have to be rounded up, i.e. we
	   would have to add 1. However we also have to subtract 1 again when
	   we store div in the CLK_DIV4 register, so we can skip this +1-1. */
	base_clock = get_pll_clk(MPLL);
	div = base_clock / 49999999;
	if (div > 15)
		div = 15;
	base_clock /= div + 1;
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

	/* We have mmc0 (4 bit bus width) */
	return s5p_sdhci_init(samsung_get_base_mmc() + (0x10000 * 0),
			      base_clock, base_clock/128, 0);
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
   can't be defined as a fix value in fss5pv210.h. */
int board_late_init(void)
{
	unsigned int boardtype = fs_nboot_args.chBoardType;
	const struct board_info *bi = &fs_board_info[boardtype];

	/* Set sercon variable if not already set */
	if (!getenv("sercon")) {
		char sercon[DEV_NAME_SIZE];

		sprintf(sercon, "%s%c", CONFIG_SYS_SERCON_NAME,
			'0' + get_debug_port(fs_nboot_args.dwDbgSerPortPA));
		setenv("sercon", sercon);
	}

	/* Set platform and arch variables if not already set */
	if (!getenv("platform")) {
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
	if (!getenv("arch"))
		setenv("arch", "fss5pv210");

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
	    setenv("installcheck", bi->updinstcheck);
	if (strcmp(getenv("updatecheck"), "default") == 0)
	    setenv("updatecheck", bi->updinstcheck);

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
	struct s5pc110_gpio *gpio =
		(struct s5pc110_gpio *)samsung_get_base_gpio();

	/* Reset AX88796 ethernet chip (toggle nRST line for min 200us). If
	   there are two ethernet chips, they are both reset by this because
	   they share the reset line. */
	s5p_gpio_direction_output(&gpio->h0, 3, 0);
	udelay(200);
	s5p_gpio_set_value(&gpio->h0, 3, 1);

	/* Activate pull-up on IRQ pin of ethernet 0; by default this pin is
	   configured as open-drain on the AX8888796, i.e. it is not actively
	   driven high */
	s5p_gpio_direction_input(&gpio->h1, 2);
	s5p_gpio_set_pull(&gpio->h1, 2, GPIO_PULL_UP);

	/* Activate ethernet 0 in any case */
	if (ne2000_initialize(0, CONFIG_DRIVER_NE2000_BASE) < 0)
		return -1;

	/* Activate ethernet 1 only if reported as available by NBoot */
	if (fs_nboot_args.chFeatures1 & FEAT_2NDLAN) {
		/* Activate pull-up on IRQ pin of ethernet 1 */
		s5p_gpio_direction_input(&gpio->h0, 0);
		s5p_gpio_set_pull(&gpio->h0, 0, GPIO_PULL_UP);

		ne2000_initialize(1, CONFIG_DRIVER_NE2000_BASE2);
	}

	return 0;
}
#endif /* CONFIG_CMD_NET */


#if 0//#### MMU-Test mit 1:1-Mapping
#ifdef CONFIG_ENABLE_MMU
ulong virt_to_phy_fss5pv210(ulong addr)
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
