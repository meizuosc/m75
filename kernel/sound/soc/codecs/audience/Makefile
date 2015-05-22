#snd-soc-es325-objs := es325.o es325-slim.o es325-i2s.o es325-i2c.o
snd-soc-es325-objs := es325.o escore.o escore-pm.o escore-list.o
snd-soc-es705-objs := es705.o es705-slim.o es705-i2s.o es705-i2c.o es705-spi.o \
			es705-cdev.o es705-uart.o es705-uart-common.o
snd-soc-es705-escore-objs := es705_escore.o escore-pm.o escore.o escore-list.o es-d300.o es-a300.o
snd-soc-es515-objs := es515.o escore.o escore-pm.o es-a212.o escore-list.o es-d202.o
snd-soc-es755-objs := es755.o escore.o escore-pm.o escore-list.o es-d300.o es-a300.o
snd-soc-escore-slim-objs += escore-slim.o
snd-soc-escore-i2s-objs += escore-i2s.o
snd-soc-escore-i2c-objs += escore-i2c.o
snd-soc-escore-spi-objs += escore-spi.o
snd-soc-escore-cdev-objs += escore-cdev.o
snd-soc-escore-uart-objs += escore-uart.o escore-uart-common.o
snd-soc-escore-vs-objs += escore-vs.o
snd-soc-es-spi-sensor-hub-objs := es705-sensorhub-demo.o

obj-$(CONFIG_SND_SOC_ES705)	+= snd-soc-es705.o
obj-$(CONFIG_SND_SOC_ES705_ESCORE)	+= snd-soc-es705-escore.o
obj-$(CONFIG_SND_SOC_ES325)	+= snd-soc-es325.o
obj-$(CONFIG_SND_SOC_ES515)	+= snd-soc-es515.o
obj-$(CONFIG_SND_SOC_ES755)	+= snd-soc-es755.o

obj-$(CONFIG_SND_SOC_ES_SLIM)	+= snd-soc-escore-slim.o
ifneq ($(CONFIG_SND_SOC_ES_SLIM),y)
ifeq ($(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SLIM),y)
	obj-y += snd-soc-escore-slim.o
endif
endif

obj-$(CONFIG_SND_SOC_ES_I2S)	+= snd-soc-escore-i2s.o
obj-$(CONFIG_SND_SOC_ES_I2C)	+= snd-soc-escore-i2c.o
ifneq ($(CONFIG_SND_SOC_ES_I2C),y)
ifeq ($(CONFIG_SND_SOC_ES_HIGH_BW_BUS_I2C),y)
	obj-y += snd-soc-escore-i2c.o
endif
endif

obj-$(CONFIG_SND_SOC_ES_SPI)	+= snd-soc-escore-spi.o
ifneq ($(CONFIG_SND_SOC_ES_SPI),y)
ifeq ($(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SPI),y)
	obj-y += snd-soc-escore-spi.o
endif
endif

obj-$(CONFIG_SND_SOC_ES_UART)	+= snd-soc-escore-uart.o
obj-$(CONFIG_SND_SOC_ES_WAKEUP_UART)	+= snd-soc-escore-uart.o
ifneq ($(CONFIG_SND_SOC_ES_UART),y)
ifeq ($(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART),y)
	obj-y += snd-soc-escore-uart.o
endif
endif

obj-$(CONFIG_SND_SOC_ES_CDEV)	+= snd-soc-escore-cdev.o
obj-$(CONFIG_SND_SOC_ES_VS)	+= snd-soc-escore-vs.o
obj-$(CONFIG_SND_SOC_ES_SPI_SENSOR_HUB)	+= snd-soc-es-spi-sensor-hub.o
