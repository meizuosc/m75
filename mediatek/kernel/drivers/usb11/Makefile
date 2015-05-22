ifeq ($(MTK_ALPS_BOX_SUPPORT),yes)
ccflags-y += -DMTK_ALPS_BOX_SUPPORT
endif

obj-$(CONFIG_MTK_USBFSH) := musbfsh_host.o musbfsh_hsdma.o musbfsh_virthub.o

ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/usb11/ -I$(MTK_PATH_SOURCE)/drivers/usb11/
ccflags-y += -Idrivers/usb/core
