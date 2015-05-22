ifeq ($(EVDO_DT_VIA_SUPPORT), yes)

ifeq ($(EVDO_VIA_HAVE_VOLT_PROTECTION), yes)#if we have voltage protection in design
ccflags-y += -DOEM_HAVE_VOLT_PROTECTION
endif

ccflags-y += -DWAKE_HOST_BY_SYNC

obj-y := core.o power.o oem.o sync.o
obj-y	+= cbp_sdio.o modem_sdio.o
obj-y += f_rawbulk.o rawbulk.o rawbulk_transfer.o

else

obj- :=dummy.o

endif
