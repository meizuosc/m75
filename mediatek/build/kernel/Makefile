include $(srctree)/../mediatek/build/Makefile
ifneq ($(KBUILD_SRC),)
$(call codebase-path,kernel,$(srctree)/..)
else
$(call codebase-path,kernel)
endif
#TO-DO 
#$(call relative-include,common.mk)
include $(srctree)/../mediatek/build/kernel/common.mk
MODULE_PREFIX_KERNEL := CUSTOM_KERNEL_
MODULE_PREFIX_HAL    := CUSTOM_HAL_
machine-y       := $(call lc,$(MTK_PLATFORM))
ifeq ($(strip $(KBUILD_OUTPUT_SUPPORT)),yes)
MACHINE         := mediatek/platform/$(call lc,$(MTK_PLATFORM))/kernel/core/
machdirs        := mediatek/platform/$(call lc,$(MTK_PLATFORM))/kernel/core/
else
MACHINE         := $(MTK_PATH_PLATFORM)/core/
machdirs        := $(MTK_PATH_PLATFORM)/core/
endif
platdirs        :=

ifeq ($(strip $(KBUILD_OUTPUT_SUPPORT)),yes)
drivers-y       += mediatek/kernel/
drivers-y       += mediatek/custom/out/kernel/
drivers-y       += mediatek/platform/$(call lc,$(MTK_PLATFORM))/kernel/drivers/
else
drivers-y       += $(MTK_PATH_PLATFORM)/drivers/
drivers-y       += $(MTK_PATH_SOURCE)/
drivers-y       += $(MTK_PATH_CUSTOM)/
endif

LINUXINCLUDE    += -I$(MTK_PATH_PLATFORM)/core/include/
LINUXINCLUDE    += -I$(MTK_PATH_SOURCE)/include/
KBUILD_CFLAGS   += $(call mtk.custom.generate-macros)

ifneq (,$(MTK_MODEM_SUPPORT))
KBUILD_CPPFLAGS += -D$(call uc,$(MTK_MODEM_SUPPORT))
endif

ifeq ($(TARGET_BUILD_VARIANT),user)
 KBUILD_CFLAGS += -DUSER_BUILD_KERNEL
endif

ifeq (,$(M))
clean: mtkclean
mtkclean:
	@echo "clean mediatek kernel source tree"
	@find $(MTK_PATH_PLATFORM) $(MTK_PATH_SOURCE) $(MTK_PATH_CUSTOM) $(RCS_FIND_IGNORE) \
	\( -name '*.[oas]' -o -name '*.ko' -o -name '.*.cmd' \
	-o -name '.*.d' -o -name '.*.tmp' -o -name '*.mod.c' \
	-o -name '*.symtypes' -o -name 'modules.order' \
	-o -name 'Module.markers' -o -name '.tmp_*.o.*' \
	-o -name '*.gcno' \) -type f -print | grep -v "/prebuilt/" | xargs rm -f
	find $(MTK_ROOT_GEN_CONFIG) -name kconfig -type f -print | xargs rm -f
	find $(PWD)/out/include/mach -name *.h -type f -print | xargs rm -f
endif

.PHONY: custgen
custom-files-kernel := $(strip $(call mtk.custom.generate-rules,custgen,kernel))
custom-files-hal    := $(strip $(call mtk.custom.generate-rules,custgen,hal))
_module_inc_kernel  :=
_module_inc_hal     :=
$(foreach i,\
  $(foreach t,$(dir $(patsubst $(MTK_ROOT_CUSTOM_OUT)/kernel/%,%,$(custom-files-kernel))), \
    $(eval _ := $(subst /, ,$(t))) \
    $(if $(filter 1,$(words $(_))),$(t),) \
    $(if $(filter 2,$(words $(_))),\
      $(if $(filter src inc,$(word 2,$(_))),$(t), \
        $(if $(filter $($(MODULE_PREFIX_KERNEL)$(call uc,$(word 1,$(_)))),$(word 2,$(_))),$(t),) \
  ),)) \
 ,$(eval _module_inc_kernel += $(filter-out $(_module_inc_kernel),$(i))) \
)

$(foreach i,\
  $(foreach t,$(dir $(patsubst $(MTK_ROOT_CUSTOM_OUT)/hal/%,%,$(custom-files-hal))), \
    $(eval _ := $(subst /, ,$(t))) \
    $(if $(filter 1,$(words $(_))),$(t),) \
    $(if $(filter 2,$(words $(_))),\
      $(if $(filter src inc,$(word 2,$(_))),$(t), \
        $(if $(filter $($(MODULE_PREFIX_HAL)$(call uc,$(word 1,$(_)))),$(word 2,$(_))),$(t),) \
  ),)) \
 ,$(eval _module_inc_hal += $(filter-out $(_module_inc_hal),$(i))) \
)
LINUXINCLUDE += $(strip $(addprefix -I$(MTK_ROOT_CUSTOM_OUT)/kernel/,$(_module_inc_kernel)))
LINUXINCLUDE += $(strip $(addprefix -I$(MTK_ROOT_CUSTOM_OUT)/hal/,$(_module_inc_hal)))
# add include path for ptgen output
#LINUXINCLUDE += -I$(MTK_ROOT_CUSTOM_OUT)/common
LINUXINCLUDE += -I$(MTK_ROOT_OUT)/PTGEN/common
LINUXINCLUDE += -I$(MTK_ROOT_OUT)/PTGEN/kernel
# for nandgen
LINUXINCLUDE += -I$(MTK_ROOT_OUT)/NANDGEN/common

# use custgen instead
mediatek-configs: $(MTK_ROOT_GEN_CONFIG)/kconfig
	@echo "[COPY] $< --> $@"
	@cp $< $@
	@echo "[COPY] $< --> .config"
	@cp $< .config
ifeq (mediatek-configs,$(MAKECMDGOALS))
include/config/auto.conf: ;
endif

ifeq ($(MTK_GPL_PACKAGE),yes)
ifndef ARCH_MTK_PLATFORM
  $(error ARCH_MTK_PLATFORM is not defined)
endif
include $(srctree)/../mediatek/build/libs/pack_dep_gen.mk
# override
MTK_DEPENDENCY_LOG :=
mtk-print-dependency :=
# pregen need by kernel
include $(srctree)/../mediatek/build/libs/pregen.mk
$(call mtk.projectconfig.generate-auto-rules)
mtk-config-files := $(strip $(call mtk.config.generate-rules,custgen,autoconfig))
#mtk-custom-files-common := $(strip $(call mtk.custom.generate-rules,custgen,common))
ifeq ($(LEGACY_DFO_GEN), yes)
custgen: $(if $(objtree),$(objtree)/)include/mach/dfo_boot.h
custgen: $(if $(objtree),$(objtree)/)include/mach/dfo_boot_default.h
endif
prepare: custgen
$(KCONFIG_CONFIG): mediatek-configs
$(MTK_ROOT_GEN_CONFIG)/kconfig: custgen
# custgen must be after pregen
custgen: drvgen nandgen ptgen
	+ make $(if $(TO_ROOT),-C $(TO_ROOT)) MTK_CUSTGEN_CUSTOM_FILES="kernel common" MTK_CUSTGEN_ERROR=no -f mediatek/build/custgen.mk $(DEAL_STDOUT_CUSTGEN)

endif
