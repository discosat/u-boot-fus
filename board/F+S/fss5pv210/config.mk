# On fss5pv210 we use two SDRAM banks:
#  DMC0: 256 MB SDRAM bank at 0x20000000 to 0x30000000
#  DMC1: 256 MB SDRAM bank at 0x40000000 to 0x50000000
#
# Linux-Kernel is expected to be at 0x20008000, entry 0x20008000
# U-Boot automatically relocates itself to the end of RAM, so we load
# ourselves at 15MB for now

CONFIG_SYS_TEXT_BASE = 0x40F00000

#### LDSCRIPT := $(SRCTREE)/board/$(BOARDDIR)/u-boot.lds
