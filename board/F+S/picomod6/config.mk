# On PicoMOD6 we use the 128 MB SDRAM bank at 0x50000000 to 0x58000000
#
# Linux-Kernel is expected to be at 0x50008000, entry 0x50008000
# U-Boot automatically relocates itself to the end of RAM, so we load
# ourselves at 15MB for now

CONFIG_SYS_TEXT_BASE = 0x50F00000

#### LDSCRIPT := $(SRCTREE)/board/$(BOARDDIR)/u-boot.lds
