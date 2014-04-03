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
#include <asm/errno.h>			  /* ENODEV */
#ifdef CONFIG_CMD_NET
#include <asm/fec.h>
#include <net.h>			  /* eth_init(), eth_halt() */
#include <netdev.h>			  /* ne2000_initialize() */
#endif
#ifdef CONFIG_CMD_LCD
#include <cmd_lcd.h>			  /* PON_*, POFF_* */
#endif
#include <serial.h>			  /* struct serial_device */

#ifdef CONFIG_GENERIC_MMC
#include <mmc.h>
#include <fsl_esdhc.h>
#endif

#ifdef CONFIG_CMD_LED
#include <status_led.h>			/* led_id_t */
#endif

#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/vybrid-regs.h>	/* SCSCM_BASE_ADDR, ... */
#include <asm/arch/vybrid-pins.h>
#include <asm/arch/iomux.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/scsc_regs.h>		/* struct vybrid_scsc_reg */
#include <asm/arch/clock.h>		/* vybrid_get_esdhc_clk() */
#include <i2c.h>

#include <usb/ehci-fsl.h>

#ifdef CONFIG_FSL_ESDHC
struct fsl_esdhc_cfg esdhc_cfg[] = {
	{ESDHC0_BASE_ADDR, 1},
	{ESDHC1_BASE_ADDR, 1},
};
#endif

/* ------------------------------------------------------------------------- */

#define NBOOT_PASSWDLEN 8
#define NBOOT_ARGS_BASE (PHYS_SDRAM_0 + 0x00001000) /* Arguments from NBoot */
#define BOOT_PARAMS_BASE (PHYS_SDRAM_0 + 0x100)	    /* Arguments to Linux */

#define BT_ARMSTONEA5 0
#define BT_PICOCOMA5  1
#define BT_NETDCUA5   2
#define BT_PICOMODA5  3
#define BT_AGATEWAY   6
#define BT_CUBEA5     7

#define FEAT_CPU400   (1<<0)		/* 0: 500 MHz, 1: 400 MHz CPU */
#define FEAT_2NDCAN   (1<<1)		/* 0: 1x CAN, 1: 2x CAN */
#define FEAT_2NDLAN   (1<<4)		/* 0: 1x LAN, 1: 2x LAN */

#define ACTION_RECOVER 0x00000040	/* Start recovery instead of update */

#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)

struct M4_ARGS
{
	unsigned int dwID;
	unsigned int dwSize;
	unsigned int dwCRC;
	unsigned int dwLoadAddress;
	unsigned int dwMCCAddress;
	unsigned int dwReserved[3];
};

/* NBoot arguments that are passed from NBoot to us */
struct nboot_args
{
	unsigned int dwID;		  /* ARGS_ID */
	unsigned int dwSize;		  /* 16*4 + 8*4 */
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
	unsigned short wBootStartBlock;	  /* Start block number of bootloader */
	unsigned short wReserved;
	unsigned int dwReserved[3];
	struct M4_ARGS m4_args;
};

struct board_info {
	char *name;			  /* Device name */
	unsigned int mach_type;		  /* Device machine ID */
	char *updatecheck;		  /* Default devices for update */
	char *installcheck;		  /* Default devices for install */
	char *recovercheck;		  /* Default devices for recover */
};

#if defined(CONFIG_MMC) && defined(CONFIG_USB_STORAGE) && defined(CONFIG_CMD_FAT)
#define UPDATE_DEF "mmc,usb"
#elif defined(CONFIG_MMC) && defined(CONFIG_CMD_FAT)
#define UPDATE_DEF "mmc"
#elif defined(CONFIG_USB_STORAGE) && defined(CONFIG_CMD_FAT)
#define UPDATE_DEF "usb"
#else
#define UPDATE_DEF NULL
#endif

