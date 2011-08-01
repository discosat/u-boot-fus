#
# (C) Copyright 2009
# Gary Jennejohn, DENX Software Engineering, <gj@denx.de>
# David Mueller, ELSOFT AG, <d.mueller@elsoft.ch>
#
# F&S PicoCOM3 board with S3C6410 cpu
#
# see http://www.fs-net.de/ for more information on F&S
#

#
# PicoCOM3 has 64 MB Mobile DDR
#
# 5000'0000 to 5400'0000
#
#
# Linux-Kernel is expected to be at 5000'8000, entry 5000'8000
# optionally with a ramdisk at 5080'0000
#
# we load ourself to 53F0'0000 without MMU
# with MMU, load address is changed to 0xc3F0_0000
#
# download area is 5000'0000
#


ifndef TEXT_BASE
###HK TEXT_BASE = 0xc3F00000
TEXT_BASE = 0x53F00000
endif

