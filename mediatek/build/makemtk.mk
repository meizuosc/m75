# *************************************************************************
# Set shell align with Android build system
# *************************************************************************
SHELL        := /bin/bash
.DELETE_ON_ERROR:
MAKEFLAGS += -rR

ifndef OUT_DIR
  OUT_DIR    :=  out
  LOGDIR      =  $(MKTOPDIR)/out/target/product
  export OUT_DIR
else
# TODO: make OUT_DIR support relative path
  LOGDIR      =  $(MKTOPDIR)/$(OUT_DIR)/target/product
  export OUT_DIR
endif

include mediatek/build/Makefile
$(call codebase-path)
$(call mtk.projectconfig.generate-auto-rules)

PRJ_MF := $(MTK_ROOT_CONFIG_OUT)/ProjectConfig.mk

ifneq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
  ifeq ($(MAKELEVEL),0)
    ifneq ($(filter new% clean% custgen,$(MAKECMDGOALS)),)
.PHONY: $(PRJ_MF)
    else ifneq ($(filter %package %image %-nodeps snod,$(MAKECMDGOALS)),)
MTK_DEPENDENCY_AUTO_CHECK := true
    endif
  endif
endif

#for multiple kernel path
MTK_KERNEL_EXTENSION := mediatek/kernel
KERNEL_SOURCE := kernel
export MTK_KERNEL_EXTENSION
export KERNEL_SOURCE
# *************************************************************************
# Set PHONY
# *************************************************************************
.PHONY : new newall remake remakeall clean cleanall \
         preloader trustonic md32 kernel android \
         check-modem update-modem sign-image encrypt-image sign-modem check-dep \
         dump-memusage gen-relkey check-appres \
         codegen btcodegen javaoptgen clean-javaoptgen emigen nandgen custgen drvgen ptgen \
         pregen preclean \
         update-api modem-info bindergen clean-modem check-seandroid

S_MODULE_LOG  =  $(OUT_DIR)/target/product/$(PROJECT)_$(CUR_MODULE).log
S_CLEAN_LOG = $(MKTOPDIR)/$(PROJECT)_$(CUR_MODULE).log
S_CODEGEN_LOG =  $(OUT_DIR)/target/product/$(PROJECT)_codegen.log
CODEGEN_LOG   =  $(LOGDIR)/$(PROJECT)_codegen.log
MODULE_LOG    =  $(LOGDIR)/$(PROJECT)_$(CUR_MODULE).log
S_LOG =  $(OUT_DIR)/target/product/$(PROJECT)_
LOG   =  $(LOGDIR)/$(PROJECT)_
CUSTOM_MEMORY_HDR = mediatek/custom/$(PROJECT)/preloader/inc/custom_MemoryDevice.h

USERID        =  $(shell whoami)
PRELOADER_WD  =  mediatek/preloader
TRUST_TEE_WD  =  mediatek/trustzone/vendor/trustonic
LK_WD         =  bootable/bootloader/lk
MD32_WD       =  md32/md32 
KERNEL_WD     =  kernel
ANDROID_WD    =  .
ALL_MODULES   =
MAKE_DEBUG    =  --no-print-directory
hide         := @ 
CMD_ARGU2    :=  $(filter-out -j%, $(CMD_ARGU))
REMAKECMD    :=  make -f$(MTK_ROOT_BUILD)/makemtk.mk CMD_ARGU=$(CMD_ARGU) $(CMD_ARGU2) $(MAKE_DEBUG)
CPUCORES     :=  $(shell cat /proc/cpuinfo | grep processor | wc -l)
MAKEJOBS     :=  -j$(CPUCORES)
makemtk_temp := $(shell mkdir -p $(LOGDIR))


#ifeq ($(ACTION),update-api)
#   MAKEJOBS :=
#endif
MAKECMD      :=  make $(MAKEJOBS) $(CMD_ARGU) $(MAKE_DEBUG)

SHOWTIMECMD   =  date "+%Y/%m/%d %H:%M:%S"
SHOWRSLT      =  /usr/bin/perl $(MKTOPDIR)/$(MTK_ROOT_BUILD)/tools/showRslt.pl

PRELOADER_OUT := $(MTK_ROOT_OUT)/PRELOADER_OBJ
PRELOADER_IMAGES := $(PRELOADER_OUT)/bin/preloader_$(PROJECT).bin
TRUST_TEE_IMAGES := $(OUT_DIR)/target/product/$(PROJECT)/trustonic/bin/mobicore.bin
LK_IMAGES     := $(MTK_ROOT_OUT)/BOOTLOADER_OBJ/build-$(PROJECT)/lk.bin
LOGO_IMAGES   := $(MTK_ROOT_OUT)/BOOTLOADER_OBJ/build-$(PROJECT)/logo.bin
MD32_IMAGES   := $(MD32_WD)/build-$(MTK_PLATFORM)/md32_p.bin $(MD32_WD)/build-$(MTK_PLATFORM)/md32_d.bin
# for muti-kernel

ifeq ($(strip $(BUILD_KERNEL)),yes)
   ifneq ($(strip $(LINUX_KERNEL_VERSION)), )
       $(shell if [ ! -e $(KERNEL_WD) ] ; then \
                  ln -s $(LINUX_KERNEL_VERSION) $(KERNEL_WD) ; \
               elif [ -h $(KERNEL_WD) ] ; then \
                  rm -f $(KERNEL_WD) ; \
                  ln -s $(LINUX_KERNEL_VERSION) $(KERNEL_WD) ;\
               fi )
   endif
endif
ifeq ($(strip $(KBUILD_OUTPUT_SUPPORT)),yes)
  KERNEL_IMAGES    := $(MTK_ROOT_OUT)/KERNEL_OBJ/kernel_$(PROJECT).bin
else
  KERNEL_IMAGES    := $(KERNEL_WD)/kernel_$(PROJECT).bin
endif
ANDROID_IMAGES   := $(LOGDIR)/$(PROJECT)/system.img \
                    $(LOGDIR)/$(PROJECT)/boot.img \
                    $(LOGDIR)/$(PROJECT)/recovery.img \
                    $(LOGDIR)/$(PROJECT)/secro.img \
                    $(LOGDIR)/$(PROJECT)/userdata.img
ifeq (true,$(BUILD_TINY_ANDROID))
  ANDROID_IMAGES := $(filter-out %recovery.img,$(ANDROID_IMAGES))
endif
ifneq ($(ACTION),)
  ANDROID_TARGET_IMAGES :=$(filter %/$(patsubst %image,%.img,$(ACTION)),$(ANDROID_IMAGES))
  ifeq (${ACTION},otapackage)
    ANDROID_TARGET_IMAGES :=$(ANDROID_IMAGES)
  endif
  ifeq (${ACTION},snod)
    ANDROID_TARGET_IMAGES :=$(filter %/system.img,$(ANDROID_IMAGES))
  endif
  ifeq (${ACTION},bootimage-nodeps)
    ANDROID_TARGET_IMAGES :=$(filter %/boot.img,$(ANDROID_IMAGES))
endif
endif
ifeq (MT6573, $(MTK_PLATFORM))
  ifeq (android, $(CUR_MODULE))
    ANDROID_IMAGES += $(LOGDIR)/$(PROJECT)/DSP_BL
  endif
endif

