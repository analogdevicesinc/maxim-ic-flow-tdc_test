################################################################################
 # Copyright (C) 2018 Maxim Integrated Products, Inc., All Rights Reserved.
 #
 # Permission is hereby granted, free of charge, to any person obtaining a
 # copy of this software and associated documentation files (the "Software"),
 # to deal in the Software without restriction, including without limitation
 # the rights to use, copy, modify, merge, publish, distribute, sublicense,
 # and/or sell copies of the Software, and to permit persons to whom the
 # Software is furnished to do so, subject to the following conditions:
 #
 # The above copyright notice and this permission notice shall be included
 # in all copies or substantial portions of the Software.
 #
 # THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 # OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 # MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 # IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 # OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 # ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 # OTHER DEALINGS IN THE SOFTWARE.
 #
 # Except as contained in this notice, the name of Maxim Integrated
 # Products, Inc. shall not be used except as stated in the Maxim Integrated
 # Products, Inc. Branding Policy.
 #
 # The mere transfer of this software does not imply any licenses
 # of trade secrets, proprietary technology, copyrights, patents,
 # trademarks, maskwork rights, or any other form of intellectual
 # property whatsoever. Maxim Integrated Products, Inc. retains all
 # ownership rights.
 #
 ###############################################################################

BOARD=max35104evkit2_max32625mbed
MAXIM_PATH=../board/$(BOARD)/csl
PROJECT=tdc_test
TARGET=MAX32625
TARGET_REV=0x4132
COMPILER=GCC
LIBS_DIR=../board/$(BOARD)/csl
CMSIS_ROOT=$(LIBS_DIR)/CMSIS

SRCS  = main.c config.c flow.c transducer.c uui.c board.c max3510x.c

PATHS=.. ../board/$(BOARD) ../board/$(BOARD)/max3510x

VPATH=$(PATHS)
IPATH=$(PATHS)


PROJ_CFLAGS+=-DMXC_ASSERT_ENABLE -DMAX35104 -Wno-unused-function

PERIPH_DRIVER_DIR=$(LIBS_DIR)/PeriphDriver
include $(PERIPH_DRIVER_DIR)/periphdriver.mk

include $(CMSIS_ROOT)/Device/Maxim/$(TARGET)/Source/$(COMPILER)/$(TARGET).mk

distclean: clean
	$(MAKE) -C ${PERIPH_DRIVER_DIR} clean
