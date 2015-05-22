# BTIF HAL driver for MT6582
# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
  ccflags-y += -I$(src)/inc -I$(MTK_PATH_PLATFORM)/core/include -I$(MTK_ROOT)/kernel/drivers/btif/hal_inc
  obj-y += btif_hal.o
  btif_hal-y	+= hal_btif.o hal_btif_dma.o 
# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD  := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif
