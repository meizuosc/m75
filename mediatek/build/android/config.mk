include mediatek/build/Makefile
$(call codebase-path)
$(call relative-include,custom.mk)

AEE_FEATURE_POLICY_PATH := $(MTK_PATH_SOURCE)/external/aee/policy
-include $(AEE_FEATURE_POLICY_PATH)/engine.mk

# MediaTek's standard source directories.
SRC_MTK_API_DIR := $(MTK_PATH_SOURCE)/frameworks/api

include $(MTK_PATH_SOURCE)/frameworks-ext/base/pathmap.mk
include $(MTK_PATH_SOURCE)/frameworks/base/mpathmap.mk


PREBUILT_PACKAGE:= $(BUILD_SYSTEM_MTK_EXTENSION)/prebuilt-package.mk
BOARD_CONFIG_DIR ?= $(MTK_ROOT_CONFIG)/$(TARGET_DEVICE)/
ARTIFACT_MODULE :=
ARTIFACT_TARGET := 

ifneq ($(FLAVOR),)
ARTIFACT_DIR := vendor/mediatek/$(TARGET_PRODUCT)[$(FLAVOR)]/artifacts
else
ARTIFACT_DIR := vendor/mediatek/$(TARGET_PRODUCT)/artifacts
endif

ARTIFACT_TARGET_FILE := $(ARTIFACT_DIR)/target.txt
ARTIFACT_COPY_FILE := $(ARTIFACT_DIR)/copy.txt
ARTIFACT_DEFAULT_INSTALLED_HEADERS := 
PARTIAL_BUILD :=

