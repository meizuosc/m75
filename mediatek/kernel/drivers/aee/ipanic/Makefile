obj-$(CONFIG_MTK_AEE_IPANIC)	+= ipanic.o 
ipanic-y	:= dump-process.o ipanic_log.o ipanic_rom.o

ifdef MTK_EMMC_SUPPORT
ipanic-y	+= ipanic_emmc.o
else
ipanic-y	+= ipanic_mtd.o
endif
