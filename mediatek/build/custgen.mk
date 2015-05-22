MAKEFLAGS += -rR
include mediatek/build/Makefile
$(call codebase-path)
all: $(MTK_ROOT_CONFIG_OUT)/ProjectConfig.mk
all: mtk-config-files mtk-custom-files
	@echo "done"

ifneq ($(MAKECMDGOALS),$(MTK_ROOT_CONFIG_OUT)/ProjectConfig.mk)
  ifdef MTK_CUSTGEN_CUSTOM_FILES
mtk-custom-files := $(strip $(call mtk.custom.generate-rules,mtk-custom-files,$(MTK_CUSTGEN_CUSTOM_FILES)))
  else
mtk-custom-files := $(strip $(call mtk.custom.generate-rules,mtk-custom-files))
  endif
  ifdef MTK_CUSTGEN_CONFIG_FILES
mtk-config-files := $(strip $(call mtk.config.generate-rules,mtk-config-files,$(MTK_CUSTGEN_CONFIG_FILES)))
  else
mtk-config-files := $(strip $(call mtk.config.generate-rules,mtk-config-files))
  endif
endif

hide := @
$(call mtk.projectconfig.generate-auto-rules)
ifneq ($(MTK_CUSTGEN_ERROR),no)
$(error custgen.mk)
endif