const struct board_info fs_board_info[8] = {
	{
		"armStoneA5",		/* 0 (BT_ARMSTONEA5) */
		MACH_TYPE_ARMSTONEA5,
		UPDATE_DEF,
		UPDATE_DEF,
		UPDATE_DEF,
	},
	{
		"PicoCOMA5",		/* 1 (BT_PICOCOMA5)*/
		MACH_TYPE_PICOCOMA5,
		UPDATE_DEF,
		UPDATE_DEF,
		UPDATE_DEF,
	},
	{
		"NetDCUA5",		/* 2 (BT_NETDCUA5) */
		MACH_TYPE_NETDCUA5,
		UPDATE_DEF,
		UPDATE_DEF,
		UPDATE_DEF,
	},
	{
		"PicoMODA5",		/* 3 (BT_PICOMODA5) */
		0,			/*####not yet registered*/
		UPDATE_DEF,
		UPDATE_DEF,
		UPDATE_DEF,
	},
	{
		"Unknown",		/* 4 */
		0,
		NULL,
		NULL,
		NULL,
	},
	{
		"Unknown",		/* 5 */
		0,
		NULL,
		NULL,
		NULL,
	},
	{
		"AGATEWAY",		/* 6 (BT_AGATEWAY) */
		MACH_TYPE_AGATEWAY,
		"TargetFS.ubi(data)",
		"ram@80300000",
		"TargetFS.ubi(recovery)",
	},
	{
		"CUBEA5",		/* 7 (BT_CUBEA5) */
		MACH_TYPE_CUBEA5,
		"TargetFS.ubi(data)",
		"ram@80300000",
		"TargetFS.ubi(recovery)",
	},
};

/* String used for system prompt */
char fs_sys_prompt[20];

/* Copy of the NBoot args */
struct nboot_args fs_nboot_args;


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
 * 26a. s5p_nand.c ###     board_nand_init()       Set fss5pv210 NAND config
 * 26b. s5p_nand.c ###     board_nand_setup()      Set OOB layout and ECC mode
 * 26c. fsvybrid.c      board_nand_setup_s3c()  Set OOB layout and ECC mode
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
	int nLAN = 0;
	int nCAN = 0;

	if (pargs->chBoardType != 7) {
		nLAN = pargs->chFeatures1 & FEAT_2NDLAN ? 2 : 1;
		nCAN = pargs->chFeatures1 & FEAT_2NDCAN ? 2 : 1;
	}

	printf("Board: %s Rev %u.%02u (%d MHz, %dx DRAM, %dx LAN, %dx CAN)\n",
	       fs_board_info[pargs->chBoardType].name,
	       pargs->chBoardRev / 100, pargs->chBoardRev % 100,
	       pargs->chFeatures1 & FEAT_CPU400 ? 400 : 500,
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
	struct nboot_args *pargs;

	pargs = (struct nboot_args *)NBOOT_ARGS_BASE;
	gd->ram_size = pargs->dwMemSize << 20;
	gd->ram_base = PHYS_SDRAM_0;

	return 0;
}


/* Now RAM is valid, U-Boot is relocated. From now on we can use variables */
int board_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct nboot_args *pargs = (struct nboot_args *)NBOOT_ARGS_BASE;
	unsigned int board_type = pargs->chBoardType;
	u32 temp;
	struct vybrid_scsc_reg *scsc;

	/* Save a copy of the NBoot args */
	memcpy(&fs_nboot_args, pargs, sizeof(struct nboot_args));

	gd->bd->bi_arch_number = fs_board_info[board_type].mach_type;
	gd->bd->bi_boot_params = BOOT_PARAMS_BASE;

	/* Prepare the command prompt */
	sprintf(fs_sys_prompt, "%s # ", fs_board_info[board_type].name);

	/* The internal clock experiences significant drift so we must use the
	   external oscillator in order to maintain correct time in the
	   hwclock */
	scsc = (struct vybrid_scsc_reg *)SCSCM_BASE_ADDR;
	temp = __raw_readl(&scsc->sosc_ctr);
	temp |= VYBRID_SCSC_SICR_CTR_SOSC_EN;
	__raw_writel(temp, &scsc->sosc_ctr);

#ifdef CONFIG_FS_VYBRID_PLL_ETH
//###	printf("### %s: using internal PLL.\n",__func__);
	//### FIXME: Use predefined address names
	temp = __raw_readl(0x4006B020);
	temp = temp | (2<<4);		/* CSCMR2[5:4] Use PLL5 main clock */
	__raw_writel(temp, 0x4006B020);
	temp = __raw_readl(0x4006B014);
	temp = temp | (1<<24);		/* CSCDR1[24] Enable ENET RMII clock */
	__raw_writel(temp, 0x4006B014);
	temp = __raw_readl(0x400500E0);
	temp = 0x2001;			/* ANADIG_PLL5_CTRL: Enable, 50MHz */
	__raw_writel(temp, 0x400500E0);
#endif
	return 0;
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
	DECLARE_GLOBAL_DATA_PTR;
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

	gd->sdhc_clk = vybrid_get_esdhc_clk(index);
	return fsl_esdhc_initialize(bis, &esdhc_cfg[index]);
}
#endif


const char *board_get_mtdparts_default(void)
{
	return MTDPARTS_DEF_LARGE;
}

