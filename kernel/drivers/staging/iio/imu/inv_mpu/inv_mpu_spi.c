/*
* Copyright (C) 2012 Invensense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#define pr_fmt(fmt) "inv_mpu: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>

#include "inv_mpu_iio.h"
#include "linux/iio/sysfs.h"
#include "inv_test/inv_counters.h"

#ifdef CONFIG_DTS_INV_MPU_IIO
#include "inv_mpu_dts.h"
#endif
#define INV_SPI_READ 0x80

int inv_plat_single_write(struct inv_mpu_state *st, u8 reg, u8 data)
{
	struct spi_message msg;
	int res;
	u8 d[2];
	struct spi_transfer xfers = {
		.tx_buf = d,
		.bits_per_word = 8,
		.len = 2,
	};

	d[0] = reg;
	d[1] = data;
	spi_message_init(&msg);
	spi_message_add_tail(&xfers, &msg);
	res = spi_sync(to_spi_device(st->dev), &msg);

	return res;
}

int inv_plat_read(struct inv_mpu_state *st, u8 reg, int len, u8 *data)
{
	struct spi_message msg;
	int res;
	u8 d[2];
	struct spi_transfer xfers[] = {
		{
			.tx_buf = d,
			.bits_per_word = 8,
			.len = 1,
		},
		{
			.rx_buf = data,
			.bits_per_word = 8,
			.len = len,
		}
	};

	if (!data)
		return -EINVAL;

	d[0] = (reg | INV_SPI_READ);

	spi_message_init(&msg);
	spi_message_add_tail(&xfers[0], &msg);
	spi_message_add_tail(&xfers[1], &msg);
	res = spi_sync(to_spi_device(st->dev), &msg);

	return res;

}

int mpu_memory_write(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
		     u32 len, u8 const *data)
{
	struct spi_message msg;
	u8 buf[258];
	int res;

	struct spi_transfer xfers = {
		.tx_buf = buf,
		.bits_per_word = 8,
		.len = len + 1,
	};

	if (!data || !st)
		return -EINVAL;

	if (len > (sizeof(buf) - 1))
		return -ENOMEM;

	inv_plat_single_write(st, REG_MEM_BANK_SEL, mem_addr >> 8);
	inv_plat_single_write(st, REG_MEM_START_ADDR, mem_addr & 0xFF);

	buf[0] = REG_MEM_R_W;
	memcpy(buf + 1, data, len);
	spi_message_init(&msg);
	spi_message_add_tail(&xfers, &msg);
	res = spi_sync(to_spi_device(st->dev), &msg);

	return res;
}
int mpu_memory_read(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
		    u32 len, u8 *data)
{
	int res;

	if (!data || !st)
		return -EINVAL;

	if (len > 256)
		return -EINVAL;

	res = inv_plat_single_write(st, REG_MEM_BANK_SEL, mem_addr >> 8);
	res = inv_plat_single_write(st, REG_MEM_START_ADDR, mem_addr & 0xFF);
	res = inv_plat_read(st, REG_MEM_R_W, len, data);

	return res;
}

/*
 *  inv_mpu_probe() - probe function.
 */
static int inv_mpu_probe(struct spi_device *spi)
{
	struct inv_mpu_state *st;
	struct iio_dev *indio_dev;
	int result;

	indio_dev = iio_allocate_device(sizeof(*st));
	if (indio_dev == NULL) {
		pr_err("memory allocation failed\n");
		result =  -ENOMEM;
		goto out_no_free;
	}
	st = iio_priv(indio_dev);

#ifdef CONFIG_DTS_INV_MPU_IIO
	enable_irq_wake(spi->irq);
	result = invensense_mpu_parse_dt(&spi->dev, &st->plat_data);
	if (result)
		goto out_free;

	/*Power on device.*/
	if (st->plat_data.power_on) {
		result = st->plat_data.power_on(&st->plat_data);
		if (result < 0) {
			dev_err(&spi->dev,
					"power_on failed: %d\n", result);
			return result;
		}
	pr_info("%s: power on here.\n", __func__);
	}
	pr_info("%s: power on.\n", __func__);

msleep(100);
#else
	st->plat_data =
		*(struct mpu_platform_data *)dev_get_platdata(&spi->dev);
#endif
	spi_set_drvdata(spi, indio_dev);
	indio_dev->dev.parent = &spi->dev;
	st->dev = &spi->dev;
	st->irq = spi->irq;
	st->i2c_dis = BIT_I2C_IF_DIS;
	if (!strcmp(spi->modalias, "icm20628_spi"))
		indio_dev->name = "icm20628";

	/* power is turned on inside check chip type*/
	result = inv_check_chip_type(indio_dev, indio_dev->name);
	if (result)
		goto out_free;

	result = inv_mpu_configure_ring(indio_dev);
	if (result) {
		pr_err("configure ring buffer fail\n");
		goto out_free;
	}
	result = iio_buffer_register(indio_dev, indio_dev->channels,
					indio_dev->num_channels);
	if (result) {
		pr_err("ring buffer register fail\n");
		goto out_unreg_ring;
	}
	result = inv_mpu_probe_trigger(indio_dev);
	if (result) {
		pr_err("trigger probe fail\n");
		goto out_remove_ring;
	}

	/* Tell the i2c counter, we have an IRQ */
	INV_I2C_SETIRQ(IRQ_MPU, spi->irq);

	result = iio_device_register(indio_dev);
	if (result) {
		pr_err("IIO device register fail\n");
		goto out_remove_trigger;
	}

	result = inv_create_dmp_sysfs(indio_dev);
	if (result) {
		pr_err("create dmp sysfs failed\n");
		goto out_unreg_iio;
	}

	mutex_init(&st->suspend_resume_lock);
	inv_init_sensor_struct(st);
	dev_info(&spi->dev, "%s is ready to go!\n", indio_dev->name);

	return 0;
out_unreg_iio:
	iio_device_unregister(indio_dev);
out_remove_trigger:
	if (indio_dev->modes & INDIO_BUFFER_TRIGGERED)
		inv_mpu_remove_trigger(indio_dev);
out_remove_ring:
	iio_buffer_unregister(indio_dev);
out_unreg_ring:
	inv_mpu_unconfigure_ring(indio_dev);
out_free:
	iio_free_device(indio_dev);
out_no_free:
	dev_err(&spi->dev, "%s failed %d\n", __func__, result);

	return -EIO;
}

