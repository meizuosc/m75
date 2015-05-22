
obj-  := dummy.o
my-dir := $(dir $(lastword $(MAKEFILE_LIST)))

#CCCI_PLATFORM_DRIVER_FILES := $(filter-out %.h,$(wildcard $(CCCI_PLATFORM_DRIVER_DIR)/*.c)\
#			$(wildcard $(CCCI_PLATFORM_DRIVER_DIR)/prebuilt/lib*.o))

my-files := $(patsubst $(obj)/%,%,$(wildcard $(my-dir)/*.c)  $(wildcard $(my-dir)/prebuilt/*.o))
ifeq ($(MTK_EMMC_SUPPORT),yes)
	my-libs := $(wildcard $(my-dir)prebuilt/libasp_EMMC.a)
else 
	my-libs := $(wildcard $(my-dir)prebuilt/libasp_NAND.a)
endif
#my-libs := 
#$(info  lib===$(my-libs))
ifneq ($(my-libs),)
 
ldflags-y += -L$(my-dir) --whole-archive $(my-libs)  --no-whole-archive
#--start-group --end-group
endif
ccflags-y  +=-I$(my-dir)/include
ccci-y +=$(my-files:.c=.o)

clean-files += $(patsubst $(obj)/%,%,$(wildcard $(my-dir)/*.o)\
	 $(wildcard $(my-dir)/.*cmd) $(wildcard $(my-dir)/*~))