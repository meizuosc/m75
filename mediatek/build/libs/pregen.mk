MTK_DFO_TARGET_OUT_HEADERS := $(OUT_DIR)/target/product/$(PROJECT)/obj/include/dfo
MTK_DFO_ALL_GENERATED_SOURCES := \
  $(MTK_DFO_TARGET_OUT_HEADERS)/CFG_Dfo_File.h \
  $(MTK_DFO_TARGET_OUT_HEADERS)/CFG_Dfo_Default.h \
  $(MTK_DFO_TARGET_OUT_HEADERS)/DfoDefines.h \
  $(MTK_DFO_TARGET_OUT_HEADERS)/DfoBootDefault.h
ifeq ($(LEGACY_DFO_GEN), yes)
MTK_DFO_ALL_GENERATED_SOURCES += \
  $(MTK_DFO_TARGET_OUT_HEADERS)/DfoBoot.h \
  $(MTK_ROOT_OUT)/BOOTLOADER_OBJ/build-$(PROJECT)/include/dfo/dfo_boot.h \
  $(if $(filter yes,$(strip $(KBUILD_OUTPUT_SUPPORT))),$(MTK_ROOT_OUT)/KERNEL_OBJ,$(KERNEL_WD))/include/mach/dfo_boot.h \
  $(if $(filter yes,$(strip $(KBUILD_OUTPUT_SUPPORT))),$(MTK_ROOT_OUT)/KERNEL_OBJ,$(KERNEL_WD))/include/mach/dfo_boot_default.h
else
MTK_DFO_ALL_GENERATED_SOURCES += \
  $(MTK_ROOT_OUT)/BOOTLOADER_OBJ/build-$(PROJECT)/include/dfo/dfo_boot_default.h
endif

ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
  -include $(MTK_DEPENDENCY_OUTPUT)/./CFG_Dfo_File.dep
  -include $(MTK_DEPENDENCY_OUTPUT)/./CFG_Dfo_Default.dep
  -include $(MTK_DEPENDENCY_OUTPUT)/./DfoDefines.dep
ifeq ($(LEGACY_DFO_GEN), yes)
  -include $(MTK_DEPENDENCY_OUTPUT)/./DfoBoot.dep
endif
  -include $(MTK_DEPENDENCY_OUTPUT)/./DfoBootDefault.dep
else
  .PHONY: $(MTK_DFO_ALL_GENERATED_SOURCES)
endif

$(MTK_DFO_TARGET_OUT_HEADERS)/CFG_Dfo_File.h: $(MTK_ROOT_BUILD)/tools/gendfo.pl $(MTK_ROOT_CONFIG_OUT)/ProjectConfig.mk
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) gen $@ ...
	$(hide) mkdir -p $(dir $@)
	$(hide) perl mediatek/build/tools/gendfo.pl nvhdr $@ >$(LOG)$(basename $(notdir $@)).log 2>&1 && \
	 $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log || \
	 $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log
	$(hide) touch $@
	$(hide) perl $(MTK_DEPENDENCY_SCRIPT) $(MTK_DEPENDENCY_OUTPUT)/$(basename $(notdir $@)).dep $@ $(dir $(LOG)$(basename $(notdir $@)).log) "\b$(notdir $(LOG)$(basename $(notdir $@)))\.log"

$(MTK_DFO_TARGET_OUT_HEADERS)/CFG_Dfo_Default.h: $(MTK_ROOT_BUILD)/tools/gendfo.pl $(MTK_ROOT_CONFIG_OUT)/ProjectConfig.mk
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) gen $@ ...
	$(hide) mkdir -p $(dir $@)
	$(hide) perl mediatek/build/tools/gendfo.pl nvdft $@ >$(LOG)$(basename $(notdir $@)).log 2>&1 && \
	 $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log || \
	 $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log
	$(hide) touch $@
	$(hide) perl $(MTK_DEPENDENCY_SCRIPT) $(MTK_DEPENDENCY_OUTPUT)/$(basename $(notdir $@)).dep $@ $(dir $(LOG)$(basename $(notdir $@)).log) "\b$(notdir $(LOG)$(basename $(notdir $@)))\.log"

