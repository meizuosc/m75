# Copyright Statement:
#
# MediaTek Inc. (C) 2010.
#

ifneq ($(MTK_EMULATOR_SUPPORT), yes)
LOCAL_PATH:= $(call my-dir)
include $(call all-makefiles-under,$(LOCAL_PATH)/$(call lc,$(MTK_PLATFORM)))
endif