SCATTER_FILE := $(OUT_DIR)/target/product/$(PROJECT)/$(MTK_PLATFORM)_Android_scatter.txt
ifeq ($(strip $(MTK_EMMC_SUPPORT)),yes)
  SCATTER_FILE := $(OUT_DIR)/target/product/$(PROJECT)/$(MTK_PLATFORM)_Android_scatter_emmc.txt
endif
ifeq ($(strip $(MTK_YAML_SCATTER_FILE_SUPPORT)),yes)
  SCATTER_FILE := $(OUT_DIR)/target/product/$(PROJECT)/$(MTK_PLATFORM)_Android_scatter.txt
endif

#wschen
OTA_SCATTER_FILE := $(MTK_ROOT_SOURCE)/misc/ota_scatter.txt

export TARGET_PRODUCT=$(PROJECT)
export FLAVOR=$(FLAVOR)

ifneq ($(ACTION), )
  SHOWBUILD     =  $(ACTION)
else
  SHOWBUILD     =  build
endif
SHOWTIME      =  $(shell $(SHOWTIMECMD))
ifeq ($(ENABLE_TEE), TRUE)
  DEAL_STDOUT := 2>&1 | tee -a $(MODULE_LOG)
  DEAL_STDOUT_CLEAN := 2>&1 | tee -a $(S_CLEAN_LOG)
  DEAL_STDOUT_CODEGEN := 2>&1 | tee -a $(CODEGEN_LOG)
  DEAL_STDOUT_BTCODEGEN := 2>&1 | tee -a $(LOG)btcodegen.log
  DEAL_STDOUT_CUSTGEN := 2>&1 | tee -a $(LOG)custgen.log
  DEAL_STDOUT_EMIGEN := 2>&1 | tee -a $(LOG)emigen.log
  DEAL_STDOUT_NANDGEN := 2>&1 | tee -a $(LOG)nandgen.log
  DEAL_STDOUT_JAVAOPTGEN := 2>&1 | tee -a $(LOG)javaoptgen.log
  DEAL_STDOUT_IMEJAVAOPTGEN := 2>&1 | tee -a $(LOG)imejavaoptgen.log
  DEAL_STDOUT_SIGN_IMAGE := 2>&1 | tee -a $(LOG)sign-image.log
  DEAL_STDOUT_ENCRYPT_IMAGE := 2>&1 | tee -a $(LOG)encrypt-image.log
  DEAL_STDOUT_DRVGEN := 2>&1 | tee -a $(LOG)drvgen.log
  DEAL_STDOUT_SIGN_MODEM := 2>&1 | tee -a $(LOG)sign-modem.log
  DEAL_STDOUT_CHECK_MODEM := 2>&1 | tee -a $(LOG)check-modem.log
  DEAL_STDOUT_MODEM_INFO := 2>&1 | tee -a $(LOG)modem-info.log
  DEAL_STDOUT_UPDATE_MD := 2>&1 | tee -a $(LOG)update-modem.log
  DEAL_STDOUT_DUMP_MEMUSAGE := 2>&1 | tee -a $(LOG)dump-memusage.log
  DEAL_STDOUT_PTGEN := 2>&1 | tee -a $(LOG)ptgen.log
  DEAL_STDOUT_MM := 2>&1 | tee -a $(LOG)mm.log
  DEAL_STDOUT_MMA := 2>&1 | tee -a $(LOG)mma.log
  DEAL_STDOUT_CUSTREL := 2>&1 | tee -a $(LOG)rel-cust.log
  DEAL_STDOUT_CHK_APPRES := 2>&1 | tee -a $(LOG)check-appres.log
  DEAL_STDOUT_BINDERGEN := 2>&1 | tee -a $(LOG)bindergen.log
  DEAL_STDOUT_TRUSTZONE := 2>&1 | tee -a $(LOG)trustzone.log
  DEAL_STDOUT_CHECK_SEANDROID :=2>&1 | tee -a $(LOG)check-seandroid.log
else
  DEAL_STDOUT  := >> $(MODULE_LOG) 2>&1
  DEAL_STDOUT_CLEAN := > $(S_CLEAN_LOG) 2>&1
  DEAL_STDOUT_CODEGEN  := > $(CODEGEN_LOG) 2>&1
  DEAL_STDOUT_BTCODEGEN  := > $(LOG)btcodegen.log 2>&1
  DEAL_STDOUT_CUSTGEN := > $(LOG)custgen.log 2>&1
  DEAL_STDOUT_EMIGEN := > $(LOG)emigen.log 2>&1
  DEAL_STDOUT_NANDGEN := > $(LOG)nandgen.log 2>&1
  DEAL_STDOUT_JAVAOPTGEN := > $(LOG)javaoptgen.log 2>&1
  DEAL_STDOUT_IMEJAVAOPTGEN := > $(LOG)imejavaoptgen.log 2>&1
  DEAL_STDOUT_SIGN_IMAGE := > $(LOG)sign-image.log 2>&1
  DEAL_STDOUT_ENCRYPT_IMAGE := > $(LOG)encrypt-image.log 2>&1
  DEAL_STDOUT_SIGN_MODEM := > $(LOG)sign-modem.log 2>&1
  DEAL_STDOUT_CHECK_MODEM := > $(LOG)check-modem.log 2>&1
  DEAL_STDOUT_MODEM_INFO := > $(LOG)modem-info.log 2>&1
  DEAL_STDOUT_DRVGEN := > $(LOG)drvgen.log 2>&1
  DEAL_STDOUT_UPDATE_MD := > $(LOG)update-modem.log 2>&1
  DEAL_STDOUT_DUMP_MEMUSAGE := > $(LOG)dump-memusage.log 2>&1
  DEAL_STDOUT_PTGEN := > $(LOG)ptgen.log 2>&1
  DEAL_STDOUT_MM := > $(LOG)mm.log 2>&1
  DEAL_STDOUT_MMA := > $(LOG)mma.log 2>&1
  DEAL_STDOUT_CUSTREL := > $(LOG)rel-cust.log 2>&1
  DEAL_STDOUT_CHK_APPRES := >> $(LOG)check-appres.log 2>&1
  DEAL_STDOUT_BINDERGEN := > $(LOG)bindergen.log 2>&1
  DEAL_STDOUT_TRUSTZONE := > $(LOG)trustzone.log 2>&1
  DEAL_STDOUT_CHECK_SEANDROID := > $(LOG)check-seandroid.log 2>&1

endif

MAKECMD    +=  TARGET_PRODUCT=$(PROJECT) GEMINI=$(GEMINI) EVB=$(EVB) FLAVOR=$(FLAVOR)

ifeq ($(BUILD_PRELOADER),yes)
  ALL_MODULES += preloader
endif

ifeq ($(TRUSTONIC_TEE_SUPPORT),yes)
  ALL_MODULES += trustonic
endif

ifeq ($(BUILD_LK),yes)
  ALL_MODULES += lk
endif

ifeq ($(BUILD_MD32),yes)
  ALL_MODULES += md32
endif

ifeq ($(BUILD_KERNEL),yes)
  ALL_MODULES += kernel
  KERNEL_ARG = kernel_$(PROJECT).config
endif

ALL_MODULES += android

include $(MTK_ROOT_BUILD)/libs/pack_dep_gen.mk
-include $(MTK_ROOT_BUILD)/tools/preprocess/preprocess.mk
include $(MTK_ROOT_BUILD)/libs/pregen.mk
include $(MTK_ROOT_BUILD)/libs/codegen.mk

