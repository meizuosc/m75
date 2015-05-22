ccflags-y += -I$(srctree)
ccflags-$(CONFIG_MD32_SUPPORT) += -I$(MTK_PATH_PLATFORM)/drivers/md32
obj-$(CONFIG_THERMAL) += mtk_thermal_monitor.o
obj-$(CONFIG_THERMAL) += mtk_cooler_shutdown.o
ifeq ($(CONFIG_MTK_LEDS), y)
    # setMaxbrightness() defined in mediatek/kernel/drivers/leds/leds_drv.c only built when CONFIG_MTK_LEDS is set
    obj-$(CONFIG_THERMAL) += mtk_cooler_backlight.o 
endif
obj-$(CONFIG_THERMAL) += mtk_cooler_kshutdown.o
#obj-$(CONFIG_THERMAL) += mtk_cooler_spkrvol.o
#obj-$(CONFIG_THERMAL) += mtk_cooler_dropcall.o
obj-$(CONFIG_THERMAL) += mtk_cooler_cam.o
obj-$(CONFIG_THERMAL) += mtk_cooler_vrt.o