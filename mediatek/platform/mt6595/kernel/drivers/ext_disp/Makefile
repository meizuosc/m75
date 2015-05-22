# drivers/video/mtk/Makefile
#
# Makefile for MT6516 video driver.
#

ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/ext_disp/  \
             -I$(MTK_PATH_SOURCE)/drivers/video/     \
             -I$(MTK_PATH_PLATFORM)/drivers/dispsys/ \
             -I$(MTK_PATH_PLATFORM)/drivers/videox/ \
             -I$(MTK_PATH_SOURCE)/drivers/sync/ \
             -I$(MTK_PATH_SOURCE)/drivers/mhl/driver/mhl_tx_drv/ \
             -I$(MTK_PATH_PLATFORM)/drivers/cmdq/ \
             -I$(MTK_PATH_SOURCE)/../hardware/dpframework/inc/ \
             -I$(srctree)/drivers/staging/android/ion

ifeq ($(CUSTOM_KERNEL_HDMI),nxp_tda19989)
	ccflags-y += 	-DHDMI_TDA19989
endif

ifeq ($(CUSTOM_KERNEL_HDMI),Sii8338)
	ccflags-y += 	-DMHL_SII8338
endif

ifeq ($(CUSTOM_KERNEL_HDMI),Sii8348)
	ccflags-y += 	-DMHL_SII8348
endif

obj-y	:= extd_ddp.o extd_debug.o  extd_drv.o extd_utils.o extd_lcm.o Kara_1280x720.o extd_fence.o