ifneq ($(strip $(SHOW_COMMANDS)),)
$(info MAKELEVEL = $(MAKELEVEL), MAKECMDGOALS = $(MAKECMDGOALS), CUR_MODULE = $(CUR_MODULE), ACTION = $(ACTION))
endif

# Note: This is used for special stand alone actions, which is depending on partial custgen. And remake has direct dependency with custgen
ifneq ($(filter %-modem modem-info banyan_opensdk,$(MAKECMDGOALS)),)
    PRIVATE_MTK_NEED_CUSTGEN := true
else ifneq ($(filter clean% new% mm,$(MAKECMDGOALS)),)
  ifeq ($(MAKELEVEL),0)
    PRIVATE_MTK_NEED_CUSTGEN := true
  endif
else ifneq ($(filter android,$(MAKECMDGOALS)),)
  ifneq ($(filter android sdk_addon,$(CUR_MODULE)),)
  else
    PRIVATE_MTK_NEED_CUSTGEN := true
  endif
endif
ifeq ($(PRIVATE_MTK_NEED_CUSTGEN),true)
  mtk-config-files := $(strip $(call mtk.config.generate-rules,mtk-config-files))
  mtk-custom-files := $(strip $(call mtk.custom.generate-rules,mtk-custom-files))
  MTK_CUSTOM_MODEM_FILES := $(filter $(MTK_ROOT_CUSTOM_OUT)/modem/%,$(mtk-custom-files))
  MTK_CUSTOM_MODEM_SOURCES := $(foreach item,$(filter %.img,$(MTK_CUSTOM_MODEM_FILES)),$(patsubst $(item):%,%,$(filter $(item):%,$(_custfmap_))))
  MTK_ALL_CUSTGEN_FILES := $(mtk-config-files) $(mtk-custom-files)
endif

# Note: Used in kernel/Android.mk
ifneq ($(filter newall remakeall,$(MAKECMDGOALS)),)
  ifeq ($(MAKELEVEL),0)
export MTK_KERNEL_MODULES_SKIP_IN_ANDROID := yes
  else
export MTK_KERNEL_MODULES_SKIP_IN_ANDROID :=
  endif
endif
ifneq ($(filter newall remake%,$(MAKECMDGOALS)),)
  ifeq ($(MAKELEVEL),0)
export MTK_SKIP_KERNEL_IN_ANDROID := yes
  endif
endif

# Note: for clean entire out folder use for skip kernel in full source build
#
# Warning: If a pregen's output is used for custgen, it must be put here instead of module build
# Warning: Be care of ANDROID_NATIVE_TARGETS, they will not trigger pregen or custgen
ifeq ($(BUILD_PRELOADER),yes)
  MTK_DEPENDENCY_PREGEN_BEFORE_PRELOADER := emigen nandgen ptgen codegen
  MTK_DEPENDENCY_PRECLEAN_BEFORE_PRELOADER :=
endif
ifeq ($(BUILD_LK),yes)
  MTK_DEPENDENCY_PREGEN_BEFORE_LK        := emigen nandgen ptgen codegen
ifeq ($(LEGACY_DFO_GEN), yes)
  MTK_DEPENDENCY_PREGEN_BEFORE_LK        += $(MTK_ROOT_OUT)/BOOTLOADER_OBJ/build-$(PROJECT)/include/dfo/dfo_boot.h
else
  MTK_DEPENDENCY_PREGEN_BEFORE_LK        += $(MTK_ROOT_OUT)/BOOTLOADER_OBJ/build-$(PROJECT)/include/dfo/dfo_boot_default.h
endif
  MTK_DEPENDENCY_PRECLEAN_BEFORE_LK      := $(filter %/rules.mk %/rules_platform.mk,$(mtk-custom-files))
endif
ifeq ($(BUILD_KERNEL),yes)
  MTK_DEPENDENCY_PREGEN_BEFORE_KERNEL    := nandgen ptgen drvgen
ifeq ($(LEGACY_DFO_GEN), yes)
  MTK_DEPENDENCY_PREGEN_BEFORE_KERNEL    += $(if $(filter yes,$(strip $(KBUILD_OUTPUT_SUPPORT))),$(MTK_ROOT_OUT)/KERNEL_OBJ,$(KERNEL_WD))/include/mach/dfo_boot.h \
                                            $(if $(filter yes,$(strip $(KBUILD_OUTPUT_SUPPORT))),$(MTK_ROOT_OUT)/KERNEL_OBJ,$(KERNEL_WD))/include/mach/dfo_boot_default.h
endif
  MTK_DEPENDENCY_PRECLEAN_BEFORE_KERNEL  := $(filter %/Makefile,$(mtk-custom-files))
endif
ifeq ($(filter generic banyan_addon banyan_addon_x86,$(PROJECT)),)
  MTK_DEPENDENCY_PREGEN_BEFORE_ANDROID   := codegen ptgen
  ifneq ($(MTK_SKIP_KERNEL_IN_ANDROID),yes)
  MTK_DEPENDENCY_PREGEN_BEFORE_ANDROID   += nandgen
  MTK_DEPENDENCY_PREGEN_BEFORE_ANDROID   += $(if $(filter yes,$(strip $(KBUILD_OUTPUT_SUPPORT))),$(MTK_ROOT_OUT)/KERNEL_OBJ,$(KERNEL_WD))/include/mach/dfo_boot.h \
                       			    $(if $(filter yes,$(strip $(KBUILD_OUTPUT_SUPPORT))),$(MTK_ROOT_OUT)/KERNEL_OBJ,$(KERNEL_WD))/include/mach/dfo_boot_default.h
  endif
else
  MTK_DEPENDENCY_PREGEN_BEFORE_ANDROID   :=
endif
  MTK_DEPENDENCY_PREGEN_BEFORE_ANDROID   += $(OUT_DIR)/target/product/$(PROJECT)/obj/include/dfo/CFG_Dfo_File.h \
                                            $(OUT_DIR)/target/product/$(PROJECT)/obj/include/dfo/CFG_Dfo_Default.h \
                                            $(OUT_DIR)/target/product/$(PROJECT)/obj/include/dfo/DfoDefines.h \
                                            $(OUT_DIR)/target/product/$(PROJECT)/obj/include/dfo/DfoBootDefault.h
ifeq ($(LEGACY_DFO_GEN), yes)
  MTK_DEPENDENCY_PREGEN_BEFORE_ANDROID   += $(OUT_DIR)/target/product/$(PROJECT)/obj/include/dfo/DfoBoot.h
endif
  MTK_DEPENDENCY_PRECLEAN_BEFORE_ANDROID := $(MTK_ROOT_CONFIG_OUT)/BoardConfig.mk

ifneq ($(filter %all pregen,$(MAKECMDGOALS)),)
  MTK_DEPENDENCY_PREGEN_LIST := $(MTK_DEPENDENCY_PREGEN_BEFORE_PRELOADER) \
                                $(MTK_DEPENDENCY_PREGEN_BEFORE_LK) \
                                $(MTK_DEPENDENCY_PREGEN_BEFORE_KERNEL) \
                                $(MTK_DEPENDENCY_PREGEN_BEFORE_ANDROID)
  MTK_DEPENDENCY_PRECLEAN_LIST := $(MTK_DEPENDENCY_PRECLEAN_BEFORE_PRELOADER) \
                                  $(MTK_DEPENDENCY_PRECLEAN_BEFORE_LK) \
                                  $(MTK_DEPENDENCY_PRECLEAN_BEFORE_KERNEL) \
                                  $(MTK_DEPENDENCY_PRECLEAN_BEFORE_ANDROID)
