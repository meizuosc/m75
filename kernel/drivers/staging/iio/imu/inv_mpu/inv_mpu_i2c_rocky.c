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
#include <linux/i2c.h>
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


#define MAX_IIC_RW_SIZE		8//rocky 
#define DEBUG_IIC_DATA_LIMIT


/**
 *  inv_i2c_read_base() - Read one or more bytes from the device registers.
 *  @st:	Device driver instance.
 *  @i2c_addr:  i2c address of device.
 *  @reg:	First device register to be read from.
 *  @length:	Number of bytes to read.
 *  @data:	Data read from device.
 *  NOTE:This is not re-implementation of i2c_smbus_read because i2c
 *       address could be specified in this case. We could have two different
 *       i2c address due to secondary i2c interface.
 */
int inv_i2c_read_base(struct inv_mpu_state *st, u16 i2c_addr,
	u8 reg, u16 length, u8 *data)
{
	struct i2c_msg msgs[2];
	int res;

	if (!data)
		return -EINVAL;

	msgs[0].addr = i2c_addr;
	msgs[0].flags = 0;	/* write */
	msgs[0].buf = &reg;
	msgs[0].len = 1;
	msgs[0].timing = 400;
//	msgs[0].ext_flag =  (msgs[0].ext_flag& I2C_MASK_FLAG) | I2C_WR_FLAG |I2C_RS_FLAG;

	msgs[1].addr = i2c_addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = data;
	msgs[1].len = length;
	msgs[1].timing = 400;
	msgs[1].ext_flag =  (msgs[0].ext_flag& I2C_MASK_FLAG) | I2C_WR_FLAG |I2C_RS_FLAG;

	res = i2c_transfer(st->sl_handle, msgs, 2);

	if (res < 2) {
		if (res >= 0)
			res = -EIO;
	} else
		res = 0;

	INV_I2C_INC_MPUWRITE(3);
	INV_I2C_INC_MPUREAD(length);

	return res;
}

/**
 *  inv_i2c_single_write_base() - Write a byte to a device register.
 *  @st:	Device driver instance.
 *  @i2c_addr:  I2C address of the device.
 *  @reg:	Device register to be written to.
 *  @data:	Byte to write to device.
 *  NOTE:This is not re-implementation of i2c_smbus_write because i2c
 *       address could be specified in this case. We could have two different
 *       i2c address due to secondary i2c interface.
 */
int inv_i2c_single_write_base(struct inv_mpu_state *st,
	u16 i2c_addr, u8 reg, u8 data)
{
	u8 tmp[2];
	struct i2c_msg msg;
	int res;

	tmp[0] = reg;
	tmp[1] = data;

	msg.addr = i2c_addr;
	msg.flags = 0;	/* write */
	msg.buf = tmp;
	msg.len = 2;

	INV_I2C_INC_MPUWRITE(3);

	res = i2c_transfer(st->sl_handle, &msg, 1);
	if (res < 1) {
		if (res == 0)
			res = -EIO;
		return res;
	} else
		return 0;
}

