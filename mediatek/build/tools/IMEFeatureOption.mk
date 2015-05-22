ime_dependency := \
	MTK_IME_PINYIN_SUPPORT=>zh \
	MTK_IME_STROKE_SUPPORT=>zh \
	MTK_IME_HANDWRITING_SUPPORT=>zh

Android_support_locales := \
    en_GB \
    en_SG \
    zh_CN \
    en_US \
    en_AU \
    zh_TW \
    en_NZ \
    fr_CA \
    nl_BE \
    fr_BE \
    de_DE \
    de_CH \
    fr_CH \
    it_CH \
    de_LI \
    nl_NL \
    pl_PL \
    ja_JP \
    fr_FR \
    ko_KR \
    es_ES \
    de_AT \
    it_IT \
    ru_RU \
    cs_CZ
## Reading locale setting directly from MTK_PRODUCT_LOCALES
locales_original := $(MTK_PRODUCT_LOCALES)
locales_original_aapt := $(MTK_PRODUCT_AAPT_CONFIG)
## Reading default latin ime setting directly from DEFAULT_LATIN_IME_LANGUAGES
latin_ime := $(DEFAULT_LATIN_IME_LANGUAGES)
locales_filtered :=

## filter hdpi, mdpi, ldpi, and remove space of PRODUCT_LOCALES
$(foreach o,$(locales_original),\
	$(if $(filter %dpi,$(o)),, \
		$(eval locales_filtered += $(o)) \
	) \
)
$(foreach o,$(locales_original_aapt),\
        $(if $(filter %dpi,$(o)),, \
		$(eval locales_filtered += $(o)) \
	) \
)

IMEFO1 :=
$(foreach l,$(locales_filtered), \
        $(eval pre := $(call uc,$(shell echo $(l) | cut -d_ -f1))) \
        $(eval IMEFO1 += $(if $(filter $(pre),$(IMEFO1)),,$(pre))) \
        $(eval IMEFO1 += $(call uc,$(l))) \
)
IMEFO2 :=
$(foreach l,$(Android_support_locales), \
        $(eval pre := $(call uc,$(shell echo $(l) | cut -d_ -f1))) \
        $(eval IMEFO2 += $(if $(filter $(pre),$(IMEFO2)),,$(pre))) \
        $(eval IMEFO2 += $(call uc,$(l))) \
)


#* parsing MTK IME configuration, 1st->project file, 2nd->MTK_IME_SUPPORT, following... *#

#my @ime_opt_arr = @ARGV;

#######################################################
# this is a workaround, maybe fix in future
######################################################
ifeq ($(MTK_IME_RUSSIAN_SUPPORT), yes)
	ime_opt_arr += MTK_IME_RUSSIAN_SUPPORT=true
endif
ifeq ($(MTK_IME_RUSSIAN_SUPPORT), no)
	ime_opt_arr += MTK_IME_RUSSIAN_SUPPORT=false
endif
ifeq ($(MTK_IME_PINYIN_SUPPORT), yes)
	ime_opt_arr += MTK_IME_PINYIN_SUPPORT=true
endif
ifeq ($(MTK_IME_PINYIN_SUPPORT), no)
	ime_opt_arr += MTK_IME_PINYIN_SUPPORT=false
endif
ifeq ($(MTK_IME_STROKE_SUPPORT), yes)
	ime_opt_arr += MTK_IME_STROKE_SUPPORT=true
endif
ifeq ($(MTK_IME_STROKE_SUPPORT), no)
	ime_opt_arr += MTK_IME_STROKE_SUPPORT=false
endif
ifeq ($(MTK_IME_HANDWRITING_SUPPORT), yes)
	ime_opt_arr += MTK_IME_HANDWRITING_SUPPORT=true
endif
ifeq ($(MTK_IME_HANDWRITING_SUPPORT), no)
	ime_opt_arr += MTK_IME_HANDWRITING_SUPPORT=false
endif
ifeq ($(MTK_IME_GERMAN_SUPPORT), yes)
	ime_opt_arr += MTK_IME_GERMAN_SUPPORT=true
endif
ifeq ($(MTK_IME_GERMAN_SUPPORT), no)
	ime_opt_arr += MTK_IME_GERMAN_SUPPORT=false
endif
ifeq ($(MTK_IME_SPANISH_SUPPORT), yes)
	ime_opt_arr += MTK_IME_SPANISH_SUPPORT=true
endif
ifeq ($(MTK_IME_SPANISH_SUPPORT), no)
	ime_opt_arr += MTK_IME_SPANISH_SUPPORT=false
endif
ifeq ($(MTK_IME_ITALIAN_SUPPORT), yes)
	ime_opt_arr += MTK_IME_ITALIAN_SUPPORT=true
endif
ifeq ($(MTK_IME_ITALIAN_SUPPORT), no)
	ime_opt_arr += MTK_IME_ITALIAN_SUPPORT=false