else ifneq ($(filter preloader,$(CUR_MODULE)),)
  MTK_DEPENDENCY_PREGEN_LIST := $(MTK_DEPENDENCY_PREGEN_BEFORE_PRELOADER)
  MTK_DEPENDENCY_PRECLEAN_LIST := $(MTK_DEPENDENCY_PRECLEAN_BEFORE_PRELOADER)
else ifneq ($(filter lk,$(CUR_MODULE)),)
  MTK_DEPENDENCY_PREGEN_LIST := $(MTK_DEPENDENCY_PREGEN_BEFORE_LK)
  MTK_DEPENDENCY_PRECLEAN_LIST := $(MTK_DEPENDENCY_PRECLEAN_BEFORE_LK)
else ifneq ($(filter kernel,$(CUR_MODULE)),)
  MTK_DEPENDENCY_PREGEN_LIST := $(MTK_DEPENDENCY_PREGEN_BEFORE_KERNEL)
  MTK_DEPENDENCY_PRECLEAN_LIST := $(MTK_DEPENDENCY_PRECLEAN_BEFORE_KERNEL)
else ifneq ($(filter android,$(CUR_MODULE)),)
  MTK_DEPENDENCY_PREGEN_LIST := $(MTK_DEPENDENCY_PREGEN_BEFORE_ANDROID)
  MTK_DEPENDENCY_PRECLEAN_LIST := $(MTK_DEPENDENCY_PRECLEAN_BEFORE_ANDROID)
endif


pregen: $(PRJ_MF) $(MTK_DEPENDENCY_PREGEN_LIST)
preclean: $(PRJ_MF) $(MTK_DEPENDENCY_PRECLEAN_LIST)
codegen: $(PRJ_MF) drvgen btcodegen cgen
newall: cleanall remakeall
cleanall: preclean
remakeall: pregen custgen
new: clean remake
ifeq ($(MAKELEVEL),0)
clean: preclean
remake: pregen custgen
endif

check-dep: $(PRJ_MF)
	$(eval include $(MTK_ROOT_BUILD)/addon/core/config.mak)
	$(if $(filter error,$(DEP_ERR_CNT)),\
                  $(error Dependency Check FAILED!!))
	@echo "Dependency Check Successfully!!"


mtk_dirs_to_clean := \
        $(MTK_ROOT_CUSTOM_OUT) \
        $(MTK_ROOT_CONFIG_OUT)

cleanall:
ifeq ($(filter -k, $(CMD_ARGU)),)
	$(hide) for i in $(ALL_MODULES); do \
	  $(REMAKECMD) CUR_MODULE=$$i $(subst all,,$@); \
	  if [ $${PIPESTATUS[0]} != 0 ]; then exit 1; fi; \
	done
else
	$(hide) let count=0; for i in $(ALL_MODULES); do \
	$(REMAKECMD) CUR_MODULE=$$i $(subst all,,$@); \
	last_return_code=$${PIPESTATUS[0]}; \
	if [ $$last_return_code != 0 ]; then let count=$$count+$$last_return_code; fi; \
	done; \
	exit $$count
endif
# The output of custgen should be removed only in cleanall, not in clean android
	@echo clean $(mtk_dirs_to_clean)
	$(hide) rm -rf $(mtk_dirs_to_clean)
ifneq ($(filter cleanall,$(MAKECMDGOALS)),)
else
# We still need ProjectConfig.mk after cleanall in new
	$(hide) if [ -e $(dir $(PRJ_MF)) ]; then chmod u+w $(dir $(PRJ_MF)); else mkdir -p $(dir $(PRJ_MF)); fi
	$(hide) make MTK_CUSTGEN_ERROR=no -f mediatek/build/custgen.mk $(PRJ_MF) $(DEAL_STDOUT_CUSTGEN)
endif


remakeall:
ifeq ($(filter -k, $(CMD_ARGU)),)
	$(hide) for i in $(ALL_MODULES); do \
	  $(REMAKECMD) CUR_MODULE=$$i $(subst all,,$@); \
	  if [ $${PIPESTATUS[0]} != 0 ]; then exit 1; fi; \
	done
else
	$(hide) let count=0; for i in $(ALL_MODULES); do \
	$(REMAKECMD) CUR_MODULE=$$i $(subst all,,$@); \
	last_return_code=$${PIPESTATUS[0]}; \
	if [ $$last_return_code != 0 ]; then let count=$$count+$$last_return_code; fi; \
	done; \
	exit $$count
endif


ANDROID_NATIVE_TARGETS := \
         update-api \
         cts sdk win_sdk otapackage banyan_addon banyan_addon_x86 dist updatepackage \
         snod bootimage systemimage recoveryimage secroimage target-files-package \
         factoryimage userdataimage userdataimage-nodeps customimage dump-comp-build-info \
         ubunturootfsimage ubunturootfsimage-nodeps ubuntucustomimage ubuntucustomimage-nodeps \
         devicepackage devicepackage-nodeps \
	 dump-products bootimage-nodeps ramdisk-nodeps javaoptgen
.PHONY: $(ANDROID_NATIVE_TARGETS)

systemimage: check-modem

customimage: clean-customimage

ALL_CUSTOMIMAGE_CLEAN_FILES := \
        $(LOGDIR)/$(PROJECT)/custom.img \
        $(LOGDIR)/$(PROJECT)/custom \
        $(OUT_DIR)/target/common/obj/JAVA_LIBRARIES/mediatek-op_intermediates \
        $(LOGDIR)/$(PROJECT)/obj/ETC/CIP_MD_SBP_intermediates \
        $(LOGDIR)/$(PROJECT)/obj/ETC/DmApnInfo.xml_intermediates \
        $(LOGDIR)/$(PROJECT)/obj/ETC/smsSelfRegConfig.xml_intermediates \
        $(MTK_ROOT_CUSTOM_OUT)/modem


clean-customimage:
	$(hide) echo $(SHOWTIME) $@ing ...
	$(hide) rm -rf $(ALL_CUSTOMIMAGE_CLEAN_FILES)

$(ANDROID_NATIVE_TARGETS): $(PRJ_MF) custgen $(filter $(OUT_DIR)/target/product/$(PROJECT)/obj/include/dfo/%,$(MTK_DEPENDENCY_PREGEN_BEFORE_ANDROID)) $(ALLJAVAOPTFILES)
	$(hide) \
        $(if $(filter update-api,$@),\
          $(if $(filter true,$(strip $(BUILD_TINY_ANDROID))), \
            echo SKIP $@... \
            , \
            $(if $(filter snod bootimage-nodeps userdataimage-nodeps,$@), \
              , \
              /usr/bin/perl $(MTK_ROOT_BUILD)/tools/mtkBegin.pl $(FULL_PROJECT) && \
             ) \
            $(REMAKECMD) ACTION=$@ CUR_MODULE=$@ android \
           ) \
          , \
          $(if $(filter snod bootimage-nodeps userdataimage-nodeps,$@), \
            , \
            /usr/bin/perl $(MTK_ROOT_BUILD)/tools/mtkBegin.pl $(FULL_PROJECT) && \
           ) \
          $(if $(filter banyan_addon banyan_addon_x86,$@), \
            $(REMAKECMD) ACTION=sdk_addon CUR_MODULE=sdk_addon android \
            , \
            $(REMAKECMD) ACTION=$@ CUR_MODULE=$@ android \
           ) \
         )