$(MTK_DFO_TARGET_OUT_HEADERS)/DfoDefines.h: $(MTK_ROOT_BUILD)/tools/gendfo.pl $(MTK_ROOT_CONFIG_OUT)/ProjectConfig.mk
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) gen $@ ...
	$(hide) mkdir -p $(dir $@)
	$(hide) perl mediatek/build/tools/gendfo.pl def $@ >$(LOG)$(basename $(notdir $@)).log 2>&1 && \
	 $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log || \
	 $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log
	$(hide) touch $@
	$(hide) perl $(MTK_DEPENDENCY_SCRIPT) $(MTK_DEPENDENCY_OUTPUT)/$(basename $(notdir $@)).dep $@ $(dir $(LOG)$(basename $(notdir $@)).log) "\b$(notdir $(LOG)$(basename $(notdir $@)))\.log"

ifeq ($(LEGACY_DFO_GEN), yes)
$(MTK_DFO_TARGET_OUT_HEADERS)/DfoBoot.h: $(MTK_ROOT_BUILD)/tools/gendfoboot.pl $(MTK_ROOT_CONFIG_OUT)/ProjectConfig.mk
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) gen $@ ...
	$(hide) mkdir -p $(dir $@)
	$(hide) perl mediatek/build/tools/gendfoboot.pl boot $@ >$(LOG)$(basename $(notdir $@)).log 2>&1 && \
	 $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log || \
	 $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log
	$(hide) touch $@
	$(hide) perl $(MTK_DEPENDENCY_SCRIPT) $(MTK_DEPENDENCY_OUTPUT)/$(basename $(notdir $@)).dep $@ $(dir $(LOG)$(basename $(notdir $@)).log) "\b$(notdir $(LOG)$(basename $(notdir $@)))\.log"
endif

$(MTK_DFO_TARGET_OUT_HEADERS)/DfoBootDefault.h: $(MTK_ROOT_BUILD)/tools/gendfoboot.pl $(MTK_ROOT_CONFIG_OUT)/ProjectConfig.mk
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) gen $@ ...
	$(hide) mkdir -p $(dir $@)
	$(hide) perl mediatek/build/tools/gendfoboot.pl bootdft $@ >$(LOG)$(basename $(notdir $@)).log 2>&1 && \
	 $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log || \
	 $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log
	$(hide) touch $@
	$(hide) perl $(MTK_DEPENDENCY_SCRIPT) $(MTK_DEPENDENCY_OUTPUT)/$(basename $(notdir $@)).dep $@ $(dir $(LOG)$(basename $(notdir $@)).log) "\b$(notdir $(LOG)$(basename $(notdir $@)))\.log"

ifeq ($(LEGACY_DFO_GEN), yes)
$(MTK_ROOT_OUT)/BOOTLOADER_OBJ/build-$(PROJECT)/include/dfo/dfo_boot.h: $(MTK_DFO_TARGET_OUT_HEADERS)/DfoBoot.h
	$(hide) mkdir -p $(dir $@)
	$(hide) cp -f $< $@
else
$(MTK_ROOT_OUT)/BOOTLOADER_OBJ/build-$(PROJECT)/include/dfo/dfo_boot_default.h: $(MTK_DFO_TARGET_OUT_HEADERS)/DfoBootDefault.h
	$(hide) mkdir -p $(dir $@)
	$(hide) cp -f $< $@
endif

ifeq ($(LEGACY_DFO_GEN), yes)
ifeq ($(MTK_GPL_PACKAGE),yes)
# working dir is kernel/out
$(if $(objtree),$(objtree)/)include/mach/dfo_boot.h: $(MTK_ROOT_BUILD)/tools/gendfoboot.pl $(MTK_ROOT_CONFIG_OUT)/ProjectConfig.mk
	-@echo [Update] $@: $?
	$(hide) echo $(SHOWTIME) gen $@ ...
	$(hide) mkdir -p $(dir $@)
	$(hide) $(if $(TO_ROOT),cd $(TO_ROOT);) perl mediatek/build/tools/gendfoboot.pl boot $@

$(if $(objtree),$(objtree)/)include/mach/dfo_boot_default.h: $(MTK_ROOT_BUILD)/tools/gendfoboot.pl $(MTK_ROOT_CONFIG_OUT)/ProjectConfig.mk
	-@echo [Update] $@: $?
	$(hide) echo $(SHOWTIME) gen $@ ...
	$(hide) mkdir -p $(dir $@)
	$(hide) $(if $(TO_ROOT),cd $(TO_ROOT);) perl mediatek/build/tools/gendfoboot.pl bootdft $@

else