#ifdef CONFIG_BOARD_LATE_INIT
/* Use this slot to init some final things before the network is started. We
   set up some environment variables for things that are board dependent and
   can't be defined as a fix value in fsvybrid.h. */
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
		setenv("arch", "fsvybrid");

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
	if (strcmp(getenv("updatecheck"), "default") == 0)
	    setenv("updatecheck", bi->updatecheck);
	if (strcmp(getenv("installcheck"), "default") == 0)
	    setenv("installcheck", bi->installcheck);
	if (strcmp(getenv("recovercheck"), "default") == 0)
	    setenv("recovercheck", bi->recovercheck);

	/* If bootargs is not set, run variable bootubi as default setting */
	if (!getenv("bootargs")) {
		char *s;
		s = getenv("bootubi");
		if (s)
			run_command(s, 0);
	}

#if 0 //#### Debug  **REMOVEME**
	{
		int i;
		printf("###Clock Gating\n");

		for (i=0; i<12; i++) {
			unsigned int addr = 0x4006b040 + 4*i;
			printf("###CCGR%d (0x%08x) = 0x%08x\n", i, addr, __raw_readl(addr));
		}
		for (i=0; i<4; i++) {
			unsigned int addr = 0x4006b090 + 4*i;
			printf("###CCPGR%d (0x%08x) = 0x%08x\n", i, addr, __raw_readl(addr));
		}
		for (i=0; i<4; i++) {
			unsigned int addr = IOMUXC_PAD_063 + 4*i;
			printf("###IOMUXC_PAD_%03d (0x%08x) = 0x%08x\n", i+63, addr, __raw_readl(addr));
		}
	}
#endif //#####

	return 0;
}
#endif


#ifdef CONFIG_CMD_NET
/* Configure pins for ethernet (setclear=1) or back to GPIO (setclear=0);
   this function is called by drivers/net/{mcffec.c,mcfmii.c} */
int fecpin_setclear(struct eth_device *dev, int setclear)
{
	struct fec_info_s *info = (struct fec_info_s *)dev->priv;
	if (setclear) {
#ifdef CONFIG_SYS_FEC0_IOBASE
		if (info->iobase == CONFIG_SYS_FEC0_IOBASE) {
			__raw_writel(0x00103192, IOMUXC_PAD_045);	/*MDC*/
			__raw_writel(0x00103193, IOMUXC_PAD_046);	/*MDIO*/
			__raw_writel(0x00103191, IOMUXC_PAD_047);	/*RxDV*/
			__raw_writel(0x00103191, IOMUXC_PAD_048);	/*RxD1*/
			__raw_writel(0x00103191, IOMUXC_PAD_049);	/*RxD0*/
			__raw_writel(0x00103191, IOMUXC_PAD_050);	/*RxER*/
			__raw_writel(0x00103192, IOMUXC_PAD_051);	/*TxD1*/
			__raw_writel(0x00103192, IOMUXC_PAD_052);	/*TxD0*/
			__raw_writel(0x00103192, IOMUXC_PAD_053);	/*TxEn*/
		}
#endif
#ifdef CONFIG_SYS_FEC1_IOBASE
		if (info->iobase == CONFIG_SYS_FEC1_IOBASE) {
			__raw_writel(0x00103192, IOMUXC_PAD_054);	/*MDC*/
			__raw_writel(0x00103193, IOMUXC_PAD_055);	/*MDIO*/
			__raw_writel(0x00103191, IOMUXC_PAD_056);	/*RxDV*/
			__raw_writel(0x00103191, IOMUXC_PAD_057);	/*RxD1*/
			__raw_writel(0x00103191, IOMUXC_PAD_058);	/*RxD0*/
			__raw_writel(0x00103191, IOMUXC_PAD_059);	/*RxER*/
			__raw_writel(0x00103192, IOMUXC_PAD_060);	/*TxD1*/
			__raw_writel(0x00103192, IOMUXC_PAD_061);	/*TxD0*/
			__raw_writel(0x00103192, IOMUXC_PAD_062);	/*TxEn*/
		}
#endif
	} else {
#ifdef CONFIG_SYS_FEC0_IOBASE
		if (info->iobase == CONFIG_SYS_FEC0_IOBASE) {
			__raw_writel(0x00003192, IOMUXC_PAD_045);	/*MDC*/
			__raw_writel(0x00003193, IOMUXC_PAD_046);	/*MDIO*/
			__raw_writel(0x00003191, IOMUXC_PAD_047);	/*RxDV*/
			__raw_writel(0x00003191, IOMUXC_PAD_048);	/*RxD1*/
			__raw_writel(0x00003191, IOMUXC_PAD_049);	/*RxD0*/
			__raw_writel(0x00003191, IOMUXC_PAD_050);	/*RxER*/
			__raw_writel(0x00003192, IOMUXC_PAD_051);	/*TxD1*/
			__raw_writel(0x00003192, IOMUXC_PAD_052);	/*TxD0*/
			__raw_writel(0x00003192, IOMUXC_PAD_053);	/*TxEn*/
		}
#endif
#ifdef CONFIG_SYS_FEC1_IOBASE
		if (info->iobase == CONFIG_SYS_FEC1_IOBASE) {
			__raw_writel(0x00003192, IOMUXC_PAD_054);	/*MDC*/
			__raw_writel(0x00003193, IOMUXC_PAD_055);	/*MDIO*/
			__raw_writel(0x00003191, IOMUXC_PAD_056);	/*RxDV*/
			__raw_writel(0x00003191, IOMUXC_PAD_057);	/*RxD1*/
			__raw_writel(0x00003191, IOMUXC_PAD_058);	/*RxD0*/
			__raw_writel(0x00003191, IOMUXC_PAD_059);	/*RxER*/
			__raw_writel(0x00003192, IOMUXC_PAD_060);	/*TxD1*/
			__raw_writel(0x00003192, IOMUXC_PAD_061);	/*TxD0*/
			__raw_writel(0x00003192, IOMUXC_PAD_062);	/*TxEn*/
		}
#endif
	}

	return 0;
}

