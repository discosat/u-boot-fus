# On PicoCOM3 we use the 64 MB SDRAM bank at 0x50000000 to 0x54000000
#
# Linux-Kernel is expected to be at 0x50008000, entry 0x50008000
# we load ourselves to 0x53f00000 with or without MMU

sinclude $(OBJTREE)/board/$(BOARDDIR)/config.tmp

ifndef CONFIG_NAND_SPL
TEXT_BASE = $(RAM_TEXT)
else
TEXT_BASE = 0
endif

LDSCRIPT := $(SRCTREE)/board/$(BOARDDIR)/u-boot.lds