endif
ifeq ($(MTK_IME_PORTUGUESE_SUPPORT), yes)
	ime_opt_arr += MTK_IME_PORTUGUESE_SUPPORT=true
endif
ifeq ($(MTK_IME_PORTUGUESE_SUPPORT), no)
	ime_opt_arr += MTK_IME_PORTUGUESE_SUPPORT=false
endif
ifeq ($(MTK_IME_INDONESIAN_SUPPORT), yes)
	ime_opt_arr += MTK_IME_INDONESIAN_SUPPORT=true
endif
ifeq ($(MTK_IME_INDONESIAN_SUPPORT), no)
	ime_opt_arr += MTK_IME_INDONESIAN_SUPPORT=false
endif
ifeq ($(MTK_IME_MALAY_SUPPORT), yes)
	ime_opt_arr += MTK_IME_MALAY_SUPPORT=true
endif
ifeq ($(MTK_IME_MALAY_SUPPORT), no)
	ime_opt_arr += MTK_IME_MALAY_SUPPORT=false
endif
ifeq ($(MTK_IME_HINDI_SUPPORT), yes)
	ime_opt_arr += MTK_IME_HINDI_SUPPORT=true
endif
ifeq ($(MTK_IME_HINDI_SUPPORT), no)
	ime_opt_arr += MTK_IME_HINDI_SUPPORT=false
endif
ifeq ($(MTK_IME_ARABIC_SUPPORT), yes)
	ime_opt_arr += MTK_IME_ARABIC_SUPPORT=true
endif
ifeq ($(MTK_IME_ARABIC_SUPPORT), no)
	ime_opt_arr += MTK_IME_ARABIC_SUPPORT=false
endif
ifeq ($(MTK_IME_THAI_SUPPORT), yes)
	ime_opt_arr += MTK_IME_THAI_SUPPORT=true
endif
ifeq ($(MTK_IME_THAI_SUPPORT), no)
	ime_opt_arr += MTK_IME_THAI_SUPPORT=false
endif

define get_support_state
$(foreach d,$(ime_dependency), \
	$(eval ime_dependency_first := $(firstword $(subst =>, ,$(d)))) \
	$(eval ime_dependency_last := $(word 2, $(subst =>, ,$(d)))) \
	$(if $(filter $(1), $(ime_dependency_first)), \
		$(if $(filter $(ime_dependency_last),$(subst _, ,$(Android_support_locales) $(locales_filtered))), \
			$(eval ime_opt_arr_boolean += $(1)=true), \
		) \
	, \
		$(if $(filter true,$(2)),, \
			$(eval ime_opt_arr_boolean += $(1)=false) \
		) \
	) \
)
endef

$(foreach o,$(ime_opt_arr), \
        $(eval _1 := $(firstword $(subst =, ,$(o)))) \
	$(eval _2 := $(word 2, $(subst =, ,$(o)))) \
	$(if $(filter $(_1)=>%,$(ime_dependency)), \
		$(if $(filter false, $(_2)), \
			$(eval ime_opt_arr_boolean += $(_1)=$(_2)) \
		, \
			$(call get_support_state,$(_1),$(_2)) \
		) \
	, \
		$(if $(filter true false, $(_2)), \
			$(eval ime_opt_arr_boolean += $(_1)=$(_2)), \
			$(eval ime_opt_arr_string += $(_1)="$(_2)") \
		) \
	) \
)

.PHONY: $(JAVAIMEOPTFILE)

$(JAVAIMEOPTFILE):
	@echo Update $@
	@rm -f $@
	@mkdir -p $(dir $@)
	@echo 'package com.mediatek.common.featureoption;' >>$@
	@echo 'public final class IMEFeatureOption' >>$@
	@echo '{' >>$@
	@echo -e '\tpublic static final String[] PRODUCT_LOCALES=\n\t{' >>$@
	@$(foreach o,$(locales_filtered),echo -e '\t\t"$(o)",' >>$@;)
	@echo -e '\t};' >>$@
	@echo -e '\tpublic static final String DEFAULT_INPUT_METHOD="$(DEFAULT_INPUT_METHOD)";' >>$@
	@echo -e '\tpublic static final String[] DEFAULT_LATIN_IME_LANGUAGES=\n\t{' >>$@
	@$(foreach o,$(latin_ime),echo -e '\t\t"$(o)",' >>$@;)
	@echo -e '\t};' >>$@
	@$(foreach o,$(filter-out $(filter $(IMEFO1),$(IMEFO2)),$(IMEFO2)),echo -e '\tpublic static final boolean\tSYS_LOCALE_$(o)=false;' >>$@;)
	@$(foreach o,$(IMEFO1),echo -e '\tpublic static final boolean\tSYS_LOCALE_$(o)=true;' >>$@;)
	@$(foreach o,$(ime_opt_arr_boolean),echo -e '\tpublic static final boolean\t$(o);' >>$@;)
	@$(foreach o,$(ime_opt_arr_string),echo -e '\tpublic static final String\t$(o);' >>$@;)
	@echo '}' >>$@