ifeq ($(TARGET_PRODUCT),emulator)
   TARGET_PRODUCT := generic
endif
.PHONY: mm
mm: $(PRJ_MF) $(MTK_ALL_CUSTGEN_FILES)
ifeq ($(HAVE_PREPROCESS_FLOW),true)
mm: run-preprocess
endif
mm:
	$(hide) echo $(SHOWTIME) $@ing...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$@.log
	$(hide) rm -f $(LOG)$@.log $(LOG)$@.log_err
	$(hide) (source build/envsetup.sh;TARGET_PRODUCT=$(TARGET_PRODUCT) FLAVOR=$(FLAVOR) mmm $(MM_PATH) $(MAKEJOBS) $(SNOD) $(DEAL_STDOUT_MM);exit $${PIPESTATUS[0]})  && \
          $(SHOWRSLT) $$? $(LOG)$@.log || \
          $(SHOWRSLT) $$? $(LOG)$@.log

.PHONY: mma
mma: pregen custgen
ifeq ($(HAVE_PREPROCESS_FLOW),true)
mma: run-preprocess
endif
mma:
	 $(hide) echo $(SHOWTIME) $@ing...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$@.log
	$(hide) rm -f $(LOG)$@.log $(LOG)$@.log_err
	$(hide) (source build/envsetup.sh;TARGET_PRODUCT=$(TARGET_PRODUCT) FLAVOR=$(FLAVOR) mmma $(MM_PATH) $(MAKEJOBS) $(SNOD) $(DEAL_STDOUT_MMA);exit $${PIPESTATUS[0]})  && \
          $(SHOWRSLT) $$? $(LOG)$@.log || \
          $(SHOWRSLT) $$? $(LOG)$@.log


.PHONY: trustzone
TRUSTZONE_PROTECT_PRIVATE_SECURITY_PATH := $(wildcard mediatek/protect-private/security/Android.mk)
TRUSTZONE_PROTECT_PRIVATE_SEC_DRV_PATH := $(wildcard mediatek/protect-private/sec_drv/Android.mk)
TRUSTZONE_PROTECT_PRIVATE_SEC_DRM_PATH := $(wildcard mediatek/protect-private/sec_drm/Android.mk)
TRUSTZONE_PROTECT_PRIVATE_MTEE_PATH := $(wildcard mediatek/protect-private/mtee/Android.mk)
TRUSTZONE_PROTECT_PRIVATE_DRM_PATH := $(wildcard mediatek/protect-private/drm/Android.mk)
TRUSTZONE_PROTECT_PATH := $(wildcard mediatek/protect/trustzone/Android.mk)
TRUSTZONE_PROTECT_PLATFORM_PATH := $(wildcard mediatek/protect/platform/$(call lc,$(MTK_PLATFORM))/trustzone/Android.mk)
TRUSTZONE_PROTECT_BSP_PATH := $(wildcard mediatek/protect-bsp/trustzone/Android.mk)
TRUSTZONE_PROTECT_BSP_PLATFORM_PATH := $(wildcard mediatek/protect-bsp/platform/$(call lc,$(MTK_PLATFORM))/trustzone/Android.mk)
TRUSTZONE_PATH := $(wildcard mediatek/trustzone/Android.mk)
TRUSTZONE_PLATFORM_PATH := $(wildcard mediatek/platform/$(call lc,$(MTK_PLATFORM))/trustzone/Android.mk)
TRUSTZONE_ALL_PATH :="$(TRUSTZONE_PROTECT_PRIVATE_SECURITY_PATH) $(TRUSTZONE_PROTECT_PRIVATE_SEC_DRV_PATH) $(TRUSTZONE_PROTECT_PRIVATE_SEC_DRM_PATH) $(TRUSTZONE_PROTECT_PRIVATE_MTEE_PATH) $(TRUSTZONE_PROTECT_PRIVATE_DRM_PATH) $(TRUSTZONE_PROTECT_PATH) $(TRUSTZONE_PROTECT_PLATFORM_PATH) $(TRUSTZONE_PROTECT_BSP_PATH) $(TRUSTZONE_PROTECT_BSP_PLATFORM_PATH) $(TRUSTZONE_PLATFORM_PATH) $(TRUSTZONE_PATH)"

trustzone:
ifeq ($(ACTION),)
	$(hide) echo $(SHOWTIME) $@ing...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$@.log
	$(hide) rm -f $(LOG)$@.log $(LOG)$@.log_err
	$(hide) (source build/envsetup.sh;TARGET_PRODUCT=$(TARGET_PRODUCT) FLAVOR=$(FLAVOR) ONE_SHOT_MAKEFILE=$(TRUSTZONE_ALL_PATH) m $(MAKEJOBS) $(SNOD) $(DEAL_STDOUT_TRUSTZONE);exit $${PIPESTATUS[0]})  && \
          $(SHOWRSLT) $$? $(LOG)$@.log || \
          $(SHOWRSLT) $$? $(LOG)$@.log
endif

.PHONY: rel-cust
ifeq ($(DUMP),true)
rel-cust: dump_option := -d
endif
rel-cust:
	$(hide) echo $(SHOWTIME) $@ing...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$@.log
	$(hide) rm -f $(LOG)$@.log $(LOG)$@.log_err
	$(hide) python $(MTK_ROOT_BUILD)/tools/customRelease.py $(dump_option) ./ $(RELEASE_DEST) $(TARGET_PRODUCT) $(MTK_RELEASE_PACKAGE).xml $(DEAL_STDOUT_CUSTREL) && \
         $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log || \
         $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log

clean:
ifneq ($(strip $(MTK_DEPENDENCY_AUTO_CHECK)), true)
	$(hide) $(REMAKECMD) ACTION=$@ $(CUR_MODULE)
endif

mrproper:
ifneq ($(strip $(MTK_DEPENDENCY_AUTO_CHECK)), true)
	$(hide) $(REMAKECMD) ACTION=$@ $(CUR_MODULE)
endif

remake:
	$(hide) /usr/bin/perl $(MTK_ROOT_BUILD)/tools/mtkBegin.pl $(FULL_PROJECT)
	$(hide) $(REMAKECMD) ACTION= $(CUR_MODULE)

#### Remove old modem files under mediatek/custom/out/$project/modem ####
clean-modem:
	$(hide) rm -rf $(strip $(MTK_ROOT_CUSTOM_OUT))/modem
ifeq ($(strip $(MTK_CIP_SUPPORT)), yes) 
	$(hide) rm -rf $(strip $(LOGDIR)/$(PROJECT))/custom/etc/extmddb
	$(hide) rm -rf $(strip $(LOGDIR)/$(PROJECT))/custom/etc/mddb
else
	$(hide) rm -rf $(strip $(LOGDIR)/$(PROJECT))/system/etc/extmddb
	$(hide) rm -rf $(strip $(LOGDIR)/$(PROJECT))/system/etc/mddb
endif

update-modem: clean-modem $(MTK_ALL_CUSTGEN_FILES) check-modem sign-modem
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) $@ing...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$@.log
	$(hide) rm -f $(LOG)$@.log $(LOG)$@.log_err
	$(hide) ./makeMtk $(FULL_PROJECT) mm build/target/board/ snod $(DEAL_STDOUT_UPDATE_MD) && \
         $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log || \
         $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log


