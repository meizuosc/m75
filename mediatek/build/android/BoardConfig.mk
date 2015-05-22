# default TARGET_CPU_ABI
TARGET_CPU_ABI := armeabi
board_config_mk := $(MTK_ROOT_CONFIG_OUT)/BoardConfig.mk

ifeq ($(MTK_HWUI_SUPPORT), yes)
  USE_OPENGL_RENDERER := true
else
  USE_OPENGL_RENDERER := false
endif

# try to include by-project BoardConfig.
# inclusion failure will auto trigger auto-generation & rebuild
include $(MTK_ROOT_CONFIG_OUT)/BoardConfig.mk
-include $(MTK_ROOT_OUT)/PTGEN/configs/partition_size.mk
