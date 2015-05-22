# common driver code for CMDQ (ROME, K2~)
# 72/82/92 does not use this common code

# drivers/cmdq
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/cmdq \
             -I$(MTK_PATH_PLATFORM)/core/include \
             -I$(MTK_PATH_PLATFORM)/drivers/dispsys \
             -I$(MTK_PATH_PLATFORM)/drivers/smi 

# Enable MET
ccflags-y += -DMET_USER_EVENT_SUPPORT

obj-y += cmdq_record.o
obj-y += cmdq_core.o
obj-y += cmdq_mutex.o
obj-y += cmdq_driver.o
obj-y += cmdq_mmp.o
obj-y += cmdq_mdp_common.o

obj-y += cmdq_test.o
