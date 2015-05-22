# Copyright Statement:
#
# MediaTek Inc. (C) 2010.
#

#
# Makefile for the Linux Bluetooth HCI device drivers.
#
#ccflags-y := -I$(src)/include -I$(src)/../common/include -I$(src)/../common/linux/include
ccflags-y :=-I$(src)/../common/core/include \
					  -I$(src)/include \
					  -I$(src)/../common/include \
					  -I$(src)/../common/linux/include \
					  -I$(src)/../../common_detect \
					  -I$(src)/../../btif/inc \
					  -I$(src)/../common/linux/pri/include
obj-$(CONFIG_MTK_COMBO_BT_HCI) += hci_stp.o
#obj-y += hci_stp.o
hci_stp-objs	:= linux/hci_stp.o