custgen: $(PRJ_MF)
	$(hide) echo $(SHOWTIME) $@ing...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$@.log
	$(hide) rm -f $(LOG)$(basename $(notdir $@)).log $(LOG)$(basename $(notdir $@)).log_err
	$(hide) make $(MAKEJOBS) $(MAKE_DEBUG) MTK_CUSTGEN_ERROR=no -f $(MTK_ROOT_BUILD)/custgen.mk $(DEAL_STDOUT_CUSTGEN) && \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log || $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log

sign-image:
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) $@ing ...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$@.log
	$(hide) rm -f $(LOG)$@.log $(LOG)$@.log_err
	$(hide) perl $(MTK_ROOT_BUILD)/tools/SignTool/SignTool.pl $(PROJECT) $(FULL_PROJECT) $(MTK_SEC_SECRO_AC_SUPPORT) $(MTK_NAND_PAGE_SIZE) $(DEAL_STDOUT_SIGN_IMAGE) && \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log || \
          $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log

encrypt-image:
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) $@ing ...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$@.log
	$(hide) rm -f $(LOG)$@.log $(LOG)$@.log_err
	$(hide) perl $(MTK_ROOT_BUILD)/tools/encrypt_image.pl $(PROJECT) $(DEAL_STDOUT_ENCRYPT_IMAGE) && \
          $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log || \
          $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log


sign-modem:
ifeq ($(filter generic banyan_addon banyan_addon_x86,$(PROJECT)),)
ifneq ($(MTK_SIGNMODEM_SUPPORT),no)
$(eval $(call mtk-check-dependency,sign-modem,$(MTK_DEPENDENCY_OUTPUT)))
endif
endif
$(MTK_DEPENDENCY_OUTPUT)/sign-modem.dep: | $(MTK_CUSTOM_MODEM_FILES)
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) $(basename $(notdir $@))ing ...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$(basename $(notdir $@)).log
	$(hide) rm -f $(LOG)$(basename $(notdir $@)).log $(LOG)$(basename $(notdir $@)).log_err
	$(hide) perl $(MTK_ROOT_BUILD)/tools/sign_modem.pl \
                     $(FULL_PROJECT) \
                     $(MTK_SEC_MODEM_ENCODE) \
                     $(MTK_SEC_MODEM_AUTH) \
                     $(PROJECT) \
                     $(MTK_SEC_SECRO_AC_SUPPORT) \
                     $(DEAL_STDOUT_SIGN_MODEM) && \
                $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log || \
                $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log
	$(call mtk-print-dependency)
	$(hide) perl $(MTK_DEPENDENCY_SCRIPT) --overwrite $@ $@  $(MTK_ROOT_BUILD)/tools/SignTool/dep/ "\.dep" $(MTK_ROOT_BUILD)/tools/SignTool/
# workaround: check-modem will fail after sign-modem
	$(hide) touch -c $(MTK_DEPENDENCY_OUTPUT)/check-modem.dep

MODEM_INFO_FLAG := $(foreach f, $(CUSTOM_MODEM), $(wildcard $(MTK_ROOT_CUSTOM)/common/modem/$(f)/modem*.info))

modem-info: clean-modem $(MTK_CUSTOM_MODEM_FILES)
modem-info: PRIVATE_MODEM_PATH := $(strip $(MTK_ROOT_CUSTOM_OUT))/modem
modem-info: PRIVATE_CHK_MD_TOOL := $(MTK_ROOT_BUILD)/tools/checkMD.pl
modem-info:
	$(hide) echo MODEM_INFO_FLAG = $(MODEM_INFO_FLAG)
	$(hide) echo $(SHOWTIME) $@ing ...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$@.log
	$(hide) rm -f $(LOG)$@.log $(LOG)$@.log_err
	$(hide) perl $(PRIVATE_CHK_MD_TOOL) \
                     PROJECT=$(PROJECT) \
                     PRIVATE_MODEM_PATH=$(PRIVATE_MODEM_PATH) \
                     MTK_PLATFORM=$(MTK_PLATFORM) \
                     MTK_MD1_SUPPORT=$(MTK_MD1_SUPPORT) \
                     MTK_MD2_SUPPORT=$(MTK_MD2_SUPPORT) \
                     MTK_GET_BIN_INFO=$@ \
                     $(DEAL_STDOUT_MODEM_INFO) && \
          $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log || \
          $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log

check-modem:
ifeq ($(filter generic banyan_addon banyan_addon_x86,$(PROJECT)),)
  ifneq ($(MTK_PLATFORM),MT8135)
-include $(MTK_DEPENDENCY_OUTPUT)/./check-modem.dep
check-modem: $(MTK_DEPENDENCY_OUTPUT)/check-modem.dep
  endif
endif
ifneq ($(filter update-modem,$(MAKECMDGOALS)),)
.PHONY: $(MTK_DEPENDENCY_OUTPUT)/check-modem.dep
.PHONY: $(MTK_CUSTOM_MODEM_FILES)
endif
$(MTK_DEPENDENCY_OUTPUT)/check-modem.dep: PRIVATE_CHK_MD_TOOL := $(MTK_ROOT_BUILD)/tools/checkMD.pl
$(MTK_DEPENDENCY_OUTPUT)/check-modem.dep: PRIVATE_MODEM_PATH := $(strip $(MTK_ROOT_CUSTOM_OUT))/modem
$(MTK_DEPENDENCY_OUTPUT)/check-modem.dep: | $(MTK_CUSTOM_MODEM_FILES)
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) $(basename $(notdir $@))ing ...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$(basename $(notdir $@)).log
	$(hide) rm -f $(LOG)$(basename $(notdir $@)).log $(LOG)$(basename $(notdir $@)).log_err
	$(hide) perl $(PRIVATE_CHK_MD_TOOL) \
                     PROJECT=$(PROJECT) \
                     PRIVATE_MODEM_PATH=$(PRIVATE_MODEM_PATH) \
                     MTK_PLATFORM=$(MTK_PLATFORM) \
                     MTK_MD1_SUPPORT=$(MTK_MD1_SUPPORT) \
                     MTK_MD2_SUPPORT=$(MTK_MD2_SUPPORT) \
                     MTK_GET_BIN_INFO=modem-info \
                     $(DEAL_STDOUT_CHECK_MODEM) && \
          $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log || \
          $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log
	$(call mtk-print-dependency)


dump-memusage: MEM_USAGE_LABEL := $(if $(LABEL),$(LABEL),$(shell date +%Y-%m-%d_%H:%M:%S))
dump-memusage: MEM_USAGE_GENERATOR := $(MTK_ROOT_BUILD)/tools/memmon/rommon.pl
dump-memusage: PRIVATE_PROJECT := $(if $(filter emulator, $(PROJECT)),generic,$(PROJECT))
dump-memusage: MEM_USAGE_DATA_LOCATION := $(MTK_ROOT_BUILD)/tools/memmon/data
dump-memusage: IMAGE_LOCATION := $(OUT_DIR)/target/product/$(PRIVATE_PROJECT)
dump-memusage:
	$(hide) echo $(SHOWTIME) $@ing ...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$@.log
	$(hide) rm -f $(LOG)$@.log $(LOG)$@.log_err
	$(hide) perl $(MEM_USAGE_GENERATOR) \
                     $(MEM_USAGE_LABEL) \
                     $(PRIVATE_PROJECT) \
                     $(FLAVOR) \
                     $(MEM_USAGE_DATA_LOCATION) \
                     $(IMAGE_LOCATION) \
                     $(DEAL_STDOUT_DUMP_MEMUSAGE) && \
                $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log || \
                $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log


