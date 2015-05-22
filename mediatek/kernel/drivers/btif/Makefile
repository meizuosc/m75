# combo driver: MT6620
# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	#subdir-ccflags-y can be used in 2.6.34 in the future
  ccflags-y += -I$(src)/inc -I$(src)/hal_inc #-I$(MTK_PATH_PLATFORM)/drivers/btif/inc
  obj-y += btif.o
  btif-y	:= mtk_btif.o mtk_btif_exp.o
  #btif-y	+= hal_btif/hal_btif.o hal_btif/hal_btif_dma.o 
# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD  := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif
