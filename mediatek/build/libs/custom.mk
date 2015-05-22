# custom.mk - add supports for custom folder generation

#internal functions supporting custom folder generation
define .mtk.custom.delete-rule
$(1): $(2).delete
$(2).delete: 
	@echo "[DELETE] $(2)"
	@rm -rf $(2)
endef

# Only show message in custgen
define .mtk.custom.generate-rule
$(1): $(2)
$(2): $(3)
	@$(if $(filter 0,$(MAKELEVEL)),,echo "[CUSTOM] copy $(3)")
	@$(if $(filter 0,$(MAKELEVEL)),,echo "           to $(2)")
	@mkdir -p $(dir $(2))
	@cp -f $(3) $(2)
endef

define .mtk.custom.split-project
$(if $(filter-out .,$(1)),$(subst /,.,$(1)) $(call .mtk.custom.split-project,$(patsubst %/,%,$(dir $(1)))),)
endef

define .mtk.custom.map-module
$(strip \
  $(eval _custom._ := $(call path.split,$1)) \
  $(eval _custom.d := $(call path.split,$(firstword $(_custom._)))) \
  $(eval _custom.f := $(lastword $(_custom._))) \
  $(eval _custom.n := CUSTOM_$(call uc,$(subst /,_,$(firstword $(_custom.d))))) \
  $(eval _custom.v := $(lastword $(_custom.d))) \
  $(if $(filter $($(_custom.n)),$(_custom.v)),
    $1 $(firstword $(_custom.d))/$(_custom.f),
    $(if $(call seq,undefined,$(origin $(_custom.n))),$1 $1,\
      $(if $(filter inc,$(_custom.v)),$1 $1,\
        $(if $(filter src,$(_custom.v)),$1 $(firstword $(_custom.d))/$(_custom.f),)\
      )
  )) \
)
endef

define .mtk.custom.generate-folder-list
$(strip $(eval _mtk_project_ :=$(subst ],,$(subst [, ,$(FULL_PROJECT)))) \
$(eval _flvlist_     := $(strip $(subst +, ,$(word 2,$(_mtk_project_))))) \
$(eval _prjlist_     := $(call .mtk.custom.split-project,$(subst .,/,$(word 1,$(_mtk_project_))))) \
$(eval _fplist_     := $(foreach p,$(_prjlist_),$(foreach f,$(_flvlist_),$(p)[$(f)]))) \
$(foreach d,$(_fplist_),\
    $(if $(call wildcard2,$(addprefix $(MTK_ROOT_CUSTOM)/,$(d))),$(error $(d):Flavor project can not be used under $(MTK_ROOT_CUSTOM)),)) \
$(eval _cust_list_   := $(if $(CUSTOMER),$(CUSTOMER))) \
$(_prjlist_) $(_cust_list_) $(call lc,$(MTK_PLATFORM)) common)
endef

define .mtk.config.generate-folder-list
$(strip $(eval _mtk_project_ :=$(subst ],,$(subst [, ,$(FULL_PROJECT)))) \
$(eval _flvlist_     := $(strip $(subst +, ,$(word 2,$(_mtk_project_))))) \
$(eval _prjlist_     := $(call .mtk.custom.split-project,$(subst .,/,$(word 1,$(_mtk_project_))))) \
$(eval _fp_list_     := $(foreach p,$(_prjlist_),$(foreach f,$(_flvlist_),$(p)[$(f)])) $(_prjlist_)) \
$(eval _cust_list_   := $(if $(CUSTOMER),$(CUSTOMER))) \
$(_fp_list_) $(_cust_list_) $(call lc,$(MTK_PLATFORM)) common)
endef