ifneq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
.PHONY: $(OTA_SCATTER_FILE)
endif
OTA_SCATTER_GENERATOR := $(MTK_ROOT_BUILD)/tools/ptgen/ota_scatter.pl
$(OTA_SCATTER_FILE): $(SCATTER_FILE) $(OTA_SCATTER_GENERATOR)
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) mkdir -p $(dir $@)
	$(hide) perl $(OTA_SCATTER_GENERATOR) $< $@


gen-relkey: PRIVATE_KEY_GENERATOR := development/tools/make_key
gen-relkey: PRIVATE_KEY_LOCATION := build/target/product/security/$(TARGET_PRODUCT)
gen-relkey: PRIVATE_KEY_LIST := releasekey media shared platform
gen-relkey: PRIVATE_SIGNATURE_SUBJECT := $(strip $(SIGNATURE_SUBJECT))
gen-relkey:
	$(hide) echo "Generating release key/certificate..."
	$(hide) if [ ! -d $(PRIVATE_KEY_LOCATION) ]; then \
                  mkdir $(PRIVATE_KEY_LOCATION); \
                fi
	$(hide) for key in $(PRIVATE_KEY_LIST); do \
                  $(PRIVATE_KEY_GENERATOR) $(strip $(PRIVATE_KEY_LOCATION))/$$key '$(PRIVATE_SIGNATURE_SUBJECT)' < /dev/null; \
                done

# check unused application resource
check-appres: PRIVATE_SCANNING_FOLDERS := packages/apps
check-appres: PRIVATE_CHECK_TOOL := $(MTK_ROOT_BUILD)/tools/FindDummyRes.py
check-appres:
	$(hide) echo $(SHOWTIME) $(SHOWBUILD)ing $@...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$@.log
	$(hide) rm -rf $(LOG)$@.log*
	$(hide) for d in $(PRIVATE_SCANNING_FOLDERS); do \
                  $(PRIVATE_CHECK_TOOL) -d $$d \
                  $(DEAL_STDOUT_CHK_APPRES); \
                done
	$(hide) $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log


preloader:
ifeq ($(BUILD_PRELOADER),yes)
	$(hide) echo $(SHOWTIME) $(SHOWBUILD)ing $@...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_MODULE_LOG)
	$(hide) rm -f $(MODULE_LOG) $(MODULE_LOG)_err
  ifneq ($(ACTION), )
	$(hide) cd $(PRELOADER_WD) && \
	  (make clean $(DEAL_STDOUT) && \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG) $(ACTION) || \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG) $(ACTION)) && cd $(MKTOPDIR)
  else
	$(hide) cd $(PRELOADER_WD) && \
	  (./build.sh $(PROJECT) $(ACTION) $(DEAL_STDOUT) && \
	  cd $(MKTOPDIR) && \
          $(call chkImgSize,$(ACTION),$(PROJECT),$(SCATTER_FILE),$(PRELOADER_IMAGES),$(DEAL_STDOUT),&&) \
          $(if $(strip $(ACTION)),:,$(call copytoout,$(PRELOADER_IMAGES),$(LOGDIR)/$(PROJECT))) && \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG) || \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG))
  endif
else
	$(hide) echo Not support $@.
endif

trustonic:
ifeq ($(TRUSTONIC_TEE_SUPPORT),yes)
	$(hide) echo $(SHOWTIME) $(SHOWBUILD)ing $@...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_MODULE_LOG)
	$(hide) rm -f $(MODULE_LOG) $(MODULE_LOG)_err
  ifneq ($(ACTION), )
  else
	$(hide) cd $(TRUST_TEE_WD)/t-base && \
	  (./build.sh $(PROJECT) $(ACTION) $(DEAL_STDOUT) && \
	  cd $(MKTOPDIR) && \
          $(call chkImgSize,$(ACTION),$(PROJECT),$(SCATTER_FILE),$(TRUST_TEE_IMAGES),$(DEAL_STDOUT),&&) \
          $(if $(strip $(ACTION)),:,$(call copytoout,$(TRUST_TEE_IMAGES),$(LOGDIR)/$(PROJECT))) && \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG) || \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG))
  endif
else
	$(hide) echo Not support $@.
endif

ifeq ($(if $(filter eng,$(TARGET_BUILD_VARIANT)),$(if $(filter yes, $(MTK_MEM_PRESERVED_MODE_ENABLE)),yes)),yes)
# eng build & enable mem preserved mode
lk: preloader
else
# user build or do not enable mem preserved mode
lk:
endif
ifeq ($(BUILD_LK),yes)
	$(hide) echo $(SHOWTIME) $(SHOWBUILD)ing $@...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_MODULE_LOG)
	$(hide) rm -f $(MODULE_LOG) $(MODULE_LOG)_err
  ifneq ($(ACTION), )
	$(hide) cd $(LK_WD) && \
	  (PROJECT=$(PROJECT) make clean $(DEAL_STDOUT) && \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG) $(ACTION) || \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG) $(ACTION)) && cd $(MKTOPDIR)
  else
	$(hide) cd $(LK_WD) && \
	  (FULL_PROJECT=$(FULL_PROJECT) make $(MAKEJOBS) $(PROJECT) $(ACTION) $(DEAL_STDOUT) && \
	  cd $(MKTOPDIR) && \
          $(call chkImgSize,$(ACTION),$(PROJECT),$(SCATTER_FILE),$(LK_IMAGES),$(DEAL_STDOUT) &&) \
          $(if $(strip $(ACTION)),:,$(call copytoout,$(LK_IMAGES),$(LOGDIR)/$(PROJECT))) && \
          $(if $(strip $(ACTION)),:,$(call copytoout,$(LOGO_IMAGES),$(LOGDIR)/$(PROJECT))) && \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG) || \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG))
  endif
else
	$(hide) echo Not support $@.
endif

md32:
ifeq ($(BUILD_MD32),yes)
  ifneq ($(wildcard $(MD32_WD)),)
	$(hide) echo $(SHOWTIME) $(SHOWBUILD)ing $@...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_MODULE_LOG)
	$(hide) rm -f $(MODULE_LOG) $(MODULE_LOG)_err
    ifneq ($(ACTION), )
	$(hide) cd $(MD32_WD) && \
	  (make PROJECT=md32_$(call lc,$(MTK_PLATFORM)) md32_$(call lc,$(MTK_PLATFORM)) clean $(DEAL_STDOUT) && \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG) $(ACTION) || \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG) $(ACTION)) && cd $(MKTOPDIR)
    else
	$(hide) cd $(MD32_WD) && \
	  (make PROJECT=md32_$(call lc,$(MTK_PLATFORM)) $(MAKEJOBS) md32_$(call lc,$(MTK_PLATFORM)) $(ACTION) $(DEAL_STDOUT) && \
	  cd $(MKTOPDIR) && \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG) || \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG))
    endif
  else
	$(hide) echo Skip $@.
  endif
else
	$(hide) echo Not support $@.
endif

kernel:
ifeq ($(BUILD_KERNEL),yes)
  ifneq ($(KMOD_PATH),)
	$(hide)	echo building kernel module KMOD_PATH=$(KMOD_PATH)
	$(hide) cd $(KERNEL_WD) && \
	(KMOD_PATH=$(KMOD_PATH) ./build.sh $(ACTION) $(KERNEL_ARG) ) && cd $(MKTOPDIR)
  else
	$(hide) echo $(SHOWTIME) $(SHOWBUILD)ing $@...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_MODULE_LOG)
	$(hide) rm -f $(MODULE_LOG) $(MODULE_LOG)_err
    ifneq ($(ACTION),clean)