$(if $(filter yes,$(strip $(KBUILD_OUTPUT_SUPPORT))),$(MTK_ROOT_OUT)/KERNEL_OBJ,$(KERNEL_WD))/include/mach/dfo_boot.h: $(MTK_DFO_TARGET_OUT_HEADERS)/DfoBoot.h
	$(hide) mkdir -p $(dir $@)
	$(hide) cp -f $< $@

$(if $(filter yes,$(strip $(KBUILD_OUTPUT_SUPPORT))),$(MTK_ROOT_OUT)/KERNEL_OBJ,$(KERNEL_WD))/include/mach/dfo_boot_default.h: $(MTK_DFO_TARGET_OUT_HEADERS)/DfoBootDefault.h
	$(hide) mkdir -p $(dir $@)
	$(hide) cp -f $< $@

endif
endif

.PHONY: dfogen
dfogen: $(MTK_DFO_ALL_GENERATED_SOURCES)


drvgen:
# $(MTK_ROOT_CUSTOM)/$project/kernel/dct/dct/*.h *.c
PRIVATE_DRVGEN_DEPENDENCY := $(shell find -L $(MTK_ROOT_SOURCE)/dct/ -type f | sed 's/ /\\ /g')
ifneq ($(MTK_DRVGEN_SUPPORT),no)
ifneq ($(PROJECT),generic)
$(eval $(call mtk-check-dependency,drvgen,$(MTK_DEPENDENCY_OUTPUT)))
$(eval $(call mtk-check-argument,drvgen,$(MTK_DEPENDENCY_OUTPUT),$(PRIVATE_DRVGEN_DEPENDENCY)))
endif
endif
$(MTK_DEPENDENCY_OUTPUT)/drvgen.dep: PRIVATE_CUSTOM_KERNEL_DCT:= $(if $(CUSTOM_KERNEL_DCT),$(CUSTOM_KERNEL_DCT),dct)
$(MTK_DEPENDENCY_OUTPUT)/drvgen.dep:
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) $(basename $(notdir $@))ing ...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$(basename $(notdir $@)).log
	$(hide) rm -f $(LOG)$(basename $(notdir $@)).log $(LOG)$(basename $(notdir $@)).log_err
	$(hide) mkdir -p $(MTK_ROOT_OUT)/DRVGEN
	$(hide) $(MTK_ROOT_SOURCE)/dct/DrvGen $(MTK_ROOT_CUSTOM)/$(PROJECT)/kernel/dct/$(PRIVATE_CUSTOM_KERNEL_DCT)/codegen.dws $(MTK_ROOT_OUT)/DRVGEN $(MTK_ROOT_OUT)/DRVGEN $(DEAL_STDOUT_DRVGEN) $(MTK_DEPENDENCY_LOG)
ifneq ($(LOG),)
	@echo '[Dependency] $(PRIVATE_DRVGEN_DEPENDENCY) $(MTK_ROOT_CUSTOM)/$(PROJECT)/kernel/dct/$(PRIVATE_CUSTOM_KERNEL_DCT)/codegen.dws' >> $(LOG)$(basename $(notdir $@)).log
	$(call mtk-print-dependency)
	$(call mtk-print-argument,$(PRIVATE_DRVGEN_DEPENDENCY))
endif


ifeq ($(MTK_TABLET_DRAM),yes)
  MEMORY_DEVICE_XLS := mediatek/build/tools/TabletEmiList/$(MTK_PLATFORM)/TabletMemoryDeviceList_$(MTK_PLATFORM).xls
else
  MEMORY_DEVICE_XLS := mediatek/build/tools/emigen/$(MTK_PLATFORM)/MemoryDeviceList_$(MTK_PLATFORM).xls
endif


CUSTOM_NAND_HDR := $(MTK_ROOT_CUSTOM)/$(PROJECT)/common/nand_device_list.h
nandgen:
ifneq ($(PROJECT), generic)
$(eval $(call mtk-check-dependency,nandgen,$(MTK_DEPENDENCY_OUTPUT)))
endif
$(MTK_DEPENDENCY_OUTPUT)/nandgen.dep:
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) $(basename $(notdir $@))ing ...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$(basename $(notdir $@)).log
	$(hide) rm -f $(LOG)$(basename $(notdir $@)).log $(LOG)$(basename $(notdir $@)).log_err
	$(hide) $(if $(TO_ROOT),cd $(TO_ROOT);) perl mediatek/build/tools/emigen/$(MTK_PLATFORM)/nandgen.pl \
                     $(CUSTOM_NAND_HDR) \
                     $(MEMORY_DEVICE_XLS) \
                     $(MTK_PLATFORM) \
                     $(PROJECT) \
                     $(MTK_NAND_PAGE_SIZE) \
                     $(DEAL_STDOUT_NANDGEN) \
                     $(MTK_DEPENDENCY_LOG)
	$(call mtk-print-dependency)


