# security objects
MASP_CORE_DRIVER_DIR := $(MTK_PATH_PLATFORM)/drivers/masp

ifeq ($(MTK_SECURITY_SW_SUPPORT), yes)
ifeq ($(CUSTOM_SEC_AUTH_SUPPORT),yes)
clean-files += $(CCCI_CUSTOM_DRIVER_DIR)/cust_auth.o
else
clean-files += $(patsubst $(obj)/%,%,$(wildcard auth/*.o))
endif
endif

ifeq ($(MTK_SECURITY_SW_SUPPORT), yes)

ifeq ($(strip $(TRUSTONIC_TEE_SUPPORT)),yes)
ccflags-y += -I$(MTK_PATH_PLATFORM)/drivers/gud/MobiCoreKernelApi/include \
             -I$(MTK_PATH_PLATFORM)/drivers/gud/MobiCoreKernelApi/public \
             -I$(MTK_PATH_SOURCE)/../protect-bsp/platform/$(call lc,$(MTK_PLATFORM))/external/trustzone/trustlets/sec/Drsec/Locals/Code/public \
             -I$(MTK_PATH_SOURCE)/../protect-bsp/platform/$(call lc,$(MTK_PLATFORM))/external/trustzone/trustlets/sec/Tlsec/Locals/Code/public

mach_obj += mach/hacc_tee_req.o
mach_obj += mach/hacc_tee.o
mach_obj += mach/hacc_service.o
ccflags-y += -DTRUSTONIC_TEE_SUPPORT

endif

ifeq ($(CUSTOM_SEC_AUTH_SUPPORT),yes)
CCCI_CUSTOM_DRIVER_DIR := $(MTK_PATH_CUSTOM)/$(call lc,$(MTK_PROJECT))/kernel/ccci
ccci_plat-y += $(CCCI_CUSTOM_DRIVER_DIR)/cust_auth.o

endif
endif

ifeq ($(MTK_SEC_MODEM_AUTH), yes)
ccflags-y  += -DENABLE_MD_IMG_SECURITY_FEATURE
endif
ifeq ($(MTK_SEC_MODEM_NVRAM_ANTI_CLONE), yes)
ccflags-y  += -DMTK_SEC_MODEM_NVRAM_ANTI_CLONE
endif

# CCCI objects
obj-$(CONFIG_MTK_ECCCI_DRIVER) := ccci.o

ccflags-y += -I$(MTK_PATH_SOURCE)/drivers/eccci/

# platform dependent parts
ccci-y := ccci_platform.o $(core_obj) $(mach_obj) $(sec_obj)
ifeq ($(CONFIG_MTK_ECCCI_CLDMA), y)
ccci-y += cldma_platform.o
endif

# always build-in parts
obj-y += ccci_off.o