define .if-cfg-on
$(if $(filter-out NO NONE FALSE,$(call uc,$(strip $($(1))))),$(2),$(3))
endef
define mtk.custom.generate-macros
$(strip $(foreach t,$(AUTO_ADD_GLOBAL_DEFINE_BY_NAME),$(call .if-cfg-on,$(t),-D$(call uc,$(t))))
$(foreach t,$(AUTO_ADD_GLOBAL_DEFINE_BY_VALUE),$(call .if-cfg-on,$(t),$(foreach v, $(call uc,$($(t))),-D$(v))))
$(foreach t,$(AUTO_ADD_GLOBAL_DEFINE_BY_NAME_VALUE),$(call .if-cfg-on,$(t),-D$(call uc,$(t))=\"$($(t))\")))
endef

# * mtk.custom.generate-rules - generate rules for customization folder generation
# example usage
# a. associate custom files with target "all"
#    $(call mtk.custom.generate-rules,all)
# b. associate custom files under */kernel and */uboot with target "vmlinux"
#    $(call mtk.custom.generate-rules,vmlinux,kernel uboot)
# c. associate auto-generated files (only support files generated under $(MTK_ROOT_CUSTOM)/$(MTK_PROJECT)
#    $(call mtk.custom.generate-rules,depend,preloader,$(MTK_ROOT_CUSTOM)/$(MTK_PROJECT)/preloader/inc/custom_emi.h
define mtk.custom.generate-rules
$(if $(MTK_ROOT_CUSTOM),$(strip \
  $(eval _custflist_ :=) $(eval _custfmap_  :=) $(eval _custfgen :=) \
  $(foreach d,$(addprefix $(MTK_ROOT_CUSTOM)/,$(MTK_CUSTOM_FOLDERS)),\
    $(eval _dirs := $(call wildcard2,$(if $(2),$(addprefix $(d)/,$(2)),$(d)))) \
    $(if $(_dirs),\
      $(eval _files := $(filter-out $(_custflist_), \
        $(patsubst $(d)/%,%,$(shell find -L $(_dirs) -type d -name ".svn" -prune -o \( ! -name .\* \) -type f -print)))) \
      $(foreach f,$(_files), \
        $(eval _      := $(call .mtk.custom.map-module,$f)) \
        $(if $_, \
          $(eval _src_       := $(firstword $_)) \
          $(eval _des_       := $(lastword $_)) \
          $(eval _des_       := $(if $(CUSTOM_MODEM),$(subst $(CUSTOM_MODEM)/,,$(_des_)),$(_des_))) \
          $(eval _custflist_ += $(_src_)) \
          $(eval _custfmap_  += $(MTK_ROOT_CUSTOM_OUT)/$(_des_):$(d)/$(_src_)) \
        ,) \
      ) \
    ,) \
  ) \
  $(eval # TODO pre-generated files should be enforced directly by architecture ) \
  $(if $(3),$(foreach f,$(filter-out $(_custflist_),$(3)), \
    $(eval _g_       := $(MTK_ROOT_CUSTOM_OUT)/$(patsubst $(MTK_ROOT_CUSTOM)/$(MTK_PROJECT)/%,%,$(f))) \
    $(if $(filter $(addprefix $(MTK_ROOT_CUSTOM_OUT)/,$(_custflist_)),$(_g_)),, \
      $(eval _custfgen  += $(_g_)) $(eval _custflist_ += $(f)) $(eval _custfmap_  += $(_g_):$(f)) \
  )),) \
  $(if $(call wildcard2,$(MTK_ROOT_OUT)/DRVGEN), \
    $(eval _files := $(filter-out $(_custflist_), \
      $(patsubst $(MTK_ROOT_OUT)/DRVGEN/%,%,$(shell find -L $(MTK_ROOT_OUT)/DRVGEN -type d -name ".svn" -prune -o \( ! -name .\* \) -type f -print)))) \
    $(foreach f,$(_files), \
      $(eval _custfmap_ += $(MTK_ROOT_CUSTOM_OUT)/kernel/dct/$(f):$(MTK_ROOT_OUT)/DRVGEN/$(f)) \
    ) \
  ,) \
  $(if $(call wildcard2,$(MTK_ROOT_OUT)/PTGEN/lk/partition.c), \
    $(eval _custfmap_ += $(MTK_ROOT_CUSTOM_OUT)/lk/partition.c:$(MTK_ROOT_OUT)/PTGEN/lk/partition.c)\
  ) \
  $(if $(call wildcard2,$(MTK_ROOT_CUSTOM_OUT)),\
    $(foreach f,$(filter-out $(_custfgen) $(foreach f,$(_custfmap_),$(word 1,$(subst :, ,$f))) $(MTK_ROOT_CUSTOM_OUT)/lk/logo/boot_logo $(MTK_ROOT_CUSTOM_OUT)/sepolicy/keys.conf,\
      $(shell find $(if $(2),$(addprefix $(MTK_ROOT_CUSTOM_OUT)/,$(2)),$(MTK_ROOT_CUSTOM_OUT)) \
        -type d -name ".svn" -prune -o \
        ! \( -name '*.[oasP]' -o -name '*.ko' -o -name '.*' -o -name '*.mod.c' -o -name '*.gcno' \
          -o -name 'modules.order' \) -type f -print 2> /dev/null)),\
      $(eval $(call .mtk.custom.delete-rule,$(1),$(f)))) \
  ,) \
  $(foreach f,$(_custfmap_),\
    $(eval $(call .mtk.custom.generate-rule,$(1),$(word 1,$(subst :, ,$(f))),$(word 2,$(subst :, ,$(f))))) \
    $(word 1,$(subst :, ,$(f))) \
   ) \
  $(eval # dump custgen dependency info. ) \
  $(if $(filter true, $(DUMP_COMP_BUILD_INFO)), \
    $(eval _depinfo := $(strip $(_custfmap_))) \
    $(eval _depfile := $(MTK_ROOT_CUSTOM_OUT)/custgen.P) \
    $(shell if [ ! -d $(dir $(_depfile)) ]; then mkdir -p $(dir $(_depfile)); fi) \
    $(call dump-words-to-file.mtk, $(strip $(_depinfo)), $(_depfile)) \
  ,) \
),)
endef

# Load Project Configuration
#   project configuration is defined in mediatek/config/<project>/ProjectConfig.mk
#   they will be include in reversed MTK_CUSTOM_FOLDER order, e.g.,
#     common/ProjectConfig.mk mtxxxx/ProjectConfig.mk prj/ProjectConfig.mk prj[flv]/ProjectConfig.mk
# pre-do include and export Project configurations for initializing some basic variables (MTK_PLATFORM)
MTK_CONFIG_FOLDERS  := $(call .mtk.config.generate-folder-list)
MTK_PROJECT_CONFIGS := $(call wildcard2,$(foreach c,$(call reverse,$(addsuffix /ProjectConfig.mk,\
    $(addprefix ../../config/,$(MTK_CONFIG_FOLDERS)))),$(call relative-path,$(c))))
$(foreach p,$(MTK_PROJECT_CONFIGS),$(eval include $p))

# update custom folder with platform
# include and export Project configurations after updating MTK_CONFIG_FOLDERS
MTK_CONFIG_FOLDERS  := $(call .mtk.config.generate-folder-list)
MTK_PROJECT_CONFIGS := $(call wildcard2,$(foreach c,$(call reverse,$(addsuffix /ProjectConfig.mk,\
    $(addprefix ../../config/,$(MTK_CONFIG_FOLDERS)))),$(call relative-path,$(c))))
$(foreach p,$(MTK_PROJECT_CONFIGS),$(eval include $p))

MTK_CUSTOM_FOLDERS  := $(call .mtk.custom.generate-folder-list)
# export all project defined variables
# it is necessary to have MTK_PROJECT here, to prevent empty project file
export_var :=
$(foreach p,$(MTK_PROJECT_CONFIGS),$(foreach f,$(strip $(shell cat $p | \
    grep -v -P "^\s*#" | sed 's/\s*=\s*.*//g')),$(eval export_var+=$f)))
export_var+= MTK_PROJECT
$(foreach i,$(export_var),$(eval $i=$(strip $($i))))
$(eval export $(export_var))

# MTK_CUSTOM_FOLDER   : decomposed project name list. for example, for rp.v2[lca+multitouch] it will be
#                       rp.v2[lca] rp.v2[multitouch] rp[lca] rp[multitouch] rp.v2 rp mtxxxx common
# MTK_PROJECT_CONFIGS : pathmap of all used configurations
export MTK_CUSTOM_FOLDERS MTK_CONFIG_FOLDERS MTK_PROJECT_CONFIGS

# get modem feature option
ifneq ($(strip $(CUSTOM_MODEM)),)
  MTK_MODEM_FILE :=
  MTK_MODEM_FEATURE_MD :=
  MTK_MODEM_FEATURE_AP :=
  $(foreach m,$(CUSTOM_MODEM),\
	$(eval d :=$(if $(wildcard $(call relative-path,../../custom/common/modem)/$(m)),$(m),$(if $(wildcard $(call relative-path,../../custom/common/modem)/$(subst ],\],$(subst [,\[,$(m)))),$(subst ],\],$(subst [,\[,$(m))))))\
	$(eval s :=$(if $(d),$(filter-out $(call relative-path,../../custom/common/modem)/$(d)/modem_feature_%.mak,$(wildcard $(call relative-path,../../custom/common/modem)/$(d)/modem_*_*_*.mak))))\
	$(if $(s),\
		$(eval MTK_MODEM_FILE += $(s))\
	)\
  )
  d :=
  s :=
  #ifeq ($(MTK_MODEM_FILE),)
   # $(warning *** Invalid CUSTOM_MODEM = $(CUSTOM_MODEM) in ProjectConfig.mk)
   # $(warning Cannot find $(call relative-path,../../custom/common/modem) for modem*.mak)
  #endif
#  $(info MTK_MODEM_FILE = $(MTK_MODEM_FILE))
  $(foreach modem_make,$(MTK_MODEM_FILE),\
	$(eval modem_name_x_yy_z := $(subst modem,MODEM,$(notdir $(basename $(modem_make)))))\
	$(eval modem_name_x := $(word 1,$(subst _, ,$(modem_name_x_yy_z)))_$(word 2,$(subst _, ,$(modem_name_x_yy_z))))\
	$(eval modem_text := $(shell cat $(modem_make) | sed -e '/^\s*#/'d | awk -F# '{print $$1}' | sed -n '/^\S\+\s*=\s*\S\+/'p | sed -e 's/\s/\?/g' ))\
	$(foreach modem_line,$(modem_text),\
		$(eval modem_option := $(subst ?, ,$(modem_line)))\
		$(eval $(modem_name_x_yy_z)_$(modem_option))\
	)\
	$(foreach modem_line,$(modem_text),\
		$(eval modem_option := $(subst ?, ,$(modem_line)))\
		$(eval modem_feature := $(word 1,$(subst =, ,$(modem_option))))\
		$(eval MODEM_$(modem_feature) := $(sort $(MODEM_$(modem_feature)) $($(modem_name_x_yy_z)_$(modem_feature))))\
		$(eval $(modem_name_x)_$(modem_feature) := $(sort $($(modem_name_x)_$(modem_feature)) $($(modem_name_x_yy_z)_$(modem_feature))))\
		$(eval MTK_MODEM_FEATURE_MD := $(sort $(MTK_MODEM_FEATURE_MD) $(modem_feature)))\
		$(eval MTK_MODEM_FEATURE_AP := $(sort $(MTK_MODEM_FEATURE_AP) MODEM_$(modem_feature) $(modem_name_x)_$(modem_feature) $(modem_name_x_yy_z)_$(modem_feature)))\
	)\
  )
  modem_name_x_yy_z :=
  modem_text :=
  modem_option :=
  modem_feature :=
#  $(foreach modem_option,$(MTK_MODEM_FEATURE_AP),\
#	$(info $(modem_option) = $($(modem_option)))\
#  )
endif