# backup old kernel_*.bin for comparision
	$(hide) if [ -e $(KERNEL_IMAGES) ]; then mv -f $(KERNEL_IMAGES) $(KERNEL_IMAGES).bak; fi
    endif
	$(hide) cd $(KERNEL_WD) && \
	  (MAKEJOBS=$(MAKEJOBS) ./build.sh $(ACTION) $(PROJECT) $(DEAL_STDOUT) && \
	   cd $(MKTOPDIR) && \
	   $(call chkImgSize,$(ACTION),$(PROJECT),$(SCATTER_FILE),$(if $(strip $(ACTION)),,$(KERNEL_IMAGES)),$(DEAL_STDOUT),&&) \
           $(if $(strip $(ACTION)),:,$(call copytoout,$(KERNEL_IMAGES),$(LOGDIR)/$(PROJECT))) && \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG) $(ACTION) || \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG) $(ACTION))
    ifneq ($(ACTION),clean)
# restore to original kernel_*.bin to keep timestamp
	$(hide) if [ -e $(KERNEL_IMAGES) ] && [ -e $(KERNEL_IMAGES).bak ] && cmp -s $(KERNEL_IMAGES) $(KERNEL_IMAGES).bak; then echo $(KERNEL_IMAGES) has no change; mv -f $(KERNEL_IMAGES).bak $(KERNEL_IMAGES); fi
	$(hide) rm -f $(KERNEL_IMAGES).bak
    endif
  endif
else
	$(hide) echo Not support $@.
endif


ifneq ($(ACTION),clean)
android: check-modem sign-modem
else
android: clean-javaoptgen
endif
ifeq ($(HAVE_PREPROCESS_FLOW),true)
  ifeq ($(ACTION),clean)
android: clean-preprocessed
  else
android: run-preprocess
  endif
endif
ifneq ($(strip $(MTK_SKIP_KERNEL_IN_ANDROID)), yes)
   ifeq ($(filter generic banyan_addon banyan_addon_x86,$(PROJECT)),)
   ifeq ($(ACTION),)
android: kernel
    endif
   endif
endif 

android: CHECK_IMAGE := $(ANDROID_TARGET_IMAGES)
android:
ifeq ($(ACTION), )
	$(hide) /usr/bin/perl $(MTK_ROOT_BUILD)/tools/mtkBegin.pl $(FULL_PROJECT)
endif
ifneq ($(DR_MODULE),)
   ifneq ($(ACTION), clean)
	$(hide) echo building android module MODULE=$(DR_MODULE)
	$(MAKECMD) $(DR_MODULE)
   else
	$(hide) echo cleaning android module MODULE=$(DR_MODULE)
	$(hide) $(MAKECMD) clean-$(DR_MODULE)
   endif
else
	$(hide) echo $(SHOWTIME) $(SHOWBUILD)ing $@...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_MODULE_LOG)
	$(hide) rm -f $(MODULE_LOG) $(MODULE_LOG)_err
  ifeq ($(ACTION),clean)
	$(hide) ($(MAKECMD) $(ACTION) $(DEAL_STDOUT_CLEAN);exit $${PIPESTATUS[0]}) && \
	$(SHOWRSLT) $${PIPESTATUS[0]} $(S_CLEAN_LOG) $(ACTION) || \
	$(SHOWRSLT) $${PIPESTATUS[0]} $(S_CLEAN_LOG) $(ACTION)

  else
	$(hide) ($(MAKECMD) $(ACTION) $(DEAL_STDOUT);exit $${PIPESTATUS[0]}) && \
	  $(if $(filter clean,$(ACTION)),,$(call chkImgSize,$(ACTION),$(PROJECT),$(SCATTER_FILE),$(if $(strip $(ACTION)),$(CHECK_IMAGE),$(ANDROID_IMAGES)),$(DEAL_STDOUT),&&)) \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG) $(ACTION) || \
	  $(SHOWRSLT) $${PIPESTATUS[0]} $(MODULE_LOG) $(ACTION)
  endif
endif


define chkImgSize
$(if $(filter no,$(MTK_CHKIMGSIZE_SUPPORT)), \
     echo "Check Img size process disabled due to MTK_CHKIMGSIZE_SUPPORT is set to no" $(5) $(6),\
     $(call chkImgSize1,$(1),$(2),$(3),$(4),$(5),$(6)) \
)
endef
##############################################################
# function:  chkImgSize1
# arguments: $(ACTION) $(PROJECT) $(SCATTER_FILE) $(IMAGES) $(DEAL_STDOUT) &&
#############################################################
define chkImgSize1
$(if $(strip $(1)), \
     $(if $(strip $(4)), \
          $(if $(filter generic, $(2)),, \
               perl $(MTK_ROOT_BUILD)/tools/chkImgSize.pl $(3) $(2) $(4) $(5) $(6) \
           ) \
      ), \
     $(if $(filter generic, $(2)),, \
         perl $(MTK_ROOT_BUILD)/tools/chkImgSize.pl $(3) $(2) $(4) $(5) $(6) \
      ) \
 )
endef

#############################################################
# function: copytoout
# arguments: $(SOURCE) $(TARGET)
#############################################################
define copytoout
mkdir -p $(2);cp -f $(1) $(2)
endef

bindergen:
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) $@ing...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$@.log
	$(hide) rm -f $(LOG)$@.log $(LOG)$@.log_err
	$(hide) $(MTK_ROOT_BUILD)/tools/bindergen/bindergen.pl $(DEAL_STDOUT_BINDERGEN) && \
	 $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log || \
	 $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log


check-seandroid:
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) $@ing...$(FULL_PROJECT) $(LOGDIR)
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$@.log
	$(hide) rm -f $(LOG)$@.log $(LOG)$@.log_err
	$(hide) python $(MTK_ROOT_BUILD)/tools/SEMTK_policy_check.py $(FULL_PROJECT) $(LOGDIR) $(DEAL_STDOUT_CHECK_SEANDROID) && \
         $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log || \
         $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$@.log


banyan_opensdk: $(filter %.mk,$(mtk-config-files))
	$(MAKECMD) $@

#make bootimage based busybox ramdisk.
BB_RD:=$(CURDIR)/mediatek/build/kernel/ramdisk.img
KERNEL_IMG:=$(OUT_DIR)/target/product/$(PROJECT)/kernel_$(PROJECT).bin
MTK_MKBT:=$(CURDIR)/mediatek/build/tools/images/mkbootimg
bootimage-busybox:
ifeq ($(MTK_MKBT), $(wildcard $(MTK_MKBT)))
	$(hide) echo "mkimage file exists!"
	chmod 0755 $(MTK_MKBT)
ifeq ($(BB_RD), $(wildcard $(BB_RD)))
	$(hide) echo "ramdisk file exists!"
ifeq ($(KERNEL_IMG), $(wildcard $(KERNEL_IMG)))
	$(MTK_MKBT) --kernel $(KERNEL_IMG) --ramdisk $(BB_RD) -o $(CURDIR)/bootbb.img
	mv $(CURDIR)/bootbb.img $(CURDIR)/out/target/product/$(PROJECT)/boot.img
endif
	chmod 0664 $(MTK_MKBT)
endif
endif
