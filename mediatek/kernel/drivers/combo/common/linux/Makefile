
ccflags-y += -I$(src)/../linux/include \
 						 -I$(src)/../core/include \
 						 -I$(src)/../include \
 						 -I$(src)/ \
 						 -I$(src)/../../../common_detect



ifeq ($(CONFIG_MTK_COMBO),y)
obj-y	+= wmt_dev.o
obj-y	+= wmt_idc.o
obj-y	+= osal.o
obj-y	+= stp_dbg.o

obj-y	+= stp_uart.o
obj-y	+= hif_sdio.o

obj-y	+= stp_sdio.o

obj-y	+= stp_chrdev_gps.o
obj-y	+= stp_chrdev_bt.o

obj-y	+= wmt_chrdev_wifi.o

ifneq ($(filter MT6630,$(MTK_COMBO_CHIP)),)
	obj-y 	+= stp_chrdev_ant.o
endif

endif
