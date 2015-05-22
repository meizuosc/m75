LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := mtk-kpd.kcm
LOCAL_MODULE_TAGS := user
include $(BUILD_KEY_CHAR_MAP)

##################################
$(call config-custom-folder,modem:modem)

##### SET MD OUTPUT POSITION #####
MTK_MODEM_OUT_ETC := $(TARGET_OUT_ETC)
ifeq ($(strip $(MTK_CIP_SUPPORT)),yes)
## Only OP07 and OP08 use custom partition, OP03 and OP06 needn't
  ifneq ($(findstring OP07_, $(OPTR_SPEC_SEG_DEF)),)
    MTK_MODEM_OUT_ETC := $(TARGET_CUSTOM_OUT)/etc
  endif
  ifneq ($(findstring OP08_, $(OPTR_SPEC_SEG_DEF)),)
    MTK_MODEM_OUT_ETC := $(TARGET_CUSTOM_OUT)/etc
  endif
endif

MTK_MODEM_LOCAL_PATH := $(LOCAL_PATH)
MTK_MODEM_INSTALLED_MODULES :=

MTK_MODEM_SRC_FIRMWARE := modem*.img
ifeq ($(strip $(MTK_MDLOGGER_SUPPORT)),yes)
  MTK_MODEM_SRC_FIRMWARE += catcher_filter*.bin
endif

  MTK_MODEM_MAP_VALUE_TO_X := 1 2
  MTK_MODEM_MAP_X_1_TO_YY := 2g wg tg lwg ltg sglte
  MTK_MODEM_MAP_X_2_TO_YY := 2g wg tg lwg ltg sglte
  MTK_MODEM_SRC_FIRMWARE_1 := dsp*.bin
  MTK_MODEM_SRC_FIRMWARE_2 := dsp*.bin


define mtk-install-modem
include $$(CLEAR_VARS)
LOCAL_MODULE := $$(notdir $(1))
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(2)
LOCAL_SRC_FILES := $(1)
include $$(BUILD_PREBUILT)
MTK_MODEM_INSTALLED_MODULES += $$(LOCAL_INSTALLED_MODULE)
endef

##### INSTALL MODEM FIRMWARE #####
$(foreach x,$(MTK_MODEM_MAP_VALUE_TO_X),\
	$(if $(filter yes,$(strip $(MTK_ENABLE_MD$(x)))),\
		$(foreach yy,$(MTK_MODEM_MAP_X_$(x)_TO_YY),\
			$(if $(wildcard $(MTK_MODEM_LOCAL_PATH)/modem/modem_$(x)_$(yy)_n.img),\
				$(foreach src,$(MTK_MODEM_SRC_FIRMWARE) $(MTK_MODEM_SRC_FIRMWARE_$(x)),\
					$(eval des := $(subst *,_$(x)_$(yy)_n,$(src)))\
					$(eval $(call mtk-install-modem,modem/$(des),$(MTK_MODEM_OUT_ETC)/firmware))\
				)\
			)\
		)\
	)\
)
$(foreach des,$(MTK_MODEM_SRC_FIRMWARE_0),\
	$(eval $(call mtk-install-modem,modem/$(des),$(MTK_MODEM_OUT_ETC)/firmware))\
)

########INSTALL MODEM_DATABASE########
ifeq ($(strip $(MTK_INCLUDE_MODEM_DB_IN_IMAGE)),yes)
ifeq ($(filter generic banyan_addon banyan_addon_x86,$(PROJECT)),)
$(foreach x,$(MTK_MODEM_MAP_VALUE_TO_X),\
	$(if $(filter yes,$(strip $(MTK_ENABLE_MD$(x)))),\
		$(foreach yy,$(MTK_MODEM_MAP_X_$(x)_TO_YY),\
			$(if $(wildcard $(MTK_MODEM_LOCAL_PATH)/modem/BPLGUInfoCustomAppSrcP_*_$(x)_$(yy)_n),\
				$(eval MTK_MODEM_SRC_MDDB := $(wildcard $(MTK_MODEM_LOCAL_PATH)/modem/BPLGUInfoCustomAppSrcP_*_$(x)_$(yy)_n))\
			,\
				$(eval MTK_MODEM_SRC_MDDB := $(wildcard $(MTK_MODEM_LOCAL_PATH)/modem/BPLGUInfoCustomApp_*_$(x)_$(yy)_n))\
			)\
			$(foreach src,$(MTK_MODEM_SRC_MDDB),\
				$(eval des := $(notdir $(src)))\
				$(eval $(call mtk-install-modem,modem/$(des),$(MTK_MODEM_OUT_ETC)/mddb))\
			)\
		)\
	)\
)
$(foreach des,$(MTK_MODEM_SRC_EXTMDDB_0),\
	$(eval $(call mtk-install-modem,modem/$(des),$(MTK_MODEM_OUT_ETC)/extmddb))\
)
endif
endif
##################################
$(info MTK_MODEM_INSTALLED_MODULES = $(MTK_MODEM_INSTALLED_MODULES))