int inv_plat_single_write(struct inv_mpu_state *st, u8 reg, u8 data)
{
	return inv_i2c_single_write_base(st, st->i2c_addr, reg, data);
}
int inv_plat_read(struct inv_mpu_state *st, u8 reg, int len, u8 *data)
{

#ifdef DEBUG_IIC_DATA_LIMIT
	int tmp;
	int res = 0;
	u8* dptr;
	dptr =data;
	while (len > 0) {
		if (len < MAX_IIC_RW_SIZE)
			tmp = len;
		else
			tmp = MAX_IIC_RW_SIZE;
		res = inv_i2c_read_base(st, st->i2c_addr, reg, tmp, dptr);
		if (res < 0)
			return res;
		dptr += tmp;
		len -= tmp;
	}
	return res;
#else
	return inv_i2c_read_base(st, st->i2c_addr, reg, len, data);
#endif
}
int mpu_memory_write(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
		     u32 len, u8 const *data)
{
	u8 bank[2];
	u8 addr[2];
	u8 buf[513];
	u32 tmp=0;
	u8 * dptr;
	

	struct i2c_msg msgs[3];
	int res=0;

	if (!data || !st)
		return -EINVAL;

	if (len >= (sizeof(buf) - 1))
		return -ENOMEM;

#ifdef DEBUG_IIC_DATA_LIMIT
	dptr = data;
	while (len > 0) {
		if (tmp < MAX_IIC_RW_SIZE)
			tmp = len;
		else
			tmp = MAX_IIC_RW_SIZE;

		bank[0] = REG_MEM_BANK_SEL;
		bank[1] = mem_addr >> 8;

		addr[0] = REG_MEM_START_ADDR;
		addr[1] = mem_addr & 0xFF;

		buf[0] = REG_MEM_R_W;
		memcpy(buf + 1, dptr, tmp);

		/* write message */
		msgs[0].addr = mpu_addr;
		msgs[0].flags = 0;
		msgs[0].buf = bank;
		msgs[0].len = sizeof(bank);

		msgs[1].addr = mpu_addr;
		msgs[1].flags = 0;
		msgs[1].buf = addr;
		msgs[1].len = sizeof(addr);

		msgs[2].addr = mpu_addr;
		msgs[2].flags = 0;
		msgs[2].buf = (u8 *)buf;
		msgs[2].len = tmp + 1;

		INV_I2C_INC_MPUWRITE(3 + 3 + (2 + tmp));
#if CONFIG_DYNAMIC_DEBUG
		{
			char *write = 0;
			pr_debug("%s WM%02X%02X%02X%s%s - %d\n", st->hw->name,
				 mpu_addr, bank[1], addr[1],
				 wr_pr_debug_begin(data, len, write),
				 wr_pr_debug_end(write),
				 len);
		}
#endif

		res = i2c_transfer(st->sl_handle, msgs, 3);
		if (res != 3) {
			if (res >= 0)
				res = -EIO;
			return res;
		} else {
			return 0;
		}		
		dptr += tmp;
		mem_addr += tmp;
		len -=tmp;
	}	
	return res;
#else
//	struct i2c_msg *msgs = NULL;
	u8 *gprDMABuf_va = NULL;
	dma_addr_t gprDMABuf_pa = NULL;
	unsigned char *buf_va = NULL;

	if(!gprDMABuf_va){
	  gprDMABuf_va = (u8 *)dma_alloc_coherent(NULL, 512, &gprDMABuf_pa, GFP_KERNEL);
	  if(!gprDMABuf_va){
		printk("[Invensense] Allocate DMA I2C Buffer failed!\n");
	  }
	}

	buf_va = gprDMABuf_va;

	int times = size/(IIC_SIZE_MAX-4);
	int last = size%(IIC_SIZE_MAX-4);
	if( last!=0 ) {
		times++;
	}

	struct i2c_msg *msgs[] = {
			{
					.addr = mpu_addr & I2C_MASK_FLAG,
					.flags = 0,
					.buf = (u8*)gprDMABuf_pa,
					.len = size,
					.timing = 100,
					.ext_flag = st->client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG,
			}

	};

//	bank[0] = REG_MEM_BANK_SEL;
//	bank[1] = mem_addr >> 8;

	offset = (index-1)*IIC_SIZE_MAX;
	tx_size = (index==times)?last:IIC_SIZE_MAX;

	buf_va[0] = REG_MEM_BANK_SEL;
	buf_va[1] = mem_addr >> 8;
	buf_va[2] = REG_MEM_START_ADDR;
	buf_va[3] = (mem_addr & 0xFF) + offset;
	buf_va[4] = REG_MEM_R_W;
	memcpy(&buf_va[5], data, size);

	msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
	msgs[0].flags = 0;
	msgs[0].buf = (u8*)gprDMABuf_pa;
	msgs[0].len = tx_size;
	msgs[0].timing = 100;
	msgs[0].ext_flag = st->client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG;

	if( i2c_transfer(st->client->adapter, msgs, 1) == 1 ) {
		ret = 0;
		//memcpy(data, buf_va, size);
	} else {
		ret = -EIO;
		printk("%s read error\n",__func__);
	}
	return ret;

	for( int index=1+1,offset=0,tx_size=0; index<=times+1; index++ ) {

		offset = (index-1-1)*IIC_SIZE_MAX;
		tx_size = (index==times+1)?last:IIC_SIZE_MAX;
		addr[0] = REG_MEM_START_ADDR;
		addr[1] = (mem_addr & 0xFF) + offset;

		buf[0] = REG_MEM_R_W;
		memcpy(buf + 1, data + offset, tx_size);

		/* write message */

		msgs[1].addr = mpu_addr & I2C_MASK_FLAG;
		msgs[1].flags = 0;
		msgs[1].buf = addr;
		msgs[1].len = sizeof(addr);
		msgs[1].timing = 100;

		msgs[index].addr = mpu_addr & I2C_MASK_FLAG;
		msgs[index].flags = 0;
		msgs[index].buf = (u8 *)buf;
		msgs[index].len = tx_size;
		msgs[index].timing = 100;

		if( i2c_transfer(st->client->adapter, msgs, (times + 2))
				== (times + 2) ) {
			ret = 0;
			//memcpy(data, buf_va, size);
		} else {
			ret = -EIO;
			printk("%s read error\n",__func__);
		}

	}
	return ret;


	INV_I2C_INC_MPUWRITE(3 + 3 + (2 + len));
#if CONFIG_DYNAMIC_DEBUG
	{
		char *write = 0;
		pr_debug("%s WM%02X%02X%02X%s%s - %d\n", st->hw->name,
			 mpu_addr, bank[1], addr[1],
			 wr_pr_debug_begin(data, len, write),
			 wr_pr_debug_end(write),
			 len);
	}
#endif

	res = i2c_transfer(st->sl_handle, msgs, 3);
	if (res != 3) {
		if (res >= 0)
			res = -EIO;
		return res;
	} else {
		return 0;
	}
#endif	
}

