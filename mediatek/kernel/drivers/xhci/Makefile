#
# Makefile for USB Host Lib
#

ccflags-y := -I$(src)/../../../../kernel/drivers/usb/host
ccflags-y += -I$(src)/../../../../kernel/drivers/usb/core

ifeq ($(MTK_OTG_PMIC_BOOST_5V),yes)
	EXTRA_CFLAGS		+= -DMTK_OTG_PMIC_BOOST_5V
endif

ifeq ($(CONFIG_USB_DEBUG),y)
	EXTRA_CFLAGS		+= -DDEBUG
endif

obj-y	+= xhci-mtk-power.o xhci-mtk-scheduler.o xhci-mtk.o xhci-mtk-attrs.o
