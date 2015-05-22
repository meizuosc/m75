ifneq ($(KERNELRELEASE),)

ifeq ($(CONFIG_MTK_COMBO),y)
	ccflags-y += -I$(src)/inc/
	ccflags-y += -I$(src)/../
	obj-y += conn_drv_init.o
	obj-y += common_drv_init.o
	obj-y += bluetooth_drv_init.o
	obj-y += gps_drv_init.o
	obj-y += fm_drv_init.o
ifeq ($(MTK_WLAN_SUPPORT), yes)
	obj-y += wlan_drv_init.o
endif
endif

else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD  := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif
