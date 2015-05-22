include $(MTK_PATH_BUILD)/common.mk
#Fix-me: modified for early porting
#$(call all-subdir-src-or-makefile)
obj-y   += keypad/
obj-y   += uart/
obj-y   += gpio/
obj-y   += wdt/
obj-y   += spm/
obj-y   += vcorefs/
obj-$(CONFIG_MD32_SUPPORT)   += md32/
obj-$(CONFIG_MTK_SYSTRACKER)   += systracker/
obj-y   += power/
obj-y   += mmc-host/
obj-y   += i2c/
obj-y   += pmic_wrap/
obj-y   += usb20/
obj-y   += usb11/
obj-y   += mu3phy/
obj-y   += ../../../common/kernel/drivers/cmdq/
obj-y   += cmdq/
obj-y   += videox/
#obj-y   += hdmitx/
ifeq ($(MTK_HDMI_SUPPORT),yes)
obj-y   += ext_disp/
endif
obj-y   += dispsys/
obj-y   += m4u/
obj-y   += jpeg/
obj-y   += videocodec/
ifneq ($(CONFIG_MTK_FPGA),y)
obj-y   += pwm/
obj-y   += dual_ccci/
obj-y   += eccci/
#obj-y   += btcvsd/
obj-y   += power_gs/
obj-y   += smi/
obj-y   += mmc-host/
obj-y   += spi/
obj-y   += rtc/
obj-y   += mjc/
#obj-y   += sound/
obj-y   += devinfo/
obj-y   += devapc/
obj-y   += auxadc/
obj-y   += ldvt/
obj-y   += masp/
obj-y   += accdet/
obj-y   += vibrator/
obj-y   += leds/
#obj-y   += smartpa/
obj-y   += anc/
obj-y   += thermal/
include $(MTK_PATH_BUILD)/common.mk
obj-y   += $(call all-subdirs-with-makefile,cpu_stress)
ifeq ($(HAVE_CMMB_FEATURE),yes)
obj-m += cmmbdrv/
endif
ifneq	($(MTK_GPU_SUPPORT),no)
obj-$(CONFIG_PVR_RGX_MT6595)   += gpu/
endif
ifeq ($(MTK_GPT_SCHEME_SUPPORT), yes)
obj-y   += partition/
endif
#obj-y   += nand/
#obj-m   += mdrvb/
#obj-y   += touchpanel/
#obj-y   += spi/
#obj-y   += ccci/
#obj-y   += mmc-host/
#obj-y   += rtc/
#
#obj-y   += jpeg/
#obj-y   += gdma/
#
endif
obj-y	+= $(call all-subdirs-with-makefile,met)

ifeq ($(MTK_NFC_SUPPORT),yes)
obj-y	+= nfc/
endif