int mpu_memory_read(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
		    u32 len, u8 *data)
{
#if 0
	u8 bank[2];
	u8 addr[2];
	u8 buf;
	u32 tmp=0;
	u8 * dptr;

	bank[0] = REG_MEM_BANK_SEL;
	bank[1] = mem_addr >> 8;

	addr[0] = REG_MEM_START_ADDR;
	addr[1] = mem_addr & 0xFF;

	buf = REG_MEM_R_W;

	/* write message */
	msgs[0].addr = mpu_addr;
	msgs[0].flags = 0;
	msgs[0].buf = bank;
	msgs[0].len = sizeof(bank);

	msgs[1].addr = mpu_addr;
	msgs[1].flags = 0;
	msgs[1].buf = addr;
	msgs[1].len = sizeof(addr);

	msgs[2].addr = mpu_addr;
	msgs[2].flags = 0;
	msgs[2].buf = &buf;
	msgs[2].len = 1;

	msgs[3].addr = mpu_addr;
	msgs[3].flags = I2C_M_RD;
	msgs[3].buf = data;
	msgs[3].len = len;
#endif
	int retval;
	unsigned char retry;
	unsigned char buf;
	unsigned char *buf_va = NULL;
	int full = size / I2C_DMA_LIMIT;
	int partial = size % I2C_DMA_LIMIT;
	int total;
	int last;
	int ii;
	static int msg_length = 0;
	u8 bank[2];
	u8 addr[2];

	if(!gprDMABuf_va1){
	  gprDMABuf_va1 = (u8 *)dma_alloc_coherent(NULL, 1024, &gprDMABuf_pa1, GFP_KERNEL);
	  if(!gprDMABuf_va1){
		printk("[Error] Allocate DMA I2C Buffer failed!\n");
	  }
	}

	buf_va = gprDMABuf_va1;

	if ((full + 2) > msg_length) {
		kfree(read_msgs1);
		msg_length = full + 2;
		read_msgs1 = kcalloc(msg_length, sizeof(struct i2c_msg), GFP_KERNEL);
	}

	bank[0] = REG_MEM_BANK_SEL;
	bank[1] = mem_addr >> 8;
	read_msgs1[0].addr = mpu_addr;
	read_msgs1[0].flags = 0;
	read_msgs1[0].buf = bank;
	read_msgs1[0].len = sizeof(bank);
	read_msgs1[0].timing = 100;

	addr[0] = REG_MEM_START_ADDR;
	addr[1] = mem_addr & 0xFF;
	read_msgs1[1].addr = mpu_addr;
	read_msgs1[1].flags = 0;
	read_msgs1[1].buf = addr;
	read_msgs1[1].len = sizeof(addr);
	read_msgs1[1].timing = 100;

	buf = REG_MEM_R_W;
	read_msgs1[2].addr = mpu_addr;
	read_msgs1[2].flags = 0;
	read_msgs1[2].buf = &buf;
	read_msgs1[2].len = 1;
	read_msgs1[2].timing = 100;

	if (partial) {
		total = full + 1;
		last = partial;
	} else {
		total = full;
		last = I2C_DMA_LIMIT;
	}

	for (ii = 1+2; ii <= total+2; ii++) {
		read_msgs1[ii].addr = mpu_addr;
		read_msgs1[ii].flags = I2C_M_RD;
		read_msgs1[ii].len = (ii == total) ? last : I2C_DMA_LIMIT;
		read_msgs1[ii].buf = gprDMABuf_pa1 + I2C_DMA_LIMIT * (ii - 1 - 2);
		read_msgs1[ii].ext_flag = (st->client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG);
		read_msgs1[ii].timing = 100;
	}

		if (i2c_transfer(st->client->adapter, read_msgs1, (total + 3)) == (total + 3)) {
			retval = 0;
		} else {
			retval = -EIO;
		}

	memcpy(data, buf_va, len);

	return retval;



#if 0
	res = i2c_transfer(st->sl_handle, msgs, 4);
	if (res != 4) {
		if (res >= 0)
			res = -EIO;
	} else
		res = 0;

	INV_I2C_INC_MPUWRITE(3 + 3 + 3);
	INV_I2C_INC_MPUREAD(len);
#if CONFIG_DYNAMIC_DEBUG
	{
		char *read = 0;
		pr_debug("%s RM%02X%02X%02X%02X - %s%s\n", st->hw->name,
			 mpu_addr, bank[1], addr[1], len,
			 wr_pr_debug_begin(data, len, read),
			 wr_pr_debug_end(read));
	}
#endif

#endif
	return res;
#endif
}

