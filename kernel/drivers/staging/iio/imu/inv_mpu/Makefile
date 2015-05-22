#
# Makefile for Invensense inv-mpu-iio/inv-mpu-iio-spi device.
#

obj-$(CONFIG_INV_MPU_IIO) += inv-mpu-iio.o
obj-$(CONFIG_INV_MPU_IIO_SPI) += inv-mpu-iio-spi.o

inv-mpu-iio-objs := inv_mpu_i2c.o
inv-mpu-iio-objs += inv_mpu_core.o
inv-mpu-iio-objs += inv_mpu_ring.o
inv-mpu-iio-objs += inv_mpu_misc.o
inv-mpu-iio-objs += inv_mpu_common.o
inv-mpu-iio-objs += inv_mpu_selftest.o
inv-mpu-iio-objs += inv_mpu_setup.o
inv-mpu-iio-objs += inv_slave_compass.o
inv-mpu-iio-objs += inv_slave_pressure.o
inv-mpu-iio-objs += inv_slave_als.o
inv-mpu-iio-objs += inv_mpu_trigger.o

inv-mpu-iio-spi-objs := inv_mpu_spi.o
inv-mpu-iio-spi-objs += inv_mpu_core.o
inv-mpu-iio-spi-objs += inv_mpu_ring.o
inv-mpu-iio-spi-objs += inv_mpu_misc.o
inv-mpu-iio-spi-objs += inv_mpu_common.o
inv-mpu-iio-spi-objs += inv_mpu_selftest.o
inv-mpu-iio-spi-objs += inv_mpu_setup.o
inv-mpu-iio-spi-objs += inv_slave_compass.o
inv-mpu-iio-spi-objs += inv_slave_pressure.o
inv-mpu-iio-spi-objs += inv_slave_als.o
inv-mpu-iio-spi-objs += inv_mpu_trigger.o

CFLAGS_inv_mpu_core.o      += -Idrivers/staging/iio
CFLAGS_inv_mpu_i2c.o      += -Idrivers/staging/iio
CFLAGS_inv_mpu_spi.o      += -Idrivers/staging/iio
CFLAGS_inv_mpu_misc.o      += -Idrivers/staging/iio
CFLAGS_inv_mpu_ring.o      += -Idrivers/staging/iio
CFLAGS_inv_mpu_common.o    += -Idrivers/staging/iio
CFLAGS_inv_mpu_selftest.o  += -Idrivers/staging/iio
CFLAGS_inv_mpu_setup.o  += -Idrivers/staging/iio
CFLAGS_inv_slave_compass.o += -Idrivers/staging/iio
CFLAGS_inv_slave_pressure.o += -Idrivers/staging/iio
CFLAGS_inv_slave_als.o += -Idrivers/staging/iio
CFLAGS_inv_mpu_trigger.o   += -Idrivers/staging/iio

# compile Invensense MPU IIO driver as DTS
ifeq ($(CONFIG_DTS_INV_MPU_IIO), y)
inv-mpu-iio-objs += inv_mpu_dts.o
CFLAGS_inv_mpu_dts.o   += -Idrivers/staging/iio
endif
