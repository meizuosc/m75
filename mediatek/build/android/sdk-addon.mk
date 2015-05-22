# include MTK SDK toolset version file
include $(MTK_PATH_SOURCE)/frameworks/banyan/TOOLSET_VERSION
include $(MTK_PATH_SOURCE)/frameworks/banyan/EMULATOR_VERSION

ifneq ($(filter banyan_addon banyan_addon_x86,$(TARGET_PRODUCT)),)

ifeq ($(strip $(MTK_BSP_PACKAGE)),no)
# make dependency between banyan_addon/sdk_addon and checkmtkapi
sdk_addon: checkmtkapi mtk-clean-temp
endif

mtk-clean-temp:
	@rm -rf $(TARGET_PRODUCT_OUT_ROOT)/mediatek

endif


# MediaTek SDK PlusOpen
SDK_PLUSOPEN_NAME := mediatek_plusopen_sdk
SDK_PLUSOPEN_COPY_DIRS := vendor/sdk
SDK_PLUSOPEN_COPY_FILES := mediatek/frameworks/banyan/README.txt:libs/README.txt
SDK_PLUSOPEN_COPY_MODULES :=
SDK_PLUSOPEN_DOC_MODULES := mediatek-sdk
SDK_PLUSOPEN_COPY_HOST_OUT := framework/mediatek-android.jar:libs/mediatek-android.jar
SDK_PLUSOPEN_COPY_HOST_OUT += framework/mediatek-compatibility.jar:libs/mediatek-compatibility.jar

ifneq ($(SDK_PLUSOPEN_NAME),)

SDK_PLUSOPEN_dir_leaf := $(SDK_PLUSOPEN_NAME)_api-$(strip $(last_released_sdk_version)).$(strip $(last_released_mtk_sdk_version)).$(strip $(MTK_SDK_EMULATOR_VERSION))
SDK_PLUSOPEN_full_target := $(HOST_OUT_SDK_ADDON)/$(SDK_PLUSOPEN_dir_leaf).zip
SDK_PLUSOPEN_staging := $(HOST_OUT_INTERMEDIATES)/SDK_ADDON/$(SDK_PLUSOPEN_NAME)_intermediates/$(SDK_PLUSOPEN_dir_leaf)
SDK_PLUSOPEN_deps :=

SDK_PLUSOPEN_files_to_copy := $(foreach file,$(filter-out %.mk %.zip,$(shell find $(SDK_PLUSOPEN_COPY_DIRS) -type f)),$(file):$(subst $(SDK_PLUSOPEN_COPY_DIRS)/,,$(file)))

SDK_PLUSOPEN_files_to_copy += $(SDK_PLUSOPEN_COPY_FILES)

ifneq ($(strip $(SDK_PLUSOPEN_COPY_MODULES)),)
$(foreach cf,$(SDK_PLUSOPEN_COPY_MODULES), \
  $(eval _src := $(call module-stubs-files,$(call word-colon,1,$(cf)))) \
  $(eval $(call stub-addon-jar,$(_src))) \
  $(eval _src := $(call stub-addon-jar-file,$(_src))) \
  $(if $(_src),,$(eval $(error Unknown or unlinkable module: $(call word-colon,1,$(cf)). Requested by $(INTERNAL_PRODUCT)))) \
  $(eval _dest := $(call word-colon,2,$(cf))) \
  $(eval SDK_PLUSOPEN_files_to_copy += $(_src):$(_dest)) \
 )
endif

ifneq ($(strip $(SDK_PLUSOPEN_COPY_HOST_OUT)),)
$(foreach cf,$(SDK_PLUSOPEN_COPY_HOST_OUT), \
  $(eval _src := $(call append-path,$(HOST_OUT),$(call word-colon,1,$(cf)))) \
  $(eval _dest := $(call word-colon,2,$(cf))) \
  $(eval SDK_PLUSOPEN_files_to_copy += $(_src):$(_dest)) \
 )
endif

$(foreach cf,$(SDK_PLUSOPEN_files_to_copy), \
  $(eval _src := $(call word-colon,1,$(cf))) \
  $(eval _dest := $(call append-path,$(SDK_PLUSOPEN_staging),$(call word-colon,2,$(cf)))) \
  $(eval $(call copy-one-file,$(_src),$(_dest))) \
  $(eval SDK_PLUSOPEN_deps += $(_dest)) \
 )

SDK_PLUSOPEN_doc_modules := $(strip $(SDK_PLUSOPEN_DOC_MODULES))
SDK_PLUSOPEN_deps += $(foreach dm, $(SDK_PLUSOPEN_doc_modules), $(call doc-timestamp-for, $(dm)))
$(SDK_PLUSOPEN_full_target): PRIVATE_DOCS_DIRS := $(addprefix $(OUT_DOCS)/, $(SDK_PLUSOPEN_doc_modules))

$(SDK_PLUSOPEN_full_target): PRIVATE_STAGING_DIR := $(SDK_PLUSOPEN_staging)

$(SDK_PLUSOPEN_full_target): $(SDK_PLUSOPEN_deps) | $(ACP)
	@echo Packaging SDK Addon: $@
	$(hide) rm -f $@
	$(hide) mkdir -p $(PRIVATE_STAGING_DIR)/docs
	$(hide) for d in $(PRIVATE_DOCS_DIRS); do \
	    $(ACP) -r $$d $(PRIVATE_STAGING_DIR)/docs ;\
	  done
	$(hide) mkdir -p $(dir $@)
	$(hide) ( F=$$(pwd)/$@ ; cd $(PRIVATE_STAGING_DIR) && zip -rq $$F * )

SDK_PLUSOPEN_toolset_dir_leaf := $(SDK_PLUSOPEN_NAME)_toolset-$(strip $(last_released_sdk_version)).$(strip $(last_released_mtk_sdk_version)).$(strip $(MTK_SDK_TOOLSET_VERSION))
SDK_PLUSOPEN_toolset_full_target := $(HOST_OUT_SDK_ADDON)/$(SDK_PLUSOPEN_toolset_dir_leaf).zip
$(SDK_PLUSOPEN_toolset_full_target): vendor/sdk/mtk_sdk_toolset-$(strip $(MTK_SDK_TOOLSET_VERSION)).zip
	@echo Packaging SDK Addon: $@
	$(hide) rm -f $@
	$(hide) mkdir -p $(dir $@)
	$(hide) cp -f $< $@

.PHONY: banyan_opensdk
banyan_opensdk: $(SDK_PLUSOPEN_full_target) $(SDK_PLUSOPEN_toolset_full_target)

endif