/*
 *  inv_mpu_probe() - probe function.
 */
static int inv_mpu_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct inv_mpu_state *st;
	struct iio_dev *indio_dev;
	int result;

#ifdef CONFIG_DTS_INV_MPU_IIO
	enable_irq_wake(client->irq);
#endif

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		result = -ENOSYS;
		pr_err("I2c function error\n");
		goto out_no_free;
	}
	indio_dev = iio_device_alloc(sizeof(*st));
	if (indio_dev == NULL) {
		pr_err("memory allocation failed\n");
		result =  -ENOMEM;
		goto out_no_free;
	}
	st = iio_priv(indio_dev);
	st->client = client;
	st->sl_handle = client->adapter;
	st->i2c_addr = client->addr;
#ifdef CONFIG_DTS_INV_MPU_IIO
	result = invensense_mpu_parse_dt(&client->dev, &st->plat_data);
	if (result)
		goto out_free;

	/*Power on device.*/
	if (st->plat_data.power_on) {
		result = st->plat_data.power_on(&st->plat_data);
		if (result < 0) {
			dev_err(&client->dev,
					"power_on failed: %d\n", result);
			return result;
		}
	pr_info("%s: power on here.\n", __func__);
	}
	pr_info("%s: power on.\n", __func__);

msleep(100);
#else
	st->plat_data =
		*(struct mpu_platform_data *)dev_get_platdata(&client->dev);
#endif
	/* power is turned on inside check chip type*/
	result = inv_check_chip_type(indio_dev, id->name);
	if (result) {
		printk("%s error line:%d\n",__func__,__LINE__);
		goto out_free;
	}

	/* Make state variables available to all _show and _store functions. */
	i2c_set_clientdata(client, indio_dev);
	indio_dev->dev.parent = &client->dev;
	st->dev = &client->dev;
	indio_dev->name = id->name;
	st->irq = client->irq;

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
	INV_I2C_SETIRQ(IRQ_MPU, client->irq);

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
	dev_info(&client->dev, "%s is ready to go!\n", indio_dev->name);

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
	iio_device_free(indio_dev);
