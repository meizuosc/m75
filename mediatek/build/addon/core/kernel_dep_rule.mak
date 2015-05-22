####################################################
# eeprom feature dependency

ifneq ($(strip $(CUSTOM_KERNEL_EEPROM)),$(strip $(CUSTOM_HAL_EEPROM)))
  $(call dep-err-seta-or-setb,CUSTOM_KERNEL_EEPROM,$(CUSTOM_HAL_EEPROM),CUSTOM_HAL_EEPROM,$(CUSTOM_KERNEL_EEPROM))
endif
ifeq ($(strip $(BUILD_KERNEL)), yes)
  ifeq ($(strip $(LINUX_KERNEL_VERSION)), )
    $(call dep-err-common, Please turn off BUILD_KERNEL or choose LINUX_KERNEL_VERSION)
  endif
endif
