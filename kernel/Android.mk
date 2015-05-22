## Android Makefile for kernel module
## by Kirby.Wu, 20090729, mediatek.inc
##
## this android makefile is (currently) used to build all kernel modules 
## and put them into android platform output directory. 
##
## to build kernel modules into system.img,
##   config build/target/board/<target>/BoardConfig.mk:
##     KERNEL_CONFIG_FILE := <your desired config file> # use .config if omit
##     TARGET_MODULES     := true                       # do make modules
##

#ifeq ($(MTK_PROJECT), gw616)
#  KERNEL_CONFIG_FILE := config-mt6516-phone
#else
#  ifeq ($(MTK_PROJECT), ds269)
#    KERNEL_CONFIG_FILE := config-mt6516-gemini
#  else
#    ifeq ($(MTK_PROJECT), oppo)
#      KERNEL_CONFIG_FILE := config-mt6516-oppo
#    else
#      ifeq ($(MTK_PROJECT), mt6516_evb)
#        KERNEL_CONFIG_FILE := config-mt6516-evb
#      else
#        ifeq ($(MTK_PROJECT), mt6573_evb)
#          KERNEL_CONFIG_FILE := config-mt6573-evb
#        else
#          ifeq ($(MTK_PROJECT), zte73v1)
#            KERNEL_CONFIG_FILE := config-mt6573-zte73v1
#          else
#            KERNEL_CONFIG_FILE := config-mt6516-$(MTK_PROJECT)
#          endif
#        endif
#      endif
#    endif
#  endif
#endif

MTK_CURRENT_KERNEL_DIR := $(call my-dir)
ifeq (kernel, $(lastword  $(subst /, , $(MTK_CURRENT_KERNEL_DIR))))
KERNEL_DIR := kernel
#KERNEL_DIR := $(call my-dir)
KERNEL_DIR_TO_ROOT := ..
#ARCH ?= arm
#CROSS_COMPILE ?= arm-linux-androideabi-
KERNEL_MAKE_CMD := + make MTK_PROJECT=$(MTK_PROJECT) -C $(KERNEL_DIR) $(if $(strip $(SHOW_COMMANDS)),V=1)

ifeq ($(strip $(KBUILD_OUTPUT_SUPPORT)),yes)
KBUILD_OUTPUT := $(KERNEL_DIR_TO_ROOT)/$(MTK_ROOT_OUT)/KERNEL_OBJ
KERNEL_OUTPUT_TO_ROOT := $(KERNEL_DIR_TO_ROOT)/../../../../..
KERNEL_DOTCONFIG_FILE := $(KBUILD_OUTPUT)/.config
KERNEL_MAKE_CMD += O=$(KBUILD_OUTPUT)
$(shell mkdir -p $(MTK_ROOT_OUT)/KERNEL_OBJ)
else
KERNEL_OUTPUT :=
KERNEL_OUTPUT_TO_ROOT := $(KERNEL_DIR_TO_ROOT)
KERNEL_DOTCONFIG_FILE := .config
endif

ifneq ($(filter /% ~%,$(TARGET_OUT)),)
KERNEL_MODULE_INSTALL_PATH := $(TARGET_OUT)
else
KERNEL_MODULE_INSTALL_PATH := $(KERNEL_OUTPUT_TO_ROOT)/$(TARGET_OUT)
endif

#$(info using $(KERNEL_CONFIG_FILE) .... )
ifeq ($(TARGET_KMODULES),true)
ALL_PREBUILT += $(TARGET_OUT)/lib/modules/modules.order
$(BUILT_SYSTEMIMAGE): kernel_modules
$(TARGET_OUT)/lib/modules/modules.order: kernel_modules
ifneq ($(ONE_SHOT_MAKEFILE),)
all_modules: kernel_modules
endif

################
## For WLAN switch
################
LINK_WLAN_NAME := $(TARGET_OUT)/lib/modules/wlan
LINK_P2P_NAME := $(TARGET_OUT)/lib/modules/p2p

KO_POSTFIX := _$(shell echo $(strip $(MTK_WLAN_CHIP)) | tr A-Z a-z)

CUR_WLAN_KO_NAME := wlan$(KO_POSTFIX).ko
CUR_P2P_KO_NAME := p2p$(KO_POSTFIX).ko

CUR_WLAN_KO_PATH := $(TARGET_OUT)/lib/modules/$(CUR_WLAN_KO_NAME)
CUR_P2P_KO_PATH := $(TARGET_OUT)/lib/modules/$(CUR_P2P_KO_NAME)

kernel_modules:
	@echo "building linux kernel modules..."
	$(KERNEL_MAKE_CMD) modules
	$(KERNEL_MAKE_CMD) INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=$(KERNEL_MODULE_INSTALL_PATH) INSTALL_MOD_DIR=$(KERNEL_MODULE_INSTALL_PATH) android_modules_install
#ifeq ($(strip $(MTK_WLAN_SUPPORT)),yes)
#	-@ln -sf $(CUR_WLAN_KO_NAME) $(LINK_WLAN_NAME).ko
#endif

endif #ifeq ($(TARGET_KMODULES),true)
endif
