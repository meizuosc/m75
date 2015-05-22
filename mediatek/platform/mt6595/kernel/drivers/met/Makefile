ifeq ($(MET_TOP),)

include $(MTK_ROOT_BUILD)/Makefile

#export CONFIG_MTK_MET=y
ccflags-y += -Werror
obj-y := core/

met-platform := $(call lc,$(MTK_PLATFORM))
platform-dir := platform/$(met-platform)/
check-platform-dir-exist := $(wildcard $(srctree)/mediatek/platform/$(met-platform)/kernel/drivers/met/platform/$(met-platform)/)
ifneq ($(check-platform-dir-exist),)
	obj-y += $(platform-dir)
endif

subdir-ccflags-y := -I$(srctree)/mediatek/platform/$(met-platform)/kernel/drivers/met

ifeq ($(CONFIG_THERMAL),y)
	subdir-ccflags-y +=	-DNO_MTK_THERMAL_GET_TEMP=0
else
	subdir-ccflags-y +=	-DNO_MTK_THERMAL_GET_TEMP=1
endif

else
ifneq ($(KERNELRELEASE),)

ifeq ($(CONFIG_MTK_MET),)
obj-y := core/
endif

ifneq ($(MET_PLATFORM),)
obj-y += platform/$(MET_PLATFORM)/
endif

subdir-ccflags-y := -I$(MET_TOP)/met

else

ifeq ($(KDIR),)
$(error Error! You should execute "make" in parent directory)
endif

export KBUILD_OUTPUT_SUPPORT=yes

default:
	$(MAKE) -C $(KDIR) M=$$PWD
clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

endif

endif
