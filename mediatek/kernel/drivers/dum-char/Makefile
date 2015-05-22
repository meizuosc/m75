# Display parameters shown under Settings -> About Phone

ifeq ($(CONFIG_MTK_MMC),y)
  CONFIG_MTK_USIF = y
else
	ifeq ($(CONFIG_MTK_MTD_NAND),y)
  	CONFIG_MTK_USIF = y
  	else
  	CONFIG_MTK_USIF = n
  	endif
endif

obj-$(CONFIG_MTK_USIF) := DumChar.o

ifeq ($(MTK_GPT_SCHEME_SUPPORT),no)
DumChar-objs :=  dumchar.o env.o
else
DumChar-objs :=  sysenv.o
endif

