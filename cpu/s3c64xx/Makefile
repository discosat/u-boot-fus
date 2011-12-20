#
# (C) Copyright 2000-2003
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# See file CREDITS for list of people who contributed to this
# project.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#

include $(TOPDIR)/config.mk

LIB	= lib$(CPU).a
START   = start.o

START-$(CONFIG_SMDK6400) += start_6400.o
START-$(CONFIG_SMDK6410) += start_irom.o
# No START on PicoMOD6 and PicoCOM3
START := $(START-y)

COBJS-$(CONFIG_SMDK6400) +=  nand_cp.o i2c.o serial.o usb_ohci.o \
		interrupts.o cpu.o nand.o onenand.o onenand_cp.o \
		usbd-otg-hs.o movi.o setup_hsmmc.o
COBJS-$(CONFIG_SMDK6410) +=  nand_cp.o i2c.o serial.o usb_ohci.o \
		interrupts.o cpu.o nand.o onenand.o onenand_cp.o \
		usbd-otg-hs.o movi.o setup_hsmmc.o
COBJS-$(CONFIG_PICOMOD6) +=  serial.o usb_ohci.o interrupts.o \
		cpu.o nand.o #usbd-otg-hs.o setup_hsmmc.o
COBJS-$(CONFIG_PICOCOM3) +=  serial.o usb_ohci.o interrupts.o \
		cpu.o nand.o #usbd-otg-hs.o setup_hsmmc.o
COBJS := $(COBJS-y)

all:	.depend $(START) $(LIB)

$(LIB):	$(COBJS)
	$(AR) crv $@ $(COBJS)

#########################################################################

.depend:	Makefile $(START:.o=.S) $(COBJS:.o=.c)
		$(CC) -M $(CFLAGS) $(START:.o=.S) $(COBJS:.o=.c) > $@

sinclude .depend

#########################################################################