CUSTOM_MEMORY_HDR := $(MTK_ROOT_CUSTOM)/$(PROJECT)/preloader/inc/custom_MemoryDevice.h
emigen:
ifneq (,$(filter MT8135 MT6575 MT6577 MT6589 MT8135 MT6582 MT6572 MT6571 MT8127 MT6592 MT6595,$(MTK_PLATFORM)))
ifneq ($(PROJECT), generic)
$(eval $(call mtk-check-dependency,emigen,$(MTK_DEPENDENCY_OUTPUT)))
endif
endif
$(MTK_DEPENDENCY_OUTPUT)/emigen.dep:
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) $(basename $(notdir $@))ing ...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$(basename $(notdir $@)).log
	$(hide) rm -f $(LOG)$(basename $(notdir $@)).log $(LOG)$(basename $(notdir $@)).log_err
	$(hide) $(if $(TO_ROOT),cd $(TO_ROOT);) perl mediatek/build/tools/emigen/$(MTK_PLATFORM)/emigen.pl $(CUSTOM_MEMORY_HDR) $(MEMORY_DEVICE_XLS) $(MTK_PLATFORM) $(PROJECT) $(DEAL_STDOUT_EMIGEN) $(MTK_DEPENDENCY_LOG)
	$(call mtk-print-dependency)


# Memory partition auto-gen related variable initilization
MEM_PARTITION_GENERATOR   := mediatek/build/tools/ptgen/$(MTK_PLATFORM)/ptgen.pl
MEM_PARTITION_TABLE       := mediatek/build/tools/ptgen/$(MTK_PLATFORM)/partition_table_$(MTK_PLATFORM).xls
PARTITION_HEADER_LOCATION := $(MTK_ROOT_CUSTOM)/$(PROJECT)/common
BOARDCONFIG_LOCATION 	  := $(MTK_ROOT_CONFIG)/$(PROJECT)/configs/partition_size.mk
ptgen:
ifneq ($(PROJECT), generic)
ifneq ($(MTK_PTGEN_SUPPORT),no)
$(eval $(call mtk-check-dependency,ptgen,$(MTK_DEPENDENCY_OUTPUT)))
ptgen: $(MTK_DEPENDENCY_OUTPUT)/ptgen.dep $(OTA_SCATTER_FILE)
endif
endif
$(MTK_DEPENDENCY_OUTPUT)/ptgen.dep:
ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
	-@echo [Update] $@: $?
endif
	$(hide) echo $(SHOWTIME) $(basename $(notdir $@))ing ...
	$(hide) echo -e \\t\\t\\t\\b\\b\\b\\bLOG: $(S_LOG)$(basename $(notdir $@)).log
	$(hide) rm -f $(LOG)$(basename $(notdir $@)).log $(LOG)$(basename $(notdir $@)).log_err
	$(hide) $(if $(TO_ROOT),cd $(TO_ROOT);) perl $(MEM_PARTITION_GENERATOR) \
                     MTK_PLATFORM=$(MTK_PLATFORM) \
                     PROJECT=$(PROJECT) \
                     FULL_PROJECT=$(FULL_PROJECT) \
                     MTK_LCA_SUPPORT=$(MTK_LCA_SUPPORT) \
                     MTK_NAND_PAGE_SIZE=$(MTK_NAND_PAGE_SIZE) \
                     MTK_EMMC_SUPPORT=$(MTK_EMMC_SUPPORT) \
                     EMMC_CHIP=$(EMMC_CHIP) \
                     MTK_LDVT_SUPPORT=$(MTK_LDVT_SUPPORT) \
                     TARGET_BUILD_VARIANT=$(TARGET_BUILD_VARIANT) \
                     MTK_EMMC_OTP_SUPPORT=$(MTK_EMMC_SUPPORT_OTP) \
                     $(DEAL_STDOUT_PTGEN) \
                     $(MTK_DEPENDENCY_LOG)
ifneq ($(MTK_GPL_PACKAGE),yes)
	$(hide) mkdir -p $(LOGDIR)/$(PROJECT)
endif
	$(call mtk-print-dependency)