out_no_free:
	dev_err(&client->adapter->dev, "%s failed %d\n", __func__, result);

	return -EIO;
}

static void inv_mpu_shutdown(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	dev_dbg(&client->adapter->dev, "Shutting down %s...\n", st->hw->name);

	/* reset to make sure previous state are not there */
	result = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_H_RESET);
	if (result)
		dev_err(&client->adapter->dev, "Failed to reset %s\n",
			st->hw->name);
	msleep(POWER_UP_TIME);
	/* turn off power to ensure gyro engine is off */
	result = inv_set_power(st, false);
	if (result)
		dev_err(&client->adapter->dev, "Failed to turn off %s\n",
			st->hw->name);
	mutex_unlock(&indio_dev->mlock);
}

/*
 *  inv_mpu_remove() - remove function.
 */
static int inv_mpu_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);

	iio_device_unregister(indio_dev);
	if (indio_dev->modes & INDIO_BUFFER_TRIGGERED)
		inv_mpu_remove_trigger(indio_dev);
	iio_buffer_unregister(indio_dev);
	inv_mpu_unconfigure_ring(indio_dev);
	iio_device_free(indio_dev);

	dev_info(&client->adapter->dev, "inv-mpu-iio module removed.\n");

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
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct inv_mpu_state *st = iio_priv(indio_dev);

	/* add code according to different request Start */
	pr_debug("%s inv_mpu_resume\n", st->hw->name);
	mutex_lock(&indio_dev->mlock);

	if (st->chip_config.dmp_on) {
		if (st->batch.on) {
			inv_switch_power_in_lp(st, true);
			write_be32_to_mem(st, st->batch.counter, BM_BATCH_THLD);
			inv_plat_single_write(st, REG_INT_ENABLE_2,
							BIT_FIFO_OVERFLOW_EN_0);
			inv_switch_power_in_lp(st, false);
		}
	} else {
		inv_switch_power_in_lp(st, true);
	}
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
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct inv_mpu_state *st = iio_priv(indio_dev);

	/* add code according to different request Start */
	pr_debug("%s inv_mpu_suspend\n", st->hw->name);

	if (st->chip_config.dmp_on) {
		if (st->batch.on) {
			inv_switch_power_in_lp(st, true);
			write_be32_to_mem(st, INT_MAX, BM_BATCH_THLD);
			inv_plat_single_write(st, REG_INT_ENABLE_2, 0);
			inv_switch_power_in_lp(st, false);
		}
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

static const u16 normal_i2c[] = { I2C_CLIENT_END };
/* device id table is used to identify what device can be
 * supported by this driver
 */
static const struct i2c_device_id inv_mpu_id[] = {
	{"mpu7400", ICM20628},
	{"icm20728", ICM20728},
	{"icm20628", ICM20628},
	{}
};

MODULE_DEVICE_TABLE(i2c, inv_mpu_id);

static struct i2c_driver inv_mpu_driver = {
	.class = I2C_CLASS_HWMON,
	.probe		=	inv_mpu_probe,
	.remove		=	inv_mpu_remove,
	.shutdown	=	inv_mpu_shutdown,
	.id_table	=	inv_mpu_id,
	.driver = {
		.owner	=	THIS_MODULE,
		.name	=	"inv-mpu-iio",
		.pm     =       INV_MPU_PMOPS,
	},
	.address_list = normal_i2c,
};

static int __init inv_mpu_init(void)
{
	int result = i2c_add_driver(&inv_mpu_driver);
	if (result) {
		pr_err("failed\n");
		return result;
	}
	return 0;
}

static void __exit inv_mpu_exit(void)
{
	i2c_del_driver(&inv_mpu_driver);
}

module_init(inv_mpu_init);
module_exit(inv_mpu_exit);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("inv-mpu-iio");
