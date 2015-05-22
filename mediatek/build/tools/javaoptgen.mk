define check_if_number
$(if $(1),$(if $(subst 0,,$(subst 1,,$(subst 2,,$(subst 3,,$(subst 4,,$(subst 5,,$(subst 6,,$(subst 7,,$(subst 8,,$(subst 9,,$(1))))))))))),,1))
endef
FILE_JAVAOPTION_PM := mediatek/build/tools/javaoption.pm
ifneq ($(wildcard $(FILE_JAVAOPTION_PM)),)
LIST_JAVAOPTION_PM := $(shell cat $(FILE_JAVAOPTION_PM))
else
$(error $(FILE_JAVAOPTION_PM) is not exist!)
endif
LIST_JAVAOPTION_DFO :=
LIST_FEATUREOPTION_BOOLEAN :=
LIST_FEATUREOPTION_INT :=
LIST_FEATUREOPTION_BOOLEAN_DFO :=
LIST_FEATUREOPTION_INT_DFO :=

#only eng load will enable dfo
ifneq ($(TARGET_BUILD_VARIANT), user)
  ifneq ($(TARGET_BUILD_VARIANT), userdebug) 
    $(foreach o,$(DFO_NVRAM_SET),\
      	$(if $(filter yes,$($(o))),\
		$(eval k := $(o)_VALUE)\
		$(eval v := $($(k)))\
		$(if $(filter $(v),$(LIST_JAVAOPTION_DFO)),, \
			$(eval LIST_JAVAOPTION_DFO += $(v))\
		) \
	,\
		$(info ignore $(o) = $($(o)))\
	)\
    )
  endif
endif

$(foreach o,$(LIST_JAVAOPTION_PM),\
	$(eval v := $($(o)))\
	$(if $(filter GEMINI,$(o)),\
		$(eval k := MTK_GEMINI_SUPPORT)\
	,\
		$(eval k := $(o))\
	)\
	$(if $(filter $(k),$(subst =, ,$(LIST_FEATUREOPTION_BOOLEAN_DFO) $(LIST_FEATUREOPTION_INT_DFO) $(LIST_FEATUREOPTION_BOOLEAN) $(LIST_FEATUREOPTION_INT))),, \
	$(if $(v),\
		$(if $(filter yes,$(v)),\
			$(if $(filter $(LIST_JAVAOPTION_DFO),$(k)),\
				$(eval LIST_FEATUREOPTION_BOOLEAN_DFO += $(k)=DynFeatureOption.getBoolean("$(k)"))\
			,\
				$(eval LIST_FEATUREOPTION_BOOLEAN += $(k)=true)\
			)\
		,\
		$(if $(filter no,$(v)),\
			$(if $(filter $(LIST_JAVAOPTION_DFO),$(k)),\
				$(eval LIST_FEATUREOPTION_BOOLEAN_DFO += $(k)=DynFeatureOption.getBoolean("$(k)"))\
			,\
				$(eval LIST_FEATUREOPTION_BOOLEAN += $(k)=false)\
			)\
		,\
		$(if $(call check_if_number,$(v)),\
			$(if $(filter $(LIST_JAVAOPTION_DFO),$(k)),\
				$(eval LIST_FEATUREOPTION_INT_DFO += $(k)=DynFeatureOption.getInt("$(k)"))\
			,\
				$(eval LIST_FEATUREOPTION_INT += $(k)=$(v))\
			)\
		,\
				$(info Unknown pattern: $(k)=$(v))\
		)) \
		)\
	,)\
	) \
)
k :=
v :=

FILE_FEATUREOPTION_JAVA := $(JAVAOPTFILE)
LIST_FEATUREOPTION_JAVA := NONE
ifneq ($(wildcard $(FILE_FEATUREOPTION_JAVA)),)
  LIST_FEATUREOPTION_JAVA := $(filter-out public static final boolean int package com.mediatek.common.featureoption; class FeatureOption { } ;,$(shell cat $(FILE_FEATUREOPTION_JAVA)))
endif
k := $(strip $(filter-out $(LIST_FEATUREOPTION_JAVA),$(LIST_FEATUREOPTION_BOOLEAN) $(LIST_FEATUREOPTION_INT) $(LIST_FEATUREOPTION_BOOLEAN_DFO) $(LIST_FEATUREOPTION_INT_DFO)))
v := $(strip $(filter-out $(LIST_FEATUREOPTION_BOOLEAN) $(LIST_FEATUREOPTION_INT) $(LIST_FEATUREOPTION_BOOLEAN_DFO) $(LIST_FEATUREOPTION_INT_DFO),$(LIST_FEATUREOPTION_JAVA)))
ifneq ($(strip $(k))$(strip $(v)),)
$(info k = $(k))
$(info v = $(v))
.PHONY: $(FILE_FEATUREOPTION_JAVA)
endif

$(FILE_FEATUREOPTION_JAVA):
	@echo Update $@
	@rm -f $@
	@mkdir -p $(dir $@)
	@echo 'package com.mediatek.common.featureoption;' >>$@
	@echo 'public final class FeatureOption' >>$@
	@echo '{' >>$@
	@$(foreach o,$(LIST_FEATUREOPTION_BOOLEAN),echo -e '\tpublic static final boolean $(o) ;' >>$@;)
	@$(foreach o,$(LIST_FEATUREOPTION_INT),echo -e '\tpublic static final int $(o) ;' >>$@;)
	@$(foreach o,$(LIST_FEATUREOPTION_BOOLEAN_DFO),echo -e '\tpublic static final boolean $(o) ;' >>$@;)
	@$(foreach o,$(LIST_FEATUREOPTION_INT_DFO),echo -e '\tpublic static final int $(o) ;' >>$@;)
	@echo '}' >>$@