extern struct fec_info_s fec_info[];

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
	 *   1. MAC0 from OTP
	 *   2. MAC1 from OTP
	 *   3. CONFIG_ETHADDR_BASE
	 */
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
int board_eth_init(bd_t *bd)
{
	int ret;
	int index = 0;

#ifdef CONFIG_FS_VYBRID_PLL_ETH
	__raw_writel(0x00101902, IOMUXC_PAD_000);   /* RMII_CLKOUT */
#else
	__raw_writel(0x00203191, IOMUXC_PAD_000);   /* RMII_CLKIN */
#endif

#ifdef CONFIG_SYS_FEC0_IOBASE
	switch (fs_nboot_args.chBoardType) {
	case BT_PICOCOMA5:
	case BT_ARMSTONEA5:
	case BT_NETDCUA5:
		set_fs_ethaddr(index);
		ret = mcffec_register(bd, &fec_info[0]);
		if (ret)
			return ret;
		index++;
		break;
	}
#endif

#ifdef CONFIG_SYS_FEC1_IOBASE
	switch (fs_nboot_args.chBoardType) {
	case BT_PICOCOMA5:
	case BT_ARMSTONEA5:
	case BT_NETDCUA5:
		if (!(fs_nboot_args.chFeatures1 & FEAT_2NDLAN))
			break;
		/* Fall through to case BT_AGATEWAY */
	case BT_AGATEWAY:
		set_fs_ethaddr(index);
#ifdef CONFIG_SYS_FEC0_IOBASE
		ret = mcffec_register(bd, &fec_info[1]);
#else
		ret = mcffec_register(bd, &fec_info[0]);
#endif
		break;
	}
#endif
	return ret;
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
u32 get_board_rev(void)
{
	u32 rev;

	/* In addition to the board revision in bits [7:0] we also encode the
	   features in bits [31:16] */
	rev = (fs_nboot_args.chFeatures1 << 24)
		| (fs_nboot_args.chFeatures2 << 16)
		| fs_nboot_args.chBoardRev;

	return rev;
}

#ifdef CONFIG_CMD_LED
/* We have LEDs on PTC30 (Pad 103) and PTC31 (Pad 104); on CUBEA5 and
   AGATEWAY, the logic is inverted */
#if 0
void __led_init (led_id_t mask, int state)
{
	printf("### __led_init()\n");
}
#endif
void __led_set (led_id_t mask, int state)
{
	if ((fs_nboot_args.chBoardType == BT_CUBEA5)
	    || (fs_nboot_args.chBoardType == BT_AGATEWAY))
		state = !state;

	if (mask <= 1) {
		/* Write to GPIO3_PSOR or GPIO3_PCOR */
		mask += 7;
		__raw_writel(1 << mask, state ? 0x400ff0c4 : 0x400ff0c8);
	}
}
void __led_toggle (led_id_t mask)
{
	if (mask <= 1) {
		/* Write to GPIO3_PTOR */
		mask += 7;
		__raw_writel(1 << mask, 0x400ff0cc);
	}
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
