# drivers/video/mtk/Makefile
#
#

obj-$(CONFIG_MTK_FB) +=                             \
         mtkfb_console.o                            \
		 mtkfb_vsync.o								\
         debug.o

# note that we use disp_drv_ext.o ... both when MTK_SEC_VIDEO_PATH_SUPPORT and
#   TRUSTONIC_TEE_SUPPORT
ifeq ($(strip $(MTK_SEC_VIDEO_PATH_SUPPORT)),yes)
ifeq ($(strip $(TRUSTONIC_TEE_SUPPORT)),yes)
obj-$(CONFIG_MTK_FB) += 							\
		disp_drv_svp.o 								\
		disp_assert_layer_svp.o                     \
		mtkfb_svp.o 								\
		disp_sync_svp.o 							\
		disp_mgr.o
else
obj-$(CONFIG_MTK_FB) += 							\
	    disp_drv.o 									\
        disp_assert_layer.o                         \
	    mtkfb.o 									\
	    disp_sync.o
endif
else
obj-$(CONFIG_MTK_FB) += 							\
		disp_drv.o 									\
        disp_assert_layer.o                         \
		mtkfb.o 									\
		disp_sync.o
endif


#ifeq ($(MTK_DISP_CONFIG_SUPPORT), yes)
obj-$(CONFIG_MTK_FB) += fbconfig_kdebug.o
#endif

ccflags-y += -I$(MTK_PATH_SOURCE)/drivers/video/  \
			 -I$(MTK_PATH_PLATFORM)/drivers/video/  \
			 -I$(MTK_PATH_PLATFORM)/drivers/dispsys/ \
			 -I$(MTK_PATH_PLATFORM)/core/include/mach/ \
			 -I$(MTK_PATH_SOURCE)/drivers/sync/

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

ifeq ($(CONFIG_MMPROFILE),y)
ccflags-y += -DDEFAULT_MMP_ENABLE
endif


ifeq ($(MTK_ALPS_BOX_SUPPORT), yes)
ccflags-y += -DMTK_ALPS_BOX_SUPPORT
endif

