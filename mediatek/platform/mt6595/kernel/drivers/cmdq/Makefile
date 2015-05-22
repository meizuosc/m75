# drivers/cmdq

ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/dispsys \
             -I$(MTK_PATH_PLATFORM)/drivers/cmdq
ccflags-y += -I$(MTK_PATH_PLATFORM)/../../common/kernel/drivers/cmdq

# Enable MET
ccflags-y += -DMET_USER_EVENT_SUPPORT

obj-y += cmdq_mdp.o
obj-y += cmdq_platform.o
obj-y += cmdq_device.o
