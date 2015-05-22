include $(MTK_ROOT_BUILD)/Makefile
LED_PLATFORM_DRIVER_DIR :=  $(srctree)/mediatek/platform/$(call lc,$(MTK_PLATFORM))/kernel/drivers/vibrator
obj-$(CONFIG_MTK_VIBRATOR) := vibrator_drv.o  drv2605_motor.o
#ccflags-y := -Idrivers/staging/android/
ccflags-y  += -I$(LED_PLATFORM_DRIVER_DIR)/
