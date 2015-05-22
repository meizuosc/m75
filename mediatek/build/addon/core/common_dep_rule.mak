##############################################################
# for resolution check

ifeq (MT6573,$(strip $(MTK_PLATFORM)))
  ifneq (,$(strip $(LCM_WIDTH)))
    ifeq ($(call gt,$(LCM_WIDTH),320),T)
      ifneq (,$(strip $(LCM_HEIGHT)))
        ifeq ($(call gt,$(LCM_HEIGHT),480),T)
          ifeq (2G,$(strip $(CUSTOM_DRAM_SIZE)))
            $(call dep-err-common, resolution should not be higher than HVGA(320*480) when CUSTOM_DRAM_SIZE=$(CUSTOM_DRAM_SIZE))
          endif
        endif
      endif
    endif
  endif
endif

##############################################################
# for MTK_GEMINI_3G_SWITCH
# Rule: When GEMINI = no, then MTK_GEMINI_3G_SWITCH = no.
# Rule: When EVB = yes, then MTK_GEMINI_3G_SWITCH = no.
# Rule: When MTK_MODEM_SUPPORT!=modem_3g (modem_3g_tdd/modem_3g_fdd), then MTK_GEMINI_3G_SWITCH = no.
# Rule: When GEMINI = yes, EVB=no and MTK_MODEM_SUPPORT=modem_3g, then MTK_GEMINI_3G_SWITCH =yes

ifeq (no,$(strip $(GEMINI)))
  ifeq (yes,$(strip $(MTK_GEMINI_3G_SWITCH)))
    $(call dep-err-common, please turn off MTK_GEMINI_3G_SWITCH when GEMINI=no)
  endif
endif

##############################################################
# for share modem

ifeq (2,$(strip $(MTK_SHARE_MODEM_SUPPORT)))
  ifeq ($(call gt,$(MTK_SHARE_MODEM_CURRENT),2),T)
    $(call dep-err-common, please set MTK_SHARE_MODEM_CURRENT as 2 or 1 or 0 when MTK_SHARE_MODEM_SUPPORT=2)
  endif
endif

ifeq (1,$(strip $(MTK_SHARE_MODEM_SUPPORT)))
  ifeq ($(call gt,$(MTK_SHARE_MODEM_CURRENT),1),T)
    $(call dep-err-common, please set MTK_SHARE_MODEM_CURRENT as 1 or 0 when MTK_SHARE_MODEM_SUPPORT=1)
  endif
endif

ifneq ($(strip $(MTK_DT_SUPPORT)),yes)
  ifeq (yes,$(strip $(GEMINI)))
    ifneq (2,$(strip $(MTK_SHARE_MODEM_CURRENT)))
      $(call dep-err-common, please set MTK_SHARE_MODEM_CURRENT=2 when GEMINI=yes)
    endif
  endif
endif

ifeq (yes,$(strip $(MTK_LTE_DC_SUPPORT)))
  ifneq (2,$(strip $(MTK_SHARE_MODEM_SUPPORT)))
    $(call dep-err-seta-or-offb,MTK_SHARE_MODEM_SUPPORT,2,MTK_LTE_DC_SUPPORT)
  endif
  ifneq (2,$(strip $(MTK_SHARE_MODEM_CURRENT)))
    $(call dep-err-seta-or-offb,MTK_SHARE_MODEM_CURRENT,2,MTK_LTE_DC_SUPPORT)
  endif
else
  ifeq (no,$(strip $(GEMINI)))
    ifneq (1,$(strip $(MTK_SHARE_MODEM_CURRENT)))
      $(call dep-err-common, please set MTK_SHARE_MODEM_CURRENT=1 when GEMINI=no)
    endif
  endif
endif

##############################################################
# for mtk sec modem

ifeq (yes,$(strip $(MTK_SEC_MODEM_AUTH)))
  ifeq (no,$(strip $(MTK_SEC_MODEM_ENCODE)))
    $(call dep-err-ona-or-offb, MTK_SEC_MODEM_ENCODE, MTK_SEC_MODEM_AUTH)
  endif
endif