define make-private-dependency
$(eval LOCAL_ADDITIONAL_DEPENDENCIES:=$(call exist-config,$(1)))
endef
define exist-config
$(foreach x, $(1), $(findstring $x,$(call wildcard2,$(BOARD_CONFIG_DIR)/configs/*.mk)))
endef
ifneq (,$(RELEASE_POLICY))
  DO_PUT_ARTIFACTS := yes
endif

#ifneq (,$(call wildcard2,$(ARTIFACT_DIR)/Android.mk))
# full/full.mk is not released for partial source tree, to recognize if we should use artifacts
ifeq (,$(call wildcard2,$(BUILD_SYSTEM_MTK_EXTENSION)/full/config.mk))
ifeq (,$(DO_PUT_ARTIFACTS))
  DO_GET_ARTIFACTS := yes
endif
endif

# define partial build flag
ifeq (,$(call wildcard2,$(BUILD_SYSTEM_MTK_EXTENSION)/full/config.mk.custrel))
PARTIAL_BUILD := true
endif

ifeq ($(PARTIAL_BUILD),true)
NOTICE-HOST-%: ;
NOTICE-TARGET-%: ;
endif

ifneq (,$(DO_PUT_ARTIFACTS))

# 'else' block in policy-path function to deal with 
# "a/b" <---> "a/b/c" case
# "a/b" is configured as a release policy
# "a/b/c" is the current used "LOCAL_PATH"
define policy-path
$(strip \
  $(eval _policy_path := $(call remove-redundant-path-delimeter,$(1))) \
  $(if $(filter $(DIR_WITH_RELEASE_POLICY), $(_policy_path)), \
    $(_policy_path), \
    $(strip \
      $(subst \
        $(word 1, \
               $(filter-out $(_policy_path), \
                 $(foreach item,$(DIR_WITH_RELEASE_POLICY), \
                           $(subst $(item),,$(_policy_path)) \
                  ) \
                ) \
         ) \
         ,, \
         $(_policy_path) \
       ) \
     ) \
   ) \
 )
endef


define remove-redundant-path-delimeter
$(strip \
  $(eval _path := $(subst /, ,$(1)))
  $(eval _full_path :=)
  $(foreach item, $(strip $(_path)), \
    $(eval _full_path += $(item)/) \
   )
  $(patsubst %/,%, \
    $(subst $(space),,$(_full_path)) \
   ) \
 )
endef


#define policy-path
#$(strip $(subst \
#  $(word 1,$(filter-out $(1),$(foreach item,$(DIR_WITH_RELEASE_POLICY),$(subst $(item),,$(1))))),,\
#  $(1) \
#)) \
#endef
define copy-artifact-file
$(2):
	$(hide)if [ -e $(1) ];then \
	mkdir -p $(dir $(2)); \
	cp $(1) $(2);fi
endef

define force-depends-on
$(1): $(2)
endef

define add-install-target
$(shell \
  if [ ! -d $(dir $(1)) ]; then mkdir -p $(dir $(1)); fi; \
  if [ ! -e $(1) ]; then touch $(1); fi; \
  echo $(2) >> $(1) \
 )
endef

ifeq (,$(RELEASE_POLICY))
  RELEASE_POLICY := headquarter
endif

ifneq (1,$(words $(RELEASE_POLICY)))
  $(error Release policy should be one of $(VALID_RELEASE_POLICY), not $(RELEASE_POLICY))
endif

VALID_RELEASE_POLICY := headquarter oversea tier1 other
ifeq (,$(filter $(VALID_RELEASE_POLICY),$(RELEASE_POLICY)))
  $(error Release policy should be one of $(VALID_RELEASE_POLICY), not $(RELEASE_POLICY))
endif

endif # DO_PUT_ARTIFACTS

ifneq (,$(DO_GET_ARTIFACTS))
artifacts_target := $(shell cat $(ARTIFACT_TARGET_FILE) 2>/dev/null)

artifacts_copy := $(shell cat $(ARTIFACT_COPY_FILE) 2>/dev/null)

PV_TOP := external/opencore
PV_INCLUDES := \
        $(PV_TOP)/codecs_v2/audio/gsm_amr/amr_nb/enc/include/ \
        $(PV_TOP)/oscl/oscl/osclbase/src/ \
        $(PV_TOP)/oscl/oscl/config/shared/ \
        $(PV_TOP)/oscl/oscl/config/android/
endif
include $(BUILD_SYSTEM_MTK_EXTENSION)/policy.mk

MTK_INC += $(MTK_PATH_CUSTOM)/cgen/cfgfileinc \
           $(MTK_PATH_CUSTOM)/cgen/cfgdefault \
           $(MTK_PATH_CUSTOM)/cgen/inc \
           $(MTK_PATH_CUSTOM)/hal/inc \
           $(MTK_PATH_CUSTOM)/hal/audioflinger


# Resolution DFO
ifeq ($(strip $(MTK_DFO_RESOLUTION_SUPPORT)),yes)
  ifeq ($(strip $(MTK_INTERNAL)),yes)
    ifeq ($(TARGET_BUILD_VARIANT),eng)
          previous_MTK_PRODUCT_AAPT_CONFIG := $(strip $(sort $(MTK_PRODUCT_AAPT_CONFIG)))
# config your custom rule here
      ifeq ($(strip $(LCM_HEIGHT)),1920)
        ifeq ($(strip $(LCM_WIDTH)),1080)
          MTK_PRODUCT_AAPT_CONFIG += hdpi xhdpi xxhdpi
        endif
      endif
      ifeq ($(strip $(LCM_HEIGHT)),1280)
        ifeq ($(strip $(LCM_WIDTH)),720)
          MTK_PRODUCT_AAPT_CONFIG += hdpi xhdpi
        endif
      endif
      ifeq ($(strip $(LCM_HEIGHT)),960)
        ifeq ($(strip $(LCM_WIDTH)),540)
          MTK_PRODUCT_AAPT_CONFIG += hdpi
        endif
      endif
      ifeq ($(strip $(LCM_HEIGHT)),854)
        ifeq ($(strip $(LCM_WIDTH)),480)
          MTK_PRODUCT_AAPT_CONFIG += hdpi
        endif
      endif
      ifeq ($(strip $(LCM_HEIGHT)),800)
        ifeq ($(strip $(LCM_WIDTH)),480)
          MTK_PRODUCT_AAPT_CONFIG += hdpi
        endif
      endif
#
          MTK_PRODUCT_AAPT_CONFIG := $(strip $(sort $(MTK_PRODUCT_AAPT_CONFIG)))
      ifneq ($(MTK_PRODUCT_AAPT_CONFIG),$(previous_MTK_PRODUCT_AAPT_CONFIG))
          $(info MTK_DFO_RESOLUTION_SUPPORT = $(MTK_DFO_RESOLUTION_SUPPORT))
          $(info MTK_PRODUCT_AAPT_CONFIG = $(MTK_PRODUCT_AAPT_CONFIG) <= $(previous_MTK_PRODUCT_AAPT_CONFIG))
      endif
    else
      export MTK_DFO_RESOLUTION_SUPPORT := no
      $(info MTK_DFO_RESOLUTION_SUPPORT is disabled due to TARGET_BUILD_VARIANT = $(TARGET_BUILD_VARIANT))
    endif
  else
    export MTK_DFO_RESOLUTION_SUPPORT := no
    $(info MTK_DFO_RESOLUTION_SUPPORT is disabled due to MTK_INTERNAL = $(MTK_INTERNAL))
  endif
endif
MTK_RESOURCE_DFO_OVERLAYS_MAPPING := qHD:-888x540 FWVGA:-782x480
MTK_RESOURCE_DFO_OVERLAYS_CHOICE := qHD FWVGA
MTK_RESOURCE_DFO_OVERLAYS_FOLDER := \
	mediatek/custom/$(PROJECT) \
	mediatek/custom/$(MTK_PLATFORM) \
	mediatek/custom/common
MTK_RESOURCE_DFO_OVERLAYS_OUTPUT := $(TARGET_OUT_INTERMEDIATES)/res_dfo
MTK_ASSET_OVERLAYS_OUTPUT := $(TARGET_OUT_INTERMEDIATES)/asset_overlay


MTK_CDEFS := $(call mtk.custom.generate-macros)
COMMON_GLOBAL_CFLAGS += $(MTK_CFLAGS) $(MTK_CDEFS)
COMMON_GLOBAL_CPPFLAGS += $(MTK_CPPFLAGS) $(MTK_CDEFS)
SRC_HEADERS += $(MTK_INC)


# input:
# $(1): LOCAL_PATH
# $(2): LOCAL_RESOURCE_DIR
# $(3): MTK_RESOURCE_DFO_OVERLAYS_CHOICE
# $(4): MTK_RESOURCE_DFO_OVERLAYS_FOLDER for input and search
# $(5): MTK_RESOURCE_DFO_OVERLAYS_MAPPING for mapping table
# $(6): MTK_RESOURCE_DFO_OVERLAYS_OUTPUT for output path
define mtk-resource-overlays-rule
$(eval dst_rule :=)\
$(eval dst_list :=)\
$(eval res_extr :=)\
$(foreach dpi_item,$(strip $(3)),\
	$(eval map_rule := $(foreach map_item,$(strip $(5)),$(if $(filter $(dpi_item),$(word 1,$(subst :, ,$(map_item)))),$(word 2,$(subst :, ,$(map_item))))))\
	$(if $(map_rule),,$(error Unknown dpi for $(dpi_item)))\
	$(foreach res_item,$(strip $(2)),\
		$(eval dst_path := $(strip $(6))/$(res_item))\
		$(foreach src_item,$(strip $(4)),\
			$(eval src_path := $(src_item)/resource_overlay/$(dpi_item)/$(res_item))\
			$(if $(wildcard $(src_path)),\
				$(foreach res_file,$(call find-subdir-assets,$(src_path)),\
					$(eval src_file := $(src_path)/$(res_file))\
					$(eval res_path := $(word 1,$(subst /, ,$(res_file))))\
					$(eval res_name := $(res_path)$(strip $(map_rule)))
					$(eval dst_file := $(dst_path)/$(patsubst $(res_path)/%,$(res_name)/%,$(res_file)))\
					$(if $(filter $(dst_file),$(dst_list)),,\
						$(eval $(call copy-one-file,$(src_file),$(dst_file)))\
						$(eval dst_rule += $(src_file):$(dst_file))\
						$(eval dst_list += $(dst_file))\
					)\
					$(if $(filter $(dst_path),$(res_extr)),,\
						$(eval res_extr += $(dst_path))\
					)\
				)\
			)\
		)\
	)\
)\
$(eval LOCAL_RESOURCE_DIR := $(res_extr) $(LOCAL_RESOURCE_DIR))\
$(eval LOCAL_GENERATED_RESOURCES := $(LOCAL_GENERATED_RESOURCES) $(dst_list))
endef


# input:
# $(1): LOCAL_PATH
# $(2): LOCAL_ASSET_DIR
# $(3): MTK_ASSET_OVERLAYS_FOLDER for input and search
# $(4): MTK_ASSET_OVERLAYS_OUTPUT for output path
define mtk-asset-overlays-rule
$(eval dst_rule :=)\
$(eval dst_list :=)\
$(eval src_item := $(strip $(wildcard $(foreach res_item,$(strip $(3)),$(res_item)/$(strip $(2))))))\
$(eval dst_path := $(strip $(4))/$(strip $(2)))\
$(if $(src_item),\
	$(eval res_list := $(call find-subdir-assets,$(strip $(2))))\
	$(foreach src_path,$(src_item),\
		$(foreach res_file,$(call find-subdir-assets,$(src_path)),\
			$(if $(filter $(res_file),$(res_list)),,\
				$(error [Error][assets_overlay]There is no $(src_path)/$(res_file) in common folder!)\
			)\
			$(eval src_file := $(src_path)/$(res_file))\
			$(eval dst_file := $(dst_path)/$(res_file))\
			$(if $(filter $(dst_file),$(dst_list)),,\
				$(eval $(call copy-one-header,$(src_file),$(dst_file)))\
				$(eval dst_rule += $(src_file):$(dst_file))\
				$(eval dst_list += $(dst_file))\
			)\
		)\
	)\
	$(if $(dst_list),\
		$(foreach res_file,$(res_list),\
			$(eval src_file := $(strip $(2))/$(res_file))\
			$(eval dst_file := $(dst_path)/$(res_file))\
			$(if $(filter $(dst_file),$(dst_list)),,\
				$(eval $(call copy-one-header,$(src_file),$(dst_file)))\
				$(eval dst_rule += $(src_file):$(dst_file))\
				$(eval dst_list += $(dst_file))\
			)\
		)\
	)\
)\
$(if $(dst_list),\
	$(eval LOCAL_ASSET_DIR := $(dst_path))\
	$(eval LOCAL_GENERATED_RESOURCES := $(LOCAL_GENERATED_RESOURCES) $(dst_list))\
)
endef


# $(1): LOCAL_PATH
# $(2): INTERMEDIATES folder 
# output: $(shell perl $(1)/copy_res.pl $(1))
define mtk-theme-copy-rule
$(eval dst_rule :=)\
$(eval dst_list :=)\
$(eval res_item :=)\
$(eval src_item := $(filter-out res assets,$(notdir $(wildcard $(strip $(1))/*))))\
$(foreach x,$(src_item),\
	$(eval src_list := $(notdir $(wildcard $(strip $(1))/$(x)/res/drawable*)))\
	$(if $(src_list),\
		$(eval res_mask += $(x)_%)\
		$(foreach y,$(src_list),\
			$(foreach z,$(notdir $(wildcard $(strip $(1))/$(x)/res/$(y)/*)),\
				$(eval src_file := $(strip $(1))/$(x)/res/$(y)/$(z))\
				$(eval dst_file := $(strip $(2))/res/$(y)/$(x)_$(z))\
				$(if $(filter $(dst_file),$(dst_list)),,\
					$(eval $(call copy-one-header,$(src_file),$(dst_file)))\
					$(eval dst_rule += $(src_file):$(dst_file))\
					$(eval dst_list += $(dst_file))\
				)\
			)\
			$(eval res_item += $(wildcard $(strip $(2))/res/$(y)/$(x)_*))\
		)\
	)\
)\
$(eval res_item := $(strip $(filter-out $(dst_list),$(res_item))))\
$(if $(res_item),\
	$(info [DELETE] $(res_item))\
	$(shell rm -f $(res_item))\
	$(eval .PHONY: $(dst_list))\
)\
$(if $(dst_list),\
	$(eval LOCAL_GENERATED_RESOURCES := $(LOCAL_GENERATED_RESOURCES) $(dst_list))\
)
endef

# $(1): MY_MODULE_PATH
# $(2): src_list 
# $(3): INTERMEDIATES folder 
define mtk-bluetooth-copy-rule
$(eval dst_rule :=)\
$(eval dst_list :=)\
$(eval res_item :=)\
$(eval src_item :=$(2))\
$(foreach x,$(src_item),\
	$(eval src_list := $(notdir $(wildcard $(strip $(1))/$(x)/res/*)))\
	$(if $(src_list),\
		$(foreach y,$(src_list),\
			$(foreach z,$(notdir $(wildcard $(strip $(1))/$(x)/res/$(y)/*)),\
				$(eval src_file := $(strip $(1))/$(x)/res/$(y)/$(z))\
				$(eval dst_file := $(strip $(3))/res/$(y)/$(z))\
				$(if $(filter $(dst_file),$(dst_list)),,\
					$(eval $(call copy-one-header,$(src_file),$(dst_file)))\
					$(eval dst_rule += $(src_file):$(dst_file))\
					$(eval dst_list += $(dst_file))\
				)\
			)\
			$(eval res_item += $(wildcard $(strip $(3))/res/$(y)/$(x)_*))\
		)\
	)\
)\
$(eval res_item := $(strip $(filter-out $(dst_list),$(res_item))))\
$(if $(res_item),\
	$(info [DELETE] $(res_item))\
	$(shell rm -f $(res_item))\
	$(eval .PHONY: $(dst_list))\
)\
$(if $(dst_list),\
	$(eval LOCAL_GENERATED_RESOURCES := $(LOCAL_GENERATED_RESOURCES) $(dst_list))\
)
endef


# $(1): input python script of genmemfiles.py
# $(2): input dir
# $(3): namespace
# $(4): dependency files
# $(5): output target
define mtk-source-genmemfiles-rule-inner
$(5): $(1) $(4)
	@echo "target Generated: $$@"
	$(hide) python $(1) -n $(3) -d $(2)
	$(hide) touch $$@
endef


# $(1): input python script of genmemfiles.py
# $(2): input dir
# $(3): namespace
define mtk-source-genmemfiles-rule
$(eval src_path := $(strip $(2)))\
$(eval dst_path := $(src_path)/src)\
$(eval src_item := $(strip $(3)))\
$(eval src_list := $(filter-out $(dst_path),$(wildcard $(src_path)/*)))\
$(eval dst_list := $(dst_path)/memfilesource_$(src_item).h $(foreach x,$(subst .,_,$(notdir $(src_list))),$(dst_path)/memfiles/_$(src_item)__$(x).h))\
$(eval res_list := $(wildcard $(dst_path)/memfilesource_$(src_item).h) $(wildcard $(dst_path)/memfiles/_$(src_item)__*.h))\
$(eval res_item := $(strip $(filter-out $(dst_list),$(res_list))))\
$(if $(res_item),\
	$(info [DELETE] $(res_item))\
	$(shell rm -rf $(res_item))\
	$(eval .PHONY: $(dst_path)/memfilesource_$(src_item).cpp)\
)\
$(eval $(call mtk-touch-one-file,$(dst_path)/memfilesource_$(src_item).cpp,$(dst_list)))\
$(eval $(call mtk-source-genmemfiles-rule-inner,$(1),$(src_path),$(src_item),$(src_list),$(dst_path)/memfilesource_$(src_item).cpp))\
$(if $(dst_list),\
	$(eval LOCAL_GENERATED_SOURCES := $(LOCAL_GENERATED_SOURCES) $(dst_list))\
)
endef


#.PHONY: mtk-config-files
#*: mtk-config-folder
#mtk-config-folder: mtk-config-files
#mtk-config-files := $(strip $(call mtk.config.generate-rules,mtk-config-files))

dump-comp-build-info:
	@echo Dump componenet level build info.

MTK_INTERNAL_PLATFORM_API_FILE := $(TARGET_OUT_COMMON_INTERMEDIATES)/PACKAGING/mediatek_public_api.txt
FRAMEWORK_RES_ID_API_FILE := $(TARGET_OUT_COMMON_INTERMEDIATES)/PACKAGING/framework_res_id_api.txt
ifeq ($(strip $(BUILD_MTK_API_MONITOR)), yes)
MTK_INTERNAL_MONITORING_API_FILE := $(TARGET_OUT_COMMON_INTERMEDIATES)/PACKAGING/mediatek_internal_api.txt
endif
JPE_TOOL := $(HOST_OUT_JAVA_LIBRARIES)/jpe_tool.jar
MKUBIFS := $(HOST_OUT_EXECUTABLES)/mkfs_ubifs$(HOST_EXECUTABLE_SUFFIX)
UBINIZE := $(HOST_OUT_EXECUTABLES)/ubinize$(HOST_EXECUTABLE_SUFFIX)

# for multi-linux check user space header & kernel header sync mechanism
ifndef CHECK_USER_SPACE_HEADER
     CHECK_USER_SPACE_HEADER := no
endif
ifeq ($(strip $(CHECK_USER_SPACE_HEADER)), yes)
include mediatek/build/android/bionic-linux-header-list.mk
#_userspace_header 
#_kernel_header
# _header_file_mapping (_userspace_header:_kernel_header)
#$(info $(_header_file_mapping))
$(foreach item, $(_header_file_mapping),\
         $(eval _userspace_header := $(wildcard $(firstword $(subst :, ,$(item)))))\
         $(eval _kernel_header:=$(wildcard $(lastword $(subst :, ,$(item)))))\
                 $(if $(_userspace_header),\
                     $(if $(_kernel_header),\
                           $(if $(shell diff $(_userspace_header) $(_kernel_header)),\
                                   $(eval $(warning *** [Bionic-Linux-Warning] "$(_kernel_header)" and "$(_userspace_header)" \
	                                        content not match, please make sure they are identical \
			                        or remove the mapping in "mediatek/build/android/bionic-linux-header-list.mk"))),\
                       $(eval $(warning *** [Bionic-Linux-Warning] "$(lastword $(subst :, ,$(item)))" does not exists, make sure "$(lastword $(subst :, ,$(item)))" exists in P4\
                                            or remove "$(item)" mapping from "mediatek/build/android/bionic-linux-header-list.mk"))),\
                  $(eval $(warning *** [Bionic-Linux-Warning] "$(firstword $(subst :, ,$(item)))" does not exists, make sure "$(firstword $(subst :, ,$(item)))" exists in \
                                       P4 or remove "$(item)" mapping from "mediatek/build/android/bionic-linux-header-list.mk"))))
   
endif
