# drivers/video/mtk/Makefile
#
#

obj-$(CONFIG_MTK_FB) += mtkfb_console.o             \
         disp_assert_layer.o                        \
         mtkfb.o                                    \
		 mtkfb_vsync.o			    \
         debug.o                                    \
         primary_display.o                        \
         disp_lcm.o                                 \
         disp_utils.o                               \
	mtkfb_fence.o\
	mtk_disp_mgr.o\
	fbconfig_kdebug_rome.o\
	mtk_mira.o \
	mtk_ovl.o

ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/video/     \
             -I$(MTK_PATH_SOURCE)/drivers/video/       \
             -I$(MTK_PATH_SOURCE)/drivers/sync/        \
			 -I$(MTK_PATH_PLATFORM)/drivers/dispsys/   \
			 -I$(MTK_PATH_PLATFORM)/drivers/m4u/       \
			 -I$(MTK_PATH_PLATFORM)/drivers/cmdq/      \
			 -I$(MTK_PATH_PLATFORM)/core/include/mach/ \
       -I$(MTK_PATH_PLATFORM)/drivers/ext_disp

ccflags-y += -I$(srctree)/drivers/staging/android/ion

ifeq ($(MTK_INTERNAL_HDMI_SUPPORT), yes)
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/hdmi/
endif  
ifeq ($(MTK_INTERNAL_MHL_SUPPORT), yes)
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/mhltx/
endif   
ifeq ($(MTK_DITHERING_SUPPORT), yes)
ccflags-y += -DDITHERING_SUPPORT
endif

ifeq ($(MTK_GPU_SUPPORT), yes)
ccflags-y += -DHWGPU_SUPPORT
endif