static void inv_mpu_shutdown(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	dev_dbg(&spi->dev, "Shutting down %s...\n", st->hw->name);

	/* reset to make sure previous state are not there */
	result = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_H_RESET);
	if (result)
		dev_err(&spi->dev, "Failed to reset %s\n",
			st->hw->name);
	msleep(POWER_UP_TIME);
	/* turn off power to ensure gyro engine is off */
	result = inv_set_power(st, false);
	if (result)
		dev_err(&spi->dev, "Failed to turn off %s\n", st->hw->name);
	mutex_unlock(&indio_dev->mlock);
}

/*
 *  inv_mpu_remove() - remove function.
 */
static int inv_mpu_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);

	iio_device_unregister(indio_dev);
	if (indio_dev->modes & INDIO_BUFFER_TRIGGERED)
		inv_mpu_remove_trigger(indio_dev);
	iio_buffer_unregister(indio_dev);
	inv_mpu_unconfigure_ring(indio_dev);
	iio_free_device(indio_dev);

	dev_info(&spi->dev, "inv-mpu-iio module removed.\n");

	return 0;
}


#ifdef CONFIG_PM
/*
 * inv_mpu_resume(): resume method for this driver.
 *    This method can be modified according to the request of different
 *    customers. It basically undo everything suspend_noirq is doing
 *    and recover the chip to what it was before suspend.
 */
static int inv_mpu_resume(struct device *dev)
{
	struct iio_dev *indio_dev = spi_get_drvdata(to_spi_device(dev));
	struct inv_mpu_state *st = iio_priv(indio_dev);

	/* add code according to different request Start */
	pr_debug("%s inv_mpu_resume\n", st->hw->name);
	mutex_lock(&indio_dev->mlock);

	if (st->chip_config.dmp_on)
		set_inv_enable(indio_dev);
	else
		inv_set_power(st, true);
	mutex_unlock(&indio_dev->mlock);
	/* add code according to different request End */
	mutex_unlock(&st->suspend_resume_lock);

	return 0;
}

/*
 * inv_mpu_suspend(): suspend method for this driver.
 *    This method can be modified according to the request of different
 *    customers. If customer want some events, such as SMD to wake up the CPU,
 *    then data interrupt should be disabled in this interrupt to avoid
 *    unnecessary interrupts. If customer want pedometer running while CPU is
 *    asleep, then pedometer should be turned on while pedometer interrupt
 *    should be turned off.
 */
static int inv_mpu_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = spi_get_drvdata(to_spi_device(dev));
	struct inv_mpu_state *st = iio_priv(indio_dev);

	/* add code according to different request Start */
	pr_debug("%s inv_mpu_suspend\n", st->hw->name);

	if (st->chip_config.dmp_on) {
		st->smd.on = true;
		set_inv_enable(indio_dev);
	} else {
		/* in non DMP case, just turn off the power */
		inv_set_power(st, false);
	}
	/* add code according to different request End */
	st->suspend_state = true;
	msleep(100);
	mutex_lock(&st->suspend_resume_lock);
	st->suspend_state = false;

	return 0;
}

static const struct dev_pm_ops inv_mpu_pmops = {
	.suspend       = inv_mpu_suspend,
	.resume        = inv_mpu_resume,
};
#define INV_MPU_PMOPS (&inv_mpu_pmops)
#else
#define INV_MPU_PMOPS NULL
#endif /* CONFIG_PM */

static const struct spi_device_id inv_mpu_id[] = {
	{"icm20728_spi", 4},
	{"icm20628_spi", 1},
	{},
};
MODULE_DEVICE_TABLE(spi, inv_mpu_id);

static struct spi_driver inv_mpu_driver_spi = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "inv_mpu_iio_spi",
		.pm   =       INV_MPU_PMOPS,
	},
	.probe = inv_mpu_probe,
	.remove = inv_mpu_remove,
	.id_table = inv_mpu_id,
	.shutdown = inv_mpu_shutdown,
};
#if 1
static int __init inv_mpu_init(void)
{
	int result = spi_register_driver(&inv_mpu_driver_spi);

	if (result) {
		pr_err("failed\n");
		return result;
	}
	return 0;
}

static void __exit inv_mpu_exit(void)
{
	spi_unregister_driver(&inv_mpu_driver_spi);
}

module_init(inv_mpu_init);
module_exit(inv_mpu_exit);
#endif

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("inv-mpu-iio-spi");


