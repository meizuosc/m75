#
# external modem control driver
#
ifneq ($(MTK_EXTERNAL_MODEM_SLOT),)
# EVDO check ----------------------------
ifeq ($(EVDO_DT_SUPPORT), yes)
obj-  := dummy.o
else

# obj-$(CONFIG_MTK_EMD_CTRL_DRIVER) += emd_ctl.o
obj-y += emd_ctl.o
emd_ctl-y := emd_ctl_init.o emd_ctl_cfifo.o emd_ctl_node.o emd_ctl_chr.o emd_ctl_spm.o

# if not define CUSTOM_KERNEL_EXTMD,then use dummy file
ifeq ($(CUSTOM_KERNEL_EXTMD),) 
emd_ctl-y += emd_ctl_hal.o
endif

# EVDO check ----------------------------
endif

else
obj-  := dummy.o
endif

