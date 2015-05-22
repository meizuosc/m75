include $(MTK_ROOT_BUILD)/Makefile
LED_PLATFORM_DRIVER_DIR :=  $(srctree)/mediatek/platform/$(call lc,$(MTK_PLATFORM))/kernel/drivers/accdet
obj-$(CONFIG_MTK_ACCDET) := accdet_drv.o

ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/power\
             -I$(MTK_PATH_SOURCE)/drivers/power
             
ccflags-y  += -I$(LED_PLATFORM_DRIVER_DIR)/
