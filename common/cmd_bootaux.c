/*
 * Copyright (C) 2014 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <asm/io.h>
#include <asm/arch/crm_regs.h>		/* CCM_CCGR1, nandf clock settings */

/* M4 Enable */
#define ENABLE_M4 (3<<2)

/* Allow for arch specific config before we boot */
static int __arch_auxiliary_core_up(u32 core_id, u32 boot_private_data)
{
	/* please define platform specific arch_auxiliary_core_up() */
	printf("###__arch_auxiliary_core_up bootaux.c\n");
	return CMD_RET_FAILURE;
}
int arch_auxiliary_core_up(u32 core_id, u32 boot_private_data)
	__attribute__((weak, alias("__arch_auxiliary_core_up")));

/* Allow for arch specific config before we boot */
static int __arch_auxiliary_core_check_up(u32 core_id)
{
	/* please define platform specific arch_auxiliary_core_check_up() */
	return 0;
}

void enable_m4(void)
{
	unsigned int val = 0;
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

  /* Enable M4 clock */
	val = readl(&mxc_ccm->CCGR3);
	val |= ENABLE_M4;
	writel(val, &mxc_ccm->CCGR3);
  printf("## Enable auxiliary core\n");
}

void disable_m4(void)
{
	unsigned int val = 0;
	struct src *src_reg = (struct src *)SRC_BASE_ADDR;
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

  /* Assert SW reset */
	val = readl(&src_reg->scr);
	val |= 0x00000010;
	writel(val, &src_reg->scr);

  /* Disable M4 clock */
  val = readl(&mxc_ccm->CCGR3);
  val &= ~ENABLE_M4;
  writel(val, &mxc_ccm->CCGR3);
  printf("## Disable auxiliary core\n");
}

int arch_auxiliary_core_check_up(u32 core_id)
	__attribute__((weak, alias("__arch_auxiliary_core_check_up")));

int do_bootaux(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
  ulong addr;
  int ret, up;

  if (argc < 2)
    return CMD_RET_USAGE;

  up = arch_auxiliary_core_check_up(0);

  if((strcmp(argv[1], "start") == 0) && !up) {
    enable_m4();
    return CMD_RET_SUCCESS;
  }
  else if((strcmp(argv[1], "stop") == 0) && up) {
    disable_m4();
    return CMD_RET_SUCCESS;
  }

  if (up) {
    printf("## Auxiliary core is already up\n");
    return CMD_RET_SUCCESS;
  }

  addr = parse_loadaddr(argv[1], NULL);

  printf("## Starting auxiliary core at 0x%08lX ...\n", addr);

  ret = arch_auxiliary_core_up(0, addr);
  if (ret)
    return CMD_RET_FAILURE;

  return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	bootaux, CONFIG_SYS_MAXARGS, 1,	do_bootaux,
	"Start auxiliary core",
	""
);
