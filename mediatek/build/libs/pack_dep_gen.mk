.PHONY: FORCE
SYMBOL_EMPTY :=
SYMBOL_SPACE := $(SYMBOL_EMPTY) $(SYMBOL_EMPTY)

ifndef MTK_DEPENDENCY_AUTO_CHECK
  MTK_DEPENDENCY_AUTO_CHECK := false
endif
export MTK_DEPENDENCY_AUTO_CHECK

MTK_DEPENDENCY_CHECK_LIST := $(MTK_ROOT_CONFIG_OUT)/ProjectConfig.mk
MTK_DEPENDENCY_OUTPUT := $(OUT_DIR)/target/product/$(PROJECT)/obj/dep
MTK_DEPENDENCY_SCRIPT := $(MTK_ROOT_BUILD)/tools/pack_dep_gen.pl
MTK_DEPENDENCY_LOG = && $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log || $(SHOWRSLT) $${PIPESTATUS[0]} $(LOG)$(basename $(notdir $@)).log

# $(1): PHONY action name, also used in dependency file basename
# $(2): path of the dependency file
define mtk-check-dependency
  ifeq ($(MTK_DEPENDENCY_AUTO_CHECK), true)
    -include $(2)/./$(1).dep
  else
    .PHONY: $(2)/$(1).dep
  endif
  $(1): $(2)/$(1).dep
  $(2)/$(1).dep: $(MTK_DEPENDENCY_CHECK_LIST)
endef

define mtk-check-argument
  $(2)/$(1).dep: $(3)
  ifdef cmd_$(2)/$(1).dep
    ifneq ($(strip $(cmd_$(2)/$(1).dep)),)
      ifneq ($(strip $(filter-out $(cmd_$(2)/$(1).dep),$(3)) $(filter-out $(3),$(cmd_$(2)/$(1).dep))),)
#  $$(info $(2)/$(1).dep: - $(cmd_$(2)/$(1).dep))
#  $$(info $(2)/$(1).dep: + $(3))
  $$(info $(2)/$(1).dep: $(strip $(filter-out $(cmd_$(2)/$(1).dep),$(3)) $(filter-out $(3),$(cmd_$(2)/$(1).dep))))
  $(2)/$(1).dep: FORCE
      endif
    endif
  endif
endef

define mtk-check-generate
	mkdir -p $(dir $@);                     \
	if [ -r $@ ] && cmp -s $@ $@.tmp; then \
		rm -f $@.tmp; \
	else \
		mv -f $@.tmp $@; \
	fi
endef

define mtk-print-dependency
	$(hide) perl $(MTK_DEPENDENCY_SCRIPT) $@ $@ $(dir $(LOG)$(basename $(notdir $@)).log) "\b$(notdir $(LOG)$(basename $(notdir $@)))\.log"
endef

define mtk-print-argument
	@echo 'cmd_$@ := $(filter-out FORCE $(MTK_DEPENDENCY_CHECK_LIST),$(if $(1),$(1),$^))' >> $@
endef

define mtk-print-configuration
	@echo cfg_$@ := $(foreach option,$(1),$(option)_$($(option))) >> $@
endef


